// skills.h - Install / uninstall embedded skill packs

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace ua::core {

struct SkillStatus {
    QString dirName;
    int     installedCount; // Across how many AI tools
    int     category;       // SkillCategory enum value
};

class Skills
{
public:
    // itemId is a tool with a non-empty skillsDirRel.
    // Install all embedded skills (:/skills/<name>/SKILL.md) into <home>/<skillsDirRel>/<name>/.
    static int installFor(const QString& itemId);

    // Same, into an explicit absolute dir (products that resolve their own
    // config folder, e.g. WorkBuddy CN vs international). Returns the names
    // actually written.
    static QStringList installInto(const QString& itemId, const QString& absSkillsDir);

    // Delete the given skill folders from an absolute dir.
    static int removeFrom(const QString& absSkillsDir, const QStringList& names);

    // Compact "name: what it is for" list of the shipped skills, for products
    // that do not advertise skills to the model themselves.
    static QString buildIndex(const QString& absSkillsDir);

    // Remove skills we previously installed (recorded in configDir/state/<id>-skills.dat).
    static int uninstallFor(const QString& itemId);

    // Remove this skill from every AI tool that has it (returns the tool count affected).
    static int uninstallByName(const QString& skillDirName);

    // Status table for the Skills Manager dialog.
    static QVector<SkillStatus> statuses();
};

} // namespace ua::core
