#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

#if defined(Q_OS_WIN)
#include <cstdlib>
#endif

#include "core/items.h"

static QString fixtureRoot;
static QStringList runningEditors;
static QStringList shortcutTargets;

namespace ua::platform {
QStringList vscodeRunningExecutables() { return runningEditors; }
QStringList vscodeShortcutTargets() { return shortcutTargets; }
}

static QString fixturePath(QString path)
{
    path = QDir::fromNativeSeparators(path);
#if defined(Q_OS_MACOS)
    if (path.startsWith(QLatin1String("/Applications/Visual Studio Code")))
        return fixtureRoot + "/platform" + path;
#elif !defined(Q_OS_WIN)
    if (path == QLatin1String("/usr/bin/code") ||
        path == QLatin1String("/usr/bin/code-insiders") ||
        path == QLatin1String("/usr/share/code/code") ||
        path == QLatin1String("/usr/share/code-insiders/code-insiders") ||
        path == QLatin1String("/opt/visual-studio-code/code") ||
        path == QLatin1String("/snap/bin/code") ||
        path == QLatin1String("/snap/bin/code-insiders"))
        return fixtureRoot + "/platform" + path;
#endif
    return path;
}

static bool insideFixture(const QString& path)
{
    const QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    return absolute.startsWith(QDir::cleanPath(fixtureRoot) + '/');
}

// Only executable probes are fenced; ordinary path normalization still uses Qt.
class FixtureFileInfo : public QFileInfo {
public:
    FixtureFileInfo() = default;
    FixtureFileInfo(const QString& path) : QFileInfo(fixturePath(path)) {}
    FixtureFileInfo(const QFileInfo& info) : QFileInfo(info) {}

    bool exists() const { return insideFixture(absoluteFilePath()) && QFileInfo::exists(); }
    bool isFile() const { return insideFixture(absoluteFilePath()) && QFileInfo::isFile(); }
    bool isDir() const { return insideFixture(absoluteFilePath()) && QFileInfo::isDir(); }
    bool isExecutable() const {
        return insideFixture(absoluteFilePath()) && QFileInfo::isExecutable();
    }
    static bool exists(const QString& path) {
        const QString mapped = fixturePath(path);
        return insideFixture(mapped) && QFileInfo::exists(mapped);
    }
};

class FixtureStandardPaths {
public:
    static constexpr auto HomeLocation = QStandardPaths::HomeLocation;
    static constexpr auto GenericDataLocation = QStandardPaths::GenericDataLocation;
    static QString writableLocation(QStandardPaths::StandardLocation location) {
        return fixtureRoot + (location == HomeLocation ? "/home" : "/data");
    }
};

class FixtureSettings {
public:
    using Format = QSettings::Format;
    static constexpr auto NativeFormat = QSettings::NativeFormat;
#if defined(Q_OS_WIN)
    static constexpr auto Registry32Format = QSettings::Registry32Format;
    static constexpr auto Registry64Format = QSettings::Registry64Format;
#endif
    static inline QHash<QString, QVariant> entries;

    FixtureSettings(const QString& path, Format format) : path_(path), format_(format) {}
    QVariant value(const QString& name, const QVariant& fallback = {}) const {
        return entries.value(key(path_, format_, name), fallback);
    }
    QStringList childGroups() const {
        QString prefix = QString::number(int(format_)) + '/' + path_;
        prefix.replace('\\', '/');
        prefix = prefix.toCaseFolded() + '/';
        QSet<QString> groups;
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            if (!it.key().startsWith(prefix)) continue;
            const QString rest = it.key().mid(prefix.size());
            const int slash = rest.indexOf('/');
            if (slash > 0) groups.insert(rest.left(slash));
        }
        return groups.values();
    }
    static void put(const QString& path, Format format, const QString& value) {
        entries.insert(key(path, format, QStringLiteral(".")), value);
    }
    static void putValue(const QString& path, Format format, const QString& name, const QString& value) {
        entries.insert(key(path, format, name), value);
    }

private:
    static QString key(QString path, Format format, QString name) {
        path.replace('\\', '/');
        if (name.isEmpty() || name == QLatin1String("Default")) name = QStringLiteral(".");
        return QString::number(int(format)) + '/' + path.toCaseFolded() + '/' + name.toCaseFolded();
    }
    QString path_;
    Format format_;
};

#define QFileInfo FixtureFileInfo
#define QSettings FixtureSettings
#define QStandardPaths FixtureStandardPaths
#include "../src/core/paths.cpp"
#undef QStandardPaths
#undef QSettings
#undef QFileInfo

using namespace ua::core;

static void putFile(const QString& path, bool executable = false)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) qFatal("Fixture parent setup failed");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write("fixture\n") < 0)
        qFatal("Fixture file setup failed");
    file.close();
    if (executable && !file.setPermissions(file.permissions() | QFileDevice::ExeOwner |
                                         QFileDevice::ExeGroup | QFileDevice::ExeOther))
        qFatal("Fixture permission setup failed");
}

static void putTextFile(const QString& path, const QByteArray& contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) qFatal("Fixture parent setup failed");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size())
        qFatal("Fixture text file setup failed");
}

static void selectFixture(const QString& root, const char* name)
{
    fixtureRoot = root + '/' + QLatin1String(name);
    if (!QDir().mkpath(fixtureRoot + "/home")) qFatal("Fixture home setup failed");
    qputenv("PATH", (fixtureRoot + "/empty-bin").toUtf8());
    qputenv("APPDATA", (fixtureRoot + "/data").toUtf8());
    qputenv("LOCALAPPDATA", (fixtureRoot + "/local").toUtf8());
    qputenv("ProgramFiles", (fixtureRoot + "/program-files").toUtf8());
    qputenv("ProgramFiles(x86)", (fixtureRoot + "/program-files-x86").toUtf8());
    qputenv("ProgramW6432", (fixtureRoot + "/program-files-native").toUtf8());
    qunsetenv("VSCODE_COPILOT_HOME");
    qunsetenv("VSCODE_INSTALL_DIR");
    qunsetenv("COPILOT_HOME");
    qunsetenv("UA_VSCODE_FIXTURE_ROOT");
    FixtureSettings::entries.clear();
    runningEditors.clear();
    shortcutTargets.clear();
}

static QString customEditor(const QString& install)
{
#if defined(Q_OS_WIN)
    return install + "/Code.exe";
#elif defined(Q_OS_MACOS)
    return install + "/Visual Studio Code.app/Contents/MacOS/Electron";
#else
    return install + "/code";
#endif
}

static QString customInstallRoot(const QString& install)
{
#if defined(Q_OS_MACOS)
    return install + "/Visual Studio Code.app";
#else
    return install;
#endif
}

static QString standardEditor()
{
#if defined(Q_OS_WIN)
    return fixtureRoot + "/local/Programs/Microsoft VS Code/Code.exe";
#elif defined(Q_OS_MACOS)
    return fixtureRoot + "/platform/Applications/Visual Studio Code.app/Contents/MacOS/Electron";
#else
    return fixtureRoot + "/platform/usr/bin/code";
#endif
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir root;
    if (!root.isValid()) qFatal("Fixture temporary root failed");
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool passed, const char* name) {
        ++checks;
        if (!passed) ++failures;
        QTextStream(stdout) << (passed ? "PASS " : "FAIL ") << name << '\n';
    };
    const auto detected = [] { return toolDetectedById(QStringLiteral("vscode")); };

    selectFixture(root.path(), "absent");
    check(!detected(), "no editor or instructions is not detected");

    selectFixture(root.path(), "path-directory");
    const QString directoryBin = fixtureRoot + "/bin";
    if (!QDir().mkpath(directoryBin + "/code")) qFatal("Fixture directory setup failed");
    qputenv("PATH", directoryBin.toUtf8());
    check(!detected(), "a directory named code is not an executable");

    selectFixture(root.path(), "standard-install");
    putFile(standardEditor(), true);
    check(detected(), "standard Visual Studio Code installation is detected");

    selectFixture(root.path(), "install-env-root-spaces");
    const QString spacedInstall = fixtureRoot + "/custom editor directory";
    putFile(customEditor(spacedInstall), true);
    qputenv("VSCODE_INSTALL_DIR", customInstallRoot(spacedInstall).toUtf8());
    check(vscodeInstallDir() == customInstallRoot(spacedInstall),
          "installation override preserves an unquoted directory containing spaces");

    selectFixture(root.path(), "install-env-executable");
    const QString executableInstall = fixtureRoot + "/custom editor executable";
    putFile(customEditor(executableInstall), true);
    qputenv("VSCODE_INSTALL_DIR", customEditor(executableInstall).toUtf8());
    check(vscodeInstallDir() == customInstallRoot(executableInstall),
          "installation override accepts an absolute editor executable containing spaces");
    qputenv("VSCODE_INSTALL_DIR", ('"' + customEditor(executableInstall) + '"').toUtf8());
    check(vscodeInstallDir() == customInstallRoot(executableInstall),
          "installation override accepts a quoted editor executable containing spaces");

    selectFixture(root.path(), "install-env-invalid-fallback");
    putFile(standardEditor(), true);
    qputenv("VSCODE_INSTALL_DIR", (fixtureRoot + "/removed editor").toUtf8());
    check(detected(), "an invalid installation override falls back to a real default installation");

    selectFixture(root.path(), "manual-selection");
    const QString selectedInstall = fixtureRoot + "/selected editor";
    const QString otherInstall = fixtureRoot + "/environment editor";
    putFile(customEditor(selectedInstall), true);
    putFile(customEditor(otherInstall), true);
    putFile(standardEditor(), true);
    qputenv("VSCODE_INSTALL_DIR", customInstallRoot(otherInstall).toUtf8());
    check(setVscodeInstallPath(customInstallRoot(selectedInstall)),
          "manual installation selection accepts a valid absolute installation root");
    check(vscodeInstallDir() == customInstallRoot(selectedInstall),
          "saved manual selection takes precedence over environment and default installations");
    check(QFileInfo(configDir() + "/vscode-install-path.txt").isFile(),
          "manual installation selection is persisted in the application configuration directory");
    check(!setVscodeInstallPath(fixtureRoot + "/missing editor"),
          "manual installation selection rejects a missing installation");
    check(vscodeInstallDir() == customInstallRoot(selectedInstall),
          "a rejected manual selection preserves the prior valid selection");
    check(!setVscodeInstallPath(QStringLiteral("relative/editor")),
          "manual installation selection rejects relative paths");
    putFile(fixtureRoot + "/unrelated application.exe", true);
    check(!setVscodeInstallPath(fixtureRoot + "/unrelated application.exe"),
          "manual installation selection rejects an unrelated executable");
    if (!QFile::remove(customEditor(selectedInstall))) qFatal("Fixture executable removal failed");
    check(vscodeInstallDir() == customInstallRoot(otherInstall),
          "a removed manual installation is revalidated and falls back to the environment");
    putFile(customEditor(selectedInstall), true);
    check(setVscodeInstallPath(customEditor(selectedInstall)),
          "manual installation selection accepts an absolute executable");
    check(vscodeInstallDir() == customInstallRoot(selectedInstall),
          "an executable selection resolves to the installation root");
    check(setVscodeInstallPath(QString()), "an empty manual selection clears the saved override");
    check(vscodeInstallDir() == customInstallRoot(otherInstall),
          "clearing the manual selection restores environment detection");

    selectFixture(root.path(), "manual-selection-reload");
    const QString reloadedInstall = fixtureRoot + "/previously selected editor";
    putFile(customEditor(reloadedInstall), true);
    putTextFile(configDir() + "/vscode-install-path.txt",
                customInstallRoot(reloadedInstall).toUtf8() + '\n');
    check(vscodeInstallDir() == customInstallRoot(reloadedInstall),
          "a previously saved installation path is read without an in-memory cache");
    putTextFile(configDir() + "/vscode-install-path.txt", (fixtureRoot + "/removed editor").toUtf8());
    check(!detected(), "a stale persisted installation path does not count as an installed editor");

    selectFixture(root.path(), "manual-selection-directory-executable");
    const QString directoryExe = fixtureRoot + "/pretend/Code.exe";
    if (!QDir().mkpath(directoryExe)) qFatal("Fixture directory setup failed");
    check(!setVscodeInstallPath(directoryExe),
          "manual installation selection rejects a directory masquerading as an editor executable");

#if defined(Q_OS_MACOS)
    selectFixture(root.path(), "user-applications");
    putFile(homeDir() + "/Applications/Visual Studio Code.app/Contents/MacOS/Electron", true);
    check(detected(), "Visual Studio Code in user Applications is detected");
    selectFixture(root.path(), "standard-insiders");
    putFile(fixtureRoot + "/platform/Applications/Visual Studio Code - Insiders.app/Contents/MacOS/Electron", true);
    check(detected(), "standard Insiders application bundle is detected");
#elif !defined(Q_OS_WIN)
    int installation = 0;
    for (const QString executable : {QStringLiteral("/usr/share/code-insiders/code-insiders"),
                                     QStringLiteral("/snap/bin/code"), QStringLiteral("/snap/bin/code-insiders")}) {
        const QByteArray name = "linux-installation-" + QByteArray::number(installation++);
        selectFixture(root.path(), name.constData());
        putFile(fixtureRoot + "/platform" + executable, true);
        check(detected(), "Linux Insiders and Snap installations are detected");
    }
#endif

    selectFixture(root.path(), "custom-path");
    const QString customBin = fixtureRoot + "/custom editor/bin";
#if defined(Q_OS_WIN)
    putFile(customBin + "/code.cmd", true);
#else
    putFile(customBin + "/code", true);
#endif
    qputenv("PATH", customBin.toUtf8());
    check(detected(), "custom installation is detected through its PATH launcher");

#if defined(Q_OS_WIN)
    qputenv("PATH", ('"' + customBin + '"').toUtf8());
    check(detected(), "quoted PATH entry with spaces is detected");

    selectFixture(root.path(), "path-launcher-root");
    const QString launcherInstall = fixtureRoot + "/PATH editor";
    putFile(launcherInstall + "/bin/code.cmd", true);
    putFile(launcherInstall + "/Code.exe", true);
    qputenv("PATH", (launcherInstall + "/bin").toUtf8());
    check(vscodeInstallDir() == launcherInstall,
          "a PATH command launcher resolves to the editor root rather than its bin directory");
    putFile(launcherInstall + "/bin/code", true);
    check(vscodeInstallDir() == launcherInstall,
          "coexisting extensionless and cmd launchers resolve to the editor root");

    selectFixture(root.path(), "insiders-path-launcher-root");
    const QString insidersInstall = fixtureRoot + "/PATH Insiders editor";
    putFile(insidersInstall + "/bin/code-insiders", true);
    putFile(insidersInstall + "/bin/code-insiders.cmd", true);
    putFile(insidersInstall + "/Code - Insiders.exe", true);
    qputenv("PATH", (insidersInstall + "/bin").toUtf8());
    check(vscodeInstallDir() == insidersInstall,
          "coexisting Insiders launchers resolve to the editor root");

    selectFixture(root.path(), "environment-expanded-install");
    const QString expandedInstall = fixtureRoot + "/expanded editor";
    putFile(expandedInstall + "/Code.exe", true);
    qputenv("UA_VSCODE_FIXTURE_ROOT", fixtureRoot.toUtf8());
    qputenv("VSCODE_INSTALL_DIR", "%UA_VSCODE_FIXTURE_ROOT%/expanded editor");
    check(vscodeInstallDir() == expandedInstall,
          "installation overrides expand percent-delimited Windows environment variables");
    qputenv("VSCODE_INSTALL_DIR", "\"%UA_VSCODE_FIXTURE_ROOT%/expanded editor/Code.exe\"");
    check(vscodeInstallDir() == expandedInstall,
          "quoted executable overrides expand Windows environment variables before validation");

    selectFixture(root.path(), "unicode-install");
    const QString unicodeInstall = fixtureRoot + QString::fromUtf8("/\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8 \xe5\xae\x89\xe8\xa3\x85");
    putFile(unicodeInstall + "/Code.exe", true);
    if (_wputenv_s(L"VSCODE_INSTALL_DIR", reinterpret_cast<const wchar_t*>(unicodeInstall.utf16())) != 0)
        qFatal("Fixture Unicode environment setup failed");
    check(vscodeInstallDir() == unicodeInstall,
          "installation overrides preserve non-ASCII Windows directory names");
    check(setVscodeInstallPath(unicodeInstall), "manual selection accepts a non-ASCII directory");
    qunsetenv("VSCODE_INSTALL_DIR");
    check(vscodeInstallDir() == unicodeInstall,
          "persisted installation paths preserve UTF-8 non-ASCII directory names");

    selectFixture(root.path(), "native-program-files");
    const QString nativeInstall = fixtureRoot + "/program-files-native/Microsoft VS Code";
    putFile(nativeInstall + "/Code.exe", true);
    check(vscodeInstallDir() == nativeInstall,
          "ProgramW6432 finds a native system installation from a different process architecture");
#endif

    selectFixture(root.path(), "insiders-path");
    const QString insidersBin = fixtureRoot + "/insiders/bin";
#if defined(Q_OS_WIN)
    putFile(insidersBin + "/code-insiders.cmd", true);
#else
    putFile(insidersBin + "/code-insiders", true);
#endif
    qputenv("PATH", insidersBin.toUtf8());
    check(detected(), "Insiders PATH launcher is detected");

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    check(vscodeInstallDir() == insidersBin,
          "Linux code-insiders executable discovery retains its containing directory");
#endif

#if defined(Q_OS_WIN)
    for (auto format : {QSettings::Registry32Format, QSettings::Registry64Format}) {
        for (const QString hive : {QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE")}) {
            selectFixture(root.path(), "registry-app-path");
            const QString editor = fixtureRoot + "/custom registry/Code.exe";
            putFile(editor, true);
            FixtureSettings::put(hive + "\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Code.exe",
                                 format, editor);
            check(detected(), "custom installation is detected through App Paths in either registry view");
        }
    }

    selectFixture(root.path(), "registry-insiders-app-path");
    const QString insidersExe = fixtureRoot + "/custom registry/Code - Insiders.exe";
    putFile(insidersExe, true);
    FixtureSettings::put("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\code-insiders.exe",
                         QSettings::Registry64Format, insidersExe);
    check(detected(), "Insiders custom installation is detected through App Paths");

    for (const QString executable : {QStringLiteral("Code.exe"), QStringLiteral("Code - Insiders.exe")}) {
        selectFixture(root.path(), "registry-open-command");
        const QString editor = fixtureRoot + "/custom registry/" + executable;
        putFile(editor, true);
        FixtureSettings::put("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\" + executable +
                             "\\shell\\open\\command", QSettings::Registry64Format,
                             '"' + editor + "\" \"%1\"");
        check(detected(), "quoted registered open command is parsed without including arguments");
    }

    selectFixture(root.path(), "stale-registry");
    FixtureSettings::put("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Code.exe",
                         QSettings::Registry64Format, fixtureRoot + "/removed/Code.exe");
    check(!detected(), "stale registry registration does not count as installed");

    selectFixture(root.path(), "registry-directory");
    const QString fakeExe = fixtureRoot + "/directory/Code.exe";
    if (!QDir().mkpath(fakeExe)) qFatal("Fixture directory setup failed");
    FixtureSettings::put("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Code.exe",
                         QSettings::Registry64Format, fakeExe);
    check(!detected(), "registry directory masquerading as Code.exe is rejected");

    int registryCase = 0;
    for (auto format : {QSettings::Registry32Format, QSettings::Registry64Format}) {
        for (const QString hive : {QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE")}) {
            const QByteArray fixtureName = "registry-uninstall-location-" + QByteArray::number(registryCase++);
            selectFixture(root.path(), fixtureName.constData());
            const QString install = fixtureRoot + "/D/Apps/Visual Studio Code";
            putFile(install + "/Code.exe", true);
            const QString key = hive + "\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{fixture-vscode}";
            FixtureSettings::putValue(key, format, "DisplayName", "Microsoft Visual Studio Code (User)");
            FixtureSettings::putValue(key, format, "InstallLocation", install);
            check(vscodeInstallDir() == install,
                  "uninstall InstallLocation finds custom installations in both hives and registry views");
        }
    }

    for (const bool quoted : {false, true}) {
        const QByteArray fixtureName = quoted ? "registry-uninstall-icon-quoted" : "registry-uninstall-icon-unquoted";
        selectFixture(root.path(), fixtureName.constData());
        const QString install = fixtureRoot + "/icon directory with spaces";
        putFile(install + "/Code.exe", true);
        const QString key = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{fixture-icon}";
        FixtureSettings::putValue(key, QSettings::Registry64Format, "DisplayName", "Microsoft Visual Studio Code");
        const QString icon = quoted ? '"' + install + "/Code.exe\",0" : install + "/Code.exe,0";
        FixtureSettings::putValue(key, QSettings::Registry64Format, "DisplayIcon", icon);
        check(vscodeInstallDir() == install,
              "uninstall DisplayIcon preserves path spaces and strips its quoted or unquoted icon index");
    }

    selectFixture(root.path(), "registry-uninstall-stale-location");
    const QString staleFallbackInstall = fixtureRoot + "/valid uninstall icon";
    putFile(staleFallbackInstall + "/Code.exe", true);
    const QString fallbackKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{fixture-fallback}";
    FixtureSettings::putValue(fallbackKey, QSettings::Registry32Format, "DisplayName", "Microsoft Visual Studio Code (User)");
    FixtureSettings::putValue(fallbackKey, QSettings::Registry32Format, "InstallLocation", fixtureRoot + "/removed installation");
    FixtureSettings::putValue(fallbackKey, QSettings::Registry32Format, "DisplayIcon", staleFallbackInstall + "/Code.exe,0");
    check(vscodeInstallDir() == staleFallbackInstall,
          "a stale uninstall InstallLocation falls back to its valid DisplayIcon");

    selectFixture(root.path(), "registry-uninstall-stale-entry");
    const QString validUninstall = fixtureRoot + "/valid uninstall registration";
    putFile(validUninstall + "/Code.exe", true);
    const QString uninstallBase = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";
    FixtureSettings::putValue(uninstallBase + "old", QSettings::Registry64Format, "DisplayName", "Microsoft Visual Studio Code");
    FixtureSettings::putValue(uninstallBase + "old", QSettings::Registry64Format, "InstallLocation", fixtureRoot + "/stale editor");
    FixtureSettings::putValue(uninstallBase + "current", QSettings::Registry64Format, "DisplayName", "Microsoft Visual Studio Code (User)");
    FixtureSettings::putValue(uninstallBase + "current", QSettings::Registry64Format, "InstallLocation", validUninstall);
    check(vscodeInstallDir() == validUninstall,
          "a stale uninstall entry does not hide another valid editor installation");

    int unrelatedEntry = 0;
    for (const QString display : {QStringLiteral("Visual Studio Code Helper"),
                                   QStringLiteral("Not Visual Studio Code"),
                                   QStringLiteral("Microsoft Visual Studio Code Extension Pack"),
                                   QStringLiteral("Visual Studio Community 2022")}) {
        const QByteArray fixtureName = "registry-uninstall-unrelated-" + QByteArray::number(unrelatedEntry++);
        selectFixture(root.path(), fixtureName.constData());
        const QString unrelatedInstall = fixtureRoot + "/unrelated product";
        putFile(unrelatedInstall + "/Code.exe", true);
        const QString key = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{fixture-unrelated}";
        FixtureSettings::putValue(key, QSettings::Registry64Format, "DisplayName", display);
        FixtureSettings::putValue(key, QSettings::Registry64Format, "InstallLocation", unrelatedInstall);
        check(!detected(), "unrelated uninstall display names do not count as Visual Studio Code");
    }

    selectFixture(root.path(), "registry-uninstall-insiders");
    const QString uninstallInsiders = fixtureRoot + "/custom insiders install";
    putFile(uninstallInsiders + "/Code - Insiders.exe", true);
    const QString insidersKey = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{fixture-insiders}";
    FixtureSettings::putValue(insidersKey, QSettings::Registry32Format, "DisplayName", "Microsoft Visual Studio Code - Insiders (User)");
    FixtureSettings::putValue(insidersKey, QSettings::Registry32Format, "InstallLocation", uninstallInsiders);
    check(vscodeInstallDir() == uninstallInsiders,
          "a custom Insiders installation is detected from the 32-bit registry uninstall view");

    selectFixture(root.path(), "registry-uninstall-directory-executable");
    const QString uninstallFakeExe = fixtureRoot + "/uninstall directory/Code.exe";
    if (!QDir().mkpath(uninstallFakeExe)) qFatal("Fixture directory setup failed");
    const QString fakeKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{fixture-directory}";
    FixtureSettings::putValue(fakeKey, QSettings::Registry64Format, "DisplayName", "Microsoft Visual Studio Code");
    FixtureSettings::putValue(fakeKey, QSettings::Registry64Format, "DisplayIcon", uninstallFakeExe + ",0");
    check(!detected(), "uninstall DisplayIcon rejects a directory masquerading as Code.exe");

    selectFixture(root.path(), "registry-unquoted-open-command");
    const QString unquotedCommandRoot = fixtureRoot + "/custom registry with spaces";
    putFile(unquotedCommandRoot + "/Code.exe", true);
    FixtureSettings::put("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\Code.exe\\shell\\open\\command",
                         QSettings::Registry64Format, unquotedCommandRoot + "/Code.exe --open-url %1");
    check(vscodeInstallDir() == unquotedCommandRoot,
          "registered open commands preserve an unquoted executable path containing spaces");

    int handlerCase = 0;
    for (auto format : {QSettings::Registry32Format, QSettings::Registry64Format}) {
        for (const QString hive : {QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE")}) {
            const QByteArray fixtureName = "registry-url-handler-" + QByteArray::number(handlerCase++);
            selectFixture(root.path(), fixtureName.constData());
            const QString install = fixtureRoot + "/registered URL editor";
            putFile(install + "/Code.exe", true);
            FixtureSettings::put(hive + "\\Software\\Classes\\vscode\\shell\\open\\command", format,
                                 '"' + install + "/Code.exe\" --open-url -- \"%1\"");
            check(vscodeInstallDir() == install,
                  "URL handler registration finds Visual Studio Code in both hives and registry views");
        }
    }

    selectFixture(root.path(), "registry-insiders-url-handler");
    const QString insidersHandler = fixtureRoot + "/URL insiders editor";
    putFile(insidersHandler + "/Code - Insiders.exe", true);
    FixtureSettings::put("HKEY_CURRENT_USER\\Software\\Classes\\vscode-insiders\\shell\\open\\command",
                         QSettings::Registry64Format, '"' + insidersHandler + "/Code - Insiders.exe\" --open-url \"%1\"");
    check(vscodeInstallDir() == insidersHandler, "the Insiders URL handler finds its custom installation");

    selectFixture(root.path(), "running-editor-process");
    const QString runningInstall = fixtureRoot + "/running custom editor";
    putFile(runningInstall + "/Code.exe", true);
    runningEditors << fixtureRoot + "/removed process/Code.exe" << runningInstall + "/Code.exe";
    check(vscodeInstallDir() == runningInstall,
          "running editor discovery validates each process path and skips stale candidates");

    selectFixture(root.path(), "running-unrelated-process");
    putFile(fixtureRoot + "/unrelated.exe", true);
    runningEditors << fixtureRoot + "/unrelated.exe";
    check(!detected(), "an unrelated running executable does not identify Visual Studio Code");

    selectFixture(root.path(), "shortcut-editor");
    const QString shortcutInstall = fixtureRoot + "/shortcut editor with spaces";
    putFile(shortcutInstall + "/Code.exe", true);
    shortcutTargets << fixtureRoot + "/removed shortcut/Code.exe" << shortcutInstall + "/Code.exe";
    check(vscodeInstallDir() == shortcutInstall,
          "resolved Start menu or desktop shortcuts detect a custom editor and skip stale targets");

    selectFixture(root.path(), "shortcut-invalid-target");
    putFile(fixtureRoot + "/unrelated.exe", true);
    shortcutTargets << fixtureRoot + "/unrelated.exe";
    check(!detected(), "unrelated shortcut targets do not identify Visual Studio Code");

    selectFixture(root.path(), "explicit-install-dir");
    const QString explicitEditor = fixtureRoot + "/portable/Code.exe";
    putFile(explicitEditor, true);
    qputenv("VSCODE_INSTALL_DIR", (fixtureRoot + "/portable").toUtf8());
    check(detected(), "portable installation is detected through VSCODE_INSTALL_DIR");
#endif

    selectFixture(root.path(), "copilot-only");
    putFile(homeDir() + "/.copilot/config.json");
    qputenv("COPILOT_HOME", (fixtureRoot + "/copilot-cli").toUtf8());
    putFile(fixtureRoot + "/copilot-cli/config.json");
    check(!detected(), "Copilot CLI configuration alone does not identify Visual Studio Code");
    check(toolConfigRoot("vscode") == homeDir() + "/.copilot",
          "COPILOT_HOME does not override Visual Studio Code instructions");

    for (bool createDirectory : {false, true}) {
        selectFixture(root.path(), createDirectory ? "empty-override" : "missing-override");
        putFile(standardEditor(), true);
        const QString override = fixtureRoot + "/custom instructions";
        if (createDirectory && !QDir().mkpath(override)) qFatal("Fixture override setup failed");
        qputenv("VSCODE_COPILOT_HOME", override.toUtf8());
        check(detected(), "empty or missing instructions override does not hide an installed editor");
        check(toolPath("vscode", itemById("vscode")->targetRel) == override + "/instructions/Ua.instructions.md",
              "instructions target still respects VSCODE_COPILOT_HOME");
    }

    selectFixture(root.path(), "instructions-only");
    putFile(homeDir() + "/.copilot/instructions/custom.instructions.md");
    check(detected(), "existing user instruction files remain valid detection evidence");

    QTextStream(stdout) << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
