// prompts.h - Prompt discovery + embedded extraction

#pragma once

#include <QString>
#include <QVector>

namespace ua::core {

struct PromptEntry {
    QString name;       // "Ua.md"
    QString fullPath;   // absolute path on disk
};

class Prompts
{
public:
    // Copy any missing embedded prompts (resources://prompts/*.md) into
    // promptsDir() so the user can edit them freely.
    static void extractEmbedded();

    // List all *.md files in promptsDir(), sorted by name.
    static QVector<PromptEntry> list();

    // List the single prompt file appropriate for a given tool id.
    // Prefer <itemId>.md (WorkBuddy, Gemini, Kimi), otherwise use Ua.md.
    static QVector<PromptEntry> listFor(const QString& itemId);

    // Resolve the preferred prompt for a given tool id: <id>.md > Ua.md > first.
    static QString pickFor(const QString& itemId);
};

} // namespace ua::core
