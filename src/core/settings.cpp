// settings.cpp - QSettings-backed user state

#include "settings.h"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

namespace ua::core {

namespace {
constexpr const char* kThemeKey      = "ui/themeMode";
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
