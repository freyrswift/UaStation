// main.cpp - Qt entry point for Ua Toolbox
//
// Boots the QApplication, wires AppController to QML, loads Main.qml.
// Single-instance guard + Mica/vibrancy backdrop are delegated to the
// platform/ shims (see platform/singleton_*.cpp and window_effects_*.cpp).

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>
#include <QWindow>

namespace {
// Opt-in message handler: when the UA_LOG environment variable is set, write
// qInfo/qWarning/qDebug lines to <exeDir>/ua.log. Off by default so field
// installs don't scribble log files next to Ua.exe.
void logToFile(QtMsgType type, const QMessageLogContext&, const QString& msg) {
    static QMutex lock;
    QMutexLocker guard(&lock);
    static QFile f(QCoreApplication::applicationDirPath() + QStringLiteral("/ua.log"));
    static bool opened = f.open(QIODevice::Append | QIODevice::Text);
    if (!opened) return;
    QTextStream s(&f);
    const char* lvl = type == QtDebugMsg    ? "DBG"
                    : type == QtInfoMsg     ? "INF"
                    : type == QtWarningMsg  ? "WRN"
                    : type == QtCriticalMsg ? "CRT"
                                            : "FTL";
    s << '[' << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "] "
      << lvl << ' ' << msg << '\n';
    f.flush();
}
} // namespace

#include "core/app_controller.h"
#include "core/settings.h"
#include "core/paths.h"

namespace ua::platform {
bool acquireSingleInstance(const QString& key);
void applyWindowEffects(QWindow* w, bool micaEnabled);
} // namespace ua::platform

int main(int argc, char* argv[])
{
    if (!qEnvironmentVariableIsEmpty("UA_LOG")) {
        qInstallMessageHandler(logToFile);
    }

    QApplication::setApplicationName(QStringLiteral("Ua"));
    QApplication::setApplicationDisplayName(QStringLiteral("Ua Toolbox"));
    QApplication::setOrganizationName(QStringLiteral("Ua"));
    QApplication::setOrganizationDomain(QStringLiteral("utlra.cc"));
    QApplication::setApplicationVersion(QStringLiteral(UA_VERSION_STR));

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/Ua.ico")));

    // Prefer the Fusion-based Basic style so our fully custom QML doesn't inherit
    // platform widget chrome; everything is painted by us.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Single-instance guard: if another Ua is running, we exit silently.
    // (Platform layer handles the "activate existing window" IPC later.)
    if (!ua::platform::acquireSingleInstance(QStringLiteral("UaToolbox.SingleInstance"))) {
        return 0;
    }

    // Load the bundled Fluent UI System Icons font so every platform draws
    // the same glyphs (Windows still has Segoe MDL2 Assets but we standardise
    // on the bundled font to keep the codepoint table single-sourced).
    QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/FluentSystemIcons-Resizable.ttf"));

    // Create persistent dirs ($APPDATA/Ua or XDG equivalent) up front so writes don't race.
    ua::core::ensureUserDirs();

    // AppController is the single QObject bridge QML talks to. It owns the
    // Items/Skills/Prompts models and dispatches install/uninstall/update calls.
    ua::core::AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);

    engine.loadFromModule(QStringLiteral("UaApp"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // Apply Mica (Win11) / vibrancy (macOS) / plain color (Linux) and give the
    // controller a handle so the tray can bring the window back.
    if (auto* window = qobject_cast<QWindow*>(engine.rootObjects().first())) {
        ua::platform::applyWindowEffects(window, /*micaEnabled=*/true);
        controller.attachMainWindow(window);
    }

    return app.exec();
}
