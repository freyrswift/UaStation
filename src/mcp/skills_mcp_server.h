#pragma once

#include <QByteArray>
#include <QString>

namespace ua::mcp {

class SkillsMcpServer
{
public:
    static constexpr qsizetype MaxRequestBytes = 64 * 1024;
    static constexpr qsizetype MaxResponseBytes = 1024 * 1024;
    static constexpr qint64 MaxFileBytes = 4 * 1024 * 1024;

    explicit SkillsMcpServer(const QString& root);

    const QString& root() const { return m_root; }
    bool validateRoot(QString* error = nullptr) const;

    // One UTF-8 JSON-RPC message, without transport framing. Notifications
    // return an empty byte array; responses include their trailing newline.
    QByteArray processLine(const QByteArray& line);

private:
    QString m_root;
    bool m_initialized = false;
    bool m_ready = false;
};

} // namespace ua::mcp
