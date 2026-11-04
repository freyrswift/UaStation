// prompts.cpp - Prompt discovery + embedded extraction

#include "prompts.h"
#include "paths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace ua::core {

namespace {

// Resource list matching the qt_add_resources call in CMakeLists.
struct EmbeddedPrompt { const char* resource; const char* filename; };
constexpr EmbeddedPrompt kEmbedded[] = {
    { ":/prompts/Ua.md",        "Ua.md" },
    { ":/prompts/workbuddy.md", "workbuddy.md" },
    { ":/prompts/gemini.md",    "gemini.md" },
    { ":/prompts/kimi.md",      "kimi.md" },
};

bool writeBytes(const QString& path, const QByteArray& data)
{
    QSaveFile out(path);
    return out.open(QIODevice::WriteOnly) && out.write(data) == data.size() && out.commit();
}

QByteArray hashOf(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

// The sidecar records the bundled bytes we last deployed. If the local copy
// still matches it, the user never edited the file and a newer bundle may
// replace it; otherwise the local edit wins.
static void syncEmbedded(const QString& resourcePath, const QString& dst)
{
    QFile in(resourcePath);
    if (!in.open(QIODevice::ReadOnly)) return;
    const QByteArray data = in.readAll();
    if (data.isEmpty()) return;
    const QString sidecar = dst + QStringLiteral(".ua-src");
    const QString bundledHash = QString::fromLatin1(hashOf(data));
    QFile local(dst);
    if (!local.exists()) {
        if (writeBytes(dst, data)) writeBytes(sidecar, bundledHash.toUtf8());
        return;
    }
    if (!local.open(QIODevice::ReadOnly)) return;
    const QByteArray current = local.readAll();
    local.close();
    if (current == data) {
        writeBytes(sidecar, bundledHash.toUtf8());
        return;
    }
    QFile previous(sidecar);
    if (!previous.open(QIODevice::ReadOnly)) return;   // first run: keep the file
    const QString deployed = QString::fromUtf8(previous.readAll()).trimmed();
    if (deployed != QString::fromLatin1(hashOf(current))) return;  // user edit
    QFile::remove(dst + QStringLiteral(".bak"));
    QFile::copy(dst, dst + QStringLiteral(".bak"));
    if (writeBytes(dst, data)) writeBytes(sidecar, bundledHash.toUtf8());
}

} // namespace

void Prompts::extractEmbedded()
{
    const QString dir = promptsDir();
    QDir().mkpath(dir);

    // Top-level shared and tool-specific prompt files.
    for (const auto& ep : kEmbedded) {
        const QString dst = QDir::cleanPath(dir + QLatin1Char('/') + QLatin1String(ep.filename));
        syncEmbedded(QString::fromLatin1(ep.resource), dst);
    }

    // Leaf routing pack: router + normalizer + index + 80 LEAF-*.md.
    // Walk the qrc tree so new leaves added to resources.qrc are picked up
    // without touching this file.
    const QString leavesDir = dir + QStringLiteral("/leaves");
    QDir().mkpath(leavesDir);
    QDirIterator it(QStringLiteral(":/prompts/leaves"),
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString dst = QDir::cleanPath(leavesDir + QLatin1Char('/') + it.fileName());
        syncEmbedded(it.filePath(), dst);
    }
}

QVector<PromptEntry> Prompts::list()
{
    QVector<PromptEntry> out;
    QDir d(promptsDir());
    const auto files = d.entryInfoList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
    out.reserve(files.size());
    for (const QFileInfo& fi : files) {
        out.push_back({fi.fileName(), fi.absoluteFilePath()});
    }
    return out;
}

QVector<PromptEntry> Prompts::listFor(const QString& itemId)
{
    const auto all = list();
    if (all.isEmpty()) return {};

    const QString specific = itemId + QStringLiteral(".md");
    const QString uaMd     = QStringLiteral("Ua.md");

    // Prefer a tool-specific prompt without changing the shared fallback.
    for (const auto& e : all) {
        if (e.name.compare(specific, Qt::CaseInsensitive) == 0)
            return { e };
    }
    for (const auto& e : all) {
        if (e.name.compare(uaMd, Qt::CaseInsensitive) == 0)
            return { e };
    }
    return { all.first() };
}

QString Prompts::pickFor(const QString& itemId)
{
    const auto filtered = listFor(itemId);
    return filtered.isEmpty() ? QString() : filtered.first().fullPath;
}

bool Prompts::restoreBuiltin(const QString& name)
{
    if (name.isEmpty() || name.contains('/') || name.contains('\\') ||
        name == QLatin1String(".") || name == QLatin1String("..")) return false;
    QFile in(QStringLiteral(":/prompts/") + name);
    if (!in.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = in.readAll();
    if (data.isEmpty()) return false;
    const QString dir = promptsDir();
    if (!QDir().mkpath(dir)) return false;
    const QString dst = QDir::cleanPath(dir + QLatin1Char('/') + name);
    if (QFileInfo::exists(dst)) {
        QFile::remove(dst + QStringLiteral(".bak"));
        QFile::copy(dst, dst + QStringLiteral(".bak"));
    }
    if (!writeBytes(dst, data)) return false;
    writeBytes(dst + QStringLiteral(".ua-src"),
               QString::fromLatin1(hashOf(data)).toUtf8());
    return true;
}

} // namespace ua::core
