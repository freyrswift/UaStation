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
#include <QThread>
#include <algorithm>

namespace ua::core {

namespace {

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

QStringList recordedFiles(const QString& itemId, const QString& name, bool* ok,
                          QStringList* kept = nullptr)
{
    *ok = true;
    if (kept) kept->clear();
    if (itemId.isEmpty() || !QFileInfo::exists(manifestPath(itemId, name)))
        return {"SKILL.md"};
    bool read = false;
    const auto doc = QJsonDocument::fromJson(Injector::readFile(manifestPath(itemId, name), &read));
    const auto root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt();
    *ok = read && doc.isObject() && (version == 1 || version == 2) &&
          root.value(QStringLiteral("files")).isArray();
    if (*ok)
        *ok = root.value(QStringLiteral("kept")).isUndefined() ||
              root.value(QStringLiteral("kept")).isArray();
    if (!*ok) return {};

    QStringList files;
    QStringList keptFiles;
    QSet<QString> seen;
    const auto append = [&](const QJsonValue& value, QStringList& out) {
        const auto path = value.toString();
        const auto key = path.toCaseFolded();
        if (!value.isString() || !validRelative(path) || seen.contains(key)) return false;
        seen.insert(key);
        out << path;
        return true;
    };
    for (const auto& value : root.value(QStringLiteral("files")).toArray()) {
        if (!append(value, files)) { *ok = false; return {}; }
    }
    if (version >= 1) {
        for (const auto& value : root.value(QStringLiteral("kept")).toArray()) {
            if (!append(value, keptFiles)) { *ok = false; return {}; }
        }
    }
    *ok = files.contains(QStringLiteral("SKILL.md")) ||
          keptFiles.contains(QStringLiteral("SKILL.md"));
    if (kept) *kept = keptFiles;
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

bool writeFile(const QString& path, const QByteArray& data);

bool saveInstalledNames(const QString& itemId, const QStringList& names)
{
    return writeFile(skillsStateFileFor(itemId), (names.join('\n') + '\n').toUtf8());
}

bool removeFile(const QString& path)
{
    // Antivirus scanners, sync clients and hosts can hold a file open for a
    // short window; retry instead of failing the whole library.
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (!QFileInfo::exists(path)) return true;
        if (detail::transactionWritable(path) && QFile::remove(path)) return true;
        if (attempt < 2) QThread::msleep(120);
    }
    return false;
}

// Same rationale as removeFile(), plus clearing a read-only attribute left by
// an external tool so the atomic replacement can rename over the target.
bool writeFile(const QString& path, const QByteArray& data)
{
    for (int attempt = 0; attempt < 3; ++attempt) {
        const QFileInfo info(path);
        if (info.exists()) {
            const auto permissions = info.permissions();
            if (!(permissions & QFile::WriteUser))
                QFile::setPermissions(path, permissions | QFile::WriteUser | QFile::WriteOwner);
        }
        if (Injector::writeAtomic(path, data)) return true;
        if (attempt < 2) QThread::msleep(200);
    }
    return false;
}

bool restoreCopy(const QString& source, const QString& target, QFile::Permissions permissions)
{
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (detail::safeCopy(source, target) && QFile::setPermissions(target, permissions)) return true;
        if (attempt < 2) QThread::msleep(150);
    }
    return false;
}

enum class FileOutcome { Written, Kept, Failed };

FileOutcome extractFile(const QString& itemId, const QString& name, const QString& relative,
                        const QString& root, bool legacyOwned)
{
    bool read = false;
    QByteArray data = Injector::readFile(":/skills/" + name + '/' + relative, &read);
    if (!read) {
        qWarning() << "[skills] bundle resource missing" << name << relative;
        return FileOutcome::Failed;
    }
    if (itemId == QLatin1String("vscode") && relative == QLatin1String("SKILL.md")) {
        data = SkillCatalog::normalize(data, name, &read);
        if (!read) {
            qWarning() << "[skills] metadata normalisation failed" << name;
            return FileOutcome::Failed;
        }
    }
    const QString target = root + '/' + name + '/' + relative;
    if (!plainPath(target, root)) {
        qWarning() << "[skills] path boundary rejected" << target;
        return FileOutcome::Failed;
    }
    const QString original = metaPath(itemId, name, relative, ".orig");
    const QString created = metaPath(itemId, name, relative, ".new");
    const QString stamp = metaPath(itemId, name, relative, ".sha256");
    const bool exists = QFileInfo::exists(target);
    const bool tracked = QFileInfo::exists(original) || QFileInfo::exists(created);
    QByteArray current;
    if (exists) {
        current = Injector::readFile(target, &read);
        if (!read) {
            qWarning() << "[skills] preserving unreadable file" << target;
            return FileOutcome::Kept;
        }
        if (tracked && skillHash(current) != Injector::readFile(stamp).trimmed()) {
            qWarning() << "[skills] preserving modified file" << target;
            return FileOutcome::Kept;
        }
        if (tracked && current == data) return FileOutcome::Written;
    }
    if (!tracked) {
        if (exists && !(legacyOwned && relative == "SKILL.md" && current == data)) {
            if (!QDir().mkpath(QFileInfo(original).absolutePath()) || !QFile::copy(target, original)) {
                qWarning() << "[skills] cannot back up" << target << "->" << original;
                return FileOutcome::Failed;
            }
        } else if (!writeFile(created, {})) {
            qWarning() << "[skills] cannot write marker" << created;
            return FileOutcome::Failed;
        }
    }
    if (!writeFile(stamp, skillHash(data))) {
        qWarning() << "[skills] cannot write stamp" << stamp;
        return exists ? FileOutcome::Kept : FileOutcome::Failed;
    }
    if (!writeFile(target, data)) {
        qWarning() << "[skills] cannot write target" << target;
        return exists ? FileOutcome::Kept : FileOutcome::Failed;
    }
    return FileOutcome::Written;
}

bool restoreFile(const QString& itemId, const QString& name, const QString& relative,
                 const QString& root)
{
    const QString target = root + '/' + name + '/' + relative;
    if (!plainPath(target, root)) {
        qWarning() << "[skills] restore path boundary rejected" << target;
        return false;
    }
    const QString original = itemId.isEmpty() ? QString() : metaPath(itemId, name, relative, ".orig");
    const QString created = itemId.isEmpty() ? QString() : metaPath(itemId, name, relative, ".new");
    const QString stamp = itemId.isEmpty() ? QString() : metaPath(itemId, name, relative, ".sha256");
    const bool exists = QFileInfo::exists(target);
    bool read = false;
    const auto current = exists ? Injector::readFile(target, &read) : QByteArray();
    if (exists && (!read || (QFileInfo::exists(stamp) &&
        skillHash(current) != Injector::readFile(stamp).trimmed()))) {
        qWarning() << "[skills] preserving user changes" << target
                   << (read ? "hash mismatch" : "unreadable");
        return false;
    }
    if (QFileInfo::exists(original)) {
        const auto permissions = QFileInfo(original).permissions();
        if (!restoreCopy(original, target, permissions)) {
            qWarning() << "[skills] cannot restore original" << original << "->" << target;
            return false;
        }
    } else if (QFileInfo::exists(created)) {
        if (!removeFile(target)) {
            qWarning() << "[skills] cannot remove created file" << target;
            return false;
        }
    } else if (exists) {
        const auto bundled = Injector::readFile(":/skills/" + name + '/' + relative, &read);
        if (!read || current != bundled || !removeFile(target)) {
            qWarning() << "[skills] cannot remove unmodified file" << target;
            return false;
        }
    }
    for (const auto& metadata : {original, created, stamp}) {
        if (!metadata.isEmpty() && !removeFile(metadata)) {
            qWarning() << "[skills] cannot remove metadata" << metadata;
            return false;
        }
    }
    return true;
}

bool packageIntact(const QString& itemId, const QString& root, const QString& name)
{
    if (!validName(name) || !validSharedSkillsRoot(itemId, root)) return false;
    const auto required = bundledFiles(name);
    if (!required.contains("SKILL.md")) return false;
    bool valid = true;
    QStringList kept;
    const auto files = recordedFiles(itemId, name, &valid, &kept);
    if (!valid) return false;
    for (const auto& relative : required) {
        if (kept.contains(relative)) continue;   // intentionally user-owned
        if (!files.contains(relative)) return false;
        const QString target = root + '/' + name + '/' + relative;
        if (!plainPath(target, root) || !QFileInfo(target).isFile()) return false;
        bool read = false;
        QByteArray data = Injector::readFile(":/skills/" + name + '/' + relative, &read);
        if (!read) return false;
        if (itemId == QLatin1String("vscode") && relative == QLatin1String("SKILL.md")) {
            data = SkillCatalog::normalize(data, name, &read);
            if (!read) return false;
        }
        if (skillHash(data) != Injector::readFile(metaPath(itemId, name, relative, ".sha256")).trimmed())
            return false;
    }
    if (itemId == QLatin1String("vscode") && !kept.contains(QStringLiteral("SKILL.md"))) {
        SkillMetadata metadata;
        if (!SkillCatalog::parse(Injector::readFile(root + '/' + name + "/SKILL.md"), &metadata) ||
            metadata.name != name) return false;
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
                               ProgressCallback progress, QString* failedSkill)
{
    if (ok) *ok = false;
    // Records which package was being written when the install stopped, so the
    // injected error can name it instead of only saying "write failed".
    const auto fail = [failedSkill](const QString& name) {
        if (failedSkill) *failedSkill = name;
        return QStringList{};
    };
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
    QStringList currentNames;
    for (const auto& skill : allSkills()) {
        currentNames << skill.dirName;
        if (!allNames.contains(skill.dirName)) allNames << skill.dirName;
    }
    detail::FileTransaction tx;
    if (!captureFiles(tx, itemId, normalizedRoot, allNames)) return {};
    if (!savedRoot.isEmpty() && savedRoot != normalizedRoot && !prior.isEmpty()) {
        if (!captureFiles(tx, itemId, savedRoot, prior)) return {};
        bool removed = false;
        QString failedName;
        removeFrom(savedRoot, prior, itemId, &removed, &failedName);
        if (!removed) return fail(failedName);
    }
    if (savedRoot == normalizedRoot) {
        QStringList retired;
        for (const auto& name : prior) if (!currentNames.contains(name)) retired << name;
        if (!retired.isEmpty()) {
            bool removed = false;
            QString failedName;
            removeFrom(normalizedRoot, retired, itemId, &removed, &failedName);
            if (!removed) return fail(failedName);
        }
    }
    if (!writeFile(skillsRootFileFor(itemId), normalizedRoot.toUtf8())) return {};
    QStringList installed;
    int completed = 0;
    const int total = allSkills().size();
    for (const auto& skill : allSkills()) {
        const auto files = bundledFiles(skill.dirName);
        if (!files.contains("SKILL.md")) {
            qWarning() << "[skills] package has no usable SKILL.md" << skill.dirName;
            return fail(skill.dirName);
        }
        bool valid = false;
        const auto oldFiles = recordedFiles(itemId, skill.dirName, &valid);
        if (!valid) {
            qWarning() << "[skills] manifest invalid" << manifestPath(itemId, skill.dirName);
            return fail(skill.dirName);
        }
        const bool legacy = savedRoot == normalizedRoot && prior.contains(skill.dirName) &&
                            !QFileInfo::exists(manifestPath(itemId, skill.dirName));
        for (const auto& relative : oldFiles) {
            if (!files.contains(relative) &&
                !restoreFile(itemId, skill.dirName, relative, normalizedRoot)) {
                // A retired file the host changed is left in place; it is no
                // longer part of the package and must not block the update.
                qWarning() << "[skills] leaving obsolete user file" << skill.dirName << relative;
            }
        }
        QStringList written;
        QStringList kept;
        for (const auto& relative : files) {
            const auto target = normalizedRoot + '/' + skill.dirName + '/' + relative;
            if (progress) progress(completed, total, target);
            switch (extractFile(itemId, skill.dirName, relative, normalizedRoot, legacy)) {
            case FileOutcome::Written:
                written << relative;
                break;
            case FileOutcome::Kept:
                kept << relative;
                break;
            case FileOutcome::Failed:
                return fail(skill.dirName);
            }
        }
        if (!kept.isEmpty()) {
            qInfo() << "[skills] kept user files" << skill.dirName << kept.size()
                    << "of" << files.size();
        }
        QJsonObject manifestObject{
            {QStringLiteral("version"), 1},
            {QStringLiteral("files"), QJsonArray::fromStringList(written)}};
        if (!kept.isEmpty())
            manifestObject[QStringLiteral("kept")] = QJsonArray::fromStringList(kept);
        const auto manifest = QJsonDocument(manifestObject).toJson();
        if (!writeFile(manifestPath(itemId, skill.dirName), manifest)) {
            qWarning() << "[skills] cannot write manifest" << manifestPath(itemId, skill.dirName);
            return fail(skill.dirName);
        }
        installed << skill.dirName;
        if (progress) progress(++completed, total, normalizedRoot + '/' + skill.dirName + "/SKILL.md");
    }
    if (!saveInstalledNames(itemId, installed)) return {};
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

int Skills::removeFrom(const QString& root, const QStringList& names, const QString& itemId, bool* ok,
                       QString* failedSkill)
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
        QStringList kept;
        const auto files = recordedFiles(itemId, name, &valid, &kept);
        if (!valid) { if (failedSkill) *failedSkill = name; return 0; }
        int leftovers = 0;
        for (const auto& relative : kept) {
            qWarning() << "[skills] keeping user-modified file" << (root + '/' + name + '/' + relative);
            ++leftovers;
        }
        for (const auto& relative : files) {
            if (!restoreFile(itemId, name, relative, root)) {
                // A file the host or the user changed is kept in place; it must
                // not block uninstalling the rest of the package.
                ++leftovers;
                continue;
            }
            QString dir = QFileInfo(root + '/' + name + '/' + relative).absolutePath();
            const QString boundary = QDir::cleanPath(root);
            while (dir.startsWith(boundary + '/')) {
                emptyDirs << dir;
                dir = QFileInfo(dir).absolutePath();
            }
        }
        if (leftovers)
            qInfo() << "[skills] uninstall kept" << leftovers << "file(s) in" << name;
        if (!itemId.isEmpty() && !removeFile(manifestPath(itemId, name))) {
            if (failedSkill) *failedSkill = name;
            return 0;
        }
        remaining.removeAll(name);

        // Kept files can leave now-empty directories behind; collect every
        // directory under the package and remove the empty ones after commit.
        const QString packageRoot = QDir::cleanPath(root + '/' + name);
        if (QFileInfo(packageRoot).isDir()) {
            emptyDirs << packageRoot;
            QDirIterator dirs(packageRoot, QDir::Dirs | QDir::NoDotAndDotDot,
                              QDirIterator::Subdirectories);
            while (dirs.hasNext()) emptyDirs << dirs.next();
        }
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
        if (!packageIntact(itemId, root, name)) return false;
    }
    return true;
}

QStringList Skills::transactionFiles(const QString& itemId, const QString& root, const QStringList& names)
{
    if (!QDir::isAbsolutePath(root) ||
        !validSharedSkillsRoot(itemId, root)) return {};
    QStringList paths{QDir::cleanPath(root + QStringLiteral("/INDEX.md"))};
    if (!itemId.isEmpty())
        paths << QDir::cleanPath(stateDir() + '/' + itemId + ".dat")
              << skillsStateFileFor(itemId) << skillsRootFileFor(itemId);
    for (const auto& name : names) {
        if (!validName(name)) return {};
        bool valid = false;
        QStringList kept;
        QStringList files = recordedFiles(itemId, name, &valid, &kept);
        if (!valid) return {};
        files << kept;
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
    if (!QDir::isAbsolutePath(absSkillsDir)) return {};
    const QString root = QDir::cleanPath(absSkillsDir);
    QJsonArray entries;
    for (const SkillDef& sd : allSkills()) {
        const QString res = QStringLiteral(":/skills/") + sd.dirName + QStringLiteral("/SKILL.md");
        QFile f(res);
        SkillMetadata metadata;
        if (!f.open(QIODevice::ReadOnly)) return {};
        bool normalized = false;
        const auto document = SkillCatalog::normalize(f.readAll(), sd.dirName, &normalized);
        if (!normalized || !SkillCatalog::parse(document, &metadata)) return {};
        entries.append(QJsonObject{
            {QStringLiteral("directory"), sd.dirName},
            {QStringLiteral("name"), metadata.name},
            {QStringLiteral("description"), metadata.description},
            {QStringLiteral("document"), root + '/' + sd.dirName + QStringLiteral("/SKILL.md")}
        });
    }
    const QJsonObject catalog{{QStringLiteral("root"), root}, {QStringLiteral("skills"), entries}};
    return QStringLiteral("# Ua Skill Catalog\n\n"
                          "Names are canonical directory keys; descriptions are normalized metadata summaries. "
                          "Original documents in non-shared libraries may declare different names. "
                          "Catalog metadata is not a loaded instruction or proof of execution. "
                          "Read the matching document and its required assets before using a skill.\n\n```json\n") +
        QString::fromUtf8(QJsonDocument(catalog).toJson(QJsonDocument::Indented)) + QStringLiteral("```\n");
}

QString Skills::buildDiscoveryPrompt(const QString& absSkillsDir, bool includeNames)
{
    if (!QDir::isAbsolutePath(absSkillsDir)) return {};
    const QString root = QDir::cleanPath(absSkillsDir);
    const QJsonObject location{
        {QStringLiteral("root"), root},
        {QStringLiteral("index"), root + QStringLiteral("/INDEX.md")},
        {QStringLiteral("count"), allSkills().size()}
    };
    QString out = QStringLiteral(
        "# Ua Skill Discovery\n\n"
        "Catalog: %1\n"
        "For a matching task, consult the client's native skill list first. If Ua skills are absent, "
        "use a file-reading tool to read the catalog index, select relevant entries by description, "
        "then read root/<directory>/SKILL.md and any required companion files. Directory keys and "
        "declared skill names may differ; use the index's document path for local reads.\n"
        "If the ua-skills MCP server is connected and its tools are available, use list_skills, "
        "then read_skill and read_asset with names returned by that server. Follow nextOffset "
        "pagination until the needed content is complete. Do not treat directory names as tool names.\n"
        "Use only available tools and permitted paths. If neither file access nor MCP is available, "
        "report the missing connection instead of claiming a skill was loaded. Loading this prompt "
        "or replying to Ua does not prove skill loading. Do not execute scripts merely because they "
        "appear in the catalog.\n").arg(QString::fromUtf8(QJsonDocument(location).toJson(QJsonDocument::Compact)));
    if (includeNames) {
        QStringList names;
        for (const auto& skill : allSkills()) names << skill.dirName;
        out += QStringLiteral("Directory names (routing hints, not loaded skill bodies): ") +
            names.join(QStringLiteral(", ")) + QLatin1Char('\n');
    }
    return out + QLatin1Char('\n');
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
    struct Profile {
        QString id;
        QString root;
        QStringList managedNames;
    };
    QVector<Profile> profiles;
    for (const Item& item : allItems()) {
        if (!item.skillsDirRel.isEmpty())
            profiles.push_back({item.id, installedRoot(item.id), loadInstalledNames(item.id)});
    }
    for (const SkillDef& sd : allSkills()) {
        int count = 0;
        bool needsRepair = false;
        for (const auto& profile : profiles) {
            if (profile.root.isEmpty()) continue;
            const bool managed = profile.managedNames.contains(sd.dirName);
            const QString file = QDir::cleanPath(
                profile.root + QLatin1Char('/') + sd.dirName + QStringLiteral("/SKILL.md"));
            if (!managed && !QFileInfo::exists(file)) continue;
            ++count;
            if (managed && !packageIntact(profile.id, profile.root, sd.dirName)) needsRepair = true;
        }
        out.push_back({sd.dirName, count, static_cast<int>(sd.category), missingBundledAssets(sd.dirName), needsRepair});
    }
    return out;
}

} // namespace ua::core
