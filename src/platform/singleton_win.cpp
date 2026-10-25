// singleton_win.cpp - Windows named mutex single-instance guard

#include "autostart.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ua::platform {

static HANDLE g_mutex = nullptr;

bool acquireSingleInstance(const QString& key)
{
    g_mutex = CreateMutexW(nullptr, TRUE, reinterpret_cast<LPCWSTR>(key.utf16()));
    if (!g_mutex) return true; // Degrade gracefully: allow run if mutex creation fails.
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return false;
    }
    return true;
}

} // namespace ua::platform
