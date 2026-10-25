// main.cpp - Qt entry point for Ua Toolbox
//
// Boots a QGuiApplication, wires the AppController, loads qml/Main.qml.
// Single-instance check and frameless/round-corner window hints live here;
// Mica / vibrancy / tray / autostart are provided by platform/* modules.

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QFontDatabase>
#include <QDir>
#include <QIcon>
#include <QLoggingCategory>
#include <QWindow>

#include "core/app_controller.h"
#include "core/settings.h"
#include "core/paths.h"

namespace ua::platform {
bool acquireSingleInstance(const QString& key);
void applyWindowEffects(QWindow* w, bool micaEnabled);
} // namespace ua::platform

int main(int argc, char* argv[])
{
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

    // TODO(cross-platform-glyphs): ship a Fluent-icons .ttf and
    // QFontDatabase::addApplicationFont it here so Linux / macOS get the glyphs
    // Windows gets from Segoe MDL2 Assets. For now icons will be blank on those
    // two platforms (SVG logos still render fine).

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
