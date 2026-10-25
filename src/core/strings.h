// strings.h - Translation table (CN / EN) exposed to QML
//
// This is the Qt-side counterpart of the old config.h kStr[][2] table. We keep
// the string IDs stable so prompts, skills and UI code can all key off them.

#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QJSEngine;
QT_END_NAMESPACE

namespace ua::core {

class Strings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int lang READ lang WRITE setLang NOTIFY langChanged)

public:
    // ONLY add new IDs at the end; do not renumber (callers index by position).
    enum StringId : int {
        ThemeSystem = 0, ThemeDark, ThemeLight,
        Topmost, Autostart, About, Exit,
        TabEditor, TabTools,
        Theme, Lang,
        LangCN, LangEN,
        DisclaimerTitle, DisclaimerBody, DisclaimerBtn,
        NotSupported, ToolNotFound, Injected, FileHasContent, NotInjected,
        Location, Open, SelectPrompt, NoPrompts,
        Website, Uninstall, Inject,
        InjectOk, UninstallOk, NoLink,
        SkillsTitle, SkillsSub, SkillsFolder, SkillsNeedInject,
        SkillRemoveOk, SkillRemoveNone,
        ToolsSkills, ToolsSkillsSub, ToolsRelay, ToolsRelaySub,
        ToolsTutorial, ToolsTutorialSub, ToolsHelp, ToolsHelpSub,
        ToolsQQ, ToolsQQSub,
        AboutText, AboutLegal, Version,
        ErrNoPromptSel, ErrReadPrompt, ErrUtf16Prompt, ErrEmptyPrompt,
        ErrReadTarget, ErrSaveState, ErrWriteTarget,
        ErrNoState, ErrRestore, ErrAutostart,
        ErrOpenUrl,
        GithubLabel, UpdateCheck, UpdateNew, UpdateLatest, CheckUpdate,
        ErrPromptsMissing, WarnPromptsEmpty,
        SkillSearchPlaceholder,
        CatAll, CatReverse, CatGame, CatKernel, CatExploit,
        CatCrack, CatMobile, CatPentest, CatAi, CatOther,
        SyncChecking, SyncUpdated, PromptsUpdatedCount,
        TrayShow, TrayTooltip,
        UpdateDownloading, UpdateReady, UpdateBtn,
        InjectRestartHint, UpdateManualHint, WarnFileModified,
        CleanData, CleanDataConfirm, CleanDataOk,
        StatusInstalled, StatusSkills,
        ConfirmUninstall, UpdateConfirm,
        SkillCountFmt, SkillToolCountFmt, SkillNoMatch,
        _Count,
    };
    Q_ENUM(StringId)

    enum Language : int { CN = 0, EN = 1 };
    Q_ENUM(Language)

    static Strings* instance();
    static Strings* create(QQmlEngine*, QJSEngine*) { return instance(); }

    int lang() const { return m_lang; }
    void setLang(int lang);

    // Direct lookup. QML bindings that call this directly do NOT auto-refresh
    // when `lang` changes, because QML tracks property reads, not method calls.
    Q_INVOKABLE QString t(int id) const;

    // Convenience for native code; same mapping.
    QString get(StringId id) const;

signals:
    void langChanged();

private:
    explicit Strings(QObject* parent = nullptr);
    int m_lang = 0;
};

} // namespace ua::core
