# .NET Reverse Engineering General Workflow

Full workflow details, IL patch reliability, string decryptor extraction, state machine identification, and dnlib scripting.

## Full Workflow (end to end)

```text
1. Identify  → confirm it is a .NET managed program (not native)
2. Detect    → identify the obfuscator with DIE / de4dot --detect
3. Deobf     → de4dot deobfuscation (keep the original sample)
4. Static    → browse the C# view in dnSpyEx to orient, inspect key logic in the IL view
5. Dynamic   → set breakpoints on key methods with the dnSpyEx debugger, watch runtime plaintext
6. Patch     → modify with the IL editor, Save Module
```

Every step's artifacts should be saved to disk: original sample `target.exe` → unpacked `target-clean.exe` → patched `target-patched.exe`.

## IL patch vs C# patch reliability

**Core conclusion: make key modifications in the IL editor, not the C# editor.**

| Dimension | C# editor (Edit Method C#) | IL editor (Edit IL) |
|------|---------------------------|---------------------|
| Compilation failure risk | High (missing references, syntax, lambda rewrite failures) | Nearly zero |
| Information fidelity | Compiler regenerates IL, may differ from the original IL | Replaced as-is, edited instruction by instruction |
| Suited for | Changing a string, changing a constant, simple logic | Changing checks, removing validation, changing control flow |
| async/await/state machines | Often fails to compile or distorts | Directly edit state machine fields, reliable |

dnSpyEx's C# decompiler is based on read-only decompilation + attempted recompilation; recompiling compiler-generated code (state machines, closures, `yield`) fails easily. The IL editor edits instruction by instruction — what you see is what you get.

### Typical IL patch patterns

```text
Change a check (if (check) → always true):
  Original: call bool Foo::Check()
      brfalse.s SKIP
  Change to: ldc.i4.1            ; push true
      brfalse.s SKIP      ; now never jumps, SKIP is never executed
  Or more directly:
      ldc.i4.1
      ret                 ; the method simply returns true

Change a check (if (check) → always false):
  ldc.i4.0
  ret

Remove an entire validation block:
  nop everything, or change to ret + the correct return value

Change a string constant:
  The C# editor is usually OK for strings (ldstr just swaps the token), but if the string lives in resources/encryption you must change the decryption logic

Change a numeric constant:
  Directly edit the operand of ldarg / ldc instructions
```

## State Machine Identification (async/await / yield)

C# `async/await` and `IEnumerator` yield compile into **state machines**: the compiler generates a nested class where `MoveNext()` uses the `state` field for switch dispatch. The dnSpyEx C# view restores these as async, but decompilation may distort; the IL view of `MoveNext()` is the most accurate.

```text
MoveNext structure of async/await:
  switch(this.<>1__state) {
    case 0: ... logic before the await; this.<>1__state = 1; await MoveNext;
    case 1: ... logic after the await;
  }

To patch async logic: change the state transitions in MoveNext or the checks inside specific cases.
Editing async functions in the C# editor almost always fails → you must use IL.
```

## String Decryptor Extraction

See `obfuscators.md` for details. Here we add dnlib scripting for batch string decryption:

```csharp
// dnlib script: scan all string decryptor calls, restore at runtime and write back
// Usage: dotnet script decrypt.csproj target.exe 0x06000012
using System;
using System.Reflection;
using dnlib.DotNet;
using dnlib.DotNet.Writer;
using dnlib.DotNet.Emit;

var module = ModuleDefMD.Load(args[0]);
var decryptorToken = uint.Parse(args[1], System.Globalization.NumberStyles.HexNumber);

// Find the decrypt method and invoke it via reflection (the assembly must be loaded into the AppDomain)
// Iterate over all methods, replacing call Decryptor(token) with ldstr "decrypted result"
foreach (var type in module.GetTypes())
    foreach (var method in type.Methods)
    {
        if (!method.HasBody) continue;
        var instrs = method.Body.Instructions;
        for (int i = 0; i < instrs.Count; i++)
        {
            // Recognize the call-to-decryptor pattern, invoke the decryptor to get plaintext, replace with ldstr
            // (boilerplate for the reflection call to the decryptor is omitted here; the idea: load the original assembly →
            //   MethodInfo.Invoke to get plaintext → instrs[i] = OpCodes.Ldstr + operand=plaintext)
        }
    }

var opts = new ModuleWriterOptions(module);
module.Write("target-decrypted.exe", opts);
```

dnlib is the de facto standard for .NET metadata programming; de4dot uses it internally. It is the first choice when writing custom deobfuscation scripts.

## Dynamic Debugging Essentials

The dnSpyEx debugger is far friendlier for .NET programs than for native ones:

- **Breakpoint at method entry**: right-click the method → Add Breakpoint
- **Inspect object values**: once broken, the Locals / Watch windows show object fields and string contents directly
- **Memory writes**: you can directly change runtime variable values (Edit Value)
- **Exception breakpoints**: Debug → Exceptions, check the exception types to break on —— obfuscators often use exception-driven control flow; breaking on exceptions reveals the real path

### Exception-driven control flow

Some obfuscators stuff normal logic into `try` and use `throw` + `catch` for jumps. Statically the IL looks like exception handling, but it is actually control flow:

```text
try { throw new CustomException(0x42); }
catch (CustomException e) {
    switch(e.Code) {
        case 0x42: real logic A; break;
        case 0x43: real logic B; break;
    }
}
```

Set an exception breakpoint (break on `CustomException`) and trace the flow of the `Code` values — faster than grinding through the IL.

## Module Initializer (Module .cctor)

A .NET module's static constructor (the `.cctor` of `<module>`) runs first when the assembly loads; obfuscators often put anti-tamper / decryption initialization there. Analysis order:

```text
1. First look at <module>.cctor (Module .cctor) —— decryption/anti-debug initialization
2. Then look at Program.Main / Startup
3. If anti-tamper lives in .cctor → patch .cctor first, then unpack
```

## General Pattern for Extracting Config / C2 / Keys

Red team tools and loaders often embed encrypted configuration in resources or fields and decrypt it at runtime:

```text
Localization flow:
1. Run strings to check for plaintext URL/IP (usually absent after obfuscation)
2. Find byte[] fields + the decryption method (AES/XOR)
3. Dynamically break at the decryption method's return point, dump the decrypted plaintext
4. Common: AES-256-CBC with Key==IV (Codegate 2013 pattern, see the .NET section of reverse-engineering/tools.md)
```

See `references/sharp-tools.md` for the concrete configuration structures of red team tools.

## Boundary with reverse-engineering

- **IL2CPP / NativeAOT** → compiled to native, no CLR metadata → use `reverse-engineering/` (IDA/r2); this skill only identifies them
- **Managed .NET** (standard C# exe/dll, Mono/Unity managed layer, Xamarin) → this skill
- **Hybrid (native loader + .NET payload)** → the loader part goes to `reverse-engineering/`; after dumping the .NET payload, switch to this skill

## On-Disk Artifact Checklist

For every .NET reverse engineering task, produce:
- `target-original.exe` (original sample, untouched)
- `target-clean.exe` (after de4dot unpacking)
- `notes.md` (identified obfuscator, decryptor token, key method addresses, config/C2/keys)
- `target-patched.exe` (after patching, if needed)
- `il-diff.txt` (IL before/after comparison, if patching was done)
