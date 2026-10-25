// autostart_linux.cpp - XDG autostart via ~/.config/autostart/<app>.desktop

#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

namespace ua::platform {

namespace {

QString desktopFilePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir::cleanPath(base + QStringLiteral("/autostart/UaToolbox.desktop"));
}

} // namespace

bool autostartEnabled()
{
    return QFileInfo::exists(desktopFilePath());
}

bool setAutostartEnabled(bool on)
{
    const QString path = desktopFilePath();
    if (!on) {
        QFile::remove(path);
        return true;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream s(&f);
    s << "[Desktop Entry]\n"
      << "Type=Application\n"
      << "Name=Ua Toolbox\n"
      << "Exec=\"" << QCoreApplication::applicationFilePath() << "\"\n"
      << "X-GNOME-Autostart-enabled=true\n"
      << "NoDisplay=true\n";
    return true;
}

} // namespace ua::platform
