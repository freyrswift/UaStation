// prompts.cpp - Prompt discovery + embedded extraction

#include "prompts.h"
#include "paths.h"

#include <QDir>
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

} // namespace

void Prompts::extractEmbedded()
{
    const QString dir = promptsDir();
    QDir().mkpath(dir);

    for (const auto& ep : kEmbedded) {
        const QString dst = QDir::cleanPath(dir + QLatin1Char('/') + QLatin1String(ep.filename));
        if (QFileInfo::exists(dst)) continue; // Never overwrite user edits

        QFile in(QString::fromLatin1(ep.resource));
        if (!in.open(QIODevice::ReadOnly)) continue;
        QFile out(dst);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) continue;
        out.write(in.readAll());
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
