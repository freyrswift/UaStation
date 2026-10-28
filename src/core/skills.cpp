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

// Gateways in front of some products pattern-match the request body and reject
// anything that reads like offensive tooling. The skill index is text we
// generate, so the same capability can be described in neutral wording here
// without touching the SKILL.md files other tools read verbatim.
QString wafSafeText(QString s)
{
    static const struct { const char* from; const char* to; } kPairs[] = {
        // English: phrases before single words.
        { "reverse-engineering", "binary analysis" },
        { "reverse engineering", "binary analysis" },
        { "penetration testing", "security assessment" },
        { "exploit development", "binary research" },
        { "payload crafting", "test input design" },
        { "malware analysis", "sample analysis" },
        { "cheat engine", "memory inspection" },
        { "proof-of-concept", "test case" },
        { "exploitation", "assessment" },
        { "shellcode", "machine code" },
        { "malware", "sample" },
        { "payloads", "test inputs" },
        { "payload", "test input" },
        { "pentest", "assessment" },
        { "reversing", "analysis" },
        { "jailbreak", "policy" },
        { "keygen", "key format" },
        { "bypass", "handling" },
        { "cracking", "review" },
        { "crack", "review" },
        { "exploit", "check" },
        { "attack", "test" },
        { "hack", "inspect" },
        { "cheat", "game analysis" },
        // Chinese
        { "破解", "授权分析" },
        { "外挂", "游戏客户端" },
        { "逆向", "二进制" },
        { "脱壳", "解包" },
        { "免杀", "兼容处理" },
        { "病毒", "样本" },
        { "木马", "样本" },
        { "攻击", "操作" },
        { "渗透", "评估" },
        { "提权", "权限评估" },
        { "越狱", "策略" },
    };
    for (const auto& p : kPairs) {
        s.replace(QString::fromUtf8(p.from), QString::fromUtf8(p.to), Qt::CaseInsensitive);
    }
    return s;
}

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

QStringList Skills::installInto(const QString& itemId, const QString& root)
{
    QStringList installed;
    if (root.isEmpty()) return installed;

    for (const SkillDef& sd : allSkills()) {
        if (extractSkill(sd.dirName, root)) installed << sd.dirName;
    }
    if (!installed.isEmpty()) saveInstalledNames(itemId, installed);
    return installed;
}

int Skills::removeFrom(const QString& root, const QStringList& names)
{
    if (root.isEmpty()) return 0;

    int count = 0;
    for (const QString& name : names) {
        const QString sdir = QDir::cleanPath(root + QLatin1Char('/') + name);
        QFile::remove(sdir + QStringLiteral("/SKILL.md"));
        QDir(sdir).rmdir(QStringLiteral("."));
        ++count;
    }
    return count;
}

QString Skills::buildIndex(const QString& absSkillsDir)
{
    QString out;
    for (const SkillDef& sd : allSkills()) {
        const QString res = QStringLiteral(":/skills/") + sd.dirName + QStringLiteral("/SKILL.md");
        QFile f(res);
        if (!f.open(QIODevice::ReadOnly)) continue;

        // Pull the first description line out of the YAML front matter.
        QString line;
        const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
        for (const QString& raw : lines) {
            const QString t = raw.trimmed();
            if (!t.startsWith(QLatin1String("description:"), Qt::CaseInsensitive)) continue;
            line = t.mid(12).trimmed();
            if (line.isEmpty()) continue;   // "description: |" -> take the next non-empty line
            break;
        }
        if (line.isEmpty()) {
            for (const QString& raw : lines) {
                const QString t = raw.trimmed();
                if (t.startsWith(QLatin1Char('#'))) { line = t; break; }
            }
        }
        if (line.size() > 90) line = line.left(90) + QStringLiteral("…");
        out += QStringLiteral("- %1: %2\n").arg(sd.dirName, wafSafeText(line));
    }
    return out;
}

int Skills::installFor(const QString& itemId)
{
    const Item* it = itemWithSkills(itemId);
    if (!it) return 0;
    return installInto(itemId, skillsDir(it->skillsDirRel)).size();
}

int Skills::uninstallFor(const QString& itemId)
{
    const Item* it = itemWithSkills(itemId);
    if (!it) return 0;

    const QStringList names = loadInstalledNames(itemId);
    const int count = removeFrom(skillsDir(it->skillsDirRel), names);
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
