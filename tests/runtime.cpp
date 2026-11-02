#include <QGuiApplication>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOperatingSystemVersion>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QtQmlIntegration>
#include <QtConcurrent>
#include <atomic>
#include <cstring>
#include <functional>
#include <memory>

#define private public
#include "core/updater.h"
#undef private
#include "core/settings.h"
#include "core/app_strings.h"
#include "core/update_installer.h"

namespace ua::core::update {
static std::atomic<int> fixtureInstallerCalls{0};
static std::atomic<bool> fixtureInstallerSucceeds{false};
static Result fixtureLaunchInstaller(const QString&, const QString&, const QString&,
                                     qint64, const QString&) {
    ++fixtureInstallerCalls;
    if (fixtureInstallerSucceeds) return {true, {}, {}};
    return {false, QStringLiteral("injected installer preparation failure with a long diagnostic path /temporary/update/package ").repeated(20), {}};
}
}

static QString fixtureRoot;
class NoticeUiFixture : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
public:
    QString version() const { return QStringLiteral("1.0.0"); }
    int acceptedCount = 0;
    int exitCount = 0;
public slots:
    void accepted() { ++acceptedCount; }
    void quit() { ++exitCount; }
};

class SkillUiFixture : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE QVariantList skills() const {
        return {QVariantMap{{"dirName", "an-extremely-long-skill-name-for-layout-checks"},
                            {"installedCount", 1}, {"category", 1},
                            {"missingAssets", QStringList{"references/missing.md"}}}};
    }
};

class FixturePaths {
public:
    static constexpr auto TempLocation = QStandardPaths::TempLocation;
    static QString writableLocation(QStandardPaths::StandardLocation) { return fixtureRoot; }
};

class FixtureFile : public QFile {
public:
    using QFile::QFile;
    static inline bool failWrites = false;
    static inline int failedWrites = 0;
protected:
    qint64 writeData(const char* data, qint64 length) override {
        if (failWrites && length > 0) {
            ++failedWrites;
            setErrorString("injected disk-full failure");
            return -1;
        }
        return QFile::writeData(data, length);
    }
};

// Keep production download logic unchanged while injecting local file/network I/O.
#define QStandardPaths FixturePaths
#define QFile FixtureFile
#define launchInstaller fixtureLaunchInstaller
#include "../src/core/updater.cpp"
#undef launchInstaller
#undef QFile
#undef QStandardPaths

class MemoryReply : public QNetworkReply {
public:
    MemoryReply(const QNetworkRequest& request, QByteArray body,
                NetworkError error, int delay, QObject* parent) : QNetworkReply(parent), data(std::move(body)) {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (error != NoError) setError(error, "injected network failure");
        QTimer::singleShot(delay, this, [this] {
            if (isFinished()) return;
            emit downloadProgress(data.size(), data.size());
            if (!data.isEmpty()) emit readyRead();
            if (isFinished()) return;
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {
        if (isFinished()) return;
        setError(OperationCanceledError, "aborted");
        QTimer::singleShot(0, this, [this] {
            if (isFinished()) return;
            setFinished(true);
            emit finished();
        });
    }
    qint64 bytesAvailable() const override { return data.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* target, qint64 maximum) override {
        const qint64 size = qMin(maximum, qint64(data.size()) - offset);
        if (size <= 0) return -1;
        std::memcpy(target, data.constData() + offset, size);
        offset += size;
        return size;
    }
private:
    QByteArray data;
    qint64 offset = 0;
};

class MemoryNetwork : public QNetworkAccessManager {
public:
    struct Response {
        QByteArray body;
        QNetworkReply::NetworkError error = QNetworkReply::NoError;
        int delay = 0;
    };
    QByteArray body;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    int delay = 0;
    int requests = 0;
    QUrl lastUrl;
    QList<QUrl> urls;
    QHash<QUrl, QList<Response>> responses;
    void enqueue(const QUrl& url, QByteArray data, QNetworkReply::NetworkError failure = QNetworkReply::NoError,
                 int wait = 0) {
        responses[url].append({std::move(data), failure, wait});
    }
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override {
        ++requests;
        lastUrl = request.url();
        urls.append(lastUrl);
        Response response{body, error, delay};
        auto queued = responses.find(lastUrl);
        if (queued != responses.end() && !queued->isEmpty()) response = queued->takeFirst();
        return new MemoryReply(request, std::move(response.body), response.error, response.delay, this);
    }
};

static bool settle(const std::function<bool()>& pending) {
    QEventLoop loop;
    QTimer timer;
    timer.setInterval(1);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] { if (!pending()) loop.quit(); });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();
    return !pending();
}

static QQuickItem* findVisual(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) return root;
    for (auto* child : root->childItems()) {
        if (auto* found = findVisual(child, name)) return found;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    using namespace ua::core;
    QGuiApplication app(argc, argv);
#ifdef Q_OS_WIN
    // The offscreen platform does not load the Windows system font database.
    if (app.platformName() == "offscreen") {
        const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
        for (const auto& file : {"segoeui.ttf", "segoeuib.ttf", "msyh.ttc"})
            QFontDatabase::addApplicationFont(fonts + file);
        QGuiApplication::setFont(QFont("Segoe UI"));
    }
#endif
    app.setOrganizationName("UaRuntimeTests");
    app.setApplicationName("UaRuntimeTests");
    app.setApplicationVersion("1.0.0");
    QTemporaryDir temp;
    if (!temp.isValid()) return 2;
    fixtureRoot = temp.path();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, fixtureRoot + "/settings");
    QTextStream out(stdout);
    int checks = 0, failures = 0;
    const auto check = [&](bool ok, const char* label) {
        ++checks;
        if (!ok) ++failures;
        out << (ok ? "PASS " : "FAIL ") << label << Qt::endl;
    };

    check(assetForPlatform(u"winnt", u"x86_64") == "Ua-windows-x64.zip",
          "Windows x64 selects the Windows release package");
    check(assetForPlatform(u"darwin", u"arm64") == "Ua-macos-arm64.dmg",
          "Apple Silicon selects the arm64 disk image");
    check(assetForPlatform(u"darwin", u"x86_64") == "Ua-macos-x64.dmg",
          "Intel and Rosetta builds select the x64 disk image");
    check(assetForPlatform(u"linux", u"x86_64") == "Ua-linux-x64.tar.gz",
          "Linux x64 selects the portable Linux archive");
    check(assetForPlatform(u"darwin", u"i386").isEmpty() &&
          assetForPlatform(u"winnt", u"arm64").isEmpty() &&
          assetForPlatform(u"linux", u"arm64").isEmpty() &&
          assetForPlatform(u"unknown", u"x86_64").isEmpty(),
          "unsupported release targets never silently download a different architecture");

    Updater updater;
    delete updater.m_net;
    auto* network = new MemoryNetwork;
    network->setParent(&updater);
    updater.m_net = network;
    int downloadFailures = 0;
    QObject::connect(&updater, &Updater::downloadFailed, &updater, [&](const QString&) { ++downloadFailures; });
    qmlRegisterSingletonInstance("UaApp", 1, 0, "Updater", &updater);
    qmlRegisterSingletonInstance("UaApp", 1, 0, "Strings", Strings::instance());
    qmlRegisterSingletonInstance("UaApp", 1, 0, "Settings", Settings::instance());
    qmlRegisterSingletonType(QUrl::fromLocalFile(UA_SOURCE_DIR "/qml/theme/Theme.qml"), "UaApp", 1, 0, "Theme");
    for (const auto& name : {"IconGlyph", "TitleButton"})
        qmlRegisterType(QUrl::fromLocalFile(QString(UA_SOURCE_DIR "/qml/components/") + name + ".qml"), "UaApp", 1, 0, name);
    QQmlEngine engine;
    NoticeUiFixture context;
    engine.rootContext()->setContextProperty("app", &context);
    const auto showsRetry = [&] {
        QQmlComponent component(&engine, QUrl::fromLocalFile(UA_SOURCE_DIR "/qml/dialogs/AboutCard.qml"));
        QScopedPointer<QObject> object(component.create());
        if (!object) return false;
        bool retry = false, latest = false;
        for (auto* child : object->findChildren<QObject*>()) {
            const auto text = child->property("text").toString();
            retry |= text == Strings::instance()->get(Strings::UpdateCheckFailed);
            latest |= text == Strings::instance()->get(Strings::UpdateLatest);
        }
        return retry && !latest;
    };

    const QByteArray package = "PK-fixture-release-data";
    const QUrl apiUrl("https://api.github.com/repos/freyrswift/UaStation/releases/latest");
    const QUrl packageUrl("https://github.com/freyrswift/UaStation/releases/download/v2.0.0/" + releaseAssetName());
    const QString packageDigest = "sha256:" + QString::fromLatin1(QCryptographicHash::hash(package, QCryptographicHash::Sha256).toHex());
    const QJsonObject validAsset{{"name", releaseAssetName()}, {"size", package.size()},
                                {"digest", packageDigest}, {"browser_download_url", packageUrl.toString()}};
    const QJsonObject validRelease{{"tag_name", "v2.0.0"}, {"draft", false}, {"prerelease", false},
                                  {"assets", QJsonArray{validAsset}}};
    const auto json = [](const QJsonObject& value) { return QJsonDocument(value).toJson(QJsonDocument::Compact); };
    const auto selectRelease = [&](const QJsonObject& release) {
        network->body = json(release);
        network->error = QNetworkReply::NoError;
        network->delay = 0;
        updater.check();
        return settle([&] { return updater.checking(); });
    };
    const auto noPackageFiles = [&] {
        QDirIterator files(fixtureRoot, {"*.zip", "*.dmg", "*.tar.gz"}, QDir::Files, QDirIterator::Subdirectories);
        return !files.hasNext();
    };

    network->error = QNetworkReply::HostNotFoundError;
    updater.check();
    check(settle([&] { return updater.checking(); }) && updater.checkFailed() &&
          !updater.available() && !updater.error().isEmpty() && showsRetry(),
          "network failure displays retry instead of latest");
    network->error = QNetworkReply::NoError;
    for (const auto& body : {QByteArray("not JSON"), QByteArray("{}"), QByteArray("{\"tag_name\":\"oops\"}"),
                             QByteArray("{\"tag_name\":\"v2.0.0-beta\"}"), QByteArray("{\"tag_name\":\"v99999999999999.0\"}")}) {
        network->body = body;
        updater.check();
        check(settle([&] { return updater.checking(); }) && updater.checkFailed() &&
              updater.latestVersion().isEmpty(), "invalid release metadata is a check failure");
    }
    for (const auto& flag : {QStringLiteral("draft"), QStringLiteral("prerelease")}) {
        auto release = validRelease;
        release[flag] = true;
        check(selectRelease(release) && updater.checkFailed() && !updater.available(),
              "draft and prerelease releases are excluded from automatic updates");
    }
    for (const auto& tag : {QStringLiteral("v1.0.0"), QStringLiteral("v0.9.0")}) {
        check(selectRelease(QJsonObject{{"tag_name", tag}}) && !updater.checkFailed() && !updater.available(),
              "current and older releases require no downloadable package");
    }
    check(selectRelease(QJsonObject{{"tag_name", "v2.0.0"}}) && updater.checkFailed() && !updater.available(),
          "new release without assets is not reported as a usable update");
    const QList<QPair<QString, QJsonValue>> invalidAssets{
        {"name", "Ua-unsupported-architecture.zip"}, {"size", 0}, {"size", -1},
        {"size", 1073741825.0}, {"size", 1.5}, {"digest", "sha256:bad"},
        {"digest", "md5:" + QString(64, '0')},
        {"browser_download_url", "https://example.invalid/update.zip"},
        {"browser_download_url", "http://github.com/freyrswift/UaStation/releases/download/v2.0.0/" + releaseAssetName()},
        {"browser_download_url", "https://github.com/freyrswift/UaStation/releases/latest/download/" + releaseAssetName()},
        {"browser_download_url", "https://github.com/freyrswift/UaStation/releases/download/v3.0.0/" + releaseAssetName()}
    };
    for (const auto& [field, value] : invalidAssets) {
        auto release = validRelease;
        auto asset = validAsset;
        asset[field] = value;
        release["assets"] = QJsonArray{asset};
        check(selectRelease(release) && updater.checkFailed() && !updater.available() && !updater.error().isEmpty(),
              "untrusted asset URL, architecture, size or digest is rejected");
    }
    check(selectRelease(validRelease) && !updater.checkFailed() && updater.error().isEmpty() &&
          updater.available() && updater.latestVersion() == "2.0.0", "successful retry clears the failed check state");

    network->body = package;
    FixtureFile::failWrites = true;
    updater.download();
    check(settle([&] { return updater.downloading(); }) && FixtureFile::failedWrites > 0 &&
          !updater.ready() && updater.downloadProgress() == 0 && downloadFailures == 1,
          "write failure emits an error instead of a ready update");
    check(noPackageFiles(), "failed download removes the partial package");
    FixtureFile::failWrites = false;
    network->error = QNetworkReply::RemoteHostClosedError;
    updater.download();
    check(settle([&] { return updater.downloading(); }) && !updater.ready() && downloadFailures == 2 && noPackageFiles(),
          "interrupted network transfer removes the downloaded package");
    network->error = QNetworkReply::NoError;
    for (const auto& body : {QByteArray{}, QByteArray("short"), package + "extra", QByteArray(package.size(), 'x')}) {
        const int failuresBefore = downloadFailures;
        network->body = body;
        updater.download();
        check(settle([&] { return updater.downloading(); }) && !updater.ready() && downloadFailures == failuresBefore + 1 &&
              !updater.error().isEmpty() && noPackageFiles(),
              "empty, truncated, oversized and SHA256-mismatched packages are rejected and removed");
    }
    network->body = package;
    const int requestsBeforeDownload = network->requests;
    updater.download();
    updater.download();
    updater.check();
    check(settle([&] { return updater.downloading(); }) && updater.ready() && updater.error().isEmpty() &&
          updater.downloadProgress() == 100 && QFileInfo(updater.m_downloadedPath).size() == package.size(),
          "retry succeeds only after package size and SHA256 both match");
    check(network->requests == requestsBeforeDownload + 1 && network->lastUrl == packageUrl,
          "duplicate operations issue one download pinned to the checked release tag");
    const auto validatedPath = updater.m_downloadedPath;
    updater.cancel();
    check(!updater.ready() && !updater.automatic() && !QFileInfo::exists(validatedPath),
          "cancel removes a validated temporary package without launching the installer");

    network->body = json(validRelease);
    network->delay = 100;
    const int requestsBeforeCheck = network->requests;
    const int failuresBeforeCancel = downloadFailures;
    updater.check();
    updater.check();
    check(updater.checking() && network->requests == requestsBeforeCheck + 1,
          "duplicate checks share one outstanding release request");
    updater.cancel();
    check(settle([&] { return updater.checking() || updater.downloading(); }) && !updater.automatic() &&
          downloadFailures == failuresBeforeCancel && !updater.checkFailed(),
          "canceling a check leaves the application usable without a spurious error");
    check(selectRelease(validRelease), "release selection remains usable after cancellation");
    network->body = package;
    network->delay = 100;
    updater.download();
    check(updater.downloading(), "download cancellation fixture starts an in-flight transfer");
    updater.cancel();
    check(settle([&] { return updater.downloading(); }) && !updater.ready() && noPackageFiles() &&
          downloadFailures == failuresBeforeCancel,
          "canceling a download removes partial files and suppresses failure notifications");
    network->delay = 0;

    updater.setOperationBusy(true);
    network->enqueue(apiUrl, json(validRelease));
    network->enqueue(packageUrl, package);
    const int requestsBeforeAutomatic = network->requests;
    updater.startAutomaticUpdate();
    updater.startAutomaticUpdate();
    check(settle([&] { return updater.checking() || updater.downloading() || !updater.ready(); }) &&
          updater.automatic() && updater.ready() && !updater.applying() &&
          update::fixtureInstallerCalls == 0 && network->requests == requestsBeforeAutomatic + 2,
          "startup update checks and downloads automatically but defers replacement during an installation");
    updater.setOperationBusy(false);
#if defined(Q_OS_WIN)
    check(settle([&] { return updater.applying() || update::fixtureInstallerCalls == 0; }) &&
          update::fixtureInstallerCalls == 1 && !updater.error().isEmpty() && !updater.applying(),
          "finishing an installation resumes automatic apply and reports preparation failure without quitting");
    const int callsBeforeRetry = update::fixtureInstallerCalls;
    updater.applyUpdate();
    updater.applyUpdate();
    check(settle([&] { return updater.applying() || update::fixtureInstallerCalls == callsBeforeRetry; }) &&
          update::fixtureInstallerCalls == callsBeforeRetry + 1 && !updater.error().isEmpty(),
          "installer preparation failures leave a retryable validated package");
#else
    check(!updater.supportsAutoInstall() && updater.ready() &&
          update::fixtureInstallerCalls == 0 && !updater.applying(),
          "manual-install platforms retain a verified package without attempting replacement");
    updater.m_error = QStringLiteral("injected preparation failure with a long diagnostic path ").repeated(30);
    emit updater.stateChanged();
#endif

    QStringList updateWarnings;
    const auto updateWarningConnection = QObject::connect(&engine, &QQmlEngine::warnings, &engine,
        [&](const QList<QQmlError>& errors) {
            for (const auto& error : errors) updateWarnings << error.toString();
        });
    QQmlComponent updateComponent(&engine, QUrl::fromLocalFile(UA_SOURCE_DIR "/qml/dialogs/UpdateDialog.qml"));
    QQuickWindow updateWindow;
    updateWindow.show();
    for (int language : {int(Strings::CN), int(Strings::EN)}) {
        Strings::instance()->setLang(language);
        for (const auto& size : {QSize(460, 524), QSize(320, 420)}) {
            QScopedPointer<QObject> updateObject(updateComponent.create());
            auto* updateItem = qobject_cast<QQuickItem*>(updateObject.data());
            updateWindow.resize(size);
            if (updateItem) updateItem->setParentItem(updateWindow.contentItem());
            settle([&] { return updateItem && updateItem->width() != size.width(); });
            auto* panel = findVisual(updateWindow.contentItem(), "updatePanel");
            auto* versionLine = findVisual(updateWindow.contentItem(), "updateVersion");
            auto* scroll = findVisual(updateWindow.contentItem(), "updateStatusScroll");
            auto* errorText = findVisual(updateWindow.contentItem(), "updateError");
            auto* later = findVisual(updateWindow.contentItem(), "updateLater");
            auto* retry = findVisual(updateWindow.contentItem(), "updateRetry");
            if (!updateItem) out << updateComponent.errorString() << Qt::endl;
            check(updateItem && panel && versionLine && scroll && errorText && later && retry,
                  "update failure dialog loads in both languages at desktop and narrow sizes");
            if (!updateItem || !panel || !versionLine || !scroll || !errorText || !later || !retry) continue;
            check(panel->x() >= 0 && panel->y() >= 0 && panel->x() + panel->width() <= size.width() &&
                  panel->y() + panel->height() <= size.height() && scroll->height() > 20 &&
                  versionLine->property("text").toString().contains("1.0.0") &&
                  errorText->isVisible() && scroll->property("contentHeight").toReal() > scroll->height() &&
                  later->isEnabled() && retry->isEnabled() && retry->isVisible() &&
                  later->x() + later->width() <= retry->x() &&
                  retry->x() + retry->width() <= retry->parentItem()->width(),
                  "long update failures remain scrollable with non-overlapping retry and later controls");
            if (language == Strings::CN && size.width() == 460 && app.arguments().contains("--update-screenshot")) {
                settle([] { return false; });
                check(updateWindow.grabWindow().save(UA_SOURCE_DIR "/build/update-failure-cn.png"),
                      "update dialog screenshot saved");
            }
            updateItem->setParentItem(nullptr);
        }
    }
    QObject::disconnect(updateWarningConnection);
    if (!updateWarnings.isEmpty()) out << updateWarnings.join('\n') << Qt::endl;
    check(updateWarnings.isEmpty(), "update dialog produces no QML warnings");
#if defined(Q_OS_WIN)
    QQmlComponent exitGuardComponent(&engine);
    exitGuardComponent.setData("import QtQuick\nimport QtQuick.Window\nimport UaApp 1.0\n"
        "Window { visible: true; onClosing: (close) => { if (Updater.applying) close.accepted = false } }", QUrl());
    QScopedPointer<QObject> exitGuardWindow(exitGuardComponent.create());
    check(bool(exitGuardWindow), "successful-update fixture keeps the preparation close guard active");
    update::fixtureInstallerSucceeds = true;
    const int callsBeforeSuccess = update::fixtureInstallerCalls;
    bool quitTimedOut = false;
    QTimer quitDeadline;
    quitDeadline.setSingleShot(true);
    QObject::connect(&quitDeadline, &QTimer::timeout, &app, [&] {
        quitTimedOut = true;
        QCoreApplication::exit(2);
    });
    quitDeadline.start(2000);
    updater.applyUpdate();
    QCoreApplication::exec();
    quitDeadline.stop();
    check(!quitTimedOut && updater.applying() && updater.error().isEmpty() &&
          update::fixtureInstallerCalls == callsBeforeSuccess + 1,
          "successful installer readiness quits the event loop only after helper preparation");
    // Reset the stub's successful exit state so the remaining UI fixtures can run.
    updater.m_applying = false;
    update::fixtureInstallerSucceeds = false;
#endif
    updater.cancel();
    check(!updater.ready() && !updater.automatic() && !updater.applying() && noPackageFiles(),
          "post-failure cancellation clears the update workflow and temporary package");

    int themeSignals = 0;
    QObject::connect(Settings::instance(), &Settings::systemIsDarkChanged, &app, [&] { ++themeSignals; });
    const bool invoked = QMetaObject::invokeMethod(app.styleHints(), "colorSchemeChanged", Qt::DirectConnection,
                                                  Q_ARG(Qt::ColorScheme, Qt::ColorScheme::Dark));
    check(invoked && themeSignals == 1, "system color scheme notifications reach the QML settings bridge");
    QQmlComponent binding(&engine);
    binding.setData("import QtQml\nimport UaApp 1.0\nQtObject { property bool dark: Settings.systemIsDark }", QUrl());
    QScopedPointer<QObject> bound(binding.create());
    check(bound && bound->property("dark").toBool() == Settings::instance()->systemIsDark(),
          "system theme is exposed as a bindable QML property");

    SkillUiFixture skillContext;
    engine.rootContext()->setContextProperty("app", &skillContext);
    QQmlComponent manager(&engine, QUrl::fromLocalFile(UA_SOURCE_DIR "/qml/dialogs/SkillsManager.qml"));
    QScopedPointer<QObject> managerRoot(manager.create());
    QQuickWindow window;
    window.resize(640, 640);
    auto* managerItem = qobject_cast<QQuickItem*>(managerRoot.data());
    if (managerItem) managerItem->setParentItem(window.contentItem());
    window.show();
    settle([&] { return !findVisual(window.contentItem(), "skillName"); });
    const auto* skillName = findVisual(window.contentItem(), "skillName");
    const auto* skillStatus = findVisual(window.contentItem(), "skillStatus");
    if (!skillName || !skillStatus)
        out << "Layout fixture: manager=" << bool(managerItem) << " name=" << bool(skillName)
            << " status=" << bool(skillStatus) << " errors=" << manager.errorString() << Qt::endl;
    check(managerItem && skillName && skillStatus && skillName->width() > 0 &&
          skillName->x() + skillName->width() <= skillStatus->x(),
          "long skill names leave room for installed count and missing-asset status");
    if (managerItem) managerItem->setParentItem(nullptr);

    NoticeUiFixture noticeContext;
    engine.rootContext()->setContextProperty("app", &noticeContext);
    Settings::instance()->setDisclaimerAccepted(true);
    QFile mainQml(UA_SOURCE_DIR "/qml/Main.qml");
    check(mainQml.open(QIODevice::ReadOnly), "startup source available");
    const auto mainSource = QString::fromUtf8(mainQml.readAll());
    check(QRegularExpression("property bool\\s+disclaimerOpen:\\s*true").match(mainSource).hasMatch() &&
          !mainSource.contains("!Settings.disclaimerAccepted"),
          "startup notice is shown even after acceptance in an older version");
    const QRegularExpression acceptedHandler("onAccepted:.*?(?=\\n\\s*onExitRequested:)",
                                             QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression updateVisibilityHandler(
        "readonly property bool updateDialogOpen:.*?(?=\\n    property string toastText)",
        QRegularExpression::DotMatchesEverythingOption);
    const auto acceptedSource = acceptedHandler.match(mainSource).captured();
    const auto updateVisibilitySource = updateVisibilityHandler.match(mainSource).captured();
    check(!acceptedSource.isEmpty() && !updateVisibilitySource.isEmpty(),
          "production announcement acceptance and update visibility handlers are available");
    QQmlComponent startupComponent(&engine);
    startupComponent.setData("import QtQuick\nimport QtQuick.Window\nimport UaApp 1.0\n"
        "Window { id: root; visible: false; property bool disclaimerOpen: true; signal accepted();\n" +
        updateVisibilitySource.toUtf8() + '\n' + acceptedSource.toUtf8() + "\n}", QUrl());
    QScopedPointer<QObject> startupObject(startupComponent.create());
    auto* startupWindow = qobject_cast<QQuickWindow*>(startupObject.data());
    if (!startupWindow) out << startupComponent.errorString() << Qt::endl;
    updater.setOperationBusy(true);
    network->enqueue(apiUrl, json(validRelease));
    network->enqueue(packageUrl, package);
    const int requestsBeforeAcceptance = network->requests;
    check(startupWindow && startupObject->property("disclaimerOpen").toBool() &&
          !updater.automatic() && network->requests == requestsBeforeAcceptance,
          "startup does not check or download an update before announcement acceptance");
    const bool acceptedInvoked = startupObject && QMetaObject::invokeMethod(startupObject.data(), "accepted");
    check(acceptedInvoked && !startupObject->property("disclaimerOpen").toBool() &&
          network->requests == requestsBeforeAcceptance,
          "announcement acceptance enters the main interface before scheduling its update check");
    check(settle([&] { return updater.checking() || updater.downloading() || !updater.ready(); }) &&
          updater.ready() && updater.automatic() && network->requests == requestsBeforeAcceptance + 2 &&
          startupWindow && startupWindow->isVisible() && startupObject->property("updateDialogOpen").toBool(),
          "production startup handler automatically checks, downloads and restores a hidden window for the update");
    if (startupWindow) {
        startupObject->setProperty("disclaimerOpen", true);
        startupWindow->hide();
        startupObject->setProperty("disclaimerOpen", false);
        check(startupWindow->isVisible() && startupWindow->visibility() == QWindow::Windowed,
              "an update notification restores the main window from a hidden tray state");
        startupObject->setProperty("disclaimerOpen", true);
        startupWindow->showMinimized();
        startupObject->setProperty("disclaimerOpen", false);
        check(startupWindow->isVisible() && startupWindow->visibility() == QWindow::Windowed,
              "an update notification restores a minimized main window");
        startupWindow->hide();
    }
    updater.cancel();
    updater.setOperationBusy(false);
    const QRegularExpression closeHandler("onClosing:.*?(?=\\n    // Tray asked)",
                                          QRegularExpression::DotMatchesEverythingOption);
    QQmlComponent closeComponent(&engine);
    closeComponent.setData("import QtQuick\nimport QtQuick.Window\nimport UaApp 1.0\nWindow { id: root; visible: true; "
        "property bool installInProgress: false; property bool disclaimerOpen: true; " +
        closeHandler.match(mainSource).captured().toUtf8() + "\n}", QUrl());
    QScopedPointer<QObject> closeObject(closeComponent.create());
    auto* closeWindow = qobject_cast<QQuickWindow*>(closeObject.data());
    check(closeWindow && closeWindow->close() && noticeContext.exitCount == 1,
          "closing before acknowledgment exits instead of hiding in the tray");
    noticeContext.exitCount = 0;

    QStringList noticeWarnings;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError>& errors) {
        for (const auto& error : errors) noticeWarnings << error.toString();
    });
    QQmlComponent noticeComponent(&engine, QUrl::fromLocalFile(UA_SOURCE_DIR "/qml/dialogs/Disclaimer.qml"));
    for (int language : {int(Strings::CN), int(Strings::EN)}) {
        Strings::instance()->setLang(language);
        for (const auto& size : {QSize(460, 524), QSize(320, 420)}) {
            QScopedPointer<QObject> noticeObject(noticeComponent.create());
            auto* notice = qobject_cast<QQuickItem*>(noticeObject.data());
            window.resize(size);
            if (notice) notice->setParentItem(window.contentItem());
            settle([&] { return notice && notice->width() != size.width(); });
            auto* panel = findVisual(window.contentItem(), "noticePanel");
            auto* scroll = findVisual(window.contentItem(), "noticeScroll");
            auto* body = findVisual(window.contentItem(), "noticeBody");
            auto* read = findVisual(window.contentItem(), "noticeReadConfirmation");
            auto* proceed = findVisual(window.contentItem(), "noticeContinue");
            auto* exit = findVisual(window.contentItem(), "noticeExit");
            if (!notice) out << noticeComponent.errorString() << Qt::endl;
            check(notice && panel && scroll && body && read && proceed && exit,
                  "usage notice and all controls load in both languages");
            if (!notice || !panel || !scroll || !body || !read || !proceed || !exit) continue;
            check(panel->x() >= 0 && panel->y() >= 0 && panel->x() + panel->width() <= size.width() &&
                  panel->y() + panel->height() <= size.height() && scroll->height() > 100 &&
                  scroll->y() + scroll->height() <= read->y() && read->height() > 0 &&
                  read->y() + read->height() < proceed->parentItem()->y(),
                  "notice text scrolls without overlapping acknowledgment or actions");
            check(scroll->property("contentHeight").toReal() > scroll->height() &&
                  body->property("text").toString().size() > 500,
                  "full notice stays available in the scrollable body");
            QObject::connect(notice, SIGNAL(accepted()), &noticeContext, SLOT(accepted()));
            QObject::connect(notice, SIGNAL(exitRequested()), &noticeContext, SLOT(quit()));
            const int acceptedBefore = noticeContext.acceptedCount;
            QMetaObject::invokeMethod(notice, "confirm");
            check(!proceed->isEnabled() && noticeContext.acceptedCount == acceptedBefore,
                  "unread notice cannot be confirmed");
            read->setProperty("checked", true);
            QMetaObject::invokeMethod(notice, "confirm");
            check(proceed->isEnabled() && noticeContext.acceptedCount == acceptedBefore + 1,
                  "explicit acknowledgment enables continue");
            const int exitBefore = noticeContext.exitCount;
            QMetaObject::invokeMethod(exit, "clicked");
            check(noticeContext.exitCount == exitBefore + 1, "exit remains available without continuing");
            read->forceActiveFocus();
            window.requestActivate();
            settle([&] { return window.activeFocusItem() != read; });
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &escape);
            if (noticeContext.exitCount != exitBefore + 2)
                out << "Escape fixture: focus=" << (window.activeFocusItem() == read)
                    << " active=" << window.isActive() << " accepted=" << escape.isAccepted() << Qt::endl;
            check(noticeContext.exitCount == exitBefore + 2, "Escape requests exit from the startup notice");
            if (language == Strings::CN && size.width() == 460 && app.arguments().contains("--notice-screenshot")) {
                read->setProperty("checked", false);
                settle([] { return false; });
                check(window.grabWindow().save(UA_SOURCE_DIR "/build/startup-notice-cn.png"),
                      "startup notice screenshot saved");
            }
            notice->setParentItem(nullptr);
        }
    }
    if (!noticeWarnings.isEmpty()) out << noticeWarnings.join('\n') << Qt::endl;
    check(noticeWarnings.isEmpty(), "usage notice produces no QML warnings");
    out << checks << " checks, " << failures << " failures; external requests=0" << Qt::endl;
    return failures == 0 ? 0 : 1;
}

#include "runtime.moc"
