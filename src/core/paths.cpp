// paths.cpp - Cross-platform path resolution

#include "paths.h"

#include "items.h"
#include "platform/vscode_discovery.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

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
    // Match the Win32 build's layout: one "Ua" segment under %APPDATA%
    // (= Roaming on Windows). Qt's QStandardPaths doesn't offer a value
    // that reliably maps to Roaming without the "<org>/<app>" suffix, so on
    // Windows we read %APPDATA% directly. Other OSes get the standard
    // XDG / Library path with a "Ua" subfolder.
#if defined(Q_OS_WIN)
    const QString roaming = qEnvironmentVariable("APPDATA");
    if (!roaming.isEmpty())
        return QDir::cleanPath(roaming + QStringLiteral("/Ua"));
#endif
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    return QDir::cleanPath(base + QStringLiteral("/Ua"));
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

// A tool is "detected" when its root config dir contains real content —
// not just empty skills/rules subdirs we may have created ourselves. Previously
// toolDetected returned true for any existing parent directory, which flagged
// an empty ~/.grok (left behind by a prior bug) as "Grok installed".
//
// Heuristic: resolve the tool's root (first path segment under $HOME),
// then look for at least one non-dot file or any file beyond the two
// directory names Ua itself creates (rules/, skills/).
static bool rootHasRealContent(const QString& root)
{
    QDir d(root);
    if (!d.exists()) return false;
    const auto entries = d.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& fi : entries) {
        // Any plain file (config/db/log the tool itself writes) → real install
        if (fi.isFile()) return true;
        // Any directory other than the two we might create ourselves
        const QString name = fi.fileName();
        if (name != QLatin1String("skills") && name != QLatin1String("rules")) {
            return true;
        }
        // skills/ or rules/ counts only if non-empty with real content
        QDir sub(fi.absoluteFilePath());
        const auto subEntries = sub.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        for (const QFileInfo& se : subEntries) {
            // A SKILL.md created by us is a tell-tale; ignore it. Everything
            // else (vendor's own rules, pre-existing user skills) counts.
            if (se.fileName() != QLatin1String("SKILL.md")) return true;
        }
    }
    return false;
}

bool toolDetected(QString relative)
{
    if (relative.isEmpty()) return false;

    // Resolve the tool's root dir (first segment of relative path).
    QString rel = relative;
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int slash = rel.indexOf(QLatin1Char('/'));
    const QString firstSeg = slash < 0 ? rel : rel.left(slash);
    const QString root = homeDir() + QLatin1Char('/') + firstSeg;

    // Special case for XDG-style paths (~/.config/<tool>/...): skip past
    // the shared .config and check the tool's own dir instead.
    if (firstSeg == QLatin1String(".config") && slash > 0) {
        const int slash2 = rel.indexOf(QLatin1Char('/'), slash + 1);
        if (slash2 > 0) {
            const QString xdgRoot = homeDir() + QLatin1Char('/') + rel.left(slash2);
            return rootHasRealContent(xdgRoot);
        }
    }

    return rootHasRealContent(root);
}

// Per-tool environment variable table. Keyed by Item::id. First non-empty env
// var wins. Mirrors ColdCoffee's definitions in detect-directory.js.
static QString firstEnv(const char* k1, const char* k2 = nullptr, const char* k3 = nullptr)
{
    for (const char* k : {k1, k2, k3}) {
        if (!k) continue;
        const QString v = qEnvironmentVariable(k).trimmed();
        if (!v.isEmpty()) return v;
    }
    return {};
}

static QString normalizedRoot(QString value)
{
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (value == QLatin1String("~")) value = homeDir();
    else if (value.startsWith(QLatin1String("~/"))) value = homeDir() + value.mid(1);
    return QDir::cleanPath(QFileInfo(value).absoluteFilePath());
}

static QString expandedVscodePath(QString value)
{
    value = value.trimmed();
    static const QRegularExpression variables(QStringLiteral("%([^%]+)%"));
    QString expanded;
    qsizetype offset = 0;
    auto matches = variables.globalMatch(value);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const QByteArray key = match.captured(1).toUtf8();
        const QString replacement = qEnvironmentVariable(key.constData());
        expanded += value.mid(offset, match.capturedStart() - offset);
        expanded += replacement.isEmpty() ? match.captured() : replacement;
        offset = match.capturedEnd();
    }
    expanded += value.mid(offset);
    expanded.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (expanded == QLatin1String("~")) expanded = homeDir();
    else if (expanded.startsWith(QLatin1String("~/"))) expanded = homeDir() + expanded.mid(1);
    return expanded;
}

enum class VscodePathKind { Path, Command, Icon };

// Directory paths keep spaces intact. Only command/icon registry values are
// parsed for arguments or an icon index; none of these values are executed.
static QString vscodeInstallFromValue(QString value, VscodePathKind kind = VscodePathKind::Path)
{
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('"'))) {
        const int end = value.indexOf(QLatin1Char('"'), 1);
        if (end < 0) return {};
        value = value.mid(1, end - 1);
    } else if (kind == VscodePathKind::Icon) {
        static const QRegularExpression iconIndex(QStringLiteral(",\\s*-?\\d+\\s*$"));
        value.remove(iconIndex);
    } else if (kind == VscodePathKind::Command) {
        static const QRegularExpression executableEnd(QStringLiteral("\\.exe(?=\\s|$)"),
                                                       QRegularExpression::CaseInsensitiveOption);
        const auto end = executableEnd.match(value);
        value = end.hasMatch() ? value.left(end.capturedEnd()) : QProcess::splitCommand(value).value(0);
    }
    value = expandedVscodePath(value);
    if (value.isEmpty() || !QDir::isAbsolutePath(value)) return {};

    QFileInfo candidate(value);
#if defined(Q_OS_MACOS)
    if (candidate.isDir() && candidate.fileName().endsWith(QLatin1String(".app")) &&
        QFileInfo(value + QStringLiteral("/Contents/MacOS/Electron")).isFile())
        return QDir::cleanPath(value);
#endif
    if (candidate.isDir()) {
#if defined(Q_OS_WIN)
        for (const QString& name : {QStringLiteral("Code.exe"), QStringLiteral("Code - Insiders.exe")}) {
            if (QFileInfo(QDir::cleanPath(candidate.absoluteFilePath() + QLatin1Char('/') + name)).isFile())
                return candidate.absoluteFilePath();
        }
#else
        for (const QString& name : {QStringLiteral("code"), QStringLiteral("code-insiders")}) {
            const QFileInfo executable(value + QLatin1Char('/') + name);
            if (executable.isFile() && executable.isExecutable()) return candidate.absoluteFilePath();
        }
#endif
        return {};
    }

    const QString name = candidate.fileName();
#if defined(Q_OS_MACOS)
    if (candidate.isFile() && name == QLatin1String("Electron") &&
        candidate.absolutePath().endsWith(QLatin1String("/Contents/MacOS"))) {
        const QString bundle = QDir::cleanPath(candidate.absolutePath() + QStringLiteral("/../.."));
        if (QFileInfo(bundle).fileName().endsWith(QLatin1String(".app"))) return bundle;
    }
#endif
    if (name.compare(QLatin1String("code.cmd"), Qt::CaseInsensitive) == 0 ||
        name.compare(QLatin1String("code-insiders.cmd"), Qt::CaseInsensitive) == 0
#if defined(Q_OS_WIN)
        || name.compare(QLatin1String("code"), Qt::CaseInsensitive) == 0
        || name.compare(QLatin1String("code-insiders"), Qt::CaseInsensitive) == 0
#endif
        ) {
        if (!candidate.isFile()) return {};
        const QString install = QFileInfo(QDir::cleanPath(candidate.absolutePath() + QStringLiteral("/..")))
            .absoluteFilePath();
        const QString exe = name.startsWith(QLatin1String("code-insiders"), Qt::CaseInsensitive)
            ? QStringLiteral("Code - Insiders.exe") : QStringLiteral("Code.exe");
        if (QFileInfo(QDir::cleanPath(install + QLatin1Char('/') + exe)).isFile()) return install;
        return candidate.absolutePath();
    }
    if (candidate.isFile() &&
        (name.compare(QLatin1String("Code.exe"), Qt::CaseInsensitive) == 0 ||
         name.compare(QLatin1String("Code - Insiders.exe"), Qt::CaseInsensitive) == 0 ||
         name.compare(QLatin1String("code"), Qt::CaseInsensitive) == 0 ||
         name.compare(QLatin1String("code-insiders"), Qt::CaseInsensitive) == 0)
#if !defined(Q_OS_WIN)
        && candidate.isExecutable()
#endif
        )
        return candidate.absolutePath();
    return {};
}

bool setVscodeInstallPath(const QString& path)
{
    const QString saved = configDir() + QStringLiteral("/vscode-install-path.txt");
    if (path.trimmed().isEmpty()) return !QFileInfo::exists(saved) || QFile::remove(saved);
    const QString install = vscodeInstallFromValue(path);
    if (install.isEmpty() || !QDir().mkpath(configDir())) return false;
    QSaveFile file(saved);
    const QByteArray bytes = install.toUtf8() + '\n';
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

#if defined(Q_OS_WIN)
// Installer registry values are paths or quoted commands, never commands to run.
static QString vscodeRegisteredInstallDir()
{
    const QStringList subkeys = {
        QStringLiteral("Microsoft/Windows/CurrentVersion/App Paths/code.exe"),
        QStringLiteral("Microsoft/Windows/CurrentVersion/App Paths/code-insiders.exe"),
        QStringLiteral("Classes/Applications/Code.exe/shell/open/command"),
        QStringLiteral("Classes/Applications/Code - Insiders.exe/shell/open/command"),
        QStringLiteral("Classes/vscode/shell/open/command"),
        QStringLiteral("Classes/vscode-insiders/shell/open/command"),
    };
    for (const auto format : {QSettings::Registry64Format, QSettings::Registry32Format}) {
        for (const auto& hive : {QStringLiteral("HKEY_CURRENT_USER/Software/"),
                                 QStringLiteral("HKEY_LOCAL_MACHINE/Software/")}) {
            for (const QString& subkey : subkeys) {
                QSettings settings(hive + subkey, format);
                const QString install = vscodeInstallFromValue(settings.value(QStringLiteral(".")).toString(),
                    subkey.endsWith(QLatin1String("/command")) ? VscodePathKind::Command : VscodePathKind::Path);
                if (!install.isEmpty()) return install;
            }
        }
    }
    return {};
}

static QString vscodeUninstallInstallDir()
{
    const QStringList roots = {
        QStringLiteral("HKEY_CURRENT_USER/Software/Microsoft/Windows/CurrentVersion/Uninstall"),
        QStringLiteral("HKEY_LOCAL_MACHINE/Software/Microsoft/Windows/CurrentVersion/Uninstall")
    };
    for (const auto format : {QSettings::Registry64Format, QSettings::Registry32Format}) {
        for (const QString& root : roots) {
            QSettings uninstall(root, format);
            for (const QString& group : uninstall.childGroups()) {
                QSettings entry(root + QLatin1Char('/') + group, format);
                const QString display = entry.value(QStringLiteral("DisplayName")).toString();
                static const QRegularExpression productName(
                    QStringLiteral("^(?:Microsoft )?Visual Studio Code(?: - Insiders| Insiders)?(?: \\((?:User|System)\\))?$"),
                    QRegularExpression::CaseInsensitiveOption);
                if (!productName.match(display.trimmed()).hasMatch()) continue;
                const QString install = vscodeInstallFromValue(entry.value(QStringLiteral("InstallLocation")).toString());
                if (!install.isEmpty()) return install;
                const QString icon = vscodeInstallFromValue(entry.value(QStringLiteral("DisplayIcon")).toString(),
                                                             VscodePathKind::Icon);
                if (!icon.isEmpty()) return icon;
            }
        }
    }
    return {};
}
#endif

QString vscodeInstallDir()
{
    QFile saved(configDir() + QStringLiteral("/vscode-install-path.txt"));
    if (saved.open(QIODevice::ReadOnly) && saved.size() <= 16384) {
        const QString install = vscodeInstallFromValue(QString::fromUtf8(saved.readAll()));
        if (!install.isEmpty()) return install;
    }
    const QString override = qEnvironmentVariable("VSCODE_INSTALL_DIR").trimmed();
    if (!override.isEmpty()) {
        const QString install = vscodeInstallFromValue(override);
        if (!install.isEmpty()) return install;
    }

    const QStringList pathDirs = qEnvironmentVariable("PATH").split(
        QDir::listSeparator(), Qt::SkipEmptyParts);
    for (QString dir : pathDirs) {
        dir = dir.trimmed();
        if (dir.startsWith(QLatin1Char('"')) && dir.endsWith(QLatin1Char('"')))
            dir = dir.mid(1, dir.size() - 2);
        if (dir.isEmpty()) continue;
        for (const auto& name : {QStringLiteral("code"), QStringLiteral("code-insiders"),
                                 QStringLiteral("Code.exe"), QStringLiteral("Code - Insiders.exe")}) {
            QString install = vscodeInstallFromValue(QDir::cleanPath(dir + QLatin1Char('/') + name));
            if (!install.isEmpty()) return install;
#if defined(Q_OS_WIN)
            for (const auto& suffix : {QStringLiteral(".exe"), QStringLiteral(".cmd")}) {
                install = vscodeInstallFromValue(QDir::cleanPath(dir + QLatin1Char('/') + name + suffix));
                if (!install.isEmpty()) return install;
            }
#endif
        }
    }

#if defined(Q_OS_WIN)
    QStringList roots;
    for (const char* key : {"LOCALAPPDATA", "ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"}) {
        QString base = qEnvironmentVariable(key).trimmed();
        if (base.isEmpty()) continue;
        if (qstrcmp(key, "LOCALAPPDATA") == 0) base += QStringLiteral("/Programs");
        roots << base + QStringLiteral("/Microsoft VS Code")
              << base + QStringLiteral("/Microsoft VS Code Insiders");
    }
#elif defined(Q_OS_MACOS)
    const QStringList roots = {
        QStringLiteral("/Applications/Visual Studio Code.app"),
        QStringLiteral("/Applications/Visual Studio Code - Insiders.app"),
        homeDir() + QStringLiteral("/Applications/Visual Studio Code.app"),
        homeDir() + QStringLiteral("/Applications/Visual Studio Code - Insiders.app"),
    };
#else
    const QStringList roots = {
        QStringLiteral("/usr/bin/code"), QStringLiteral("/usr/bin/code-insiders"),
        QStringLiteral("/usr/share/code/code"), QStringLiteral("/usr/share/code-insiders/code-insiders"),
        QStringLiteral("/opt/visual-studio-code/code"), QStringLiteral("/snap/bin/code"),
        QStringLiteral("/snap/bin/code-insiders"),
    };
#endif
    for (const QString& root : roots) {
#if defined(Q_OS_MACOS)
        if (QFileInfo(root + QStringLiteral("/Contents/MacOS/Electron")).isFile()) return root;
#else
        const QString install = vscodeInstallFromValue(root);
        if (!install.isEmpty()) return install;
#endif
    }
#if defined(Q_OS_WIN)
    if (const QString install = vscodeRegisteredInstallDir(); !install.isEmpty()) return install;
    if (const QString install = vscodeUninstallInstallDir(); !install.isEmpty()) return install;
    for (const QString& executable : ua::platform::vscodeRunningExecutables()) {
        if (const QString install = vscodeInstallFromValue(executable); !install.isEmpty()) return install;
    }
    for (const QString& target : ua::platform::vscodeShortcutTargets()) {
        if (const QString install = vscodeInstallFromValue(target); !install.isEmpty()) return install;
    }
#endif
    return {};
}

static bool vscodeExecutableDetected()
{
    return !vscodeInstallDir().isEmpty();
}

static QString defaultRootRelative(const Item& item)
{
    if (item.id == QLatin1String("windsurf")) return QStringLiteral(".codeium/windsurf");
    if (item.id == QLatin1String("cline")) return QStringLiteral("Documents/Cline");
    QString rel = item.targetRel;
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QStringList parts = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return {};
    if (parts.first() == QLatin1String(".config") && parts.size() > 1)
        return parts.first() + QLatin1Char('/') + parts[1];
    return parts.first();
}

QString toolHomeOverride(const QString& itemId)
{
    // Doubao: real data root is %LOCALAPPDATA%\Doubao\User Data\Default, not ~/.doubao.
    // DOUBAO_USER_SKILLS, if set, points directly at the skills folder.
    if (itemId == QLatin1String("doubao")) {
        const QString skills = qEnvironmentVariable("DOUBAO_USER_SKILLS").trimmed();
        if (!skills.isEmpty()) return normalizedRoot(skills);
        const QString local = qEnvironmentVariable("LOCALAPPDATA").trimmed();
        if (local.isEmpty()) return {};
        return QDir::cleanPath(local + QStringLiteral("/Doubao/User Data/Default"));
    }

    QString v;
    if      (itemId == QLatin1String("codex"))      v = firstEnv("CODEX_HOME", "CODEX_DIR");
    else if (itemId == QLatin1String("claude"))     v = firstEnv("CLAUDE_CONFIG_DIR", "CLAUDE_HOME");
    else if (itemId == QLatin1String("grok"))       v = firstEnv("GROK_HOME", "GROK_DIR");
    else if (itemId == QLatin1String("deepseek"))   v = firstEnv("DSH_HOME");
    else if (itemId == QLatin1String("gemini"))     v = firstEnv("GEMINI_HOME", "GEMINI_DIR");
    else if (itemId == QLatin1String("qwen"))       v = firstEnv("QWEN_HOME");
    else if (itemId == QLatin1String("trae"))       v = firstEnv("TRAE_HOME");
    else if (itemId == QLatin1String("codebuddy"))  v = firstEnv("CODEBUDDY_HOME");
    else if (itemId == QLatin1String("opencode"))   v = firstEnv("OPENCODE_HOME");
    else if (itemId == QLatin1String("windsurf"))   v = firstEnv("DEVIN_HOME", "WINDSURF_HOME");
    else if (itemId == QLatin1String("cursor"))     v = firstEnv("CURSOR_HOME");
    else if (itemId == QLatin1String("workbuddy"))  v = firstEnv("WORKBUDDY_CONFIG_DIR", "WORKBUDDY_HOME");
    else if (itemId == QLatin1String("cline"))      v = firstEnv("CLINE_HOME");
    else if (itemId == QLatin1String("copilot"))    v = firstEnv("COPILOT_HOME");
    else if (itemId == QLatin1String("vscode"))     v = firstEnv("VSCODE_COPILOT_HOME");
    else if (itemId == QLatin1String("zcode"))      v = firstEnv("ZCODE_HOME", "GLM_HOME", "ZHIPU_HOME");
    else if (itemId == QLatin1String("minimax"))    v = firstEnv("MIMOCODE_HOME");
    else if (itemId == QLatin1String("kimi"))       v = firstEnv("KIMI_CODE_HOME");

    if (v.isEmpty()) return {};
    return normalizedRoot(v);
}

QString toolConfigRoot(const QString& itemId)
{
    const QString override = toolHomeOverride(itemId);
    if (!override.isEmpty()) return override;
    const Item* item = itemById(itemId);
    if (!item) return {};
    const QString root = defaultRootRelative(*item);
    if (root.isEmpty()) return {};
    if (itemId == QLatin1String("workbuddy")) {
        const QString current = joinHome(root);
        const QString legacy = joinHome(QStringLiteral(".workbuddy-ai"));
        if (!QFileInfo(current).isDir() && QFileInfo(legacy).isDir()) return legacy;
    }
    return joinHome(root);
}

QString toolPath(const QString& itemId, const QString& relative)
{
    if (relative.isEmpty()) return {};
    const Item* item = itemById(itemId);
    if (!item) return {};
    QString rel = relative;
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QString prefix = defaultRootRelative(*item);
    const QString root = toolConfigRoot(itemId);
    if (rel == prefix) return root;
    if (!rel.startsWith(prefix + QLatin1Char('/'))) return joinHome(rel);
    return QDir::cleanPath(root + QLatin1Char('/') + rel.mid(prefix.size() + 1));
}

bool toolDetectedById(const QString& itemId)
{
    // Doubao: real install under %LOCALAPPDATA%\Doubao\..., not ~/.doubao.
    // ~/.doubao can be a stray empty dir created by earlier bugs; it does not
    // mean the Doubao client is actually installed.
    if (itemId == QLatin1String("doubao")) {
        const QString root = toolHomeOverride(QStringLiteral("doubao"));
        if (root.isEmpty()) return false;
        return QFileInfo(root).isDir();
    }

    const Item* it = itemById(itemId);
    if (!it) return false;

    if (itemId == QLatin1String("vscode")) {
        // VS Code reads user-level Copilot instructions from ~/.copilot even
        // before the first instruction file exists, so detect the editor
        // independently of that directory.
        const QString override = toolHomeOverride(itemId);
        if (!override.isEmpty()) return rootHasRealContent(override) || vscodeExecutableDetected();
        const QString instructions = QDir::cleanPath(
            toolConfigRoot(itemId) + QStringLiteral("/instructions"));
        return rootHasRealContent(instructions) || vscodeExecutableDetected();
    }

    // Env-var override takes priority: if a user set CODEX_HOME etc., that's
    // where their tool lives, so detection must look there.
    return rootHasRealContent(toolConfigRoot(itemId));
}

} // namespace ua::core
