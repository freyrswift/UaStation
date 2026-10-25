// window_effects_mac.mm - NSVisualEffectView vibrancy for macOS
//
// Attaches an NSVisualEffectView behind the QWindow's content view, giving us
// a Mica-equivalent translucent backdrop that matches system dark/light mode.

#include "autostart.h"

#include <QWindow>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

namespace ua::platform {

void applyWindowEffects(QWindow* window, bool micaEnabled)
{
    if (!window || !micaEnabled) return;

    NSView* nsview = reinterpret_cast<NSView*>(window->winId());
    if (!nsview) return;
    NSWindow* nswin = [nsview window];
    if (!nswin) return;

    // Rounded, borderless-style window
    [nswin setStyleMask:([nswin styleMask] | NSWindowStyleMaskFullSizeContentView)];
    [nswin setTitlebarAppearsTransparent:YES];
    [nswin setMovableByWindowBackground:YES];
    [nswin setOpaque:NO];
    nswin.backgroundColor = [NSColor clearColor];

    NSVisualEffectView* fx = [[NSVisualEffectView alloc] initWithFrame:[nsview bounds]];
    fx.material = NSVisualEffectMaterialSidebar;
    fx.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    fx.state = NSVisualEffectStateFollowsWindowActiveState;
    fx.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    NSView* content = [nswin contentView];
    [content addSubview:fx positioned:NSWindowBelow relativeTo:nil];
}

} // namespace ua::platform
