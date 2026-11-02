# IDA ↔ reverse-skill Integration (Portable)

This page describes generic steps without any absolute paths tied to a specific machine. The local readiness report lives in `LOCAL-READINESS.md` at the repo root (gitignored).

## Target Shape

| Item | Convention |
|----|------|
| IDA install directory | Environment variable `IDADIR` (directory contains `ida.exe` or `ida.dll`) |
| HTTP MCP | `http://127.0.0.1:13337/mcp` |
| Client server name | Keep only **`idapro`** (do not also register `ida-pro-mcp`) |
| Startup | `scripts/start.ps1` (`--unsafe`, no `?ext=dbg`) |
| Opening a database | For large files prefer `scripts/open.ps1`; do not call `idb_open` directly through partial clients |

Pointing both MCP names at the same 13337 registers the tools twice and fights the idalib worker for the port.

## Installation

```powershell
setx IDADIR "<your IDA install directory>"

# Must use mrexodia/ida-pro-mcp; do not install the PyPI ida-mcp
python -m pip install "git+https://github.com/mrexodia/ida-pro-mcp.git"

# Activate idalib (adjust the path for your local IDA)
python "<IDADIR>\idalib\python\py-activate-idalib.py" -d "<IDADIR>"

# Install the plugin + client configuration
python -m ida_pro_mcp --install --transport streamable-http --scope global
```

## Startup and Keep-Alive

MCP entries of `type: http` do not launch the process for you. When 13337 is not listening, all client calls report errors.

| Script | Role |
|------|------|
| `scripts/start.ps1` | If healthy, `OK:<n>:reuse`; port listening but RPC timing out is treated as busy, not killed; replaces the managed supervisor only when nothing is listening or `py_eval` is missing; never kills `ida.exe` |
| `scripts/watchdog.ps1` | Health check every minute; reuse when busy/healthy; calls `start.ps1` only on down/stale |
| `scripts/install-autostart.ps1` | Registers the scheduled task `reverse-skill-ida-mcp` (at logon + every minute) |
| `scripts/start-gui.ps1` | Starts the GUI plugin when the idalib license fails |
| `scripts/open.ps1` | Calls `idb_open` directly over HTTP, bypassing some clients' schema validation |

Logs: `%LOCALAPPDATA%\reverse-skill\ida-mcp\supervisor.log` and `watchdog.log`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "skills\ida-reverse\scripts\start.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -File "skills\ida-reverse\scripts\open.ps1" -Path "C:\path\to\target.exe" -TimeoutSeconds 600
powershell -NoProfile -ExecutionPolicy Bypass -File "skills\ida-reverse\scripts\install-autostart.ps1"
```

When the GUI holds 13337 but is momentarily not responding, `start.ps1` prints `WARN:gui_busy` and exits, to avoid killing an IDA instance mid-analysis.

## Client

All entries point at Streamable HTTP: `http://127.0.0.1:13337/mcp`, server name `idapro`.

After changing the configuration you MUST start a new session. If the port was not listening when Cursor started, bringing the service up afterwards will **not** reconnect automatically; refresh manually in the MCP panel.

## Known Caveats

1. System32 files: `open.ps1` copies them to a temporary path (output includes `(temp copy)`)
2. Do not call `idb_open` directly through partial clients' MCP
3. `start.ps1` prefers `python -m ida_pro_mcp.idalib_supervisor`, more robust than the `.cmd` wrapper
4. When a formal install and a desktop portable copy coexist, `IDADIR` wins
5. Do not add `?ext=dbg` (debugger tools are not exposed by default)
