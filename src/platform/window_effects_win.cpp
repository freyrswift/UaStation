// window_effects_win.cpp - DWM Mica + rounded corners on Windows 11

#include "autostart.h"

#include <QWindow>
#include <QOperatingSystemVersion>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace ua::platform {

namespace {

// Values documented at docs.microsoft.com/windows/win32/api/dwmapi
constexpr DWORD kAttrCornerPref        = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD kAttrBackdropType      = 38; // DWMWA_SYSTEMBACKDROP_TYPE
constexpr DWORD kAttrUseImmersiveDark  = 20; // DWMWA_USE_IMMERSIVE_DARK_MODE
constexpr DWORD kCornerRound           = 2;  // DWMWCP_ROUND
constexpr DWORD kBackdropMica          = 2;  // DWMSBT_MAINWINDOW

bool isWindows11()
{
    const auto v = QOperatingSystemVersion::current();
    return v.type() == QOperatingSystemVersion::Windows &&
           v.majorVersion() >= 10 && v.microVersion() >= 22000;
}

} // namespace

void applyWindowEffects(QWindow* window, bool micaEnabled)
{
    if (!window) return;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) return;

    // Rounded corners on Win11 (no-op elsewhere).
    DWORD corner = kCornerRound;
    DwmSetWindowAttribute(hwnd, kAttrCornerPref, &corner, sizeof(corner));

    if (!(micaEnabled && isWindows11())) return;

    DWORD backdrop = kBackdropMica;
    DwmSetWindowAttribute(hwnd, kAttrBackdropType, &backdrop, sizeof(backdrop));

    // Mica needs the frame extended into the client area so DWM draws it.
    MARGINS m{ -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &m);
}

} // namespace ua::platform
