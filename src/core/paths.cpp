// paths.cpp - Cross-platform path resolution

#include "paths.h"

#include "items.h"

#include <QDir>
#include <QFileInfo>
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

static bool vscodeExecutableDetected()
{
    const QStringList pathDirs = qEnvironmentVariable("PATH").split(
        QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString& dir : pathDirs) {
        const QString base = QDir::cleanPath(dir + QStringLiteral("/code"));
        if (QFileInfo(base).isExecutable() || QFileInfo(base + QStringLiteral(".exe")).isExecutable())
            return true;
    }

#if defined(Q_OS_WIN)
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    const QString programFiles = qEnvironmentVariable("ProgramFiles");
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)");
    const QStringList roots = {
        local + QStringLiteral("/Programs/Microsoft VS Code/Code.exe"),
        local + QStringLiteral("/Programs/Microsoft VS Code Insiders/Code - Insiders.exe"),
        programFiles + QStringLiteral("/Microsoft VS Code/Code.exe"),
        programFiles + QStringLiteral("/Microsoft VS Code Insiders/Code - Insiders.exe"),
        programFilesX86 + QStringLiteral("/Microsoft VS Code/Code.exe"),
        programFilesX86 + QStringLiteral("/Microsoft VS Code Insiders/Code - Insiders.exe"),
    };
#elif defined(Q_OS_MACOS)
    const QStringList roots = {
        QStringLiteral("/Applications/Visual Studio Code.app"),
        QStringLiteral("/Applications/Visual Studio Code - Insiders.app"),
    };
#else
    const QStringList roots = {
        QStringLiteral("/usr/bin/code"),
        QStringLiteral("/usr/share/code/code"),
        QStringLiteral("/opt/visual-studio-code/code"),
    };
#endif

    for (const QString& root : roots) {
        if (!root.isEmpty() && QFileInfo::exists(root)) return true;
    }
    return false;
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
        if (!override.isEmpty()) return rootHasRealContent(override);
        const QString instructions = QDir::cleanPath(
            toolConfigRoot(itemId) + QStringLiteral("/instructions"));
        return rootHasRealContent(instructions) || vscodeExecutableDetected();
    }

    // Env-var override takes priority: if a user set CODEX_HOME etc., that's
    // where their tool lives, so detection must look there.
    return rootHasRealContent(toolConfigRoot(itemId));
}

} // namespace ua::core
