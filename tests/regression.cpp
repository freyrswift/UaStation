#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDesktopServices>
#include <QPainter>
#include <QProcess>
#include <QUuid>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <qqml.h>

#include "core/app_controller.h"
#include "core/injector.h"
#include "core/install_plan.h"
#include "core/items.h"
#include "core/skills.h"
#include "core/app_strings.h"
#include "platform/instance_activation.h"

static QString fixtureRoot;

// Exercise the production filesystem logic with standard user locations
// redirected into temporary directories, never the developer's real profile.
class FixtureStandardPaths {
public:
    static constexpr auto HomeLocation = QStandardPaths::HomeLocation;
    static constexpr auto GenericDataLocation = QStandardPaths::GenericDataLocation;
    static QString writableLocation(QStandardPaths::StandardLocation location) {
        return fixtureRoot + (location == HomeLocation ? "/home" : "/data");
    }
};
#define QStandardPaths FixtureStandardPaths
#include "../src/core/paths.cpp"
#undef QStandardPaths

using namespace ua::core;

static void put(const QString& path, const QByteArray& bytes) {
    if (!Injector::writeAtomic(path, bytes)) qFatal("Fixture setup failed");
}

static void selectFixture(const QString& root, const char* name) {
    fixtureRoot = root + '/' + name;
    qputenv("APPDATA", (fixtureRoot + "/data").toUtf8());
    qputenv("LOCALAPPDATA", (fixtureRoot + "/local").toUtf8());
    const QString temporary = fixtureRoot + "/temp";
    if (!QDir().mkpath(temporary)) qFatal("Fixture temp setup failed");
    for (const char* key : {"TMP", "TEMP", "TMPDIR"}) qputenv(key, temporary.toUtf8());
    for (const char* key : {"CODEX_HOME", "CODEX_DIR", "CLAUDE_CONFIG_DIR", "CLAUDE_HOME",
                           "DSH_HOME", "OPENCODE_HOME", "CURSOR_HOME", "WORKBUDDY_HOME", "WORKBUDDY_CONFIG_DIR",
                           "ZCODE_HOME", "GLM_HOME", "ZHIPU_HOME", "MIMOCODE_HOME",
                           "DOUBAO_USER_SKILLS", "GROK_HOME", "GROK_DIR", "GEMINI_HOME", "GEMINI_DIR",
                           "QWEN_HOME", "TRAE_HOME", "CODEBUDDY_HOME", "DEVIN_HOME", "WINDSURF_HOME",
                           "CLINE_HOME", "COPILOT_HOME", "VSCODE_COPILOT_HOME", "KIMI_CODE_HOME"}) qunsetenv(key);
    ensureUserDirs();
    put(fixtureRoot + "/prompt.md", "Fixture prompt.\n");
}

class CloseFixture : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool trayAvailable MEMBER trayAvailable)
public:
    bool trayAvailable = false;
    int quitCalls = 0;
    Q_INVOKABLE void quit() { ++quitCalls; }
};

class UrlFixture : public QObject {
    Q_OBJECT
public:
    QUrl opened;
public slots:
    void open(const QUrl& url) { opened = url; }
};

static QJsonObject jsonAt(const QString& path) {
    const auto doc = QJsonDocument::fromJson(Injector::readFile(path));
    if (!doc.isObject()) qFatal("Expected fixture JSON object");
    return doc.object();
}

static InstallPlan workbuddyFixture(const QString& root, const char* name) {
    selectFixture(root, name);
    put(fixtureRoot + "/local/Programs/WorkBuddy/WorkBuddy.exe", "Fixture only\n");
    const QString config = homeDir() + "/.workbuddy";
    QJsonObject overrides;
    overrides[allSkills().first().dirName] = false;
    overrides["external-skill"] = false;
    put(config + "/settings.json", QJsonDocument(QJsonObject{{"skillOverrides", overrides},
        {"theme", "original"}}).toJson());
    put(config + "/app/app-config.json", "{\"generateMemoryEnabled\":false,\"theme\":\"original\"}\n");
    const auto plan = planFor(*itemById("workbuddy"));
    QStringList paths = {plan.skillsDir, plan.skillsSettings, plan.appConfig};
    for (const auto& target : plan.targets) paths << target.path;
    paths << plan.cacheFiles;
    for (const auto& path : paths) {
        if (!QDir::cleanPath(path).startsWith(QDir::cleanPath(fixtureRoot) + '/'))
            qFatal("WorkBuddy fixture escaped temporary root");
    }
    return plan;
}

static void progressTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    selectFixture(root, "direct-install");
    put(homeDir() + "/.codex/config.toml", "model = \"fixture\"\n");
    AppController controller;
    Strings::instance()->setLang(Strings::CN);
    QVector<QVariantList> events;
    int starts = 0, finishes = 0, errors = 0, ticks = 0;
    bool busy = false, reentered = false, rejected = false, finalStateSaved = false;
    QObject::connect(&controller, &AppController::installStarted, &controller, [&](const QString&, bool) { ++starts; busy = true; });
    QObject::connect(&controller, &AppController::installFinished, &controller, [&] { ++finishes; busy = false; });
    QObject::connect(&controller, &AppController::errorRaised, &controller, [&](const QString&) { ++errors; });
    QObject::connect(&controller, &AppController::installProgress, &controller, [&](int current, int total, const QString& path) {
        events.push_back({current, total, path});
        if (current == total) finalStateSaved = Injector::hasState("codex");
    });
    QTimer timer;
    timer.setInterval(5);
    QObject::connect(&timer, &QTimer::timeout, &controller, [&] {
        ++ticks;
        if (busy && !reentered) {
            reentered = true;
            rejected = !controller.install("codex", fixtureRoot + "/prompt.md");
        }
    });
    qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/theme/Theme.qml")), "UaApp", 1, 0, "Theme");
    qmlRegisterSingletonInstance("UaApp", 1, 0, "Strings", Strings::instance());
    for (const auto& type : {"IconGlyph", "TitleButton", "InstallProgressBar"})
        qmlRegisterType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/components/") + type + ".qml"), "UaApp", 1, 0, type);
    qmlRegisterType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/dialogs/InjectDialog.qml")), "UaApp", 1, 0, "InjectDialog");
    QFontDatabase::addApplicationFont(":/fonts/FluentSystemIcons-Resizable.ttf");
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError>& list) {
        for (const auto& error : list) warnings << error.toString();
    });
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport QtQuick.Window\nimport UaApp 1.0\n"
        "Window { width: 460; height: 524; visible: true; color: Theme.bg; "
        "InjectDialog { objectName: 'dialog'; itemId: 'codex'; onCloseRequested: visible = false } "
        "InstallProgressBar { id: progress; visible: false; z: 10 } "
        "Connections { target: app; "
        "function onInstallStarted(name, removing) { progress.itemName = name; progress.current = 0; progress.total = 0; progress.visible = true } "
        "function onInstallProgress(current, total, path) { progress.current = current; progress.total = total; progress.currentPath = path } "
        "function onInstallFinished() { progress.visible = false } } }", QUrl());
    QScopedPointer<QObject> object(component.create());
    auto* window = qobject_cast<QQuickWindow*>(object.data());
    check(window != nullptr, "direct-install UI loads without preview dependency");
    if (!window) { QTextStream(stderr) << component.errorString(); Injector::setProgressCallback({}); return; }
    auto* dialog = window->findChild<QQuickItem*>("dialog");
    auto* button = window->findChild<QQuickItem*>("installButton");
    auto* progress = window->findChild<QQuickItem*>("installProgressOverlay");
    auto* bar = window->findChild<QQuickItem*>("installProgressBar");
    auto* label = window->findChild<QQuickItem*>("installProgressLabel");
    check(dialog && button && progress && bar && label, "install button and progress controls exist");
    if (!dialog || !button || !progress || !bar || !label) { Injector::setProgressCallback({}); return; }
    dialog->setProperty("promptList", QVariantList{QVariantMap{{"name", "codex.md"}, {"fullPath", fixtureRoot + "/prompt.md"}}});
    check(button->property("label").toString() == Strings::instance()->get(Strings::Inject) && button->width() == 72,
          "original direct-install button label and size restored");
    timer.start();
    QMetaObject::invokeMethod(button, "triggered");
    timer.stop();
    QCoreApplication::processEvents();
    check(Injector::hasState("codex") && !dialog->isVisible(), "single button press installs without confirmation");
    check(starts == 1 && finishes == 1 && errors == 0 && !progress->isVisible(), "install progress starts and finishes once");
    check(ticks > 0 && reentered && rejected, "UI stays responsive and duplicate install is rejected");
    int last = -1, total = events.isEmpty() ? 0 : events.first()[1].toInt();
    bool ordered = !events.isEmpty();
    QSet<QString> skillPaths;
    for (const auto& event : events) {
        const int current = event[0].toInt();
        ordered = ordered && current >= last && current <= total && event[1].toInt() == total;
        last = current;
        const auto path = event[2].toString();
        if (path.endsWith("/SKILL.md")) skillPaths.insert(path);
    }
    check(ordered && events.first()[0].toInt() == 0 && last == total && finalStateSaved,
          "progress is monotonic and reaches 100 only after journal save");
    check(skillPaths.size() == allSkills().size(), "progress includes every bundled skill");
    events.clear();
    check(!controller.install("codex", fixtureRoot + "/missing.md") && starts == 2 && finishes == 2 && errors == 1 &&
          !progress->isVisible() && events.isEmpty(), "failure closes progress and never reports completion");
    const auto failing = workbuddyFixture(root, "progress-failure");
    put(failing.appConfig, "{invalid");
    events.clear();
    check(!controller.install("workbuddy", fixtureRoot + "/prompt.md") && !Injector::hasState("workbuddy") &&
          !events.isEmpty() && events.last()[0].toInt() < events.last()[1].toInt(),
          "failure after skills stays below 100 and rolls back");
    put(failing.appConfig, "{}\n");
    events.clear();
    check(controller.install("workbuddy", fixtureRoot + "/prompt.md") && !events.isEmpty() &&
          events.last()[0].toInt() == events.last()[1].toInt() && Injector::hasState("workbuddy"),
          "index and application settings are included in successful progress total");

    auto pump = [] {
        QElapsedTimer timer; timer.start();
        do { QCoreApplication::processEvents(QEventLoop::AllEvents, 20); QThread::msleep(5); }
        while (timer.elapsed() < 180);
    };
    auto* theme = engine.singletonInstance<QObject*>(qmlTypeId("UaApp", 1, 0, "Theme"));
    progress->setVisible(true);
    progress->setProperty("itemName", "Codex");
    progress->setProperty("currentPath", "D:/Profiles/Example/skills/long-skill-directory-name/SKILL.md");
    progress->setProperty("total", 140);
    progress->setProperty("current", 70);
    const auto artifacts = qEnvironmentVariable("UA_UI_ARTIFACT_DIR");
    if (!artifacts.isEmpty()) QDir().mkpath(artifacts);
    for (const auto& size : {QSize(460, 524), QSize(640, 720)}) {
        window->resize(size);
        for (int mode : {1, 2}) for (auto language : {Strings::CN, Strings::EN}) {
            theme->setProperty("mode", mode);
            Strings::instance()->setLang(language);
            pump();
            const auto frame = window->grabWindow();
            const auto track = bar->mapRectToScene(bar->boundingRect()).toAlignedRect();
            int accent = 0;
            for (int y = track.top(); y <= track.bottom() && y < frame.height(); ++y)
                for (int x = track.left(); x <= track.right() && x < frame.width(); ++x) {
                    const auto pixel = frame.pixelColor(x, y);
                    if (pixel.blue() > 200 && pixel.red() < 150 && pixel.green() > 150) ++accent;
                }
            check(!frame.isNull() && accent > 200 && bar->property("value").toDouble() == 0.5 &&
                  label->property("text").toString().startsWith("50%"), "localized progress bar renders actual 50 percent");
            if (!artifacts.isEmpty()) frame.save(artifacts + QString("/progress-%1-%2-%3.png").arg(size.width()).arg(mode).arg(language));
        }
    }
    progress->setProperty("current", 200);
    pump();
    check(bar->property("value").toDouble() == 1 && label->property("text").toString().startsWith("100%"), "progress is clamped to track width");
    progress->setProperty("total", 0);
    pump();
    check(bar->property("indeterminate").toBool() && !label->property("text").toString().contains("0 / 0"), "preparation uses indeterminate progress");
    check(warnings.isEmpty(), "install and progress QML have no runtime warnings");
    if (!warnings.isEmpty()) QTextStream(stderr) << warnings.join('\n') << '\n';
    Injector::setProgressCallback({});
}

static void featureTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    selectFixture(root, "workbuddy-native-root");
    qputenv("WORKBUDDY_HOME", (fixtureRoot + "/legacy-override").toUtf8());
    const QString nativeRoot = fixtureRoot + "/native-override";
    qputenv("WORKBUDDY_CONFIG_DIR", nativeRoot.toUtf8());
    check(toolConfigRoot("workbuddy") == nativeRoot &&
          toolPath("workbuddy", ".workbuddy/skills") == nativeRoot + "/skills",
          "WorkBuddy uses its native config environment variable before legacy alias");
    selectFixture(root, "package-files");
    const QString name = allSkills().first().dirName;
    const QString skillsRoot = fixtureRoot + "/skills";
    const QString package = skillsRoot + '/' + name;
    const QString script = package + "/scripts/check.js";
    const QString reference = package + "/references/nested/guide.md";
    const QString asset = package + "/assets/sample.csv";
    const QString meta = stateDir() + "/claude-skills-backups/" +
        QString::fromLatin1(name.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
    put(script, "original script\n");
    put(package + "/user.txt", "keep me\n");
    bool installed = false;
    const auto names = Skills::installInto("claude", skillsRoot, &installed);
    check(installed && names.size() == allSkills().size() && QFileInfo::exists(reference) &&
          Injector::readFile(asset) == "name,value\nfixture,42\n", "package includes nested references and assets");
    const auto manifest = jsonAt(meta + ".files.json");
    const auto ownedFiles = manifest.value("files").toArray();
    check(ownedFiles.contains("SKILL.md") && ownedFiles.contains("scripts/check.js") &&
          ownedFiles.contains("references/nested/guide.md") && ownedFiles.contains("assets/sample.csv"),
          "package manifest records all fixture-owned files alongside real companions");
    const auto installedScript = Injector::readFile(script);
    check(installedScript == Injector::readFile(":/skills/" + name + "/scripts/check.js"),
          "package installs script bytes without executing them");
    check(Skills::missingBundledAssets("radare2").isEmpty(),
          "bundled skill audit confirms radare2 companion assets");
    check(Skills::missingBundledAssets("rea-analysis-optimization").contains("references/kuna-case-study.md"),
          "bundled skill audit exposes genuinely missing references");
    check(Skills::transactionFiles("claude", skillsRoot, {name}).contains(reference),
          "outer transaction captures companion files");
    bool reinstalled = false;
    Skills::installInto("claude", skillsRoot, &reinstalled);
    check(reinstalled, "complete package reinstall is idempotent");
    put(reference, "user-edited reference\n");
    bool removed = true;
    check(Skills::removeFrom(skillsRoot, {name}, "claude", &removed) == 0 && !removed &&
          Injector::readFile(script) == installedScript && QFileInfo::exists(asset),
          "edited companion aborts uninstall and rolls earlier files back");
    bool conflict = true;
    Skills::installInto("claude", skillsRoot, &conflict);
    check(!conflict && Injector::readFile(reference) == "user-edited reference\n" &&
          jsonAt(meta + ".files.json") == manifest, "reinstall preserves user edits and manifest");
    put(reference, Injector::readFile(":/skills/" + name + "/references/nested/guide.md"));
    Skills::removeFrom(skillsRoot, names, "claude", &removed);
    check(removed && Injector::readFile(script) == "original script\n" &&
          !QFileInfo::exists(reference) && !QFileInfo::exists(asset) &&
          Injector::readFile(package + "/user.txt") == "keep me\n",
          "uninstall restores originals and preserves unrelated files");
    check(!QFileInfo::exists(meta + ".files.json") &&
          !QFileInfo::exists(package + "/references"), "uninstall removes manifests and empty nested directories");

    Skills::installInto("claude", skillsRoot, &installed);
    const auto validManifest = Injector::readFile(meta + ".files.json");
    put(meta + ".files.json", "{broken");
    Skills::removeFrom(skillsRoot, {name}, "claude", &removed);
    check(!removed && QFileInfo::exists(script), "corrupt package manifest fails closed");
    put(meta + ".files.json", "{\"version\":1,\"files\":[\"SKILL.md\",\"../outside.txt\"]}");
    check(Skills::transactionFiles("claude", skillsRoot, {name}).isEmpty(), "manifest rejects traversal paths");
    put(meta + ".files.json", validManifest);
    const auto previousNames = Injector::readFile(stateDir() + "/claude-skills.dat");
    const QString blockedRoot = fixtureRoot + "/blocked";
    put(blockedRoot + '/' + name + "/scripts", "not a directory\n");
    bool migrated = true;
    Skills::installInto("claude", blockedRoot, &migrated);
    check(!migrated && QFileInfo::exists(script) &&
          Injector::readFile(stateDir() + "/claude-skills.dat") == previousNames &&
          Skills::installedRoot("claude") == skillsRoot, "failed package migration restores source files and metadata");

    selectFixture(root, "legacy-package");
    const QString legacyRoot = fixtureRoot + "/skills";
    const QString legacyPackage = legacyRoot + '/' + name;
    put(legacyPackage + "/SKILL.md", Injector::readFile(":/skills/" + name + "/SKILL.md"));
    const auto companion = Injector::readFile(":/skills/" + name + "/assets/sample.csv");
    put(legacyPackage + "/assets/sample.csv", companion);
    put(stateDir() + "/claude-skills.dat", name.toUtf8() + '\n');
    put(stateDir() + "/claude-skills-root.dat", legacyRoot.toUtf8());
    const auto upgraded = Skills::installInto("claude", legacyRoot, &installed);
    Skills::removeFrom(legacyRoot, upgraded, "claude", &removed);
    check(installed && removed && !QFileInfo::exists(legacyPackage + "/SKILL.md") &&
          Injector::readFile(legacyPackage + "/assets/sample.csv") == companion,
          "legacy upgrade owns only old SKILL.md and backs up preexisting companions");

    selectFixture(root, "activation");
    AppController controller;
    QWindow window;
    controller.attachMainWindow(&window);
    const QString key = "ua-test-" + QUuid::createUuid().toString(QUuid::Id128);
    ua::platform::InstanceActivation server(key);
    int activated = 0;
    server.setHandler([&] { ++activated; controller.showMainWindow(); });
    check(server.listen(), "local activation listener starts");
    auto notify = [&](const QString& instanceKey) {
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {"--notify-instance", instanceKey});
        if (!child.waitForStarted(3000)) return false;
        QElapsedTimer timer;
        timer.start();
        while (child.state() != QProcess::NotRunning && timer.elapsed() < 8000) {
            QCoreApplication::processEvents();
            QThread::msleep(5);
        }
        if (child.state() != QProcess::NotRunning) { child.kill(); child.waitForFinished(); return false; }
        return child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0;
    };
    window.hide();
    check(notify(key) && activated == 1 && window.isVisible(), "second process restores hidden window");
    window.showMinimized();
    check(notify(key) && activated == 2 && window.windowState() != Qt::WindowMinimized,
          "second process restores minimized window");
    window.showMaximized();
    check(notify(key) && activated == 3 && window.windowState() == Qt::WindowMaximized,
          "activation preserves maximized window");
    const QString delayedKey = key + "-delayed";
    ua::platform::InstanceActivation delayed(delayedKey);
    bool started = false;
    QTimer::singleShot(150, &delayed, [&] { started = delayed.listen(); });
    check(notify(delayedKey) && started, "second launch retries while first instance starts");
    bool pending = false;
    delayed.setHandler([&] { pending = true; });
    check(pending, "activation before window attachment is retained");
    QElapsedTimer timer;
    timer.start();
    check(!ua::platform::InstanceActivation::notify(key + "-missing", 100) && timer.elapsed() < 1000,
          "missing instance activation has a bounded timeout");
}

static void reliabilityTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    selectFixture(root, "changed-pointer");
    const QString config = homeDir() + "/.codex/config.toml";
    put(config, "model = \"fixture\"\n");
    const bool pointerInstall = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    auto legacyPointerState = jsonAt(stateDir() + "/codex.dat");
    legacyPointerState.remove("tomlInstalledLine");
    put(stateDir() + "/codex.dat", QJsonDocument(legacyPointerState).toJson());
    const QByteArray userConfig = "model = \"fixture\"\nmodel_instructions_file = \"user-new.md\"\n";
    put(config, userConfig);
    check(Injector::scan("codex") == InstallState::NeedsRepair,
          "changed TOML pointer is not reported as active");
    check(pointerInstall && Injector::uninstall("codex").ok && Injector::readFile(config) == userConfig,
          "legacy install state also preserves user's later TOML pointer");
    const bool pointerReinstall = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    const QByteArray newerConfig = "model = \"fixture\"\nmodel_instructions_file = \"user-newer.md\"\n";
    put(config, newerConfig);
    check(pointerReinstall && Injector::install("codex", fixtureRoot + "/prompt.md").ok &&
          Injector::uninstall("codex").ok && Injector::readFile(config) == newerConfig,
          "reinstall records the user's current pointer as its new baseline");

    selectFixture(root, "single-skill-then-uninstall");
    put(homeDir() + "/.claude/settings.json", "{}\n");
    const QString name = allSkills().first().dirName;
    const QString skill = homeDir() + "/.claude/skills/" + name + "/SKILL.md";
    const QByteArray originalSkill = "User's original named skill.\n";
    put(skill, originalSkill);
    const bool skillInstall = Injector::install("claude", fixtureRoot + "/prompt.md").ok;
    check(skillInstall && Skills::uninstallByName(name) == 1 &&
          Injector::readFile(skill) == originalSkill &&
          Injector::scan("claude") == InstallState::Installed,
          "single-skill removal updates the item journal without a false repair warning");
    check(Injector::uninstall("claude").ok && !Injector::hasState("claude") &&
          Injector::readFile(skill) == originalSkill,
          "full uninstall skips previously restored skills");

    selectFixture(root, "missing-files");
    put(homeDir() + "/.codex/config.toml", "model = \"fixture\"\n");
    const bool scanInstall = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    const QString missingSkill = Skills::installedRoot("codex") + '/' + name + "/SKILL.md";
    const auto skillBytes = Injector::readFile(missingSkill);
    const bool removedSkill = QFile::remove(missingSkill);
    check(scanInstall && removedSkill && Injector::scan("codex") == InstallState::NeedsRepair,
          "missing installed skill marks the installation for repair");
    put(missingSkill, skillBytes);
    const bool removedMarked = QFile::remove(homeDir() + "/.codex/AGENTS.md");
    const bool removedManaged = QFile::remove(homeDir() + "/.codex/managed-prompts/Ua.md");
    check(removedMarked && removedManaged && Injector::scan("codex") == InstallState::NeedsRepair,
          "missing prompt files do not retain installed status");
    check(Injector::install("codex", fixtureRoot + "/prompt.md").ok &&
          Injector::scan("codex") == InstallState::Installed && Injector::uninstall("codex").ok,
          "reinstall repairs missing targets and remains uninstallable");

    const auto plan = workbuddyFixture(root, "upgrade-baseline");
    const QString product = fixtureRoot + "/local/Programs/WorkBuddy/resources/app.asar.unpacked/cli/product.json";
    const QByteArray version1 = "{\"version\":\"fixture-v1\"}\n";
    const QByteArray version2 = "{\"version\":\"fixture-v2\"}\n";
    put(product, version1);
    const bool hostInstall = Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok;
    put(product, version2);
    check(hostInstall && Injector::hostUpgraded("workbuddy") &&
          Injector::scan("workbuddy") == InstallState::NeedsRepair,
          "host upgrade is detected as repair needed");
    const auto installedSkill = plan.skillsDir + '/' + name + "/SKILL.md";
    const auto savedSkill = Injector::readFile(installedSkill);
    put(installedSkill, "User edit blocks reinstall\n");
    const auto sidecarBefore = Injector::readFile(product + ".bak-inject");
    check(!Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok &&
          Injector::readFile(product) == version2 &&
          Injector::readFile(product + ".bak-inject") == sidecarBefore,
          "failed reinstall rolls back the refreshed baseline");
    put(installedSkill, savedSkill);
    check(Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok &&
          Injector::uninstall("workbuddy").ok && Injector::readFile(product) == version2,
          "uninstall after reinstall restores the upgraded host version");
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--notify-instance")
        return ua::platform::InstanceActivation::notify(app.arguments().at(2)) ? 0 : 1;
#ifdef Q_OS_WIN
    // The offscreen platform has no system font database on Windows.
    if (app.platformName() == "offscreen") {
        const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
        for (const auto& file : {"segoeui.ttf", "segoeuib.ttf", "consola.ttf", "msyh.ttc"})
            QFontDatabase::addApplicationFont(fonts + file);
        QApplication::setFont(QFont("Segoe UI"));
    }
#endif
    app.setOrganizationName("UaRegression");
    app.setApplicationName("UaRegression");
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir temp;
    if (!temp.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path() + "/settings");
    QTextStream out(stdout);
    int failures = 0;
    int checks = 0;
    auto check = [&](bool ok, const char* name) {
        ++checks;
        if (!ok) ++failures;
        out << (ok ? "PASS " : "FAIL ") << name << '\n';
        out.flush();
    };

    if (app.arguments().contains("--reliability-only")) {
        reliabilityTests(temp.path(), check);
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

    if (app.arguments().contains("--features-only")) {
        featureTests(temp.path(), check);
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

    if (app.arguments().contains("--progress-only")) {
        progressTests(temp.path(), check);
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

    if (!app.arguments().contains("--logos-only")) {
    selectFixture(temp.path(), "dedicated-path");
    for (const auto& pair : {qMakePair("codex", "CODEX_HOME"), qMakePair("cursor", "CURSOR_HOME"),
                             qMakePair("deepseek", "DSH_HOME"), qMakePair("zcode", "ZCODE_HOME"),
                             qMakePair("workbuddy", "WORKBUDDY_HOME")}) {
        const QString root = fixtureRoot + "/custom-" + pair.first;
        qputenv(pair.second, root.toUtf8());
        put(root + "/settings.json", "{}\n");
        const auto plan = planFor(*itemById(pair.first));
        check(toolDetectedById(pair.first) && plan.targets.first().path.startsWith(root + '/') &&
              plan.skillsDir.startsWith(root + '/'), pair.first);
    }

    selectFixture(temp.path(), "xdg-path");
    qputenv("OPENCODE_HOME", (fixtureRoot + "/custom").toUtf8());
    const auto open = planFor(*itemById("opencode"));
    check(open.targets.first().path == fixtureRoot + "/custom/AGENTS.md" &&
          open.skillsDir == fixtureRoot + "/custom/skills", "XDG root has no extra product segment");

    selectFixture(temp.path(), "repeat");
    const QString config = homeDir() + "/.codex/config.toml";
    const QByteArray original = "model = \"fixture\"\n";
    put(config, original);
    check(Injector::install("codex", fixtureRoot + "/prompt.md").ok, "initial install");
    check(Injector::install("codex", fixtureRoot + "/prompt.md").ok, "repeat install");
    check(Injector::uninstall("codex").ok && Injector::readFile(config) == original &&
          !QFileInfo::exists(homeDir() + "/.codex/managed-prompts/Ua.md"), "repeat uninstall restores original pointer state");

    selectFixture(temp.path(), "toml-profile");
    const QString profileConfig = homeDir() + "/.codex/config.toml";
    const QByteArray profile = "note = '''\n[not-a-table]\nmodel_instructions_file = \"text\"\n'''\n"
                               "[profiles.custom]\nmodel_instructions_file = \"original.md\"\n";
    put(profileConfig, profile);
    const bool installed = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    const auto profileAfter = Injector::readFile(profileConfig);
    check(installed && profileAfter.startsWith("model_instructions_file = ") &&
          profileAfter.endsWith(profile), "TOML profile and multiline string stay unchanged");
    check(Injector::uninstall("codex").ok && Injector::readFile(profileConfig) == profile,
          "TOML profile round trip");

    selectFixture(temp.path(), "original-pointer");
    const QString originalConfig = homeDir() + "/.codex/config.toml";
    const QByteArray originalPointer = "\"model_instructions_file\" = \"original.md\" # keep\r\n[profiles.x]\r\nmodel = \"x\"\r\n";
    put(originalConfig, originalPointer);
    const bool first = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    const bool second = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    check(first && second && Injector::uninstall("codex").ok &&
          Injector::readFile(originalConfig) == originalPointer, "quoted root pointer restored byte for byte");

    selectFixture(temp.path(), "root-migration");
    const QString rootA = fixtureRoot + "/codex-a";
    const QString rootB = fixtureRoot + "/codex-b";
    const QByteArray pointerA = "model_instructions_file = \"a.md\"\n";
    const QByteArray pointerB = "model_instructions_file = \"b.md\"\n";
    put(rootA + "/config.toml", pointerA);
    put(rootB + "/config.toml", pointerB);
    qputenv("CODEX_HOME", rootA.toUtf8());
    const bool installedA = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    qputenv("CODEX_HOME", rootB.toUtf8());
    const bool installedB = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    check(installedA && installedB && Injector::readFile(rootA + "/config.toml") == pointerA,
          "root migration restores prior TOML before switching");
    check(Injector::uninstall("codex").ok && Injector::readFile(rootB + "/config.toml") == pointerB,
          "root migration retains destination TOML original");

    selectFixture(temp.path(), "migration-failure");
    const QString failA = fixtureRoot + "/codex-a";
    const QString failB = fixtureRoot + "/codex-b";
    put(failA + "/config.toml", pointerA);
    put(failB + "/settings.json", "{}\n");
    QDir().mkpath(failB + "/config.toml");
    qputenv("CODEX_HOME", failA.toUtf8());
    const bool beforeMigration = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    const QByteArray beforeToml = Injector::readFile(failA + "/config.toml");
    const QString retired = "retired-fixture-skill";
    const QString retiredFile = failA + "/skills/" + retired + "/SKILL.md";
    const QString skillNames = stateDir() + "/codex-skills.dat";
    const QString retiredMeta = stateDir() + "/codex-skills-backups/" +
        QString::fromLatin1(retired.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
    put(retiredFile, "Retired fixture skill\n");
    put(retiredMeta + ".new", {});
    put(retiredMeta + ".sha256", QCryptographicHash::hash("Retired fixture skill\n", QCryptographicHash::Sha256).toHex());
    put(skillNames, Injector::readFile(skillNames) + retired.toUtf8() + '\n');
    const QByteArray beforeNames = Injector::readFile(skillNames);
    qputenv("CODEX_HOME", failB.toUtf8());
    const auto failedMigration = Injector::install("codex", fixtureRoot + "/prompt.md");
    check(beforeMigration && !failedMigration.ok && Injector::readFile(failA + "/config.toml") == beforeToml &&
          Injector::readFile(retiredFile) == "Retired fixture skill\n" &&
          Injector::readFile(skillNames) == beforeNames && QFileInfo::exists(retiredMeta + ".new"),
          "migration rollback restores old pointer and retired skills");

    selectFixture(temp.path(), "skills-original");
    const QString name = allSkills().first().dirName;
    const QString root = fixtureRoot + "/skills";
    const QString skill = root + '/' + name + "/SKILL.md";
    const QByteArray customSkill = "My original skill\n";
    put(skill, customSkill);
    bool skillsOk = false;
    const auto names = Skills::installInto("claude", root, &skillsOk);
    bool removedOk = false;
    Skills::removeFrom(root, names, "claude", &removedOk);
    check(skillsOk && removedOk && Injector::readFile(skill) == customSkill, "existing skill restored on uninstall");

    selectFixture(temp.path(), "skills-migration");
    const QString skillsA = fixtureRoot + "/skills-a";
    const QString skillsB = fixtureRoot + "/skills-b";
    QFile bundle(QStringLiteral(":/skills/") + name + QStringLiteral("/SKILL.md"));
    const bool bundleRead = bundle.open(QIODevice::ReadOnly);
    const QByteArray bundleBytes = bundle.readAll();
    put(skillsB + '/' + name + "/SKILL.md", bundleBytes);
    bool migrateA = false;
    bool migrateB = false;
    Skills::installInto("claude", skillsA, &migrateA);
    const auto migratedNames = Skills::installInto("claude", skillsB, &migrateB);
    bool migrationRemoved = false;
    Skills::removeFrom(skillsB, migratedNames, "claude", &migrationRemoved);
    check(bundleRead && migrateA && migrateB && migrationRemoved &&
          QFileInfo::exists(skillsB + '/' + name + "/SKILL.md") &&
          Injector::readFile(skillsB + '/' + name + "/SKILL.md") == bundleBytes,
          "skill migration preserves destination's identical preexisting file");

    selectFixture(temp.path(), "skills-edited");
    const QString editRoot = fixtureRoot + "/skills";
    const auto editedNames = Skills::installInto("claude", editRoot);
    const QString edited = editRoot + '/' + name + "/SKILL.md";
    put(edited, "User edit after install\n");
    bool editedRemoved = true;
    Skills::removeFrom(editRoot, editedNames, "claude", &editedRemoved);
    check(!editedRemoved && Injector::readFile(edited) == "User edit after install\n", "external skill edits are preserved");

    selectFixture(temp.path(), "partial");
    put(homeDir() + "/.codex/config.toml", original);
    put(homeDir() + "/.codex/managed-prompts", "blocks directory creation");
    const auto partial = Injector::install("codex", fixtureRoot + "/prompt.md");
    check(!partial.ok && !Injector::hasState("codex") &&
          !QFileInfo::exists(homeDir() + "/.codex/AGENTS.md"), "failed install rolls back earlier targets");

    selectFixture(temp.path(), "uninstall-failure");
    put(homeDir() + "/.codex/config.toml", original);
    const bool beforeUninstall = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    const QString markedFile = homeDir() + "/.codex/AGENTS.md";
    const QByteArray beforeMarked = Injector::readFile(markedFile);
    put(homeDir() + "/.codex/skills/" + name + "/SKILL.md", "Edited fixture skill\n");
    check(beforeUninstall && !Injector::uninstall("codex").ok && Injector::hasState("codex") &&
          Injector::readFile(markedFile) == beforeMarked &&
          QFileInfo::exists(homeDir() + "/.codex/managed-prompts/Ua.md"),
          "failed uninstall restores earlier targets and preserves state");

    selectFixture(temp.path(), "oversize");
    const QString large = homeDir() + "/.claude/CLAUDE.md";
    const QByteArray largeBytes(16 * 1024 * 1024 + 1, 'x');
    put(large, largeBytes);
    check(!Injector::install("claude", fixtureRoot + "/prompt.md").ok &&
          Injector::readFile(large).isEmpty() && QFileInfo(large).size() == largeBytes.size(),
          "oversized target is rejected without truncation");

    selectFixture(temp.path(), "skill-manager");
    const QString customRoot = fixtureRoot + "/custom-claude";
    qputenv("CLAUDE_CONFIG_DIR", customRoot.toUtf8());
    put(customRoot + "/settings.json", "{}\n");
    const auto skillPlan = planFor(*itemById("claude"));
    Skills::installInto("claude", skillPlan.skillsDir);
    int visible = 0;
    for (const auto& status : Skills::statuses()) {
        if (status.dirName == name) visible = status.installedCount;
    }
    qputenv("CLAUDE_CONFIG_DIR", (fixtureRoot + "/changed-env").toUtf8());
    check(visible == 1 && Skills::uninstallByName(name) == 1 &&
          !QFileInfo::exists(customRoot + "/skills/" + name + "/SKILL.md"), "skill manager uses persisted install root");

    selectFixture(temp.path(), "ui-paths");
    const QString uiRoot = fixtureRoot + "/custom-codex";
    qputenv("CODEX_HOME", uiRoot.toUtf8());
    put(uiRoot + "/settings.json", "{}\n");
    put(uiRoot + "/skills/example/SKILL.md", "Fixture skill\n");
    AppController pathController;
    QVariantMap diagnostic;
    for (const auto& entry : pathController.envCheck()) {
        const auto map = entry.toMap();
        if (map.value("id").toString() == "codex") diagnostic = map;
    }
    check(diagnostic.value("targetPath") == uiRoot + "/AGENTS.md" &&
          diagnostic.value("installDir") == uiRoot &&
          diagnostic.value("skillsDir") == uiRoot + "/skills" &&
          diagnostic.value("skillCount").toInt() == 1, "diagnostics use resolved install paths");
    UrlFixture urlFixture;
    QDesktopServices::setUrlHandler("file", &urlFixture, "open");
    pathController.revealTargetDir("codex");
    check(urlFixture.opened.toLocalFile() == uiRoot, "open target uses resolved path");
    put(stateDir() + "/codex-skills-root.dat", (uiRoot + "/skills").toUtf8());
    qputenv("CODEX_HOME", (fixtureRoot + "/next-codex").toUtf8());
    pathController.openSkillsFolder("codex");
    check(urlFixture.opened.toLocalFile() == uiRoot + "/skills", "open skills uses recorded install root");
    QDesktopServices::unsetUrlHandler("file");

    selectFixture(temp.path(), "corrupt-state");
    put(homeDir() + "/.claude/settings.json", "{}\n");
    const QString corruptState = stateDir() + "/claude.dat";
    put(corruptState, "{broken\n");
    check(Injector::scan("claude") == InstallState::NeedsRepair, "corrupt state needs repair");
    check(!Injector::install("claude", fixtureRoot + "/prompt.md").ok &&
          Injector::readFile(corruptState) == "{broken\n" &&
          !QFileInfo::exists(homeDir() + "/.claude/CLAUDE.md"), "corrupt state is preserved without target writes");
    ItemState invalidState;
    put(corruptState, "garbage\nunknown\nnot-a-timestamp\n");
    check(!Injector::loadState("claude", invalidState), "malformed legacy state rejected");

    selectFixture(temp.path(), "legacy-state");
    const QString legacyTarget = homeDir() + "/.claude/CLAUDE.md";
    put(legacyTarget, Injector::wrapMarked("Fixture instructions\n"));
    put(stateDir() + "/claude.dat", "\n0\n1\n");
    check(Injector::uninstall("claude").ok && !QFileInfo::exists(legacyTarget), "v1 install state remains uninstallable");
    put(homeDir() + "/.claude/settings.json", "{}\n");
    const bool v2Installed = Injector::install("claude", fixtureRoot + "/prompt.md").ok;
    auto v2State = jsonAt(stateDir() + "/claude.dat");
    v2State["version"] = 2;
    v2State.remove("settings");
    v2State.remove("cacheFiles");
    put(stateDir() + "/claude.dat", QJsonDocument(v2State).toJson());
    check(v2Installed && Injector::uninstall("claude").ok && !QFileInfo::exists(legacyTarget),
          "v2 install state remains uninstallable");

    const auto settingsPlan = workbuddyFixture(temp.path(), "settings-roundtrip");
    const QString indexFile = settingsPlan.skillsDir + "/INDEX.md";
    const QByteArray originalIndex = "User-maintained index\n";
    put(indexFile, originalIndex);
    const auto originalSettings = jsonAt(settingsPlan.skillsSettings);
    const auto originalApp = jsonAt(settingsPlan.appConfig);
    check(Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok, "settings install");
    ItemState settingsState;
    check(Injector::loadState("workbuddy", settingsState) && settingsState.settings.size() == 3 &&
          jsonAt(settingsPlan.appConfig).value("localMemoryEnabled").toBool() &&
          !jsonAt(settingsPlan.skillsSettings).value("skillOverrides").toObject().contains(name),
          "only changed fields recorded including absent keys");
    check(Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok,
          "settings repeat install preserves first values");
    check(Injector::uninstall("workbuddy").ok && jsonAt(settingsPlan.skillsSettings) == originalSettings &&
          jsonAt(settingsPlan.appConfig) == originalApp && Injector::readFile(indexFile) == originalIndex,
          "settings uninstall restores original values absent keys and index");

    const auto editedPlan = workbuddyFixture(temp.path(), "settings-user-edits");
    const bool settingsInstalled = Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok;
    auto editedSettings = jsonAt(editedPlan.skillsSettings);
    auto editedOverrides = editedSettings.value("skillOverrides").toObject();
    editedOverrides[name] = "user-choice";
    editedOverrides["new-skill"] = true;
    editedSettings["skillOverrides"] = editedOverrides;
    editedSettings["theme"] = "edited";
    put(editedPlan.skillsSettings, QJsonDocument(editedSettings).toJson());
    auto editedApp = jsonAt(editedPlan.appConfig);
    editedApp["localMemoryEnabled"] = false;
    editedApp["theme"] = "edited";
    put(editedPlan.appConfig, QJsonDocument(editedApp).toJson());
    check(settingsInstalled && Injector::uninstall("workbuddy").ok &&
          jsonAt(editedPlan.skillsSettings) == editedSettings &&
          jsonAt(editedPlan.appConfig).value("localMemoryEnabled") == false &&
          jsonAt(editedPlan.appConfig).value("theme") == "edited" &&
          jsonAt(editedPlan.appConfig).value("generateMemoryEnabled") == false,
          "uninstall preserves subsequent user settings");
    check(!QFileInfo::exists(editedPlan.skillsDir + "/INDEX.md"), "uninstall removes generated index");

    selectFixture(temp.path(), "whole-file-conflict");
    put(homeDir() + "/.codex/settings.json", "{}\n");
    const bool conflictInstalled = Injector::install("codex", fixtureRoot + "/prompt.md").ok;
    const QString managed = homeDir() + "/.codex/managed-prompts/Ua.md";
    put(managed, "External edit\n");
    const auto installedAgent = Injector::readFile(homeDir() + "/.codex/AGENTS.md");
    check(conflictInstalled && !Injector::uninstall("codex").ok &&
          Injector::readFile(managed) == "External edit\n" && Injector::hasState("codex") &&
          Injector::readFile(homeDir() + "/.codex/AGENTS.md") == installedAgent,
          "whole-file conflict preserves external edits and rolls back earlier removal");

    selectFixture(temp.path(), "removed-marker");
    const QString originalAgent = homeDir() + "/.claude/CLAUDE.md";
    put(originalAgent, "Original user instructions\n");
    const bool markerInstalled = Injector::install("claude", fixtureRoot + "/prompt.md").ok;
    put(originalAgent, "User replacement without markers\n");
    check(markerInstalled && Injector::uninstall("claude").ok &&
          Injector::readFile(originalAgent) == "User replacement without markers\n",
          "removed marker does not overwrite replacement user content");

    const auto movedPlan = workbuddyFixture(temp.path(), "settings-moved-root");
    const auto movedOriginal = jsonAt(movedPlan.skillsSettings);
    const bool movedInstalled = Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok;
    const QString otherRoot = fixtureRoot + "/other-workbuddy";
    qputenv("WORKBUDDY_HOME", otherRoot.toUtf8());
    put(otherRoot + "/settings.json", "{\"untouched\":true}\n");
    check(movedInstalled && Injector::uninstall("workbuddy").ok &&
          jsonAt(movedPlan.skillsSettings) == movedOriginal &&
          Injector::readFile(otherRoot + "/settings.json") == "{\"untouched\":true}\n",
          "uninstall restores recorded settings after environment changes");

    const auto migrationPlan = workbuddyFixture(temp.path(), "settings-migration");
    const auto migrationOriginal = jsonAt(migrationPlan.skillsSettings);
    const auto migrationAppOriginal = jsonAt(migrationPlan.appConfig);
    const bool migrationInstalled = Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok;
    const QString destination = fixtureRoot + "/destination";
    qputenv("WORKBUDDY_HOME", destination.toUtf8());
    const auto destinationSettings = QJsonObject{{"skillOverrides", QJsonObject{{name, true}}}};
    const auto destinationApp = QJsonObject{{"generateMemoryEnabled", QJsonValue::Null}};
    put(destination + "/settings.json", QJsonDocument(destinationSettings).toJson());
    put(destination + "/app/app-config.json", "invalid JSON\n");
    const QByteArray migrationState = Injector::readFile(stateDir() + "/workbuddy.dat");
    const QByteArray installedSettings = Injector::readFile(migrationPlan.skillsSettings);
    check(migrationInstalled && !Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok &&
          Injector::readFile(migrationPlan.skillsSettings) == installedSettings &&
          Injector::readFile(stateDir() + "/workbuddy.dat") == migrationState &&
          Skills::installedRoot("workbuddy") == migrationPlan.skillsDir &&
          !QFileInfo::exists(destination + "/skills/INDEX.md"),
          "failed settings migration restores prior root and journal");
    put(destination + "/app/app-config.json", QJsonDocument(destinationApp).toJson());
    check(Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok &&
          jsonAt(migrationPlan.skillsSettings) == migrationOriginal &&
          jsonAt(migrationPlan.appConfig) == migrationAppOriginal,
          "successful settings migration restores previous root");
    check(Injector::uninstall("workbuddy").ok && jsonAt(destination + "/settings.json") == destinationSettings &&
          jsonAt(destination + "/app/app-config.json") == destinationApp,
          "migrated settings restore destination originals including null");

    const auto invalidJsonPlan = workbuddyFixture(temp.path(), "invalid-json");
    const QByteArray brokenJson = "{not-json\n";
    put(invalidJsonPlan.appConfig, brokenJson);
    const QByteArray beforeSettings = Injector::readFile(invalidJsonPlan.skillsSettings);
    check(!Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok &&
          Injector::readFile(invalidJsonPlan.appConfig) == brokenJson &&
          Injector::readFile(invalidJsonPlan.skillsSettings) == beforeSettings &&
          !Injector::hasState("workbuddy") &&
          !QFileInfo::exists(invalidJsonPlan.targets.first().path) &&
          !QFileInfo::exists(invalidJsonPlan.skillsDir + '/' + name + "/SKILL.md"),
          "invalid JSON fails install and rolls back earlier writes");

    const auto cachePlan = workbuddyFixture(temp.path(), "cache-rollback");
    check(Injector::install("workbuddy", fixtureRoot + "/prompt.md").ok, "cache fixture install");
    ItemState cacheState;
    check(Injector::loadState("workbuddy", cacheState), "cache fixture state");
    const QString cacheFile = fixtureRoot + "/cache.json";
    const QString cacheBlocker = fixtureRoot + "/blocked-cache";
    put(cacheFile, "cached content\n");
    QDir().mkpath(cacheBlocker);
    cacheState.cacheFiles = {cacheFile, cacheBlocker};
    check(Injector::saveState("workbuddy", cacheState), "cache fixture journal saved");
    const QByteArray cachedSettings = Injector::readFile(cachePlan.skillsSettings);
    const QByteArray cachedApp = Injector::readFile(cachePlan.appConfig);
    const QByteArray cachedTarget = Injector::readFile(cachePlan.targets.first().path);
    check(!Injector::uninstall("workbuddy").ok && Injector::hasState("workbuddy") &&
          Injector::readFile(cacheFile) == "cached content\n" &&
          Injector::readFile(cachePlan.skillsSettings) == cachedSettings &&
          Injector::readFile(cachePlan.appConfig) == cachedApp &&
          Injector::readFile(cachePlan.targets.first().path) == cachedTarget &&
          QFileInfo::exists(cachePlan.skillsDir + '/' + name + "/SKILL.md"),
          "failed cache cleanup rolls back settings targets skills and caches");

    if (app.arguments().contains("--installation-only")) {
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

    selectFixture(temp.path(), "language");
    AppController controller;
    Strings::instance()->setLang(Strings::CN);
    controller.refreshStates();
    const auto chinese = controller.toolsItems().first().toMap().value("name").toString();
    Strings::instance()->setLang(Strings::EN);
    const auto english = controller.toolsItems().first().toMap().value("name").toString();
    check(chinese != english && english == "Skills Manager", "language change invalidates snapshot");

    QFile mainQml(QStringLiteral(UA_SOURCE_DIR "/qml/Main.qml"));
    check(mainQml.open(QIODevice::ReadOnly), "QML source available");
    const QString source = QString::fromUtf8(mainQml.readAll());
    const QRegularExpression closeHandler("onClosing:.*?(?=\\n    // Tray asked)", QRegularExpression::DotMatchesEverythingOption);
    const auto match = closeHandler.match(source);
    CloseFixture closeFixture;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("app", &closeFixture);
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport QtQuick.Window\nWindow { id: root; visible: true; "
                      "property bool installInProgress: false; property bool disclaimerOpen: false; " +
                      match.captured().toUtf8() + "\n}", QUrl());
    for (bool tray : {false, true}) {
        closeFixture.trayAvailable = tray;
        closeFixture.quitCalls = 0;
        QScopedPointer<QObject> windowObject(component.create());
        auto* window = qobject_cast<QWindow*>(windowObject.data());
        if (window) window->setProperty("installInProgress", true);
        check(window && !window->close() && window->isVisible() && closeFixture.quitCalls == 0,
              "window close preserves active installation");
        if (window) window->setProperty("installInProgress", false);
        const bool closed = window && window->close();
        check(window && !window->isVisible() && (tray ? !closed && closeFixture.quitCalls == 0 :
              closed && closeFixture.quitCalls == 1), tray ? "close hides with tray" : "close exits without tray");
    }
    check(source.contains("danger: true; onClicked: root.close()"), "custom close shares native handler");
    QFile desktop(QStringLiteral(UA_SOURCE_DIR "/cmake/DeployLinux.cmake"));
    check(desktop.open(QIODevice::ReadOnly) && desktop.readAll().contains("Exec=ua\n"), "Linux launcher matches executable name");
    }

    selectFixture(temp.path(), "logo-grid");
    AppController logoController;
    QSet<QByteArray> logoHashes;
    int logoCount = 0;
    for (const Item& item : allItems()) {
        if (item.group != ItemGroup::Editor || item.logo.isEmpty()) continue;
        QString resource = item.logo;
        resource.replace("qrc:/", ":/");
        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        if (resource.endsWith(".svg")) {
            QSvgRenderer renderer(resource);
            QPainter painter(&image);
            renderer.render(&painter);
            check(renderer.isValid(), qPrintable(item.id + " logo SVG"));
        } else {
            image = QImage(resource);
        }
        check(!image.isNull(), qPrintable(item.id + " logo decoded"));
        int visiblePixels = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 16) ++visiblePixels;
            }
        }
        check(visiblePixels > image.width() * image.height() / 100,
              qPrintable(item.id + " logo has visible artwork"));
        const QByteArray pixels(reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes());
        logoHashes.insert(QCryptographicHash::hash(pixels, QCryptographicHash::Sha256));
        ++logoCount;
    }
    check(logoCount == 19 && logoHashes.size() == logoCount, "all 19 product logos are distinct");

    const Item* vscode = itemById(QStringLiteral("vscode"));
    check(vscode && vscode->name == QStringLiteral("Visual Studio Code") &&
          vscode->logo == QStringLiteral("qrc:/logos/vscode.svg"),
          "Visual Studio Code item exposes name and logo");
    if (vscode) {
        const InstallPlan plan = planFor(*vscode);
        check(plan.targets.size() == 1 &&
              plan.targets.first().path.endsWith("/.copilot/instructions/Ua.instructions.md") &&
              plan.targets.first().kind == TargetKind::Marked,
              "Visual Studio Code uses a dedicated instructions target");
    }

    qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/theme/Theme.qml")),
                             "UaApp", 1, 0, "Theme");
    qmlRegisterSingletonInstance("UaApp", 1, 0, "Strings", Strings::instance());
    qmlRegisterType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/components/IconGlyph.qml")),
                    "UaApp", 1, 0, "IconGlyph");
    qmlRegisterType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/components/Card.qml")),
                    "UaApp", 1, 0, "Card");
    qmlRegisterType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/components/IconGlyph.qml")),
                    "UaApp", 1, 0, "IconGlyph");
    QFontDatabase::addApplicationFont(":/fonts/FluentSystemIcons-Resizable.ttf");
    QQmlEngine renderEngine;
    renderEngine.rootContext()->setContextProperty("logoItems", logoController.editorItems());
    QQmlComponent scene(&renderEngine);
    scene.setData("import QtQuick\nimport QtQuick.Window\nimport UaApp 1.0\n"
                  "Window { width: 940; height: 430; visible: true; color: Theme.bg; "
                  "property int themeMode: 1; Binding { target: Theme; property: 'mode'; value: themeMode } "
                  "Grid { anchors { fill: parent; margins: 12 } columns: 3; spacing: 8; "
                  "Repeater { model: logoItems; delegate: Card { width: 300; height: 48; item: modelData } } } }",
                  QUrl());
    QScopedPointer<QObject> sceneObject(scene.create());
    auto* sceneWindow = qobject_cast<QQuickWindow*>(sceneObject.data());
    if (!sceneWindow) out << scene.errorString();
    const QString artifacts = qEnvironmentVariable("UA_UI_ARTIFACT_DIR");
    if (!artifacts.isEmpty()) QDir().mkpath(artifacts);
    for (int mode : {1, 2}) {
        if (sceneObject) sceneObject->setProperty("themeMode", mode);
        QImage frame;
        QElapsedTimer timer;
        timer.start();
        do {
            app.processEvents(QEventLoop::AllEvents, 50);
            if (sceneWindow) frame = sceneWindow->grabWindow();
        } while (sceneWindow && (frame.isNull() || timer.elapsed() < 150) && timer.elapsed() < 2000);
        check(!frame.isNull(), mode == 1 ? "dark logo grid rendered" : "light logo grid rendered");
        if (frame.isNull()) continue;
        QSet<QString> labels;
        std::function<void(QQuickItem*)> inspectLabel = [&](QQuickItem* object) {
            const QString text = object->property("text").toString();
            for (const Item& item : allItems()) {
                if (item.group == ItemGroup::Editor && text == item.name) {
                    if (object->width() > 0 && object->height() > 0) labels.insert(text);
                    else out << "Label geometry " << text << ": " << object->width() << 'x' << object->height() << '\n';
                }
            }
            for (auto* child : object->childItems()) inspectLabel(child);
        };
        inspectLabel(sceneWindow->contentItem());
        check(labels.size() == logoCount, "all product labels have visible geometry");
        const qreal scale = frame.width() / 940.0;
        int contrastTiles = 0;
        for (int index = 0; index < logoCount; ++index) {
            const int left = 22 + (index % 3) * 308;
            const int top = 22 + (index / 3) * 56;
            int minimum = 255;
            int maximum = 0;
            for (int y = top; y < top + 28; ++y) {
                for (int x = left; x < left + 28; ++x) {
                    const int gray = qGray(frame.pixel(qRound(x * scale), qRound(y * scale)));
                    minimum = qMin(minimum, gray);
                    maximum = qMax(maximum, gray);
                }
            }
            if (maximum - minimum > 60) ++contrastTiles;
        }
        check(contrastTiles == logoCount, mode == 1 ? "all dark-theme logo tiles have contrast" :
                                                               "all light-theme logo tiles have contrast");
        if (!artifacts.isEmpty()) {
            check(frame.save(artifacts + (mode == 1 ? "/logos-dark.png" : "/logos-light.png")),
                  "logo grid screenshot saved");
        }
    }
    out << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}

#include "regression.moc"
