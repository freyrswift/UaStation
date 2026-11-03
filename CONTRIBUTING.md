# Contributing / 参与贡献

欢迎提交可复现的问题、文档改进和小范围修复。中文或英文均可。
Reproducible reports, documentation improvements, and focused fixes are welcome in Chinese or English.

## 使用与贡献条款 / Usage and contribution terms

- 新增 Ua 自有作品仅供个人学习、参考和非商业使用；禁止销售、转售、收费打包和商业使用。详见 [LICENSE](LICENSE) 与[使用政策](docs/usage-policy.md)。
  New Ua-owned work is for personal study, reference, and noncommercial use only; sale, resale, paid bundling, and commercial use are prohibited. See [LICENSE](LICENSE) and the [usage policy](docs/usage-policy.md).
- 贡献条款仅适用于你原创且有权提交的新内容，并按上述仓库条款提交；不对既有内容作一揽子重新许可。
  Submit your original new material that you have the right to contribute under those repository terms; this does not blanket-relicense existing content.
- 保留先前 MIT 授权、上游署名及所有第三方许可；引入第三方材料时注明来源和原许可。
  Preserve prior MIT grants, upstream attribution, and all third-party licenses; identify the source and original license of third-party material.

## 开始前 / Before you start

- 先搜索已有 Issue 和 PR；较大功能先说明使用场景与预期结果。
  Search existing issues and PRs; discuss the use case and expected outcome before a large change.
- 阅读 [构建指南](docs/building.md)、[共享技能](docs/shared-skills.md) 和相关源码，沿用已有模式。
  Read the [build guide](docs/building.md), [shared-skills guide](docs/shared-skills.md), and relevant source; follow existing patterns.
- 从 `main` 创建主题分支；一次 PR 解决一个问题，避免无关格式化或生成文件。
  Branch from `main`; keep each PR focused and exclude unrelated formatting or generated files.

## 本地准备 / Local setup

- 需要 CMake 3.21+、Ninja、C++20 编译器和 Qt 6；[CMakeLists.txt](CMakeLists.txt) 最低要求 Qt 6.5，CI 使用 Qt 6.8.3。
  Use CMake 3.21+, Ninja, a C++20 compiler, and Qt 6; [CMakeLists.txt](CMakeLists.txt) requires Qt 6.5 or newer, while CI uses Qt 6.8.3.
- 按[构建指南](docs/building.md)选择本机 Qt kit、模块和部署依赖；在目标操作系统上构建。
  Follow the [build guide](docs/building.md) for the native Qt kit, modules, and deployment dependencies; build on the target OS.
- VS Code 中优先使用 CMake Tools 配置、构建和运行 CTest，并启用 `BUILD_TESTING=ON`。
  In VS Code, prefer CMake Tools for configuration, builds, and CTest; enable `BUILD_TESTING=ON`.

## 验证改动 / Validate changes

补充能复现缺陷的回归测试，先运行相关测试，再在提交前运行完整 CTest 套件。
Add regression coverage for fixes, run the relevant tests first, then run the full CTest suite before submission.

测试名以 [tests/CMakeLists.txt](tests/CMakeLists.txt) 为准：
Test names are defined in [tests/CMakeLists.txt](tests/CMakeLists.txt):

| 范围 / Area | CTest names |
| --- | --- |
| 回归与安装 / Regression and installation | `ua_regression`, `ua_installation`, `ua_progress`, `ua_features`, `ua_reliability` |
| 提示词与界面 / Prompts and UI | `ua_prompts`, `ua_vscode_ui` |
| 共享技能与 MCP / Shared skills and MCP | `ua_shared_skills`, `ua_skill_catalog`, `ua_mcp_server` |
| 运行时、检测与更新 / Runtime, detection, updates | `ua_runtime`, `ua_detection`, `ua_update_installer` |
| Windows 快捷方式 / Windows shortcuts only | `ua_shortcuts` |

[CI](.github/workflows/build.yml) 定义四个原生目标：Windows x64、macOS ARM64、macOS Intel x64、Linux x64。
[CI](.github/workflows/build.yml) defines four native targets: Windows x64, macOS ARM64, macOS Intel x64, and Linux x64.
检查对应任务结果；本机通过或工作流定义本身不代表其他平台已验证，也不代表所有工具适配器均已验证。
Check the matching jobs; a local pass or workflow definition does not establish validation on other platforms or for every tool adapter.

## 技能问题 / Skill-related issues

- 区分部署、发现和调用：支持原生目录的客户端从 `~/.agents/skills` 发现技能；重载并新建会话后检查来源。
  Separate deployment, discovery, and invocation: supporting clients discover `~/.agents/skills`; reload, start a new session, and check the source.
- MCP 路径需记录客户端/插件版本、模型、失败工具及脱敏响应；按需核对 `list_skills`、`read_skill`、`read_asset`。
  For MCP, record client/plugin versions, model, failing tool, and sanitized response; check `list_skills`, `read_skill`, and `read_asset` as applicable.
- 部署数量只反映本地文件状态，不证明模型已加载或使用；实际调用取决于客户端/插件、模型工具调用能力和会话设置。
  Deployed counts reflect local files, not proof of model loading or use; invocation depends on the client/plugin, model tool calling, and session settings.
- 说明本地、WSL、SSH 或容器环境；远程会话具有独立的文件系统和运行环境。详见[共享技能](docs/shared-skills.md)。
  Specify local, WSL, SSH, or container execution; remote sessions have separate filesystems and runtimes. See [shared skills](docs/shared-skills.md).

## 提交 PR / Open a pull request

- 说明问题、改动原因、关联 Issue，以及用户可见变化；界面变化附截图。
  Describe the problem, rationale, linked issue, and user-visible changes; attach screenshots for UI changes.
- 列出实际验证的平台/架构、Qt 与编译器版本、运行的测试及结果；明确标注未执行的检查。
  List verified OS/architecture, Qt and compiler versions, tests run, and results; explicitly mark checks not run.
- 提交前运行 `git diff --check` 并检查暂存内容，只包含本次改动。
  Run `git diff --check` and review the staged changes; include only the intended change.
- Issue、PR、日志和截图先脱敏：移除令牌、密钥、密码、用户名及真实用户主目录；路径用 `<HOME>` / `<USER>` 表示，保留相关相对路径与错误信息。
  Sanitize issues, PRs, logs, and screenshots: remove tokens, keys, passwords, usernames, and real home paths; use `<HOME>` / `<USER>` while preserving relevant relative paths and errors.
- 仅附最小相关片段，不上传完整用户配置、构建目录或本地状态文件。
  Attach only minimal relevant excerpts, not full user configurations, build directories, or local state files.
