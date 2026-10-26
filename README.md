# Ua Toolbox

[![Release](https://img.shields.io/github/v/release/freyrswift/UaStation?style=flat-square)](https://github.com/freyrswift/UaStation/releases)
[![License](https://img.shields.io/github/license/freyrswift/UaStation?style=flat-square)](LICENSE)
[![Stars](https://img.shields.io/github/stars/freyrswift/UaStation?style=flat-square)](https://github.com/freyrswift/UaStation)
[![Downloads](https://img.shields.io/github/downloads/freyrswift/UaStation/total?style=flat-square)](https://github.com/freyrswift/UaStation/releases)

[中文](#ua-toolbox-中文) | [English](#ua-toolbox-english)

---

## Ua Toolbox 中文

一键为 AI 编程工具安装提示词和技能的桌面工具，跨 **Windows / Linux / macOS**。

<p align="center">
  <img src="docs/screenshots/main-editor.png" width="460" alt="主界面" />
</p>
### ⚠️ 禁止商用声明

> **本工具完全免费、开源，禁止任何形式的商业使用。**
>
> 严禁对本工具进行出售、倒卖、打包收费、捆绑销售或以任何方式向他人收取费用。
> 如果你是花钱购买的，**你被骗了，请立即要求退款并举报卖家**。
>
> 违反本声明的行为将保留依法追究的权利。

### 法律声明

本工具依据中华人民共和国相关法律法规发布，使用本工具即表示您同意以下条款：

1. **合法用途**：本工具仅用于对用户本人合法拥有或已获书面授权的 AI 编程工具进行本地配置管理。严禁将本工具用于任何违反《中华人民共和国网络安全法》《数据安全法》《个人信息保护法》及其他法律法规的活动。

2. **授权使用**：用户在使用本工具对任何系统、软件或服务进行安全研究时，必须事先取得相关权利人的明确书面授权。未经授权的渗透测试、逆向工程或漏洞利用行为可能违反《中华人民共和国刑法》第二百八十五条、第二百八十六条等相关规定。

3. **免责条款**：本工具按"原样"提供，作者不对因使用本工具产生的任何直接或间接损失承担责任。用户应对自身使用行为及其后果承担全部法律责任。

4. **免费声明**：本工具完全免费且开源。任何以本工具名义收取费用的行为均属欺诈，请立即举报并申请退款。

5. **未成年人保护**：未满 18 周岁的用户应在监护人指导下使用本工具。

### 功能特性

| 特性 | 说明 |
|------|------|
| 🔧 一键安装 | 选择 AI 工具 → 选择提示词 → 点击安装，三步搞定 |
| 🤖 支持的 AI 工具 | Claude Code、DeepSeek Harness（陆续扩展） |
| 🔍 智能检测 | 自动识别电脑中已安装的 AI 工具，未检测到的自动灰显 |
| 📝 内嵌提示词 | 启动即释放 `Ua.md` 到 `%APPDATA%/Ua/prompts/`，可自己增减 |
| 🎒 40 个 Skills | 覆盖逆向 / 游戏 / 内核 / 漏洞 / 破解 / 移动 / 渗透 / AI 等分类 |
| 🌐 中英双语 | 界面所有文字实时切换，不需要重启 |
| 🎨 深浅主题 | 跟随系统或手动切换，Win11 Mica 效果 |
| 🖥 跨平台 | Windows x64 / Linux x64 / macOS arm64 一套代码 |
| 📦 托盘常驻 | 关闭 → 隐藏到系统托盘，不打扰 |

### 使用方法

1. 从 [Releases](https://github.com/freyrswift/UaStation/releases) 下载 `Ua.exe`
2. 双击运行（无需安装、无需管理员权限）
3. 选择 AI 编程工具（如 Claude Code）
4. 选择提示词模板，点"安装"
5. 重启目标 AI 工具，即刻生效

> 💡 卸载：选中已安装的工具，点"卸载"即可恢复原始配置。

#### 验证安装效果

打开目标 AI 工具，输入 `Ua` 后回车，如果回复"**Ua 已经准备好随时待命**"即表示提示词已成功加载：

<p align="center">
  <img src="docs/screenshots/claude-code-demo.png" width="640" alt="Ua 已经准备好随时待命" />
</p>

之后就可以用大白话向 AI 分配任务，提示词会自动触发对应的技能模板。

### 系统要求

- **Windows** 10 1809+ 或 Windows 11（x64）
- **Linux**：GTK/Qt-friendly 桌面环境（Ubuntu 22.04+ 验证通过）（x64）
- **macOS** 11+（arm64 / x86_64）

### 构建方法

依赖：**Qt 6.5+** · **CMake 3.21+** · **Ninja 或 Make** · 对应平台的 C++20 编译器。

```powershell
# Windows (MSVC)：
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH=D:\Qt\6.8.3\msvc2022_64
cmake --build build

# Linux：
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cpack -G TGZ    # 可选：打 .tar.gz

# macOS：
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cpack -G DragNDrop    # 打 .dmg
```

Windows 构建的 Ua.exe 自动跟 Qt 运行时 DLL + QML 插件打在同一目录（走 `windeployqt`），直接拷整个 `build/` 就能分发。

### Star History

[![Star History Chart](https://api.star-history.com/svg?repos=freyrswift/UaStation&type=Date)](https://star-history.com/#freyrswift/UaStation&Date)

### 交流反馈

遇到问题或有建议？扫码加入 QQ 群：

<p align="center">
  <img src="docs/screenshots/qq-group.jpg" width="280" alt="QQ群：535041466" />
</p>

<p align="center">
  QQ群号：<strong>535041466</strong> | <a href="https://qm.qq.com/q/3XsJIxaZde">点击加入</a>
</p>
### 许可证

MIT License，详见 [LICENSE](LICENSE)。

---

## Ua Toolbox English

A cross-platform (**Windows / Linux / macOS**) desktop tool that installs prompts and skills into AI coding tools in one click.

<p align="center">
  <img src="docs/screenshots/main-editor.png" width="460" alt="主界面" />
</p>
### ⚠️ Anti-Commercial Notice

> **This tool is completely free and open source. Commercial use is strictly prohibited.**
>
> It is forbidden to sell, resell, bundle, or charge for this tool in any way.
> If you paid money for it, **you were scammed — demand a refund and report the seller immediately**.
>
> The author reserves the right to pursue legal action against violators.

### Legal Notice

This tool is provided in compliance with applicable laws and regulations. By using this tool, you agree to the following terms:

1. **Lawful Use Only**: This tool is intended solely for managing local configurations of AI coding tools that the user legally owns or has written authorization to access.

2. **Authorization Required**: Users must obtain explicit written authorization before conducting any security research on systems, software, or services.

3. **Disclaimer**: This tool is provided "as is" without warranty. The author assumes no liability for any damages arising from use. Users bear full legal responsibility for their actions.

4. **Free Software**: This tool is completely free and open source. Any attempt to charge for it is fraudulent.

5. **Minor Protection**: Users under 18 should use this tool under parental guidance.

### Features

| Feature | Description |
|---------|-------------|
| 🔧 One-Click Install | Select tool → Pick prompt → Install. Three steps. |
| 🤖 Supported Tools | Claude Code, DeepSeek Harness (more coming) |
| 🔍 Auto Detection | Automatically detects installed AI tools on your system |
| 📝 Embedded Prompt | Drops `Ua.md` into `%APPDATA%/Ua/prompts/` at first launch |
| 🎒 40 Skills | Reverse / Game / Kernel / Exploit / Crack / Mobile / Pentest / AI categories |
| 🌐 Bilingual | Full CN/EN interface, live switch — no restart |
| 🎨 Themes | Dark / Light / System, Win11 Mica support |
| 🖥 Cross-platform | Windows x64 / Linux x64 / macOS arm64, one codebase |
| 📦 Tray-resident | Close → hide to system tray, stays out of the way |

### Usage

1. Download `Ua.exe` from [Releases](https://github.com/freyrswift/UaStation/releases)
2. Double-click to run (no installer, no admin required)
3. Select an AI tool (e.g. Claude Code)
4. Choose a prompt template and click "Install"
5. Restart the AI tool to apply

> 💡 To uninstall: select the tool and click "Remove" to restore original config.

#### Verify Installation

Open your AI tool and type `Ua` — if it replies **"Ua is ready and standing by"**, the prompt is loaded correctly:

<p align="center">
  <img src="docs/screenshots/claude-code-demo.png" width="640" alt="Ua is ready" />
</p>

After that, you can give the AI tasks in plain language — the prompt automatically routes to the right skill template.

### Requirements

- **Windows** 10 1809+ or Windows 11 (x64)
- **Linux**: a GTK/Qt-friendly desktop (verified on Ubuntu 22.04+, x64)
- **macOS** 11+ (arm64 / x86_64)

### Build

Dependencies: **Qt 6.5+** · **CMake 3.21+** · **Ninja or Make** · a C++20 compiler for your OS.

```bash
# Windows (MSVC):
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=D:\Qt\6.8.3\msvc2022_64
cmake --build build

# Linux:
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cpack -G TGZ        # optional: .tar.gz

# macOS:
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cpack -G DragNDrop  # .dmg
```

On Windows the build auto-deploys Qt runtime + QML plugins next to Ua.exe (via `windeployqt`), so `build/` ships as-is.

Requires Visual Studio 2022+ with C++ Desktop Development workload.

### Star History

[![Star History Chart](https://api.star-history.com/svg?repos=freyrswift/UaStation&type=Date)](https://star-history.com/#freyrswift/UaStation&Date)

### Community

Questions or feedback? Join our QQ Group:

<p align="center">
  <img src="docs/screenshots/qq-group.jpg" width="280" alt="QQ Group: 535041466" />
</p>

<p align="center">
  QQ Group: <strong>535041466</strong> | <a href="https://qm.qq.com/q/3XsJIxaZde">Join</a>
</p>
### License

MIT License. See [LICENSE](LICENSE).
