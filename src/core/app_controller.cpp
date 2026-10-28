// app_controller.cpp - QML-facing controller

#include "app_controller.h"

#include "items.h"
#include "injector.h"
#include "paths.h"
#include "prompts.h"
#include "skills.h"
#include "strings.h"

#include <QAction>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>

namespace ua::core {


namespace {
// Products that ship a usage guide.
bool hasTip(const QString& id)
{
    return id == QLatin1String("claude")
        || id == QLatin1String("deepseek")
        || id == QLatin1String("workbuddy")
        || id == QLatin1String("codex")
        || id == QLatin1String("cursor")
        || id == QLatin1String("windsurf")
        || id == QLatin1String("zcode")
        || id == QLatin1String("opencode");
}

// Product-specific paragraphs for the template-driven guides.
QString noteFor(const QString& id)
{
    if (id == QLatin1String("codex"))
        return QStringLiteral("  Codex 走双通道：AGENTS.md（原生）+ config.toml 的 model_instructions_file 指针。\n"
                              "  Codex 会自己重写 config.toml，指针可能丢，这时 AGENTS.md 兜底。新开会话生效。");
    if (id == QLatin1String("cursor"))
        return QStringLiteral("  规则必须是 .mdc 且带 front-matter（description / globs / alwaysApply），否则 Cursor 不认。\n"
                              "  改完在 Cursor 里 Reload Window；也可在 Settings → Rules 里确认规则已加载。");
    if (id == QLatin1String("windsurf"))
        return QStringLiteral("  Windsurf 只读一个全局规则文件，上限 6000 字符，超了会被静默截断。\n"
                              "  提示词太长就先精简；改完重启应用。");
    if (id == QLatin1String("zcode"))
        return QStringLiteral("  ZCode 读 ~/.zcode/AGENTS.md；另有一份全局记忆\n"
                              "  ~/.zcode/cli/memories/global/memory/MEMORY.md。新开会话生效。");
    if (id == QLatin1String("opencode"))
        return QStringLiteral("  OpenCode 读 ~/.config/opencode/AGENTS.md。\n"
                              "  它也兼容 ~/.claude/skills 作为技能兜底，所以技能目录为空时不一定报错。");
    return QString();
}

} // namespace

AppController::AppController(QObject* parent) : QObject(parent)
{
    Prompts::extractEmbedded();
    syncTips();
    setupTray();

    // Translated card labels: refresh the items snapshot whenever the UI
    // language flips, so the Tools-tab tiles swap CN/EN live.
    connect(Strings::instance(), &Strings::langChanged,
            this, &AppController::itemsChanged);

    // Install progress -> QML signal. install() runs synchronously on the GUI
    // thread so mid-install signals only land in a later event loop iteration,
    // but AppController::install() posts a start/end toast around the call so
    // users at least see "patching N files..." before the UI freezes.
    Injector::setProgressCallback(
        [this](int cur, int tot, const QString& path) {
            emit installProgress(cur, tot, path);
        });
}

AppController::~AppController() = default;

void AppController::attachMainWindow(QWindow* window)
{
    m_mainWindow = window;
    if (!window) return;
    // Intercept the X button so it hides to tray instead of closing the app.
    connect(window, &QWindow::visibleChanged, this, [this](bool v) {
        if (!v && m_tray && m_tray->isVisible()) {
            // Window got hidden: don't quit Qt, keep the tray alive.
        }
    });
}

void AppController::showMainWindow()
{
    if (!m_mainWindow) return;
    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->requestActivate();
    emit showMainWindowRequested();
}

void AppController::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(QStringLiteral(":/icons/Ua.ico")));
    m_tray->setToolTip(Strings::instance()->get(Strings::TrayTooltip));

    m_trayMenu = new QMenu();
    QAction* showAction = m_trayMenu->addAction(Strings::instance()->get(Strings::TrayShow));
    m_trayMenu->addSeparator();
    QAction* exitAction = m_trayMenu->addAction(Strings::instance()->get(Strings::Exit));

    connect(showAction, &QAction::triggered, this, &AppController::showMainWindow);
    connect(exitAction, &QAction::triggered, this, &AppController::quit);

    connect(m_tray, &QSystemTrayIcon::activated,
            this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showMainWindow();
        }
    });

    // Keep the tooltip in sync with language switches.
    connect(Strings::instance(), &Strings::langChanged, this, [this, showAction, exitAction]() {
        if (m_tray)      m_tray->setToolTip(Strings::instance()->get(Strings::TrayTooltip));
        if (showAction)  showAction->setText(Strings::instance()->get(Strings::TrayShow));
        if (exitAction)  exitAction->setText(Strings::instance()->get(Strings::Exit));
    });

    m_tray->setContextMenu(m_trayMenu);
    m_tray->show();

    // Qt otherwise quits when the last visible window closes; we want to stay
    // alive in the tray.
    QGuiApplication::setQuitOnLastWindowClosed(false);
}

QString AppController::version() const
{
    return QCoreApplication::applicationVersion();
}

QVariantList AppController::buildItemsSnapshot() const
{
    QVariantList out;
    out.reserve(allItems().size());
    auto* strings = Strings::instance();
    for (const Item& it : allItems()) {
        // Resolve translated name/sub when the item carries a StringId,
        // falling back to the raw name/sub otherwise (e.g. Claude, DeepSeek).
        const QString name = it.nameId >= 0 ? strings->get(static_cast<Strings::StringId>(it.nameId)) : it.name;
        const QString sub  = it.subId  >= 0 ? strings->get(static_cast<Strings::StringId>(it.subId))  : it.sub;
        QVariantMap m;
        m[QStringLiteral("id")]              = it.id;
        m[QStringLiteral("name")]            = name;
        m[QStringLiteral("sub")]             = sub;
        m[QStringLiteral("group")]           = static_cast<int>(it.group);
        m[QStringLiteral("logo")]            = it.logo;
        m[QStringLiteral("framed")]          = it.framed;
        m[QStringLiteral("glyph")]           = it.glyph;
        m[QStringLiteral("url")]             = it.url;
        m[QStringLiteral("targetRel")]       = it.targetRel;
        m[QStringLiteral("skillsDirRel")]    = it.skillsDirRel;
        m[QStringLiteral("isSkillsManager")] = it.isSkillsManager;
        m[QStringLiteral("hint")]            = it.hintId >= 0 ? strings->get(static_cast<Strings::StringId>(it.hintId)) : QString();
        m[QStringLiteral("hasTips")]         = hasTip(it.id);
        m[QStringLiteral("hidden")]          = it.hidden;
        m[QStringLiteral("supported")]       = !it.targetRel.isEmpty();
        m[QStringLiteral("detected")]        = toolDetected(it.targetRel);
        const auto st = Injector::scan(it.id);
        m[QStringLiteral("state")]           = static_cast<int>(st);
        m[QStringLiteral("upgraded")]        = (st == InstallState::Installed) && Injector::hostUpgraded(it.id);
        out.push_back(m);
    }
    return out;
}

const QVariantList& AppController::cachedSnapshot() const
{
    if (m_snapshotDirty) {
        m_snapshotCache = buildItemsSnapshot();
        m_snapshotDirty = false;
    }
    return m_snapshotCache;
}

void AppController::invalidateSnapshot()
{
    m_snapshotDirty = true;
}

QVariantList AppController::items()
{
    return cachedSnapshot();
}

QVariantList AppController::editorItems()
{
    QVariantList out;
    for (const QVariant& v : cachedSnapshot()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("hidden")).toBool()) continue;
        if (m.value(QStringLiteral("group")).toInt() == static_cast<int>(ItemGroup::Editor)) {
            out.push_back(v);
        }
    }
    return out;
}

QVariantList AppController::toolsItems()
{
    QVariantList out;
    for (const QVariant& v : cachedSnapshot()) {
        if (v.toMap().value(QStringLiteral("group")).toInt() == static_cast<int>(ItemGroup::Tools)) {
            out.push_back(v);
        }
    }
    return out;
}

QVariantList AppController::prompts()
{
    QVariantList out;
    for (const auto& e : Prompts::list()) {
        QVariantMap m;
        m[QStringLiteral("name")]     = e.name;
        m[QStringLiteral("fullPath")] = e.fullPath;
        out.push_back(m);
    }
    return out;
}

QVariantList AppController::promptsFor(const QString& itemId)
{
    QVariantList out;
    for (const auto& e : Prompts::listFor(itemId)) {
        QVariantMap m;
        m[QStringLiteral("name")]     = e.name;
        m[QStringLiteral("fullPath")] = e.fullPath;
        out.push_back(m);
    }
    return out;
}

QVariantList AppController::envCheck()
{
    QVariantList out;
    auto* strings = Strings::instance();
    const bool cn = strings->lang() == Strings::CN;

    for (const Item& it : allItems()) {
        if (it.group != ItemGroup::Editor) continue;
        if (it.targetRel.isEmpty()) continue;

        const QString name = it.nameId >= 0
            ? strings->get(static_cast<Strings::StringId>(it.nameId)) : it.name;
        const bool detected = toolDetected(it.targetRel);
        const QString target = targetPath(it.targetRel);
        const bool targetExists = QFileInfo::exists(target);
        const bool targetWritable = targetExists
            ? QFileInfo(target).isWritable()
            : QFileInfo(QFileInfo(target).absolutePath()).isWritable();
        const auto state = Injector::scan(it.id);

        int skillCount = 0;
        if (!it.skillsDirRel.isEmpty()) {
            const QString sd = skillsDir(it.skillsDirRel);
            QDir d(sd);
            if (d.exists()) {
                skillCount = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
            }
        }

        QString installDir;
        if (detected) {
            QString rel = it.targetRel;
            rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
            const int slash = rel.indexOf(QLatin1Char('/'));
            if (slash > 0) {
                installDir = homeDir() + QLatin1Char('/') + rel.left(slash);
            }
        }

        QString stateLabel;
        switch (state) {
        case InstallState::Installed:    stateLabel = cn ? QStringLiteral("已安装") : QStringLiteral("Installed"); break;
        case InstallState::ActiveNoState:stateLabel = cn ? QStringLiteral("有内容") : QStringLiteral("Has content"); break;
        default:                         stateLabel = cn ? QStringLiteral("未安装") : QStringLiteral("Not installed"); break;
        }

        QVariantMap m;
        m[QStringLiteral("id")]             = it.id;
        m[QStringLiteral("name")]           = name;
        m[QStringLiteral("detected")]       = detected;
        m[QStringLiteral("installDir")]     = installDir;
        m[QStringLiteral("targetPath")]     = target;
        m[QStringLiteral("targetExists")]   = targetExists;
        m[QStringLiteral("targetWritable")] = targetWritable;
        m[QStringLiteral("state")]          = static_cast<int>(state);
        m[QStringLiteral("stateLabel")]     = stateLabel;
        m[QStringLiteral("skillsDir")]      = it.skillsDirRel.isEmpty() ? QString() : skillsDir(it.skillsDirRel);
        m[QStringLiteral("skillCount")]     = skillCount;
        out.push_back(m);
    }
    return out;
}

void AppController::refreshStates()
{
    invalidateSnapshot();
    invalidateSnapshot();
    emit itemsChanged();
}

void AppController::refreshPrompts()
{
    emit promptsChanged();
}

void AppController::openUrl(const QString& url)
{
    if (url.isEmpty()) return;
    QDesktopServices::openUrl(QUrl(url));
}

void AppController::revealTargetDir(const QString& itemId)
{
    const Item* it = itemById(itemId);
    if (!it) return;
    const QString full = targetPath(it->targetRel);
    const QFileInfo info(full);
    QDir().mkpath(info.absolutePath());
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
}


QString AppController::tipFor(const Item& item) const
{
    // Hand-written minimal guides: for these two the only things a user needs
    // are "did it install" and (for Claude) "which model to pick".
    const QString verifySteps =
        QStringLiteral(
            "  安装完成后的验证（三步）：\n"
            "    1. 新建一个会话窗口（旧窗口不加载新提示词）\n"
            "    2. 输入启动命令：ua\n"
            "    3. 回「Ua 已经准备好随时待命」＝ 安装成功，可以用了\n");

    if (item.id == QLatin1String("claude")) {
        return QStringLiteral("Claude 使用技巧\n\n")
               + verifySteps
               + QStringLiteral(
                     "\n"
                     "  模型怎么选：只用 opus4.8 以下的模型。\n"
                     "  opus4.8 及以上带云审核，任务会被拦，别浪费额度。\n");
    }

    if (item.id == QLatin1String("deepseek")) {
        return QStringLiteral("DeepSeek Harness 使用技巧\n\n") + verifySteps;
    }

    if (item.id == QLatin1String("workbuddy")) {
        return QStringLiteral("WorkBuddy 使用技巧\n\n")
               + QStringLiteral(
                     "  安装完成后的验证（三步）：\n"
                     "    1. 完全退出 WorkBuddy 再重新打开（关窗口只是最小化，托盘右键退出）\n"
                     "    2. 新建一个会话窗口（旧会话不加载新提示词）\n"
                     "    3. 输入启动命令：ua，回「Ua 已经准备好随时待命」＝ 安装成功，可以用了\n"
                     "    4. 部分能破不是全破的\n"
                     "  技能不生效时：让它先读 ~/.workbuddy/skills/INDEX.md 再动手。\n");
    }

    QFile in(QStringLiteral(":/docs/tips.txt"));
    if (!in.open(QIODevice::ReadOnly)) return {};

    QString text = QString::fromUtf8(in.readAll());
    auto* strings = Strings::instance();

    const QString name = item.nameId >= 0
                             ? strings->get(static_cast<Strings::StringId>(item.nameId))
                             : item.name;
    const QString effect = item.hintId >= 0
                               ? strings->get(static_cast<Strings::StringId>(item.hintId))
                               : QStringLiteral("重启应用后生效");

    text.replace(QStringLiteral("{{PRODUCT}}"), name);
    text.replace(QStringLiteral("{{EFFECT}}"), effect);
    text.replace(QStringLiteral("{{TARGET}}"),
                 item.targetRel.isEmpty() ? QStringLiteral("（该产品不走文件注入）")
                                          : QStringLiteral("~/") + item.targetRel);
    text.replace(QStringLiteral("{{SKILLS}}"),
                 item.skillsDirRel.isEmpty() ? QStringLiteral("（该产品不支持技能）")
                                             : QStringLiteral("~/") + item.skillsDirRel);
    text.replace(QStringLiteral("{{NOTES}}"), noteFor(item.id));
    text.replace(QStringLiteral("{{RESTART_NOTE}}"),
                 item.id == QLatin1String("claude")
                     ? QString()
                     : QStringLiteral("  通用：改过提示词或技能后，完全退出该应用 → 重开 → 新开会话（旧会话不加载新内容）"));
    return text;
}

QString AppController::syncTips()
{
    const QString dir = configDir() + QStringLiteral("/docs");
    QDir().mkpath(dir);

    for (const Item& it : allItems()) {
        if (it.group != ItemGroup::Editor || it.targetRel.isEmpty()) continue;
        if (!hasTip(it.id)) continue;
        const QString body = tipFor(it);
        if (body.isEmpty()) continue;

        const QString dst = dir + QStringLiteral("/tips-") + it.id + QStringLiteral(".txt");
        QFile existing(dst);
        bool differs = true;
        if (existing.open(QIODevice::ReadOnly)) {
            differs = existing.readAll() != body.toUtf8();
            existing.close();
        }
        if (!differs) continue;

        QFile out(dst);
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) out.write(body.toUtf8());
    }
    return dir;
}

void AppController::openTip(const QString& itemId)
{
    syncTips();
    const Item* it = itemById(itemId);
    const QString path = configDir() + QStringLiteral("/docs/tips-")
                         + (it ? it->id : itemId) + QStringLiteral(".txt");
    if (!QFileInfo::exists(path)) { emit errorRaised(tr("技巧文档生成失败")); return; }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool AppController::install(const QString& itemId, const QString& promptFile)
{
    // Injector owns the whole install now: every target file in the product's
    // plan plus that product's skill packs and config switches.
    // Nudge a toast + paint before the GUI thread blocks inside Injector::install()
    // so the user at least sees "patching..." instead of a frozen window.
    const Item* it = itemById(itemId);
    const QString displayName = it ? (it->nameId >= 0
                                      ? Strings::instance()->get(static_cast<Strings::StringId>(it->nameId))
                                      : it->name)
                                   : itemId;
    const QString cn = Strings::instance()->lang() == Strings::CN
                       ? QStringLiteral("正在为 %1 应用补丁...").arg(displayName)
                       : QStringLiteral("Patching %1...").arg(displayName);
    emit toast(cn);
    QCoreApplication::processEvents();

    const auto r = Injector::install(itemId, promptFile);
    invalidateSnapshot();
    emit itemsChanged();
    if (!r.ok) emit errorRaised(r.error);
    return r.ok;
}

bool AppController::uninstall(const QString& itemId)
{
    const auto r = Injector::uninstall(itemId);
    invalidateSnapshot();
    emit itemsChanged();
    if (!r.ok) emit errorRaised(r.error);
    return r.ok;
}

QVariantList AppController::skills()
{
    QVariantList out;
    for (const auto& s : Skills::statuses()) {
        QVariantMap m;
        m[QStringLiteral("dirName")]        = s.dirName;
        m[QStringLiteral("installedCount")] = s.installedCount;
        m[QStringLiteral("category")]       = s.category;
        out.push_back(m);
    }
    return out;
}

bool AppController::removeSkill(const QString& skillDirName)
{
    const int n = Skills::uninstallByName(skillDirName);
    invalidateSnapshot();
    emit itemsChanged();
    return n > 0;
}

void AppController::openSkillsFolder()
{
    for (const Item& it : allItems()) {
        if (it.skillsDirRel.isEmpty()) continue;
        const QString dir = skillsDir(it.skillsDirRel);
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        return;
    }
}

void AppController::showToast(const QString& text)
{
    emit toast(text);
}

void AppController::quit()
{
    QCoreApplication::quit();
}

} // namespace ua::core
