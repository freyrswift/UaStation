# Tool Adapters

## Purpose

Make extraction deeper when local CLI tools are available, without making the workflow depend on any single tool.

## Readiness Report

The checkpoint script should emit:

- `raw/tool-availability.txt`
- `indexes/tool-readiness.md`
- `indexes/install-plan.md`

Use these to decide whether the environment is ready for:

- Android unpacking
- managed decompilation
- Unity asset extraction
- IL2CPP reconstruction
- native reverse engineering
- runtime/device work

## Managed Code

Preferred order:

1. `ilspycmd`
2. `monodis`
3. printable-string identifier extraction from DLLs

Expected outputs:

- `raw/managed-identifiers.txt`
- `indexes/types-index.md`

## IL2CPP And Native Code

Preferred order:

1. `readelf`
2. `nm`
3. printable strings plus file inventory

Expected outputs:

- `raw/native-symbols.txt`
- `indexes/native-symbols.md`
- `indexes/recovery-plan.md`

## Environment Variable Overrides

Use these when tools are installed outside `PATH`:

- `UNITY_RESEARCH_ILSPYCMD`
- `UNITY_RESEARCH_MONODIS`
- `UNITY_RESEARCH_CPP2IL`
- `UNITY_RESEARCH_IL2CPP_DUMPER`
- `UNITY_RESEARCH_READELF`
- `UNITY_RESEARCH_NM`
- `UNITY_RESEARCH_OBJDUMP`
- `UNITY_RESEARCH_FILE`
- `UNITY_RESEARCH_STRINGS`

## Policy

- prefer the deepest available deterministic adapter
- if a tool is missing, fall back without blocking the session
- save the result to `raw/` and `indexes/` before reasoning
- never make the user specify these tools in the prompt
- do not install missing tools without explicit user approval
- if installation is approved, use `indexes/install-plan.md` as the starting point
- if a missing tool materially affects the requested topic, emit a user-facing notice with `Required`, `Recommended`, or `Optional`
- if the issue is broader than one known tool, emit a user-facing capability-gap notice instead of pretending the current stack is sufficient
