// Query a release, verify its package, then hand off to a detached installer.
#include "updater.h"
#include "update_installer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QtConcurrentRun>

namespace ua::core {
namespace {

bool writeAll(QFile* file, const QByteArray& bytes)
{
    qint64 offset = 0;
    while (offset < bytes.size()) {
        const qint64 written = file->write(bytes.constData() + offset, bytes.size() - offset);
        if (written <= 0) return false;
        offset += written;
    }
    return true;
}

QString stripVPrefix(QString s)
{
    if (s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V'))) s.remove(0, 1);
    return s;
}

int semverCompare(const QString& a, const QString& b)
{
    const auto aa = a.split(QLatin1Char('.'));
    const auto bb = b.split(QLatin1Char('.'));
    for (int i = 0; i < qMax(aa.size(), bb.size()); ++i) {
        const qulonglong ai = i < aa.size() ? aa[i].toULongLong() : 0;
        const qulonglong bi = i < bb.size() ? bb[i].toULongLong() : 0;
        if (ai != bi) return ai < bi ? -1 : 1;
    }
    return 0;
}

constexpr auto kReleasesPage = "https://github.com/freyrswift/UaStation/releases/latest";
constexpr auto kApiLatest = "https://api.github.com/repos/freyrswift/UaStation/releases/latest";
constexpr qint64 kMaxPackageSize = 1024LL * 1024 * 1024;

QString assetForPlatform(QStringView kernel, QStringView architecture)
{
    if (kernel == u"winnt" && architecture == u"x86_64")
        return QStringLiteral("Ua-windows-x64.zip");
    if (kernel == u"darwin" && architecture == u"arm64")
        return QStringLiteral("Ua-macos-arm64.dmg");
    if (kernel == u"darwin" && architecture == u"x86_64")
        return QStringLiteral("Ua-macos-x64.dmg");
    if (kernel == u"linux" && architecture == u"x86_64")
        return QStringLiteral("Ua-linux-x64.tar.gz");
    return {};
}

QString releaseAssetName()
{
    return assetForPlatform(QSysInfo::kernelType(), QSysInfo::buildCpuArchitecture());
}

} // namespace

Updater* Updater::instance()
{
    static Updater u;
    return &u;
}

Updater::Updater(QObject* parent) : QObject(parent), m_net(new QNetworkAccessManager(this)) {}
Updater::~Updater() = default;

bool Updater::available() const
{
    return !m_latest.isEmpty() && semverCompare(QCoreApplication::applicationVersion(), m_latest) < 0;
}

bool Updater::supportsAutoInstall() const
{
#if defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

void Updater::setOperationBusy(bool busy)
{
    if (m_operationBusy == busy) return;
    m_operationBusy = busy;
    emit stateChanged();
    if (!busy && m_pendingApply)
        QTimer::singleShot(0, this, &Updater::applyUpdate);
}

void Updater::startAutomaticUpdate()
{
    if (m_checking || m_downloading || m_applying) return;
    m_automatic = true;
    if (ready()) {
        m_error.clear();
        emit stateChanged();
        if (supportsAutoInstall()) QTimer::singleShot(0, this, &Updater::applyUpdate);
    } else {
        check();
    }
}

void Updater::check()
{
    if (m_checking || m_downloading || m_applying) return;
    const auto generation = ++m_generation;
    m_checking = true;
    m_checked = false;
    m_checkFailed = false;
    m_pendingApply = false;
    m_latest.clear();
    m_error.clear();
    m_assetUrl.clear();
    m_expectedHash.clear();
    m_expectedSize = 0;
    m_downloadedPath.clear();
    m_temp.reset();
    m_progress = 0;
    emit stateChanged();

    QNetworkRequest request(QUrl(QString::fromLatin1(kApiLatest)));
    request.setRawHeader("User-Agent", "Ua-Toolbox-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(30000);
    auto* reply = m_net->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_generation) return;
        m_reply.clear();
        m_checking = false;
        m_checked = true;
        QString failure;
        const auto document = QJsonDocument::fromJson(reply->readAll());
        const auto release = document.object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        const QString version = stripVPrefix(tag);
        static const QRegularExpression versionPattern(QStringLiteral("^[0-9]{1,9}(?:\\.[0-9]{1,9}){1,3}$"));
        if (reply->error() != QNetworkReply::NoError) {
            failure = reply->errorString();
        } else if (!document.isObject() || !versionPattern.match(version).hasMatch() ||
                   release.value(QStringLiteral("draft")).toBool() ||
                   release.value(QStringLiteral("prerelease")).toBool()) {
            failure = tr("Invalid release information");
        } else {
            m_latest = version;
            if (available()) {
                const QString name = releaseAssetName();
                const QUrl expected(QStringLiteral("https://github.com/freyrswift/UaStation/releases/download/%1/%2")
                                        .arg(tag, name));
                int matches = 0;
                for (const auto& value : release.value(QStringLiteral("assets")).toArray()) {
                    const auto asset = value.toObject();
                    if (asset.value(QStringLiteral("name")).toString() != name || name.isEmpty()) continue;
                    ++matches;
                    m_assetUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
                    const QString digest = asset.value(QStringLiteral("digest")).toString();
                    static const QRegularExpression sha256(QStringLiteral("^sha256:([a-fA-F0-9]{64})$"));
                    const auto match = sha256.match(digest);
                    m_expectedHash = match.hasMatch() ? match.captured(1).toLatin1().toLower() : QByteArray();
                    m_expectedSize = asset.value(QStringLiteral("size")).toInteger();
                }
                if (matches != 1 || m_assetUrl != expected || m_expectedHash.size() != 64 ||
                    m_expectedSize <= 0 || m_expectedSize > kMaxPackageSize) {
                    failure = tr("No verified update package is available for this platform and architecture");
                    m_assetUrl.clear();
                }
            }
        }
        m_checkFailed = !failure.isEmpty();
        m_error = failure;
        if (m_checkFailed) m_latest.clear();
        emit stateChanged();
        if (m_automatic && available() && !m_checkFailed)
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_generation && m_automatic) download();
            });
    });
}

void Updater::download()
{
    if (m_checking || m_downloading || m_applying || !available()) return;
    if (ready()) { applyUpdate(); return; }
    auto fail = [this](const QString& reason) {
        m_error = reason;
        emit stateChanged();
        emit downloadFailed(reason);
    };
    if (m_assetUrl.isEmpty() || m_expectedHash.size() != 64 || m_expectedSize <= 0) {
        fail(tr("Update package metadata is missing; check for updates again"));
        return;
    }
    m_temp = std::make_unique<QTemporaryDir>(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                                .filePath(QStringLiteral("Ua-download-XXXXXX")));
    if (!m_temp->isValid()) { fail(tr("Could not create the update download directory")); return; }
    const QString outPath = QDir(m_temp->path()).filePath(releaseAssetName());
    auto* file = new QFile(outPath, this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const auto reason = file->errorString();
        delete file;
        fail(reason);
        return;
    }
    const auto generation = ++m_generation;
    m_downloading = true;
    m_progress = 0;
    m_error.clear();
    m_writeError.clear();
    emit stateChanged();
    QNetworkRequest request(m_assetUrl);
    request.setRawHeader("User-Agent", "Ua-Toolbox-Updater");
    request.setTransferTimeout(30000);
    auto* reply = m_net->get(request);
    m_reply = reply;
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
    auto received = std::make_shared<qint64>(0);
    auto consume = [this, reply, file, hash, received] {
        const auto bytes = reply->readAll();
        *received += bytes.size();
        if (*received > m_expectedSize) {
            m_writeError = tr("Update package exceeds its advertised size");
        } else if (m_writeError.isEmpty() && !writeAll(file, bytes)) {
            m_writeError = file->errorString();
        }
        if (m_writeError.isEmpty()) hash->addData(bytes);
        return m_writeError.isEmpty();
    };
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, consume, generation] {
        if (generation == m_generation && !consume()) reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, generation](qint64 got, qint64) {
        if (generation != m_generation) return;
        m_progress = static_cast<int>(qBound(qint64(0), 100 * got / m_expectedSize, qint64(99)));
        emit stateChanged();
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, outPath, hash, received, consume, generation] {
        reply->deleteLater();
        if (generation != m_generation) {
            file->close();
            file->deleteLater();
            QFile::remove(outPath);
            QDir().rmdir(QFileInfo(outPath).absolutePath());
            return;
        }
        if (!consume() || !file->flush()) {
            if (m_writeError.isEmpty()) m_writeError = file->errorString();
        }
        file->close();
        file->deleteLater();
        m_reply.clear();
        m_downloading = false;
        QString failure = m_writeError;
        if (failure.isEmpty() && reply->error() != QNetworkReply::NoError) failure = reply->errorString();
        if (failure.isEmpty() && *received != m_expectedSize) failure = tr("Update package is incomplete");
        if (failure.isEmpty() && hash->result().toHex() != m_expectedHash)
            failure = tr("Update package SHA-256 verification failed");
        if (failure.isEmpty()) {
            m_downloadedPath = outPath;
            m_progress = 100;
        } else {
            QFile::remove(outPath);
            m_progress = 0;
            m_error = failure;
            emit downloadFailed(failure);
        }
        emit stateChanged();
        if (m_automatic && ready() && supportsAutoInstall()) {
            m_pendingApply = true;
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_generation && m_automatic) applyUpdate();
            });
        }
    });
}

void Updater::applyUpdate()
{
    if (!ready() || m_applying) return;
    if (!supportsAutoInstall()) {
        // Keep manual-install packages available after the current app exits.
        m_temp->setAutoRemove(false);
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_downloadedPath).absolutePath()));
        return;
    }
    m_pendingApply = true;
    if (m_operationBusy) { emit stateChanged(); return; }
    m_pendingApply = false;
    m_applying = true;
    m_error.clear();
    emit stateChanged();
    const auto archive = m_downloadedPath;
    const auto appDir = QCoreApplication::applicationDirPath();
    const auto executable = QCoreApplication::applicationFilePath();
    const auto pid = QCoreApplication::applicationPid();
    const auto sha256 = QString::fromLatin1(m_expectedHash);
    auto* watcher = new QFutureWatcher<update::Result>(this);
    connect(watcher, &QFutureWatcher<update::Result>::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (result.ok) {
            // The close guard must stay active until the helper is ready. exit()
            // ends the loop directly; quit() would be vetoed by that same guard.
            QCoreApplication::exit(0);
        } else {
            m_applying = false;
            m_error = result.error;
            if (!result.logPath.isEmpty()) m_error += QStringLiteral("\nLog: ") + result.logPath;
            emit stateChanged();
            emit downloadFailed(m_error);
        }
    });
    watcher->setFuture(QtConcurrent::run([archive, appDir, executable, pid, sha256] {
        return update::launchInstaller(archive, appDir, executable, pid, sha256);
    }));
}

void Updater::cancel()
{
    if (m_applying) return;
    ++m_generation;
    m_automatic = false;
    m_pendingApply = false;
    if (m_reply) m_reply->abort();
    m_reply.clear();
    m_checking = false;
    m_downloading = false;
    m_error.clear();
    m_progress = 0;
    const QString completedPath = m_downloadedPath;
    m_downloadedPath.clear();
    if (!completedPath.isEmpty()) {
        QFile::remove(completedPath);
        m_temp.reset();
    }
    // A cancelled reply can still hold its file open until its finished signal.
    // Its callback removes that file; the directory is removed on destruction.
    emit stateChanged();
}

void Updater::openReleasesPage()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kReleasesPage)));
}

} // namespace ua::core
