// injector.h - Prompt injection + backup / state management
//
// Ported from the old inject.cpp; same marker scheme so an exe-written config
// from the Win32 version remains compatible with a Qt-built one and vice versa.

#pragma once

#include <QDateTime>
#include <QString>
#include <QByteArray>

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
    static InjectResult install(const QString& itemId, const QString& promptFile);
    static InjectResult uninstall(const QString& itemId);

    // State scan: inspects the target file + the state file to decide colour.
    static InstallState scan(const QString& itemId);

    // Does the given item have any skills state recorded?
    static bool  hasState(const QString& itemId);
    static bool  loadState(const QString& itemId, TargetState& out);
    static bool  saveState(const QString& itemId, const TargetState& state);
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
};

// Marker constants - kept binary-identical to the Win32 build so the two can
// read each other's files.
inline constexpr const char* kMarkerBegin = "<!-- UA-INJECT:BEGIN -->";
inline constexpr const char* kMarkerEnd   = "<!-- UA-INJECT:END -->";

} // namespace ua::core
