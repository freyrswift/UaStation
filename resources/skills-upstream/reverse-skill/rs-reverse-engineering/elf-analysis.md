# ELF Binary Deep Analysis Reference

> Structure parsing, anti-analysis technique identification, and analysis tips when reverse engineering Linux/Android ELF files.

---

## ELF Structure Quick Reference

### File Header (ELF Header)

```text
Offset  Size  Field             Description
0x00  4    e_ident[EI_MAG]   Magic: 7f 45 4c 46 ("\x7fELF")
0x04  1    e_ident[EI_CLASS] 1=32bit, 2=64bit
0x05  1    e_ident[EI_DATA]  1=LE, 2=BE
0x10  2    e_type            2=EXEC, 3=DYN(PIE/SO), 4=CORE
0x12  2    e_machine         0x03=x86, 0x3E=x86_64, 0xB7=AArch64, 0x28=ARM
0x18  8    e_entry           Entry point virtual address
0x20  8    e_phoff           Program header table offset
0x28  8    e_shoff           Section header table offset (may be 0 after strip)
0x38  2    e_phnum           Number of program headers
0x3C  2    e_shnum           Number of section headers
```

### Program Header

```text
Type value  Name       Description
0x01   PT_LOAD    Loadable segment (code/data)
0x02   PT_DYNAMIC Dynamic linking information
0x03   PT_INTERP  Interpreter path (/lib/ld-linux.so)
0x04   PT_NOTE    Auxiliary information
0x06   PT_PHDR    The program header table itself
0x6474e550 PT_GNU_EH_FRAME  Exception handling
0x6474e551 PT_GNU_STACK     Stack executable flag
0x6474e552 PT_GNU_RELRO     Read-only relocations
```

### Common Sections

| Section | Description |
|------|------|
| `.text` | Code segment |
| `.rodata` | Read-only data (string constants) |
| `.data` | Initialized global variables |
| `.bss` | Uninitialized global variables |
| `.plt` / `.got` | Dynamic linking trampolines/jump tables |
| `.init_array` | Array of constructor pointers |
| `.fini_array` | Array of destructor pointers |
| `.dynamic` | Dynamic linking information |
| `.symtab` / `.dynsym` | Symbol tables |
| `.strtab` / `.dynstr` | String tables |

---

## Identifying Anti-Analysis Techniques

### Common ELF Anti-Analysis Techniques

| Technique | Signature | Countermeasure |
|------|------|---------|
| Corrupted program headers | PHDR filled with garbage data (e.g., 0x0a) | Repair manually or ignore the corrupted PHDR |
| No section headers | `e_shoff = 0`, `e_shnum = 0` | Rely only on program headers for analysis, not sections |
| Stripped symbols (strip) | No `.symtab`, all function names lost | GoReSym(Go) / signature matching / FLIRT |
| Static linking | No `.dynamic`, huge file size | Use FLIRT/Lumina to identify library functions |
| Disguised file type | Extension .sh/.txt/.jpg | Use the `file` command / magic bytes to determine |
| UPX packing | Contains the `UPX!` marker | Unpack with `upx -d` |
| Custom packer | Entry point jumps to decompression code | Run dynamically to OEP, then dump |
| Anti-debugging | ptrace(TRACEME) | LD_PRELOAD hook / patch |
| Anti-VM | Checks /proc/cpuinfo | Modify cpuinfo or hook the read |
| Encrypted code | Decrypts .text at runtime | Breakpoint after decryption, then dump |

### Identifying Self-Decompressing/Self-Modifying Code

```text
Signatures:
1. mmap(PROT_READ|PROT_WRITE|PROT_EXEC) call near the entry point
2. Immediately followed by memcpy or a copy loop
3. Then mprotect changes permissions
4. Finally br/jmp to the newly mapped address

Analysis strategy:
1. Find the mmap call → record the returned address
2. Set a breakpoint after mprotect(PROT_EXEC)
3. Dump the decompressed memory region
4. Analyze it as a new binary
```

---

## ARM64 (AArch64) Reverse Engineering Quick Reference

### Registers

| Register | Purpose |
|--------|------|
| x0-x7 | Arguments/return value |
| x8 | Indirect result (syscall number) |
| x9-x15 | Temporary registers |
| x16-x17 | IP0/IP1 (PLT trampolines) |
| x18 | Platform register (Android: shadow call stack) |
| x19-x28 | Callee-saved |
| x29 (FP) | Frame pointer |
| x30 (LR) | Link register (return address) |
| SP | Stack pointer |
| PC | Program counter |

### Common Instruction Patterns

```text
Function prologue:
  stp x29, x30, [sp, #-N]!    # save FP and LR
  mov x29, sp                  # set frame pointer

Function epilogue:
  ldp x29, x30, [sp], #N      # restore FP and LR
  ret                          # return (br x30)

System call:
  mov x8, #NR                  # syscall number
  svc #0                       # trigger syscall

Conditional branches:
  cmp x0, #0
  b.eq label                   # branch if equal
  b.ne label                   # branch if not equal
  cbz x0, label                # branch if x0 == 0
  cbnz x0, label               # branch if x0 != 0

Address loading:
  adrp x0, page                # load the high bits of the page address
  add x0, x0, #offset          # add the low 12-bit offset
  ldr x0, [x1, #offset]        # load from memory
```

### Linux ARM64 System Call Numbers

| Number | Name | Description |
|------|------|------|
| 56 | openat | Open a file |
| 63 | read | Read |
| 64 | write | Write |
| 57 | close | Close |
| 222 | mmap | Memory mapping |
| 226 | mprotect | Change memory permissions |
| 117 | ptrace | Process tracing |
| 220 | clone | Create process/thread |
| 221 | execve | Execute a program |
| 93 | exit | Exit |
| 94 | exit_group | Exit the process group |

---

## Identifying Common Compression/Packing Algorithms

| Algorithm | Identification Signature | Decompression Method |
|------|---------|---------|
| **LZSS** | Bitstream + literal/match flags | Custom decompressor (e.g., this report) |
| **ZLIB/Deflate** | Magic: `78 01`/`78 9C`/`78 DA` | `zlib.decompress()` |
| **GZIP** | Magic: `1F 8B` | `gzip -d` / `gunzip` |
| **LZ4** | Magic: `04 22 4D 18` | `lz4 -d` |
| **LZMA/XZ** | Magic: `FD 37 7A 58 5A 00` (XZ) | `xz -d` / `lzma -d` |
| **Brotli** | No fixed magic, depends on context | `brotli -d` |
| **Zstandard** | Magic: `28 B5 2F FD` | `zstd -d` |
| **UPX** | String `UPX!` | `upx -d` |
| **Custom** | Decompression loop at the entry point | Reverse the algorithm, then write a decompressor |

### Clues for Identifying Custom Compression

```text
1. Loop + bit operations near the entry point (shifts, AND, OR)
2. "Sliding window" back-references (reading backwards from the output buffer) → LZ family
3. Frequency table / Huffman tree construction → Deflate/Huffman
4. Fixed-size block processing → block compression (LZ4/Snappy)
5. Arithmetic coding signatures (interval shrinking) → LZMA/ANS
```

---

## Linux Process Injection Techniques

### mmap + Code Injection

```text
Flow:
1. mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0)
2. Write shellcode/payload into the mapped region
3. mprotect(addr, size, PROT_READ|PROT_EXEC)  # make executable
4. Jump to the mapped address and execute

Signatures:
- The mmap return value is saved
- Immediately followed by memcpy or a write loop
- Then mprotect changes permissions
- Finally br/blr to that address
```

### ptrace Injection

```text
Flow:
1. ptrace(PTRACE_ATTACH, target_pid)
2. waitpid(target_pid)
3. ptrace(PTRACE_GETREGS, target_pid, &regs)
4. Modify regs.pc to point at the injected code
5. ptrace(PTRACE_SETREGS, target_pid, &regs)
6. ptrace(PTRACE_CONT, target_pid)

Signatures:
- Opening /proc/<pid>/mem or using ptrace
- Reading/modifying the target process's registers
- Writing shellcode into the target process's address space
```

### /proc/self/mem Self-Modification

```text
Flow:
1. open("/proc/self/mem", O_RDWR)
2. lseek(fd, target_addr, SEEK_SET)
3. write(fd, new_code, size)

Uses:
- Bypass W^X protection (mmap'd pages cannot be both W+X)
- Modify its own code segment (.text is normally read-only)
- Patch instructions at runtime
```

---

## Strategy for Analyzing Large ELFs

For large binaries of 5MB+:

```text
1. Quick reconnaissance (5 minutes)
   - file / rabin2 -I → architecture, type, protections
   - strings | grep -i "error\|fail\|http\|/proc\|/dev" → key strings
   - rabin2 -i → imported functions (if any)
   - rabin2 -E → exported functions

2. Structural analysis (10 minutes)
   - readelf -l → program headers (LOAD segment layout)
   - Code near the entry point → any decompression/decryption
   - Find .init_array → constructors (may contain anti-debugging)

3. Locate the key logic
   - Start from string cross-references
   - Start from system calls (mmap/ptrace/open)
   - Start from network functions (connect/send/recv)

4. Divide and conquer
   - If self-decompressing → decompress first, analyze the payload
   - If multi-module → analyze in blocks by functionality
   - Use binary-diff to compare different versions
```

---

## Tool Command Quick Reference

```bash
# Basic information
file binary
readelf -h binary          # ELF header
readelf -l binary          # program headers
readelf -S binary          # section headers (if any)
rabin2 -I binary           # general information

# Strings
strings -a binary | less
rabin2 -z binary           # strings in the data segment
rabin2 -zz binary          # strings across the whole file

# Disassembly
r2 -A binary               # radare2 analysis
objdump -d binary          # GNU disassembly
aarch64-linux-gnu-objdump -d binary  # ARM64 cross-disassembly

# Dynamic analysis
strace -f ./binary         # system call tracing
ltrace -f ./binary         # library call tracing
qemu-aarch64 -strace ./binary  # ARM64 emulated execution

# Memory dump
gdb -p <pid> -ex "dump memory out.bin 0xADDR 0xADDR+SIZE" -ex quit

# Repair a corrupted ELF
# Manually modify e_phnum or patch the corrupted PHDR
python -c "
import struct
with open('binary', 'r+b') as f:
    f.seek(0x38)  # e_phnum offset (64-bit)
    f.write(struct.pack('<H', 2))  # set the correct PHDR count
"
```
