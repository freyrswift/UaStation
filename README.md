# Ua Toolbox

**19 款 AI 编程工具的桌面提示词与技能管理器。**

集中选择、安装和管理提示词与技能，查看实际目录，并通过原生技能目录或只读 MCP
让支持的客户端按需使用同一套技能。Qt 6 / C++20 原生界面，中英双语，深色与浅色主题。

[![Release](https://img.shields.io/github/v/release/freyrswift/UaStation?style=flat-square)](https://github.com/freyrswift/UaStation/releases/latest)
[![Build](https://github.com/freyrswift/UaStation/actions/workflows/build.yml/badge.svg)](https://github.com/freyrswift/UaStation/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-Non--Commercial_Source--Available-327456?style=flat-square)](LICENSE)
[![Skills](https://img.shields.io/badge/bundled_skills-134-286eb4?style=flat-square)](docs/shared-skills.md)

[下载正式版](https://github.com/freyrswift/UaStation/releases/latest) ·
[更新日志](CHANGELOG.md) · [使用与分发](docs/usage-policy.md) ·
[反馈问题](https://github.com/freyrswift/UaStation/issues) · [English](#english)

<p align="center">
  <img src="docs/screenshots/main-editor.png" width="460" alt="Ua Toolbox 编辑器主界面，展示工具卡片与安装状态" />
</p>

> **免费提供，仅限本许可证允许的个人非商业学习、研究、参考和使用。禁止倒卖、出售及商业使用。**
> 严禁利用本工具进行未获许可的商业牟利，或恶意破坏、干扰他人程序、系统及数据。
> 一经发现，相关权利人将保留证据并在各自权利范围内依法追究责任。具体责任依法认定。
> 新条款不撤销旧 MIT 授权，也不改写第三方许可证；详见 [完整说明](docs/usage-policy.md)。

## 获取与版本

| 平台 | 正式包名称 | 说明 |
| --- | --- | --- |
| Windows x64 | `Ua-windows-x64.zip` | Windows 10 1809+ / Windows 11；本地已验证 |
| macOS Apple Silicon | `Ua-macos-arm64.dmg` | ARM64 原生构建，最低部署目标 macOS 12 |
| macOS Intel | `Ua-macos-x64.dmg` | x86_64 原生构建，最低部署目标 macOS 12 |
| Linux x64 | `Ua-linux-x64.tar.gz` | Ubuntu 24.04 构建基线，保留系统运行库依赖 |

正式包以 [Releases](https://github.com/freyrswift/UaStation/releases) 的实际资产为准。
`main` 可能含尚未正式发布的功能；请按下载版本的更新日志判断功能，不把 CI 中间产物当作正式版。
macOS 签名、公证及各平台适配边界见 [构建说明](docs/building.md)。

Windows 发布包自带 Qt 与所需 MSVC 运行库，运行 Ua 本身不要求另装 .NET、Python 或 Node。
技能中随附脚本的依赖由脚本自身决定；Node 可用时会执行相关 JS 补丁语法校验。

<a id="usage"></a>
## 使用步骤

1. 下载对应系统的正式包并完整解压，不只移动单个可执行文件。
2. 启动 Windows 的 `ua.exe`、macOS 的 `ua.app`，或 Linux 的 `bin/ua`。
3. 阅读并确认启动公告，选择目标工具卡片和提示词，核对显示的程序目录与指令目录。
4. 点击“安装”，等待完成，再重启目标工具或重载编辑器窗口。
5. 按下文分别检查提示词和技能。需要撤销时，在 Ua 中卸载，对应原文件按备份恢复。

提示词验证：输入独立的 `Ua`，预期回复“Ua 已经准备好随时待命”或
“Ua is ready and standing by”。这只验证相应提示词响应，不代表 134 项技能全部加载或执行。

重要配置请自行保留备份。Ua 会对其管理的文件执行备份、事务回滚与修改冲突保护；
这些机制不替代整机备份，也不保证外部工具升级后的路径和格式永远保持不变。

## Visual Studio Code 与共享技能

从 2.2.3 起，Visual Studio Code 卡片安装两部分：

| 内容 | 默认目录 | 用途 |
| --- | --- | --- |
| Copilot 用户级指令 | `~/.copilot/instructions/Ua.instructions.md` | 带 `applyTo: "**"` 的指令文件 |
| 完整共享技能包 | `~/.agents/skills` | 134 项技能及随附脚本、参考资料 |

`VSCODE_COPILOT_HOME` 只覆盖指令目录，不改变共享库。
`VSCODE_INSTALL_DIR` 只指定编辑器位置，不改变指令位置。
Windows 自动检测覆盖常见安装位置、PATH、注册表、运行进程与快捷方式；
自定义或便携版未被检测到时，在卡片中点击“选择程序”。

支持该原生目录的客户端可在重载后发现技能；支持 MCP 的其他插件，可在弹窗中选择
Visual Studio Code、通用 JSON 或 Codex TOML 格式，复制配置并合并到插件自己的设置。
配置包含随包附带的 `ua-skills-mcp` 程序路径，移动发布目录后需重新复制。

MCP 提供 `list_skills`、`read_skill`、`read_asset`，只读指定技能库中的文本，不执行脚本。
不同模型是否实际调用技能，取决于客户端、插件与工具调用能力，而非仅取决于模型名称。
普通聊天窗口不会因为目录存在就自动获得技能访问能力。

共享库由 Visual Studio Code 卡片单独管理；其他卡片保留自己的安装目录。
卸载该卡片会恢复或移除其管理的共享文件，依赖该共享库的客户端也会受影响。
文件数不代表某次会话的实际调用数。完整操作、分页及远程环境说明见 [共享技能指南](docs/shared-skills.md)。

## 支持的工具

| 工具 | 主要指令入口或适配方式 |
| --- | --- |
| Claude Code | `~/.claude/CLAUDE.md` 标记块 |
| Codex | `AGENTS.md` 与 `config.toml` 指令指针 |
| DeepSeek Harness | `AGENTS.md` 与独立指令文件 |
| Cursor | `.cursor/rules/Ua.mdc` 与 `.cursorrules` |
| Devin Desktop / Windsurf | `global_rules.md` |
| ZCode | `AGENTS.md`、Memory 与客户端适配 |
| OpenCode | `~/.config/opencode/AGENTS.md` |
| WorkBuddy | 提示词、配置、技能和客户端适配 |
| Gemini CLI | `GEMINI.md` |
| Qwen Code | `QWEN.md` |
| Trae | `project_rules.md` |
| CodeBuddy | `CODEBUDDY.md` |
| Cline | `~/Documents/Cline/Rules/Ua.md` |
| Copilot CLI | 独立的 `copilot-instructions.md` |
| Visual Studio Code | Copilot 指令与 `~/.agents/skills` |
| Grok | `~/.grok/AGENTS.md` |
| 豆包 / Doubao | 本地客户端 `.user_skills` 目录 |
| MiniMax | `~/.config/mimocode/AGENTS.md` |
| Kimi | `~/.kimi-code/AGENTS.md` |

入口取决于工具版本、系统与覆盖设置；界面显示的解析路径比示例路径更具体。
每个工具支持的环境变量和别名见 [目录与环境变量](docs/env-vars.md)。
WorkBuddy、ZCode 的深层客户端发现偏向 Windows；桌面程序可构建不等于所有适配器已在所有平台验证。

## 主要能力

- **完整技能包**：40 项内置技能与 94 项上游技能，保留配套文件及上游许可证。
- **任务路由**：父路由、细分叶子与口语归一映射按资源内容维护，不把入口文件数当作技能包总数。
- **安装可追踪**：显示目标路径、异步文件进度、安装状态与宿主升级提示。
- **修改可保护**：原子写入、sidecar 备份、事务失败回滚及外部修改冲突检测。
- **提示词可选择**：通用 Ua 与 WorkBuddy、Gemini、Kimi 专属提示词；用户编辑按现有保护规则保留。
- **模型与客户端分离**：原生技能发现与 MCP 两条路径，不因更换本地或云端模型而重复复制同一共享库。
- **桌面体验**：搜索过滤、中英双语、深浅主题、系统托盘与 Windows 窗口效果。

## 常见问题

<details>
<summary>卡片显示“未检测到”，但工具已经安装？</summary>

先确认工具确实在当前环境运行。Visual Studio Code 可通过“选择程序”指定实际程序，
也可使用 `VSCODE_INSTALL_DIR`。其他工具检查其配置目录、覆盖变量及工具版本。
WSL、SSH 和容器不等同于本机 Windows 用户环境。反馈时附系统、工具版本和已脱敏的解析路径。
</details>

<details>
<summary>只看到一个 .md 文件，技能有没有安装？</summary>

指令文件与技能目录是两件事。Visual Studio Code 的完整共享包位于 `~/.agents/skills`；
编辑器卡片显示部署数量。目标客户端先读取技能名称和描述，按需再读取正文及配套文件。
请检查客户端的 Skills 来源或 MCP 工具调用结果，不只检查指令文件。
</details>

<details>
<summary>切换模型、插件或远程环境，需要重新安装吗？</summary>

同一支持技能或 MCP 的客户端切换模型，通常保留原目录或配置即可。
换插件时需确认该插件支持的配置格式；换到 WSL、SSH、容器或另一台机器时，
应在实际运行环境部署对应技能与服务，不把 Windows 程序路径直接用于 Linux。
</details>

<details>
<summary>更新、卸载或修复时提示文件被占用、已修改？</summary>

先结束相关安装操作；若 Windows 的 MCP 服务正在运行，在客户端停止该服务后重试。
外部修改冲突表示 Ua 正在保留用户改动，不要直接删除备份或状态文件。
先备份并检查差异，再决定恢复原内容或保留自定义配置。
</details>

<details>
<summary>是否提供收费激活、付费代装或商业授权？</summary>

官方版本免费，没有收费激活码。当前许可证仅授予个人非商业范围内的学习和使用；
收费代装、倒卖、付费课程打包、商业整合和企业业务使用不在该授权内。
旧版 MIT 与第三方独立组件的原有权利保持不变。完整范围以 [LICENSE](LICENSE) 和
[使用与分发说明](docs/usage-policy.md) 为准，不因第三方声称“授权”而改变。
</details>

## 开发、反馈与贡献

构建要求为 C++20、CMake 3.21+ 和 Qt 6.5+；CI 基线为 Qt 6.8.3。
Visual Studio Code 中可使用 CMake Tools 配置、构建与运行已注册测试；
各平台依赖、原生构建和打包步骤见 [构建文档](docs/building.md)。

欢迎带复现步骤的问题反馈、目录适配、技能元数据修复、测试和文档改进。
提交前请阅读 [贡献指南](CONTRIBUTING.md)，说明平台、Ua 版本、目标工具与插件/模型组合。
日志、截图与配置示例应遮盖令牌、私钥、账号、个人目录和其他敏感内容。

- [更新日志](CHANGELOG.md)
- [共享技能与 MCP](docs/shared-skills.md)
- [目录与环境变量](docs/env-vars.md)
- [平台构建](docs/building.md)
- [第三方组件与授权](docs/THIRD_PARTY_NOTICES.md)
- [使用与分发规则](docs/usage-policy.md)
- QQ 交流群：**535041466** · [加入群聊](https://qm.qq.com/q/3XsJIxaZde)

## 授权与来源

当前新增的 Ua 自有内容适用 [Ua Non-Commercial Source-Available License 1.0](LICENSE)。
这是**非商业源码可见**授权，不是继续以 MIT 许可整个当前版本，也不宣称符合不受领域限制的开源定义。

历史版本与先前已授予 MIT 权利的部分保持原许可；相关文本保留在
[历史 MIT 授权](licenses/ua-prior-MIT.txt)。第三方组件保持自己的许可，不受本项目新增非商业条款改写。
商标、Logo 与第三方服务归各自权利人所有，支持某款工具不代表与其厂商合作或由其背书。

## English

**Ua Toolbox manages prompts and skill packages for 19 AI coding tools.**
It is a native Qt 6 / C++20 desktop application with Chinese/English UI,
light/dark themes, explicit installation paths, backups and transactional recovery.

### Download And Use

Download the package matching your OS from [Releases](https://github.com/freyrswift/UaStation/releases/latest),
extract the complete package, launch Ua, read the startup notice, select a tool
and a prompt, and install. Restart the target client before checking the result.

Build targets cover Windows x64, macOS ARM64/Intel and Linux x64.
Windows is locally validated; consult the matching CI job and release assets
for other targets. See [platform builds](docs/building.md) for requirements,
unsigned macOS builds and Linux runtime dependencies. The `main` branch may
contain changes that are not yet in the latest release.

### Shared Skills And MCP

Starting with 2.2.3, the Visual Studio Code adapter deploys all 134 skill packages
to `~/.agents/skills`, separately from Copilot instructions. Other adapters keep
their existing installation profiles. Native discovery requires a compatible
client; other MCP-capable plugins can use the configuration copied from Ua.

The bundled read-only `ua-skills-mcp` exposes `list_skills`, `read_skill` and
`read_asset`. It reads text resources, does not execute scripts, and does not
make every plugin or model automatically skill-aware. Deployed file counts and
the `Ua` activation reply are not evidence that all skills have been used.

See the [shared-skills guide](docs/shared-skills.md) and
[environment variables](docs/env-vars.md). Merge copied configuration with
existing settings instead of replacing them; deploy separately in remote environments.

### License And Distribution

Newly licensed Ua-owned material is for personal, non-commercial learning,
research, reference and use. Sale, resale, paid installation/support, commercial
deployment, business use, paid bundles and indirect payment gates are outside
the grant. Free sharing must preserve attribution, license texts and third-party
notices; modified binary distributions must include their corresponding changes
in source form and be clearly marked as unofficial.

Prohibited commercial exploitation or malicious damage to other people's
programs, systems or data may lead to evidence preservation and legal action
by the relevant rights holders. Liability is determined under applicable law.

This is a **non-commercial source-available** license, not an unrestricted
open-source license. Previous MIT grants and third-party licenses remain
unchanged. Read [LICENSE](LICENSE), [usage policy](docs/usage-policy.md) and
[third-party notices](docs/THIRD_PARTY_NOTICES.md) for the actual scope.

### Contribute

Read [CONTRIBUTING.md](CONTRIBUTING.md), provide reproducible environment details,
and remove secrets from logs and screenshots. See [CHANGELOG.md](CHANGELOG.md)
for versioned changes and the Actions tab for the matching build/test results.
