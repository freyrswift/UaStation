// app_controller.cpp - QML-facing controller

#include "app_controller.h"

#include "items.h"
#include "injector.h"
#include "install_plan.h"
#include "paths.h"
#include "prompts.h"
#include "skills.h"
#include "app_strings.h"

#include <QAction>
#include <QCoreApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QScopedValueRollback>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>
#include <QtConcurrent/QtConcurrent>

namespace ua::core {


namespace {
// Products that ship a usage guide.
bool hasTip(const QString& id)
{
    // All Editor-group tools with a real injection target get a tip page.
    const Item* it = itemById(id);
    return it && it->group == ItemGroup::Editor && !it->targetRel.isEmpty();
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
            this, &AppController::refreshStates);

    // Worker-thread progress is delivered to QML through queued connections.
    Injector::setProgressCallback(
        [this](int cur, int tot, const QString& path) {
            emit installProgress(cur, tot, path);
        });
}

AppController::~AppController() = default;

void AppController::attachMainWindow(QWindow* window)
{
    m_mainWindow = window;
    if (window) {
        connect(window, &QWindow::activeChanged, this, [this, window]() {
            if (window->isActive() && !m_operationBusy) refreshStates();
        });
    }
}

bool AppController::trayAvailable() const
{
    return m_tray && m_tray->isVisible() && QSystemTrayIcon::isSystemTrayAvailable();
}

void AppController::showMainWindow()
{
    if (!m_mainWindow) return;
    if (!m_operationBusy) refreshStates();
    const Qt::WindowStates state = m_mainWindow->windowState();
    if (state & Qt::WindowMinimized) {
        if (state & Qt::WindowMaximized) m_mainWindow->showMaximized();
        else m_mainWindow->showNormal();
    } else if (state & Qt::WindowMaximized) {
        m_mainWindow->showMaximized();
    } else {
        m_mainWindow->show();
    }
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
        m[QStringLiteral("supported")]       = !it.targetRel.isEmpty() || it.id == QLatin1String("doubao");
        m[QStringLiteral("detected")]        = toolDetectedById(it.id);
        if (it.id == QLatin1String("vscode")) {
            m[QStringLiteral("editorInstallDir")] = m.value(QStringLiteral("detected")).toBool()
                ? vscodeInstallDir() : QString();
            m[QStringLiteral("instructionPath")] = toolPath(it.id, it.targetRel);
            const QString root = planFor(it).skillsDir;
            int count = 0;
            for (const auto& name : Skills::installedNames(it.id)) {
                if (QFileInfo(root + '/' + name + QStringLiteral("/SKILL.md")).isFile()) ++count;
            }
            m[QStringLiteral("sharedSkillsRoot")] = root;
            m[QStringLiteral("sharedSkillsCount")] = count;
            m[QStringLiteral("sharedSkillsTotal")] = allSkills().size();
        }
        const auto st = Injector::scan(it.id);
        m[QStringLiteral("state")]           = static_cast<int>(st);
        m[QStringLiteral("upgraded")]        = (st == InstallState::Installed || st == InstallState::NeedsRepair) && Injector::hostUpgraded(it.id);
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
        const bool detected = toolDetectedById(it.id);
        const InstallPlan plan = planFor(it);
        const QString target = plan.valid() ? plan.targets.first().path : QString();
        const bool targetExists = QFileInfo::exists(target);
        QString writablePath = target;
        while (!writablePath.isEmpty() && !QFileInfo::exists(writablePath)) {
            const QString parent = QFileInfo(writablePath).absolutePath();
            if (parent == writablePath) break;
            writablePath = parent;
        }
        const bool targetWritable = !writablePath.isEmpty() && QFileInfo(writablePath).isWritable();
        const auto state = Injector::scan(it.id);

        int skillCount = 0;
        const QString sd = Skills::installedRoot(it.id);
        if (!sd.isEmpty()) {
            QDir d(sd);
            if (d.exists()) {
                skillCount = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
            }
        }

        const QString installDir = detected ? toolConfigRoot(it.id) : QString();

        QString stateLabel;
        switch (state) {
        case InstallState::Installed:    stateLabel = cn ? QStringLiteral("已安装") : QStringLiteral("Installed"); break;
        case InstallState::ActiveNoState:stateLabel = cn ? QStringLiteral("有内容") : QStringLiteral("Has content"); break;
        case InstallState::NeedsRepair: stateLabel = Strings::instance()->get(Strings::NeedsRepair); break;
        default:                         stateLabel = cn ? QStringLiteral("未安装") : QStringLiteral("Not installed"); break;
        }

        QVariantMap m;
        m[QStringLiteral("id")]             = it.id;
        m[QStringLiteral("name")]           = name;
        m[QStringLiteral("detected")]       = detected;
        m[QStringLiteral("installDir")]     = installDir;
        if (it.id == QLatin1String("vscode"))
            m[QStringLiteral("editorInstallDir")] = detected ? vscodeInstallDir() : QString();
        m[QStringLiteral("targetPath")]     = target;
        m[QStringLiteral("targetExists")]   = targetExists;
        m[QStringLiteral("targetWritable")] = targetWritable;
        m[QStringLiteral("state")]          = static_cast<int>(state);
        m[QStringLiteral("stateLabel")]     = stateLabel;
        m[QStringLiteral("skillsDir")]      = sd;
        m[QStringLiteral("skillCount")]     = skillCount;
        out.push_back(m);
    }
    return out;
}

void AppController::refreshStates()
{
    invalidateSnapshot();
    emit itemsChanged();
}

bool AppController::setVscodeLocation(const QString& path)
{
    if (m_operationBusy || m_updateInProgress) return false;
    if (!setVscodeInstallPath(path)) {
        emit errorRaised(Strings::instance()->lang() == Strings::CN
            ? QStringLiteral("请选择 Visual Studio Code 的 Code.exe、Insiders 程序或安装目录")
            : QStringLiteral("Select the Visual Studio Code executable or installation folder"));
        return false;
    }
    refreshStates();
    emit toast(Strings::instance()->lang() == Strings::CN
        ? (path.trimmed().isEmpty() ? QStringLiteral("已恢复 Visual Studio Code 自动检测")
                                   : QStringLiteral("Visual Studio Code 程序路径已更新"))
        : (path.trimmed().isEmpty() ? QStringLiteral("Visual Studio Code automatic detection restored")
                                   : QStringLiteral("Visual Studio Code location updated")));
    return true;
}

void AppController::chooseVscodeLocation()
{
    if (m_operationBusy || m_updateInProgress) return;
    const bool cn = Strings::instance()->lang() == Strings::CN;
    const QString title = cn ? QStringLiteral("选择 Visual Studio Code 程序")
                             : QStringLiteral("Select Visual Studio Code");
    QString selected;
    {
        QScopedValueRollback<bool> busy(m_operationBusy, true);
#if defined(Q_OS_MACOS)
        selected = QFileDialog::getExistingDirectory(nullptr, title, vscodeInstallDir());
#else
        selected = QFileDialog::getOpenFileName(nullptr, title, vscodeInstallDir(),
#if defined(Q_OS_WIN)
            QStringLiteral("Visual Studio Code (Code*.exe);;Executables (*.exe)"));
#else
            QStringLiteral("Visual Studio Code (code code-insiders);;All files (*)"));
#endif
#endif
    }
    if (!selected.isEmpty()) setVscodeLocation(selected);
}

void AppController::refreshPrompts()
{
    emit promptsChanged();
}

bool AppController::restorePrompt(const QString& name)
{
    if (m_operationBusy || m_updateInProgress) return false;
    const bool cn = Strings::instance()->lang() == Strings::CN;
    if (!Prompts::restoreBuiltin(name)) {
        emit errorRaised(cn ? QStringLiteral("没有找到对应的内置提示词")
                            : QStringLiteral("No bundled prompt with that name"));
        return false;
    }
    emit promptsChanged();
    emit toast(cn ? QStringLiteral("已恢复内置提示词，原文件备份为 .bak")
                  : QStringLiteral("Bundled prompt restored; previous file saved as .bak"));
    return true;
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
    const auto plan = planFor(*it);
    if (!plan.valid()) return;
    const QString full = plan.targets.first().path;
    const QString dir = QFileInfo(full).absolutePath();
    // Don't mkpath a non-existent target dir — that would create an empty
    // config folder and make the tool look installed on the next scan.
    if (!QFileInfo::exists(dir)) {
        emit errorRaised(Strings::instance()->lang() == Strings::CN
            ? QStringLiteral("该工具还没安装")
            : QStringLiteral("Tool not installed"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}


QString AppController::tipFor(const Item& item) const
{
    if (item.id == QLatin1String("vscode")) {
        return QStringLiteral(
            "Visual Studio Code 技能\n\n"
            "共享技能目录：%1\n"
            "安装包含 SKILL.md 和随附脚本、参考资料，不是把全部正文塞进一个指令文件。\n"
            "重载窗口并新建会话，在 Chat: Open Customizations 的 Skills 中确认目录；"
            "支持原生技能的客户端可输入 /技能名，或按任务匹配按需加载。\n\n"
            "其他支持 MCP 的插件：在本软件 Visual Studio Code 弹窗复制对应 MCP 配置，"
            "合并到该插件的配置并启动 ua-skills 服务。Visual Studio Code 使用 servers，"
            "通用客户端常用 mcpServers，Codex 使用 TOML。已有配置需合并，不要整份覆盖。\n"
            "连接后先调用 list_skills，再用 read_skill 读取选中的技能；"
            "read_asset 读取它的配套文件，服务本身只读，不执行脚本。\n\n"
            "更换同一客户端的本地/云端模型通常无需重装；更换插件、远程 SSH/WSL/容器环境"
            "需要在对应运行环境部署技能或配置 MCP。仅支持普通聊天、没有文件或 MCP 工具的客户端"
            "不会因为目录存在就自动加载。界面数量表示本地部署数，不代表模型已经使用。\n"
            "共享库由 Visual Studio Code 卡片管理，卸载该卡片也会移除它管理的共享技能，"
            "其他消费者随之失去这些技能；用户原文件按备份恢复。\n").arg(planFor(item).skillsDir);
    }
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
    if (!hasTip(itemId)) {
        emit errorRaised(Strings::instance()->lang() == Strings::CN
            ? QStringLiteral("该工具暂无使用技巧")
            : QStringLiteral("No tips available for this tool"));
        return;
    }
    syncTips();
    const Item* it = itemById(itemId);
    const QString path = configDir() + QStringLiteral("/docs/tips-")
                         + (it ? it->id : itemId) + QStringLiteral(".txt");
    if (!QFileInfo::exists(path)) { emit errorRaised(tr("技巧文档生成失败")); return; }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

namespace {

QString itemDisplayName(const QString& itemId)
{
    const Item* it = itemById(itemId);
    if (!it) return itemId;
    return it->nameId >= 0
        ? Strings::instance()->get(static_cast<Strings::StringId>(it->nameId))
        : it->name;
}

// Run `fn` on a worker thread, keeping the UI event loop spinning so progress
// signals emitted from the worker reach QML via queued connections. Returns
// the operation result once the worker finishes.
template <typename Fn>
auto runInWorker(QObject* parent, Fn fn)
{
    using Result = decltype(fn());
    QFutureWatcher<Result> watcher;
    watcher.setParent(parent);
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcherBase::finished, &loop, &QEventLoop::quit);
    auto future = QtConcurrent::run(std::move(fn));
    watcher.setFuture(future);
    loop.exec();
    return future.result();
}

} // namespace

bool AppController::install(const QString& itemId, const QString& promptFile, bool force)
{
    if (m_operationBusy || m_updateInProgress) return false;
    QScopedValueRollback<bool> busy(m_operationBusy, true);
    const QString displayName = itemDisplayName(itemId);
    emit installStarted(displayName, /*uninstalling=*/false);

    const auto r = runInWorker(this, [itemId, promptFile, force]() {
        return Injector::install(itemId, promptFile, force);
    });

    emit installFinished();
    invalidateSnapshot();
    emit itemsChanged();
    if (!r.ok) emit errorRaised(r.error);
    return r.ok;
}

bool AppController::uninstall(const QString& itemId)
{
    if (m_operationBusy || m_updateInProgress) return false;
    QScopedValueRollback<bool> busy(m_operationBusy, true);
    const QString displayName = itemDisplayName(itemId);
    emit installStarted(displayName, /*uninstalling=*/true);

    const auto r = runInWorker(this, [itemId]() {
        return Injector::uninstall(itemId);
    });

    emit installFinished();
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
        m[QStringLiteral("missingAssets")]  = s.missingAssets;
        m[QStringLiteral("needsRepair")]    = s.needsRepair;
        out.push_back(m);
    }
    return out;
}

bool AppController::removeSkill(const QString& skillDirName)
{
    if (m_updateInProgress) return false;
    const int n = Skills::uninstallByName(skillDirName);
    invalidateSnapshot();
    emit itemsChanged();
    return n > 0;
}

void AppController::openSkillsFolder(const QString& itemId)
{
    // Open the skills folder of a specific tool when itemId is given;
    // fall back to the first tool with a skillsDirRel otherwise.
    auto openIfValid = [](const QString& dir) {
        if (dir.isEmpty() || !QFileInfo(dir).isDir()) return false;
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        return true;
    };

    if (!itemId.isEmpty()) {
        if (const Item* it = itemById(itemId)) {
            if (!it->skillsDirRel.isEmpty()) {
                const QString dir = Skills::installedRoot(itemId);
                if (openIfValid(dir)) return;
                emit errorRaised(Strings::instance()->lang() == Strings::CN
                    ? QStringLiteral("该工具还没有技能，先点安装")
                    : QStringLiteral("No skills installed yet — install first"));
                return;
            }
        }
    }

    for (const Item& it : allItems()) {
        if (it.skillsDirRel.isEmpty()) continue;
        const QString dir = Skills::installedRoot(it.id);
        if (openIfValid(dir)) return;
    }
    emit errorRaised(Strings::instance()->lang() == Strings::CN
        ? QStringLiteral("还没有安装任何技能")
        : QStringLiteral("No skills installed yet"));
}

QString AppController::skillsMcpConfig(const QString& format) const
{
    QString executable = QCoreApplication::applicationDirPath() + QStringLiteral("/ua-skills-mcp");
#if defined(Q_OS_WIN)
    executable += QStringLiteral(".exe");
#endif
    if (!QFileInfo(executable).isFile()) return {};
    const auto* item = itemById(QStringLiteral("vscode"));
    if (!item) return {};
    const QString root = planFor(*item).skillsDir;
    if (format == QLatin1String("codex")) {
        // JSON strings are also valid TOML basic strings for these path values.
        auto quoted = [](const QString& value) {
            const auto json = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
            return QString::fromUtf8(json.mid(1, json.size() - 2));
        };
        return QStringLiteral("[mcp_servers.ua-skills]\ncommand = %1\nargs = [\"--root\", %2]\n")
            .arg(quoted(executable), quoted(root));
    }
    if (format != QLatin1String("vscode") && format != QLatin1String("generic")) return {};
    QJsonObject server{{QStringLiteral("command"), executable},
                       {QStringLiteral("args"), QJsonArray{QStringLiteral("--root"), root}}};
    if (format == QLatin1String("vscode")) server.insert(QStringLiteral("type"), QStringLiteral("stdio"));
    const QString key = format == QLatin1String("vscode") ? QStringLiteral("servers") : QStringLiteral("mcpServers");
    return QString::fromUtf8(QJsonDocument(QJsonObject{{key, QJsonObject{{QStringLiteral("ua-skills"), server}}}}).toJson());
}

bool AppController::copySkillsMcpConfig(const QString& format)
{
    const QString config = skillsMcpConfig(format);
    const bool cn = Strings::instance()->lang() == Strings::CN;
    if (config.isEmpty()) {
        emit errorRaised(cn ? QStringLiteral("技能服务程序缺失，请使用完整发布包")
                            : QStringLiteral("Skills service missing; use the complete package"));
        return false;
    }
    QGuiApplication::clipboard()->setText(config);
    emit toast(cn ? QStringLiteral("MCP 配置已复制") : QStringLiteral("MCP configuration copied"));
    return true;
}

void AppController::showToast(const QString& text)
{
    emit toast(text);
}

void AppController::quit()
{
    if (m_operationBusy || m_updateInProgress) return;
    QCoreApplication::quit();
}

} // namespace ua::core
