#pragma once

#include <QString>

namespace ua::core::update {

struct Result {
    bool ok = false;
    QString error;
    QString logPath;
};

// The caller must quit only after this returns success. The helper has then
// validated and staged the package and is waiting for the supplied process.
Result launchInstaller(const QString& archivePath,
                       const QString& appDirectory,
                       const QString& executablePath,
                       qint64 processId,
                       const QString& expectedSha256);

} // namespace ua::core::update
