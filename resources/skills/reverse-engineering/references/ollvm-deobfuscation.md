# OLLVM Deobfuscation / Obfuscator-LLVM Deobfuscation

> OLLVM deobfuscation workflow for APK .so files, ELF binaries, and control-flow flattening scenarios.
> Tool and variant information is based on a survey of community-active projects in 2026, not training memory.
> Applicable to: Android NDK hardening, CTF reverse engineering, packed .so analysis, and countering commercial obfuscators.

---

## 0. Quick Decision: Which Tool Should I Use?

Based on your environment and your assessment of the target's obfuscation type, find your match directly:

| Your situation | Preferred tool | Alternative | Notes |
|---------|---------|------|------|
| Have IDA Pro 7.5-7.7 + Hex-Rays, want one-click de-flattening | **obpo-plugin** | d810-ng | obpo uses microcode + data flow + hybrid execution; strongest results, but it is a cloud plugin (requires network, core is closed-source) |
| Have IDA Pro (any recent version), want a local all-in-one deobfuscator | **d810-ng** | original D-810 | Local, open source, integrates Z3, supports OLLVM/Tigress/Hodur/Approov variants |
| Have Binary Ninja | **ollvm-breaker** | — | Battle-tested on Android .so (libvdog and similar hardened samples) |
| No IDA/BN, pure scripting, target is x86/x64 | **ollvm-unflattener** (Miasm) | angr deflat | Based on Miasm symbolic execution, BFS multi-layer processing |
| No IDA/BN, pure scripting, target is x86/x64 | **ollvm-unflattener** (Miasm) | angr deflat | Based on Miasm symbolic execution, BFS multi-layer processing |
| Pure Python symbolic execution, CTF scenario | **angr** Deobfuscator | Triton | No GUI dependency, scriptable |
| Target is an ARM64 .so, no IDA | **deollvm** (Unicorn) | angr | Unicorn-based ARM64 deflat |
| Hit BR obfuscation (indirect branches) | **DeObfBR** | set data section read-only | Goron/Arkari-style BR obfuscation can be simply countered by making the data section read-only |
| Hit Tigress obfuscation | d810-ng `UnflattenerSwitchCase`/`UnflattenerTigressIndirect` | — | d810-ng has built-in Tigress-specific unflatteners |

> **Core recommendation:** prefer **d810-ng** (local, actively maintained, broad variant coverage). When cloud service is available, **obpo-plugin** gives the best results. If both fail, move on to **angr/Miasm** symbolic execution for customized handling.

---

## 1. The Modern OLLVM Variant Ecosystem (2026 Community Survey)

OLLVM is long past being just that 2017 original repository. Below are the currently active obfuscator branches. **Before deobfuscating you must first determine which variant the target is**, because countermeasures differ greatly between variants:

### 1.1 Obfuscator Branch Lineage

| Variant | Base LLVM | New features vs. original OLLVM | Countermeasure key points |
|------|----------|----------------------|---------|
| **Obfuscator** (original) | 3.3~4.0 | sub + bcf + fla (the three foundational passes) | Standard tools handle all |
| **Hikari** | 6~8 | Anti Class Dump, Function Call Obfuscate, Function Wrapper, Indirect Branching, Split BB, String Encryption | Need to decrypt strings first + fix indirect jumps |
| **Hikari-LLVM15** | 15~19 | + Anti Debugging, Anti Hook, Constant Encryption | Now closed-source; Constant Encryption raises static analysis difficulty |
| **goron** | 7~10 | Indirect Branch/Call/GlobalVariable | ⚠️ Goron-style indirect obfuscation can be simply countered by "setting the data section read-only" |
| **Arkari** (komimoe/Hikari) | 14~latest | Based on goron, continuously maintained | Same as goron; making the data section read-only partially counters it |
| **Pluto** | 14 | MBA Obfuscation, Random CF, Split BB, **Trap Angr** (specifically to trip angr) | ⚠️ The Trap Angr pass defeats angr symbolic execution; switch tools or bypass the traps |
| **Polaris** (formerly Pluto) | 16 | Alias Access, Indirect Branch/Call, String Encryption, Merge Function, Linear MBA, Dirty Bytes Insertion, Function Splitting, Junk Insertion | Combines Hikari+Pluto; the trickiest; requires layered processing |
| **O-MVLL** | open-obfuscator | Python-driven pass manager; Anti Hooking, Arithmetic(MBA), BB Duplicate, CF Breaking, Function Outline, Indirect Branch/Call, Opaque Constants | Common in modern Android hardening; Python configuration is easy to customize |
| **amice** (Rust) | Rust implementation | Full set + VM Flatten, Instruction Virtualization, Delayed Offset Loading, Parameter Aggregation | Contains VM-ization; requires VM handler recovery, not just deflat |
| **VMP family** (SmallVmp/VMPilot/xVMP/VMPacker) | — | Instruction virtualization | **Not in OLLVM scope**; requires VM reversing; see VM-specific tools |

### 1.2 Key Identification Clues

- **Trap Angr** (Pluto/Polaris): if angr explodes or path-explodes mid-run, suspect the target uses the Trap Angr pass → switch to d810-ng or a Unicorn dynamic approach
- **Goron/Arkari indirect jumps**: if the dispatcher uses indirect jumps (BR x8 instead of switch), first try setting the relevant data section read-only; indirect jump targets often become statically solvable
- **Constant Encryption** (Hikari-LLVM15/Polaris/O-MVLL): constants are decrypted at runtime, pure static analysis cannot see the real values → need Unicorn to dynamically execute the decryption stub
- **VM Flatten** (amice): control flow becomes a VM dispatch loop, **do not treat it as ordinary fla**; you must first identify the VM handler table

---

## 2. OLLVM Obfuscation Type Detection

Identification signatures of the three core OLLVM passes:

### 2.1 Control Flow Flattening (`fla`)

**IDA view characteristics:**
- The function entry first jumps to a unique dispatcher block
- Main logic is split into multiple basic blocks, each jumping back to the dispatcher at the end
- The dispatcher decides which block executes next via a **state variable**
- A huge `switch` structure with no logical relationship between the cases

```
Original:             OLLVM flattened:
  block_A               entry -> dispatcher
  block_B                 ↓
  block_C              state_machine:
                         switch(state):
                           0 → block_A
                           1 → block_B
                           2 → block_C
```

**Variant forms (several dispatchers recognized by d810-ng):**
- O-LLVM: switch / if-chain + state variable
- Tigress: `m_jtbl` (switch-case) or `m_ijmp` (indirect jump; requires `goto_table_info` configuration)
- Hodur (PlugX): nested `while(1)` state machine, `jnz state, #CONST`, **no switch dispatcher**
- Approov: `while(v8 != C)`, state constants concentrated in `0xF6000–0xF6FFF`

### 2.2 Bogus Control Flow (`bcf`)

- **Unreachable fake branches** inserted between every real branch
- Fake branches protected by **opaque predicates** (conditions always true/false, but static analysis cannot directly prove it)
- Lots of dead code bloats function size

```c
// Classic opaque predicate: x(x+1) is always even, but the compiler cannot prove it
if (x * (x + 1) % 2 == 0) {
    // real logic
} else {
    // unreachable junk code
}
```

### 2.3 Instruction Substitution (`sub`) → MBA

- Simple arithmetic/bitwise operations replaced with equivalent complex expressions (MBA, Mixed Boolean-Arithmetic)

```
a + b  →  (a ^ b) + 2*(a & b)
a ^ b  →  (a | b) - (a & b)
a - b  →  a + (~b) + 1
```

### 2.4 Quick Classification Table

| Obfuscation type | IDA signature | Primary countermeasures |
|---------|---------|------------|
| fla (flattening) | Huge switch + dispatcher | obpo / d810-ng / deflat |
| bcf (bogus control flow) | Unreachable branches + dead code | d810-ng opaque predicate removal / symbolic execution |
| sub/MBA | Complex arithmetic expressions | d810-ng MBA simplifier / SiMBA (Z3) |
| fla + bcf + sub | Everything applied, massive bloat | **Layered deobfuscation (first bcf, then fla, then sub)** |

---

## 3. Mainstream Tools in Detail (Community-Active Projects)

### 3.1 obpo-plugin — Strongest Results, Cloud Plugin

> [obpo-project/obpo-plugin](https://github.com/obpo-project/obpo-plugin) · 629⭐ · active 2026-06

A pseudocode optimizer based on Hex-Rays **microcode**, using **data-flow tracking + program slicing + hybrid execution (concolic)** to rebuild flattened control flow. Widely recognized as one of the strongest in the community.

**Key features:**
- Operates at the microcode level, directly optimizing decompiler output (does not modify ASM)
- Supports IDA 7.5.0 / 7.6.0 / 7.7.0 + Hex-Rays
- Architectures: ARM, ARM64, x86, x86_64, PowerPC, PowerPC64, MIPS (7.6/7.5)
- **Cloud plugin**: the target function's binary is uploaded to obpo-server for processing (core is closed-source, plugin is free and open source)
- Server is maintained at the author's own expense, 600s timeout, **multi-threading/malicious calls are prohibited**

**Installation and usage:**
```text
1. Download obpo_plugin.py and the obpoplugin directory
2. Copy them to the IDA plugins path
3. Restart IDA, open the target binary
4. Locate the dispatch block (dispatcher) in the CFG; it usually looks like this:
   [see the repo's assets/dispatchblock.png for a screenshot]
5. Right-click → OBPO → Mark and process function
6. After processing completes, refresh the decompiler
7. Based on the decompilation changes, continue marking new dispatch blocks (iteratively process nested fla)
```

**Applicable scenarios and limitations:**
- ✅ Standard and nested fla, good results
- ⚠️ Requires network; use with caution on sensitive samples (unpublished internal vulnerabilities, trade secrets) — the binary gets uploaded
- ⚠️ The server may be down; depends on author maintenance
- ❌ Cannot solve all obfuscation (explicitly stated by the author)

### 3.2 d810-ng — Local All-in-One First Choice

> [w00tzenheimer/d810-ng](https://github.com/w00tzenheimer/d810-ng) · 223⭐ · updated 2026-06-26

A modern maintained/refactored version (Next Generation) of D-810. Runs locally, open source, integrates the **Z3 SMT** solver, and covers the most variants.

**Core capabilities (organized from the d810-ng README):**

*Instruction-level optimization:*
| Category | Description |
|------|------|
| MBA simplification | `(a+b)-2*(a&b) => a^b`, Z3-verified DSL rules |
| Hacker's Delight | Bitwise equivalences (from the Hacker's Delight book) |
| O-LLVM patterns | Obfuscator-LLVM-specific MBA patterns |
| Constant folding | 22 constant simplification rules |
| Predicate simplification | Opaque predicate removal (setz/setnz/lnot/smod) |
| Z3 rules | Falls back to SMT solving when template matching fails |
| Hodur-specific | MBA patterns from the PlugX (Hodur) malware |

*Control-flow Unflatteners (classified by target obfuscation):*
| Unflattener | Target | Description |
|------------|------|------|
| `Unflattener` | O-LLVM | Standard switch/if-chain + state variable |
| `UnflattenerSwitchCase` | Tigress | Tigress switch-case dispatch (`m_jtbl`) |
| `UnflattenerTigressIndirect` | Tigress | Tigress indirect jump (`m_ijmp`); requires `goto_table_info` configuration |
| `HodurUnflattener` | Hodur (PlugX) | Nested `while(1)` + `jnz state, #CONST`, no switch |
| `BadWhileLoop` | Approov | `while(v8 != C)`, state constants in 0xF6000–0xF6FFF |
| `UnflattenerFakeJump` | Generic | Removes always-true/always-false conditional jumps |
| `SingleIterationLoopUnflattener` | Residue | Cleans up single-iteration loops where `INIT == CHECK` and `UPDATE != CHECK` |
| `UnflattenControlFlowRule` (experimental) | Generic | CFG unflattener based on path emulation |

**Installation and usage:**
```text
1. clone d810-ng
2. Install dependencies (including Z3)
3. Copy to the IDA plugins directory
4. In IDA press Ctrl-Shift-D to load the plugin
5. In the GUI, check the rule sets to apply
6. Apply to the target function
```

**Why d810-ng instead of the original D-810:**
- The original D-810 is minimally maintained
- d810-ng has CI tests, refactored code, and new Tigress/Hodur/Approov-specific unflatteners
- Integrates Z3, falls back to SMT solving when template matching fails; higher success rate

### 3.3 ollvm-unflattener — Miasm Symbolic Execution, Pure Scripting

> [cdong1012/ollvm-unflattener](https://github.com/cdong1012/ollvm-unflattener) · 265⭐ · active 2026-06

Based on the **Miasm** symbolic execution engine; no IDA/BN dependency; pure Python command line.

**Features:**
- Uses Miasm symbolic execution to recover the original control flow (distinct from MODeflattener's purely static approach)
- **BFS multi-layer processing**: automatically follows the target function's calls, recursively deobfuscating
- Supports Windows/Linux x86/x64
- Outputs a new deobfuscated binary

**Installation and usage:**
```bash
git clone https://github.com/cdong1012/ollvm-unflattener.git
cd ollvm-unflattener
pip install -r requirements.txt   # miasm, graphviz, keystone-engine

# Basic usage
python unflattener -i <input.bin> -o <output.bin> -t <function_addr> -a
# -a: automatically follow calls for multi-layer processing
```

**Applicable:** no IDA, target x86/x64, batch scripted processing needed.

### 3.4 ollvm-breaker — Binary Ninja in Practice

> [amimo/ollvm-breaker](https://github.com/amimo/ollvm-breaker) · 441⭐

De-flattening using **Binary Ninja**; the repo ships the Android hardened sample `libvdog.so` as a test case, with the JNI_OnLoad, crazy::GetPackageName, prevent_attach_one functions already fixed.

**Applicable:** Binary Ninja users, Android .so field work.

### 3.5 deollvm — ARM64 Unicorn

> [GeT1t/deollvm](https://github.com/GeT1t/deollvm) · 34⭐ · 2026-04

Unicorn-based ARM64 OLLVM deflat. An alternative for ARM64 .so when you have no IDA.

### 3.6 DeObfBR — BR Obfuscation Specialist

> [Mrack/DeObfBR](https://github.com/Mrack/DeObfBR) · 96⭐ · 2026-06-25

Specifically removes **BR obfuscation** (indirect branch obfuscation, Goron/Arkari style).

**⚠️ Simple countermeasure trick (from awesome-ollvm):** Goron/Arkari-style indirect-related obfuscation can be simply countered by **setting the data section read-only** — indirect jump targets often depend on a runtime-writable data section; once read-only, they become statically solvable.

### 3.7 angr — General Symbolic Execution Framework

```python
import angr

proj = angr.Project("target.so", auto_load_libs=False)
cfg = proj.analyses.CFGFast()
func = proj.kb.functions[0x12345]

# Built-in Deobfuscator
deob = proj.analyses.Deobfuscator(func=func)
deob.normalize()
```

**⚠️ Pluto/Polaris Trap Angr pass:** these two variants specifically wrote traps to defeat angr symbolic execution. If angr path-explodes or crashes, suspect the target uses Trap Angr → switch to d810-ng or a Unicorn dynamic approach.

---

## 4. Complete Deobfuscation Workflow (by Scenario)

### 4.1 General Decision Tree

```
Target binary
  ↓
1. Identify the OLLVM variant (see section 1.2 clues)
  ├── Original OLLVM / Hikari / O-MVLL  → standard fla/bcf/sub
  ├── Pluto / Polaris                → watch for Trap Angr, avoid angr
  ├── Goron / Arkari                 → try data-section-read-only first, then handle BR
  ├── Tigress                        → d810-ng Tigress unflattener
  ├── Hodur (PlugX)                  → d810-ng HodurUnflattener
  └── amice (contains VM)            → not plain fla; needs VM handler recovery
  ↓
2. Choose a tool (see the section 0 decision table)
  ├── Have IDA + network access + non-sensitive sample → obpo-plugin
  ├── Have IDA + local                → d810-ng
  ├── Have Binary Ninja               → ollvm-breaker
  ├── No GUI + x86/x64                → ollvm-unflattener (Miasm)
  ├── No GUI + ARM64                  → deollvm (Unicorn) / angr
  └── Pure symbolic execution / CTF   → angr
  ↓
3. Layered deobfuscation (order matters)
  a) Remove opaque predicates first (bcf)   → d810-ng opaque predicate removal
  b) Then remove control flow flattening (fla) → unflattener
  c) Finally simplify MBA (sub)             → d810-ng MBA simplifier / SiMBA
  ↓
4. Verify
  ├── Function size significantly reduced?
  ├── CFG changed from star/radial shape to chain/tree shape?
  └── Frida hook on key functions to verify logic correctness?
```

### 4.2 Android NDK .so Deobfuscation Special Topic

OLLVM-hardened .so compiled with the Android NDK is the most common scenario in APK reverse engineering.

**Step 1 — Extract the .so:**
```bash
adb pull /data/app/~~/lib/arm64/libnative.so
# Or unzip directly from the APK: unzip target.apk -d out/ ; find out -name "*.so"
```

**Step 2 — Identify OLLVM and the variant:**
```bash
readelf -a libnative.so | grep -E "Size|text"   # .text abnormally large but few functions → probably OLLVM
# Open in IDA and check function signatures:
#   huge switch → fla
#   unreachable branches → bcf
#   complex arithmetic → sub/MBA
#   indirect jump BR x8 → Goron/Arkari, try data-section-read-only
#   while(1) + jnz state → Hodur, use d810-ng HodurUnflattener
```

**Step 3 — Deobfuscate (layered):**
```
a) bcf: d810-ng opaque predicate removal  (or obpo handles it automatically)
b) fla: d810-ng Unflattener / obpo-plugin / deollvm(ARM64)
c) sub: d810-ng MBA simplifier
```

**Step 4 — Frida dynamic verification:**
```javascript
// Trace the OLLVM state variable to help deflat determine the state variable's address
const target = Module.findBaseAddress("libnative.so");
console.log("[+] libnative.so @", target);

// Hook at the dispatcher entry, observe the state change sequence
Interceptor.attach(target.add(0x1234), {  // dispatcher offset
    onEnter(args) {
        // Read the state variable (register/stack location must be determined from the decompilation)
        console.log("[state]", this.context.x8);  // assume state is in x8
    }
});
```

### 4.3 CTF Quick Deobfuscation

CTFs are usually time-pressured; take the fastest path first:

```python
#!/usr/bin/env python3
"""CTF OLLVM quick deflat with angr"""
import angr

proj = angr.Project("challenge", auto_load_libs=False)
cfg = proj.analyses.CFGFast()

# Find the largest few functions (most likely obfuscated)
funcs = sorted(cfg.functions.values(), key=lambda f: f.size, reverse=True)[:5]
for func in funcs:
    print(f"[*] {func.name} @ {hex(func.addr)} size={hex(func.size)}")
    try:
        deob = proj.analyses.Deobfuscator(func=func)
        deob.normalize()
        print(f"    [+] deobfuscated")
    except Exception as e:
        print(f"    [-] failed: {e}")
        # angr fails → suspect Trap Angr → switch to d810-ng / Unicorn
```

---

## 5. MBA Expression Simplification

### 5.1 Common OLLVM MBA Patterns

```python
# These equivalences are the simplification targets for expressions generated by the OLLVM sub pass
"(a | b) + (a & b)"        # → a + b
"(a | b) - (a & b)"        # → a ^ b
"(a ^ b) + 2*(a & b)"      # → a + b
"(a | b) & ~(a & b)"       # → a ^ b
"~(~a & ~b)"               # → a | b (De Morgan)
```

### 5.2 Tool Selection

| Tool | Approach | Applicable |
|------|------|------|
| **d810-ng MBA simplifier** | Batch inside IDA, Z3-verified | First choice, integrated into the decompilation workflow |
| **SiMBA** (`pip install simba-simplifier`) | CLI/library | Pure expression simplification, batch processing |
| **Arybo** | Symbolic bit-vectors | Large numbers of MBA expressions |
| **Direct Z3 solving** | SMT | Most general, when template matching all fails |

```python
# SiMBA example
from simba import simplify_mba
exprs = ["(a | b) + (a & b)", "(a ^ b) + 2*(a & b)"]
for e in exprs:
    print(f"{e}  →  {simplify_mba(e)}")
```

---

## 6. Complete Deobfuscation Case Script

```bash
#!/bin/bash
# OLLVM deobfuscation pipeline (2026 community tools)
# Applicable to ELF/.so hardened with standard OLLVM / Hikari / O-MVLL

BINARY=$1

echo "[*] Stage 0: Basic analysis and variant identification"
file $BINARY
readelf -h $BINARY 2>/dev/null | head -5
echo "    → Confirm the variant in IDA (see section 1)"

echo "[*] Stage 1: d810-ng local deobfuscation (first choice)"
echo "    IDA → Ctrl-Shift-D to load d810-ng"
echo "    Check: MBA + Opaque predicate + Unflattener"
echo "    Apply to target functions"
echo "    Save the IDB"

echo "[*] Stage 2: obpo-plugin (if d810-ng results are insufficient and network is available)"
echo "    IDA → right-click dispatcher → OBPO → Mark and process"
echo "    ⚠️ Do not use on sensitive samples (binary uploaded to cloud service)"

echo "[*] Stage 3: No-IDA alternative (x86/x64)"
echo "    python unflattener -i $BINARY -o deobf.bin -t <func_addr> -a"

echo "[*] Stage 4: ARM64 .so no-IDA alternative"
echo "    deollvm (Unicorn) or angr Deobfuscator"

echo "[+] Done. Re-analyze in IDA to verify."
```

---

## 7. Common Pitfalls (Community Field Summary)

| Problem | Cause | Solution |
|------|------|---------|
| angr path explosion/abnormal exit | Pluto/Polaris **Trap Angr** pass | Switch to d810-ng or a Unicorn dynamic approach |
| obpo-plugin unreachable | Server self-funded, may be down | Fall back to local d810-ng; you may file an issue in the obpo repo |
| Goron/Arkari indirect jump deflat fails | Dispatcher uses BR x8 instead of switch | First make the data section read-only, then use DeObfBR |
| Function still messy after d810-ng | OLLVM used custom pass parameters/seed | First use symbolic execution to remove opaque predicates, then unflatten |
| Nested fla (multi-layer flattening) not fully cleaned in one pass | obpo/d810-ng cleans only one layer at a time | **Iterative processing**: mark each newly appearing dispatcher |
| ARM64 .so deflat script errors | Old deflat scripts only support x86 | Use d810-ng / obpo (supports ARM64) / deollvm |
| Hikari strings invisible | String Encryption pass | Use Unicorn to emulate the decryption stub, dump the decrypted strings |
| amice target completely unaffected by deflat | Contains VM Flatten / Instruction Virtualization | **Not OLLVM fla**; requires VM handler recovery (see VM reversing) |
| Hodur(PugX) sample has no switch dispatcher | Nested while(1) + jnz state | Use d810-ng **HodurUnflattener**, not the plain Unflattener |
| Approov sample state constants show no pattern | Constants concentrated in 0xF6000–0xF6FFF | Use the d810-ng **BadWhileLoop** unflattener |
| Sensitive sample mistakenly run through obpo | Binary uploaded to cloud service | For classified/unpublished-vulnerability samples **use local tools only** (d810-ng/angr) |
| Frida hook on OLLVM function hangs | State variable modified causing infinite loop | Add a conditional breakpoint at the dispatcher entry to limit execution count |

---

## 8. Tool Quick Reference (2026 Community Activity)

| Tool | Platform | Approach | Stars/price | Last update | Open source | Notes |
|------|------|------|---------|---------|------|------|
| **obpo-plugin** | IDA | microcode+concolic (cloud) | 629 | 2026-06 | Plugin open source/core closed | Strongest results, requires network |
| **ollvm-breaker** | Binary Ninja | BN API | 441 | 2026-06 | ✅ | Android .so field-proven |
| **ollvm-unflattener** | CLI | Miasm symbolic execution | 265 | 2026-06 | ✅ | x86/x64, BFS multi-layer |
| **d810-ng** | IDA | microcode+Z3 | 223 | 2026-06 | ✅ | **Local first choice**, broad variant coverage |
| **DeObfBR** | — | BR obfuscation specialist | 96 | 2026-06 | ✅ | Goron/Arkari indirect branches |
| **IDA_Ollvm-unflattener** | IDA | Miasm plugin version | 90 | 2026-04 | ✅ | IDA plugin wrapper of ollvm-unflattener |
| **deollvm** | CLI | Unicorn | 34 | 2026-04 | ✅ | ARM64 specialist |
| **angr** | CLI | Symbolic execution | — | Active | ✅ | General purpose, countered by Trap Angr |
| **SiMBA** | CLI/library | MBA simplification | — | — | ✅ | Expression simplification |
| **Triton** | CLI | Symbolic execution + taint | — | Active | ✅ | Dynamic symbolic execution |

---

## 9. Reference Links

**Obfuscators (for understanding the adversary):**
- [obfuscator-llvm/obfuscator](https://github.com/obfuscator-llvm/obfuscator) — original OLLVM
- [HikariObfuscator/Hikari](https://github.com/HikariObfuscator/Hikari) — Hikari
- [komimoe/Hikari](https://github.com/komimoe/Hikari) — Arkari (based on goron, LLVM 14+)
- [amimo/goron](https://github.com/amimo/goron) — goron
- [bluesadi/Pluto](https://github.com/bluesadi/Pluto) — Pluto
- [za233/Polaris-Obfuscator](https://github.com/za233/Polaris-Obfuscator) — Polaris (formerly Pluto)
- [open-obfuscator/o-mvll](https://github.com/open-obfuscator/o-mvll) — O-MVLL
- [fuqiuluo/amice](https://github.com/fuqiuluo/amice) — Rust implementation of OLLVM passes
- [lich4/awesome-ollvm](https://github.com/lich4/awesome-ollvm) — **variant ecosystem overview (strongly recommended reading first)**

**Deobfuscation tools:**
- [obpo-project/obpo-plugin](https://github.com/obpo-project/obpo-plugin) — strongest cloud plugin
- [w00tzenheimer/d810-ng](https://github.com/w00tzenheimer/d810-ng) — local first choice
- [cdong1012/ollvm-unflattener](https://github.com/cdong1012/ollvm-unflattener) — Miasm pure scripting
- [amimo/ollvm-breaker](https://github.com/amimo/ollvm-breaker) — Binary Ninja
- [GeT1t/deollvm](https://github.com/GeT1t/deollvm) — ARM64 Unicorn
- [Mrack/DeObfBR](https://github.com/Mrack/DeObfBR) — BR obfuscation specialist
- [maskelihileci/IDA_Ollvm-unflattener](https://github.com/maskelihileci/IDA_Ollvm-unflattener) — IDA plugin version
- [angr](https://angr.io/) — symbolic execution framework
- [SiMBA](https://github.com/tech-srl/simba) — MBA simplification

**Academic/blogs:**
- [Quarkslab: Deobfuscation: Recovering an OLLVM-protected program](https://blog.quarkslab.com/deobfuscation-recovering-an-ollvm-protected-program.html) — classic deflat principles
- [MODeflattener](https://github.com/mrT4ntr4/MODeflattener) — static deflat (contrast to ollvm-unflattener)

> Related documents: [[anti-analysis.md]] (anti-debugging/anti-analysis master table), [[tools-advanced.md]] (advanced toolset), [[elf-analysis.md]] (ELF file analysis), [[ai-assisted-re.md]] (AI-assisted reverse engineering)
