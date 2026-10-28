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

struct Item;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items       READ items       NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList editorItems READ editorItems NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList toolsItems  READ toolsItems  NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList prompts     READ prompts     NOTIFY promptsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    // Called from main.cpp once the main QML window is created so Close->Hide
    // (and tray left-click->show) can target it.
    void attachMainWindow(QWindow* window);

    QVariantList items();
    QVariantList editorItems();
    QVariantList toolsItems();
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

    // Return the single prompt appropriate for a given tool.
    QVariantList promptsFor(const QString& itemId);

    // Environment check: detailed status for each editor tool.
    QVariantList envCheck();

    // Per-product usage guide: generated from the item's real paths + notes,
    // written to <configDir>/docs/tips-<id>.txt and opened.
    void openTip(const QString& itemId);

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
    void installProgress(int current, int total, const QString& path);

private:
    QVariantList buildItemsSnapshot() const;
    // Cached snapshot: rebuilt on first access and after install/uninstall.
    const QVariantList& cachedSnapshot() const;
    void invalidateSnapshot();
    void setupTray();
    void showMainWindow();
    // Renders the bundled guide template for one product.
    QString tipFor(const Item& item) const;
    // Writes every product's guide under <configDir>/docs; returns the folder.
    QString syncTips();
    static QString toolExecutable(const QString& itemId);

    QSystemTrayIcon* m_tray = nullptr;
    QMenu*           m_trayMenu = nullptr;
    QWindow*         m_mainWindow = nullptr;
    mutable QVariantList m_snapshotCache;
    mutable bool         m_snapshotDirty = true;
};

} // namespace ua::core
