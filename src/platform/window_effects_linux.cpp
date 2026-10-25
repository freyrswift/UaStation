// window_effects_linux.cpp - Linux has no cross-DE Mica equivalent; QML paints
// the rounded body itself so there's nothing to attach here.

#include "autostart.h"

#include <QWindow>

namespace ua::platform {

void applyWindowEffects(QWindow* /*window*/, bool /*micaEnabled*/)
{
    // Rounded corners + blur under GNOME/KDE would need WM-specific hints; our
    // QML renders a round-rect body directly, so this is intentionally empty.
}

} // namespace ua::platform
