# Environment variables and paths

Ua reads these variables from its own process environment. Set them before
starting Ua; changing a terminal's environment does not update an already running
instance. These are Ua's path-resolution rules, not a promise that every client
supports the same variables.

The implementation is in [paths.cpp](https://github.com/freyrswift/UaStation/blob/main/src/core/paths.cpp),
[install_plan.cpp](https://github.com/freyrswift/UaStation/blob/main/src/core/install_plan.cpp), and
[items.cpp](https://github.com/freyrswift/UaStation/blob/main/src/core/items.cpp). `~` below means the current user's home as
resolved by Qt, including on Windows.

## Tool configuration overrides

For each row, Ua uses the **first nonempty value from left to right**, after
trimming whitespace. An empty or whitespace-only variable falls through to the
next alias, then to the default. A nonempty override wins even if its directory
does not exist; that may affect detection as well as the installation target.

These variables name a **configuration root**, not an instruction file or a
skills directory, except for the Doubao variable described separately. Ua keeps
the relative suffix of each target under the new root. For example, `CODEX_HOME`
changes both the Codex instruction/configuration files and its skills directory.

| Ua client | Variables, highest precedence first | Default root | Targets under the selected root |
| --- | --- | --- | --- |
| Claude | `CLAUDE_CONFIG_DIR`, `CLAUDE_HOME` | `~/.claude` | `CLAUDE.md`, `skills/` |
| DeepSeek Harness | `DSH_HOME` | `~/.dsh` | `AGENTS.md`, `skills/`; `prompt-inject.md` only when `plugins/dsh-purge/` exists |
| Codex | `CODEX_HOME`, `CODEX_DIR` | `~/.codex` | `AGENTS.md`, `managed-prompts/Ua.md`, `config.toml`, `skills/` |
| Cursor | `CURSOR_HOME` | `~/.cursor` for skills; rule-directory probing below | `rules/Ua.mdc`, `skills/` |
| Devin Desktop / Windsurf | `DEVIN_HOME`, `WINDSURF_HOME` | `~/.codeium/windsurf` | `memories/global_rules.md`, `skills/` |
| ZCode | `ZCODE_HOME`, `GLM_HOME`, `ZHIPU_HOME` | `~/.zcode` | `AGENTS.md`, `cli/memories/global/memory/MEMORY.md`, `skills/` |
| OpenCode | `OPENCODE_HOME` | `~/.config/opencode` | `AGENTS.md`, `skills/` |
| WorkBuddy | `WORKBUDDY_CONFIG_DIR`, `WORKBUDDY_HOME` | `~/.workbuddy`, with legacy fallback below | `SOUL.md`, `MEMORY.md`, account/personal memory targets, `skills/`, settings and configuration targets |
| Gemini CLI | `GEMINI_HOME`, `GEMINI_DIR` | `~/.gemini` | `GEMINI.md`, `skills/` |
| Qwen Code | `QWEN_HOME` | `~/.qwen` | `QWEN.md`, `skills/` |
| Trae | `TRAE_HOME` | `~/.trae` | `rules/project_rules.md`, `skills/` |
| CodeBuddy | `CODEBUDDY_HOME` | `~/.codebuddy` | `CODEBUDDY.md`, `skills/` |
| Cline | `CLINE_HOME` | `~/Documents/Cline` | `Rules/Ua.md`, `skills/` |
| Copilot CLI | `COPILOT_HOME` | `~/.copilot` | `copilot-instructions.md`, `skills/` |
| Visual Studio Code | `VSCODE_COPILOT_HOME` | `~/.copilot` | `instructions/Ua.instructions.md`; shared skills are separate |
| Grok | `GROK_HOME`, `GROK_DIR` | `~/.grok` | `AGENTS.md`, `skills/` |
| MiniMax / mimocode | `MIMOCODE_HOME` | `~/.config/mimocode` | `AGENTS.md`, `skills/` |
| Kimi | `KIMI_CODE_HOME` | `~/.kimi-code` | `AGENTS.md`, `skills/` |

All 19 Ua profiles define a skills-deployment target. When the installation
target resolves, Ua deploys the profile's library and generates `INDEX.md` at its
root. Doubao's separate target-resolution requirements are described below.
The seven newly added libraries use `~/.codeium/windsurf/skills`,
`~/.gemini/skills`, `~/.qwen/skills`, `~/.trae/skills`, `~/.codebuddy/skills`,
`~/Documents/Cline/skills`, and `~/.copilot/skills` by default. Their configuration
root overrides also relocate their libraries and indexes; all other existing
roots remain unchanged. These are Ua deployment destinations, not a guarantee
that each client supports native skill discovery.

Ua normalizes backslashes, expands `~` or a leading `~/`, cleans the path, and
makes relative overrides absolute against Ua's working directory. Prefer
absolute paths. General tool-root overrides do not expand embedded `%NAME%`,
`$NAME`, or `${NAME}` expressions themselves; the launching shell may expand
them before Ua receives them.

### Client-specific behavior

- **Cursor:** without `CURSOR_HOME`, the first existing rule directory wins, in
  this order: `~/.cursor/rules`, `%APPDATA%/Cursor/User/rules`,
  `%APPDATA%/Cursor/rules`, `%LOCALAPPDATA%/Cursor/User/rules`,
  `%LOCALAPPDATA%/Cursor/rules`, `~/.config/Cursor/User/rules`, then
  `~/Library/Application Support/Cursor/User/rules`. Candidates requiring an
  unset environment variable are skipped. If none exists, Ua selects
  `~/.cursor/rules`. An existing `~/.cursorrules` is also updated only when no
  override is active. `CURSOR_HOME` selects `<root>/rules/Ua.mdc` and disables
  those additional rule probes; skills use `<root>/skills`.
- **WorkBuddy:** with no override, `~/.workbuddy-ai` is used only if
  `~/.workbuddy` is not a directory and the legacy directory exists. The override
  selects the configuration root, not the installed application. WorkBuddy and
  ZCode installation-directory discovery remains separate and Windows-oriented;
  their configuration overrides do not redirect install-directory targets.
- **Doubao:** `DOUBAO_USER_SKILLS` points directly to the user skills directory.
  If it is empty, Ua derives that directory from `LOCALAPPDATA` as
  `%LOCALAPPDATA%/Doubao/User Data/Default/.doubao/agent_mode/workspace/.user_skills`.
  Ua writes `ua-doubao/SKILL.md` beneath it and uses it for skills installation.
  If both variables are empty, the plan has no Doubao targets. `~/.doubao` is not
  the active installation location.

## Visual Studio Code: three independent locations

| Purpose | Setting | Effect |
| --- | --- | --- |
| Find the editor program | Saved manual selection, then `VSCODE_INSTALL_DIR`, then discovery | Editor detection only; not an instruction or skills root |
| Write Copilot instructions | `VSCODE_COPILOT_HOME`, otherwise `~/.copilot` | `<root>/instructions/Ua.instructions.md` |
| Publish shared skills | Fixed `~/.agents/skills` | Managed by the Visual Studio Code card; neither variable above moves it |

`COPILOT_HOME` applies to the **Copilot CLI** card, not the Visual Studio Code
card. Similarly, `CODEX_HOME` affects Codex's own profile, not the shared skills
library. Visual Studio Code remains the sole owner of that fixed shared library,
including its generated `INDEX.md`; other clients use their separate libraries.

Each installed prompt copy includes a `# Ua Skill Discovery` block with compact
JSON fields `root`, `index`, and `count` identifying its resolved library root,
index path, and skill count. Discovery prefers native skill metadata, then the
local index and matching skill files, or configured and available MCP tools.
Each index catalog uses the canonical directory name as its `name`, a normalized
summary description capped at 1024 characters, and the actual absolute `SKILL.md`
document path. This catalog metadata is distinct from a deployed non-shared
`SKILL.md`'s declared Front Matter name, which may differ because that file is
kept verbatim. All 134 exact skill directory names and their catalog summaries
remain in each index even when prompt length limits omit the optional inline list.
This guidance neither grants file access nor guarantees model use. Plain chat
requires suitable client capabilities and configuration; remote sessions need
paths and services accessible from their execution environment. See
[shared skills](shared-skills.md) for discovery order, prompt limits, and MCP access.

### Editor discovery precedence

Ua tries the following sources in order and uses the first valid editor path:

1. The saved manual selection in `vscode-install-path.txt` under Ua's data root.
2. `VSCODE_INSTALL_DIR`.
3. Candidates from `PATH`, in path-entry order, checking Stable and Insiders
   launchers.
4. Known platform locations: on Windows, locations derived in order from
   `LOCALAPPDATA` (with `/Programs`), `ProgramFiles`, `ProgramFiles(x86)`, and
   `ProgramW6432`; on macOS, system and user Applications folders; on Linux,
   known `/usr/bin`, `/usr/share`, `/opt`, and `/snap/bin` locations.
5. Windows only: registered application/protocol paths, uninstall records,
   running editor executables, then shortcut targets.

A valid saved manual selection takes precedence over the environment. An invalid
or stale saved path or environment value falls through to later sources. Use
**Locate program** in the Visual Studio Code dialog to save a selection. The
resolver also supports clearing the saved selection with an empty path; refreshing
detection does not itself clear it.

`VSCODE_INSTALL_DIR` accepts an existing installation directory or a recognized
editor executable/launcher path. On macOS it also accepts the application bundle
or its `Contents/MacOS/Electron` executable. Ua trims whitespace, handles a quoted
path, expands `%NAME%` references with nonempty environment values, expands a
leading `~`, and requires an absolute path after expansion. An unexpanded unknown
`%NAME%` is left literal. `$NAME` expansion is not implemented by this resolver.
Provide a path, not a command with arguments; discovered registry command/icon
values are parsed separately and are not executed.

### Launch examples

These are illustrative local paths; select directories appropriate to your
installation. No settings are written to the system-wide environment.

```powershell
$env:CODEX_HOME = 'D:/AI/Codex'
$env:VSCODE_INSTALL_DIR = 'D:/Apps/Microsoft VS Code'
$env:VSCODE_COPILOT_HOME = 'D:/AI/VSCodeCopilot'
& 'D:/Apps/Ua/ua.exe'
```

```bash
export CODEX_HOME="$HOME/ai/codex"
export VSCODE_COPILOT_HOME="$HOME/ai/vscode-copilot"
/opt/ua/bin/ua
```

The examples relocate the selected profiles, while the shared publisher still
uses `~/.agents/skills`. The read-only MCP executable can separately read another
absolute directory through its `--root` command-line option; this does not change
the publisher's installation path. See [MCP startup](https://github.com/freyrswift/UaStation/blob/main/src/mcp/main.cpp).

## Ua's own data and logging

Ua's writable data root is resolved by [paths.cpp](https://github.com/freyrswift/UaStation/blob/main/src/core/paths.cpp):

| Platform | Data root |
| --- | --- |
| Windows | `%APPDATA%/Ua` when `APPDATA` is nonempty; otherwise Qt's `GenericDataLocation` with `/Ua` appended |
| macOS | Qt's `GenericDataLocation` with `/Ua` appended, normally `~/Library/Application Support/Ua` |
| Linux | Qt's `GenericDataLocation` with `/Ua` appended, normally `$XDG_DATA_HOME/Ua` or `~/.local/share/Ua` |

The data root contains `prompts/`, `state/`, `backups/`, and the saved editor path.
This uses **data** location semantics, not Linux's `XDG_CONFIG_HOME`. Ua does not
implement a separate application-specific environment override for this root.
UI settings use Qt's separate `QSettings` storage; see
[settings.cpp](https://github.com/freyrswift/UaStation/blob/main/src/core/settings.cpp).

In [main.cpp](https://github.com/freyrswift/UaStation/blob/main/src/main.cpp), any nonempty `UA_LOG` value enables append-only Qt
message logging to `ua.log` next to the executable. Even `UA_LOG=0` enables it;
unset the variable or set it empty to disable it. It is an enable switch, not a
log-file path, and the executable directory must be writable for logging to open.

## Build-time Qt hint

[CMakeLists.txt](https://github.com/freyrswift/UaStation/blob/main/CMakeLists.txt) reads the environment variable `Qt6_DIR` and
prepends its value to `CMAKE_PREFIX_PATH`. If that variable is not defined, Windows
also tries `D:/Qt/6.8.3/msvc2022_64` when it exists. Normal CMake Qt package lookup
then applies. The project reads `Qt6_DIR`, not `QT_DIR`; this build-time hint does
not affect any runtime profile or shared-skills path. See
[building](https://github.com/freyrswift/UaStation/blob/main/docs/building.md) for the supported build setup, and
[third-party notices](THIRD_PARTY_NOTICES.md) for the copied licenses and Qt
6.8.3 source/replacement information. Repository links above target the current
`main` branch; use the release tag's exact CI source commit when reproducing a
published build.
