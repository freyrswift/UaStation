# Kernel Driver Reverse Engineering Reference

> Covers Windows/Linux kernel driver reverse engineering, rootkit analysis, and C/C++ binary pattern recognition.

---

## Windows Driver Reverse Engineering

### Driver Types

| Type | Characteristics | Analysis Focus |
|------|------|---------|
| WDM (Windows Driver Model) | Legacy drivers, manual IRP management | DriverEntry → device creation → Dispatch routines |
| KMDF (Kernel Mode Driver Framework) | Modern framework, event-driven | EvtDriverDeviceAdd → Queue → I/O callbacks |
| WDF (Windows Driver Foundation) | Umbrella term for KMDF + UMDF | Look for WdfDriverCreate calls |
| Minifilter | File system filter driver | FltRegisterFilter → Pre/Post callbacks |

### WDM Driver Analysis Workflow

```text
1. Find DriverEntry (entry point)
   - IDA identifies it automatically, or search for IoCreateDevice / IoCreateSymbolicLink

2. Find device name and symbolic link
   - IoCreateDevice → DeviceName (e.g., \Device\MyDriver)
   - IoCreateSymbolicLink → SymLink (e.g., \DosDevices\MyDriver)

3. Find Dispatch routines
   - DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctl
   - This is the entry point called from user mode via DeviceIoControl

4. Analyze IOCTL handling
   - switch(IoControlCode) dispatches different functions
   - IOCTL encoding: CTL_CODE(DeviceType, Function, Method, Access)
   - Method: METHOD_BUFFERED / METHOD_IN_DIRECT / METHOD_OUT_DIRECT / METHOD_NEITHER

5. Find vulnerabilities
   - User-controlled buffer with no length validation → overflow
   - METHOD_NEITHER uses user pointers directly → arbitrary read/write
   - No IOCTL permission check → callable by unprivileged users
```

### IOCTL Encoding Decoded

```python
# Decode IOCTL code
def decode_ioctl(code):
    device_type = (code >> 16) & 0xFFFF
    access = (code >> 14) & 0x3
    function = (code >> 2) & 0xFFF
    method = code & 0x3
    
    methods = {0: "BUFFERED", 1: "IN_DIRECT", 2: "OUT_DIRECT", 3: "NEITHER"}
    access_types = {0: "ANY", 1: "READ", 2: "WRITE", 3: "READ|WRITE"}
    
    return f"DevType=0x{device_type:X} Func=0x{function:X} Method={methods[method]} Access={access_types[access]}"

# Example
decode_ioctl(0x80002034)
# DevType=0x8000 Func=0x80D Method=BUFFERED Access=ANY
```

### IDA Plugins

| Plugin | Purpose | Link |
|------|------|------|
| **Driver Buddy Reloaded** | Automatically identifies IOCTLs, Dispatch routines, device names | https://github.com/VoidSec/DriverBuddyReloaded |
| **WinDbg + IDA** | Kernel debugging combined with static analysis | Built in |
| **FLIRT/Lumina** | Identifies WDK library functions | Built into IDA |

### Reference Articles

- [Windows Drivers RE Methodology (VoidSec)](https://voidsec.com/windows-drivers-reverse-engineering-methodology/) — the most complete WDM driver reverse engineering methodology
- [Driver Reversing 101](https://eversinc33.com/posts/driver-reversing.html) — WDM vs KMDF comparison
- [Methodology of Reversing Vulnerable Killer Drivers](https://whiteknightlabs.com/2025/10/28/methodology-of-reversing-vulnerable-killer-drivers/) — vulnerable driver analysis

---

## Linux Kernel Module Reverse Engineering

### LKM (Loadable Kernel Module) Structure

```text
Key functions:
- init_module / module_init → executed when the module loads
- cleanup_module / module_exit → executed when the module unloads

Key structures:
- struct file_operations → open/read/write/ioctl for character devices
- struct net_device_ops → network device operations
- struct block_device_operations → block device operations
```

### Analysis Workflow

```text
1. Confirm it is a kernel module
   file module.ko → "ELF 64-bit ... relocatable" (note: relocatable, not executable)

2. Find init/exit functions
   readelf -s module.ko | grep -E "init_module|cleanup_module"
   or look in the .modinfo section for module information

3. Find the file_operations structure
   Search for register_chrdev / cdev_add / misc_register
   → locate the fops structure → identify the ioctl/read/write handler functions

4. Analyze ioctl handling
   unlocked_ioctl / compat_ioctl functions
   → dispatch via switch(cmd)

5. Look for rootkit behavior
   - Modifying sys_call_table → syscall hook
   - Modifying the /proc filesystem → hiding processes/files
   - Registering netfilter hooks → hiding network connections
   - Modifying the VFS layer → hiding files
```

### Common Rootkit Techniques

| Technique | Signature | Detection Method |
|------|------|---------|
| syscall table hook | Modified `sys_call_table` entries | Compare the in-memory table against vmlinux on disk |
| VFS hook | Modified `file_operations` function pointers | Check whether fops pointers point outside the kernel code section |
| Netfilter hook | `nf_register_net_hook` | Walk the netfilter hook linked list |
| kprobe/ftrace hook | Registered kprobe or ftrace callbacks | Check the ftrace registration list |
| eBPF rootkit | Loads malicious BPF programs | `bpftool prog list` |
| DKOM | Direct modification of kernel objects (process list) | Walk the task_struct list and compare against /proc |

### Tools

| Tool | Purpose |
|------|------|
| `crash` | Kernel dump analysis |
| `volatility3` | Memory forensics (Linux profile) |
| `dmesg` / `journalctl` | Kernel logs |
| `lsmod` / `/proc/modules` | List of loaded modules |
| `modinfo` | Module metadata |
| `strace` | System call tracing (user-mode view) |

---

## C/C++ Reverse Engineering Pattern Recognition

### Common C Language Patterns

| Source Pattern | Disassembly Signature |
|---------|-----------|
| `if-else` | `cmp` + `jcc` (conditional jump) |
| `switch-case` | Jump table (`jmp [rax*8 + table]`) or consecutive `cmp`s |
| `for` loop | `cmp` + `jl/jle` + loop body + `inc/add` + `jmp` back |
| `while` loop | Condition check at the top of the loop |
| `do-while` | Condition check at the bottom of the loop |
| Function pointer call | `call rax` or `call [reg+offset]` |
| `struct` access | `[reg+fixed offset]` (e.g., `[rdi+0x10]`) |
| `malloc` + use | `call malloc` → return value stored in a register → subsequent access via that register + offset |
| String comparison | `call strcmp` or `repe cmpsb` |

### C++-Specific Patterns

| Source Pattern | Disassembly Signature |
|---------|-----------|
| **Virtual function call** | `mov rax, [rcx]` (load vtable) → `call [rax+offset]` (call virtual function) |
| **Constructor** | Allocate memory → write vtable pointer → initialize members |
| **Destructor** | Clean up members → may call `operator delete` |
| **this pointer** | The first argument (rcx/rdi) is the object pointer |
| **Inheritance** | vtable contains parent-class virtual functions + child-class overrides |
| **Multiple inheritance** | Multiple vtable pointers inside the object (at different offsets) |
| **RTTI** | `type_info` pointer preceding the vtable |
| **Exception handling** | `__cxa_throw` / `_CxxThrowException` |
| **STL containers** | `std::vector`: `{begin, end, capacity}` three-pointer structure |
| **std::string** | Small string optimization (SSO): short strings inline, long strings heap-allocated |

### vtable Reverse Engineering Method

```text
1. Find the vtable
   - Search for consecutive arrays of function pointers (in the .rodata or .rdata section)
   - The constructor writes the vtable pointer via `mov [rcx], offset vtable`

2. Determine the class hierarchy
   - At offset -8 before the vtable there is usually an RTTI pointer (if not stripped)
   - Multiple vtables sharing the first few entries → inheritance relationship

3. Annotate virtual functions
   - vtable[0] is usually the destructor (or deleting destructor)
   - Annotate the rest by offset: vtable[1] = func1, vtable[2] = func2...

4. Working in IDA
   - Create a struct at the vtable address (each field is a function pointer)
   - Add comments on `call [rax+offset]` indicating which virtual function is called
```

### Structure Recovery

```text
Method 1: infer from access patterns
  mov eax, [rdi+0x00]  → field_0: int/ptr (4/8 bytes)
  mov ecx, [rdi+0x08]  → field_8: int/ptr
  movss xmm0, [rdi+0x10] → field_10: float

Method 2: infer from sizeof
  call malloc(0x30) → structure size is 0x30 (48 bytes)
  
Method 3: infer from the constructor
  The constructor initializes all fields → field types and offsets become obvious

Method 4: use IDA's "Create struct" feature
  Select access patterns → Edit → Struct → Create struct from selection
```

---

## Common Compiler Fingerprints

| Compiler | Identification Fingerprints |
|--------|---------|
| MSVC | `_security_cookie` checks, `__fastcall` calling convention, Rich Header |
| GCC | `__stack_chk_fail`, `-fstack-protector`, `.note.GNU-stack` |
| Clang/LLVM | Similar to GCC but different optimization patterns, `__asan_*` (if sanitizers are enabled) |
| MinGW | GCC fingerprints + Windows API calls |
| AOSP Clang | Android-specific `__android_log_print`, PGO markers |

### Optimization Level Identification

| Optimization Level | Characteristics |
|---------|------|
| -O0 | Lots of redundant movs, every variable on the stack, no function inlining |
| -O1 | Basic optimization, some variables in registers |
| -O2 | Loop unrolling, function inlining, tail-call optimization |
| -O3 / -Os | Aggressive inlining, vectorization (SIMD), hard-to-read code |
| PGO | Hot-path optimization, cold code split into `.text.cold` |
| LTO | Cross-module inlining, global dead-code elimination |

---

## Kernel Debugging Environments

### Windows

```text
Debugger: WinDbg Preview
Connection: network debugging (recommended) or serial port

Debugee setup:
bcdedit /debug on
bcdedit /dbgsettings net hostip:192.168.x.x port:50000

Debugger connection:
WinDbg → File → Attach to Kernel → Net → Port:50000 Key:xxx

Common commands:
!analyze -v          # automatic crash analysis
lm                   # list loaded modules
!drvobj \Driver\xxx  # inspect a driver object
dt nt!_DRIVER_OBJECT # display the structure
bp module!function   # set a breakpoint
```

### Linux

```text
Debugger: GDB + QEMU or kgdb

QEMU kernel debugging:
qemu-system-x86_64 -kernel bzImage -s -S ...
gdb vmlinux -ex "target remote :1234"

Common commands:
info threads         # kernel threads
lx-symbols           # load kernel symbols (requires scripts/gdb/)
p init_task          # inspect the init process
lx-dmesg             # kernel logs
```

---

## Agent Action Anchors (Issue #65 U–AV)

Aligned with `references/nonpe-format-cookbook.md` §5 (short table, does not replace the workflows above):

| ID | Action | Evidence |
|----|------|----------|
| AG | `DriverEntry` is short → scan `MajorFunction` non-empty slots, prioritize DEVICE_CONTROL/CREATE | `E-driver-irp-handlers` |
| AH | Build an IOCTL control-code → handler table with METHOD_* | `E-driver-ioctl` |
| AI | Suspected BYOVD: cross-check against public vulnerable-driver lists; record name/hash/signature and call intent; **do not write exploit steps** | `E-driver-byovd` |

## Reference Resources

| Resource | Description | Link |
|------|------|------|
| VoidSec driver reverse engineering methodology | Complete Windows WDM driver analysis workflow | https://voidsec.com/windows-drivers-reverse-engineering-methodology/ |
| Elastic Rootkit series | Linux rootkit taxonomy + detection | https://security-labs.elastic.co/security-labs/linux-rootkits-1-hooked-on-linux |
| Driver Buddy Reloaded | IDA driver analysis plugin | https://github.com/VoidSec/DriverBuddyReloaded |
| LOLDrivers | List of known vulnerable drivers | https://www.loldrivers.io/ |
| Windows Driver Samples | Official Microsoft driver samples | https://github.com/microsoft/Windows-driver-samples |
| Linux Kernel Module Programming | Kernel module development tutorial | https://sysprog21.github.io/lkmpg/ |
| Trail of Bits - Devirtualizing C++ | vtable reverse engineering method | https://blog.trailofbits.com/2017/02/13/devirtualizing-c-with-binary-ninja/ |
