# Symbol Migration Prompt Template

## Standard comparison prompt (copy and use directly)

```text
I have disassembly outputs and procedure code of the same function.

This is the function for reference:

**Disassembly for Reference**
```c
{disasm_for_reference}
```

**Procedure code for Reference**
```c
{procedure_for_reference}
```

This is the function you need to reverse-engineering:

**Disassembly to reverse-engineering**
```c
{disasm_code}
```

**Procedure code to reverse-engineering**
```c
{procedure}
```

What you need to do is to collect all references to "{symbol_name_list}" in the function you need to reverse-engineering and output those references as YAML.

Example:
```yaml
found_vcall:
  - insn_va: '0x180777700'
    insn_disasm: call [rax+68h]
    vfunc_offset: '0x68'
    func_name: ILoopMode_OnLoopActivate

found_call:
  - insn_va: '0x180888800'
    insn_disasm: call sub_180999900
    func_name: CLoopMode_RegisterEventMapInternal

found_funcptr:
  - insn_va: '0x180666600'
    insn_disasm: lea rdx, sub_15BC910
    funcptr_name: CLoopMode_OnClientPollNetworking

found_gv:
  - insn_va: '0x180444400'
    insn_disasm: mov rcx, cs:qword_180666600
    gv_name: g_pNetworkMessages

found_struct_offset:
  - insn_va: '0x1801BA12A'
    insn_disasm: mov rcx, [r14+58h]
    offset: '0x58'
    size: 8
    struct_name: CResourceService
    member_name: m_pEntitySystem
```

If nothing found, output an empty YAML. DO NOT output anything other than the desired YAML. DO NOT collect unrelated symbols.
```

## Variable filling notes

| Variable | Source | How to obtain |
|------|------|---------|
| `{disasm_for_reference}` | Old-version IDA | `idapro_disasm(addr="function name")` |
| `{procedure_for_reference}` | Old-version IDA | `idapro_decompile(addr="function name")` |
| `{disasm_code}` | New-version IDA | `idapro_disasm(addr="corresponding address")` |
| `{procedure}` | New-version IDA | `idapro_decompile(addr="corresponding address")` |
| `{symbol_name_list}` | Extracted from the old version | Extract all symbol names that are not sub_/loc_ from the reference code |

## Batch invocation script skeleton (Python)

```python
import yaml
import httpx
import json
from pathlib import Path

PROMPT_TEMPLATE = open("prompt-template.txt").read()

def migrate_function(ref_disasm, ref_procedure, target_disasm, target_procedure, symbols, api_url, api_key, model="deepseek-chat"):
    prompt = PROMPT_TEMPLATE.format(
        disasm_for_reference=ref_disasm,
        procedure_for_reference=ref_procedure,
        disasm_code=target_disasm,
        procedure=target_procedure,
        symbol_name_list=", ".join(symbols)
    )
    
    resp = httpx.post(api_url, json={
        "model": model,
        "messages": [{"role": "user", "content": prompt}],
        "temperature": 0
    }, headers={"Authorization": f"Bearer {api_key}"}, timeout=60)
    
    content = resp.json()["choices"][0]["message"]["content"]
    
    # Extract the YAML block
    if "```yaml" in content:
        yaml_str = content.split("```yaml")[1].split("```")[0]
    elif "```" in content:
        yaml_str = content.split("```")[1].split("```")[0]
    else:
        yaml_str = content
    
    return yaml.safe_load(yaml_str)


def apply_results(results, ida_session):
    """Apply the parsed YAML results to IDA"""
    if not results:
        return
    
    renames = []
    comments = []
    
    if "found_call" in results:
        for item in results["found_call"]:
            # Extract the call target from insn_disasm
            # call sub_XXXXXXX → extract the address of sub_XXXXXXX
            renames.append({"addr": item["insn_va"], "name": item["func_name"], "type": "call_target"})
    
    if "found_funcptr" in results:
        for item in results["found_funcptr"]:
            renames.append({"addr": item["insn_va"], "name": item["funcptr_name"], "type": "funcptr_target"})
    
    if "found_gv" in results:
        for item in results["found_gv"]:
            renames.append({"addr": item["insn_va"], "name": item["gv_name"], "type": "gv"})
    
    if "found_vcall" in results:
        for item in results["found_vcall"]:
            comments.append({
                "addr": item["insn_va"],
                "comment": f"vcall: {item['func_name']} @ +{item['vfunc_offset']}"
            })
    
    if "found_struct_offset" in results:
        for item in results["found_struct_offset"]:
            comments.append({
                "addr": item["insn_va"],
                "comment": f"{item['struct_name']}.{item['member_name']} @ +{item['offset']}"
            })
    
    return {"renames": renames, "comments": comments}
```

## API configuration suggestions

```yaml
# Use DeepSeek by default (cheap)
default:
  api_url: "https://api.deepseek.com/v1/chat/completions"
  model: "deepseek-chat"
  
# Fall back to GPT for oversized functions
fallback:
  api_url: "https://api.openai.com/v1/chat/completions"
  model: "gpt-4o"

# Or use Claude
alternative:
  api_url: "https://api.anthropic.com/v1/messages"
  model: "claude-sonnet-4-20250514"
```
