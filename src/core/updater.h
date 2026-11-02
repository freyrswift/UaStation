// updater.h - GitHub release update checker + downloader

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QtQmlIntegration>
#include <memory>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;
class QJSEngine;
class QTemporaryDir;
QT_END_NAMESPACE

namespace ua::core {

class Updater : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool    checked          READ checked          NOTIFY stateChanged)
    Q_PROPERTY(bool    checking         READ checking         NOTIFY stateChanged)
    Q_PROPERTY(bool    checkFailed      READ checkFailed      NOTIFY stateChanged)
    Q_PROPERTY(bool    available        READ available        NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion    READ latestVersion    NOTIFY stateChanged)
    Q_PROPERTY(bool    downloading      READ downloading      NOTIFY stateChanged)
    Q_PROPERTY(int     downloadProgress READ downloadProgress NOTIFY stateChanged)
    Q_PROPERTY(bool    ready            READ ready            NOTIFY stateChanged)
    Q_PROPERTY(bool    automatic        READ automatic        NOTIFY stateChanged)
    Q_PROPERTY(bool    applying         READ applying         NOTIFY stateChanged)
    Q_PROPERTY(QString error            READ error            NOTIFY stateChanged)
    Q_PROPERTY(bool    operationBusy    READ operationBusy WRITE setOperationBusy NOTIFY stateChanged)
    Q_PROPERTY(bool    supportsAutoInstall READ supportsAutoInstall CONSTANT)

public:
    static Updater* instance();
    static Updater* create(QQmlEngine*, QJSEngine*) { return instance(); }
    ~Updater() override;

    bool    checked() const         { return m_checked; }
    bool    checking() const        { return m_checking; }
    bool    checkFailed() const     { return m_checkFailed; }
    bool    available() const;
    QString latestVersion() const   { return m_latest; }
    bool    downloading() const     { return m_downloading; }
    int     downloadProgress() const{ return m_progress; }
    bool    ready() const           { return !m_downloadedPath.isEmpty(); }
    bool    automatic() const       { return m_automatic; }
    bool    applying() const        { return m_applying; }
    QString error() const           { return m_error; }
    bool    operationBusy() const   { return m_operationBusy; }
    bool    supportsAutoInstall() const;
    void setOperationBusy(bool busy);

    Q_INVOKABLE void startAutomaticUpdate();
    Q_INVOKABLE void check();
    Q_INVOKABLE void download();     // Fetch the release binary into temp
    Q_INVOKABLE void applyUpdate();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openReleasesPage();  // Fallback: open GitHub Releases in a browser

signals:
    void stateChanged();
    void downloadFailed(const QString& reason);

private:
    explicit Updater(QObject* parent = nullptr);

    QNetworkAccessManager* m_net = nullptr;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QTemporaryDir> m_temp;
    bool    m_checked = false;
    bool    m_checking = false;
    bool    m_checkFailed = false;
    bool    m_downloading = false;
    bool    m_automatic = false;
    bool    m_applying = false;
    bool    m_operationBusy = false;
    bool    m_pendingApply = false;
    int     m_progress = 0;
    quint64 m_generation = 0;
    QString m_latest;
    QString m_downloadedPath;  // Local file after a successful download
    QString m_writeError;
    QString m_error;
    QUrl m_assetUrl;
    QByteArray m_expectedHash;
    qint64 m_expectedSize = 0;
};

} // namespace ua::core
