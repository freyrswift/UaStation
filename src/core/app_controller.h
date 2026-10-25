// app_controller.h - QObject exposed to QML as `app`

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

QT_BEGIN_NAMESPACE
class QSystemTrayIcon;
class QMenu;
class QWindow;
QT_END_NAMESPACE

namespace ua::core {

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items    READ items    NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList prompts  READ prompts  NOTIFY promptsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    // Called from main.cpp once the main QML window is created so Close->Hide
    // (and tray left-click->show) can target it.
    void attachMainWindow(QWindow* window);

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
    void showMainWindowRequested();

private:
    QVariantList buildItemsSnapshot() const;
    void setupTray();
    void showMainWindow();

    QSystemTrayIcon* m_tray = nullptr;
    QMenu*           m_trayMenu = nullptr;
    QWindow*         m_mainWindow = nullptr;
};

} // namespace ua::core
