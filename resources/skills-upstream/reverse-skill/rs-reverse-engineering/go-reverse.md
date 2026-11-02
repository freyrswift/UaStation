# Go Binary Reverse Engineering Guide

> Go-compiled binaries pose unique challenges: static linking produces huge sizes, tens of thousands of functions, unusual string formats, and difficult symbol recovery after stripping.
> This document covers the toolchain, recovery techniques, and a practical workflow.

---

## Identifying Go Binaries

Quickly determine whether a binary was compiled with Go:

```bash
# String signatures
strings binary | grep -E "runtime\.|go\.buildid|GOROOT"

# rabin2 reconnaissance
rabin2 -z binary | grep -i "runtime"

# Abnormally large file size (statically linked runtime)
# Typical Hello World: C ~20KB, Go ~2MB
```

Common characteristics:
- Large numbers of functions with the `runtime.` prefix
- Contains a `go.buildid` section
- Contains `GOROOT`, `GOPATH` path strings
- Function count of 5000-50000+ (including the entire runtime and standard library)

---

## Core Toolchain

### Symbol Recovery

| Tool | Purpose | Link |
|------|------|------|
| **GoReSym** | By Mandiant, parses Go symbol information (pclntab/moduledata) | https://github.com/mandiant/GoReSym |
| **GoResolver** | By Volexity, automatically deobfuscates Garble binaries using CFG similarity | https://github.com/volexity/GoResolver |
| **redress** | Analyzes stripped Go binaries, recovers types/interfaces/package structure | https://github.com/goretk/redress |
| **GoStringUngarbler** | By Google, specializes in recovering Garble-obfuscated strings | https://github.com/mandiant/GoStringUngarbler |

### IDA Plugins

| Tool | Purpose | Link |
|------|------|------|
| **go_parser** | IDA plugin, parses moduledata/pclntab/type information | https://github.com/0xjiayu/go_parser |
| **IDAGolangHelper** | IDA script collection, parses Go type information | https://github.com/sibears/IDAGolangHelper |
| **AlphaGolang** | SentinelLabs IDAPython script collection | https://github.com/SentineLabs/AlphaGolang |
| **IDA 9.2+ native support** | Hex-Rays official Go decompilation improvements | https://hex-rays.com/blog/stop-guessing-and-start-going |

### Ghidra Plugins

| Tool | Purpose | Link |
|------|------|------|
| **Ghidra + GoReSym output** | Export symbols with GoReSym, then import into Ghidra | Used together |
| **golang_loader_assist** | Ghidra Go loading assistant | Community script |

### Standalone Analysis Tools

| Tool | Purpose | Link |
|------|------|------|
| **gore** | Go reverse engineering library (the engine under redress) | https://github.com/goretk/gore |
| **garble** | Go obfuscator (know it to beat it) | https://github.com/burrowers/garble |

---

## Key Structures in Go Binaries

### pclntab (PC Line Table)

The most important structure in a Go binary, containing:
- All function names and address mappings
- Source file paths
- Line number information
- Stack frame sizes

Even after symbols are stripped, pclntab usually still exists (the Go runtime depends on it).

```text
Locating it:
1. Search for magic bytes: 0xFFFFFFF0 (Go 1.16+) or 0xFFFFFFFB (Go 1.18+)
2. Use GoReSym to locate it automatically
3. Use the go_parser IDA plugin to parse it automatically
```

### moduledata

Contains:
- pclntab pointer
- Type information table
- itab (interface table)
- Global variable information

### String Format

Go strings are not C-style null-terminated; they are `(pointer, length)` structures:

```text
C string:  "hello\0"
Go string: struct { ptr *byte; len int } → ptr points to "hello" (no \0)
```

This means the default string identification in IDA/Ghidra misses large numbers of Go strings.

**Solutions**:
- Use `go_parser` to identify Go strings automatically
- Use GoReSym to export the string list
- Manually: find `runtime.stringtable` or locate strings via cross-references

---

## Practical Workflow

### Scenario 1: Non-stripped Go Binary

```text
1. GoReSym -t -d -p binary > symbols.json
   → exports all function names, types, source file paths
2. Load into IDA/Ghidra
3. Import GoReSym's symbol information
4. Filter out runtime.* and standard-library functions; focus on user code
5. Start analysis from main.main
```

### Scenario 2: Stripped Go Binary

```text
1. GoReSym -t -d -p binary > symbols.json
   → even after stripping, pclntab usually remains
2. If GoReSym fails → use redress
   redress -src binary    # recover source file paths
   redress -pkg binary    # recover package structure
   redress -type binary   # recover type information
3. Load into IDA + the go_parser plugin
4. Run go_parser for automatic recovery
5. Start from the recovered main.main
```

### Scenario 3: Garble-Obfuscated Go Binary

```text
Garble will:
- Randomize function names (main.main → main.a3f2b1c)
- Encrypt strings
- Remove file path information
- Obfuscate package names

Countermeasures:
1. GoResolver (CFG signature matching)
   → recovers standard-library function names via control-flow-graph similarity
2. GoStringUngarbler (string decryption)
   → automatically recognizes Garble's string encryption patterns and decrypts
3. Dynamic analysis (Frida/dlv)
   → hook runtime functions to observe actual behavior
4. Comparative analysis
   → compile a Hello World with the same Go version, use binary-diff to compare the runtime portion
```

### Scenario 4: Mixed CGo Builds

```text
1. Identify the CGo boundary (_cgo_* functions)
2. Recover the Go part with go_parser
3. Analyze the C part with regular IDA
4. Watch bridge functions like _cgo_topofstack, crosscall2
```

---

## Common Command Quick Reference

```bash
# GoReSym: export symbols
GoReSym -t -d -p binary > symbols.json
GoReSym -t -d -p binary -o ida_script.py  # generate an IDA script

# redress: analyze stripped binaries
redress -src binary          # source file paths
redress -pkg binary          # package structure
redress -type binary         # type information
redress -interface binary    # interface information
redress -filepath binary     # full file paths

# GoResolver: deobfuscate Garble
GoResolver -binary binary -output resolved.json

# GoStringUngarbler: decrypt Garble strings
GoStringUngarbler -i binary -o deobfuscated_binary

# Quickly determine the Go version
strings binary | grep "go1\."
GoReSym -p binary | grep "Version"
```

---

## Go Analysis Workflow in IDA

```text
1. Load the binary (choose the correct architecture)
2. Wait for automatic analysis to finish
3. Run the go_parser plugin:
   - File → Script File → go_parser.py
   - or Edit → Plugins → Go Parser
4. The plugin automatically:
   - parses pclntab
   - recovers function names
   - marks Go strings
   - parses type information
5. Filter views:
   - hide runtime.* functions
   - focus on main.* and third-party packages
6. Start reversing from main.main
```

---

## Common Pitfalls

| Pitfall | Description | Solution |
|------|------|------|
| Too many functions to review | Go static linking yields 5000-50000 functions | Filter by package name; only look at main.* and business packages |
| Incomplete string identification | Go strings are not null-terminated | Recover with go_parser or GoReSym |
| Hard-to-read decompilation | Go's defer/goroutine/interface make pseudocode complex | IDA 9.2+ has improvements, or assist with dynamic analysis |
| Garble obfuscation | Function names/strings all randomized | GoResolver + GoStringUngarbler |
| Version differences | pclntab formats differ across Go versions | GoReSym supports Go 1.2-1.23+ |
| CGo boundary | Mixed Go and C code | Identify _cgo_* functions as the dividing line |

---

## Working with Other Skills

| Need | Use |
|------|--------|
| Deep IDA analysis of Go binaries | `ida-reverse/` + go_parser plugin |
| Ghidra analysis (free) | Ghidra + GoReSym symbol import |
| Quick reconnaissance | `radare2/` — `rabin2 -z` for strings |
| Dynamic hooking | Frida (hook runtime functions) or dlv (native Go debugger) |
| Cross-version comparison | `binary-diff/` — migrate old-version symbols to the new version |
| Garble deobfuscation | GoResolver + GoStringUngarbler |
