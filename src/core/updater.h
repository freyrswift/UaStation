// updater.h - GitHub release update checker + downloader

#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;
class QJSEngine;
QT_END_NAMESPACE

namespace ua::core {

class Updater : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool    checked          READ checked          NOTIFY stateChanged)
    Q_PROPERTY(bool    checking         READ checking         NOTIFY stateChanged)
    Q_PROPERTY(bool    available        READ available        NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion    READ latestVersion    NOTIFY stateChanged)
    Q_PROPERTY(bool    downloading      READ downloading      NOTIFY stateChanged)
    Q_PROPERTY(int     downloadProgress READ downloadProgress NOTIFY stateChanged)
    Q_PROPERTY(bool    ready            READ ready            NOTIFY stateChanged)

public:
    static Updater* instance();
    static Updater* create(QQmlEngine*, QJSEngine*) { return instance(); }

    bool    checked() const         { return m_checked; }
    bool    checking() const        { return m_checking; }
    bool    available() const;
    QString latestVersion() const   { return m_latest; }
    bool    downloading() const     { return m_downloading; }
    int     downloadProgress() const{ return m_progress; }
    bool    ready() const           { return !m_downloadedPath.isEmpty(); }

    Q_INVOKABLE void check();
    Q_INVOKABLE void download();     // Fetch the release binary into temp
    Q_INVOKABLE void applyUpdate();  // Swap-and-relaunch (Windows) / open file manager (Mac/Linux)
    Q_INVOKABLE void openReleasesPage();  // Fallback: open GitHub Releases in a browser

signals:
    void stateChanged();
    void downloadFailed(const QString& reason);

private:
    explicit Updater(QObject* parent = nullptr);

    QNetworkAccessManager* m_net = nullptr;
    bool    m_checked = false;
    bool    m_checking = false;
    bool    m_downloading = false;
    int     m_progress = 0;
    QString m_latest;
    QString m_downloadedPath;  // Local file after a successful download
};

} // namespace ua::core
