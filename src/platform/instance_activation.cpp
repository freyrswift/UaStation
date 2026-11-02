#include "instance_activation.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>
#include <utility>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ua::platform {

QString InstanceActivation::serverName(const QString& key)
{
    const auto identity = (key + '\n' + QDir::homePath()).toUtf8();
    return QStringLiteral("ua-") + QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(32));
}

InstanceActivation::InstanceActivation(const QString& key, QObject* parent)
    : QObject(parent), m_name(serverName(key))
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (auto* socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(5000, socket, [socket] { socket->abort(); socket->deleteLater(); });
            socket->write(QByteArray::number(QCoreApplication::applicationPid()) + '\n');
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                if (socket->bytesAvailable() > 64) { socket->abort(); return; }
                if (!socket->canReadLine()) return;
                if (socket->readLine() != "activate\n") { socket->abort(); return; }
                if (m_handler) m_handler();
                else m_pending = true;
                socket->write("ok\n");
                socket->disconnectFromServer();
            });
        }
    });
}

bool InstanceActivation::listen()
{
    // Only the process holding the platform's single-instance lock calls this.
    QLocalServer::removeServer(m_name);
    return m_server.listen(m_name);
}

void InstanceActivation::setHandler(std::function<void()> handler)
{
    m_handler = std::move(handler);
    if (m_pending && m_handler) { m_pending = false; m_handler(); }
}

bool InstanceActivation::notify(const QString& key, int timeoutMs)
{
    QElapsedTimer elapsed;
    elapsed.start();
    QLocalSocket socket;
    while (elapsed.elapsed() < timeoutMs) {
        socket.connectToServer(serverName(key));
        const int remaining = timeoutMs - int(elapsed.elapsed());
        if (remaining <= 0) return false;
        if (socket.waitForConnected(qMin(200, remaining))) break;
        socket.abort();
        QThread::msleep(25);
    }
    if (socket.state() != QLocalSocket::ConnectedState) return false;
    auto readLine = [&]() -> QByteArray {
        while (!socket.canReadLine()) {
            const int remaining = timeoutMs - int(elapsed.elapsed());
            if (remaining <= 0 || !socket.waitForReadyRead(remaining)) return {};
            if (socket.bytesAvailable() > 64) return {};
        }
        return socket.readLine();
    };
    bool valid = false;
    const auto pid = readLine().trimmed().toULongLong(&valid);
    if (!valid || !pid) return false;
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(static_cast<DWORD>(pid));
#endif
    socket.write("activate\n");
    socket.flush();
    return readLine() == "ok\n";
}

} // namespace ua::platform
