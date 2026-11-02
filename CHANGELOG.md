# Changelog

## [2.2.1] - 2026-11-03

### Added
- 更新 Gemini CLI（Gemini-4-Argon）与 Kimi（K3.1）专属提示词，统一 Ua 名称和中英文激活回复，其他应用提示词保持原样
- Windows 确认启动公告后自动检查版本，发现新版后显示进度、自动下载并更新重启
- 更新包按具体版本下载并校验大小与 SHA-256，替换前检查目录权限，失败时保留或恢复原程序
- 更新失败支持重试或稍后处理，安装任务执行期间延后重启
- macOS 与 Linux 保留校验后的更新包下载和手动安装入口

### Fixed
- 修复更新安装器准备完成后窗口关闭保护拦截退出、导致自动重启卡住的问题
- Visual Studio Code 检测补充 Windows 自定义安装的注册表路径，以及 `code.cmd` 和 Insiders 启动器
- 修复 `VSCODE_COPILOT_HOME` 指向空目录时跳过编辑器安装检测的问题，指令仍写入指定目录
- 窗口重新激活或从托盘恢复时刷新工具状态，避免沿用启动时的检测结果

## [2.2.0] - 2026-11-02

### Added
- Visual Studio Code 独立编辑器卡片、名称和 Logo，支持工具数由 18 增至 19
- Visual Studio Code Copilot 用户级指令文件，包含 `applyTo: "**"` 元数据和 `VSCODE_COPILOT_HOME` 目录覆盖
- Windows 可执行文件版本信息，与界面及 macOS Bundle 共用 CMake 项目版本
- 各平台发布包内附更新日志、说明与许可证，GitHub Release 自动读取对应版本的更新日志

### Fixed
- 修复 WorkBuddy 部分 bug。
- Visual Studio Code 使用完整名称及官方标准蓝色图标
- Visual Studio Code 检测与 Copilot CLI 配置分离，环境变量覆盖优先
- Visual Studio Code 指令写入时保留已有内容及 UTF-8 BOM，并补齐缺失的 Front Matter
- Logo 回归场景适配 19 个工具，覆盖深色和浅色主题
- Windows 发布包排除包含本地构建路径的中间资源文件
- Linux CI 使用 Qt 6.8.3 对应的 `linux_gcc_64` 安装标识
- CI 安装阶段使用绝对路径，修复 Linux Qt 运行库部署失败
- 本地化头文件更名为 `app_strings.h`，避免遮蔽 POSIX `strings.h` 并保留 Qt 自动注册

## [2.1.0] - 2026-10-29

### Added
- 支持 4 个新工具：Grok、豆包、MiniMax、Kimi（总数 14 → 18）
- 内置 94 项上游技能（REA + reverse-skill），总技能数 40 → 134
- 两层任务路由：86 片细分叶子 + 73 条口语归一映射
- 安装/卸载进度条（异步化 + 实时文件路径）
- 工具卡片搜索框
- 宿主升级检测（SHA256 patchStamp）

### Changed
- Windsurf 更名为 Devin Desktop
- 多个工具 Logo 更新为更贴近品牌的设计
- 卡片状态灯逻辑：绿/橙/黄/无

### Fixed
- `revealTargetDir()` 不再凭空创建空目录
- `envCheck()` 对 `.config/*` 路径取正确层级
- `openTip()` 对无 tip 工具的友好提示
- `Injector::install()` 拒绝对未检测工具写盘
- 其他小 bug

### Removed
- iFlow 工具（上游关停）

### Security
- `safeCopy()` 包装所有文件拷贝，失败不再静默
- JsPatch 写盘后 `node --check` 校验，失败自动回滚
- 保存状态失败时自动回滚已写靶点

---

## [2.0.0] - 2026-10-28

### Added
- Qt 6 + C++20 原生应用，替代旧 Win32 版
- 14 个 AI 工具深度注入支持
- WorkBuddy 10 层注入
- 40 项内嵌技能
- GitHub Actions 自动构建发布
