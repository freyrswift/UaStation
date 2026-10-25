// skills.h - Install / uninstall embedded skill packs

#pragma once

#include <QString>
#include <QVector>

namespace ua::core {

struct SkillStatus {
    QString dirName;
    int     installedCount; // Across how many AI tools
};

class Skills
{
public:
    // itemId is a tool with a non-empty skillsDirRel.
    // Install all embedded skills (:/skills/<name>/SKILL.md) into <home>/<skillsDirRel>/<name>/.
    static int installFor(const QString& itemId);

    // Remove skills we previously installed (recorded in configDir/state/<id>-skills.dat).
    static int uninstallFor(const QString& itemId);

    // Remove this skill from every AI tool that has it (returns the tool count affected).
    static int uninstallByName(const QString& skillDirName);

    // Status table for the Skills Manager dialog.
    static QVector<SkillStatus> statuses();
};

} // namespace ua::core
