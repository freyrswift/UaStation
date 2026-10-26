// autostart.h - Cross-platform platform shim (single-instance + window effects)
//
// Filename is historical; this header no longer carries autostart hooks
// because the Settings UI doesn't expose that toggle. Keep the include
// path stable until a rename makes sense.

#pragma once

#include <QString>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

namespace ua::platform {

// Acquire single-instance lock. Returns false if another instance already runs.
bool acquireSingleInstance(const QString& key);

// Apply native backdrop effect (Mica on Win11, vibrancy on macOS, no-op on Linux).
void applyWindowEffects(QWindow* w, bool micaEnabled);

} // namespace ua::platform
