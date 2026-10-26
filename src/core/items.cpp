// items.cpp - Static data tables

#include "items.h"
#include "strings.h"   // for Strings::StringId values used as nameId / subId

namespace ua::core {

const QVector<Item>& allItems()
{
    static const QVector<Item> kItems = {
        {   QStringLiteral("claude"),
            QStringLiteral("Claude"),
            {},
            ItemGroup::Editor,
            QStringLiteral("qrc:/logos/claude.svg"),
            false,
            {},
            QStringLiteral("https://docs.anthropic.com/en/docs/claude-code/overview"),
            QStringLiteral(".claude/CLAUDE.md"),
            {},
            QStringLiteral(".claude/skills"),
            false,
        },
        {   QStringLiteral("deepseek"),
            QStringLiteral("DeepSeek Harness"),
            {},
            ItemGroup::Editor,
            QStringLiteral("qrc:/logos/deepseek.svg"),
            false,
            {},
            {},
            QStringLiteral(".dsh/AGENTS.md"),
            {},
            QStringLiteral(".dsh/skills"),
            false,
        },
        {   QStringLiteral("skills-manager"),
            QStringLiteral("Skills 管理"),
            QStringLiteral("管理已安装的 Skills"),
            ItemGroup::Tools,
            {},
            false,
            QStringLiteral("e8fd"),
            {}, {}, {}, {}, /*isSkillsManager=*/true,
            Strings::ToolsSkills,    Strings::ToolsSkillsSub,
        },
        {   QStringLiteral("relay"),
            QStringLiteral("满血中转站"),
            QStringLiteral("API 中转服务"),
            ItemGroup::Tools,
            {},
            false,
            QStringLiteral("e774"),
            QStringLiteral("https://utlra.cc/"),
            {}, {}, {}, false,
            Strings::ToolsRelay,     Strings::ToolsRelaySub,
        },
        {   QStringLiteral("tutorial"),
            QStringLiteral("使用教程"),
            QStringLiteral("从安装到上手"),
            ItemGroup::Tools,
            {},
            false,
            QStringLiteral("e82d"),
            QStringLiteral("https://github.com/freyrswift/UaStation#usage"),
            {}, {}, {}, false,
            Strings::ToolsTutorial,  Strings::ToolsTutorialSub,
        },
        {   QStringLiteral("help"),
            QStringLiteral("不会用点这里"),
            QStringLiteral("常见问题与求助"),
            ItemGroup::Tools,
            {},
            false,
            QStringLiteral("e897"),
            QStringLiteral("https://qm.qq.com/q/3XsJIxaZde"),
            {}, {}, {}, false,
            Strings::ToolsHelp,      Strings::ToolsHelpSub,
        },
        {   QStringLiteral("qq"),
            QStringLiteral("加入QQ群"),
            QStringLiteral("交流与反馈"),
            ItemGroup::Tools,
            {},
            false,
            QStringLiteral("e8bd"),
            QStringLiteral("https://qm.qq.com/q/3XsJIxaZde"),
            {}, {}, {}, false,
            Strings::ToolsQQ,        Strings::ToolsQQSub,
        },
    };
    return kItems;
}

const QVector<SkillDef>& allSkills()
{
    using C = SkillCategory;
    static const QVector<SkillDef> kSkills = {
        { QStringLiteral("ai-llm-security"),                    C::Ai },
        { QStringLiteral("apk-reverse"),                        C::Mobile },
        { QStringLiteral("apkseep"),                            C::Mobile },
        { QStringLiteral("cheat-engine"),                       C::Reverse },
        { QStringLiteral("client-license-validation-bypass"),   C::Crack },
        { QStringLiteral("dma-attack"),                         C::Game },
        { QStringLiteral("exploit-development"),                C::Exploit },
        { QStringLiteral("ida-reverse"),                        C::Reverse },
        { QStringLiteral("malware-analysis"),                   C::Pentest },
        { QStringLiteral("mcp-js-reverse-playbook"),            C::Mobile },
        { QStringLiteral("mobile-security"),                    C::Mobile },
        { QStringLiteral("process-injection-detect"),           C::Other },
        { QStringLiteral("radare2"),                            C::Reverse },
        { QStringLiteral("re-angr"),                            C::Other },
        { QStringLiteral("re-anti-cheat"),                      C::Game },
        { QStringLiteral("re-console"),                         C::Game },
        { QStringLiteral("re-crash-triage"),                    C::Exploit },
        { QStringLiteral("re-deobfuscate"),                     C::Crack },
        { QStringLiteral("re-drm"),                             C::Crack },
        { QStringLiteral("red-team-ops"),                       C::Pentest },
        { QStringLiteral("re-ebpf"),                            C::Kernel },
        { QStringLiteral("re-evasion"),                         C::Pentest },
        { QStringLiteral("re-exploit"),                         C::Exploit },
        { QStringLiteral("re-frida"),                           C::Pentest },
        { QStringLiteral("re-fuzzing"),                         C::Exploit },
        { QStringLiteral("re-game"),                            C::Game },
        { QStringLiteral("re-hypervisor"),                      C::Kernel },
        { QStringLiteral("re-kernel"),                          C::Kernel },
        { QStringLiteral("re-keygen"),                          C::Crack },
        { QStringLiteral("re-license"),                         C::Crack },
        { QStringLiteral("re-packer-id"),                       C::Crack },
        { QStringLiteral("re-patching"),                        C::Crack },
        { QStringLiteral("re-pwn"),                             C::Exploit },
        { QStringLiteral("re-shellcode"),                       C::Exploit },
        { QStringLiteral("re-unpack"),                          C::Crack },
        { QStringLiteral("reverse-engineering"),                C::Reverse },
        { QStringLiteral("softseep"),                           C::Reverse },
        { QStringLiteral("threat-hunting"),                     C::Pentest },
        { QStringLiteral("unity-game-research"),                C::Game },
        { QStringLiteral("web-security"),                       C::Pentest },
    };
    return kSkills;
}

const Item* itemById(const QString& id)
{
    for (const Item& it : allItems()) {
        if (it.id == id) return &it;
    }
    return nullptr;
}

} // namespace ua::core
