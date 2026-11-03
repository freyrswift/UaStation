// strings.cpp - Translation table body (ported from old config.h kStr[][2])

#include "app_strings.h"

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
    { "使用公告",            "Usage Notice" },
    { "请在使用 Ua 工具箱前阅读本公告，并遵守《中华人民共和国网络安全法》《中华人民共和国数据安全法》《中华人民共和国个人信息保护法》《中华人民共和国民法典》等适用法律法规。\n\n"
      "1. 合法使用与授权\n仅在拥有合法权利、取得有效授权或法律允许的范围内操作系统、软件、文件和数据。不得利用本工具非法侵入、干扰或破坏网络和计算机系统，窃取数据，或实施侵害他人合法权益的行为。本工具的安装不产生对第三方系统的操作授权。\n\n"
      "2. 数据与个人信息保护\n处理个人信息应有合法依据，遵循目的明确、最小必要原则，依法履行告知、同意等义务并采取保护措施。请勿擅自输入、上传、泄露国家秘密、商业秘密、他人隐私或其他受保护数据；涉及重要数据、数据出境等事项时，应另行遵守相应规定。\n\n"
      "3. 知识产权与第三方服务\n尊重软件、代码、模型、文档等内容的知识产权，遵守相应许可、合同和适用的平台规则。提示词或配置调整不免除这些义务。\n\n"
      "4. 配置变更与 AI 输出\n安装、卸载等操作可能修改本地配置，请先核对目标路径并备份重要文件。AI 输出可能存在错误或遗漏，请在执行命令、发布内容或作出决定前人工复核，不将其视为专业意见或结果保证。\n\n"
      "5. 非商业使用与分发\n本版新增 Ua 自有内容仅供个人非商业学习、研究、参考和使用，禁止倒卖、出售、收费代装、付费打包及商业使用。免费分享须保留来源和许可证；旧版 MIT 授权及第三方组件许可不变，具体范围见随包 LICENSE 与 docs/usage-policy.md。\n\n"
      "6. 责任与公告效力\n严禁利用本工具从事本许可证未授权的商业牟利，或恶意破坏、干扰他人程序、系统及数据。一经发现，相关权利人将保留证据并在各自权利范围内依法追究责任，具体责任依法认定。使用者和提供者各自依法承担相应责任；本公告不排除或限制依法不得排除或限制的责任，也不代替其他法定义务。",
      "Before using Ua Toolbox, read this notice and comply with applicable laws, including the PRC Cybersecurity Law, Data Security Law, Personal Information Protection Law, and Civil Code.\n\n"
      "1. Lawful use and permission\nOperate on systems, software, files, and data only with lawful rights, valid permission, or another legal basis. Do not unlawfully access, disrupt, or damage systems, steal data, or infringe others' rights. Installing this tool does not authorize operations on third-party systems.\n\n"
      "2. Data and personal information\nUse a lawful basis, a clear purpose, and data minimization. Meet applicable notice, consent, and protection duties. Do not improperly enter, upload, or disclose state secrets, trade secrets, private information, or other protected data. Additional rules may apply to important data or cross-border transfers.\n\n"
      "3. Intellectual property and third-party services\nRespect intellectual property and applicable licenses, contracts, and platform rules. Prompts or configuration changes do not remove these duties.\n\n"
      "4. Configuration changes and AI output\nInstallation and removal may change local configuration. Verify paths and back up important files first. AI output may contain errors or omissions. Review commands, content, and decisions before acting; output is not professional advice or a guarantee.\n\n"
      "5. Non-commercial use and distribution\nNewly licensed Ua-owned material is for personal, non-commercial learning, research, reference and use. Sale, resale, paid installation, paid bundles and commercial use are outside the grant. Free sharing must retain attribution and licenses. Earlier MIT grants and third-party licenses remain unchanged; see the bundled LICENSE and docs/usage-policy.md.\n\n"
      "6. Responsibility and effect\nProhibited commercial exploitation and malicious damage to or interference with other people's programs, systems or data are forbidden. Relevant rights holders will preserve evidence and pursue responsibility within their rights under applicable law; liability is determined by law. Users and providers each bear their respective legal responsibilities. This notice does not exclude liability that cannot lawfully be excluded or replace other legal duties." },
    { "继续使用",            "Continue" },
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
    { "免费提供，新增自有内容仅供个人非商业学习和使用。旧版与第三方许可不变，完整范围见 LICENSE。请遵守《使用公告》。",
      "Free download for personal non-commercial study and use of newly licensed Ua material. Earlier and third-party licenses remain unchanged. See LICENSE and Usage Notice." },
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
    { "安装并重新启动",         "Install and Restart" },
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
    { "工作台",                 "Workbench" },
    { "Seep 逆向工作台",        "Seep Reverse Workbench" },
    { "AI 驱动的逆向工程平台",   "AI-driven reverse engineering platform" },
    { "工作台文档",             "Documentation" },
    { "部署手册与使用指南",      "Deployment manual and user guide" },
    { "MCP 服务",              "MCP Server" },
    { "逆向分析 MCP 工具集",    "Reverse analysis MCP toolkit" },
    { "新建会话后生效",         "Takes effect in a new session" },
    { "重新加载窗口后生效",     "Reload the window to apply" },
    { "重启应用后生效",         "Restart the app to apply" },
    { "内容超出该工具的长度上限", "Content exceeds this tool's length limit" },
    { "安装记录损坏或版本不兼容，已保留原文件：%1", "Install state is invalid or incompatible. Original files preserved: %1" },
    { "文件在安装后发生变更，已停止卸载并保留现状：%1", "File changed after installation. Uninstall stopped; current files preserved: %1" },
    { "安装不完整，请重新安装", "Installation needs repair" },
    { "检查更新失败，点击重试", "Update check failed, retry" },
    { "缺附件", "Missing assets" },
    { "更新下载失败：", "Update download failed: " },
    { "我已阅读并理解上述公告", "I have read and understood this notice" },
    { "更新日期：2026年11月1日", "Updated: November 1, 2026" },
    { "软件更新",              "Software Update" },
    { "正在准备更新...",        "Preparing update..." },
    { "正在等待当前任务完成...", "Waiting for the current task..." },
    { "更新安装完成后将自动重新打开软件。", "The app will reopen automatically after installation." },
    { "更新失败",              "Update failed" },
    { "重试",                  "Retry" },
    { "稍后",                  "Later" },
    { "发布页面",              "Release Page" },
    { "新版本 %1 / 当前 %2",    "New version %1 / Current %2" },
    { "打开下载文件夹",        "Open Folder" },
    { "下载完成后请解压或挂载更新包，退出当前软件后手动替换安装。", "Extract or mount the downloaded package, then quit the app and replace the installation manually." },
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
