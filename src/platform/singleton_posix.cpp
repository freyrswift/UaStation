// singleton_posix.cpp - Single-instance guard for Linux / macOS
//
// Uses a QLockFile under $XDG_RUNTIME_DIR / /tmp. QLockFile handles PID
// liveness checks, so a crashed instance leaves no stale lock behind.

#include "autostart.h"

#include <QDir>
#include <QLockFile>
#include <QStandardPaths>

namespace ua::platform {

static QLockFile* g_lock = nullptr;

bool acquireSingleInstance(const QString& key)
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString base       = runtimeDir.isEmpty() ? QDir::tempPath() : runtimeDir;
    const QString path       = QDir::cleanPath(base + QLatin1Char('/') + key + QStringLiteral(".lock"));

    g_lock = new QLockFile(path);
    g_lock->setStaleLockTime(0); // Treat stale locks (dead PIDs) as removable.
    return g_lock->tryLock(0);
}

} // namespace ua::platform
