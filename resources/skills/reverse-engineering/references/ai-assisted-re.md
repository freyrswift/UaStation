# AI-Assisted Reverse Engineering

> LLM-driven decompilation / multi-agent verification / neural semantic recovery
> The biggest paradigm shift of 2025-2026

## Core Tools and Models

### LLM4Decompile
- The first open-source framework to use LLMs for binary → source decompilation
- Supports x86/ARM/MIPS multiple architectures
- Input: assembly code → output: C source
- Training data: millions of source-assembly pairs

### Decaf (2026)
- **Compiler feedback verification**: compile the LLM-generated source → compare against the original binary
- Effect: decompilation rate 26% → 83.9% (ExeBench Real -O2)
- Key insight: feedback loops beat larger models

### Constraint-Guided Multi-Agent (2026)
- Three-tier verification pipeline:
  1. Syntactic correctness (parsing)
  2. Compilability (GCC)
  3. Behavioral equivalence (LLM-generated test cases)
- 84-97% re-executable rate, only $0.03-0.05 per run

### REMEND (2026)
- Specialized: extracting mathematical equations from binaries
- 89.8-92.4% accuracy (across 3 ISAs × 3 optimization levels × 2 languages)
- Speed: 0.132s/function, only 12M parameters

### Glaurung
- Open-source Ghidra alternative, Rust core + Python bindings
- **AI-native architecture**: LLM agents embedded in every analysis layer
- Evidence artifacts: plain/rich/JSON/JSONL multi-format output for LLM consumption
- Supports: ELF/PE/Mach-O, x86/ARM/RISC-V, IOC detection, entropy analysis

## Workflow: AI-Enhanced Binary Analysis

### 1. LLM-assisted quick reconnaissance

```text
□ Extract strings → LLM semantic classification (URLs/keys/paths/protocols)
□ Import table analysis → LLM infers functionality (crypto=OpenSSL? network=libcurl?)
□ Disassembly snippets → LLM recognizes patterns (crypto algorithms, anti-debug, VM detection)
□ Error messages → LLM infers context ("Invalid license" → where the licensing logic lives)
```

### 2. Neural decompilation

```bash
# LLM4Decompile
python llm4decompile.py --binary target.so --arch arm64 --output target.c

# Verify the result (recompile + compare)
gcc -O2 -o target_recompiled target.c -fPIC -shared
# → verify output behavioral equivalence
```

### 3. Multi-agent verification

```text
Agent 1 (syntax): check whether the generated C code can parse
  ↓ fail → feed the error back to the LLM to retry
Agent 2 (compile): GCC compile → check warnings/errors
  ↓ fail → feed the compile errors back to the LLM
Agent 3 (behavior): LLM generates inputs → run the original and recompiled versions → compare outputs
  ↓ mismatch → feed the differences back to the LLM → iterate the fix
```

### 4. LLM-assisted static analysis

```text
□ Function renaming: feed decompiled pseudocode → LLM suggests semantic names
□ Type recovery: analyze context → LLM infers struct/class definitions
□ Algorithm identification: assembly snippet → LLM identifies the crypto algorithm (AES/TEA/RC4/custom)
□ Protocol reverse engineering: network packet sequences → LLM infers the protocol format
□ Comment generation: decompiled code → LLM generates Chinese/English comments
```

### 5. macOS/iOS private framework reverse engineering (MOTIF)

```text
Problem: macOS private frameworks are undocumented, type information missing
Approach: LLM analyzes usage patterns → infers method signatures and parameter types
Effect: ObjC signature recovery 15% → 86% (vs static analysis)
```

## LLM Prompt Templates

### Function semantic analysis

```
You are a reverse engineering expert. Analyze this decompiled function:

[pseudocode]

1. What does this function do? (one sentence)
2. Suggest a meaningful function name.
3. What are the input parameters and their likely types?
4. What is the return value?
5. What external APIs/functions does it depend on?
6. Any security-relevant operations (crypto, auth, network, file I/O)?
```

### Algorithm identification

```
Analyze this assembly/disassembly for cryptographic operations:

[assembly code]

1. Is this a known cryptographic algorithm? (AES/DES/RC4/TEA/ChaCha20/custom?)
2. Identify the key schedule and round structure.
3. What is the key size?
4. Are there any hardcoded constants that identify the algorithm?
```

### Protocol format inference

```
Given this network packet sequence, infer the protocol structure:

[hex dump]

1. Identify magic bytes and length fields.
2. Propose a struct definition for the packet header.
3. What field(s) appear to be checksums/CRCs?
4. Is this a known protocol or custom?
```

## Tool Selection

| Scenario | Recommended tool | Cost |
|------|---------|------|
| Fast decompilation | LLM4Decompile | Free (local GPU) |
| High-precision decompilation | Constraint-Guided Multi-Agent | ~$0.05/binary |
| Mathematical function extraction | REMEND | Free |
| Full-platform RE | Glaurung (Rust) | Free and open source |
| LLM interaction | Claude API / GPT-4 / DeepSeek | ~$0.01-0.10/call |

## Limitations

- **Complex control flow**: virtualized/obfuscated code remains hard (control flow flattening, VMProtect)
- **Indirect calls**: virtual function tables and function pointers are hard to recover
- **Inlined functions**: boundaries blur after compiler inlining
- **Floating point**: semantic recovery of vectorized instructions still needs improvement
- **Context window**: large functions (>1000 lines) exceed LLM context limits

Source: Decaf (2026), REMEND (2026), Constraint-Guided Multi-Agent Decompilation (2026), LLM4Decompile, Glaurung
