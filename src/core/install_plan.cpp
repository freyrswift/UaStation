// install_plan.cpp - Per-product install profile resolution

#include "install_plan.h"

#include "injector.h"
#include "items.h"
#include "paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace ua::core {

namespace {

QString firstExistingDir(const QStringList& candidates)
{
    for (const QString& c : candidates) {
        if (!c.isEmpty() && QFileInfo(c).isDir()) return c;
    }
    return candidates.isEmpty() ? QString() : candidates.first();
}

// Cursor keeps global User Rules in a data dir that moves between builds:
// portable installs, a custom --user-data-dir, and CN/EN channel layouts all
// differ. Probe the known spots and use the first that exists.
QStringList cursorRuleDirs()
{
    QStringList out;
    out << QDir::cleanPath(homeDir() + QStringLiteral("/.cursor/rules"));

    const QString appData = qEnvironmentVariable("APPDATA");
    const QString local   = qEnvironmentVariable("LOCALAPPDATA");
    if (!appData.isEmpty()) {
        out << QDir::cleanPath(appData + QStringLiteral("/Cursor/User/rules"));
        out << QDir::cleanPath(appData + QStringLiteral("/Cursor/rules"));
    }
    if (!local.isEmpty()) {
        out << QDir::cleanPath(local + QStringLiteral("/Cursor/User/rules"));
        out << QDir::cleanPath(local + QStringLiteral("/Cursor/rules"));
    }
    out << QDir::cleanPath(homeDir() + QStringLiteral("/.config/Cursor/User/rules"));
    out << QDir::cleanPath(homeDir() + QStringLiteral("/Library/Application Support/Cursor/User/rules"));
    return out;
}

// WorkBuddy 5.8.x reads <home>/.workbuddy (see getWorkbuddyConfigBaseDir in the
// bundle); .workbuddy-ai is the pre-5.8 data folder that the app migrates away
// from. Probe the live one first, keep the legacy one as a fallback.
QString workbuddyConfigDir()
{
    const QStringList candidates{
        QDir::cleanPath(homeDir() + QStringLiteral("/.workbuddy")),
        QDir::cleanPath(homeDir() + QStringLiteral("/.workbuddy-ai")),
    };
    return firstExistingDir(candidates);
}

bool isWorkbuddyRoot(const QString& dir)
{
    if (QFileInfo(dir + QStringLiteral("/WorkBuddy.exe")).isFile()) return true;
    if (QFileInfo(dir + QStringLiteral("/resources/app.asar.unpacked/cli/product.json")).isFile()) return true;
    return false;
}

QString workbuddyInstallDir()
{
    // 1) LOCALAPPDATA/Programs/WorkBuddy
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    if (!local.isEmpty()) {
        const QString c1 = QDir::cleanPath(local + QStringLiteral("/Programs/WorkBuddy"));
        if (isWorkbuddyRoot(c1)) return c1;
    }
    // 2) Program Files
    const QString pf = qEnvironmentVariable("ProgramFiles");
    if (!pf.isEmpty()) {
        const QString c2 = QDir::cleanPath(pf + QStringLiteral("/WorkBuddy"));
        if (isWorkbuddyRoot(c2)) return c2;
    }
    // 3) Scan all drive letters for common install paths
    static const char* kSuffixes[] = {
        "/123/WorkBuddy",
        "/Program Files/WorkBuddy",
        "/Programs/WorkBuddy",
    };
    for (char letter = 'C'; letter <= 'Z'; ++letter) {
        for (const char* suffix : kSuffixes) {
            const QString candidate = QDir::cleanPath(
                QString(QLatin1Char(letter)) + QLatin1Char(':') + QLatin1String(suffix));
            if (isWorkbuddyRoot(candidate)) return candidate;
        }
    }
    // 4) Registry: check Uninstall keys for WorkBuddy
    {
        auto checkReg = [](const QString& root) -> QString {
            QSettings reg(root + QStringLiteral("\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
                          QSettings::NativeFormat);
            for (const QString& key : reg.childGroups()) {
                reg.beginGroup(key);
                const QString name = reg.value(QStringLiteral("DisplayName")).toString();
                if (name.contains(QStringLiteral("WorkBuddy"), Qt::CaseInsensitive)) {
                    QString loc = reg.value(QStringLiteral("InstallLocation")).toString();
                    reg.endGroup();
                    if (!loc.isEmpty() && isWorkbuddyRoot(loc)) return loc;
                } else {
                    reg.endGroup();
                }
            }
            return {};
        };
        QString loc = checkReg(QStringLiteral("HKEY_LOCAL_MACHINE"));
        if (loc.isEmpty()) loc = checkReg(QStringLiteral("HKEY_CURRENT_USER"));
        if (!loc.isEmpty()) return loc;
    }
    return {};
}

} // namespace

InstallPlan planFor(const Item& item)
{
    InstallPlan plan;
    plan.maxChars = item.maxChars;

    if (item.id == QLatin1String("cursor")) {
        const QString dir = firstExistingDir(cursorRuleDirs());
        plan.targets.push_back({QDir::cleanPath(dir + QStringLiteral("/Ua.mdc")),
                                TargetKind::Marked});

        // Sync to .cursorrules if it already exists (don't create it)
        const QString cursorrules = QDir::cleanPath(homeDir() + QStringLiteral("/.cursorrules"));
        if (QFileInfo::exists(cursorrules)) {
            plan.targets.push_back({cursorrules, TargetKind::Marked});
        }

        plan.skillsDir = QDir::cleanPath(homeDir() + QStringLiteral("/.cursor/skills"));
        return plan;
    }

    if (item.id == QLatin1String("workbuddy")) {
        const QString cfg = workbuddyConfigDir();

        // 1) SOUL.md — the persona file IdentityCollector reads into SoulContent
        //    on every session (10k char cap). This is the only channel that is
        //    always in the prompt; the memory files below are secondary.
        plan.targets.push_back({QDir::cleanPath(cfg + QStringLiteral("/SOUL.md")),
                                TargetKind::Marked});

        // 2) Account-level memory archives: one file per logged-in uid. The app
        //    treats these as a cache of the server profile and only falls back
        //    to them when the remote profile is empty — kept read-only so the
        //    cloud cannot blank our block.
        QDir memDir(cfg + QStringLiteral("/memory"));
        const QFileInfoList archives =
            memDir.entryInfoList({QStringLiteral("*_memory.md")}, QDir::Files, QDir::Time);
        for (const QFileInfo& fi : archives) {
            plan.targets.push_back({fi.absoluteFilePath(), TargetKind::MemoryArchive});
        }

        // 3) User-level file memory: the fallback the agent is told to read.
        plan.targets.push_back({QDir::cleanPath(cfg + QStringLiteral("/MEMORY.md")),
                                TargetKind::WholeFile});

        // 3b) user-personal SOUL.md: the AI persona/identity file (per-UID dir).
        {
            QDirIterator personalIt(cfg, {QStringLiteral("user-*-personal")},
                                    QDir::Dirs, QDirIterator::NoIteratorFlags);
            while (personalIt.hasNext()) {
                personalIt.next();
                const QString soul = QDir::cleanPath(
                    personalIt.filePath() + QStringLiteral("/SOUL.md"));
                if (QFileInfo(soul).isFile()) {
                    plan.targets.push_back({soul, TargetKind::WholeFile});
                }
            }
        }

        // 4) Install-dir deep targets: replace <content_policy> blocks in
        //    templates, product.json, and welcomemode plugin templates.
        const QString installDir = workbuddyInstallDir();
        if (!installDir.isEmpty()) {
            const QString unpacked = QDir::cleanPath(
                installDir + QStringLiteral("/resources/app.asar.unpacked"));

            // a) resources/templates/*.tpl
            {
                QDir tplDir(unpacked + QStringLiteral("/resources/templates"));
                const QFileInfoList tpls = tplDir.entryInfoList(
                    {QStringLiteral("*.tpl")}, QDir::Files);
                for (const QFileInfo& fi : tpls) {
                    plan.targets.push_back({fi.absoluteFilePath(),
                                            TargetKind::PolicyReplace});
                }
            }

            // b) cli/product.json
            {
                const QString pj = QDir::cleanPath(
                    unpacked + QStringLiteral("/cli/product.json"));
                if (QFileInfo(pj).isFile()) {
                    plan.targets.push_back({pj, TargetKind::PolicyReplace});
                }
            }

            // c) plugins/workbuddy-builtin/welcomemode/*/prompt.tpl
            {
                QDirIterator it(
                    unpacked + QStringLiteral("/resources/plugins/workbuddy-builtin/welcomemode"),
                    {QStringLiteral("prompt.tpl")}, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    it.next();
                    plan.targets.push_back({it.filePath(),
                                            TargetKind::PolicyReplace});
                }
            }

            // c2) Plugin cache: WorkBuddy loads templates from here at runtime,
            //     not from the install dir. Versioned subdirectories.
            {
                QDirIterator cacheIt(
                    cfg + QStringLiteral("/plugins/cache/workbuddy-builtin"),
                    {QStringLiteral("prompt.tpl")}, QDir::Files, QDirIterator::Subdirectories);
                while (cacheIt.hasNext()) {
                    cacheIt.next();
                    plan.targets.push_back({cacheIt.filePath(),
                                            TargetKind::PolicyReplace});
                }
            }

            // c3) Interaction mode fragments in plugin cache (contain content_policy too).
            // Narrow to fragment files that are known to carry policy blocks — reading
            // every .md on scan would stall the UI (WorkBuddy's cache has thousands).
            {
                static const QStringList kFragmentNames = {
                    QStringLiteral("interaction.md"),
                    QStringLiteral("mode-behavior.md"),
                    QStringLiteral("current-mode.md"),
                    QStringLiteral("agent-loop.md"),
                    QStringLiteral("memory-system.md"),
                    QStringLiteral("memory-context.md"),
                };
                QDirIterator fragIt(
                    cfg + QStringLiteral("/plugins/cache/workbuddy-builtin"),
                    kFragmentNames, QDir::Files, QDirIterator::Subdirectories);
                while (fragIt.hasNext()) {
                    fragIt.next();
                    // Injector's PolicyReplace case is a no-op when <content_policy>
                    // isn't present, so we don't need to pre-peek the file content.
                    plan.targets.push_back({fragIt.filePath(),
                                            TargetKind::PolicyReplace});
                }
            }

            // e) CLI JS file: command security gates to unlock
            {
                const QString mjs = QDir::cleanPath(
                    unpacked + QStringLiteral("/cli/dist/codebuddy-lite-wb.mjs"));
                if (QFileInfo(mjs).isFile()) {
                    plan.targets.push_back({mjs, TargetKind::JsPatch});
                }
            }

            // f) Broker program policy check: always allow
            {
                const QString cjs = QDir::cleanPath(
                    unpacked + QStringLiteral("/cli/vendor/shim/broker-program-policy-check.cjs"));
                if (QFileInfo(cjs).isFile()) {
                    plan.targets.push_back({cjs, TargetKind::JsPatch});
                }
            }

            // g) Sandbox rules: relax file deny rules
            {
                QDirIterator sandboxIt(
                    unpacked + QStringLiteral("/cli/vendor/sandbox"),
                    {QStringLiteral("tsbx_rules.json")}, QDir::Files, QDirIterator::Subdirectories);
                while (sandboxIt.hasNext()) {
                    sandboxIt.next();
                    plan.targets.push_back({sandboxIt.filePath(), TargetKind::JsPatch});
                }
            }

            // d) Runtime caches: patch content_policy in the config cache too,
            //    then mark it for deletion so WorkBuddy rebuilds from patched sources.
            {
                const QString cacheJson = QDir::cleanPath(
                    cfg + QStringLiteral("/cache/acc-product-config-v3.json"));
                if (QFileInfo(cacheJson).isFile()) {
                    plan.targets.push_back({cacheJson, TargetKind::PolicyReplace});
                    plan.cacheFiles << cacheJson;
                }

                // conversation-product-spill: per-session config cache copies
                QDir spillDir(cfg + QStringLiteral("/cache/conversation-product-spill"));
                const QFileInfoList spills = spillDir.entryInfoList(
                    {QStringLiteral("acc-product-config-v3-*.json")}, QDir::Files);
                for (const QFileInfo& fi : spills) {
                    plan.targets.push_back({fi.absoluteFilePath(), TargetKind::PolicyReplace});
                    plan.cacheFiles << fi.absoluteFilePath();
                }

                QDir pluginCache(cfg + QStringLiteral("/plugins/cache"));
                const QFileInfoList jsons = pluginCache.entryInfoList(
                    {QStringLiteral("*.json")}, QDir::Files);
                for (const QFileInfo& fi : jsons)
                    plan.cacheFiles << fi.absoluteFilePath();

                // TEMP session snapshots: workbuddy-product-spill-*/acc-product-config-v3.json
                const QString tempDir = QDir::tempPath();
                QDirIterator spillIt(tempDir, {QStringLiteral("workbuddy-product-spill-*")},
                                     QDir::Dirs, QDirIterator::NoIteratorFlags);
                while (spillIt.hasNext()) {
                    spillIt.next();
                    const QString spillJson = QDir::cleanPath(
                        spillIt.filePath() + QStringLiteral("/acc-product-config-v3.json"));
                    if (QFileInfo(spillJson).isFile()) {
                        plan.targets.push_back({spillJson, TargetKind::PolicyReplace});
                        plan.cacheFiles << spillJson;
                    }
                }
            }
        }

        plan.skillsDir      = QDir::cleanPath(cfg + QStringLiteral("/skills"));
        plan.skillsSettings = QDir::cleanPath(cfg + QStringLiteral("/settings.json"));
        plan.appConfig      = QDir::cleanPath(cfg + QStringLiteral("/app/app-config.json"));
        plan.injectSkillIndex = true;   // WorkBuddy leaves discovery to the model
        return plan;
    }

    if (item.id == QLatin1String("codex")) {
        // Two channels on purpose: Codex rewrites config.toml itself and drops
        // unknown keys, so the pointer alone is not durable; AGENTS.md is read
        // natively and survives that rewrite.
        const QString home = QDir::cleanPath(homeDir() + QStringLiteral("/.codex"));
        const QString managed = QDir::cleanPath(home + QStringLiteral("/managed-prompts/Ua.md"));
        plan.targets.push_back({QDir::cleanPath(home + QStringLiteral("/AGENTS.md")),
                                TargetKind::Marked});
        plan.targets.push_back({managed, TargetKind::WholeFile});
        plan.tomlFile      = QDir::cleanPath(home + QStringLiteral("/config.toml"));
        plan.pointerTarget = managed;
        plan.skillsDir     = QDir::cleanPath(home + QStringLiteral("/skills"));
        return plan;
    }

    if (item.id == QLatin1String("zcode")) {
        // ZCode keeps a global memory file of its own; writing it covers the
        // memory channel while AGENTS.md covers the instruction channel.
        plan.targets.push_back({targetPath(QStringLiteral(".zcode/AGENTS.md")),
                                TargetKind::Marked});
        plan.targets.push_back({targetPath(QStringLiteral(".zcode/cli/memories/global/memory/MEMORY.md")),
                                TargetKind::WholeFile});

        // ZCode install dir: probe registry, common paths, all drives
        QString zcodeInstall;
        {
            // Check common install locations
            static const char* kPaths[] = {
                "/zcide/ZCode",
                "/Program Files/ZCode",
                "/Programs/ZCode",
            };
            auto isZcodeRoot = [](const QString& d) {
                return QFileInfo(d + QStringLiteral("/ZCode.exe")).isFile()
                    || QFileInfo(d + QStringLiteral("/resources/glm/zcode.cjs")).isFile();
            };
            const QString local = qEnvironmentVariable("LOCALAPPDATA");
            if (!local.isEmpty()) {
                const QString c = QDir::cleanPath(local + QStringLiteral("/Programs/ZCode"));
                if (isZcodeRoot(c)) zcodeInstall = c;
            }
            if (zcodeInstall.isEmpty()) {
                const QString pf = qEnvironmentVariable("ProgramFiles");
                if (!pf.isEmpty()) {
                    const QString c = QDir::cleanPath(pf + QStringLiteral("/ZCode"));
                    if (isZcodeRoot(c)) zcodeInstall = c;
                }
            }
            if (zcodeInstall.isEmpty()) {
                for (char letter = 'C'; letter <= 'Z'; ++letter) {
                    for (const char* suffix : kPaths) {
                        const QString c = QDir::cleanPath(
                            QString(QLatin1Char(letter)) + QLatin1Char(':') + QLatin1String(suffix));
                        if (isZcodeRoot(c)) { zcodeInstall = c; break; }
                    }
                    if (!zcodeInstall.isEmpty()) break;
                }
            }
            // 4) Registry: check Uninstall keys for ZCode
            if (zcodeInstall.isEmpty()) {
                auto checkReg = [&isZcodeRoot](const QString& root) -> QString {
                    QSettings reg(root + QStringLiteral("\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
                                  QSettings::NativeFormat);
                    for (const QString& key : reg.childGroups()) {
                        reg.beginGroup(key);
                        const QString name = reg.value(QStringLiteral("DisplayName")).toString();
                        if (name.contains(QStringLiteral("ZCode"), Qt::CaseInsensitive)) {
                            QString loc = reg.value(QStringLiteral("InstallLocation")).toString();
                            reg.endGroup();
                            if (!loc.isEmpty() && isZcodeRoot(loc)) return loc;
                        } else {
                            reg.endGroup();
                        }
                    }
                    return {};
                };
                zcodeInstall = checkReg(QStringLiteral("HKEY_LOCAL_MACHINE"));
                if (zcodeInstall.isEmpty()) zcodeInstall = checkReg(QStringLiteral("HKEY_CURRENT_USER"));
            }
        }

        // JS patch: system prompt identity + security notice in zcode.cjs
        if (!zcodeInstall.isEmpty()) {
            const QString cjs = QDir::cleanPath(
                zcodeInstall + QStringLiteral("/resources/glm/zcode.cjs"));
            if (QFileInfo(cjs).isFile()) {
                plan.targets.push_back({cjs, TargetKind::JsPatch});
            }
        }

        plan.skillsDir = skillsDir(QStringLiteral(".zcode/skills"));
        return plan;
    }

    if (item.id == QLatin1String("deepseek")) {
        const QString dsh = QDir::cleanPath(homeDir() + QStringLiteral("/.dsh"));

        // 1) AGENTS.md — standard marker block injection (workspace instructions)
        plan.targets.push_back({QDir::cleanPath(dsh + QStringLiteral("/AGENTS.md")),
                                TargetKind::Marked});

        // 2) prompt-inject.md — only if dsh-purge plugin is installed
        //    (dsh-purge reads this file as the persona injection source;
        //     without the plugin, nobody reads it)
        const QString dshPurge = QDir::cleanPath(dsh + QStringLiteral("/plugins/dsh-purge"));
        if (QFileInfo(dshPurge).isDir()) {
            plan.targets.push_back({QDir::cleanPath(dsh + QStringLiteral("/prompt-inject.md")),
                                    TargetKind::WholeFile});
        }

        plan.skillsDir = QDir::cleanPath(dsh + QStringLiteral("/skills"));
        return plan;
    }

    if (!item.targetRel.isEmpty()) {
        plan.targets.push_back({targetPath(item.targetRel), TargetKind::Marked});
    }
    if (!item.skillsDirRel.isEmpty()) {
        plan.skillsDir = skillsDir(item.skillsDirRel);
    }
    return plan;
}

} // namespace ua::core
