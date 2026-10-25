// strings.cpp - Translation table body (ported from old config.h kStr[][2])

#include "strings.h"

#include <QSettings>
#include <array>

namespace ua::core {

namespace {

// Row order matches StringId; must be kept in sync with the enum.
// Entry format: { CN, EN }
constexpr std::array<std::array<const char*, 2>, Strings::_Count> kTable {{
    { "跟随系统",            "System" },
    { "深色",                "Dark" },
    { "浅色",                "Light" },
    { "窗口置顶",            "Always on Top" },
    { "开机自启",            "Start on Boot" },
    { "关于",                "About" },
    { "退出",                "Exit" },
    { "编辑器",              "Editor" },
    { "工具",                "Tools" },
    { "主题",                "Theme" },
    { "语言",                "Language" },
    { "中文",                "中文" },
    { "English",             "English" },
    { "免责声明",            "Disclaimer" },
    { "本工具完全免费，仅供学习和授权安全研究使用。\n\n"
      "1. 用户应确保在合法授权范围内使用本工具，对使用本工具所产生的一切后果自行承担责任。\n\n"
      "2. 本工具不收取任何费用。如果你是花钱购买的，说明你被骗了，请立即退款。\n\n"
      "3. 作者不对因使用本工具导致的任何直接或间接损失承担责任。\n\n"
      "4. 继续使用即表示你已阅读并同意以上条款。",
      "This tool is completely free and is intended solely for learning and authorized security research.\n\n"
      "1. Users must ensure they use this tool within legally authorized scope and bear full responsibility for any consequences.\n\n"
      "2. This tool is free of charge. If you paid for it, you were scammed — request a refund immediately.\n\n"
      "3. The author is not liable for any direct or indirect damages caused by using this tool.\n\n"
      "4. By continuing, you agree to the above terms." },
    { "我已了解",            "I Understand" },
    { "暂不支持自动安装",    "Auto-install not supported" },
    { "未检测到该工具",      "Tool not detected" },
    { "已安装",              "Installed" },
    { "文件已有内容",        "File has content" },
    { "未安装",              "Not installed" },
    { "位置",                "Path" },
    { "打开",                "Open" },
    { "选择提示词",          "Select Prompt" },
    { "prompts\\ 文件夹里没有 .md 文件", "No .md files in prompts\\ folder" },
    { "官网",                "Site" },
    { "卸载",                "Remove" },
    { "安装",                "Install" },
    { " 安装成功",           " installed" },
    { " 已卸载",             " removed" },
    { "还没有设置链接",      "No link configured" },
    { "Skills 管理",          "Skills Manager" },
    { "个内嵌 skill",         " embedded skills" },
    { "打开 Skills 文件夹",   "Open Skills Folder" },
    { "请先安装提示词后再查看", "Please install a prompt first" },
    { "已从所有工具中移除",   "Removed from all tools" },
    { "没有需要移除的",       "Nothing to remove" },
    { "Skills 管理",          "Skills Manager" },
    { "管理已安装的 Skills",  "Manage installed skills" },
    { "满血中转站",           "API Relay" },
    { "API 中转服务",         "API relay service" },
    { "使用教程",             "Tutorial" },
    { "从安装到上手",         "Getting started guide" },
    { "不会用点这里",         "Need Help?" },
    { "常见问题与求助",       "FAQ and support" },
    { "加入QQ群",             "Join QQ Group" },
    { "交流与反馈",           "Chat and feedback" },
    { "Ua 工具箱  版本 ",      "Ua Toolbox  Version " },
    { "本工具仅供学习与授权安全研究使用。用户应在合法授权范围内使用，作者不承担任何法律责任。本工具完全免费且开源。",
      "For educational and authorized security research only. Users are responsible for legal compliance. The author assumes no liability. This tool is free and open source." },
    { UA_VERSION_STR,         UA_VERSION_STR },
    { "请先选择一个提示词",   "Please select a prompt first" },
    { "读取提示词文件失败",   "Failed to read prompt file" },
    { "提示词文件请保存为 UTF-8 编码", "Please save prompt file as UTF-8" },
    { "提示词文件是空的",     "Prompt file is empty" },
    { "读取目标文件失败",     "Failed to read target file" },
    { "保存状态失败",         "Failed to save state" },
    { "写入目标文件失败",     "Failed to write target file" },
    { "这个应用还没有通过本工具注入过", "This app has not been injected" },
    { "恢复备份失败",         "Failed to restore backup" },
    { "设置开机自启失败",     "Failed to set autostart" },
    { "打开链接失败",         "Failed to open link" },
    { "GitHub 仓库",          "GitHub Repository" },
    { "检查更新中...",        "Checking for updates..." },
    { "发现新版本：",         "New version available: " },
    { "已是最新版本",         "Already up to date" },
    { "检查更新",             "Check for Updates" },
    { "prompts 文件夹丢失，请重新下载完整版本", "prompts folder missing, please re-download" },
    { "prompts 文件夹是空的", "prompts folder is empty" },
    { "搜索...",              "Search..." },
    { "全部",                 "All" },
    { "逆向",                 "Reverse" },
    { "游戏",                 "Game" },
    { "内核",                 "Kernel" },
    { "漏洞",                 "Exploit" },
    { "破解",                 "Crack" },
    { "移动",                 "Mobile" },
    { "渗透",                 "Pentest" },
    { "AI",                   "AI" },
    { "其他",                 "Other" },
    { "正在同步提示词...",    "Syncing prompts..." },
    { "提示词已更新",         "Prompts updated" },
    { "%1 个提示词已更新",    "%1 prompts updated" },
    { "显示窗口",             "Show Window" },
    { "Ua 工具箱",            "Ua Toolbox" },
    { "正在下载更新...",      "Downloading update..." },
    { "下载完成，即将重启",   "Download complete, restarting..." },
    { "立即更新",             "Update Now" },
    { " 安装成功，请重启 AI 工具生效", " installed, please restart the AI tool" },
    { "自动更新下载失败。是否打开浏览器手动下载最新版本？",
      "Auto-update download failed. Open browser to download the latest version?" },
    { "该文件在安装后被修改过，卸载将恢复到安装前的版本。确定继续？",
      "This file was modified after installation. Uninstalling will restore the pre-install version. Continue?" },
    { "清理所有数据",         "Clean All Data" },
    { "将删除所有备份、状态和设置。确定？", "This will delete all backups, state and settings. Continue?" },
    { "数据已清理",           "All data cleaned" },
    { "已安装",               "installed" },
    { "技能",                 "skills" },
    { "确认卸载？这将恢复原始配置文件。", "Confirm uninstall? This will restore the original config file." },
    { "发现新版本 %1（当前 %2），立即下载并更新？",
      "New version %1 available (current %2). Download and update now?" },
    { "共 %1 / %2 个 skill",   "%1 of %2 skills" },
    { "%1 个工具",             "%1 tools" },
    { "没有匹配的 skill",      "No matching skills" },
}};

constexpr const char* kLangSettingsKey = "ui/lang";

} // namespace

Strings* Strings::instance()
{
    static Strings s_instance;
    return &s_instance;
}

Strings::Strings(QObject* parent) : QObject(parent)
{
    QSettings settings;
    m_lang = settings.value(QString::fromLatin1(kLangSettingsKey), 0).toInt();
    if (m_lang != CN && m_lang != EN) m_lang = CN;
}

void Strings::setLang(int lang)
{
    if (lang != CN && lang != EN) return;
    if (lang == m_lang) return;
    m_lang = lang;

    QSettings settings;
    settings.setValue(QString::fromLatin1(kLangSettingsKey), lang);
    emit langChanged();
}

QString Strings::t(int id) const
{
    if (id < 0 || id >= _Count) return {};
    const char* raw = kTable[static_cast<size_t>(id)][static_cast<size_t>(m_lang)];
    return QString::fromUtf8(raw);
}

QString Strings::get(StringId id) const
{
    return t(static_cast<int>(id));
}

} // namespace ua::core
