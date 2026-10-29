// prompts.cpp - Prompt discovery + embedded extraction

#include "prompts.h"
#include "paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace ua::core {

namespace {

// Resource list matching the qt_add_resources call in CMakeLists.
struct EmbeddedPrompt { const char* resource; const char* filename; };
constexpr EmbeddedPrompt kEmbedded[] = {
    { ":/prompts/Ua.md",        "Ua.md" },
    { ":/prompts/workbuddy.md", "workbuddy.md" },
};

// Copy a single qrc file to disk, preserving user edits.
static void copyIfMissing(const QString& resourcePath, const QString& dst)
{
    if (QFileInfo::exists(dst)) return; // Never overwrite user edits
    QFile in(resourcePath);
    if (!in.open(QIODevice::ReadOnly)) return;
    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    out.write(in.readAll());
}

} // namespace

void Prompts::extractEmbedded()
{
    const QString dir = promptsDir();
    QDir().mkpath(dir);

    // Top-level prompt files (Ua.md, workbuddy.md).
    for (const auto& ep : kEmbedded) {
        const QString dst = QDir::cleanPath(dir + QLatin1Char('/') + QLatin1String(ep.filename));
        copyIfMissing(QString::fromLatin1(ep.resource), dst);
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
        copyIfMissing(it.filePath(), dst);
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

    // WorkBuddy uses workbuddy.md; everything else uses Ua.md.
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

} // namespace ua::core
