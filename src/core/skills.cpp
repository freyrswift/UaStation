// skills.cpp - Install / uninstall embedded skill packs

#include "skills.h"
#include "items.h"
#include "paths.h"
#include "injector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace ua::core {

namespace {

QString skillsStateFileFor(const QString& itemId)
{
    return QDir::cleanPath(stateDir() + QLatin1Char('/') + itemId + QStringLiteral("-skills.dat"));
}

QStringList loadInstalledNames(const QString& itemId)
{
    QStringList out;
    bool ok = false;
    const QByteArray raw = Injector::readFile(skillsStateFileFor(itemId), &ok);
    if (!ok || raw.isEmpty()) return out;
    for (const QByteArray& line : raw.split('\n')) {
        const QString s = QString::fromUtf8(line).trimmed();
        if (!s.isEmpty()) out << s;
    }
    return out;
}

bool saveInstalledNames(const QString& itemId, const QStringList& names)
{
    QByteArray data;
    for (const QString& n : names) {
        data += n.toUtf8();
        data += '\n';
    }
    return Injector::writeAtomic(skillsStateFileFor(itemId), data);
}

// Copy :/skills/<dirName>/SKILL.md to <skillsRoot>/<dirName>/SKILL.md
bool extractSkill(const QString& dirName, const QString& skillsRoot)
{
    const QString res = QStringLiteral(":/skills/") + dirName + QStringLiteral("/SKILL.md");
    QFile in(res);
    if (!in.open(QIODevice::ReadOnly)) {
        qWarning() << "[skills] cannot open resource" << res;
        return false;
    }

    const QString outDir = QDir::cleanPath(skillsRoot + QLatin1Char('/') + dirName);
    if (!QDir().mkpath(outDir)) {
        qWarning() << "[skills] mkpath failed" << outDir;
        return false;
    }
    const QString outFile = QDir::cleanPath(outDir + QStringLiteral("/SKILL.md"));

    const QByteArray data = in.readAll();
    const bool ok = Injector::writeAtomic(outFile, data);
    if (!ok) qWarning() << "[skills] writeAtomic failed" << outFile << "data=" << data.size();
    return ok;
}

const Item* itemWithSkills(const QString& itemId)
{
    const Item* it = itemById(itemId);
    return (it && !it->skillsDirRel.isEmpty()) ? it : nullptr;
}

} // namespace

int Skills::installFor(const QString& itemId)
{
    const Item* it = itemWithSkills(itemId);
    if (!it) return 0;

    const QString root = skillsDir(it->skillsDirRel);
    QStringList installed;
    int count = 0;
    for (const SkillDef& sd : allSkills()) {
        if (extractSkill(sd.dirName, root)) {
            installed << sd.dirName;
            ++count;
        }
    }
    if (!installed.isEmpty()) saveInstalledNames(itemId, installed);
    return count;
}

int Skills::uninstallFor(const QString& itemId)
{
    const Item* it = itemWithSkills(itemId);
    if (!it) return 0;

    const QString root = skillsDir(it->skillsDirRel);
    const QStringList names = loadInstalledNames(itemId);
    int count = 0;
    for (const QString& name : names) {
        const QString sdir = QDir::cleanPath(root + QLatin1Char('/') + name);
        QFile::remove(sdir + QStringLiteral("/SKILL.md"));
        QDir(sdir).rmdir(QStringLiteral("."));
        ++count;
    }
    QFile::remove(skillsStateFileFor(itemId));
    return count;
}

int Skills::uninstallByName(const QString& skillDirName)
{
    if (skillDirName.isEmpty()) return 0;

    int toolCount = 0;
    for (const Item& it : allItems()) {
        if (it.skillsDirRel.isEmpty()) continue;
        QStringList names = loadInstalledNames(it.id);
        if (!names.contains(skillDirName)) continue;

        const QString root = skillsDir(it.skillsDirRel);
        const QString sdir = QDir::cleanPath(root + QLatin1Char('/') + skillDirName);
        QFile::remove(sdir + QStringLiteral("/SKILL.md"));
        QDir(sdir).rmdir(QStringLiteral("."));

        names.removeAll(skillDirName);
        if (names.isEmpty()) QFile::remove(skillsStateFileFor(it.id));
        else                 saveInstalledNames(it.id, names);
        ++toolCount;
    }
    return toolCount;
}

QVector<SkillStatus> Skills::statuses()
{
    QVector<SkillStatus> out;
    out.reserve(allSkills().size());
    for (const SkillDef& sd : allSkills()) {
        int count = 0;
        for (const Item& it : allItems()) {
            if (it.skillsDirRel.isEmpty()) continue;
            const QString file = QDir::cleanPath(
                skillsDir(it.skillsDirRel) + QLatin1Char('/') + sd.dirName + QStringLiteral("/SKILL.md"));
            if (QFileInfo::exists(file)) ++count;
        }
        out.push_back({sd.dirName, count, static_cast<int>(sd.category)});
    }
    return out;
}

} // namespace ua::core
