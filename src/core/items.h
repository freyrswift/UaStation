// items.h - Static data: AI tools + Tool-tab entries + embedded skills
//
// This is the ported counterpart of the old config.h kItems[] and kSkills[].
// All data is const; AppController wraps it into QML-friendly ListModels.

#pragma once

#include <QString>
#include <QVector>

namespace ua::core {

enum class ItemGroup : int {
    Editor = 0,
    Tools  = 1,
};

// Mirrors the old Item struct. `targetRel` is the POSIX-style path under $HOME
// (e.g. ".claude/CLAUDE.md"). An empty string means auto-install is not supported.
//
// `nameId` / `subId` reference core::Strings::StringId; when they are -1 the
// raw `name` / `sub` strings are used instead. Translated items set them so
// their labels follow the current UI language.
struct Item {
    QString id;             // Internal key, e.g. "claude" / "deepseek" / "skills-manager"
    QString name;           // Display name (brand names stay hard-coded)
    QString sub;            // Subtitle fallback when subId is unset
    ItemGroup group;
    QString logo;           // "qrc:/logos/claude.svg" or empty when glyph is used
    bool framed = false;    // Logo already has a tile background; skip card tint
    QString glyph;          // Fluent icon code point (hex string, e.g. "e8fd")
    QString url;            // Website / click-through URL
    QString targetRel;      // Config target relative to $HOME
    QString note;           // Shown when auto-install is unsupported
    QString skillsDirRel;   // Skills install dir relative to $HOME
    bool    isSkillsManager = false; // Special-case: opens Skills manager, not a URL
    int     nameId = -1;    // Strings::StringId; -1 keeps `name` as-is
    int     subId  = -1;    // Strings::StringId; -1 keeps `sub` as-is
};

enum class SkillCategory : int {
    All = 0, Reverse, Game, Kernel, Exploit,
    Crack, Mobile, Pentest, Ai, Other,
    _Count,
};

struct SkillDef {
    QString dirName;        // Folder under skills/, matches QRC prefix
    SkillCategory category;
};

const QVector<Item>&     allItems();
const QVector<SkillDef>& allSkills();

// Convenience: look up an item by its stable id.
const Item* itemById(const QString& id);

} // namespace ua::core
