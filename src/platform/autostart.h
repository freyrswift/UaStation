// autostart.h - Cross-platform platform-shim API
//
// Despite the filename this covers more than autostart: single-instance and
// window backdrop effects live here too so there's one include per platform
// consumer. Implementations are in platform/<name>_<os>.cpp.

#pragma once

#include <QString>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

namespace ua::platform {

// Current state (checks HKCU\Run on Win, ~/Library/LaunchAgents on Mac,
// ~/.config/autostart/<app>.desktop on Linux).
bool autostartEnabled();

// Enable/disable. Returns true on success.
bool setAutostartEnabled(bool on);

// Acquire single-instance lock. Returns false if another instance already runs.
bool acquireSingleInstance(const QString& key);

// Apply native backdrop effect (Mica on Win11, vibrancy on macOS, no-op on Linux).
void applyWindowEffects(QWindow* w, bool micaEnabled);

} // namespace ua::platform
