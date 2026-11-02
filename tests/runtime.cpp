#include <QGuiApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
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
#include <cstring>
#include <functional>

#define private public
#include "core/updater.h"
#undef private
#include "core/settings.h"
#include "core/strings.h"

static QString fixtureRoot;
class NoticeUiFixture : public QObject {
    Q_OBJECT
public:
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
#include "../src/core/updater.cpp"
#undef QFile
#undef QStandardPaths

class MemoryReply : public QNetworkReply {
public:
    MemoryReply(const QNetworkRequest& request, QByteArray body,
                NetworkError error, QObject* parent) : QNetworkReply(parent), data(std::move(body)) {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (error != NoError) setError(error, "injected network failure");
        QTimer::singleShot(0, this, [this] {
            if (!data.isEmpty()) emit readyRead();
            setFinished(true);
            emit finished();
        });
    }
    void abort() override { setError(OperationCanceledError, "aborted"); }
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
    QByteArray body;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    int requests = 0;
    QUrl lastUrl;
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override {
        ++requests;
        lastUrl = request.url();
        return new MemoryReply(request, body, error, this);
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
    QObject context;
    context.setProperty("version", "1.0.0");
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

    network->error = QNetworkReply::HostNotFoundError;
    updater.check();
    check(settle([&] { return updater.checking(); }) && updater.checkFailed() &&
          !updater.available() && showsRetry(), "network failure displays retry instead of latest");
    network->error = QNetworkReply::NoError;
    for (const auto& body : {QByteArray("not JSON"), QByteArray("{}"), QByteArray("{\"tag_name\":\"oops\"}")}) {
        network->body = body;
        updater.check();
        check(settle([&] { return updater.checking(); }) && updater.checkFailed() &&
              updater.latestVersion().isEmpty(), "invalid release metadata is a check failure");
    }
    network->body = "{\"tag_name\":\"v2.0.0\"}";
    updater.check();
    check(settle([&] { return updater.checking(); }) && !updater.checkFailed() &&
          updater.available() && updater.latestVersion() == "2.0.0", "successful retry clears the failed check state");

    network->body = QByteArray(128 * 1024, 'x');
    FixtureFile::failWrites = true;
    updater.download();
    check(settle([&] { return updater.downloading(); }) && FixtureFile::failedWrites > 0 &&
          !updater.ready() && updater.downloadProgress() == 0 && downloadFailures == 1,
          "write failure emits an error instead of a ready update");
    check(QDir(fixtureRoot).entryList(QDir::Files).isEmpty(), "failed download removes the partial package");
    FixtureFile::failWrites = false;
    network->body.clear();
    updater.download();
    check(settle([&] { return updater.downloading(); }) && !updater.ready() && downloadFailures == 2,
          "empty package is not considered ready");
    network->body = "PK-fixture-release-data";
    updater.download();
    check(settle([&] { return updater.downloading(); }) && updater.ready() &&
          updater.downloadProgress() == 100 && QFileInfo(updater.m_downloadedPath).size() == network->body.size(),
          "retry succeeds after a disk write failure");
    check(network->lastUrl.path().endsWith('/' + releaseAssetName()),
          "download URL uses the release asset for the running binary");

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
    const QRegularExpression closeHandler("onClosing:.*?(?=\\n    // Tray asked)",
                                          QRegularExpression::DotMatchesEverythingOption);
    QQmlComponent closeComponent(&engine);
    closeComponent.setData("import QtQuick\nimport QtQuick.Window\nWindow { id: root; visible: true; "
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
