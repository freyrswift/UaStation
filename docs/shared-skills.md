# 共享技能

## 安装与所有权

在 Ua 中选择 Visual Studio Code 并安装，134 个技能连同随附脚本和参考资料会部署到当前用户
`~/.agents/skills`。Copilot 指令仍写入原来的独立位置；`VSCODE_COPILOT_HOME` 只影响指令，
不改变共享技能库。

共享目录只有 Visual Studio Code 卡片一个管理者。其他卡片继续使用各自已有目录，
不会将同一共享文件登记为多个安装所有者。安装前备份同名原文件；检测到已管理文件被编辑时
停止并回滚，不覆盖修改。卸载会恢复原文件或移除生成文件，因此依赖共享库的其他客户端也会失去这些生成技能。

部署到共享库的技能使用目录名作为唯一 `name`。超长描述仅在部署副本中缩短至标准上限，
正文、其他元数据、资源文件及仓库中的原技能保持原样。此规范化不影响其他工具的旧安装方式。

## 原生发现

支持 `~/.agents/skills` 的本地客户端可以直接发现共享库。Visual Studio Code 重载窗口后新建会话，
运行 `Chat: Open Customizations`，检查 Skills 中的来源路径。可以输入 `/技能目录名` 手动调用，
也可让代理按任务与描述选择技能。不同版本或插件的命令和支持目录可能不同。

安装目录不等于每次对话都加载全部正文。客户端通常先读取名称和描述，再按需读取
`SKILL.md` 及其引用的配套文件。界面显示的是本地部署数，不代表某次模型调用已使用该技能。

## MCP 客户端

发布包包含独立的 `ua-skills-mcp.exe`（Windows）或 `ua-skills-mcp`（macOS/Linux），
只依赖包内 Qt 运行库，不需要 Node 或 Python。

在 Visual Studio Code 卡片弹窗的 MCP 下拉框选择客户端格式，点击复制图标，并合并到客户端现有配置：

- `MCP: Visual Studio Code`：使用 `servers` 根节点，合并到用户级 MCP 配置或项目 `.vscode/mcp.json`。
- `MCP: 通用`：使用 `mcpServers` 根节点，适用于采用该约定的插件；以插件实际配置格式为准。
- `MCP: Codex`：复制 `[mcp_servers.ua-skills]` TOML 节，合并到 Codex 的配置。

不要用片段覆盖整个已有配置。复制结果包含当前发布包中服务程序的绝对路径以及共享目录的绝对路径。
移动发布包后需要重新复制配置；更新包保留同一安装位置时路径无需更改。
Windows 更新前请停止客户端中的该 MCP 服务；检测到服务文件被占用时，Ua 会保留主窗口并提示重试。

启动服务并在客户端确认工具可用：

1. 调用 `list_skills`，查看技能名称、描述和资源列表。
2. 调用 `read_skill`，指定返回的技能名称，读取完整指令。
3. 使用 `read_asset` 读取该技能下的 UTF-8 文本参考资料或脚本；遵循返回结构中的分页字段继续读取。

`list_skills` 的 `offset` 和 `limit` 按技能条目计数；`read_skill`、`read_asset` 按 Unicode
码点计数。返回的 `nextOffset` 为下一页起点，`null` 表示结束。二进制资源仍随原生技能包部署，
MCP 文本读取接口会拒绝二进制内容。

服务仅通过标准输入输出提供 JSON-RPC MCP 协议，不监听网络端口，不执行脚本，
也不提供任意目录读取或写入工具。它只读取指定技能库中的有效技能及配套文件。

## 模型与运行环境

技能由代理或插件加载，不由模型名称决定。同一个支持工具调用的客户端切换本地、GitHub Copilot
或其他云端模型时，可保留同一技能目录/MCP 配置。具体是否选择、读取和执行技能仍由客户端能力、
模型工具调用及会话设置决定；普通聊天窗口不会仅因目录存在而获得文件访问能力。

WSL、SSH、容器及其他远程会话有自己的文件系统和执行环境。应在该环境部署共享库与相应平台的服务程序，
而不是把 Windows 可执行文件路径直接填入远程 Linux 配置。

## 参考

- [Visual Studio Code Agent Skills](https://code.visualstudio.com/docs/agent-customization/agent-skills)
- [Codex MCP](https://developers.openai.com/codex/mcp)
- [MCP 标准输入输出协议](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports)
