// settings.h - User settings persisted via QSettings

#pragma once

#include <QObject>
#include <QtQmlIntegration>

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QJSEngine;
QT_END_NAMESPACE

namespace ua::core {

class Settings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int  themeMode READ themeMode  WRITE setThemeMode  NOTIFY themeModeChanged)
    Q_PROPERTY(bool disclaimerAccepted READ disclaimerAccepted WRITE setDisclaimerAccepted NOTIFY disclaimerAcceptedChanged)

public:
    enum ThemeMode : int { System = 0, Dark = 1, Light = 2 };
    Q_ENUM(ThemeMode)

    static Settings* instance();
    static Settings* create(QQmlEngine*, QJSEngine*) { return instance(); }

    int  themeMode() const;
    void setThemeMode(int mode);

    bool disclaimerAccepted() const;
    void setDisclaimerAccepted(bool on);

    // System colour-scheme bridge (QStyleHints::colorScheme on Qt 6.5+).
    Q_INVOKABLE bool systemIsDark() const;

signals:
    void themeModeChanged();
    void disclaimerAcceptedChanged();

private:
    explicit Settings(QObject* parent = nullptr);
    int  m_themeMode = System;
    bool m_disclaimerAccepted = false;
};

} // namespace ua::core
