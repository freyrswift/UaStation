#pragma once

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QVector>

namespace ua::core::detail {

inline bool transactionWritable(const QString& path)
{
    if (!QFileInfo::exists(path)) return true;
    return QFile::setPermissions(path, QFileInfo(path).permissions() | QFile::WriteOwner | QFile::WriteUser);
}

// Stream into an atomic replacement so a failed copy preserves the destination.
inline bool safeCopy(const QString& src, const QString& dst)
{
    QFile source(src);
    if (!source.open(QIODevice::ReadOnly)) return false;
    const QFileInfo info(dst);
    if (!QDir().mkpath(info.absolutePath())) return false;
    if (!transactionWritable(dst)) return false;
    QSaveFile output(dst);
    if (!output.open(QIODevice::WriteOnly)) return false;
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(64 * 1024);
        if (source.error() != QFile::NoError || output.write(chunk) != chunk.size()) {
            output.cancelWriting();
            return false;
        }
    }
    return output.commit();
}

// Keep this operation's preimages, including reinstalls, separate from the
// persistent first-install backups used by uninstall.
class FileTransaction {
public:
    bool capture(const QString& path) {
        if (path.isEmpty()) return true;
        if (m_captured.contains(path)) return true;
        if (!m_temp.isValid()) return false;
        Entry entry;
        entry.path = path;
        const QFileInfo info(path);
        entry.existed = info.exists();
        entry.permissions = info.permissions();
        if (entry.existed) {
            if (!info.isFile() || info.isSymLink()) return false;
            entry.copy = m_temp.filePath(QString::number(m_entries.size()));
            if (!QFile::copy(path, entry.copy)) return false;
        }
        m_entries.push_back(entry);
        m_captured.insert(path);
        return true;
    }

    bool rollback() {
        bool ok = true;
        for (auto it = m_entries.crbegin(); it != m_entries.crend(); ++it) {
            if (it->existed) {
                if (!safeCopy(it->copy, it->path) ||
                    !QFile::setPermissions(it->path, it->permissions)) ok = false;
            } else if (QFileInfo::exists(it->path)) {
                transactionWritable(it->path);
                if (!QFile::remove(it->path)) ok = false;
            }
        }
        m_finished = true;
        return ok;
    }

    void commit() { m_finished = true; }
    ~FileTransaction() {
        if (!m_finished && !rollback()) qCritical() << "[inject] transaction rollback failed";
    }

private:
    struct Entry {
        QString path;
        QString copy;
        bool existed = false;
        QFile::Permissions permissions;
    };
    QTemporaryDir m_temp;
    QVector<Entry> m_entries;
    QSet<QString> m_captured;
    bool m_finished = false;
};

} // namespace ua::core::detail
