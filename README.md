# Ua Toolbox

[![Release](https://img.shields.io/github/v/release/freyrswift/UaStation?style=flat-square)](https://github.com/freyrswift/UaStation/releases)
[![License](https://img.shields.io/github/license/freyrswift/UaStation?style=flat-square)](LICENSE)
[![Stars](https://img.shields.io/github/stars/freyrswift/UaStation?style=flat-square)](https://github.com/freyrswift/UaStation)
[![Downloads](https://img.shields.io/github/downloads/freyrswift/UaStation/total?style=flat-square)](https://github.com/freyrswift/UaStation/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078d4?style=flat-square)](https://github.com/freyrswift/UaStation/releases)

[中文](#中文) | [English](#english)

---

## 中文

一键给 **18 款 AI 编程工具**注入提示词和技能的桌面工具。原生 Qt，20 MB，仅 Windows。

<p align="center">
  <img src="docs/screenshots/main-editor.png" width="460" alt="主界面" />
</p>

### 快速开始

1. 从 [Releases](https://github.com/freyrswift/UaStation/releases/latest) 下载 `Ua-windows-x64.zip`
2. 解压到任意目录
3. 双击 `ua.exe`
4. 选中要装的 AI 工具 → 选提示词 → 点"安装"
5. 重启目标 AI 工具 → 输入 `Ua` 验证："Ua 已经准备好随时待命" 就是成功

卸载：选中已装的工具点"卸载"，备份自动还原。

### 支持的 AI 工具（18 款）

| # | 工具 | 开发者 | 注入方式 |
|---|---|---|---|
| 01 | Claude Code | Anthropic | `~/.claude/CLAUDE.md` 标记块 |
| 02 | Codex | OpenAI | 双通道：`AGENTS.md` + `config.toml` 指针 |
| 03 | DeepSeek Harness | DeepSeek | `AGENTS.md` + `prompt-inject.md`（dsh-purge 集成） |
| 04 | Cursor | Anysphere | `.cursor/rules/Ua.mdc` + `.cursorrules` 同步 |
| 05 | Devin Desktop (ex-Windsurf) | Cognition | `global_rules.md` 标记块 |
| 06 | ZCode | Z.ai (智谱) | `AGENTS.md` + Memory + `zcode.cjs` JS 补丁 |
| 07 | OpenCode | opencode.ai | `.config/opencode/AGENTS.md` |
| 08 | WorkBuddy | 腾讯 | 10 层深度注入（模板 / product.json / 命令闸门 / 沙箱 / 缓存等） |
| 09 | Gemini CLI | Google | `GEMINI.md` 标记块 |
| 10 | Qwen Code | 阿里通义 | `QWEN.md` 标记块 |
| 11 | Trae | 字节跳动 | `project_rules.md` 标记块 |
| 12 | CodeBuddy | 腾讯 | `CODEBUDDY.md` 标记块 |
| 13 | Cline | Cline Bot | `~/Documents/Cline/Rules/Ua.md` |
| 14 | Copilot CLI | GitHub | `copilot-instructions.md` 标记块 |
| 15 | Grok | xAI | `~/.grok/AGENTS.md` |
| 16 | 豆包 | 字节跳动 | `%LOCALAPPDATA%\Doubao\...\.user_skills\ua-doubao\SKILL.md` |
| 17 | MiniMax | mimocode | `~/.config/mimocode/AGENTS.md` |
| 18 | Kimi | Moonshot AI | `~/.kimi-code/AGENTS.md` |

每个工具的配置目录都支持**环境变量覆盖**（`CODEX_HOME` / `CLAUDE_CONFIG_DIR` / `KIMI_CODE_HOME` 等），参见 [docs/env-vars.md](docs/env-vars.md)。

### 功能

| 特性 | 说明 |
|------|------|
| 🧠 两层任务路由 | 86 片细分叶子技能 + 73 条口语归一映射 + 6 大父路由 |
| 🎒 134 个 Skills | 40 内置 + 94 项 vendored 上游技能（REA + reverse-skill） |
| 🔧 深度注入 | WorkBuddy 10 层 / ZCode `zcode.cjs` 补丁 / Codex 双通道 |
| 🔄 异步进度条 | 安装/卸载实时进度，不卡 UI |
| 🚦 智能检测 | 环境变量 + 真实路径 + 内容验证三重判断 |
| 🧯 事务保护 | 原子写盘 + sidecar 备份 + 失败自动回滚 + `node --check` 校验 |
| 🚨 宿主升级检测 | SHA256 patchStamp，宿主升级覆盖补丁后自动提示重装 |
| 🔍 搜索过滤 | 顶部搜索框实时过滤工具卡片 |
| 📝 内嵌提示词 | 启动即释放 `Ua.md` 和 86 片叶子到 `%APPDATA%/Ua/prompts/`，可编辑 |
| 🌐 中英双语 | 实时切换，不需重启 |
| 🎨 Mica 窗口 | Win11 风格无边框 + 系统托盘 |

### 系统要求

- **Windows 10 1809+ / Windows 11（x64）**
- 不需要安装 .NET / Python / Node；Qt 运行时随 exe 发布

### 构建

```powershell
# 装 Qt 6.8.3 (msvc2022_64) + VS 2022 + Ninja + CMake 3.21+
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix dist
```

CI 自动构建见 [`.github/workflows/build.yml`](.github/workflows/build.yml)。

### ⚠️ 禁止商用

本工具完全免费、开源。严禁出售、倒卖、打包收费或向他人收费。
如果你为此付过钱，**你被骗了，请立即退款并举报**。

### 法律声明

本工具仅供用户在**自己合法拥有或已获书面授权**的 AI 工具上做本地配置管理。
严禁用于违反《网络安全法》《数据安全法》《个人信息保护法》等相关法律的活动。
使用本工具产生的所有后果由用户自行承担。

### 更新日志

见 [CHANGELOG.md](CHANGELOG.md)。

### 交流

QQ 群：**535041466** · [点击加入](https://qm.qq.com/q/3XsJIxaZde)

### 许可证

[MIT License](LICENSE)

---

## English

One-click prompt and skill installer for **18 AI coding tools**. Native Qt, 20 MB, Windows only.

### Quick Start

1. Download `Ua-windows-x64.zip` from [Releases](https://github.com/freyrswift/UaStation/releases/latest)
2. Extract anywhere
3. Double-click `ua.exe`
4. Pick a tool → pick a prompt → click Install
5. Restart the target AI tool → type `Ua` → expect `Ua is ready and standing by`

To uninstall: select an installed tool, click Uninstall — original files are restored from backup.

### Supported Tools

Claude Code, Codex, DeepSeek Harness, Cursor, Devin Desktop (ex-Windsurf), ZCode, OpenCode, WorkBuddy, Gemini CLI, Qwen Code, Trae, CodeBuddy, Cline, Copilot CLI, Grok, Doubao, MiniMax, Kimi. See the Chinese table above for injection details.

### Features

- Two-tier task routing: 86 leaf skills + colloquial normalizer + 6 parent routes
- 134 bundled skills (40 native + 94 vendored upstream)
- Deep injection on WorkBuddy / ZCode / Codex (not just prompt files)
- Async install with real progress bar
- Smart detection: env-var + real path + content check
- Transactional writes with sidecar backup + auto rollback
- Host upgrade detection via SHA256 patchStamp
- Live search filter
- Embedded prompts auto-extract to `%APPDATA%/Ua/prompts/`
- Mica window + system tray

### Requirements

Windows 10 1809+ / Windows 11, x64. No .NET / Python / Node needed.

### Build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix dist
```

### License

[MIT](LICENSE). Non-commercial use only — selling or bundling this tool for money is prohibited.

### Changelog

See [CHANGELOG.md](CHANGELOG.md).
