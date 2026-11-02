#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

#include "core/items.h"

static QString fixtureRoot;

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
    static void put(const QString& path, Format format, const QString& value) {
        entries.insert(key(path, format, QStringLiteral(".")), value);
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

static void selectFixture(const QString& root, const char* name)
{
    fixtureRoot = root + '/' + QLatin1String(name);
    if (!QDir().mkpath(fixtureRoot + "/home")) qFatal("Fixture home setup failed");
    qputenv("PATH", (fixtureRoot + "/empty-bin").toUtf8());
    qputenv("APPDATA", (fixtureRoot + "/data").toUtf8());
    qputenv("LOCALAPPDATA", (fixtureRoot + "/local").toUtf8());
    qputenv("ProgramFiles", (fixtureRoot + "/program-files").toUtf8());
    qputenv("ProgramFiles(x86)", (fixtureRoot + "/program-files-x86").toUtf8());
    qunsetenv("VSCODE_COPILOT_HOME");
    qunsetenv("COPILOT_HOME");
    FixtureSettings::entries.clear();
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
