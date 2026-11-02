# Non-PE / Multi-Format Agent Response Cookbook U–AV (Issue #65)

> Companion to the PE anti-debugging cookbook A–T (../anti-analysis.md): organized by **file type**, giving "trigger → one-line action → Evidence".  
> This is **not** a second main workflow. Once Triage identifies the type, jump to the matching skill + this table.  
> Default: **authorized isolated lab / authorized samples and devices**. For device wiping, BYOVD, reflective injection, etc., write **detection and forensics** only — no unauthorized destruction/exploitation tutorials.  
> Failed bypasses or recoveries MUST also be recorded as Evidence; silently treating them as "harmless" is forbidden.

## 0. Routing Quick Reference

| Type clue | Primary skill | Section in this table |
|----------|----------|----------|
| .bat / .cmd / batch | malware-analysis | §1 |
| .ps1 / PowerShell | malware-analysis | §2 |
| Office macro / VBA / .docm/.xlsm | malware-analysis | §3 |
| Web/frontend JS obfuscation, JSVMP | js-reverse | §4 |
| .sys / kernel driver | 
everse-engineering/kernel-driver-reverse.md + cre | §5 |
| .dll focus | malware-analysis / 
e-agent-workflow | §6 (deduplicated against A–T) |
| APK / Magisk / hidden icon | pk-reverse | §7–§8 |

When Triage identifies the file type as script/macro/JS/APK/DLL/SYS: complete the **P0 anchor Evidence** for that type before digging deeper; PE anti-debugging still goes through A–T.

## 1. BAT/CMD (U V W)

| ID | Trigger | Action (summary) | Evidence | Priority |
|----|------|--------------|----------|------|
| **U** | Many single-character SET variables + %a%%b% concatenation, or ^ line continuations splitting commands | Expand SET line by line; list the restored commands; a batch deobfuscation tool may be used; **forbidden** to treat as "no action" without restoring first | E-batch-deobf | P0 |
| **V** | Text opens as garbage; hex header FF FE (UTF-16 LE BOM) | Confirm BOM → convert to UTF-8 then parse; or chcp 65001 + type | E-batch-encoding | P2 |
| **W** | Lots of REM/:: and redundant GOTO/labels drowning the real logic | Strip comments; trace the real GOTO paths; execute in isolation and capture the actual cmd command log | E-batch-deadcode | P1 |

## 2. PowerShell (X Z)

> Numbering keeps the proposer's convention: **no patch Y**.

| ID | Trigger | Action (summary) | Evidence | Priority |
|----|------|--------------|----------|------|
| **X** | Multiple layers of FromBase64String / Gzip / Compress / nested -replace | Decode **layer by layer**; record each layer's result separately; tools optional (PowerDecode etc.), otherwise manual/script | E-ps-decode-layer-N | P0 |
| **Z** | Reversed strings, fragments + concatenation feeding Invoke-Expression/IEX | Restore the full string; break on IEX or use script-block logging; put the plaintext command into Evidence | E-ps-string-restore | P1 |

## 3. VBA Macros (AA AB AC)

| ID | Trigger | Action (summary) | Evidence | Priority |
|----|------|--------------|----------|------|
| **AA** | olevba/OLEDump show only P-Code, source stream empty (VBA Stomping) | P-Code decompiler tools; if incomplete, observe via Word/Excel macro debugging; document the limitations | E-vba-pcode | P0 |
| **AB** | Lots of Chr() concatenation or Base64 strings; suspected shellcode/nested script | Restore the string via Immediate window/script; determine the type after decoding; dynamically watch CreateObject/Shell | E-vba-str-decode | P1 |
| **AC** | Meaningless If 1=2, or InsertLines/DeleteLines self-modification | Statically follow the real branch; dynamically bp the self-modifying API and dump the modified macro | E-vba-selfmod | P2 |

## 4. JavaScript (AD AE AF) → primary path js-reverse

| ID | Trigger | Action (summary) | Evidence | Priority |
|----|------|--------------|----------|------|
| **AD** | Custom bytecode array + while/switch interpreter (JSVMP) | Locate the VM entry and opcode dispatch; dynamic log traces; dual-track AST + dynamic; see js-reverse DeepDive | E-js-vmp | P0 |
| **AE** | while(1){switch} + large string-array indexing | Rebuild with AST/Babel; restore strings from array indices; wakaru etc. optional; do **not** paste the whole long-form PE ollvm-deobfuscation doc | E-js-deobf | P0 |
| **AF** | debugger statements, console hijacking, performance.now deltas, DevTools detection | Disable breakpoints / pin the time source / headless browser; patch the detection points; authorized pages only | E-js-anti-debug | P1 |

## 5. SYS Kernel Drivers (AG AH AI)

| ID | Trigger | Action (summary) | Evidence | Priority |
|----|------|--------------|----------|------|
| **AG** | DriverEntry is very short; the logic is not in the entry point | Scan MajorFunction[] non-empty slots; prioritize IRP_MJ_DEVICE_CONTROL/CREATE; put the address list into evidence | E-driver-irp-handlers | P0 |
| **AH** | DeviceIoControl / IOCTL dispatch present | Build a control-code → handler-function table; mark METHOD_* and buffering direction; map the usermode communication surface | E-driver-ioctl | P0 |
| **AI** | Sample loads/drops a well-known vulnerable driver or an anomalously signed driver (BYOVD pattern) | Cross-check against **public** lists such as LOLDrivers; record driver name/hash/signature; analyze the **invocation intent**; do **not** expand into exploit steps | E-driver-byovd | P1 |

See the kernel-driver-reverse.md workflow for details; this table only adds agent action anchors.

## 6. DLL (AJ–AQ) — deduplicated against A–T / #72

| ID | Trigger | Action (summary) | Evidence | Priority |
|----|------|--------------|----------|------|
| **AJ** | DLL analysis only looked at exports/EP, ignoring TLS or DllMain | **Both TLS callbacks + DllMain MUST be examined**; dynamic breakpoint order still follows the four-stage rocket (TLS→EP/DllMain→API→ExitProcess) | E-dll-tls-dllmain | P0 |
| **AK** | Export names benign-then-malicious, misnamed, or exports inconsistent with behavior | Cross-reference the export table against actual calls; list anomalous exports | E-exports-anomaly | P0 |
| **AL** | No or very few exports, yet still loaded | Locate via entry point, strings, xrefs, callers; do not give up just because of "no exports" | E-dll-noexport | P0 |
| **AM** | Need to restore exported-function parameters and calling conventions | **See A–T patch R** (Delay-Load / E-delay-import); no duplicated long-form here | E-delay-import | P0 pointer |
| **AN** | Need to recover exported-function arguments and calling convention | Cross-references + dynamically inspect registers/stack; annotate stdcall/fastcall etc. | E-dll-export-abi | P1 |
| **AO** | Suspected DLL hijacking/sideloading | Check for same-named DLL in the application directory, the search path, KnownDLLs; legitimate program + anomalous DLL combinations | E-dll-sideload | P1 |
| **AP** | Fileless mapping / reflective loading clues | Memory artifacts, loader behavior, pathless modules; forensics in an authorized environment | E-dll-reflective | P1 |
| **AQ** | Risk downgraded solely because export names "don't look malicious" | **Forbidden** to judge safety from export names alone; combine segment permissions, entry point, strings, dynamic behavior | E-dll-export-priority | P1 |

The DLL/SYS hard gates remain: E-imports + E-exports (see 
e-agent-workflow).

## 7. Android Device Wiping / Persistence (AR AS AT) → pk-reverse

> **Authorized samples, images, or test devices only.** The action is detection, IOC extraction, and mapping persistence paths — not carrying out destruction.

| ID | Trigger | Action (summary) | Evidence | Priority |
|----|------|--------------|----------|------|
| **AR** | Magisk module/script contains database deletion, flashing, mass rm of system partitions, or other **device-wiping signature commands** | Signature command table + module path; flag high-risk destructive capability; do not execute the wiping commands | E-android-wiper-cmd | P0 |
| **AS** | Looping curl|sh / remote script pulls, unconventional C2 URLs | Extract URLs; analyze whether the downloaded payload contains wiping commands; record temporary paths | E-android-wiper-backdoor | P0 |
| **AT** | /data/adb/service.d, post-fs-data.d, suspicious /system/priv-app, etc. | List persistence scripts/APKs; put content summaries into evidence | E-android-persistence | P1 |

## 8. Android Transparent/Hidden Icons (AU AV) → pk-reverse

| ID | Trigger | Action (summary) | Evidence | Priority |
|----|------|--------------|----------|------|
| **AU** | Fully transparent LAUNCHER icon/empty label, Theme.NoDisplay, missing LAUNCHER category, disabled component | apt dump badging + manifest; decompile to inspect icon pixels; put anomalies into evidence | E-android-hidden-icon-manifest | P0 |
| **AV** | Installed but no desktop icon; background traffic/auto-start/high-risk permissions/dynamic icon restoration | pm list vs desktop; dumpsys package; broadcasts and device_admin; put behavior into evidence | E-android-hidden-icon-behavior | P1 |

## 9. Constraints (Global)

1. **Not a parallel main workflow**: stage gates still defer to 
e-agent-workflow / each skill.  
2. **Evidence MUST be recorded**: including failures, partial recoveries, quality= annotations.  
3. **Deduplicate against A–T**: no PE anti-debugging duplication; AM→R; AJ adds the DLL perspective without overturning the TLS rocket.  
4. **Missing tools**: record n/a + manual equivalent; do not pretend a commercial suite was used.  
5. **Authorization**: destructive/injection/driver-vulnerability categories get defensive analysis and forensic phrasing only.

## 10. P0 Minimal Checklist (when a type matches)

`	ext
□ bat/cmd → U (+ V/W when needed)
□ ps1 → X (+ Z)
□ vba → AA (+ AB/AC)
□ heavily obfuscated js → AD or AE (+ AF)
□ sys → AG + AH (+ AI if BYOVD suspected)
□ dll → AJ + AK/AL; Delay-Load goes through R
□ apk destructive/hidden → AR/AS or AU (+ AT/AV)
`
