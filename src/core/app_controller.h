// app_controller.h - QObject exposed to QML as `app`

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace ua::core {

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items    READ items    NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList prompts  READ prompts  NOTIFY promptsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);

    QVariantList items();
    QVariantList prompts();
    QString version() const;

public slots:
    // Item actions
    void refreshStates();
    void refreshPrompts();
    void openUrl(const QString& url);
    void revealTargetDir(const QString& itemId);
    bool install(const QString& itemId, const QString& promptFile);
    bool uninstall(const QString& itemId);

    // Skill actions
    QVariantList skills();
    bool removeSkill(const QString& skillDirName);
    void openSkillsFolder();

    // Lifecycle
    void showToast(const QString& text);
    void quit();

signals:
    void itemsChanged();
    void promptsChanged();
    void toast(const QString& text);
    void errorRaised(const QString& text);

private:
    QVariantList buildItemsSnapshot() const;
};

} // namespace ua::core
