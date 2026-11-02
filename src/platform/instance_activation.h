#pragma once

#include <QLocalServer>
#include <QObject>
#include <functional>

namespace ua::platform {

class InstanceActivation : public QObject {
public:
    explicit InstanceActivation(const QString& key, QObject* parent = nullptr);
    bool listen();
    void setHandler(std::function<void()> handler);
    static bool notify(const QString& key, int timeoutMs = 5000);

private:
    static QString serverName(const QString& key);
    QString m_name;
    QLocalServer m_server;
    std::function<void()> m_handler;
    bool m_pending = false;
};

} // namespace ua::platform
