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
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>

namespace ua::core {

AppController::AppController(QObject* parent) : QObject(parent)
{
    Prompts::extractEmbedded();
    setupTray();

    // Translated card labels: refresh the items snapshot whenever the UI
    // language flips, so the Tools-tab tiles swap CN/EN live.
    connect(Strings::instance(), &Strings::langChanged,
            this, &AppController::itemsChanged);
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
        m[QStringLiteral("supported")]       = !it.targetRel.isEmpty();
        m[QStringLiteral("detected")]        = toolDetected(it.targetRel);
        m[QStringLiteral("state")]           = static_cast<int>(Injector::scan(it.id));
        out.push_back(m);
    }
    return out;
}

QVariantList AppController::items()
{
    return buildItemsSnapshot();
}

QVariantList AppController::editorItems()
{
    QVariantList out;
    for (const QVariant& v : buildItemsSnapshot()) {
        if (v.toMap().value(QStringLiteral("group")).toInt() == static_cast<int>(ItemGroup::Editor)) {
            out.push_back(v);
        }
    }
    return out;
}

QVariantList AppController::toolsItems()
{
    QVariantList out;
    for (const QVariant& v : buildItemsSnapshot()) {
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

void AppController::refreshStates()
{
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

bool AppController::install(const QString& itemId, const QString& promptFile)
{
    const auto r = Injector::install(itemId, promptFile);
    const Item* it = itemById(itemId);
    if (r.ok && it && !it->skillsDirRel.isEmpty()) Skills::installFor(itemId);
    emit itemsChanged();
    if (!r.ok) emit errorRaised(r.error);
    return r.ok;
}

bool AppController::uninstall(const QString& itemId)
{
    const auto r = Injector::uninstall(itemId);
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
        out.push_back(m);
    }
    return out;
}

bool AppController::removeSkill(const QString& skillDirName)
{
    const int n = Skills::uninstallByName(skillDirName);
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
