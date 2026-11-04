// skills.h - Install / uninstall embedded skill packs

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

namespace ua::core {

struct SkillStatus {
    QString dirName;
    int     installedCount; // Across how many AI tools
    int     category;       // SkillCategory enum value
    QStringList missingAssets; // Local references mentioned by SKILL.md but absent from the bundle
    bool needsRepair = false; // A deployed package is missing files or predates this bundle
};

class Skills
{
public:
    using ProgressCallback = std::function<void(int, int, const QString&)>;
    // itemId is a tool with a non-empty skillsDirRel.
    // Install every embedded package file, preserving its relative directory.
    static int installFor(const QString& itemId);

    // Same, into an explicit absolute dir (products that resolve their own
    // config folder, e.g. WorkBuddy CN vs international). Returns the names
    // actually written. On failure `failedSkill` receives the package that
    // could not be written so callers can name it in the UI.
    static QStringList installInto(const QString& itemId, const QString& absSkillsDir,
                                   bool* ok = nullptr, ProgressCallback progress = {},
                                   QString* failedSkill = nullptr);

    // Delete the given skill folders from an absolute dir.
    static int removeFrom(const QString& absSkillsDir, const QStringList& names);
    static int removeFrom(const QString& absSkillsDir, const QStringList& names,
                          const QString& itemId, bool* ok = nullptr,
                          QString* failedSkill = nullptr);

    // Last installed location, retained when an environment override changes.
    static QString installedRoot(const QString& itemId);
    static QStringList installedNames(const QString& itemId);
    static bool intact(const QString& itemId, const QStringList& expectedNames);
    static QStringList missingBundledAssets(const QString& skillDirName);
    static QStringList transactionFiles(const QString& itemId, const QString& root, const QStringList& names);

    // Names, descriptions and document locations from the shipped metadata.
    static QString buildIndex(const QString& absSkillsDir);
    static QString buildDiscoveryPrompt(const QString& absSkillsDir, bool includeNames = true);

    // Remove skills we previously installed (recorded in configDir/state/<id>-skills.dat).
    static int uninstallFor(const QString& itemId);

    // Remove this skill from every AI tool that has it (returns the tool count affected).
    static int uninstallByName(const QString& skillDirName);

    // Status table for the Skills Manager dialog.
    static QVector<SkillStatus> statuses();
};

} // namespace ua::core
