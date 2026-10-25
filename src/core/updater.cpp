// updater.cpp - Query GitHub /releases/latest and compare semver

#include "updater.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>

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

    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api.github.com/repos/freyrswift/UaStation/releases/latest")));
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

} // namespace ua::core
