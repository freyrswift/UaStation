// app_controller.cpp - QML-facing controller

#include "app_controller.h"

#include "items.h"
#include "injector.h"
#include "paths.h"
#include "prompts.h"
#include "skills.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

namespace ua::core {

AppController::AppController(QObject* parent) : QObject(parent)
{
    Prompts::extractEmbedded();
}

QString AppController::version() const
{
    return QCoreApplication::applicationVersion();
}

QVariantList AppController::buildItemsSnapshot() const
{
    QVariantList out;
    out.reserve(allItems().size());
    for (const Item& it : allItems()) {
        QVariantMap m;
        m[QStringLiteral("id")]              = it.id;
        m[QStringLiteral("name")]            = it.name;
        m[QStringLiteral("sub")]             = it.sub;
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
