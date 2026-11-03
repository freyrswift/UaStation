#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <shlobj.h>

static QString fixtureRoot;

static QString sourceDirectory(const QString& name)
{
    return fixtureRoot + QLatin1Char('/') + name;
}

class FixtureStandardPaths {
public:
    static constexpr auto ApplicationsLocation = QStandardPaths::ApplicationsLocation;
    static constexpr auto DesktopLocation = QStandardPaths::DesktopLocation;

    static QStringList standardLocations(QStandardPaths::StandardLocation location)
    {
        if (location == ApplicationsLocation) return {sourceDirectory(QStringLiteral("user-programs"))};
        if (location == DesktopLocation) return {sourceDirectory(QStringLiteral("user-desktop"))};
        return {};
    }
};

static HRESULT WINAPI fixtureKnownFolderPath(REFKNOWNFOLDERID folder, DWORD, HANDLE, PWSTR* path)
{
    if (!path) return E_POINTER;
    *path = nullptr;
    QString value;
    if (IsEqualGUID(folder, FOLDERID_CommonPrograms))
        value = sourceDirectory(QStringLiteral("common-programs"));
    else if (IsEqualGUID(folder, FOLDERID_PublicDesktop))
        value = sourceDirectory(QStringLiteral("public-desktop"));
    else
        return E_INVALIDARG;

    value = QDir::toNativeSeparators(value);
    *path = static_cast<PWSTR>(CoTaskMemAlloc((size_t(value.size()) + 1) * sizeof(wchar_t)));
    if (!*path) return E_OUTOFMEMORY;
    value.toWCharArray(*path);
    (*path)[value.size()] = L'\0';
    return S_OK;
}

#define QStandardPaths FixtureStandardPaths
#define SHGetKnownFolderPath fixtureKnownFolderPath
#include "../src/platform/vscode_discovery.cpp"
#undef SHGetKnownFolderPath
#undef QStandardPaths

static void selectFixture(const QString& root, const QString& name)
{
    fixtureRoot = root + QLatin1Char('/') + name;
    for (const QString& source : {QStringLiteral("user-programs"), QStringLiteral("common-programs"),
                                  QStringLiteral("user-desktop"), QStringLiteral("public-desktop")}) {
        if (!QDir().mkpath(sourceDirectory(source))) qFatal("Shortcut fixture directory setup failed");
    }
}

static void putFile(const QString& path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) qFatal("Shortcut target directory setup failed");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write("fixture\n") != 8)
        qFatal("Shortcut target file setup failed");
}

static void putShortcut(const QString& path, const QString& target)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) qFatal("Shortcut parent setup failed");
    IShellLinkW* shortcut = nullptr;
    HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_IShellLinkW, reinterpret_cast<void**>(&shortcut));
    if (FAILED(result)) qFatal("Shortcut COM object creation failed: %lx", static_cast<unsigned long>(result));

    const QString nativeTarget = QDir::toNativeSeparators(target);
    result = shortcut->SetPath(reinterpret_cast<LPCWSTR>(nativeTarget.utf16()));
    IPersistFile* persistence = nullptr;
    if (SUCCEEDED(result))
        result = shortcut->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistence));
    if (SUCCEEDED(result)) {
        const QString nativePath = QDir::toNativeSeparators(path);
        result = persistence->Save(reinterpret_cast<LPCWSTR>(nativePath.utf16()), TRUE);
    }
    if (persistence) persistence->Release();
    shortcut->Release();
    if (FAILED(result)) qFatal("Shortcut fixture save failed: %lx", static_cast<unsigned long>(result));
}

static QString editorTarget(const QString& name = QStringLiteral("Code.exe"))
{
    return fixtureRoot + QString::fromUtf8("/\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8 \xe5\xae\x89\xe8\xa3\x85/") + name;
}

static QStringList discoveredTargets()
{
    QStringList targets = ua::platform::vscodeShortcutTargets();
    for (QString& target : targets) target = QDir::cleanPath(QDir::fromNativeSeparators(target));
    return targets;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--process-fixture"))) {
        QTextStream(stdout) << "ready\n" << Qt::flush;
        QTimer::singleShot(15000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        qFatal("Shortcut test COM initialization failed");

    QTemporaryDir temporary;
    if (!temporary.isValid()) qFatal("Shortcut temporary directory creation failed");
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool passed, const char* description) {
        ++checks;
        if (!passed) ++failures;
        QTextStream(stdout) << (passed ? "PASS " : "FAIL ") << description << '\n';
    };

    selectFixture(temporary.path(), QStringLiteral("processes"));
    for (const QString& name : {QStringLiteral("Code.exe"), QStringLiteral("Code - Insiders.exe")}) {
        const QString executable = editorTarget(name);
        if (!QDir().mkpath(QFileInfo(executable).absolutePath()) ||
            !QFile::copy(QCoreApplication::applicationFilePath(), executable))
            qFatal("Process fixture copy failed");
        QProcess child;
        child.start(executable, {QStringLiteral("--process-fixture")});
        const bool ready = child.waitForStarted(5000) && child.waitForReadyRead(5000) &&
            child.readAllStandardOutput().contains("ready");
        bool discovered = false;
        QElapsedTimer timer;
        timer.start();
        while (ready && !discovered && timer.elapsed() < 3000) {
            for (const QString& path : ua::platform::vscodeRunningExecutables())
                discovered = discovered || QDir::cleanPath(QDir::fromNativeSeparators(path)) == executable;
            if (!discovered) QThread::msleep(20);
        }
        if (child.state() != QProcess::NotRunning) {
            child.kill();
            child.waitForFinished(3000);
        }
        check(ready && discovered && child.state() == QProcess::NotRunning,
              qPrintable(name + QStringLiteral(" is discovered through a native process query with a Unicode path")));
    }

    selectFixture(temporary.path(), QStringLiteral("empty"));
    check(discoveredTargets().isEmpty(), "empty fixtures do not read actual Start menu or desktop shortcuts");

    for (const QString& source : {QStringLiteral("user-programs"), QStringLiteral("common-programs"),
                                  QStringLiteral("user-desktop"), QStringLiteral("public-desktop")}) {
        selectFixture(temporary.path(), source);
        const QString target = editorTarget();
        putFile(target);
        putShortcut(sourceDirectory(source) + QStringLiteral("/Visual Studio Code.lnk"), target);
        check(discoveredTargets().contains(target),
              qPrintable(source + QStringLiteral(" resolves a real shortcut with a spaced Unicode target")));
    }

    selectFixture(temporary.path(), QStringLiteral("deduplicate"));
    const QString duplicateTarget = editorTarget();
    putFile(duplicateTarget);
    for (const QString& source : {QStringLiteral("user-programs"), QStringLiteral("common-programs"),
                                  QStringLiteral("user-desktop"), QStringLiteral("public-desktop")})
        putShortcut(sourceDirectory(source) + QStringLiteral("/Visual Studio Code.lnk"), duplicateTarget);
    const QStringList duplicates = discoveredTargets();
    check(duplicates.size() == 1 && duplicates.contains(duplicateTarget),
          "the same shortcut target from all four sources is returned once");

    selectFixture(temporary.path(), QStringLiteral("depth"));
    const QString allowedTarget = editorTarget();
    const QString deepTarget = editorTarget(QStringLiteral("Code - Insiders.exe"));
    putFile(allowedTarget);
    putFile(deepTarget);
    const QString levelFour = sourceDirectory(QStringLiteral("user-programs")) + QStringLiteral("/one/two/three/four");
    putShortcut(levelFour + QStringLiteral("/Code.lnk"), allowedTarget);
    putShortcut(levelFour + QStringLiteral("/five/Insiders.lnk"), deepTarget);
    const QStringList depthTargets = discoveredTargets();
    check(depthTargets.contains(allowedTarget), "a shortcut at directory depth four is included");
    check(!depthTargets.contains(deepTarget), "a shortcut beyond directory depth four is excluded");

    selectFixture(temporary.path(), QStringLiteral("broken"));
    const QString validTarget = editorTarget();
    putFile(validTarget);
    putShortcut(sourceDirectory(QStringLiteral("user-programs")) + QStringLiteral("/broken.lnk"),
                fixtureRoot + QStringLiteral("/removed directory/Code.exe"));
    putShortcut(sourceDirectory(QStringLiteral("user-programs")) + QStringLiteral("/valid.lnk"), validTarget);
    check(discoveredTargets().contains(validTarget), "a broken shortcut does not interrupt discovery of a valid neighbor");

    selectFixture(temporary.path(), QStringLiteral("entry-budget"));
    for (int index = 0; index < 4096; ++index)
        putFile(sourceDirectory(QStringLiteral("user-programs")) + QLatin1Char('/') + QString::number(index) + QStringLiteral(".txt"));
    const QString budgetTarget = editorTarget();
    putFile(budgetTarget);
    putShortcut(sourceDirectory(QStringLiteral("common-programs")) + QStringLiteral("/Code.lnk"), budgetTarget);
    check(!discoveredTargets().contains(budgetTarget), "ordinary directory entries consume the shared 4096-entry budget");

    selectFixture(temporary.path(), QStringLiteral("source-priority"));
    for (int index = 0; index < 4096; ++index)
        putFile(sourceDirectory(QStringLiteral("user-desktop")) + QLatin1Char('/') + QString::number(index) + QStringLiteral(".txt"));
    const QString priorityTarget = editorTarget();
    putFile(priorityTarget);
    putShortcut(sourceDirectory(QStringLiteral("common-programs")) + QStringLiteral("/Code.lnk"), priorityTarget);
    check(discoveredTargets().contains(priorityTarget), "shared Start menu discovery precedes a desktop that exhausts the entry budget");

    QTextStream(stdout) << checks << " checks, " << failures << " failures\n";
    if (SUCCEEDED(initialized)) CoUninitialize();
    return failures == 0 ? 0 : 1;
}

#else
int main(int, char**)
{
    return 0;
}
#endif
