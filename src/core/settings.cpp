// settings.cpp - QSettings-backed user state

#include "settings.h"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

namespace ua::platform {
bool  autostartEnabled();
bool  setAutostartEnabled(bool on);
} // namespace ua::platform

namespace ua::core {

namespace {
constexpr const char* kThemeKey      = "ui/themeMode";
constexpr const char* kTopmostKey    = "ui/topmost";
constexpr const char* kDisclaimerKey = "ui/disclaimerAccepted";
} // namespace

Settings* Settings::instance()
{
    static Settings s;
    return &s;
}

Settings::Settings(QObject* parent) : QObject(parent)
{
    QSettings s;
    m_themeMode          = s.value(QString::fromLatin1(kThemeKey), System).toInt();
    m_topmost            = s.value(QString::fromLatin1(kTopmostKey), false).toBool();
    m_disclaimerAccepted = s.value(QString::fromLatin1(kDisclaimerKey), false).toBool();
}

int Settings::themeMode() const { return m_themeMode; }

void Settings::setThemeMode(int mode)
{
    if (mode < System || mode > Light) return;
    if (mode == m_themeMode) return;
    m_themeMode = mode;
    QSettings().setValue(QString::fromLatin1(kThemeKey), mode);
    emit themeModeChanged();
}

bool Settings::topmost() const { return m_topmost; }

void Settings::setTopmost(bool on)
{
    if (on == m_topmost) return;
    m_topmost = on;
    QSettings().setValue(QString::fromLatin1(kTopmostKey), on);
    emit topmostChanged();
}

bool Settings::autostart() const
{
    return ua::platform::autostartEnabled();
}

void Settings::setAutostart(bool on)
{
    if (on == autostart()) return;
    if (ua::platform::setAutostartEnabled(on)) emit autostartChanged();
}

bool Settings::disclaimerAccepted() const { return m_disclaimerAccepted; }

void Settings::setDisclaimerAccepted(bool on)
{
    if (on == m_disclaimerAccepted) return;
    m_disclaimerAccepted = on;
    QSettings().setValue(QString::fromLatin1(kDisclaimerKey), on);
    emit disclaimerAcceptedChanged();
}

bool Settings::systemIsDark() const
{
    const auto* hints = QGuiApplication::styleHints();
    if (!hints) return false;
    return hints->colorScheme() == Qt::ColorScheme::Dark;
}

} // namespace ua::core
