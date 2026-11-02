#include "update_installer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#if defined(Q_OS_WIN)
#define NOMINMAX
#include <windows.h>
#endif

namespace ua::core::update {
namespace {

bool saveFile(const QString& path, const QByteArray& bytes, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
        !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

QString fileHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return {};
    return QString::fromLatin1(hash.result().toHex());
}

#if defined(Q_OS_WIN)
QByteArray windowsScript()
{
    // Paths are data in a JSON manifest, never interpolated into PowerShell.
    return QByteArray(R"UA_INSTALLER(param([Parameter(Mandatory=$true)][string]$ManifestPath)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$m = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$job = [IO.Path]::GetFullPath($m.Job)
$log = Join-Path $job 'installer.log'
$ready = Join-Path $job 'ready'
$failed = Join-Path $job 'failed'
$staging = Join-Path $job 'staging'
$backup = Join-Path $job 'rollback'
$touched = New-Object 'System.Collections.Generic.List[object]'
$changed = $false
$child = $null

function Log([string]$value) {
    Add-Content -LiteralPath $log -Value ((Get-Date -Format o) + ' ' + $value) -Encoding UTF8
}
function Within([string]$root, [string]$path) {
    $root = [IO.Path]::GetFullPath($root).TrimEnd('\') + '\'
    return [IO.Path]::GetFullPath($path).StartsWith($root, [StringComparison]::OrdinalIgnoreCase)
}
function Check-Parents([string]$root, [string]$path) {
    $item = [IO.Path]::GetFullPath($path)
    while ($item -and (Within $root $item)) {
        if (Test-Path -LiteralPath $item) {
            $info = Get-Item -LiteralPath $item -Force
            if (($info.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw ('Installation contains a symbolic link or junction: ' + $item)
            }
        }
        $item = [IO.Path]::GetDirectoryName($item)
    }
}
function Start-App {
    return Start-Process -FilePath $m.Executable -WorkingDirectory $m.AppDirectory -PassThru
}
function Show-Failure([string]$message) {
    try {
        Add-Type -AssemblyName PresentationFramework
        [System.Windows.MessageBox]::Show($message + "`n`nLog: " + $log, 'Ua update failed', 'OK', 'Error') | Out-Null
    } catch {
        Log ('Error dialog failed: ' + $_.Exception.Message)
    }
}

try {
    Log 'Preparing update.'
    [IO.File]::WriteAllText((Join-Path $job 'helper.pid'), $PID.ToString())
    if ([IO.Path]::GetFullPath($PSScriptRoot) -ne $job -or
        (Get-Content -LiteralPath (Join-Path $job 'owner') -Raw).Trim() -ne $m.Token) {
        throw 'The update helper directory is invalid.'
    }
    if (!(Within $m.AppDirectory $m.Executable)) { throw 'Executable is outside the installation directory.' }
    if (!(Test-Path -LiteralPath $m.Executable -PathType Leaf)) { throw 'The original executable is missing.' }
    $appInfo = Get-Item -LiteralPath $m.AppDirectory -Force
    if (($appInfo.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Installation root is a symbolic link or junction.' }
    $old = Get-Process -Id $m.ProcessId -ErrorAction Stop
    if ($old.StartTime.ToUniversalTime().ToFileTimeUtc().ToString() -ne $m.ProcessStart) {
        throw 'The application process has changed.'
    }
    if ((Get-FileHash -LiteralPath $m.Archive -Algorithm SHA256).Hash -ne $m.Sha256) { throw 'Update package checksum mismatch.' }
    New-Item -ItemType Directory -Path $staging | Out-Null
    New-Item -ItemType Directory -Path $backup | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($m.Archive)
    try {
        [long]$expanded = 0
        $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('/', '\')
            if ([string]::IsNullOrEmpty($name) -or [IO.Path]::IsPathRooted($name) -or
                $name.Contains(':') -or $name.Contains([char]0) -or
                (($entry.ExternalAttributes -shr 16) -band 61440) -eq 40960) {
                throw ('Invalid archive entry: ' + $entry.FullName)
            }
            foreach ($part in $name.TrimEnd('\').Split('\')) {
                if ($part -eq '..' -or $part -eq '.' -or !$part -or $part -match '[ .]$' -or
                    $part -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') {
                    throw ('Invalid archive path: ' + $entry.FullName)
                }
            }
            $target = [IO.Path]::GetFullPath((Join-Path $staging $name))
            if (!(Within $staging $target) -or !$seen.Add($target)) { throw 'Duplicate or escaping archive entry.' }
            $expanded += $entry.Length
            if ($expanded -gt 2147483648 -or $zip.Entries.Count -gt 30000) { throw 'Update package exceeds extraction limits.' }
        }
        foreach ($entry in $zip.Entries) {
            $target = [IO.Path]::GetFullPath((Join-Path $staging $entry.FullName.Replace('/', '\')))
            if ($entry.FullName.EndsWith('/') -or $entry.FullName.EndsWith('\')) {
                [IO.Directory]::CreateDirectory($target) | Out-Null
            } else {
                [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $false)
            }
        }
    } finally { $zip.Dispose() }
    $payloadExe = Join-Path $staging 'ua.exe'
    if (!(Test-Path -LiteralPath $payloadExe -PathType Leaf) -or
        !(Test-Path -LiteralPath (Join-Path $staging 'Qt6Core.dll') -PathType Leaf) -or
        !(Test-Path -LiteralPath (Join-Path $staging 'platforms\qwindows.dll') -PathType Leaf)) {
        throw 'Update package is missing the application or Qt runtime.'
    }
    $newVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($payloadExe)
    $oldVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($m.Executable)
    if ($newVersion.OriginalFilename -and $oldVersion.OriginalFilename -and
        $newVersion.OriginalFilename -ne $oldVersion.OriginalFilename) { throw 'Update executable identity does not match.' }
    $files = @(Get-ChildItem -LiteralPath $staging -Recurse -Force -File)
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($staging.Length + 1)
        if ($relative -ieq 'ua.exe') { $relative = $m.RelativeExecutable }
        $destination = Join-Path $m.AppDirectory $relative
        Check-Parents $m.AppDirectory $destination
        if (Test-Path -LiteralPath $destination -PathType Container) { throw ('A directory blocks an update file: ' + $relative) }
        if (Test-Path -LiteralPath $destination -PathType Leaf) {
            $attributes = [IO.File]::GetAttributes($destination)
            if (($attributes -band [IO.FileAttributes]::ReadOnly) -ne 0) { throw ('Update file is read-only: ' + $relative) }
        }
        $parent = [IO.Path]::GetDirectoryName($destination)
        while (!(Test-Path -LiteralPath $parent -PathType Container)) { $parent = [IO.Path]::GetDirectoryName($parent) }
        $probe = Join-Path $parent ('.ua-update-' + $m.Token + '.tmp')
        $probeStream = New-Object IO.FileStream($probe, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $probeStream.Dispose()
        Remove-Item -LiteralPath $probe
    }
    if (Test-Path -LiteralPath (Join-Path $job 'cancel')) { throw 'Update preparation was cancelled.' }
    Log 'Package extracted and validated. Waiting for the application to exit.'
    [IO.File]::WriteAllText($ready, 'ready')
    if (!$old.WaitForExit(120000)) { throw 'The application did not exit within two minutes.' }
    $old.Dispose()
    if (Test-Path -LiteralPath (Join-Path $job 'cancel')) { throw 'Update cancelled before installation.' }
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($staging.Length + 1)
        if ($relative -ieq 'ua.exe') { $relative = $m.RelativeExecutable }
        $destination = Join-Path $m.AppDirectory $relative
        Check-Parents $m.AppDirectory $destination
        $saved = Join-Path $backup $relative
        $existed = Test-Path -LiteralPath $destination -PathType Leaf
        if ($existed) {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($saved)) | Out-Null
            Copy-Item -LiteralPath $destination -Destination $saved -Force
        }
        $touched.Add([PSCustomObject]@{ Destination = $destination; Backup = $saved; Existed = $existed })
        $changed = $true
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
    }
    Log 'Files installed. Starting the updated application.'
    $child = Start-App
    if ($child.WaitForExit(2500)) { throw ('Updated application exited immediately with code ' + $child.ExitCode) }
    $child.Dispose()
    $child = $null
    [IO.File]::WriteAllText((Join-Path $job 'complete'), 'complete')
    Log 'Update completed successfully.'
    try {
        foreach ($owned in @($staging, $backup, $m.Archive)) {
            if (!(Within $job $owned)) { throw 'Refusing to clean an unowned update path.' }
            if (Test-Path -LiteralPath $owned) { Remove-Item -LiteralPath $owned -Recurse -Force }
        }
    } catch { Log ('Temporary cleanup failed: ' + $_.Exception.Message) }
} catch {
    $message = $_.Exception.Message
    Log ('Update failed: ' + $message)
    if ($changed) {
        if ($child -and !$child.HasExited) { $child.Kill(); $child.WaitForExit(10000) | Out-Null }
        $rollbackErrors = New-Object 'System.Collections.Generic.List[string]'
        for ($i = $touched.Count - 1; $i -ge 0; --$i) {
            $entry = $touched[$i]
            try {
                if ($entry.Existed) { Copy-Item -LiteralPath $entry.Backup -Destination $entry.Destination -Force }
                elseif (Test-Path -LiteralPath $entry.Destination -PathType Leaf) { Remove-Item -LiteralPath $entry.Destination -Force }
            } catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        if ($rollbackErrors.Count) { $message += "`nRollback errors: " + ($rollbackErrors -join '; '); Log $message }
        else { Log 'Original files restored.' }
    }
    [IO.File]::WriteAllText($failed, $message, (New-Object Text.UTF8Encoding($false)))
    if (Test-Path -LiteralPath $ready) {
        try {
            $running = Get-Process -Id $m.ProcessId -ErrorAction SilentlyContinue
            if (!$running) { $restored = Start-App; $restored.Dispose() }
        } catch { Log ('Restart failed: ' + $_.Exception.Message) }
        Show-Failure $message
    }
    exit 1
}
)UA_INSTALLER");
}

QString processStart(qint64 processId)
{
    if (processId <= 0 || static_cast<quint64>(processId) > MAXDWORD) return {};
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 static_cast<DWORD>(processId));
    if (!process) return {};
    FILETIME created{}, exited{}, kernel{}, user{};
    const BOOL ok = GetProcessTimes(process, &created, &exited, &kernel, &user);
    CloseHandle(process);
    if (!ok) return {};
    const quint64 ticks = (static_cast<quint64>(created.dwHighDateTime) << 32) |
                          created.dwLowDateTime;
    return QString::number(ticks);
}
#endif

} // namespace

Result launchInstaller(const QString& archivePath, const QString& appDirectory,
                       const QString& executablePath, qint64 processId,
                       const QString& expectedSha256)
{
#if !defined(Q_OS_WIN)
    Q_UNUSED(archivePath)
    Q_UNUSED(appDirectory)
    Q_UNUSED(executablePath)
    Q_UNUSED(processId)
    Q_UNUSED(expectedSha256)
    return {false, QStringLiteral("Automatic installation is currently supported on Windows only.")};
#else
    static const QRegularExpression hashPattern(QStringLiteral("^[0-9a-fA-F]{64}$"));
    if (!hashPattern.match(expectedSha256).hasMatch())
        return {false, QStringLiteral("A valid SHA256 checksum is required to install an update.")};
    const QFileInfo archive(archivePath);
    const QFileInfo app(appDirectory);
    const QFileInfo executable(executablePath);
    if (!archive.isFile() || !archive.fileName().endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
        return {false, QStringLiteral("The Windows update package must be an existing ZIP file.")};
    if (!app.isDir() || !executable.isFile())
        return {false, QStringLiteral("The installation directory or executable is missing.")};
    const QString root = app.canonicalFilePath();
    const QString exe = executable.canonicalFilePath();
    const QString relative = QDir(root).relativeFilePath(exe);
    if (relative.startsWith(QStringLiteral("../")) || QDir::isAbsolutePath(relative) ||
        relative.contains(QLatin1Char('/')) || !relative.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
        return {false, QStringLiteral("The update must target an executable in the installation root.")};
    const QString started = processStart(processId);
    if (started.isEmpty()) return {false, QStringLiteral("The application process could not be verified.")};
    if (fileHash(archive.absoluteFilePath()).compare(expectedSha256, Qt::CaseInsensitive) != 0)
        return {false, QStringLiteral("Update package checksum mismatch.")};
    const QString windows = qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"));
    const QString powershell = QDir(windows).filePath(QStringLiteral("System32/WindowsPowerShell/v1.0/powershell.exe"));
    if (!QFileInfo::exists(powershell))
        return {false, QStringLiteral("Windows PowerShell is required to install the update.")};

    QTemporaryDir job(QDir::tempPath() + QStringLiteral("/ua-install-XXXXXX"));
    if (!job.isValid()) return {false, QStringLiteral("The temporary update directory could not be created.")};
    const QString ownedArchive = job.filePath(QStringLiteral("package.zip"));
    if (!QFile::copy(archive.absoluteFilePath(), ownedArchive))
        return {false, QStringLiteral("The verified update package could not be staged.")};
    const QString token = job.path().section(QLatin1Char('/'), -1);
    const QString log = job.filePath(QStringLiteral("installer.log"));
    QJsonObject manifest{
        {QStringLiteral("Job"), QDir::toNativeSeparators(job.path())},
        {QStringLiteral("Token"), token},
        {QStringLiteral("Archive"), QDir::toNativeSeparators(ownedArchive)},
        {QStringLiteral("AppDirectory"), QDir::toNativeSeparators(root)},
        {QStringLiteral("Executable"), QDir::toNativeSeparators(exe)},
        {QStringLiteral("RelativeExecutable"), relative},
        {QStringLiteral("ProcessId"), processId},
        {QStringLiteral("ProcessStart"), started},
        {QStringLiteral("Sha256"), expectedSha256.toLower()},
    };
    QString error;
    const QString script = job.filePath(QStringLiteral("install.ps1"));
    const QString manifestPath = job.filePath(QStringLiteral("manifest.json"));
    if (!saveFile(job.filePath(QStringLiteral("owner")), token.toUtf8(), &error) ||
        !saveFile(script, windowsScript(), &error) ||
        !saveFile(manifestPath, QJsonDocument(manifest).toJson(), &error))
        return {false, error};

    QProcess helper;
    helper.setProgram(powershell);
    helper.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                         QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                         QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
                         QStringLiteral("-File"), QDir::toNativeSeparators(script),
                         QStringLiteral("-ManifestPath"), QDir::toNativeSeparators(manifestPath)});
    helper.setWorkingDirectory(job.path());
    helper.setStandardOutputFile(job.filePath(QStringLiteral("helper-output.log")));
    helper.setStandardErrorFile(job.filePath(QStringLiteral("helper-error.log")));
    helper.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
        args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        args->startupInfo->wShowWindow = SW_HIDE;
    });
    qint64 helperPid = 0;
    job.setAutoRemove(false);
    if (!helper.startDetached(&helperPid))
        return {false, QStringLiteral("The update helper could not be started: ") + helper.errorString(), log};

    QElapsedTimer deadline;
    deadline.start();
    const QString ready = job.filePath(QStringLiteral("ready"));
    const QString failed = job.filePath(QStringLiteral("failed"));
    while (deadline.elapsed() < 120000) {
        if (QFileInfo::exists(failed)) {
            QFile result(failed);
            if (result.open(QIODevice::ReadOnly)) error = QString::fromUtf8(result.readAll());
            return {false, error.isEmpty() ? QStringLiteral("The update helper failed to prepare the update.") : error, log};
        }
        if (QFileInfo::exists(ready)) return {true, {}, log};
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(helperPid));
        const bool finished = !process || WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
        if (process) CloseHandle(process);
        if (finished)
            return {false, QStringLiteral("The update helper exited before preparing the update. See ") + log, log};
        QThread::msleep(50);
    }
    saveFile(job.filePath(QStringLiteral("cancel")), QByteArrayLiteral("cancel"), &error);
    return {false, QStringLiteral("Preparing the update timed out. The application has not been closed."), log};
#endif
}

} // namespace ua::core::update
