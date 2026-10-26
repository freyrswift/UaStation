// updater.cpp - Query GitHub /releases/latest, download, apply.

#include "updater.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOperatingSystemVersion>
#include <QStandardPaths>
#include <QUrl>

namespace ua::core {

namespace {

QString stripVPrefix(QString s)
{
    if (s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V'))) s.remove(0, 1);
    return s;
}

// Lexicographic semver compare on dotted integers.
int semverCompare(const QString& a, const QString& b)
{
    const auto aa = a.split(QLatin1Char('.'));
    const auto bb = b.split(QLatin1Char('.'));
    const int n = qMax(aa.size(), bb.size());
    for (int i = 0; i < n; ++i) {
        const int ai = i < aa.size() ? aa[i].toInt() : 0;
        const int bi = i < bb.size() ? bb[i].toInt() : 0;
        if (ai != bi) return ai < bi ? -1 : 1;
    }
    return 0;
}

constexpr const char* kReleaseAssetWin   = "Ua-windows-x64.zip";
constexpr const char* kReleaseAssetLinux = "Ua-linux-x64.tar.gz";
constexpr const char* kReleaseAssetMac   = "Ua-macos-arm64.dmg";
constexpr const char* kReleasesPage      = "https://github.com/freyrswift/UaStation/releases/latest";
constexpr const char* kApiLatest         = "https://api.github.com/repos/freyrswift/UaStation/releases/latest";

QString releaseAssetName()
{
    const auto t = QOperatingSystemVersion::current().type();
    if (t == QOperatingSystemVersion::Windows) return QString::fromLatin1(kReleaseAssetWin);
    if (t == QOperatingSystemVersion::MacOS)   return QString::fromLatin1(kReleaseAssetMac);
    return QString::fromLatin1(kReleaseAssetLinux);
}

} // namespace

Updater* Updater::instance()
{
    static Updater u;
    return &u;
}

Updater::Updater(QObject* parent) : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

bool Updater::available() const
{
    if (m_latest.isEmpty()) return false;
    return semverCompare(QCoreApplication::applicationVersion(), m_latest) < 0;
}

void Updater::check()
{
    if (m_checking) return;
    m_checking = true;
    m_checked  = false;
    m_latest.clear();
    emit stateChanged();

    QNetworkRequest req(QUrl(QString::fromLatin1(kApiLatest)));
    req.setRawHeader("User-Agent", "Ua-Toolbox-Updater");
    req.setRawHeader("Accept",     "application/vnd.github+json");

    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checking = false;
        m_checked  = true;

        if (reply->error() == QNetworkReply::NoError) {
            const auto doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isObject()) {
                m_latest = stripVPrefix(doc.object().value(QStringLiteral("tag_name")).toString());
            }
        }
        emit stateChanged();
    });
}

void Updater::download()
{
    if (m_downloading || !available()) return;

    const QString url = QStringLiteral("https://github.com/freyrswift/UaStation/releases/latest/download/%1")
                            .arg(releaseAssetName());
    const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString outPath = tmpDir + QLatin1Char('/') + releaseAssetName();

    QFile* f = new QFile(outPath, this);
    if (!f->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete f;
        emit downloadFailed(tr("无法写入临时文件 / cannot write temp file"));
        return;
    }

    m_downloading = true;
    m_progress    = 0;
    m_downloadedPath.clear();
    emit stateChanged();

    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("User-Agent", "Ua-Toolbox-Updater");
    // Qt 6 follows redirects by default; no attribute needed.

    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::readyRead, this, [reply, f]() {
        f->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 got, qint64 total) {
        if (total > 0) {
            m_progress = static_cast<int>(100 * got / total);
            emit stateChanged();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, f, outPath]() {
        reply->deleteLater();
        f->write(reply->readAll());
        f->close();
        f->deleteLater();

        m_downloading = false;
        if (reply->error() == QNetworkReply::NoError) {
            m_downloadedPath = outPath;
            m_progress = 100;
        } else {
            QFile::remove(outPath);
            m_progress = 0;
            emit downloadFailed(reply->errorString());
        }
        emit stateChanged();
    });
}

void Updater::applyUpdate()
{
    if (m_downloadedPath.isEmpty()) return;

    // We shipped a .zip/.tar.gz/.dmg archive, not a bare exe — the safest
    // cross-platform behaviour is to open the file manager at the download
    // location and let the user extract/mount the archive themselves. A real
    // in-place swap-and-relaunch flow needs code signing on Win/macOS and
    // package-manager integration on Linux; those are out of scope for v2.
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(m_downloadedPath).absolutePath()));
}

void Updater::openReleasesPage()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kReleasesPage)));
}

} // namespace ua::core
