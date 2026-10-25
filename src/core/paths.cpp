// paths.cpp - Cross-platform path resolution

#include "paths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ua::core {

static QString joinHome(const QString& relative)
{
    QString rel = relative;
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(homeDir() + QLatin1Char('/') + rel);
}

QString homeDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString configDir()
{
    // Use AppDataLocation; Qt gives us the <app>-scoped subpath because we set
    // QGuiApplication::applicationName("Ua") before this is called.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir::cleanPath(dir);
}

QString promptsDir()
{
    return QDir::cleanPath(configDir() + QStringLiteral("/prompts"));
}

QString stateDir()
{
    return QDir::cleanPath(configDir() + QStringLiteral("/state"));
}

QString backupsDir()
{
    return QDir::cleanPath(configDir() + QStringLiteral("/backups"));
}

void ensureUserDirs()
{
    QDir().mkpath(configDir());
    QDir().mkpath(promptsDir());
    QDir().mkpath(stateDir());
    QDir().mkpath(backupsDir());
}

QString targetPath(QString relative)
{
    if (relative.isEmpty()) return {};
    return joinHome(relative);
}

QString skillsDir(QString relative)
{
    if (relative.isEmpty()) return {};
    return joinHome(relative);
}

bool toolDetected(QString relative)
{
    if (relative.isEmpty()) return false;
    const QString full = joinHome(relative);
    const QFileInfo info(full);
    // Mirror the old behaviour: check the parent dir first (e.g. ~/.cursor/rules/
    // for a target of ~/.cursor/rules/Ua.mdc), fall back to the tool root dir.
    const QDir parent = info.dir();
    if (parent.exists()) return true;

    // Fallback: strip to the first segment of the relative path and check that.
    QString rel = relative;
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int slash = rel.indexOf(QLatin1Char('/'));
    const QString firstSeg = slash < 0 ? rel : rel.left(slash);
    const QFileInfo root(homeDir() + QLatin1Char('/') + firstSeg);
    return root.exists() && root.isDir();
}

} // namespace ua::core
