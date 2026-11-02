# EDR Hook Survey Quick Reference

> Authorized red teaming / adversary emulation / own-product testing only; use against unauthorized targets is forbidden.

This document summarizes the user-mode and kernel-mode monitoring points of mainstream EDRs / AVs, to help the red team's reconnaissance phase quickly pin down "what needs handling".

## 1. Mainstream EDR Fingerprints and Hook Patterns Quick Reference

| Vendor / Product | User-Mode Components | Kernel Drivers | Main Monitoring Surfaces |
|------------|-----------|---------|-----------|
| CrowdStrike Falcon | `CSFalconService.exe`, `CSAgent.sys` injected into target processes | `CSAgent.sys`, `CSBoot.sys` | Heavy kernel callbacks + ETW-TI; fewer user-mode hooks (cloud lookups) |
| Microsoft Defender for Endpoint (MDE) | `MsMpEng.exe`, `MpClient.dll` | `WdFilter.sys`, `WdBoot.sys`, `WdNisDrv.sys` | AMSI + ETW-TI + ntdll inline hooks + comprehensive kernel callbacks |
| SentinelOne | `SentinelAgent.exe`, `SentinelHelperService.exe` | `SentinelMonitor.sys`, `SentinelDeviceControl.sys` | Heavy ntdll user-mode hooks + kernel callbacks + its own ETW provider |
| Elastic Defend (formerly Endpoint Security) | `elastic-endpoint.exe` | `elastic-endpoint-driver.sys` | Mainly ETW + a few ntdll hooks, with uploads via Elastic Agent |
| ESET | `ekrn.exe`, `eamsi.dll` | `eamonm.sys`, `epfwwfp.sys` | Very many user-mode hooks (NtCreateFile / NtOpenProcess etc.) |
| Sophos Intercept X | `SophosFileScanner.exe`, `SophosNtpService.exe` | `SophosED.sys`, `hmpalert.sys` | ntdll hooks + HMPA memory protection + kernel callbacks |
| Kaspersky | `avp.exe`, `klif.sys` | `klif.sys`, `klhk.sys` | Heavy user-mode hooks + KLIF's own mini-filter + network filter drivers |
| Trend Micro Apex One | `TmListen.exe`, `TmCCSF.dll` | `tmcomm.sys`, `tmactmon.sys` | User-mode hooks + behavior-monitoring drivers |
| Carbon Black | `RepMgr.exe`, `RepWAV.exe` | `ParityDriver.sys` | Leans kernel callbacks + ETW |

### Quick Fingerprinting Script

```powershell
$edrSigs = @{
    'CSAgent'           = 'CrowdStrike Falcon'
    'SentinelAgent'     = 'SentinelOne'
    'elastic-endpoint'  = 'Elastic Defend'
    'ekrn'              = 'ESET'
    'MsMpEng'           = 'Microsoft Defender'
    'SophosFileScanner' = 'Sophos Intercept X'
    'avp'               = 'Kaspersky'
    'TmListen'          = 'Trend Micro Apex One'
    'cb'                = 'Carbon Black'
}

Get-Process | ForEach-Object {
    foreach ($k in $edrSigs.Keys) {
        if ($_.ProcessName -match $k) {
            "[+] $($edrSigs[$k]) detected: $($_.ProcessName) (PID $($_.Id))"
        }
    }
}

Get-ChildItem 'C:\Windows\System32\drivers\*.sys' |
    Where-Object { $_.Name -match 'CSAgent|Sentinel|elastic|eam|WdFilter|Sophos|klif|tmcomm|Parity' } |
    Select-Object Name, VersionInfo
```

## 2. Key User-Mode ntdll Hook Functions

`ntdll.dll` exports that EDRs almost certainly hook (grouped by ATT&CK behavior):

| Function | Monitored Behavior | ATT&CK |
|------|-----------|--------|
| `NtCreateThreadEx` | Remote thread injection, QueueUserAPC injection | T1055.002 / T1055.004 |
| `NtAllocateVirtualMemory` | Shellcode allocating RWX memory | T1055 |
| `NtAllocateVirtualMemoryEx` | Cross-process memory allocation (new Win10+ API) | T1055 |
| `NtProtectVirtualMemory` | Changing page permissions RW→RX | T1055 |
| `NtWriteVirtualMemory` | Cross-process shellcode writes | T1055.012 |
| `NtMapViewOfSection` | Section-based injection (Process Doppelganging / Ghosting) | T1055.013 |
| `NtCreateSection` | Pairs with MapViewOfSection | T1055.013 |
| `NtOpenProcess` | Opening a target process for a handle | T1057 |
| `NtQueueApcThread` / `NtQueueApcThreadEx` | APC injection | T1055.004 |
| `NtCreateProcess` / `NtCreateProcessEx` / `NtCreateUserProcess` | Child process creation (incl. PPID spoofing) | T1106 |
| `NtSetContextThread` | Changing thread context (thread-hijack injection) | T1055.003 |
| `NtResumeThread` | Resuming a thread after injection | T1055 |
| `NtQuerySystemInformation` | Enumerating processes / drivers / handles | T1057 / T1082 |
| `NtAdjustPrivilegesToken` | Privilege escalation acquiring SeDebugPrivilege etc. | T1134 |
| `NtLoadDriver` | Loading kernel drivers (BYOVD) | T1543.003 |

### Verifying Whether a Hook Exists

```powershell
# Simple: disassembly-diff the on-disk ntdll against the current process's ntdll
# 1. Grab the on-disk ntdll
copy C:\Windows\System32\ntdll.dll C:\temp\ntdll_clean.dll

# 2. In windbg, attach to any process and export the current ntdll's .text section
# .writemem c:\temp\ntdll_live.bin ntdll!.text L?<size>

# 3. Disassemble NtAllocateVirtualMemory in IDA / radare2; it should normally be:
#    mov r10, rcx
#    mov eax, <SSN>
#    test byte ptr [...]
#    jne ...
#    syscall
#    ret
# If the first instruction becomes jmp <some address>, that's a hook
```

## 3. Kernel Callback Monitoring Points

Common kernel callbacks registered by EDRs (all can be unregistered via the BYOVD route in `attack-chain`, but at a high cost):

| API | Callback Timing | Defensive Use |
|-----|--------------|-----------|
| `PsSetCreateProcessNotifyRoutineEx` | Process create / exit | Block suspicious child processes |
| `PsSetCreateThreadNotifyRoutine` | Thread create / exit | Detect remote thread injection |
| `PsSetLoadImageNotifyRoutine` | DLL / EXE loaded into any process | Module integrity / unsigned blocking |
| `CmRegisterCallback` / `CmRegisterCallbackEx` | Registry operations | Persistence detection |
| `ObRegisterCallbacks` | `OpenProcess` / `OpenThread` handle requests | Prevent LSASS handle acquisition (T1003.001) |
| `MmRegisterPhysicalMemoryCallback` | Physical memory mapping | Anti DMA / memory forensics |
| `IoRegisterFsRegistrationChange` | Filesystem registration | Cooperates with minifilters |
| `KeRegisterNmiCallback` | NMI (rarely used by EDRs) | Anomaly monitoring |
| `EtwRegister` (kernel side) | Kernel ETW reporting | Symbiotic with ETW-TI |

### Enumerating Registered Callbacks with windbg

```text
0: kd> dx -r1 nt!PspCreateProcessNotifyRoutine
0: kd> dx -r1 nt!PspCreateThreadNotifyRoutine
0: kd> dx -r1 nt!PspLoadImageNotifyRoutine

0: kd> !object \Callback
0: kd> !object \Callback\ProcessObject
```

Or use tools like PChunter / DRVHV so ordinary users can view callback lists visually.

## 4. Statically Dumping the Hook Table (IDA + windbg Workflow)

### Workflow A: Single-Process Comparison

```text
1. Find a process already injected with the EDR's user-mode component (any live process)
2. windbg attach (-pn target.exe)
3. lm m ntdll  → get the module base
4. .writemem c:\temp\ntdll_live.bin ntdll+0x0 L?<image size>
5. Copy C:\Windows\System32\ntdll.dll to c:\temp\ntdll_disk.dll
6. Load both files in IDA, jump to NtAllocateVirtualMemory:
     - disk: standard prologue
     - live: first instruction jmp <0x7FFE000000xx>
7. Follow the jmp target address → that's the EDR trampoline, dump it
8. Go inside the trampoline to see which DLL it finally lands in, confirming the EDR module name
```

### Workflow B: Bulk Hook Table Generation

Use `HookHunter` or a homegrown script:

```powershell
# pseudo workflow, see the scripts mentioned in references
$disk = Get-Content C:\Windows\System32\ntdll.dll -Encoding Byte
$live = # obtained via OpenProcess + ReadProcessMemory
# compare the first 16 bytes of each export in the .text section
```

## 5. Automatic Detection with pe-sieve

`pe-sieve` is the first choice for recon of EDR hooks and implant self-checks:

```powershell
# Basic scan
pe-sieve64.exe /pid 1234

# Recommended combo (with shellcode and hook detection)
pe-sieve64.exe /pid 1234 /shellc 3 /modules 3 /imp 3 /data 3 /dir hooks_dump

# Key parameters:
#   /shellc N    shellcode scan level (0-3)
#   /modules N   module integrity check (0-3)
#   /imp N       IAT hook check
#   /data N      data section scan
#   /dir <path>  dump output directory
```

The output produces `*.tag` files under `hooks_dump/<pid>.<name>/` listing hook addresses:

```text
modified_modules.tag example:
71f10000;ntdll.dll
71f1a3b0;hook;jmp_far
71f1c020;hook;jmp_near
```

Feed these directly into IDA, jumping to the corresponding RVA for follow-up analysis.

### Embedding pe-sieve in an Implant (self-check)

In practice, `pe-sieve` is often compiled as a lib (`libpe-sieve`) so the implant self-checks at startup: if ntdll is hooked, trigger the unhook flow; if it finds itself hooked, be careful — it may be inside a sandbox.

## 6. Dynamic Observation with API Monitor v2

API Monitor v2 (Rohitab) is well suited for seeing when and where the EDR inserts hooks in the lab:

```text
1. Launch API Monitor v2 (as administrator)
2. In API Filter check:
     - NT Native API → Memory Management
     - NT Native API → Process and Thread
     - Windows Defender / AMSI (if visible)
3. Monitor New Process → select the implant test sample
4. Observe:
     - NtAllocateVirtualMemory call order
     - Whether it is relayed by an EDR DLL
5. In the Modules tab, see which EDR DLLs got LoadLibrary-injected
```

## 7. Common EDR DLLs (User-Mode) Quick Reference

| DLL | Vendor | Notes |
|-----|------|------|
| `umppc*.dll` | Microsoft Defender | MpClient userland |
| `mpoav.dll` | Microsoft Defender | AMSI provider |
| `aswAMSI.dll` | Avast | AMSI provider |
| `eamsi.dll` | ESET | AMSI provider |
| `IDPMServiceClient.dll` | Sophos | HMPA injection |
| `klsihk64.dll` | Kaspersky | Injected into target processes |
| `CrowdStrike.Sensor.dll` | CrowdStrike | Old versions; newer ones rely mainly on the kernel |
| `SentinelInjection64.dll` | SentinelOne | User-mode injection |
| `TmUmEvt64.dll` | Trend Micro | Behavior monitoring |

After confirming the target EDR, decide which DLL to reverse for the hook table.

## Reference Links

- pe-sieve: <https://github.com/hasherezade/pe-sieve>
- HollowsHunter: <https://github.com/hasherezade/hollows_hunter>
- API Monitor v2: <http://www.rohitab.com/apimonitor>
- MITRE ATT&CK T1562: <https://attack.mitre.org/techniques/T1562/>
- MITRE ATT&CK T1055: <https://attack.mitre.org/techniques/T1055/>
- ired.team EDR notes: <https://www.ired.team/offensive-security/defense-evasion>

## Routing Callback

After finishing the hook survey, return to Step 3 of `SKILL.md` to choose the bypass technique combination, then execute per `references/unhook-techniques.md` and `references/telemetry-blinding.md`.
