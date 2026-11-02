# Red Team Sharp* Tool Analysis & Tool Installation Matrix & dnSpy MCP

## Red Team Sharp* Tool Analysis

Red team tools are largely written in C# (the Sharp* series); reverse engineering them is a common scenario: understanding detection logic, changing signatures, extracting embedded configuration.

### Quick reference of common Sharp* tools

| Tool | Function | Reverse engineering focus |
|------|------|-----------|
| **Rubeus** | Kerberos attacks (AS-REP roast / Kerberoast / S4U / pass-the-ticket)| Rubeus has a fixed project structure; look in the `Interop.*` P/Invoke sections for native calls |
| **SharpHound** | BloodHound data collector | LDAP query logic, the set of collected attributes |
| **SharpShell / SharpWS** | Remote execution, lateral movement | WMI / WinRM calls, command obfuscation |
| **Seatbelt** | Reconnaissance | Collected item list, decision logic |
| **SharpRoast** | Kerberoasting | Ticket request/parsing |
| **Inveigh / SharpSploit** | Man-in-the-middle / general exploitation framework | Reflective loading, API call chains |

### General analysis routine

```text
1. Open in dnSpyEx (usually not obfuscated; a few teams add ConfuserEx)
2. Look at Program.Main or the entry command dispatch (Rubeus uses a switch(command) structure)
3. Find the implementation class/method of the target command
4. Look at the P/Invoke sections (Interop.* namespaces) —— native API calls are here
5. Extract embedded resources (some tools embed config/templates)
6. If signatures need changing (EDR evasion): change command strings, API calls, string constants
```

### Rubeus structure example

Rubeus uses command dispatch, one class per subcommand. To find the Kerberoasting logic:

```text
Entry: Rubeus.CommandLineParser → parse args
Dispatch: switch(command) → "kerberoast" → execute Ask.TGS(...)
P/Invoke: Rubeus.Interop.Lsa* / Native.cs → native Kerberos API
Key: LsaCallAuthenticationPackage (KERB_RETRIEVE_TKT_REQUEST)
```

Changing signatures (evasion): rename the command string `"kerberoast"` to a custom name, change the `Rubeus` banner string, change the P/Invoke call order.

### Embedded configuration extraction

Many loaders/tools embed C2, keys, and certificates encrypted in resources or fields:

```powershell
# In dnSpyEx look at Resources (the resource tree)
# Or from the command line
powershell -c "[System.Reflection.Assembly]::LoadFile('target.exe').GetManifestResourceNames()"
# After finding a resource, in dnSpyEx right-click → extract / Save
```

For runtime-decrypted configuration → dynamically break at the decryption method's return point and dump the plaintext (see `common-workflow.md`).

---

## Tool Installation Matrix

### Windows (preferred, dnSpyEx is a GUI)

```powershell
# Option A: Chocolatey
choco install dnspy ilspy de4dot detect-it-easy

# Option B: manually download releases (recommended, version control)
# dnSpyEx:    https://github.com/dnSpyEx/dnSpy/releases
# de4dot:     https://github.com/de4dot/de4dot/releases
# ILSpy:      https://github.com/icsharpcode/ILSpy/releases
# DIE:        https://github.com/horsicq/Detect-It-Easy/releases
# dnlib:      dotnet add package dnlib  (NuGet)
```

### Linux / macOS (no dnSpyEx GUI, use the CLI)

```bash
# ILSpy CLI decompilation
dotnet tool install -g ilspycmd
ilspycmd target.exe -p -o outdir/         # decompile into a directory

# de4dot cross-platform (needs mono or dotnet)
# Download the de4dot .dll artifacts from the release, run with dotnet
dotnet de4dot.dll target.exe -o target-clean.exe

# dnlib (scripting, needs the dotnet SDK)
dotnet new console -o dnclean && cd dnclean
dotnet add package dnlib

# DIE CLI (diec)
# Linux: install from https://github.com/horsicq/Detect-It-Easy
diec target.exe
```

### .NET runtime prerequisites

```bash
# Linux
sudo apt install dotnet-runtime-8.0        # or 6.0/7.0 depending on the target
# macOS
brew install --cask dotnet-sdk
```

> dnSpyEx (with IL editor + debugger) only has a Windows GUI version. .NET reverse engineering on Linux/macOS can only use `ilspycmd` decompilation + `dnlib` script patching; there is no equivalent interactive debugging GUI. When patching is needed, prefer Windows.

---

## dnSpy MCP Integration

The community already has several dnSpy MCP projects that expose dnSpy's decompilation/IL inspection as MCP tools the AI can call directly —— fully aligned with reverse-skill's MCP philosophy.

### Mainstream dnSpy MCP projects

| Project | Features | Fit |
|------|------|------|
| **soufianetahiri/dnspy-mcp** | Core MCP Server, exposes tools like decompile, IL inspection | Claude Code / Cursor |
| **AgentSmithers/DnSpy-MCPserver-Extension** | Runs as a dnSpyEx extension, deeply integrated with the GUI | Loaded inside dnSpyEx |
| **malwarecakefactory/dnspy-mcp-extension** | 33 tools, covering the whole triage → deobfuscation pipeline | Full-pipeline automation |

### Registering in the Claude MCP config

After installing the dnSpyEx extension per the respective project README, register it in `~/.claude/mcp.json` (exact command/args per the project README):

```json
{
  "mcpServers": {
    "dnspy": {
      "command": "dotnet",
      "args": ["path/to/dnspy-mcp.dll"]
    }
  }
}
```

Once registered, this skill's AI integration path: the user says "analyze this .NET" → routed to `dotnet-reverse/` → prefer calling the `dnspy_decompile` / `dnspy_inspect_il` tool surface → fall back to the GUI if that fails.

> dnSpy MCP is not a reverse-skill built-in bootstrap capability; the user must manually install the extension and register it per the project README. It may be considered for `bootstrap-manifest.json` later.

---

## Community Resource Index

### Highly recommended

- **Washi's blog** — .NET reverse engineering expert: https://blog.washi.dev/posts/misconceptions-about-dotnet/
  - Core point: **do not over-rely on dnSpy's C# decompiler; get familiar with the IL editor** (consistent with this project's IL-first principle)
- **dnSpyEx** — the actively maintained fork of dnSpy: https://github.com/dnSpyEx/dnSpy
- **de4dot** — .NET deobfuscation: https://github.com/de4dot/de4dot
- **dnlib** — metadata programming: https://github.com/dnlib/dnlib

### Hands-on tutorials

- Medium "De-obfuscating and reversing a .NET/C# spyware" — hands-on dnSpy + de4dot deobfuscation of an info-stealer
- YouTube "dnSpy Patch .NET EXEs & DLLs" — step-by-step patching + keygen
- Kanxue Forum .NET reverse engineering board — search ".net reverse" / "dnSpy" / "ConfuserEx" for many hands-on posts, Nuitka reversing, AV evasion discussions
- Guided Hacking "Top 5 .NET Reverse Engineering Tools" — dnSpy still ranks first
- StackExchange / Reverse Engineering — advanced topics like debugging `DynamicMethod`

### Existing .NET resources in this repository (integration)

- `reverse-engineering/tools.md` `.NET Analysis` section — dnSpy/ILSpy tool quick reference + the Codegate 2013 two-stage XOR+AES-CBC pattern
- `reverse-engineering/field-notes.md` `.NET` section — tool notes
- `reverse-engineering/awesome-re-resources.md` — de4dot is listed
- `field-journal/seed-014_unity-il2cpp-reverse.md` — Unity IL2CPP (native side, complementary to the .NET managed layer)

In-depth .NET reverse engineering content is consolidated into this module; `reverse-engineering/` keeps only quick-reference indexes.
