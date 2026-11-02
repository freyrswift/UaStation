# RE Agent Workflow Gates (Static ↔ Dynamic)

> Source inspiration: binary-re phase division, community RE skills (Frida/r2/Ghidra/IDA loop), the Cerberus three-headed loop (static/dynamic/instrumentation)  
> Issue #65 increments: the IAT repair iron law, six-phase mapping, .NET/DLL·SYS equivalent paths; the user-instruction feasibility gate; bypass patches 6–10; anti-debug/obfuscation recipes A–T; non-PE multi-format recipes U–AV (2026-08-12)  
> Applies to: `reverse-engineering/`, `ida-reverse/`, `radare2/`, `malware-analysis/`, and handoff with the cre role

## 0. Startup

```text
□ scope.md: offline sample path or authorized device / lab machine
□ tool-index: actual paths for file/strings/r2/ida/frida etc.
□ role: cre (ops/role-map)
```

## 0.5 User-Instruction Feasibility Gate (Issue #65)

**Principle**: obey the user's **goal**, not blindly follow the user's **step order**. Before skipping steps you must state the prerequisite and ask for confirmation; mandatory steps confirmed afterward must be done, with Evidence quality honestly labeled.

| Situation | Agent MUST |
|------|------------|
| User wants X, and the current state can yield **valid** Evidence | Execute X, update Evidence |
| User wants X, but a **known blocking prerequisite** exists (e.g., packing confirmed and static IAT unreadable) | **Forbidden** to pretend a meaningful IAT is complete; ① state the blockage in one sentence; ② give the recommended order (unpack/repair IAT first, or go straight to dynamic API capture); ③ **ask the user to confirm** whether to "still force reading the current garbage table" or "follow the recommended order" |
| User explicitly **forces** the current step (e.g., wants the IAT even unpacked) | Execute and record Evidence, MUST label `quality=unreadable` / `packed` (or equivalent); **forbidden** to draw conclusions like "no network capability" from it |
| User accepts the recommended order | Do the prerequisite steps first; after completion do X automatically or on request; **forbidden** to pass the prerequisite step (e.g., unpacking) off as "import table check completed" |

**Relation to "redo X"**: redoing X still = redoing the named step (or its negotiated, confirmed legitimate prerequisite); swapping in an unrelated step is forbidden. Unpacking is a **prerequisite** for the import table, not a **substitute** for it.

Typical conflict: the user says on a packed sample "don't unpack yet, look at the import table first" → packers routinely tamper with the import directory / encrypt descriptors, making the static table garbage and meaningless → follow the "blocking prerequisite" row of this table; neither silently unpacking as a substitute nor silently handing over the garbage table as done is allowed.

## 1. Triage (5–15 minutes · mandatory starting point)

```text
□ Compute the sample hash (MD5/SHA256) → unique ID
□ Identify file type: EXE / DLL / SYS / ELF / Mach-O / .NET / script (bat/ps1/vba) / JS / APK etc.
□ Non-PE/script/APK/driver specifics: see §3.4 and `references/nonpe-format-cookbook.md` (U–AV)
□ file / DIE / entropy / packer signatures (PEiD / DIE / Exeinfo etc.)
□ Architecture: x86 / x64 / ARM; compiler language clues (VC++ / Delphi / .NET / Go / Rust)
□ Packing type clues: UPX / ASPack / VMProtect / Themida / unknown obfuscation
□ strings / rabin2 -z to catch stragglers
□ MUST import/export anchor (see "Import Table Hard Gate and Equivalent Paths" below); if the user jumps ahead and there is a packer → go through §0.5 first
□ Output: E-triage (MUST include imports or an equivalent anchor category summary, with quality labeling where applicable) + a list of hypotheses
```

**Phase gate (Triage → Static/Dynamic)**: before imports **or** a legitimate equivalent anchor summary is recorded in E-triage, you MUST NOT enter Dynamic (unless an IAT repair failure is already recorded and the dynamic bypass chosen, see §1.2), nor claim "basic triage complete". On parse failure you still MUST write the failed output into Evidence; skipping is not allowed. When the user asks to "redo the import table check", you MUST redo the imports/equivalent step itself (or first complete the prerequisite negotiated under §0.5); swapping in other analysis steps as a substitute is forbidden.

### 1.1 Import Table Hard Gate and Equivalent Paths

| Sample Type | MUST Anchor (Evidence) | Notes |
|----------|----------------------|------|
| Native PE/ELF/Mach-O (readable IAT) | `E-imports` / `E-triage-imports`: import category summary | `rabin2 -i` / IDA imports / equivalent |
| DLL / SYS / shared library | **Both** `E-imports` + `E-exports` (`rabin2 -i` + `rabin2 -E`) | The export table has the same priority as the import table (external entry points) |
| .NET managed (no traditional IAT) | **Equivalent path**: dnSpy/IL/metadata/assembly-reference and sensitive-API summary → still written into the `E-imports` or `E-triage-imports` semantic slot | **Forbidden** to skip the hard gate because "there is no IAT"; inspecting in dnSpy = the native "check the import table" |
| Import table parse failed / empty / packer garbage | Still record the failure or garbage-table output as Evidence, and label `quality` | Silent skipping not allowed; a garbage table must not support capability-negation conclusions |

**Clean-import-table warning (MUST flag)**: if the import table is "too clean" (only base DLLs like kernel32/ntdll, almost no business APIs), strongly suspect `LoadLibrary` + `GetProcAddress` dynamic loading → note the suspicion in Evidence, and **SHOULD** switch to Dynamic to capture in-memory APIs; do not claim "no network/no file capability" from the static IAT alone.

**High-risk API combinations (patch 8 · SHOULD)**: when the import table is long, output **malicious-combination clusters** first and filter out pure system base calls. Examples (non-exhaustive):

- High-risk cluster: `FindWindowA/W` + `WriteProcessMemory` + `CreateRemoteThread` (injection)
- High-risk cluster: `CryptEncrypt` / `CryptAcquireContext` + lots of `FindFirstFile` / `DeleteFile` (ransomware tendency)
- High-risk cluster: `InternetOpen` / `WinHttp` / `URLDownloadToFile` + persistence APIs (`RegSetValue` / `CreateService`)
- `CreateFile` / `ReadFile` alone is mostly benign noise unless co-occurring with the clusters above

### 1.2 Unpacking and IAT Handling (high-risk fork · Issue #65)

```text
Branch A: no packer / .NET managed
  → go straight to §2 Static (.NET takes the equivalent anchor)

Branch B: packed / heavily obfuscated
  Step 1: attempt unpacking (automatic unpacker / manual OEP hunt) — must be in an authorized, isolated environment
  Step 2: attempt IAT repair
    Tools: x86 → ImportREC (or equivalent); x64 → Scylla (or equivalent). Grinding ImportREC on a 64-bit sample is forbidden.
    Case B1: repair succeeds and parses → record E-imports (post-repair) → §2 Static
    Case B2: ImportREC/Scylla errors, the repaired binary won't run, or the IAT is all garbage (VMP/encrypted packers)
      → [IAT repair iron law] immediately terminate further static IAT repair
      → MUST record E-iat-repair-fail (commands, tools, failure symptoms, decision to go dynamic)
      → go straight to §3 Dynamic: API breakpoints / hardware execution breakpoints / memory search to capture imports
      → this does not count as "skipping the import table": the import-table path was attempted and recorded in Evidence
    Case B3 (patch 6): after unpacking and IAT repair, double-click crashes / BSODs (suspected file CRC/size self-check)
      → give up further static file repair; record E-self-check-crash or fold into E-iat-repair-fail
      → switch to §3 Dynamic: break on CreateFile / GetFileSize / hash-related APIs to locate the check bypass point
```

**IAT repair iron law (MUST)**: prefer automatic/semi-automatic repair; as soon as the repair tool errors or the repaired program won't run, **stop immediately** grinding on the static import table, switch to dynamic debugging, and capture imported functions at runtime with API breakpoints (e.g., `bp CreateFile` / key network APIs).

## 2. Static (basic static anchors → deep dig)

| Tool | When |
|------|------|
| radare2 / rabin2 | Fast functions/imports/strings (imports already done as a Triage MUST or failure-bypass recorded) |
| IDA / Ghidra (MCP or headless) | Deep digging, cross-references, types; recheck import categorization during the survey phase |
| jadx / dnSpy | Android / .NET |
| OLLVM docs | Control-flow flattening suspected |

```text
□ Confirm E-imports / E-triage already contains the import table or an equivalent anchor Evidence (backfill first if missing; deferring is forbidden)
□ If DLL/SYS: confirm E-exports is recorded
□ Sensitive API grouping + high-risk combination clustering (patch 8)
□ Hardcoded domain/IP/URL strings; whether a resource section hides a payload
□ Locate key functions (crypto/checks/network/licensing) → write addresses/symbols into Evidence
□ One road blocked → switch tools (IDA↔r2↔Ghidra)
□ Time box (patch 9 · SHOULD default): ~15 minutes of static digging with no key path → force a switch to §3 Dynamic (user/task may override the duration)
```

**Without MCP**: you can export decompiled text and analyze it (cf. the P4nda0s reverse-skills / IDA-NO-MCP approach), still writing Evidence paths.

## 3. Dynamic (cross-validation loop zone)

Core idea: **static provides leads → dynamic validates → validation stalls → return to static for re-review** (no fixed single order).

### 3.0 Breakpoint Opening Moves (patches 7 + 10 · MUST order)

Before launching the sample in a user-mode debugger (x64dbg etc.), preset breakpoints as a "four-stage rocket" (names may differ slightly by architecture/tool; the order does not):

1. **TLS callback** breakpoint (may already have run before the debugger's EP)
2. **Entry point EP** breakpoint
3. **Sensitive API** breakpoints (e.g., `CreateRemoteThread` / network / file writes)
4. **Backstop**: `ExitProcess` / process-exit-path breakpoint (patch 10) — once it exits directly due to anti-debugging, **don't rush to restart**; dump memory immediately and write the pre-crash image path into Evidence for string/data recovery

```text
□ Frida / x64dbg / gdb / emulator: validate static hypotheses
□ Run only after presetting breakpoints per §3.0; single-step track the stack/registers (white box)
□ Behavior monitoring: sandbox / Procmon / RegShot (black box)
□ IAT-repair-failed / self-check-crash samples: hardware execution breakpoints or memory search to forcibly capture APIs; CreateFile/GetFileSize to find CRC checks
□ Anti-debug / anti-Frida → reverse-engineering/anti-analysis
□ Android: generate root-detection / SSL-pinning bypass scripts as needed, **must be on an authorized device**
□ Crash logs drive the next round of hooks (adaptive loop)
□ Time box (patch 9 · SHOULD default): ~200 single-stepped instructions with no malicious-behavior leads → force a return to static string searching / re-anchor (overridable)
```

### 3.1 Sandbox / Dynamic No-Behavior Emergency Branch (MUST)

```text
No behavior or immediate exit / infinite sleep
  → check anti-debug / anti-VM routines (CPUID, high-resolution timing, sandbox fingerprints, etc.)
  → try hardware breakpoint bypasses, patching detection points, or switch to a physical machine / higher-fidelity environment
  → write "no behavior + suspected anti-VM" into Evidence; writing "sample harmless" without qualification is forbidden
```

### 3.2 Time box Strategy (patch 9 · SHOULD)

| Phase | Default Threshold (overridable by user/task) | Action |
|------|------------------------------|------|
| Static digging with no key path | ~15 minutes | switch to Dynamic |
| Dynamic single-stepping without progress | ~200 instructions | back to Static string/xref re-anchoring |
| Any path repeatedly failing | record Evidence, then switch tools or bypass | spinning on the same failed technique is forbidden |


### 3.3 Anti-Debug / Obfuscation Bypass Quick Reference (Issue #65 patches A–T · high frequency)

Full index and action details are in `reverse-engineering/anti-analysis.md`, "Agent response cookbook A–T". Only **P0 must-checks + common transitions** are listed here. Default to an **authorized isolated lab**; patching/flag-tweaking is not an unauthorized-production action.

| Trigger Signature | First Action (summary) | Evidence |
|----------|------------------|----------|
| `cpuid` followed by jz/jnz (A) | Lab: tweak flags or patch to the real branch; record the check address | `E-anti-debug-cpuid` |
| `rdtsc` + sub/cmp (B) | bp rdtsc or hook the time source; spinning until the sandbox times out and calling it "harmless" is forbidden | `E-anti-debug-rdtsc` |
| PEB BeingDebugged / NtGlobalFlag (K) | ScyllaHide or manually patch the PEB; patch the conditional jump | `E-anti-debug-peb` |
| `NtQueryInformationProcess` DebugPort/Flags/Object (P) | ScyllaHide / hook the return value; record the class parameter | `E-anti-debug-ntqip` |
| Few imports but rich behavior → API hashing (N) | bp GetProcAddress; hash-lookup and backport into IDA | `E-api-hash` |
| Empty strings but network/file behavior → string encryption (I) | find the decode routine's xref; dump after decryption and backport | `E-string-decrypt` |
| Signed but from a dubious source (F) | SigCheck: valid/revoked/timestamp; an invalid signature does **not** lower the threat rating | `E-sig-forge` |
| No IOCs from standard strings → try wide chars (T) | `strings -el` / UTF-16LE; Alt+A unicode | `E-wide-strings` |
| Debugger-name strings / Toolhelp scans (C) | bp the CreateToolhelp32Snapshot chain | `E-anti-debug-procscan` |
| AddVectoredExceptionHandler + deliberate exceptions (D) | bp VEH registration; analyze the handler | `E-anti-debug-veh` |
| int3 / DR0–DR7 (M) | patch int3; software breakpoints or ScyllaHide to hide hardware BPs | `E-anti-debug-bp` |
| Multiple PE headers / overlapping sections (G) | real section-table mapping + entropy; don't trust section names | `E-pe-anomaly` |
| File tail > sum of sections = Overlay (J) | extract the overlay; file/entropy; find the loading-offset xref | `E-overlay` |
| Abnormally large/high-entropy .rsrc RT_RCDATA (Q) | extract resources; FindResource chain + decrypt-dump | `E-rsrc-payload` |
| DLLs loaded only at runtime (R) | check Delay Imports; bp the delay-load helper | `E-delay-import` |
| while+switch star-shaped CFG (H) | **See** `ollvm-deobfuscation.md`; if plugins fail, take the dynamic path | `E-cff` |
| Always-true/always-false branches (S) | **See** ollvm / symbolic execution; dynamic is authoritative | `E-opaque-pred` |
| `/proc/self/status` TracerPid (L) | **Linux/ELF**; hook or patch; not mandatory on the primary Windows path | `E-anti-debug-tracerpid` |

**Constraints**: record Evidence even when a bypass fails; writing "anti-debug triggered exit" as "sample harmless" is forbidden. The full A–T plus P2 (E compile time, O junk instructions) are in the anti-analysis recipe section.

### 3.4 Non-PE / Multi-Format Bypass (Issue #65 patches U–AV · routing)

Full index: `reverse-engineering/references/nonpe-format-cookbook.md`. Only **type → entry point** is listed here; action details live in the cookbook / respective skills.

| Type | Jump To | P0 Evidence Anchors (examples) |
|------|------|---------------------------|
| BAT/CMD | cookbook §1 + malware | `E-batch-deobf` |
| PowerShell | cookbook §2 + malware | `E-ps-decode-layer-N` |
| VBA macros | cookbook §3 + malware | `E-vba-pcode` |
| Heavily obfuscated JS / JSVMP | **js-reverse** + cookbook §4 | `E-js-vmp` / `E-js-deobf` |
| SYS drivers | kernel-driver-reverse + cookbook §5 | `E-driver-irp-handlers` / `E-driver-ioctl` |
| DLL focus areas | cookbook §6 (AM ≡ A–T **R**) | `E-dll-tls-dllmain` / `E-exports` |
| Android wipe/hidden-icon | **apk-reverse** + cookbook §7–8 | `E-android-wiper-*` / `E-android-hidden-icon-*` |

**Constraints**: do not start a separate "non-PE six phases"; division of labor with §3.3 A–T (PE anti-debug vs multi-format). Authorized lab; wipe/BYOVD/reflective = detection-forensics phrasing.


## 4. Synthesis (IOC / attack chain / report)

### Decision quality overlay (Issue #77)

Before closing Synthesis, apply [analysis-decision-framework.md](../../ops/analysis-decision-framework.md) **P0 checklist**: R41 grounded claims, R4* validated sufficiency, R1 confidence->dynamic, R2 hypothesis exit, R43 deadlock replan (under feasibility gate), R8/R23 no default malice/IOC. Multi-module -> R50; anti-analysis effort -> R51 + A-T cookbook.

Blindspots (Rust/Go/VMP/injection/OLE/PDF/agent-meta): [analysis-blindspot-cookbook.md](../../ops/analysis-blindspot-cookbook.md) R52-R81 — detection-oriented; not a parallel master flow.



```text
□ Finding: algorithm / check logic / exploitable point / behavior conclusions
□ Path: callflow or solve steps with E-* attached
□ IOC: network fingerprints + host fingerprints (table if present; otherwise n/a + reason)
□ Report via docs-generator (malware/apt/null/vuln overlay chosen per task) + optional diagrams
□ Optional: YARA / Snort·Suricata rule formalization
□ field-journal sanitized
```

## 5. Six-Phase Field Mapping (Issue #65 mind map → this file)

| Field Phase | Section in This File | Hard Gates / Iron Laws |
|----------|------------|-------------|
| 1 Initial rapid assessment | §0–§1 Triage | Hash, architecture, file type, packer detection; imports/equivalent anchors; §0.5 instruction gate |
| 2 Unpacking and IAT | §1.2 | IAT iron law; failure/self-check crash → Evidence → Dynamic |
| 3 Basic static anchors | §2 Static | High-risk API combinations; time box SHOULD |
| 4 Deep cross-validation | §3 Dynamic | Four-stage breakpoint rocket; no-behavior emergency; time box; §3.3 A–T; §3.4 U–AV type routing |
| 5 Extract IOCs and the attack chain | §4 Synthesis | IOC + Kill Chain / Path |
| 6 Archive and rule formalization | §4 + docs-generator / YARA | Structured report; rules optional |

## 6. Differences from "Piles of RE Skill Plugins"

- This pack uses **phase gates + tool-index** and does not enable Hex-Rays "unsafe fully-automatic execution"-style plugins by default  
- Dynamic instrumentation defaults to an **offline/lab** network_profile  
- IAT/import table: **attempt + record** beats "infinite static grinding" or "silent skipping"  
- User instructions: **goal first + prerequisite negotiation**; substituting unrelated steps for the named step is forbidden
