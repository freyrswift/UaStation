// install_plan.h - Per-product install profiles
//
// Every product stores its instructions differently, so the injector asks for a
// plan instead of assuming one file. A plan is: which absolute paths to write,
// how each one is shaped, and which side files (skills dir, settings.json,
// app-config.json) need adjusting afterwards.
//
// Paths are probed at install time (a product may live in several places: a
// portable build, a custom --user-data-dir, a CN vs international data folder).

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace ua::core {

struct Item;

enum class TargetKind : int {
    Marked = 0,       // Marker block inside an existing file (keeps user content)
    WholeFile = 1,    // Replace the whole file (memory files the agent reads as-is)
    MemoryArchive = 2,// WorkBuddy <uid>_memory.md profile document
    PolicyReplace = 3,// Replace <content_policy> block in a template or JSON file
    JsPatch = 4,      // Text-level search-and-replace patches on JS files
};

struct TargetSpec {
    QString    path;                 // absolute
    TargetKind kind = TargetKind::Marked;
};

struct InstallPlan {
    QVector<TargetSpec> targets;
    QString skillsDir;        // absolute; empty = product has no skills support
    QString skillsSettings;   // absolute settings.json whose skillOverrides we clear
    QString appConfig;        // absolute app-config.json whose memory flags we raise
    QString tomlFile;         // absolute config.toml carrying a model_instructions_file pointer
    QString pointerTarget;    // absolute file the pointer must reference
    bool    injectSkillIndex = false;  // product does not advertise skills itself
    int     maxChars = 0;     // per-target cap (0 = no cap)
    QStringList cacheFiles;   // runtime caches to delete after install/uninstall

    bool valid() const { return !targets.isEmpty(); }
};

// Build the plan for one item. Falls back to Item::targetRel when the product
// needs nothing product-specific.
InstallPlan planFor(const Item& item);

} // namespace ua::core
