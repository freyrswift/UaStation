# Changelog

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
