#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
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
#include "core/file_transaction.h"
#include "core/injector.h"
#include "core/install_plan.h"
#include "core/items.h"
#include "core/prompts.h"
#include "core/skill_catalog.h"
#include "core/skills.h"
#include "core/app_strings.h"
#include "platform/instance_activation.h"

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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
                           "CLINE_HOME", "COPILOT_HOME", "VSCODE_COPILOT_HOME", "VSCODE_INSTALL_DIR", "KIMI_CODE_HOME"}) qunsetenv(key);
    ensureUserDirs();
    put(fixtureRoot + "/prompt.md", "Fixture prompt.\n");
}

static void promptTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    selectFixture(root, "prompt-routing");
    const QString directory = promptsDir();
    check(QDir::cleanPath(directory).startsWith(QDir::cleanPath(fixtureRoot) + '/'),
          "prompt fixtures stay inside the temporary user directory");
    Prompts::extractEmbedded();
    const QStringList bundled = {"Ua.md", "workbuddy.md", "gemini.md", "kimi.md"};
    QSet<QString> extracted;
    for (const auto& entry : Prompts::list()) extracted.insert(entry.name);
    check(extracted == QSet<QString>(bundled.cbegin(), bundled.cend()),
          "all four embedded top-level prompts are extracted");
    for (const auto& name : bundled) {
        const auto resource = Injector::readFile(":/prompts/" + name);
        check(!resource.isEmpty() && Injector::readFile(directory + '/' + name) == resource,
              qPrintable(name + " extracts the exact embedded bytes"));
    }

    for (const auto& item : allItems()) {
        if (item.group != ItemGroup::Editor) continue;
        const QString expected = item.id == "workbuddy" || item.id == "gemini" || item.id == "kimi"
            ? item.id + ".md" : QStringLiteral("Ua.md");
        const auto selected = Prompts::listFor(item.id);
        check(selected.size() == 1 && selected.first().name == expected &&
              selected.first().fullPath == directory + '/' + expected &&
              Prompts::pickFor(item.id) == selected.first().fullPath,
              qPrintable(item.id + " routes to " + expected));
    }
    for (const auto& id : {QStringLiteral("GEMINI"), QStringLiteral("KIMI")}) {
        const auto selected = Prompts::listFor(id);
        check(selected.size() == 1 && selected.first().name == id.toLower() + ".md",
              qPrintable(id + " dedicated prompt routing is case-insensitive"));
    }
    const auto unknown = Prompts::listFor("unlisted-editor");
    check(unknown.size() == 1 && unknown.first().name == "Ua.md",
          "unknown editors retain the original Ua prompt fallback");

    for (const auto& name : {QStringLiteral("gemini.md"), QStringLiteral("kimi.md")}) {
        const auto text = QString::fromUtf8(Injector::readFile(directory + '/' + name));
        check(text.contains(QStringLiteral("Ua \u5df2\u7ecf\u51c6\u5907\u597d\u968f\u65f6\u5f85\u547d")),
              qPrintable(name + " includes the original Chinese Ua activation reply"));
        check(text.contains(QStringLiteral("Ua is ready and standing by")),
              qPrintable(name + " includes the original English Ua activation reply"));
        check(text.contains(QStringLiteral("\u6211\u662f Ua")),
              qPrintable(name + " retains the Ua identity reply"));
    }

    for (const auto& name : bundled)
        put(directory + '/' + name, QByteArray("\xEF\xBB\xBFuser-edited ") + name.toUtf8() + "\r\n");
    put(directory + "/custom.md", "Unrelated user prompt.\n");
    Prompts::extractEmbedded();
    for (const auto& name : bundled)
        check(Injector::readFile(directory + '/' + name) ==
              QByteArray("\xEF\xBB\xBFuser-edited ") + name.toUtf8() + "\r\n",
              qPrintable(name + " preserves existing user edits and UTF-8 BOM"));
    check(Injector::readFile(directory + "/custom.md") == "Unrelated user prompt.\n",
          "prompt extraction leaves unrelated user prompts unchanged");

    for (const auto& name : {QStringLiteral("gemini.md"), QStringLiteral("kimi.md")}) {
        check(QFile::remove(directory + '/' + name), qPrintable(name + " missing-file fixture removed"));
    }
    Prompts::extractEmbedded();
    for (const auto& name : {QStringLiteral("gemini.md"), QStringLiteral("kimi.md")}) {
        const auto resource = Injector::readFile(":/prompts/" + name);
        check(!resource.isEmpty() && Injector::readFile(directory + '/' + name) == resource &&
              Prompts::pickFor(QFileInfo(name).completeBaseName()) == directory + '/' + name,
              qPrintable(name + " missing dedicated prompt is re-extracted and selected"));
    }
    for (const auto& name : {QStringLiteral("Ua.md"), QStringLiteral("workbuddy.md")})
        check(Injector::readFile(directory + '/' + name) ==
              QByteArray("\xEF\xBB\xBFuser-edited ") + name.toUtf8() + "\r\n",
              qPrintable(name + " stays unchanged when dedicated prompts are restored"));
}

class CloseFixture : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool trayAvailable MEMBER trayAvailable)
    Q_PROPERTY(bool applying MEMBER applying)
public:
    bool trayAvailable = false;
    bool applying = false;
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

static QByteArray skillBody(const QByteArray& document) {
    QByteArray bytes = document;
    Injector::stripBom(bytes);
    int cursor = bytes.indexOf('\n') + 1;
    if (cursor <= 0 || bytes.left(cursor).trimmed() != "---") return {};
    while (cursor < bytes.size()) {
        const int newline = bytes.indexOf('\n', cursor);
        const int end = newline < 0 ? bytes.size() : newline;
        if (bytes.mid(cursor, end - cursor).trimmed() == "---")
            return newline < 0 ? QByteArray() : bytes.mid(newline + 1);
        cursor = end + 1;
    }
    return {};
}

static QMap<QString, QByteArray> treeSnapshot(const QString& root) {
    if (!QFileInfo(root).isDir()) qFatal("Fixture snapshot root is missing");
    QMap<QString, QByteArray> snapshot;
    QDirIterator entries(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                         QDirIterator::Subdirectories);
    while (entries.hasNext()) {
        const QString path = entries.next();
        const QString relative = QDir(root).relativeFilePath(path);
        const QFileInfo info(path);
        if (info.isSymLink()) {
            snapshot.insert("link:" + relative, info.symLinkTarget().toUtf8());
        } else if (info.isDir()) {
            snapshot.insert("dir:" + relative, QByteArray{});
        } else {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) qFatal("Fixture snapshot file is unreadable");
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!hash.addData(&file)) qFatal("Fixture snapshot file read failed");
            snapshot.insert("file:" + relative, hash.result());
        }
    }
    return snapshot;
}

static bool createDirectoryLink(const QString& target, const QString& path) {
#if defined(Q_OS_WIN)
    const QString nativeTarget = QDir::toNativeSeparators(target);
    const QString nativePath = QDir::toNativeSeparators(path);
    if (CreateSymbolicLinkW(reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                           reinterpret_cast<LPCWSTR>(nativeTarget.utf16()),
                           SYMBOLIC_LINK_FLAG_DIRECTORY | 0x2) != 0) return true;
    QProcess junction;
    junction.start(qEnvironmentVariable("ComSpec", QStringLiteral("cmd.exe")),
                   {QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"),
                    nativePath, nativeTarget});
    if (!junction.waitForStarted(5000)) {
        if (junction.state() != QProcess::NotRunning) {
            junction.kill();
            junction.waitForFinished(5000);
        }
        return false;
    }
    if (!junction.waitForFinished(5000)) {
        junction.kill();
        junction.waitForFinished(5000);
        return false;
    }
    return junction.exitStatus() == QProcess::NormalExit && junction.exitCode() == 0;
#else
    return QFile::link(target, path);
#endif
}

static bool removeDirectoryLink(const QString& path) {
#if defined(Q_OS_WIN)
    const QString nativePath = QDir::toNativeSeparators(path);
    return RemoveDirectoryW(reinterpret_cast<LPCWSTR>(nativePath.utf16())) != 0;
#else
    return QFile::remove(path);
#endif
}

static void skillDiscoveryTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    const QString discoveryHeader = "# Ua Skill Discovery";
    const QString namesHeader = "Directory names (routing hints, not loaded skill bodies): ";
    const auto catalogObject = [](const QString& text) {
        const QString fence = "```json\n";
        const qsizetype begin = text.indexOf(fence);
        const qsizetype end = begin < 0 ? -1 : text.indexOf("```", begin + fence.size());
        if (end < 0) return QJsonObject{};
        return QJsonDocument::fromJson(text.mid(begin + fence.size(), end - begin - fence.size()).toUtf8()).object();
    };
    const auto discoveryObject = [](const QString& text) {
        const QString prefix = "Catalog: ";
        const qsizetype begin = text.indexOf(prefix);
        if (begin < 0) return QJsonObject{};
        return QJsonDocument::fromJson(text.mid(begin + prefix.size()).section('\n', 0, 0).toUtf8()).object();
    };
    const auto hasIndexRecord = [](const ItemState& state, const QString& path) {
        int matches = 0;
        for (const auto& record : state.targets)
            if (record.path == path && record.kind == static_cast<int>(TargetKind::WholeFile) &&
                !record.patchStamp.isEmpty()) ++matches;
        return matches == 1;
    };

    selectFixture(root, "skill-discovery-profiles");
    // Keep host-install probing away from optional real WorkBuddy/ZCode targets.
    put(fixtureRoot + "/local/Programs/WorkBuddy/WorkBuddy.exe", "Synthetic executable marker.\n");
    put(fixtureRoot + "/local/Programs/ZCode/ZCode.exe", "Synthetic executable marker.\n");
    QStringList directories;
    QVector<SkillMetadata> metadata;
    bool parsed = !allSkills().isEmpty();
    for (const auto& skill : allSkills()) {
        directories << skill.dirName;
        SkillMetadata entry;
        bool normalized = false;
        const auto document = SkillCatalog::normalize(
            Injector::readFile(":/skills/" + skill.dirName + "/SKILL.md"), skill.dirName, &normalized);
        parsed = normalized && SkillCatalog::parse(document, &entry) && parsed;
        metadata << entry;
    }
    check(parsed, "discovery fixture parses normalized canonical metadata for every shipped skill");
    if (directories.isEmpty()) return;
    const QMap<QString, QString> addedRoots = {
        {"windsurf", ".codeium/windsurf/skills"}, {"gemini", ".gemini/skills"},
        {"qwen", ".qwen/skills"}, {"trae", ".trae/skills"}, {"codebuddy", ".codebuddy/skills"},
        {"cline", "Documents/Cline/skills"}, {"copilot", ".copilot/skills"}
    };
    int profiles = 0;
    QSet<QString> roots;
    for (const auto& item : allItems()) {
        if (item.group != ItemGroup::Editor) continue;
        ++profiles;
        const auto plan = planFor(item);
        const QString skillsRoot = QDir::cleanPath(plan.skillsDir);
        bool contained = !item.skillsDirRel.isEmpty() && plan.valid() && plan.injectSkillIndex &&
            QDir::isAbsolutePath(skillsRoot) && skillsRoot.startsWith(QDir::cleanPath(fixtureRoot) + '/');
        for (const auto& target : plan.targets)
            contained = contained && target.path.startsWith(QDir::cleanPath(fixtureRoot) + '/');
        check(contained, qPrintable(item.id + " resolves discovery and prompt targets inside the fixture"));
        if (!contained) continue;
        roots.insert(skillsRoot);
        if (addedRoots.contains(item.id))
            check(item.skillsDirRel == addedRoots.value(item.id) && skillsRoot == homeDir() + '/' + addedRoots.value(item.id),
                  qPrintable(item.id + " publishes its newly supported local skill root"));
        const QString indexPath = skillsRoot + "/INDEX.md";
        const auto catalog = Skills::buildIndex(skillsRoot);
        const auto object = catalogObject(catalog);
        const auto entries = object.value("skills").toArray();
        bool exact = catalog.startsWith("# Ua Skill Catalog\n") && object.value("root").toString() == skillsRoot &&
            entries.size() == directories.size();
        for (qsizetype i = 0; i < entries.size() && i < directories.size(); ++i) {
            const auto entry = entries.at(i).toObject();
            exact = exact && entry.value("directory").toString() == directories.at(i) &&
                entry.value("name").toString() == metadata.at(i).name &&
                entry.value("description").toString() == metadata.at(i).description &&
                entry.value("document").toString() == skillsRoot + '/' + directories.at(i) + "/SKILL.md";
        }
        check(exact, qPrintable(item.id + " catalog retains every exact name description and absolute document path"));
        const auto full = Skills::buildDiscoveryPrompt(skillsRoot);
        const auto compact = Skills::buildDiscoveryPrompt(skillsRoot, false);
        const QJsonObject expected{{"root", skillsRoot}, {"index", indexPath}, {"count", directories.size()}};
        const qsizetype namesAt = full.indexOf(namesHeader);
        check(full.startsWith(discoveryHeader + '\n') && compact.startsWith(discoveryHeader + '\n') &&
              discoveryObject(full) == expected && discoveryObject(compact) == expected && namesAt >= 0 &&
              full.mid(namesAt + namesHeader.size()).trimmed() == directories.join(", ") &&
              !compact.contains(namesHeader) && compact.size() < full.size(),
              qPrintable(item.id + " full and compact discovery share the actual root index and count"));
        check(compact.contains("native skill list first") && compact.contains("SKILL.md") &&
              compact.contains("companion files") && compact.contains("tools are available") &&
              compact.contains("list_skills") && compact.contains("read_skill") && compact.contains("read_asset") &&
              compact.contains("nextOffset") && compact.contains("instead of claiming a skill was loaded") &&
              compact.contains("does not prove skill loading") && compact.contains("Do not execute scripts"),
              qPrintable(item.id + " compact discovery retains conditional file and MCP entrypoints without execution claims"));
        check(Skills::transactionFiles(item.id, skillsRoot, {}).contains(indexPath) && !QFileInfo::exists(indexPath),
              qPrintable(item.id + " captures the index transactionally without helper-side writes"));
    }
    check(profiles == 19 && roots.size() == 19, "all 19 editor profiles have independent discovery libraries");
    check(InstallPlan{}.injectSkillIndex && Skills::buildIndex("relative/skills").isEmpty() &&
          Skills::buildDiscoveryPrompt("relative/skills").isEmpty(),
          "discovery defaults on and rejects relative catalog roots");

    selectFixture(root, "skill-discovery-windsurf");
    const auto capped = planFor(*itemById("windsurf"));
    const QString cappedTarget = capped.targets.first().path;
    const QString cappedIndex = capped.skillsDir + "/INDEX.md";
    const QByteArray body = "# Synthetic routing body\nPreserve this entire body, including its final sentinel.\nBODY-END\n";
    const QByteArray original = "Original Windsurf user instructions.\n";
    const QString source = fixtureRoot + "/prompt.md";
    put(source, body);
    put(cappedTarget, original);
    const bool first = Injector::install("windsurf", source).ok;
    const auto firstTarget = Injector::readFile(cappedTarget);
    ItemState firstState;
    check(first && Injector::loadState("windsurf", firstState) && capped.maxChars == 6000 &&
          QString::fromUtf8(firstTarget).size() <= capped.maxChars && firstTarget.contains(namesHeader.toUtf8()) &&
          firstTarget.contains(body) && firstTarget.contains(original) && hasIndexRecord(firstState, cappedIndex),
          "capped profile includes full directory names when the complete rendered target fits");
    if (first && firstTarget.contains(namesHeader.toUtf8())) {
        const int padding = qMax(0, capped.maxChars - static_cast<int>(QString::fromUtf8(firstTarget).size())) + 100;
        const QByteArray userEdit = "\nUser text added outside the installed marker.\n" + QByteArray(padding, 'u') + "\nUSER-END\n";
        const QByteArray editedTarget = firstTarget + userEdit;
        const QByteArray expectedUserText = Injector::wipeMarked(editedTarget);
        put(cappedTarget, editedTarget);
        const bool repeated = Injector::install("windsurf", source).ok;
        const auto compactTarget = Injector::readFile(cappedTarget);
        ItemState repeatedState;
        check(repeated && Injector::loadState("windsurf", repeatedState) &&
              QString::fromUtf8(editedTarget).size() > capped.maxChars &&
              QString::fromUtf8(compactTarget).size() <= capped.maxChars &&
              compactTarget.contains(Skills::buildDiscoveryPrompt(capped.skillsDir, false).toUtf8()) &&
              !compactTarget.contains(namesHeader.toUtf8()) && compactTarget.contains(body) &&
              compactTarget.contains(original) && compactTarget.contains(userEdit) &&
              repeatedState.targets.size() == firstState.targets.size() && hasIndexRecord(repeatedState, cappedIndex),
              "rendered cap fallback keeps compact routing full body and surrounding user edits");
        check(Injector::install("windsurf", source).ok && Injector::readFile(cappedTarget) == compactTarget &&
              compactTarget.count(kMarkerBegin) == 1 && compactTarget.count(discoveryHeader.toUtf8()) == 1,
              "repeat capped installation is byte-stable without duplicate routing blocks");
        const auto stateBeforeFailure = treeSnapshot(stateDir());
        const auto indexBeforeFailure = Injector::readFile(cappedIndex);
        const QString oversizedSource = fixtureRoot + "/oversized-prompt.md";
        const QByteArray oversizedBody = QByteArray(capped.maxChars + 1, 'p') + "\nOVERSIZED-END\n";
        put(oversizedSource, oversizedBody);
        check(!Injector::install("windsurf", oversizedSource).ok &&
              Injector::readFile(cappedTarget) == compactTarget && Injector::readFile(cappedIndex) == indexBeforeFailure &&
              treeSnapshot(stateDir()) == stateBeforeFailure && Injector::readFile(oversizedSource) == oversizedBody,
              "over-cap body is rejected without truncating prompts index or ownership state");
        check(Injector::uninstall("windsurf").ok && Injector::readFile(cappedTarget) == expectedUserText &&
              !QFileInfo::exists(cappedIndex) && !Injector::hasState("windsurf") &&
              !Injector::uninstall("windsurf").ok && Injector::readFile(cappedTarget) == expectedUserText,
              "repeated capped uninstall removes discovery and preserves original plus later user text");
    } else if (first) {
        check(Injector::uninstall("windsurf").ok, "capped fixture cleanup after unexpected discovery size");
    }
    check(Injector::readFile(source) == body, "capped installation leaves its source prompt byte-exact");

    selectFixture(root, "skill-discovery-gemini");
    const auto gemini = planFor(*itemById("gemini"));
    const QString target = gemini.targets.first().path;
    const QString index = gemini.skillsDir + "/INDEX.md";
    const QString package = gemini.skillsDir + '/' + directories.first();
    const QByteArray originalPrompt = "Original Gemini instructions.\n";
    const QByteArray originalIndex = "User-maintained skill index.\r\n";
    const QByteArray originalSkill = "Synthetic preexisting skill body.\r\n";
    const QByteArray promptSource = QByteArray("\xEF\xBB\xBF") + body;
    put(fixtureRoot + "/prompt.md", promptSource);
    put(target, originalPrompt);
    put(index, originalIndex);
    put(package + "/SKILL.md", originalSkill);
    put(package + "/personal.txt", "Keep package user notes.\n");
    put(gemini.skillsDir + "/local-only/SKILL.md", "Synthetic local-only skill.\n");
    const bool installed = Injector::install("gemini", fixtureRoot + "/prompt.md").ok;
    ItemState state;
    const bool loaded = Injector::loadState("gemini", state);
    check(installed && loaded && gemini.maxChars == 0 && state.skills == directories &&
          Skills::installedRoot("gemini") == gemini.skillsDir && Skills::intact("gemini", state.skills) &&
          hasIndexRecord(state, index) && Injector::scan("gemini") == InstallState::Installed &&
          Injector::readFile(index) == Skills::buildIndex(gemini.skillsDir).toUtf8() &&
          Injector::readFile(target).contains(Skills::buildDiscoveryPrompt(gemini.skillsDir).toUtf8()) &&
          Injector::readFile(target).contains(body) && Injector::readFile(fixtureRoot + "/prompt.md") == promptSource,
          "uncapped Gemini installs one complete local catalog and discovery while preserving its source body");
    if (installed && loaded) {
        auto legacy = state;
        for (auto it = legacy.targets.begin(); it != legacy.targets.end();) {
            if (it->path == index) it = legacy.targets.erase(it);
            else ++it;
        }
        check(Injector::saveState("gemini", legacy) && Injector::scan("gemini") == InstallState::NeedsRepair,
              "old skill-bearing state without an index record requires repair even when the index exists");
        auto wrongKind = state;
        auto wrongRoot = state;
        for (auto& record : wrongKind.targets)
            if (record.path == index) record.kind = static_cast<int>(TargetKind::Marked);
        for (auto& record : wrongRoot.targets)
            if (record.path == index) record.path = fixtureRoot + "/other-library/INDEX.md";
        check(Injector::saveState("gemini", wrongKind) && Injector::scan("gemini") == InstallState::NeedsRepair &&
              Injector::saveState("gemini", wrongRoot) && Injector::scan("gemini") == InstallState::NeedsRepair,
              "repair scan requires a WholeFile index belonging to the recorded installed library");
        auto emptySkills = state;
        emptySkills.skills.clear();
        check(Injector::saveState("gemini", emptySkills) && Injector::scan("gemini") == InstallState::NeedsRepair &&
              Injector::saveState("gemini", state) && Injector::scan("gemini") == InstallState::Installed,
              "repair scan requires nonempty skills and accepts the restored complete state");
        const auto savedIndex = Injector::readFile(index);
        qputenv("GEMINI_HOME", (fixtureRoot + "/changed-home").toUtf8());
        check(Injector::scan("gemini") == InstallState::Installed,
              "Gemini scan uses the recorded installed library after its environment root changes");
        check(QFile::remove(index) && Injector::scan("gemini") == InstallState::NeedsRepair,
              "Gemini scan detects a missing recorded index independently of the current environment root");
        put(index, savedIndex);
        qunsetenv("GEMINI_HOME");
        check(Injector::scan("gemini") == InstallState::Installed,
              "restoring the same Gemini index bytes and environment restores installed status");
    }
    const QByteArray geminiEdit = "Later Gemini user instructions.\n";
    put(target, Injector::readFile(target) + geminiEdit);
    const auto expectedGemini = Injector::wipeMarked(Injector::readFile(target));
    check(installed && Injector::install("gemini", fixtureRoot + "/prompt.md").ok &&
          Injector::readFile(target).count(discoveryHeader.toUtf8()) == 1 &&
          Injector::readFile(target).contains(geminiEdit),
          "uncapped reinstall retains later prompt edits and one discovery block");
    check(Injector::uninstall("gemini").ok && Injector::readFile(target) == expectedGemini &&
          Injector::readFile(index) == originalIndex && Injector::readFile(package + "/SKILL.md") == originalSkill &&
          Injector::readFile(package + "/personal.txt") == "Keep package user notes.\n" &&
          Injector::readFile(gemini.skillsDir + "/local-only/SKILL.md") == "Synthetic local-only skill.\n" &&
          !QFileInfo::exists(package + "/scripts/check.js") && !Injector::hasState("gemini") &&
          !Injector::uninstall("gemini").ok && Injector::readFile(index) == originalIndex &&
          Injector::readFile(target) == expectedGemini && !Injector::hasState("gemini"),
          "Gemini repeat uninstall restores original index and skill while retaining unrelated user files");

    selectFixture(root, "skill-discovery-vscode");
    qputenv("VSCODE_COPILOT_HOME", (fixtureRoot + "/custom-copilot").toUtf8());
    const auto vscode = planFor(*itemById("vscode"));
    const QString native = homeDir() + "/.agents/skills";
    const QByteArray instructions = "---\napplyTo: \"**\"\n---\n\nOriginal VS Code user instructions.\n";
    put(vscode.targets.first().path, instructions);
    const bool vscodeInstalled = Injector::install("vscode", fixtureRoot + "/prompt.md").ok;
    check(vscodeInstalled && vscode.skillsDir == native && Skills::installedRoot("vscode") == native &&
          Injector::readFile(native + "/INDEX.md") == Skills::buildIndex(native).toUtf8() &&
          Injector::readFile(vscode.targets.first().path).startsWith("---\napplyTo: \"**\"\n---") &&
          Injector::readFile(vscode.targets.first().path).contains(Skills::buildDiscoveryPrompt(native).toUtf8()) &&
          !QFileInfo::exists(fixtureRoot + "/custom-copilot/skills/INDEX.md"),
          "VS Code discovery uses its existing native shared root despite the instruction override");
    check(Injector::uninstall("vscode").ok && Injector::readFile(vscode.targets.first().path) == instructions &&
          !QFileInfo::exists(native + "/INDEX.md") && !Injector::uninstall("vscode").ok &&
          Injector::readFile(vscode.targets.first().path) == instructions && !Injector::hasState("vscode"),
          "VS Code repeat uninstall restores original native instructions and removes its generated index");

    selectFixture(root, "skill-discovery-linked-index");
    const auto linked = planFor(*itemById("gemini"));
    const QString linkedIndex = linked.skillsDir + "/INDEX.md";
    const QString sentinelDir = fixtureRoot + "/index-link-target";
    put(linked.targets.first().path, originalPrompt);
    put(sentinelDir + "/sentinel.txt", "Do not overwrite this link target.\n");
    check(QDir().mkpath(linked.skillsDir), "linked index fixture creates a local library directory");
    const bool linkedCreated = createDirectoryLink(sentinelDir, linkedIndex);
    check(linkedCreated, "index fixture creates a directory symlink or junction without external paths");
    if (linkedCreated) {
        const auto sentinelBefore = treeSnapshot(sentinelDir);
        const auto stateBefore = treeSnapshot(stateDir());
        const auto backupsBefore = treeSnapshot(backupsDir());
        check(Skills::transactionFiles("gemini", linked.skillsDir, directories).isEmpty() &&
              !Injector::install("gemini", fixtureRoot + "/prompt.md").ok &&
              Injector::readFile(linked.targets.first().path) == originalPrompt &&
              treeSnapshot(sentinelDir) == sentinelBefore && treeSnapshot(stateDir()) == stateBefore &&
              treeSnapshot(backupsDir()) == backupsBefore && !Injector::hasState("gemini") &&
              !QFileInfo::exists(linked.skillsDir + '/' + directories.first() + "/SKILL.md"),
              "linked index preflight rejects installation before prompt library backup or state writes");
        const bool removed = removeDirectoryLink(linkedIndex);
        check(removed && Injector::readFile(sentinelDir + "/sentinel.txt") == "Do not overwrite this link target.\n",
              "index link cleanup removes only the fixture link and preserves its sentinel");
        if (!removed) qFatal("Fixture index link cleanup failed");
    }
}

static void sharedSkillsTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    selectFixture(root, "shared-skills-ownership");
    const Item* vscode = itemById("vscode");
    check(vscode != nullptr, "Visual Studio Code has a shared-skills installation profile");
    if (!vscode) return;
    const QString library = homeDir() + "/.agents/skills";
    const QString copilot = fixtureRoot + "/custom-copilot";
    qputenv("VSCODE_COPILOT_HOME", copilot.toUtf8());
    const auto plan = planFor(*vscode);
    check(vscode->skillsDirRel == ".agents/skills" && plan.skillsDir == library &&
          plan.targets.first().path == copilot + "/instructions/Ua.instructions.md",
          "Copilot instruction override does not relocate the native shared library");
    bool solePublisher = true;
    for (const auto& item : allItems()) {
        if (item.id != "vscode" && QDir::cleanPath(planFor(item).skillsDir) == QDir::cleanPath(library))
            solePublisher = false;
    }
    check(solePublisher && planFor(*itemById("codex")).skillsDir == homeDir() + "/.codex/skills" &&
          planFor(*itemById("claude")).skillsDir == homeDir() + "/.claude/skills",
          "only one installation owner publishes the shared library while other profiles retain their roots");
    check(allSkills().size() == 134, "shared library contains all 134 bundled skill packages");
    bool secondOwner = true;
    Skills::installInto("codex", library, &secondOwner);
    check(!secondOwner && Skills::installedNames("codex").isEmpty() && !QFileInfo::exists(library),
          "other tool installers cannot claim ownership of the reserved shared library");

    const QString name = allSkills().first().dirName;
    const QString package = library + '/' + name;
    const QString originalPath = package + "/SKILL.md";
    const QByteArray original = "Original user-owned skill.\r\n";
    put(originalPath, original);
    put(package + "/personal-note.txt", "Keep this personal file.\n");
    put(library + "/unrelated-local/SKILL.md", "Unrelated local skill.\n");
    const QString codexLocal = homeDir() + "/.codex/skills/local-skill/SKILL.md";
    const QString claudeLocal = homeDir() + "/.claude/skills/local-skill/SKILL.md";
    put(codexLocal, "Original Codex-local skill.\n");
    put(claudeLocal, "Original Claude-local skill.\n");
    bool installed = false;
    const auto names = Skills::installInto("vscode", library, &installed);
    check(installed && names.size() == allSkills().size() && Skills::installedRoot("vscode") == library &&
          Skills::installedNames("vscode") == names && Skills::intact("vscode", names),
          "shared installation records a complete intact package library under one owner");
    check(Injector::readFile(codexLocal) == "Original Codex-local skill.\n" &&
          Injector::readFile(claudeLocal) == "Original Claude-local skill.\n" &&
          !QFileInfo::exists(homeDir() + "/.codex/skills/" + name + "/SKILL.md") &&
          !QFileInfo::exists(homeDir() + "/.claude/skills/" + name + "/SKILL.md"),
          "shared installation does not rewrite or duplicate other tools' local skill packages");

    bool metadataValid = true, bodiesUnchanged = true, assetsUnchanged = true;
    QSet<QString> canonicalNames;
    int companionCount = 0;
    for (const auto& skill : allSkills()) {
        const QString resourceRoot = ":/skills/" + skill.dirName;
        const auto bundled = Injector::readFile(resourceRoot + "/SKILL.md");
        const auto deployed = Injector::readFile(library + '/' + skill.dirName + "/SKILL.md");
        const auto text = QString::fromUtf8(deployed);
        const QRegularExpression declaration("^name:[ \\t]*[\"']?" + QRegularExpression::escape(skill.dirName) +
            "[\"']?[ \\t]*\\r?$", QRegularExpression::MultilineOption);
        metadataValid = metadataValid && !deployed.isEmpty() && declaration.match(text).hasMatch() &&
            QRegularExpression("^[a-z0-9]+(?:-[a-z0-9]+)*$").match(skill.dirName).hasMatch() &&
            skill.dirName.size() <= 64 && !canonicalNames.contains(skill.dirName);
        canonicalNames.insert(skill.dirName);
        bodiesUnchanged = bodiesUnchanged && !skillBody(bundled).isEmpty() && skillBody(deployed) == skillBody(bundled);
        QDirIterator files(resourceRoot, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QString source = files.next();
            const QString relative = source.mid(resourceRoot.size() + 1);
            if (relative == "SKILL.md") continue;
            ++companionCount;
            assetsUnchanged = assetsUnchanged && Injector::readFile(source) ==
                Injector::readFile(library + '/' + skill.dirName + '/' + relative);
        }
    }
    check(metadataValid && canonicalNames.size() == 134,
          "every shared package declares a unique native skill name matching its directory");
    check(bodiesUnchanged, "front matter normalization preserves every bundled skill body byte for byte");
    check(companionCount > 0 && assetsUnchanged,
          "shared installation includes all nested bundled scripts references and assets unchanged");
    const QString changed = package + "/references/nested/guide.md";
    const QByteArray installedReference = Injector::readFile(changed);
    const QByteArray installedSkill = Injector::readFile(originalPath);
    const QByteArray namesState = Injector::readFile(stateDir() + "/vscode-skills.dat");
    const QString manifestPath = stateDir() + "/vscode-skills-backups/" +
        QString::fromLatin1(name.toUtf8().toBase64(QByteArray::Base64UrlEncoding)) + ".files.json";
    const QByteArray manifest = Injector::readFile(manifestPath);
    bool repeated = false;
    const auto repeatedNames = Skills::installInto("vscode", library, &repeated);
    check(repeated && repeatedNames == names && Injector::readFile(originalPath) == installedSkill &&
          Injector::readFile(manifestPath) == manifest && Injector::readFile(stateDir() + "/vscode-skills.dat") == namesState,
          "shared reinstall is idempotent and retains the original restoration baseline");
    put(changed, "User-edited shared reference.\n");
    bool conflictInstall = true;
    Skills::installInto("vscode", library, &conflictInstall);
    check(!installedReference.isEmpty() && !conflictInstall &&
          Injector::readFile(changed) == "User-edited shared reference.\n" &&
          Injector::readFile(originalPath) == installedSkill && Injector::readFile(manifestPath) == manifest &&
          Injector::readFile(stateDir() + "/vscode-skills.dat") == namesState,
          "shared reinstall conflict preserves external edits and rolls package ownership metadata back");
    bool conflictRemove = true;
    const int removedBeforeRepair = Skills::removeFrom(library, names, "vscode", &conflictRemove);
    check(!conflictRemove && removedBeforeRepair == 0 && Injector::readFile(originalPath) == installedSkill &&
          Injector::readFile(changed) == "User-edited shared reference.\n" &&
          Injector::readFile(manifestPath) == manifest,
          "shared uninstall conflict rolls earlier restores back without deleting edited assets");
    put(changed, installedReference);

    const QString referencesDir = package + "/references";
    const QString heldReferences = package + "/held-references";
    const bool movedReferences = QDir().rename(referencesDir, heldReferences);
    check(movedReferences, "shared companion fixture moves its reference directory");
    if (movedReferences) {
        const bool linkedReferences = createDirectoryLink(heldReferences, referencesDir);
        check(linkedReferences, "shared companion fixture redirects references through a directory link");
        if (linkedReferences) {
            check(!Skills::intact("vscode", names) &&
                  Skills::transactionFiles("vscode", library, names).isEmpty(),
                  "shared integrity and transaction capture both reject linked companion directories");
            const bool unlinked = removeDirectoryLink(referencesDir);
            check(unlinked, "shared companion fixture removes only its directory link");
            if (!unlinked) qFatal("Fixture companion link cleanup failed");
        }
        check(QDir().rename(heldReferences, referencesDir) && Skills::intact("vscode", names),
              "restoring the real companion directory restores intact status");
    }

    const QString agents = homeDir() + "/.agents";
    const QString redirectedAgents = homeDir() + "/.agents-redirect-target";
    const auto libraryBeforeRedirect = treeSnapshot(agents);
    const auto stateBeforeRedirect = treeSnapshot(stateDir());
    const bool movedAgents = QDir().rename(agents, redirectedAgents);
    check(movedAgents, "shared ancestor fixture moves the installed library within its temporary home");
    if (movedAgents) {
        const bool linkedAgents = createDirectoryLink(redirectedAgents, agents);
        check(linkedAgents, "shared ancestor fixture redirects .agents through a directory link or junction");
        if (linkedAgents) {
            const auto unchanged = [&] {
                return treeSnapshot(redirectedAgents) == libraryBeforeRedirect &&
                    treeSnapshot(stateDir()) == stateBeforeRedirect;
            };
            check(!Skills::intact("vscode", names),
                  "shared integrity rejects a redirected home ancestor");
            check(Skills::transactionFiles("vscode", library, names).isEmpty() && unchanged(),
                  "shared transaction capture rejects a redirected ancestor without changing target or ownership state");
            check(Skills::transactionFiles(QString(), library, names).isEmpty() && unchanged(),
                  "shared anonymous transaction capture also rejects a redirected ancestor");
            bool redirectRemoved = true;
            check(Skills::removeFrom(library, names, "vscode", &redirectRemoved) == 0 &&
                  !redirectRemoved && unchanged(),
                  "shared explicit-owner removal rejects a redirected ancestor without changing target or ownership state");
            check(Skills::removeFrom(library, names) == 0 && unchanged(),
                  "shared inferred-owner removal rejects a redirected ancestor without changing target or ownership state");
            bool anonymousRemoved = true;
            check(Skills::removeFrom(library, names, QString(), &anonymousRemoved) == 0 &&
                  !anonymousRemoved && unchanged(),
                  "shared anonymous removal also rejects a redirected ancestor");
            bool redirectInstalled = true;
            check(Skills::installInto("vscode", library, &redirectInstalled).isEmpty() &&
                  !redirectInstalled && unchanged(),
                  "shared reinstall rejects a redirected ancestor without changing target or ownership state");
            const bool unlinkedAgents = removeDirectoryLink(agents);
            check(unlinkedAgents, "shared ancestor fixture removes only its directory link or junction");
            if (!unlinkedAgents) qFatal("Fixture ancestor link cleanup failed");
        }
        const bool restoredAgents = QDir().rename(redirectedAgents, agents);
        check(restoredAgents, "shared ancestor fixture restores the original directory");
        if (!restoredAgents) qFatal("Fixture ancestor directory restore failed");
        check(treeSnapshot(agents) == libraryBeforeRedirect && treeSnapshot(stateDir()) == stateBeforeRedirect &&
              Skills::intact("vscode", names),
              "shared ancestor rejection preserves byte-exact files and ownership metadata after fixture restoration");
    }

    AppController controller;
    const QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(
#if defined(Q_OS_WIN)
        "ua-skills-mcp.exe"
#else
        "ua-skills-mcp"
#endif
    );
    QStringList configFormats = {"vscode", "generic", "codex"};
    for (const auto& format : configFormats) {
        const QString config = controller.skillsMcpConfig(format);
        bool valid = false;
        if (format == "codex") {
            valid = config.contains("[mcp_servers.ua-skills]") && config.contains("--root") &&
                config.contains(library) && config.contains(executable);
        } else {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(config.toUtf8(), &error);
            const auto servers = document.object().value(format == "vscode" ? "servers" : "mcpServers").toObject();
            const auto server = servers.value("ua-skills").toObject();
            const auto arguments = server.value("args").toArray();
            valid = error.error == QJsonParseError::NoError && document.isObject() &&
                server.value("command").toString() == executable && arguments == QJsonArray{"--root", library} &&
                (format != "vscode" || server.value("type").toString() == "stdio");
        }
        check(valid, qPrintable(format + " MCP configuration selects the packaged server and the same shared library"));
        QApplication::clipboard()->setText("Previous clipboard text");
        check(controller.copySkillsMcpConfig(format) && QApplication::clipboard()->text() == config,
              qPrintable(format + " MCP configuration round-trips through the clipboard"));
    }
    QApplication::clipboard()->setText("Keep unrelated clipboard text");
    check(controller.skillsMcpConfig("unsupported").isEmpty() && !controller.copySkillsMcpConfig("unsupported") &&
          QApplication::clipboard()->text() == "Keep unrelated clipboard text",
          "invalid MCP formats preserve the existing clipboard");
    UrlFixture folder;
    QDesktopServices::setUrlHandler("file", &folder, "open");
    controller.openSkillsFolder("vscode");
    QDesktopServices::unsetUrlHandler("file");
    check(QDir::cleanPath(folder.opened.toLocalFile()) == QDir::cleanPath(library),
          "shared skills folder action opens the same native and MCP library");

    bool removed = false;
    check(Skills::removeFrom(library, names, "vscode", &removed) == names.size() && removed &&
          Injector::readFile(originalPath) == original &&
          Injector::readFile(package + "/personal-note.txt") == "Keep this personal file.\n" &&
          Injector::readFile(library + "/unrelated-local/SKILL.md") == "Unrelated local skill.\n" &&
          !QFileInfo::exists(changed) && !QFileInfo::exists(manifestPath),
          "shared uninstall restores originals removes owned assets and preserves unrelated local files");

    selectFixture(root, "shared-skills-legacy-repair");
    const auto legacyPlan = planFor(*vscode);
    const QString target = legacyPlan.targets.first().path;
    const QByteArray originalInstructions = "---\napplyTo: \"**\"\n---\n\nExisting personal instruction.\n";
    put(target, Injector::insertMarked(originalInstructions, Injector::wrapMarked("Legacy prompt only.\n")));
    const QString originalBackup = backupsDir() + "/vscode-legacy-original.md";
    put(originalBackup, originalInstructions);
    ItemState legacy;
    legacy.installedAt = QDateTime::currentMSecsSinceEpoch();
    legacy.targets.push_back({target, originalBackup, true, static_cast<int>(TargetKind::Marked), {}});
    check(Injector::saveState("vscode", legacy) && Injector::scan("vscode") == InstallState::NeedsRepair,
          "legacy prompt-only Visual Studio Code state is flagged for shared skill repair");
    const auto repaired = Injector::install("vscode", fixtureRoot + "/prompt.md");
    ItemState repairedState;
    check(repaired.ok && Injector::loadState("vscode", repairedState) && repairedState.skills.size() == 134 &&
          Skills::installedRoot("vscode") == legacyPlan.skillsDir &&
          Skills::intact("vscode", repairedState.skills) && Injector::scan("vscode") == InstallState::Installed,
          "reinstall repairs a legacy prompt-only state with all 134 native and MCP skill packages");
    check(Injector::readFile(target).contains(originalInstructions.trimmed()) &&
          Injector::readFile(target).contains("applyTo: \"**\""),
          "shared repair preserves personal instructions and Copilot front matter");
    check(Injector::uninstall("vscode").ok && Injector::readFile(target) == originalInstructions &&
          !Injector::hasState("vscode") && !QFileInfo::exists(legacyPlan.skillsDir + '/' + name + "/SKILL.md"),
          "shared repair remains fully uninstallable to the original prompt-only baseline");
    Injector::setProgressCallback({});
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
    bool busy = false, reentered = false, rejected = false, locationRejected = false, finalStateSaved = false;
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
            locationRejected = !controller.setVscodeLocation(fixtureRoot + "/invalid.exe");
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
    check(ticks > 0 && reentered && rejected && locationRejected,
          "UI stays responsive and configuration changes are rejected during install");
    int last = -1, total = events.isEmpty() ? 0 : events.first()[1].toInt();
    bool ordered = !events.isEmpty();
    QSet<QString> skillPaths;
    QSet<QString> expectedSkillPaths;
    const QString skillRoot = planFor(*itemById("codex")).skillsDir;
    for (const auto& skill : allSkills())
        expectedSkillPaths.insert(skillRoot + '/' + skill.dirName + "/SKILL.md");
    for (const auto& event : events) {
        const int current = event[0].toInt();
        ordered = ordered && current >= last && current <= total && event[1].toInt() == total;
        last = current;
        const auto path = event[2].toString();
        if (expectedSkillPaths.contains(path)) skillPaths.insert(path);
    }
    check(ordered && events.first()[0].toInt() == 0 && last == total && finalStateSaved,
          "progress is monotonic and reaches 100 only after journal save");
    check(skillPaths == expectedSkillPaths, "progress includes every top-level bundled skill");
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

static void detectionRefreshTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    selectFixture(root, "detection-refresh");
    AppController controller;
    QWindow window;
    controller.attachMainWindow(&window);
    const auto codexDetected = [&]() {
        for (const auto& item : controller.editorItems()) {
            const auto value = item.toMap();
            if (value.value("id").toString() == "codex") return value.value("detected").toBool();
        }
        return false;
    };
    check(!codexDetected(), "initial snapshot reflects absent tool");
    const QString config = homeDir() + "/.codex/config.toml";
    put(config, "model = \"fixture\"\n");
    check(!codexDetected(), "cached snapshot awaits refresh after external installation");
    controller.showMainWindow();
    check(codexDetected(), "restoring the window detects an externally installed tool");
    check(QFile::remove(config), "external uninstall fixture removed");
    controller.showMainWindow();
    check(!codexDetected(), "restoring the window detects an externally removed tool");

    const QString editor = fixtureRoot + "/Portable Tools/Visual Studio Code";
#if defined(Q_OS_WIN)
    const QString executable = editor + "/Code.exe";
#else
    const QString executable = editor + "/code";
#endif
    put(executable, "Fixture only. Never execute.\n");
    check(QFile::setPermissions(executable, QFile::permissions(executable) | QFileDevice::ExeOwner |
                               QFileDevice::ExeGroup | QFileDevice::ExeOther),
          "manual editor fixture has executable permissions");
    const QString copilotRoot = fixtureRoot + "/vscode-instructions";
    const QString cliRoot = fixtureRoot + "/copilot-cli";
    qputenv("VSCODE_COPILOT_HOME", copilotRoot.toUtf8());
    qputenv("COPILOT_HOME", cliRoot.toUtf8());
    put(copilotRoot + "/instructions/existing.instructions.md", "User instruction content.\n");
    put(cliRoot + "/config.json", "{\"owner\":\"cli\"}\n");
    const QString instructionTarget = planFor(*itemById("vscode")).targets.first().path;
    const QString saved = configDir() + "/vscode-install-path.txt";
    const auto vscodeSnapshot = [](AppController& app) {
        for (const auto& item : app.editorItems())
            if (item.toMap().value("id").toString() == "vscode") return item.toMap();
        return QVariantMap{};
    };
    int refreshed = 0, errors = 0;
    QObject::connect(&controller, &AppController::itemsChanged, &controller, [&] { ++refreshed; });
    QObject::connect(&controller, &AppController::errorRaised, &controller, [&](const QString&) { ++errors; });
    check(!controller.setVscodeLocation(fixtureRoot + "/missing/Code.exe") &&
          !QFileInfo::exists(saved) && refreshed == 0 && errors == 1,
          "invalid manual selection leaves existing settings and snapshot untouched");
    check(controller.setVscodeLocation(executable) && refreshed == 1 &&
          vscodeSnapshot(controller).value("detected").toBool() &&
          vscodeSnapshot(controller).value("editorInstallDir").toString() == editor,
          "manual executable selection immediately refreshes the Visual Studio Code card");
    const QByteArray stored = Injector::readFile(saved);
    check(QString::fromUtf8(stored).trimmed() == editor,
          "selected editor location is persisted under the application config directory");
    const QString unrelated = fixtureRoot + "/other.exe";
    put(unrelated, "Not an editor.\n");
    check(!controller.setVscodeLocation(unrelated) && refreshed == 1 && errors == 2 &&
          Injector::readFile(saved) == stored &&
          vscodeSnapshot(controller).value("editorInstallDir").toString() == editor,
          "invalid replacement preserves the previous valid editor location");
    controller.setUpdateInProgress(true);
    check(!controller.setVscodeLocation(QString()) && Injector::readFile(saved) == stored && refreshed == 1,
          "update preparation blocks manual editor configuration changes");
    controller.setUpdateInProgress(false);
    {
        AppController relaunched;
        check(vscodeSnapshot(relaunched).value("detected").toBool() &&
              vscodeSnapshot(relaunched).value("editorInstallDir").toString() == editor,
              "a new controller restores the persisted editor location");
    }
    check(toolConfigRoot("vscode") == copilotRoot && toolConfigRoot("copilot") == cliRoot &&
          planFor(*itemById("vscode")).targets.first().path == instructionTarget &&
          Injector::readFile(copilotRoot + "/instructions/existing.instructions.md") == "User instruction content.\n" &&
          Injector::readFile(cliRoot + "/config.json") == "{\"owner\":\"cli\"}\n" &&
          !QFileInfo::exists(instructionTarget),
          "manual editor selection neither redirects nor writes Copilot or Copilot CLI configuration");
    check(controller.setVscodeLocation(QString()) && !QFileInfo::exists(saved) && refreshed == 2,
          "clearing a selected editor location restores automatic detection");
    Injector::setProgressCallback({});
}

static void vscodeUiTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    selectFixture(root, "vscode-location-ui");
    AppController controller;
    qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/theme/Theme.qml")),
                             "UaApp", 1, 0, "Theme");
    qmlRegisterSingletonInstance("UaApp", 1, 0, "Strings", Strings::instance());
    for (const auto& type : {"IconGlyph", "TitleButton"})
        qmlRegisterType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/components/") + type + ".qml"),
                        "UaApp", 1, 0, type);
    qmlRegisterType(QUrl::fromLocalFile(QStringLiteral(UA_SOURCE_DIR "/qml/dialogs/InjectDialog.qml")),
                    "UaApp", 1, 0, "InjectDialog");
    QFontDatabase::addApplicationFont(":/fonts/FluentSystemIcons-Resizable.ttf");
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError>& errors) {
        for (const auto& error : errors) warnings << error.toString();
    });
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport QtQuick.Window\nimport UaApp 1.0\n"
        "Window { width: 460; height: 524; visible: true; color: Theme.bg; "
        "property bool fixtureAbsent: true; "
        "InjectDialog { objectName: 'dialog'; itemId: 'vscode'; "
        "item: fixtureAbsent ? ({ id: 'vscode', name: 'Visual Studio Code', "
        "logo: 'qrc:/logos/vscode.svg', supported: true, detected: false, state: 0, "
        "hasTips: true, targetRel: '.copilot/instructions/Ua.instructions.md' }) "
        ": app.items.find(it => it.id === itemId) } }", QUrl());
    QScopedPointer<QObject> object(component.create());
    auto* window = qobject_cast<QQuickWindow*>(object.data());
    check(window != nullptr, "Visual Studio Code location dialog loads");
    if (!window) {
        QTextStream(stderr) << component.errorString();
        Injector::setProgressCallback({});
        return;
    }
    auto* dialog = window->findChild<QQuickItem*>("dialog");
    auto* panel = window->findChild<QQuickItem*>("injectDialogPanel");
    auto* choose = window->findChild<QQuickItem*>("chooseVscodeLocationButton");
    auto* refresh = window->findChild<QQuickItem*>("refreshVscodeLocationButton");
    auto* location = window->findChild<QQuickItem*>("editorLocationLabel");
    auto* status = window->findChild<QQuickItem*>("toolDetectionLabel");
    auto* promptsLabel = window->findChild<QQuickItem*>("injectDialogPromptsLabel");
    auto* actions = window->findChild<QQuickItem*>("injectDialogActions");
    auto* install = window->findChild<QQuickItem*>("installButton");
    auto* sharedControls = window->findChild<QQuickItem*>("sharedSkillsControls");
    auto* sharedStatus = window->findChild<QQuickItem*>("sharedSkillsStatus");
    auto* openShared = window->findChild<QQuickItem*>("openSharedSkillsButton");
    auto* mcpFormat = window->findChild<QQuickItem*>("skillsMcpFormat");
    auto* copyMcp = window->findChild<QQuickItem*>("copySkillsMcpButton");
    check(dialog && panel && choose && refresh && location && status && promptsLabel && actions && install,
          "Visual Studio Code dialog exposes location, refresh and original installation controls");
    if (!dialog || !panel || !choose || !refresh || !location || !status || !promptsLabel || !actions || !install) {
        Injector::setProgressCallback({});
        return;
    }
    check(sharedControls && sharedStatus && openShared && mcpFormat && copyMcp,
          "Visual Studio Code dialog exposes shared skill status folder and all three MCP configuration controls");
    if (!sharedControls || !sharedStatus || !openShared || !mcpFormat || !copyMcp) {
        Injector::setProgressCallback({});
        return;
    }
    auto pump = [] {
        QElapsedTimer timer;
        timer.start();
        do {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        } while (timer.elapsed() < 180);
    };
    pump();
    check(choose->isVisible() && refresh->isVisible() && !install->property("actionable").toBool(),
          "missing-editor dialog keeps manual selection and refresh available");
    check(actions->childItems().size() == 4 && panel->width() == 380 && panel->height() == 440,
          "manual selection preserves the four-button footer and original dialog size");
    check(sharedControls->isVisible() && sharedStatus->isVisible() &&
          sharedStatus->property("text").toString().contains("/134"),
          "shared library readiness remains visible before an editor is detected");
    const QStringList formats = {"vscode", "generic", "codex"};
    for (int index = 0; index < formats.size(); ++index) {
        mcpFormat->setProperty("currentIndex", index);
        QApplication::clipboard()->setText("Before dialog action");
        check(QMetaObject::invokeMethod(copyMcp, "triggered") &&
              QApplication::clipboard()->text() == controller.skillsMcpConfig(formats[index]),
              qPrintable(formats[index] + " MCP dialog control copies its matching configuration"));
    }
    const QString editor = fixtureRoot + "/Portable Tools/Visual Studio Code";
#if defined(Q_OS_WIN)
    const QString executable = editor + "/Code - Insiders.exe";
#else
    const QString executable = editor + "/code-insiders";
#endif
    put(executable, "Fixture only. Never execute.\n");
    check(QFile::setPermissions(executable, QFile::permissions(executable) | QFileDevice::ExeOwner |
                               QFileDevice::ExeGroup | QFileDevice::ExeOther),
          "Insiders UI fixture is executable without launching it");
    window->setProperty("fixtureAbsent", false);
    check(controller.setVscodeLocation(executable), "UI accepts an Insiders executable in a custom directory");
    pump();
    check(dialog->property("item").toMap().value("detected").toBool() &&
          location->property("text").toString() == editor && choose->isVisible() && refresh->isVisible() &&
          install->property("actionable").toBool() && !Injector::hasState("vscode"),
          "manual selection updates the live dialog and enables install without performing injection");
    int refreshes = 0;
    QObject::connect(&controller, &AppController::itemsChanged, &controller, [&] { ++refreshes; });
    check(QMetaObject::invokeMethod(refresh, "triggered") && refreshes == 1,
          "refresh control reruns application detection");
    const QString artifacts = qEnvironmentVariable("UA_UI_ARTIFACT_DIR");
    if (!artifacts.isEmpty()) QDir().mkpath(artifacts);
    auto* theme = engine.singletonInstance<QObject*>(qmlTypeId("UaApp", 1, 0, "Theme"));
    for (bool absent : {true, false}) {
        window->setProperty("fixtureAbsent", absent);
        for (const auto& size : {QSize(460, 524), QSize(640, 720)}) {
            window->resize(size);
            for (int mode : {1, 2}) for (auto language : {Strings::CN, Strings::EN}) {
                if (theme) theme->setProperty("mode", mode);
                Strings::instance()->setLang(language);
                pump();
                const auto frame = window->grabWindow();
                const auto chooseRect = choose->mapRectToItem(panel, choose->boundingRect());
                const auto refreshRect = refresh->mapRectToItem(panel, refresh->boundingRect());
                const auto promptsRect = promptsLabel->mapRectToItem(panel, promptsLabel->boundingRect());
                bool footerFits = true;
                for (auto* button : actions->childItems()) {
                    const auto bounds = button->mapRectToItem(panel, button->boundingRect());
                    footerFits = footerFits && button->width() == 72 && button->height() == 30 &&
                        bounds.left() >= 0 && bounds.right() <= panel->width() && bounds.bottom() <= panel->height();
                }
                bool labelsFit = true;
                for (auto* button : {choose, refresh}) {
                    for (auto* child : button->childItems()) {
                        if (child->property("text").isValid())
                            labelsFit = labelsFit && child->width() + 8 <= button->width();
                    }
                }
                check(!frame.isNull() && choose->isVisible() && refresh->isVisible() && labelsFit && footerFits &&
                      chooseRect.left() >= 0 && refreshRect.right() <= panel->width() - 16 &&
                      chooseRect.bottom() < promptsRect.top() && refreshRect.bottom() < promptsRect.top() &&
                      !chooseRect.intersects(refreshRect),
                      "localized editor location controls render and fit without overlapping existing controls");
                check(status->property("text").toString() == Strings::instance()->get(
                          absent ? Strings::ToolNotFound : Strings::NotInjected),
                      "editor status remains localized before and after location selection");
                const auto sharedRect = sharedControls->mapRectToItem(panel, sharedControls->boundingRect());
                const auto sharedStatusRect = sharedStatus->mapRectToItem(panel, sharedStatus->boundingRect());
                const auto footerRect = actions->mapRectToItem(panel, actions->boundingRect());
                bool sharedFits = sharedControls->isVisible() && sharedStatus->isVisible() &&
                    sharedRect.left() >= 16 && sharedRect.right() <= panel->width() - 16 &&
                    sharedRect.top() > promptsRect.bottom() && sharedRect.bottom() < footerRect.top() &&
                    sharedStatus->property("contentWidth").toReal() <= sharedStatus->width();
                QList<QRectF> sharedButtons;
                for (auto* button : {openShared, mcpFormat, copyMcp}) {
                    const auto bounds = button->mapRectToItem(panel, button->boundingRect());
                    sharedFits = sharedFits && button->isVisible() && bounds.left() >= 16 &&
                        bounds.right() <= panel->width() - 16 && bounds.top() > sharedStatusRect.bottom() &&
                        bounds.bottom() < footerRect.top();
                    for (const auto& other : sharedButtons) sharedFits = sharedFits && !bounds.intersects(other);
                    for (auto* child : button->childItems()) {
                        if (child->isVisible() && child->property("contentWidth").isValid())
                            sharedFits = sharedFits && child->property("contentWidth").toReal() <= button->width() - 8;
                    }
                    sharedButtons << bounds;
                }
                check(sharedFits,
                      "localized shared skills controls fit the original dialog without text clipping or footer overlap");
                if (!artifacts.isEmpty())
                    check(frame.save(artifacts + QString("/vscode-location-%1-%2-%3-%4.png")
                        .arg(absent ? "absent" : "detected").arg(size.width()).arg(mode).arg(language)),
                          "editor location screenshot saved");
            }
        }
    }
    window->setProperty("fixtureAbsent", false);
    dialog->setProperty("itemId", "codex");
    pump();
    check(!sharedControls->isVisible() && !sharedStatus->isVisible() && !openShared->isVisible() &&
          !mcpFormat->isVisible() && !copyMcp->isVisible() &&
          !choose->isVisible() && !refresh->isVisible(),
          "other tool dialogs keep their existing controls without Visual Studio Code shared-library actions");
    check(warnings.isEmpty(), "Visual Studio Code location dialog has no QML runtime warnings");
    if (!warnings.isEmpty()) QTextStream(stderr) << warnings.join('\n') << '\n';
    Injector::setProgressCallback({});
}

static void skillAssetTests(const std::function<void(bool, const char*)>& check) {
    check(Skills::missingBundledAssets("fixture-missing-assets") ==
              QStringList{"references/missing.md", "scripts/missing.js"},
          "asset audit detects missing local links and commands without repository-root false positives");
    for (const auto& skill : allSkills()) {
        const auto missing = Skills::missingBundledAssets(skill.dirName);
        check(missing.isEmpty(), qPrintable(skill.dirName + " bundles every referenced companion: " + missing.join(", ")));
    }
}

static void featureTests(const QString& root, const std::function<void(bool, const char*)>& check) {
    skillAssetTests(check);
    detectionRefreshTests(root, check);
    selectFixture(root, "update-lifecycle");
    {
        AppController controller;
        controller.setUpdateInProgress(true);
        check(!controller.install("codex", "unused") && !controller.uninstall("codex") &&
              !controller.removeSkill("unused"), "update preparation blocks new configuration writes");
        bool survivedQuit = false;
        QTimer::singleShot(0, &controller, &AppController::quit);
        QTimer::singleShot(20, &controller, [&] {
            survivedQuit = true;
            controller.setUpdateInProgress(false);
            controller.quit();
        });
        QCoreApplication::exec();
        check(survivedQuit, "tray quit waits until update preparation has finished");
        Injector::setProgressCallback({});
    }
    selectFixture(root, "workbuddy-native-root");
    qputenv("WORKBUDDY_HOME", (fixtureRoot + "/legacy-override").toUtf8());
    const QString nativeRoot = fixtureRoot + "/native-override";
    qputenv("WORKBUDDY_CONFIG_DIR", nativeRoot.toUtf8());
    check(toolConfigRoot("workbuddy") == nativeRoot &&
          toolPath("workbuddy", ".workbuddy/skills") == nativeRoot + "/skills",
          "WorkBuddy uses its native config environment variable before legacy alias");
    selectFixture(root, "file-transaction-capture");
    const QString transactionExisting = fixtureRoot + "/existing.txt";
    const QString transactionAbsent = fixtureRoot + "/absent.txt";
    const QString transactionRetry = fixtureRoot + "/retry.txt";
    put(transactionExisting, "first preimage\n");
    {
        detail::FileTransaction transaction;
        check(transaction.capture(transactionExisting) && transaction.capture(transactionAbsent),
              "file transaction captures existing and absent preimages");
        put(transactionExisting, "intermediate bytes\n");
        put(transactionAbsent, "new bytes\n");
        check(transaction.capture(transactionExisting) && transaction.capture(transactionAbsent),
              "duplicate capture retains the first existing and absent preimages");
        check(QDir().mkpath(transactionRetry) && !transaction.capture(transactionRetry) &&
              QDir().rmdir(transactionRetry), "failed transaction capture leaves its path retryable");
        put(transactionRetry, "retry preimage\n");
        check(transaction.capture(transactionRetry), "transaction retries a previously failed capture");
        put(transactionExisting, "final bytes\n");
        put(transactionAbsent, "final new bytes\n");
        put(transactionRetry, "changed retry bytes\n");
        check(transaction.rollback() && Injector::readFile(transactionExisting) == "first preimage\n" &&
              !QFileInfo::exists(transactionAbsent) && Injector::readFile(transactionRetry) == "retry preimage\n",
              "transaction rollback restores first preimages and successful retry preimage");
    }
    selectFixture(root, "package-files");
    const QString name = allSkills().first().dirName;
    const QString skillsRoot = fixtureRoot + "/skills";
    const QString package = skillsRoot + '/' + name;
    const QString script = package + "/scripts/check.js";
    const QString reference = package + "/references/nested/guide.md";
    const QString asset = package + "/assets/sample.csv";
    const QString restoredReference = skillsRoot + "/rea-analysis-optimization/references/kuna-case-study.md";
    const auto preexistingReference = Injector::readFile(":/skills/rea-analysis-optimization/references/kuna-case-study.md");
    const QString meta = stateDir() + "/claude-skills-backups/" +
        QString::fromLatin1(name.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
    put(script, "original script\n");
    put(restoredReference, preexistingReference);
    put(package + "/user.txt", "keep me\n");
    bool installed = false;
    const auto names = Skills::installInto("claude", skillsRoot, &installed);
    check(installed && names.size() == allSkills().size() && QFileInfo::exists(reference) &&
          Injector::readFile(asset) == Injector::readFile(":/skills/" + name + "/assets/sample.csv"),
          "package includes nested references and assets");
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
    const QString restoredScript = skillsRoot + "/web-security/scripts/owasp_scanner.py";
    check(!Injector::readFile(restoredReference).isEmpty() &&
          Injector::readFile(restoredReference) == Injector::readFile(":/skills/rea-analysis-optimization/references/kuna-case-study.md") &&
          !Injector::readFile(restoredScript).isEmpty() &&
          Injector::readFile(restoredScript) == Injector::readFile(":/skills/web-security/scripts/owasp_scanner.py"),
          "restored upstream references and scripts deploy byte for byte");
    check(QFile::remove(restoredReference) && !Skills::intact("claude", names),
          "a missing installed companion still requires repair");
    const auto damagedStatuses = Skills::statuses();
    check(std::any_of(damagedStatuses.cbegin(), damagedStatuses.cend(), [](const SkillStatus& status) {
        return status.dirName == "rea-analysis-optimization" && status.installedCount == 1 &&
               status.needsRepair && status.missingAssets.isEmpty();
    }), "skill manager distinguishes damaged local deployment from complete bundled resources");
    put(restoredReference, Injector::readFile(":/skills/rea-analysis-optimization/references/kuna-case-study.md"));
    const QString referenceManifest = stateDir() + "/claude-skills-backups/" +
        QString::fromLatin1(QByteArray("rea-analysis-optimization").toBase64(QByteArray::Base64UrlEncoding)) + ".files.json";
    const auto currentReferenceManifest = Injector::readFile(referenceManifest);
    put(referenceManifest, "{\"version\":1,\"files\":[\"SKILL.md\"]}");
    check(!Skills::intact("claude", names),
          "old manifests missing newly bundled companions require repair even when files were copied manually");
    bool upgradedManifest = false;
    Skills::installInto("claude", skillsRoot, &upgradedManifest);
    check(upgradedManifest && Injector::readFile(referenceManifest) == currentReferenceManifest &&
          Skills::intact("claude", names), "reinstall upgrades old entry-only manifests and preserves existing restoration stamps");
    const QString unchangedName = "rea-changelog-update";
    const QString unchangedStamp = stateDir() + "/claude-skills-backups/" +
        QString::fromLatin1(unchangedName.toUtf8().toBase64(QByteArray::Base64UrlEncoding)) + ".sha256";
    const QString unchangedPath = skillsRoot + '/' + unchangedName + "/SKILL.md";
    const auto currentBytes = Injector::readFile(unchangedPath);
    const QByteArray previousBytes = "Previously shipped skill body.\n";
    put(unchangedPath, previousBytes);
    put(unchangedStamp, QCryptographicHash::hash(previousBytes, QCryptographicHash::Sha256).toHex());
    check(!Skills::intact("claude", names), "same-path content revisions require package refresh");
    bool refreshed = false;
    Skills::installInto("claude", skillsRoot, &refreshed);
    check(refreshed && Injector::readFile(unchangedPath) == currentBytes && Skills::intact("claude", names),
          "reinstall refreshes unchanged paths from their recorded older release without treating them as user edits");
    check(Skills::transactionFiles("claude", skillsRoot, {name}).contains(reference),
          "outer transaction captures companion files");
    const QString scriptStamp = meta + '-' + QString::fromLatin1(QCryptographicHash::hash(
        QByteArray("scripts/check.js"), QCryptographicHash::Sha256).toHex()) + ".sha256";
    const auto installedStamp = Injector::readFile(scriptStamp);
    const auto preservedTime = QDateTime::fromMSecsSinceEpoch(946684800000LL, Qt::UTC);
    auto backdate = [&](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadWrite) && file.setFileTime(preservedTime, QFileDevice::FileModificationTime);
    };
    check(backdate(script) && backdate(scriptStamp), "managed companion and stamp timestamps can be backdated");
    const auto scriptModified = QFileInfo(script).lastModified();
    const auto stampModified = QFileInfo(scriptStamp).lastModified();
    bool reinstalled = false;
    Skills::installInto("claude", skillsRoot, &reinstalled);
    check(reinstalled, "complete package reinstall is idempotent");
    check(Injector::readFile(script) == installedScript && Injector::readFile(scriptStamp) == installedStamp &&
          QFileInfo(script).lastModified() == scriptModified && QFileInfo(scriptStamp).lastModified() == stampModified,
          "unchanged managed companion reinstall preserves file and stamp bytes and modification times");
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
    check(Injector::readFile(restoredReference) == preexistingReference && QFileInfo::exists(restoredReference),
          "uninstall restores an untracked preexisting companion even when its bytes matched the bundle");
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
    check(!Skills::intact("claude", {name}), "legacy entry-only installs require companion deployment");
    const auto upgraded = Skills::installInto("claude", legacyRoot, &installed);
    Skills::removeFrom(legacyRoot, upgraded, "claude", &removed);
    check(installed && removed && !QFileInfo::exists(legacyPackage + "/SKILL.md") &&
          Injector::readFile(legacyPackage + "/assets/sample.csv") == companion,
          "legacy upgrade owns only old SKILL.md and backs up preexisting companions");

    selectFixture(root, "unmanaged-same-name");
    put(homeDir() + "/.claude/skills/" + name + "/SKILL.md", "Personal skill with an independently chosen layout.\n");
    const auto personalStatuses = Skills::statuses();
    check(std::any_of(personalStatuses.cbegin(), personalStatuses.cend(), [&](const SkillStatus& status) {
        return status.dirName == name && status.installedCount == 1 && !status.needsRepair;
    }), "independent same-name user skills are counted without enforcing Ua package ownership or repair");

    selectFixture(root, "activation");
#ifdef Q_OS_UNIX
    // macOS local sockets have a short path limit; nested fixtures exceed it.
    QTemporaryDir activationTemp("/tmp/ua-XXXXXX");
    if (!activationTemp.isValid()) qFatal("Activation temp setup failed");
    qputenv("TMPDIR", activationTemp.path().toUtf8());
#endif
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
    const QString manifestOnly = stateDir() + "/codex-skills-backups/" +
        QString::fromLatin1(name.toUtf8().toBase64(QByteArray::Base64UrlEncoding)) + ".files.json";
    put(manifestOnly, "{\"version\":1,\"files\":[\"SKILL.md\"]}");
    check(Injector::scan("codex") == InstallState::NeedsRepair &&
          Injector::install("codex", fixtureRoot + "/prompt.md").ok &&
          Injector::scan("codex") == InstallState::Installed,
          "real install flow repairs an old manifest with existing stamps and returns the tool to installed");
    const QString retired = "retired-upgrade-fixture";
    const QString retiredPath = Skills::installedRoot("codex") + '/' + retired + "/SKILL.md";
    const QString retiredMeta = stateDir() + "/codex-skills-backups/" +
        QString::fromLatin1(retired.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
    const QByteArray retiredBytes = "Previously installed retired skill.\n";
    put(retiredPath, retiredBytes);
    put(retiredMeta + ".orig", "Original user skill before Ua.\n");
    put(retiredMeta + ".sha256", QCryptographicHash::hash(retiredBytes, QCryptographicHash::Sha256).toHex());
    put(stateDir() + "/codex-skills.dat", Injector::readFile(stateDir() + "/codex-skills.dat") + retired.toUtf8() + '\n');
    ItemState retiredState;
    const bool retiredStateLoaded = Injector::loadState("codex", retiredState);
    retiredState.skills << retired;
    check(retiredStateLoaded && Injector::saveState("codex", retiredState) && Injector::scan("codex") == InstallState::NeedsRepair &&
          Injector::install("codex", fixtureRoot + "/prompt.md").ok &&
          !Skills::installedNames("codex").contains(retired) &&
          Injector::readFile(retiredPath) == "Original user skill before Ua.\n" &&
          Injector::scan("codex") == InstallState::Installed,
          "same-root upgrades retire removed owned packages transactionally and restore original user content");
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

    if (app.arguments().contains("--prompts-only")) {
        promptTests(temp.path(), check);
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

    if (app.arguments().contains("--shared-skills-only")) {
        sharedSkillsTests(temp.path(), check);
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

    if (app.arguments().contains("--skill-discovery-only")) {
        skillDiscoveryTests(temp.path(), check);
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

    if (app.arguments().contains("--detection-refresh-only")) {
        detectionRefreshTests(temp.path(), check);
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

    if (app.arguments().contains("--vscode-ui-only")) {
        vscodeUiTests(temp.path(), check);
        out << checks << " checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    }

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

    if (app.arguments().contains("--skill-assets-only")) {
        skillAssetTests(check);
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
    engine.rootContext()->setContextProperty("Updater", &closeFixture);
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
        closeFixture.applying = true;
        check(window && !window->close() && window->isVisible() && closeFixture.quitCalls == 0,
              "window close preserves update preparation");
        closeFixture.applying = false;
        const bool closed = window && window->close();
        check(window && !window->isVisible() && (tray ? !closed && closeFixture.quitCalls == 0 :
              closed && closeFixture.quitCalls == 1), tray ? "close hides with tray" : "close exits without tray");
    }
    check(source.contains("danger: true; onClicked: root.close()"), "custom close shares native handler");
    QFile desktop(QStringLiteral(UA_SOURCE_DIR "/cmake/DeployLinux.cmake"));
    check(desktop.open(QIODevice::ReadOnly | QIODevice::Text) && desktop.readAll().contains("Exec=ua\n"),
          "Linux launcher matches executable name");
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
                  "Window { width: 664; height: 430; visible: true; color: Theme.bg; "
                  "property int themeMode: 1; Binding { target: Theme; property: 'mode'; value: themeMode } "
                  "Grid { anchors { fill: parent; margins: 12 } columns: 3; spacing: 8; "
                  "Repeater { model: logoItems; delegate: Card { width: 208; height: 48; item: modelData } } } }",
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
        QQuickItem* vscodeTitle = nullptr;
        std::function<void(QQuickItem*)> inspectLabel = [&](QQuickItem* object) {
            const QString text = object->property("text").toString();
            for (const Item& item : allItems()) {
                if (item.group == ItemGroup::Editor && text == item.name) {
                    if (object->width() > 0 && object->height() > 0) labels.insert(text);
                    else out << "Label geometry " << text << ": " << object->width() << 'x' << object->height() << '\n';
                    if (item.id == QStringLiteral("vscode")) vscodeTitle = object;
                }
            }
            for (auto* child : object->childItems()) inspectLabel(child);
        };
        inspectLabel(sceneWindow->contentItem());
        check(labels.size() == logoCount, "all product labels have visible geometry");
        check(vscodeTitle && vscodeTitle->isVisible() &&
              vscodeTitle->property("text").toString() == QStringLiteral("Visual Studio Code"),
              mode == 1 ? "dark Visual Studio Code title is present and complete" :
                          "light Visual Studio Code title is present and complete");
        check(vscodeTitle && vscodeTitle->property("truncated").isValid() &&
              !vscodeTitle->property("truncated").toBool(),
              mode == 1 ? "dark Visual Studio Code title is not truncated" :
                          "light Visual Studio Code title is not truncated");
        auto* vscodeCard = vscodeTitle && vscodeTitle->parentItem() ?
            vscodeTitle->parentItem()->parentItem() : nullptr;
        const qreal titleTop = vscodeCard ? vscodeTitle->mapToItem(vscodeCard, QPointF()).y() : -1;
        check(vscodeCard && vscodeTitle->height() > 0 && titleTop >= 0 &&
              titleTop + vscodeTitle->height() <= vscodeCard->height(),
              mode == 1 ? "dark Visual Studio Code title fits within its card vertically" :
                          "light Visual Studio Code title fits within its card vertically");
        const qreal scale = frame.width() / 664.0;
        int contrastTiles = 0;
        for (int index = 0; index < logoCount; ++index) {
            const int left = 22 + (index % 3) * 216;
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
