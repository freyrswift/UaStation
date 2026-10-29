// paths.h - Cross-platform path resolution for Ua
//
// Replaces the Win32 USERPROFILE / CSIDL_APPDATA code in the old inject.cpp.
// On every supported OS the AI tools (Claude Code, DeepSeek Harness, …) use
// `$HOME/.<tool>/<config>.md`, so most target-path logic is identical once
// $HOME is resolved via QStandardPaths.

#pragma once

#include <QString>

namespace ua::core {

// Ensures the writable per-user data dir exists; call once at startup.
void ensureUserDirs();

// Equivalents to the old HomeDir() / AppDataDir():
//   homeDir()         -> C:\Users\<name> / /Users/<name> / /home/<name>
//   configDir()       -> $APPDATA/Ua on Win, ~/Library/Application Support/Ua on Mac,
//                        $XDG_CONFIG_HOME/Ua on Linux (default ~/.config/Ua).
QString homeDir();
QString configDir();

// Writable prompts folder: <configDir>/prompts/
QString promptsDir();

// Per-tool state dir under configDir: <configDir>/state/
QString stateDir();

// Backups dir: <configDir>/backups/
QString backupsDir();

// Resolve the AI tool config target path, e.g. "~/.claude/CLAUDE.md".
// Takes the suffix as a POSIX-style string with "/" separators; the function
// does the platform-correct join. Empty when the tool has no target.
QString targetPath(QString relative);

// Resolve a skills install dir, e.g. "~/.claude/skills".
QString skillsDir(QString relative);

// Return true if the parent directory of `targetPath(relative)` exists, which
// is how we detect that the tool itself is installed. Mirrors ToolDetected().
bool toolDetected(QString relative);

// Resolve an environment-variable override for a given tool's root dir.
// Returns the resolved absolute path if any of the tool's env vars is set
// (e.g. CODEX_HOME, CLAUDE_CONFIG_DIR), otherwise an empty string. For
// Doubao, resolves DOUBAO_USER_SKILLS or the %LOCALAPPDATA%\Doubao\... root.
QString toolHomeOverride(const QString& itemId);

// Per-tool detection: honors env-var overrides and per-tool special cases
// (Doubao lives in %LOCALAPPDATA%\Doubao\..., not ~/.doubao). Falls back to
// the relative-path heuristic in toolDetected().
bool toolDetectedById(const QString& itemId);

} // namespace ua::core
