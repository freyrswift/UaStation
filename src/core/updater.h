// updater.h - GitHub release update checker

#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QQmlEngine;
class QJSEngine;
QT_END_NAMESPACE

namespace ua::core {

class Updater : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool    checked       READ checked       NOTIFY stateChanged)
    Q_PROPERTY(bool    checking      READ checking      NOTIFY stateChanged)
    Q_PROPERTY(bool    available     READ available     NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)

public:
    static Updater* instance();
    static Updater* create(QQmlEngine*, QJSEngine*) { return instance(); }

    bool    checked() const       { return m_checked; }
    bool    checking() const      { return m_checking; }
    bool    available() const;
    QString latestVersion() const { return m_latest; }

    Q_INVOKABLE void check();

signals:
    void stateChanged();

private:
    explicit Updater(QObject* parent = nullptr);
    void onReplyFinished();

    QNetworkAccessManager* m_net = nullptr;
    bool    m_checked = false;
    bool    m_checking = false;
    QString m_latest;
};

} // namespace ua::core
