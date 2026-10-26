---
name: cheat-engine-reversing
description: Use Cheat Engine for memory analysis, value scanning, pointer finding, and code injection during game/software reversing. Use this skill whenever the user needs to find where values are stored in memory, modify runtime values, trace what writes to addresses, create persistent pointers, or inject custom code into running processes. Also use for CTF challenges, game analysis, or understanding how programs store and modify data in memory. Make sure to use this skill when the user mentions memory scanning, game hacking, reverse engineering, finding addresses, pointer scanning, or any task involving runtime memory analysis.
---

# Cheat Engine Reversing Guide

Cheat Engine is a powerful memory scanning and modification tool for analyzing running programs. This guide covers memory scanning, value modification, pointer scanning, and code injection techniques for reversing and CTF work.

## Quick Start

1. **Download** from https://www.cheatengine.org/downloads.php
2. **Complete the built-in tutorial** - highly recommended for learning the interface
3. **Attach to a process** - click the computer icon and select the target process

## Memory Scanning

### Initial Scan

When searching for a value in memory:

1. **Select the scan type**:
   - `Exact Value` - You know the current value
   - `Unknown initial value` - You don't know the starting value
   - `Between two values` - Value is in a range
   - `Increased/Decreased value` - Track changes

2. **Choose the value type**:
   - `4 Bytes` (default for integers)
   - `Float` or `Double` for decimal values
   - `String` for text data
   - `Array of bytes` for custom formats

3. **Optional**: Check "Stop game while scanning" for precise timing

### Filtering Results

After the initial scan, narrow down candidates:

1. **Change the value in the target program** (e.g., lose health, spend currency)
2. **Perform a "Next Scan"** with the new value or change type
3. **Repeat** until you have a small number of candidates

**Pro tip**: If you have many results, make the value change multiple times and scan after each change.

### Unknown Initial Value

When you don't know the starting value but know how it changes:

1. Perform scan with `Unknown initial value`
2. Make the value change in the program
3. Select the change type (e.g., "Decreased value" by 1)
4. Perform "Next Scan"
5. Results show all values modified in that way

## Modifying Values

Once you've found the correct address:

1. **Double-click** the address in the list
2. **Double-click** the value field
3. **Enter the new value**
4. **Check the checkbox** to apply the modification

The change is immediately applied to memory. Note: the game may not update the display until it reads the value again.

## Finding What Accesses/Writes to an Address

To understand how a value is used:

1. **Right-click** the found address
2. Select:
   - `Find out what accesses this address` - See all code reading this value
   - `Find out what writes to this address` - See code modifying this value (more specific)
3. **Play the game** and trigger the value change
4. The debugger window fills with addresses that modify the value
5. **Double-click** an address to view disassembly

This is essential for understanding game logic and finding injection points.

## Pointer Scanning

Memory addresses change between game runs. Pointers provide persistent access.

### Finding a Pointer

1. Find the value address using normal scanning
2. Use `Find out what writes to this address`
3. Double-click a result to open disassembly
4. Look for hex values in brackets like `[edx]` or `[eax+1234]`
5. **Scan for that hex value** as an exact value
6. Usually the smallest address is the correct pointer base

### Adding a Pointer

1. Click `Add Address Manually`
2. Check the `Pointer` checkbox
3. Enter the pointer address (e.g., `Tutorial-i386.exe+2426B0`)
4. The first address auto-populates from the pointer
5. Click OK

Now you can modify the value even after restarting the game, as the pointer resolves to the current memory location.

### Pointer Scanner 2 (CE 7.4+)

For more robust pointer finding:

- Use `Pointers must end with specific offsets` to reduce false positives
- Use the **Deviation slider** to filter unstable pointers
- After rescan, press `Ctrl+A → Space` to mark all, then `Ctrl+I` to invert and deselect failed addresses
- Use `Compare results with other saved pointer map` to find resilient base pointers

## Code Injection

Inject custom assembly code to modify program behavior.

### Using Auto Assembler

1. Find the instruction you want to modify
2. Click `Show disassembler`
3. Press `Ctrl+A` to open Auto Assembler
4. Select `Template → Code Injection`
5. The address is usually autofilled

### Template Structure

```
[ENABLE]
originalcode:
  ; Original instructions here

newmem:
  ; Your injected code here
  ; Example: add points instead of subtracting
  add [life], 2
  jmp returnhere

originalcode:
  ; Jump back to original code
  jmp newmem
  nop

returnhere:
```

6. **Click Execute** to inject the code

### 1-byte JMP (CE 7.5+)

For size-constrained routines:
- CE automatically generates a 1-byte JMP stub (0xEB)
- Installs an SEH handler and places INT3 at original location
- Enables "tight" hooks in packed or constrained code

## Advanced Features (CE 7.x)

### Ultimap 3 - Intel PT Tracing

Record every branch without single-stepping (won't trip most anti-debug):

```
Memory View → Tools → Ultimap 3 → Check «Intel PT»
Select number of buffers → Start
```

After capture: `Right-click → Save execution list to file`

Combine with `Find out what addresses this instruction accesses` to locate game-logic hotspots.

### DBVM - Kernel-Level Stealth

CE's built-in Type-2 hypervisor (AMD-V/SVM supported):

1. Create hardware breakpoints invisible to Ring-3/anti-debug
2. Read/write protected kernel memory
3. Perform VM-EXIT-less timing-attack bypasses

**Note**: DBVM won't load with HVCI/Memory-Integrity enabled on Windows 11.

### Remote Debugging with ceserver

Attach to Linux, Android, macOS, iOS over TCP:

```bash
# On target (arm64)
./ceserver_arm64 &

# On analyst workstation
adb forward tcp:52736 tcp:52736
# Or use SSH tunnel

# In Cheat Engine: Network icon → Host = localhost → Connect
```

For Frida integration: see `bb33bb/frida-ceserver` on GitHub.

### Other Notable Tools

- **Patch Scanner** (MemView → Tools) - Detects unexpected code changes in executable sections
- **Structure Dissector 2** - Drag address → `Ctrl+D` → `Guess fields` to auto-evaluate C-structures
- **.NET & Mono Dissector** - Improved Unity support; call methods from Lua console
- **Big-Endian custom types** - Reversed byte order scan/edit for console emulators
- **Autosave & tabs** for AutoAssembler/Lua windows

## OPSEC & Installation Notes

### Installation

- Official installer includes ad-offers - **always click Decline** or compile from source
- AVs flag `cheatengine.exe` as HackTool (expected behavior)

### Anti-Cheat Evasion

- Modern anti-cheat (EAC/Battleye, ACE-BASE.sys) detects CE's window class even when renamed
- **Run in a disposable VM** or disable network play
- For user-mode only: `Settings → Extra → Kernel mode debug = off` to avoid unsigned driver BSOD on Windows 11 24H2

## Common Workflows

### Finding and Freezing a Value

1. Scan for exact value
2. Change value in game, perform next scan
3. Repeat until 1-5 candidates remain
4. Test each by modifying - correct one affects the game
5. Right-click → `Freeze` to lock the value

### Creating a Persistent Cheat

1. Find the value address
2. Use pointer scanning to find the base pointer
3. Add as pointer address
4. Optionally inject code to modify behavior
5. Save the pointer table for future use

### Analyzing Game Logic

1. Find a value of interest
2. Use `Find out what writes to this address`
3. Analyze the disassembly to understand the logic
4. Use code injection to modify behavior
5. Use Ultimap 3 to trace execution paths

## Hotkeys Configuration

Set up hotkeys in `Edit → Settings → Hotkeys`:

- **Stop game** - Useful for precise memory scanning
- **Freeze/unfreeze** - Quick value locking
- **Scan** - Rapid iteration

## References

- [Cheat Engine 7.5 release notes](https://github.com/cheat-engine/cheat-engine/releases/tag/7.5)
- [frida-ceserver cross-platform bridge](https://github.com/bb33bb/frida-ceserver-Mac-and-IOS)
- Complete the built-in Cheat Engine tutorial for hands-on learning
