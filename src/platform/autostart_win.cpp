// autostart_win.cpp - Windows HKCU\Run autostart

#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace ua::platform {

namespace {
constexpr const wchar_t* kRunKey   = L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const char*    kValueKey = "UaToolbox";
} // namespace

bool autostartEnabled()
{
    QSettings run(QString::fromWCharArray(kRunKey), QSettings::NativeFormat);
    return run.contains(QString::fromLatin1(kValueKey));
}

bool setAutostartEnabled(bool on)
{
    QSettings run(QString::fromWCharArray(kRunKey), QSettings::NativeFormat);
    if (on) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(QString::fromLatin1(kValueKey), QStringLiteral("\"%1\"").arg(exe));
    } else {
        run.remove(QString::fromLatin1(kValueKey));
    }
    run.sync();
    return run.status() == QSettings::NoError;
}

} // namespace ua::platform
