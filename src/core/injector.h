// injector.h - Prompt injection + backup / state management
//
// Ported from the old inject.cpp; same marker scheme so an exe-written config
// from the Win32 version remains compatible with a Qt-built one and vice versa.

#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVector>

#include <functional>

namespace ua::core {

enum class InstallState : int {
    NotInstalled   = 0,  // No marker, no file content
    Installed      = 1,  // Marker present (or legacy: state file + content)
    ActiveNoState  = 2,  // File has content but was not placed by us
};

struct TargetState {
    QString  backupFile;        // Full path of the pre-install backup
    bool     originalExisted = false;
    qint64   installedAt = 0;   // ms since epoch
};

// One written file for one item. A product may need several (WorkBuddy writes
// every memory archive plus MEMORY.md), so state is a list, not a single file.
struct TargetRecord {
    QString path;               // absolute path we wrote
    QString backup;             // absolute backup path (empty when it did not exist)
    bool    existed = false;
    int     kind = 0;           // ua::core::TargetKind
    QString patchStamp;         // SHA256 (first 16 hex) of file right after we wrote it;
                                // empty for Marked targets since user edits are allowed
};

struct ItemState {
    QVector<TargetRecord> targets;
    qint64      installedAt = 0;
    QStringList skills;         // skill folder names we installed
    QString     tomlFile;       // config.toml we patched (Codex)
    QString     tomlPriorLine;  // the line that was there before us
    bool        tomlHadLine = false;
};

struct InjectResult {
    bool    ok = false;
    QString error;              // Translated error key, filled on failure
};

class Injector
{
public:
    // High-level API matching the old Install/Uninstall calls.
    // itemId is the stable id from items.h (e.g. "claude" / "deepseek").
    // promptFile is the absolute path of the chosen prompt .md.
    //
    // Per-product dispatch happens inside install() via InstallPlan (see
    // install_plan.h): WorkBuddy writes SOUL.md + each memory archive +
    // MEMORY.md, Codex writes AGENTS.md + managed-prompts + config.toml
    // pointer, Cursor probes rule dirs, etc.
    static InjectResult install(const QString& itemId, const QString& promptFile);
    static InjectResult uninstall(const QString& itemId);

    // State scan: inspects the target file + the state file to decide colour.
    static InstallState scan(const QString& itemId);

    // Returns true if a previously-installed stamped target (PolicyReplace /
    // JsPatch / WholeFile / MemoryArchive) has drifted — i.e. the host app
    // upgraded itself and overwrote our patch, so a reinstall is needed.
    static bool hostUpgraded(const QString& itemId);

    // Does the given item have any skills state recorded?
    static bool  hasState(const QString& itemId);
    static bool  loadState(const QString& itemId, ItemState& out);
    static bool  saveState(const QString& itemId, const ItemState& state);
    static bool  deleteState(const QString& itemId);

    // File I/O helpers.
    static QByteArray readFile(const QString& path, bool* ok = nullptr);
    static bool       writeAtomic(const QString& path, const QByteArray& data);
    static bool       isUtf16(const QByteArray& bytes);
    static bool       stripBom(QByteArray& bytes);
    static bool       hasMarker(const QByteArray& text);

    // Backup helpers.
    static QString makeBackup(const QString& itemId, const QString& targetPath, bool fileExists);
    static bool    restoreBackup(const QString& targetPath, const TargetState& state);

    // Marker-block manipulation (public for testing).
    static QByteArray wrapMarked(const QByteArray& body);
    static QByteArray insertMarked(const QByteArray& previous, const QByteArray& marked);
    static QByteArray wipeMarked(const QByteArray& text);

    // Progress callback: fired at the start of every target in install().
    // Signature: (current, total, path). Install runs synchronously on the
    // caller's thread, so QML signals emitted from here only observe mid-flight
    // progress once the GUI thread is pumped (we currently log + toast around
    // the whole install -- see AppController).
    using ProgressCallback = std::function<void(int, int, const QString&)>;
    static void setProgressCallback(ProgressCallback cb);
};

// Marker constants - kept binary-identical to the Win32 build so the two can
// read each other's files.
inline constexpr const char* kMarkerBegin = "<!-- UA-INJECT:BEGIN -->";
inline constexpr const char* kMarkerEnd   = "<!-- UA-INJECT:END -->";

} // namespace ua::core
