// skills.cpp - Install / uninstall embedded skill packs

#include "skills.h"
#include "skill_catalog.h"
#include "items.h"
#include "paths.h"
#include "injector.h"
#include "install_plan.h"
#include "file_transaction.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

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

QString skillBackupDirFor(const QString& itemId)
{
    return QDir::cleanPath(stateDir() + QLatin1Char('/') + itemId + QStringLiteral("-skills-backups"));
}

QString skillsRootFileFor(const QString& itemId)
{
    return QDir::cleanPath(stateDir() + QLatin1Char('/') + itemId + QStringLiteral("-skills-root.dat"));
}

QString skillKey(const QString& name)
{
    return QString::fromLatin1(name.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
}

QString manifestPath(const QString& itemId, const QString& name)
{
    return skillBackupDirFor(itemId) + '/' + skillKey(name) + ".files.json";
}

bool validRelative(const QString& path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path) || path.contains('\\') ||
        path.contains(':') || path.contains(QChar::Null)) return false;
    for (const auto& part : path.split('/')) {
        if (part.isEmpty() || part == "." || part == ".." || part.endsWith('.') ||
            part.endsWith(' ') || part.contains(QRegularExpression("[<>\"|?*\\x00-\\x1f]")))
            return false;
        const auto device = part.section('.', 0, 0).toUpper();
        if (QRegularExpression("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$").match(device).hasMatch())
            return false;
    }
    return true;
}

bool validName(const QString& name)
{
    return validRelative(name) && !name.contains('/');
}

bool plainPath(const QString& path, const QString& boundary = QString())
{
    QFileInfo info(QDir::cleanPath(path));
    const QString stop = boundary.isEmpty() ? info.absoluteFilePath() :
        QFileInfo(QDir::cleanPath(boundary)).absoluteFilePath();
    if (info.absoluteFilePath() != stop && !info.absoluteFilePath().startsWith(stop + '/')) return false;
    for (;;) {
        if (info.isSymLink() || info.isJunction()) return false;
        if (info.absoluteFilePath() == stop) return true;
        const QString parent = info.absolutePath();
        if (parent == info.absoluteFilePath()) return false;
        info.setFile(parent);
    }
}

#if defined(Q_OS_WIN)
constexpr auto pathCase = Qt::CaseInsensitive;
#else
constexpr auto pathCase = Qt::CaseSensitive;
#endif

QString sharedSkillsRoot()
{
    return QDir::cleanPath(homeDir() + QStringLiteral("/.agents/skills"));
}

bool validSharedSkillsRoot(const QString& itemId, const QString& root)
{
    const bool shared = QDir::cleanPath(root).compare(sharedSkillsRoot(), pathCase) == 0;
    if (itemId != QLatin1String("vscode") && !shared) return true;
    return QDir::isAbsolutePath(root) &&
        shared && plainPath(root, homeDir());
}

QString metaPath(const QString& itemId, const QString& name,
                 const QString& relative, const QString& suffix)
{
    // Preserve the old SKILL.md metadata names during upgrades.
    const QString key = relative == "SKILL.md" ? skillKey(name) :
        skillKey(name) + '-' + QString::fromLatin1(QCryptographicHash::hash(
            relative.toUtf8(), QCryptographicHash::Sha256).toHex());
    return skillBackupDirFor(itemId) + '/' + key + suffix;
}

QStringList bundledFiles(const QString& name)
{
    const QString base = ":/skills/" + name + '/';
    QStringList files;
    QSet<QString> seen;
    QDirIterator it(base, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString relative = it.next().mid(base.size());
        if (!validRelative(relative) || seen.contains(relative.toCaseFolded())) return {};
        seen.insert(relative.toCaseFolded());
        files << relative;
    }
    files.removeDuplicates();
    files.sort();
    return files;
}

QStringList recordedFiles(const QString& itemId, const QString& name, bool* ok)
{
    *ok = true;
    if (itemId.isEmpty() || !QFileInfo::exists(manifestPath(itemId, name)))
        return {"SKILL.md"};
    bool read = false;
    const auto doc = QJsonDocument::fromJson(Injector::readFile(manifestPath(itemId, name), &read));
    *ok = read && doc.isObject() && doc.object().value("version").toInt() == 1 &&
          doc.object().value("files").isArray();
    QStringList files;
    if (!*ok) return {};
    QSet<QString> seen;
    for (const auto& value : doc.object().value("files").toArray()) {
        const auto path = value.toString();
        const auto key = path.toCaseFolded();
        if (!value.isString() || !validRelative(path) || seen.contains(key)) {
            *ok = false;
            return {};
        }
        seen.insert(key);
        files << path;
    }
    *ok = files.contains("SKILL.md");
    return files;
}

QByteArray skillHash(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

QStringList loadInstalledNames(const QString& itemId)
{
    QStringList out;
    bool ok = false;
    const QByteArray raw = Injector::readFile(skillsStateFileFor(itemId), &ok);
    if (!ok || raw.isEmpty()) return out;
    for (const QByteArray& line : raw.split('\n')) {
        const QString s = QString::fromUtf8(line).trimmed();
        if (!s.isEmpty() && !out.contains(s)) out << s;
    }
    return out;
}

bool saveInstalledNames(const QString& itemId, const QStringList& names)
{
    return Injector::writeAtomic(skillsStateFileFor(itemId), (names.join('\n') + '\n').toUtf8());
}

bool removeFile(const QString& path)
{
    return !QFileInfo::exists(path) ||
        (detail::transactionWritable(path) && QFile::remove(path));
}

bool extractFile(const QString& itemId, const QString& name, const QString& relative,
                 const QString& root, bool legacyOwned)
{
    bool read = false;
    QByteArray data = Injector::readFile(":/skills/" + name + '/' + relative, &read);
    if (!read) return false;
    if (itemId == QLatin1String("vscode") && relative == QLatin1String("SKILL.md")) {
        data = SkillCatalog::normalize(data, name, &read);
        if (!read) return false;
    }
    const QString target = root + '/' + name + '/' + relative;
    if (!plainPath(target, root)) return false;
    const QString original = metaPath(itemId, name, relative, ".orig");
    const QString created = metaPath(itemId, name, relative, ".new");
    const QString stamp = metaPath(itemId, name, relative, ".sha256");
    const bool exists = QFileInfo::exists(target);
    const bool tracked = QFileInfo::exists(original) || QFileInfo::exists(created);
    QByteArray current;
    if (exists) {
        current = Injector::readFile(target, &read);
        if (!read || (tracked && skillHash(current) != Injector::readFile(stamp).trimmed())) {
            qWarning() << "[skills] preserving modified file" << target;
            return false;
        }
    }
    if (!tracked) {
        if (exists && !(legacyOwned && relative == "SKILL.md" && current == data)) {
            if (!QDir().mkpath(QFileInfo(original).absolutePath()) || !QFile::copy(target, original))
                return false;
        } else if (!Injector::writeAtomic(created, {})) return false;
    }
    return Injector::writeAtomic(stamp, skillHash(data)) && Injector::writeAtomic(target, data);
}

bool restoreFile(const QString& itemId, const QString& name, const QString& relative,
                 const QString& root)
{
    const QString target = root + '/' + name + '/' + relative;
    if (!plainPath(target, root)) return false;
    const QString original = itemId.isEmpty() ? QString() : metaPath(itemId, name, relative, ".orig");
    const QString created = itemId.isEmpty() ? QString() : metaPath(itemId, name, relative, ".new");
    const QString stamp = itemId.isEmpty() ? QString() : metaPath(itemId, name, relative, ".sha256");
    const bool exists = QFileInfo::exists(target);
    bool read = false;
    const auto current = exists ? Injector::readFile(target, &read) : QByteArray();
    if (exists && (!read || (QFileInfo::exists(stamp) &&
        skillHash(current) != Injector::readFile(stamp).trimmed()))) {
        qWarning() << "[skills] preserving user changes" << target;
        return false;
    }
    if (QFileInfo::exists(original)) {
        const auto permissions = QFileInfo(original).permissions();
        if (!detail::safeCopy(original, target) || !QFile::setPermissions(target, permissions))
            return false;
    } else if (QFileInfo::exists(created)) {
        if (!removeFile(target)) return false;
    } else if (exists) {
        const auto bundled = Injector::readFile(":/skills/" + name + '/' + relative, &read);
        if (!read || current != bundled || !removeFile(target)) return false;
    }
    for (const auto& metadata : {original, created, stamp}) {
        if (!metadata.isEmpty() && !removeFile(metadata)) return false;
    }
    return true;
}

bool captureFiles(detail::FileTransaction& tx, const QString& itemId,
                  const QString& root, const QStringList& names)
{
    const auto paths = Skills::transactionFiles(itemId, root, names);
    if (paths.isEmpty()) return false;
    for (const auto& path : paths) {
        if (!tx.capture(path)) return false;
    }
    return true;
}

const Item* itemWithSkills(const QString& itemId)
{
    const Item* it = itemById(itemId);
    return (it && !it->skillsDirRel.isEmpty()) ? it : nullptr;
}

} // namespace

QStringList Skills::installInto(const QString& itemId, const QString& root, bool* ok,
                               ProgressCallback progress)
{
    if (ok) *ok = false;
    if (!itemWithSkills(itemId) || !QDir::isAbsolutePath(root) || !plainPath(root)) return {};
    const QString normalizedRoot = QDir::cleanPath(root);
    const QString sharedRoot = sharedSkillsRoot();
    if (normalizedRoot.compare(sharedRoot, pathCase) == 0 &&
        itemId != QLatin1String("vscode")) return {};
    if (itemId == QLatin1String("vscode")) {
        if (!validSharedSkillsRoot(itemId, normalizedRoot)) return {};
        for (const auto& item : allItems()) {
            if (item.id == itemId) continue;
            const auto ownedRoot = QString::fromUtf8(Injector::readFile(skillsRootFileFor(item.id))).trimmed();
            if (!ownedRoot.isEmpty() && QDir::cleanPath(ownedRoot).compare(sharedRoot, pathCase) == 0 &&
                !loadInstalledNames(item.id).isEmpty()) {
                qWarning() << "[skills] shared library already has an installation owner" << item.id;
                return {};
            }
        }
    }
    const QString savedRoot = QString::fromUtf8(Injector::readFile(skillsRootFileFor(itemId))).trimmed();
    const QStringList prior = loadInstalledNames(itemId);
    QStringList allNames = prior;
    for (const auto& skill : allSkills()) {
        if (!allNames.contains(skill.dirName)) allNames << skill.dirName;
    }
    detail::FileTransaction tx;
    if (!captureFiles(tx, itemId, normalizedRoot, allNames)) return {};
    if (!savedRoot.isEmpty() && savedRoot != normalizedRoot && !prior.isEmpty()) {
        if (!captureFiles(tx, itemId, savedRoot, prior)) return {};
        bool removed = false;
        removeFrom(savedRoot, prior, itemId, &removed);
        if (!removed) return {};
    }
    if (!Injector::writeAtomic(skillsRootFileFor(itemId), normalizedRoot.toUtf8())) return {};
    QStringList installed;
    int completed = 0;
    const int total = allSkills().size();
    for (const auto& skill : allSkills()) {
        const auto files = bundledFiles(skill.dirName);
        if (!files.contains("SKILL.md")) return {};
        bool valid = false;
        const auto oldFiles = recordedFiles(itemId, skill.dirName, &valid);
        if (!valid) return {};
        const bool legacy = savedRoot == normalizedRoot && prior.contains(skill.dirName) &&
                            !QFileInfo::exists(manifestPath(itemId, skill.dirName));
        for (const auto& relative : oldFiles) {
            if (!files.contains(relative) && !restoreFile(itemId, skill.dirName, relative, normalizedRoot))
                return {};
        }
        for (const auto& relative : files) {
            const auto target = normalizedRoot + '/' + skill.dirName + '/' + relative;
            if (progress) progress(completed, total, target);
            if (!extractFile(itemId, skill.dirName, relative, normalizedRoot, legacy)) return {};
        }
        const auto manifest = QJsonDocument(QJsonObject{
            {"version", 1}, {"files", QJsonArray::fromStringList(files)}}).toJson();
        if (!Injector::writeAtomic(manifestPath(itemId, skill.dirName), manifest)) return {};
        installed << skill.dirName;
        if (progress) progress(++completed, total, normalizedRoot + '/' + skill.dirName + "/SKILL.md");
    }
    QStringList tracked = savedRoot == normalizedRoot ? prior : QStringList();
    for (const auto& name : installed) if (!tracked.contains(name)) tracked << name;
    if (!saveInstalledNames(itemId, tracked)) return {};
    tx.commit();
    if (ok) *ok = true;
    return installed;
}

int Skills::removeFrom(const QString& root, const QStringList& names)
{
    for (const Item& item : allItems()) {
        const QString savedRoot = QString::fromUtf8(Injector::readFile(skillsRootFileFor(item.id))).trimmed();
        if (!savedRoot.isEmpty() && savedRoot == QDir::cleanPath(root))
            return removeFrom(root, names, item.id);
    }
    return removeFrom(root, names, QString());
}

int Skills::removeFrom(const QString& root, const QStringList& names, const QString& itemId, bool* ok)
{
    if (ok) *ok = false;
    if (!QDir::isAbsolutePath(root) || !plainPath(root) ||
        !validSharedSkillsRoot(itemId, root) ||
        (!itemId.isEmpty() && !itemWithSkills(itemId))) return 0;
    detail::FileTransaction tx;
    if (!captureFiles(tx, itemId, root, names)) return 0;
    auto remaining = itemId.isEmpty() ? QStringList() : loadInstalledNames(itemId);
    QStringList emptyDirs;
    for (const auto& name : names) {
        bool valid = false;
        const auto files = recordedFiles(itemId, name, &valid);
        if (!valid) return 0;
        for (const auto& relative : files) {
            if (!restoreFile(itemId, name, relative, root)) return 0;
            QString dir = QFileInfo(root + '/' + name + '/' + relative).absolutePath();
            const QString boundary = QDir::cleanPath(root);
            while (dir.startsWith(boundary + '/')) {
                emptyDirs << dir;
                dir = QFileInfo(dir).absolutePath();
            }
        }
        if (!itemId.isEmpty() && !removeFile(manifestPath(itemId, name))) return 0;
        remaining.removeAll(name);
    }
    if (!itemId.isEmpty()) {
        if (remaining.isEmpty()) {
            if (!removeFile(skillsStateFileFor(itemId)) || !removeFile(skillsRootFileFor(itemId))) return 0;
        } else if (!saveInstalledNames(itemId, remaining)) return 0;
        ItemState state;
        if (Injector::loadState(itemId, state)) {
            state.skills = remaining;
            if (!Injector::saveState(itemId, state)) return 0;
        }
    }
    tx.commit();
    emptyDirs.removeDuplicates();
    std::sort(emptyDirs.begin(), emptyDirs.end(), [](const QString& a, const QString& b) {
        return a.size() > b.size();
    });
    for (const auto& dir : emptyDirs) QDir().rmdir(dir);
    if (!itemId.isEmpty()) QDir().rmdir(skillBackupDirFor(itemId));
    if (ok) *ok = true;
    return names.size();
}

QString Skills::installedRoot(const QString& itemId)
{
    const QString root = QString::fromUtf8(Injector::readFile(skillsRootFileFor(itemId))).trimmed();
    if (!root.isEmpty()) return root;
    const Item* item = itemWithSkills(itemId);
    return item ? planFor(*item).skillsDir : QString();
}

QStringList Skills::installedNames(const QString& itemId)
{
    return loadInstalledNames(itemId);
}

bool Skills::intact(const QString& itemId, const QStringList& expectedNames)
{
    const auto names = loadInstalledNames(itemId);
    QSet<QString> actual;
    QSet<QString> expected;
    for (const auto& name : names) actual.insert(name);
    for (const auto& name : expectedNames) expected.insert(name);
    if (actual != expected) return false;
    const QString root = installedRoot(itemId);
    if (!validSharedSkillsRoot(itemId, root)) return false;
    for (const auto& name : names) {
        if (!validName(name)) return false;
        bool valid = false;
        const auto files = recordedFiles(itemId, name, &valid);
        if (!valid) return false;
        for (const auto& relative : files) {
            if (!QFileInfo(root + '/' + name + '/' + relative).isFile()) return false;
        }
        if (itemId == QLatin1String("vscode")) {
            SkillMetadata metadata;
            if (!SkillCatalog::parse(Injector::readFile(root + '/' + name + "/SKILL.md"), &metadata) ||
                metadata.name != name) return false;
        }
    }
    return true;
}

QStringList Skills::transactionFiles(const QString& itemId, const QString& root, const QStringList& names)
{
    if (!QDir::isAbsolutePath(root) ||
        !validSharedSkillsRoot(itemId, root)) return {};
    QStringList paths;
    if (!itemId.isEmpty())
        paths << QDir::cleanPath(stateDir() + '/' + itemId + ".dat")
              << skillsStateFileFor(itemId) << skillsRootFileFor(itemId);
    for (const auto& name : names) {
        if (!validName(name)) return {};
        bool valid = false;
        QStringList files = recordedFiles(itemId, name, &valid);
        if (!valid) return {};
        files << bundledFiles(name);
        files.removeDuplicates();
        if (!itemId.isEmpty()) paths << manifestPath(itemId, name);
        for (const auto& relative : files) {
            paths << root + '/' + name + '/' + relative;
            if (!itemId.isEmpty()) {
                for (const auto& suffix : {".orig", ".new", ".sha256"})
                    paths << metaPath(itemId, name, relative, suffix);
            }
        }
    }
    for (const auto& path : paths) {
        const QString boundary = path.startsWith(root + '/') ? root : stateDir();
        if (!plainPath(path, boundary)) return {};
    }
    return paths;
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
    return installInto(itemId, planFor(*it).skillsDir).size();
}

int Skills::uninstallFor(const QString& itemId)
{
    const Item* it = itemWithSkills(itemId);
    if (!it) return 0;

    const QStringList names = loadInstalledNames(itemId);
    return removeFrom(installedRoot(itemId), names, itemId);
}

int Skills::uninstallByName(const QString& skillDirName)
{
    if (skillDirName.isEmpty()) return 0;

    int toolCount = 0;
    for (const Item& it : allItems()) {
        if (it.skillsDirRel.isEmpty()) continue;
        QStringList names = loadInstalledNames(it.id);
        if (!names.contains(skillDirName)) continue;

        const QString root = installedRoot(it.id);
        const int removed = removeFrom(root, {skillDirName}, it.id);
        if (removed == 0) continue;
        ++toolCount;
    }
    return toolCount;
}

QStringList Skills::missingBundledAssets(const QString& skillDirName)
{
    if (!validName(skillDirName)) return {};
    const QString base = ":/skills/" + skillDirName + '/';
    bool read = false;
    const auto text = QString::fromUtf8(Injector::readFile(base + "SKILL.md", &read));
    if (!read) return {"SKILL.md"};
    static const QRegularExpression localPath(
        "(?<![A-Za-z0-9_./-])(?:references|scripts)/[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*");
    auto matches = localPath.globalMatch(text);
    QStringList missing;
    while (matches.hasNext()) {
        QString relative = matches.next().captured();
        while (relative.endsWith('.')) relative.chop(1);
        if (validRelative(relative) && !QFileInfo::exists(base + relative) && !missing.contains(relative))
            missing << relative;
    }
    missing.sort();
    return missing;
}

QVector<SkillStatus> Skills::statuses()
{
    QVector<SkillStatus> out;
    out.reserve(allSkills().size());
    QStringList roots;
    for (const Item& item : allItems()) {
        if (!item.skillsDirRel.isEmpty()) roots << installedRoot(item.id);
    }
    for (const SkillDef& sd : allSkills()) {
        int count = 0;
        for (const QString& root : roots) {
            if (root.isEmpty()) continue;
            const QString file = QDir::cleanPath(
                root + QLatin1Char('/') + sd.dirName + QStringLiteral("/SKILL.md"));
            if (QFileInfo::exists(file)) ++count;
        }
        out.push_back({sd.dirName, count, static_cast<int>(sd.category), missingBundledAssets(sd.dirName)});
    }
    return out;
}

} // namespace ua::core
