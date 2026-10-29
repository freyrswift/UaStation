// paths.cpp - Cross-platform path resolution

#include "paths.h"

#include "items.h"

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

QString toolHomeOverride(const QString& itemId)
{
    // Doubao: real data root is %LOCALAPPDATA%\Doubao\User Data\Default, not ~/.doubao.
    // DOUBAO_USER_SKILLS, if set, points directly at the skills folder.
    if (itemId == QLatin1String("doubao")) {
        const QString skills = qEnvironmentVariable("DOUBAO_USER_SKILLS").trimmed();
        if (!skills.isEmpty()) return skills;
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
    else if (itemId == QLatin1String("workbuddy"))  v = firstEnv("WORKBUDDY_HOME");
    else if (itemId == QLatin1String("cline"))      v = firstEnv("CLINE_HOME");
    else if (itemId == QLatin1String("copilot"))    v = firstEnv("COPILOT_HOME");
    else if (itemId == QLatin1String("zcode"))      v = firstEnv("ZCODE_HOME", "GLM_HOME", "ZHIPU_HOME");
    else if (itemId == QLatin1String("minimax"))    v = firstEnv("MIMOCODE_HOME");
    else if (itemId == QLatin1String("kimi"))       v = firstEnv("KIMI_CODE_HOME");

    if (v.isEmpty()) return {};
    // Expand ~/ and normalize to absolute path
    if (v.startsWith(QLatin1String("~/")) || v.startsWith(QLatin1String("~\\"))) {
        v = homeDir() + QLatin1Char('/') + v.mid(2);
    }
    return QDir::cleanPath(v);
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

    // Env-var override takes priority: if a user set CODEX_HOME etc., that's
    // where their tool lives, so detection must look there.
    const QString envRoot = toolHomeOverride(itemId);
    if (!envRoot.isEmpty()) return rootHasRealContent(envRoot);

    return toolDetected(it->targetRel);
}

} // namespace ua::core
