#include "core/update_installer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>

#include <cstdio>
#include <iterator>

#if defined(Q_OS_WIN)
#define NOMINMAX
#include <windows.h>
#endif

namespace {
int checks = 0;
int failures = 0;

void check(bool passed, const char* description)
{
    ++checks;
    if (!passed) ++failures;
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", description);
    std::fflush(stdout);
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString sha256(const QString& path)
{
    return QString::fromLatin1(QCryptographicHash::hash(readFile(path), QCryptographicHash::Sha256).toHex());
}

bool waitForFile(const QString& path, int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        if (QFileInfo::exists(path)) return true;
        QThread::msleep(50);
    }
    return false;
}

#if defined(Q_OS_WIN)
QString powershell()
{
    return QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows")))
        .filePath(QStringLiteral("System32/WindowsPowerShell/v1.0/powershell.exe"));
}

bool zip(const QString& workspace, const QString& source, const QString& target,
         bool traversal = false)
{
    const QString script = workspace + QStringLiteral("/zip.ps1");
    if (!writeFile(script, QByteArrayLiteral(
            "param([string]$Source,[string]$Target,[string]$Traversal)\n"
            "$ErrorActionPreference='Stop'\n"
            "Add-Type -AssemblyName System.IO.Compression.FileSystem\n"
            "[IO.Compression.ZipFile]::CreateFromDirectory($Source,$Target)\n"
            "if ($Traversal -eq 'yes') {\n"
            " $zip=[IO.Compression.ZipFile]::Open($Target,[IO.Compression.ZipArchiveMode]::Update)\n"
            " try { $entry=$zip.CreateEntry('../outside.txt'); $stream=$entry.Open(); $stream.WriteByte(65); $stream.Dispose() } finally { $zip.Dispose() }\n"
            "}\n"))) return false;
    QProcess process;
    process.start(powershell(), {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                                QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                                QStringLiteral("-File"), script, source, target,
                                traversal ? QStringLiteral("yes") : QStringLiteral("no")});
    if (!process.waitForFinished(30000) || process.exitCode() != 0) {
        std::fprintf(stderr, "%s", process.readAllStandardError().constData());
        return false;
    }
    return true;
}

bool prepareExecutable(const QString& directory)
{
    wchar_t libraryPath[32768]{};
    const DWORD length = GetModuleFileNameW(GetModuleHandleW(L"Qt6Core.dll"), libraryPath,
                                            static_cast<DWORD>(std::size(libraryPath)));
    const QString qtCore = QString::fromWCharArray(libraryPath, static_cast<int>(length));
    return QDir().mkpath(directory) &&
           QFile::copy(QCoreApplication::applicationFilePath(), directory + QStringLiteral("/ua.exe")) &&
           QFile::copy(qtCore,
                       directory + QStringLiteral("/Qt6Core.dll"));
}

bool startFixture(QProcess* process, const QString& directory,
                  const QString& name = QStringLiteral("ua.exe"))
{
    if (!writeFile(directory + QStringLiteral("/fixture-mode"), "fixture") ||
        !writeFile(directory + QStringLiteral("/version.txt"), "old")) return false;
    process->setWorkingDirectory(directory);
    process->start(directory + QLatin1Char('/') + name);
    return process->waitForStarted(10000) && waitForFile(directory + QStringLiteral("/launch-old.txt"), 10000);
}

void invalidPackageTests(const QString& workspace)
{
    const QString app = workspace + QStringLiteral("/invalid app's directory");
    const QString payload = workspace + QStringLiteral("/invalid-payload");
    const QString archive = workspace + QStringLiteral("/invalid.zip");
    check(prepareExecutable(app) && QDir().mkpath(payload) &&
          QFile::copy(QCoreApplication::applicationFilePath(), payload + QStringLiteral("/ua.exe")),
          "invalid-package fixtures prepared");
    QProcess old;
    check(startFixture(&old, app), "old fixture process started");
    if (old.state() == QProcess::NotRunning) return;
    check(zip(workspace, payload, archive), "invalid package created");
    const auto result = ua::core::update::launchInstaller(archive, app, app + QStringLiteral("/ua.exe"),
                                                         old.processId(), sha256(archive));
    check(!result.ok && result.error.contains(QStringLiteral("Qt runtime")),
          "missing runtime rejected before original application exits");
    check(old.state() == QProcess::Running && readFile(app + QStringLiteral("/version.txt")) == "old",
          "preparation failure preserves running original application");
    old.kill();
    old.waitForFinished(10000);
}

void traversalTests(const QString& workspace)
{
    const QString app = workspace + QStringLiteral("/traversal-app");
    const QString payload = workspace + QStringLiteral("/traversal-payload");
    const QString archive = workspace + QStringLiteral("/traversal.zip");
    check(prepareExecutable(app) && QDir().mkpath(payload), "traversal fixtures prepared");
    QProcess old;
    check(startFixture(&old, app), "traversal original fixture started");
    if (old.state() == QProcess::NotRunning) return;
    check(zip(workspace, payload, archive, true), "path-traversal archive created");
    const auto result = ua::core::update::launchInstaller(archive, app, app + QStringLiteral("/ua.exe"),
                                                         old.processId(), sha256(archive));
    check(!result.ok && result.error.contains(QStringLiteral("Invalid archive path")),
          "archive traversal is rejected before extraction");
    check(!QFileInfo::exists(QFileInfo(result.logPath).absolutePath() + QStringLiteral("/outside.txt")),
          "archive traversal never writes outside the staging directory");
    old.kill();
    old.waitForFinished(10000);
}

void replacementTests(const QString& workspace)
{
    const QString app = workspace + QStringLiteral("/app 'quoted' ") + QString::fromUtf8("\xe4\xb8\xad\xe6\x96\x87");
    const QString payload = workspace + QStringLiteral("/new-payload");
    const QString archive = workspace + QStringLiteral("/release.zip");
    check(prepareExecutable(app) && prepareExecutable(payload), "replacement fixtures and Qt runtime prepared");
    const QString executable = app + QStringLiteral("/chosen app.exe");
    check(QFile::rename(app + QStringLiteral("/ua.exe"), executable), "original executable has a custom name");
    check(writeFile(payload + QStringLiteral("/fixture-mode"), "fixture") &&
          writeFile(payload + QStringLiteral("/version.txt"), "new") &&
          writeFile(payload + QStringLiteral("/platforms/qwindows.dll"), "platform fixture") &&
          writeFile(payload + QStringLiteral("/nested/new-file.txt"), "new dependency") &&
          writeFile(app + QStringLiteral("/keep-user-file.txt"), "preserve"),
          "new package dependencies and unrelated user file prepared");
    QProcess old;
    check(startFixture(&old, app, QStringLiteral("chosen app.exe")), "replaceable original fixture started");
    if (old.state() == QProcess::NotRunning) return;
    check(zip(workspace, payload, archive), "full replacement package created");
    const auto badHash = ua::core::update::launchInstaller(archive, app, executable,
                                                          old.processId(), QString(64, QLatin1Char('0')));
    check(!badHash.ok && badHash.error.contains(QStringLiteral("checksum")), "checksum mismatch prevents installation");
    const auto result = ua::core::update::launchInstaller(archive, app, executable,
                                                         old.processId(), sha256(archive));
    check(result.ok, "helper stages valid package with quotes, spaces and Unicode paths");
    if (!result.ok) std::fprintf(stderr, "%s\n", result.error.toUtf8().constData());
    check(readFile(app + QStringLiteral("/version.txt")) == "old", "files remain unchanged until exact original process exits");
    old.kill();
    old.waitForFinished(10000);
    if (!result.ok) return;
    check(waitForFile(app + QStringLiteral("/launch-new.txt"), 20000), "replacement executable relaunches from original path");
    check(readFile(app + QStringLiteral("/launch-path.txt")) == QFileInfo(executable).canonicalFilePath().toUtf8() &&
          !QFileInfo::exists(app + QStringLiteral("/ua.exe")), "custom executable name is preserved without creating a second executable");
    const QString complete = QFileInfo(result.logPath).absolutePath() + QStringLiteral("/complete");
    check(waitForFile(complete, 15000), "helper records successful post-launch verification");
    check(readFile(app + QStringLiteral("/version.txt")) == "new" &&
          readFile(app + QStringLiteral("/nested/new-file.txt")) == "new dependency" &&
          readFile(app + QStringLiteral("/platforms/qwindows.dll")) == "platform fixture",
          "full package files and nested dependencies are installed");
    check(readFile(app + QStringLiteral("/keep-user-file.txt")) == "preserve", "unrelated user files are preserved");
    const DWORD newPid = readFile(app + QStringLiteral("/launch-new.txt")).toULong();
    HANDLE child = OpenProcess(SYNCHRONIZE, FALSE, newPid);
    if (child) {
        check(WaitForSingleObject(child, 20000) == WAIT_OBJECT_0, "restarted harmless fixture exits normally");
        CloseHandle(child);
    }
}

void skillsServiceLockTests(const QString& workspace)
{
    const QString app = workspace + QStringLiteral("/skills-lock-app");
    const QString payload = workspace + QStringLiteral("/skills-lock-payload");
    const QString archive = workspace + QStringLiteral("/skills-lock.zip");
    const QString server = app + QStringLiteral("/ua-skills-mcp.exe");
    check(prepareExecutable(app) && prepareExecutable(payload) &&
          writeFile(server, "locked service fixture") &&
          writeFile(payload + QStringLiteral("/ua-skills-mcp.exe"), "new service fixture") &&
          writeFile(payload + QStringLiteral("/platforms/qwindows.dll"), "platform fixture"),
          "skills service lock fixtures prepared");
    QProcess old;
    check(startFixture(&old, app), "skills service original fixture started");
    if (old.state() == QProcess::NotRunning) return;
    check(zip(workspace, payload, archive), "skills service update package created");
    const HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(server.utf16()), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(lock != INVALID_HANDLE_VALUE, "skills service executable is held with a native read lock");
    if (lock != INVALID_HANDLE_VALUE) {
        const auto result = ua::core::update::launchInstaller(archive, app, app + QStringLiteral("/ua.exe"),
                                                             old.processId(), sha256(archive));
        check(!result.ok && result.error.contains(QStringLiteral("MCP service")),
              "occupied skills service is rejected during preparation with actionable error");
        check(old.state() == QProcess::Running && readFile(server) == "locked service fixture" &&
              readFile(app + QStringLiteral("/version.txt")) == "old",
              "skills service lock failure preserves running app and original package files");
        CloseHandle(lock);
    }
    old.kill();
    old.waitForFinished(10000);
}

BOOL CALLBACK closeHelperWindow(HWND window, LPARAM helperId)
{
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == static_cast<DWORD>(helperId)) PostMessageW(window, WM_CLOSE, 0, 0);
    return TRUE;
}

void rollbackTests(const QString& workspace)
{
    const QString app = workspace + QStringLiteral("/rollback-app");
    const QString payload = workspace + QStringLiteral("/rollback-payload");
    const QString archive = workspace + QStringLiteral("/rollback.zip");
    check(prepareExecutable(app) && prepareExecutable(payload), "rollback application fixtures prepared");
    check(writeFile(payload + QStringLiteral("/fixture-mode"), "fixture") &&
          writeFile(payload + QStringLiteral("/version.txt"), "new") &&
          writeFile(payload + QStringLiteral("/fixture-crash"), "crash") &&
          writeFile(payload + QStringLiteral("/platforms/qwindows.dll"), "platform fixture") &&
          writeFile(app + QStringLiteral("/keep-user-file.txt"), "preserve"),
          "deliberately failing update payload prepared");
    QProcess old;
    check(startFixture(&old, app), "rollback original fixture started");
    if (old.state() == QProcess::NotRunning) return;
    check(zip(workspace, payload, archive), "rollback archive created");
    const auto result = ua::core::update::launchInstaller(archive, app, app + QStringLiteral("/ua.exe"),
                                                         old.processId(), sha256(archive));
    check(result.ok, "rollback helper prepares validated payload");
    if (!result.ok) {
        std::fprintf(stderr, "%s\n", result.error.toUtf8().constData());
        old.kill();
        old.waitForFinished(10000);
        return;
    }
    old.kill();
    old.waitForFinished(10000);
    QFile::remove(app + QStringLiteral("/launch-old.txt"));
    const QString job = QFileInfo(result.logPath).absolutePath();
    check(waitForFile(job + QStringLiteral("/failed"), 20000), "immediate updated-app exit triggers rollback");
    check(readFile(app + QStringLiteral("/version.txt")) == "old" &&
          !QFileInfo::exists(app + QStringLiteral("/fixture-crash")) &&
          !QFileInfo::exists(app + QStringLiteral("/platforms/qwindows.dll")),
          "rollback restores touched files and removes newly installed files");
    check(waitForFile(app + QStringLiteral("/launch-old.txt"), 10000), "original application restarts after rollback");
    check(readFile(app + QStringLiteral("/keep-user-file.txt")) == "preserve", "rollback preserves unrelated files");
    const DWORD helperId = readFile(job + QStringLiteral("/helper.pid")).toULong();
    const DWORD restartedId = readFile(app + QStringLiteral("/launch-old.txt")).toULong();
    HANDLE restarted = OpenProcess(SYNCHRONIZE, FALSE, restartedId);
    if (restarted) {
        check(WaitForSingleObject(restarted, 20000) == WAIT_OBJECT_0, "restored fixture exits normally");
        CloseHandle(restarted);
    }
    HANDLE helper = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, helperId);
    if (helper) {
        // Unattended runners may have no interactive desktop. After validating
        // rollback, clean up the owned helper even if its deliberate error notice stays open.
        QElapsedTimer deadline;
        deadline.start();
        DWORD state = WaitForSingleObject(helper, 0);
        while (state == WAIT_TIMEOUT && deadline.elapsed() < 5000) {
            EnumWindows(closeHelperWindow, static_cast<LPARAM>(helperId));
            state = WaitForSingleObject(helper, 100);
        }
        if (state == WAIT_TIMEOUT) {
            const BOOL terminated = TerminateProcess(helper, 1);
            check(terminated && WaitForSingleObject(helper, 10000) == WAIT_OBJECT_0,
                  "owned rollback helper is cleaned up without an interactive desktop");
        } else {
            check(state == WAIT_OBJECT_0, "rollback helper exits after its failure notice closes");
        }
        CloseHandle(helper);
    } else {
        check(GetLastError() == ERROR_INVALID_PARAMETER, "rollback helper already exited");
    }
}
#endif
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    const QString appDir = QCoreApplication::applicationDirPath();
    if (QFileInfo::exists(appDir + QStringLiteral("/fixture-mode"))) {
        const QString version = QString::fromUtf8(readFile(appDir + QStringLiteral("/version.txt"))).trimmed();
        if (!writeFile(appDir + QStringLiteral("/launch-") + version + QStringLiteral(".txt"),
                       QByteArray::number(QCoreApplication::applicationPid()))) return 2;
        if (!writeFile(appDir + QStringLiteral("/launch-path.txt"), QCoreApplication::applicationFilePath().toUtf8())) return 2;
        if (QFileInfo::exists(appDir + QStringLiteral("/fixture-crash"))) return 42;
        QThread::sleep(12);
        return 0;
    }
#if defined(Q_OS_WIN)
    QTemporaryDir workspace(QDir::tempPath() + QStringLiteral("/ua-installer-test-XXXXXX"));
    check(workspace.isValid(), "isolated test workspace created");
    if (workspace.isValid()) {
        invalidPackageTests(workspace.path());
        traversalTests(workspace.path());
        replacementTests(workspace.path());
        skillsServiceLockTests(workspace.path());
        rollbackTests(workspace.path());
    }
#else
    const auto result = ua::core::update::launchInstaller({}, {}, {}, 0, {});
    check(!result.ok && result.error.contains(QStringLiteral("Windows only")),
          "non-Windows automatic-install limitation is explicit");
#endif
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
