// autostart_mac.cpp - macOS LaunchAgent-based autostart
//
// Writes ~/Library/LaunchAgents/cc.utlra.ua.plist with RunAtLoad=true. Users
// can disable it directly from System Settings > Login Items without us.

#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

namespace ua::platform {

namespace {

constexpr const char* kAgentId = "cc.utlra.ua";

QString agentPath()
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return QDir::cleanPath(home + QStringLiteral("/Library/LaunchAgents/") + QLatin1String(kAgentId) + QStringLiteral(".plist"));
}

} // namespace

bool autostartEnabled()
{
    return QFileInfo::exists(agentPath());
}

bool setAutostartEnabled(bool on)
{
    const QString path = agentPath();
    const QString id   = QString::fromLatin1(kAgentId);

    if (!on) {
        QProcess::execute(QStringLiteral("launchctl"), {QStringLiteral("unload"), path});
        return QFile::remove(path);
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream s(&f);
    s << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
      << "<plist version=\"1.0\">\n<dict>\n"
      << "  <key>Label</key><string>" << id << "</string>\n"
      << "  <key>ProgramArguments</key>\n  <array>\n"
      << "    <string>" << QCoreApplication::applicationFilePath() << "</string>\n"
      << "  </array>\n"
      << "  <key>RunAtLoad</key><true/>\n"
      << "</dict>\n</plist>\n";
    f.close();

    QProcess::execute(QStringLiteral("launchctl"), {QStringLiteral("load"), path});
    return true;
}

} // namespace ua::platform
