# .NET Obfuscator Deobfuscation In Detail

Identification, unpacking, and anti-tamper bypass for mainstream .NET obfuscators. Core tools: **de4dot** (auto-detects most shells) + **dnSpyEx** (manual patching) + **dnlib** (scripting).

## Master Decision Table

| Obfuscator | de4dot type | Typical signatures | Auto-unpack | Manual essentials |
|--------|-------------|---------|---------|---------|
| ConfuserEx 1.x/2.x | `cfze` | anti-tamper, control flow deformation, string encryption, anti-debug | ✅ mostly automatic | Newer versions need anti-tamper patched first |
| ConfuserEx 3.x / private mods | `cfze` | same as above + custom protector | ⚠️ partial | runtime dump / dnlib |
| SmartAssembly | `sa` | string encoding, resource compression, method call hiding | ✅ automatic | resource decompression |
| Babel.NET | `babel` | method body encryption, control flow, strings | ✅ automatic | — |
| Eazfuscator.NET | `eaz` | string/resource encryption, expression obfuscation | ⚠️ partial | string decryptor |
| .NET Reactor | `reactor` | necrobit (code segment encryption) + anti-tamper | ⚠️ hard on new versions | dump + rebuild metadata |
| Themida .NET | — | outer shell + virtualization | ❌ de4dot can't | dump memory, go native-style |
| Agile.NET / CliSecure | `agile` | method body encryption | ✅ automatic | — |

## de4dot Standard Usage

```powershell
# Auto-detect (enough most of the time)
de4dot target.exe -o target-clean.exe

# Explicitly specify the type (when auto-detection fails)
de4dot --type cfze target.exe -o target-clean.exe

# Probe the shell type first
de4dot --detect target.exe

# Batch
de4dot *.exe

# Only decrypt strings, leave control flow alone (minimal intervention)
de4dot --strtyp delegate --strtok METHOD_TOKEN target.exe
```

de4dot's `--strtyp` / `strtok` mode: decrypts only the string decryptor (specifying the decryptor method token) and keeps the original control flow. Suited for the scenario "just want to see plaintext strings without touching anti-tamper".

---

## ConfuserEx (most common)

### Signature identification

- The entry `<module>` class has `[MethodImpl(NoInlining)]` anti-tamper checks
- Heavy `Dictionary<string, T>` string decryptor calls
- Control flow flattening (switch dispatch + state variable)
- `.cmp` compressed resources embedded in resources
- dnSpyEx C# view: garbled class/method names (`\uXXXX` or meaningless characters), method bodies full of `int num = ...; switch(num)`

### Unpacking flow

```powershell
# 1. Standard unpacking
de4dot target.exe -o target-clean.exe

# 2. If de4dot reports "unknown" or the output won't open → newer/private ConfuserEx
#    Confirm anti-tamper first:
Open in dnSpyEx → find the integrity check in Module .cctor or Main
```

### anti-tamper bypass (common with newer ConfuserEx)

ConfuserEx's `anti tamper` verifies method body hashes at runtime and crashes if they were modified. de4dot usually handles old versions; newer ones need manual work:

```text
Method A — patch the check function directly in dnSpyEx:
  1. Find the anti-tamper check method (usually invoked in <module>'s static constructor)
  2. IL edit: change the check method body to ret (return immediately)
  3. Save → feed it to de4dot again

Method B — runtime dump:
  1. Run it and dump the in-memory assembly with MegaDumper / ExtremeDumper
  2. The dump is already decrypted; use de4dot to clean up leftovers
```

### After control flow restoration

de4dot restores the flattened switch dispatch back into normal if/while. If restoration is incomplete (leftover state machines visible), run de4dot again or follow the IL manually.

---

## SmartAssembly

```powershell
de4dot --type sa target.exe -o target-clean.exe
```

Signatures:
- Strings encoded with the `SmartAssembly.Runtime.Strong` family
- Resource compression (`{assembly}.Resources`)
- Method call hiding (`ProcessCaller` / indirect calls)

de4dot has the best compatibility with SmartAssembly; essentially one-shot.

---

## .NET Reactor (necrobit)

`.NET Reactor`'s **necrobit** stores the real method bodies encrypted in resources, decrypting and injecting them at runtime, while the original method bodies are empty shells. de4dot works on old versions but often fails on newer ones (4.x+).

```text
When de4dot fails:
1. Let the program run (dotnet target.exe or just double-click)
2. MegaDumper / ExtremeDumper to dump process memory → export the decrypted assembly
3. Use de4dot to clean leftover obfuscation from the dump artifact
4. If the metadata is corrupted, rebuild it with dnlib (see common-workflow.md)
```

---

## Manual String Decryptor Extraction

Obfuscators encrypt strings and call a decryption method at runtime to restore them. de4dot auto-detects most decryptors; when detection fails, do it manually:

```text
1. Find the decrypt method in dnSpyEx (usually a fixed signature: static string Decrypt(int) or Decrypt(string, int))
   - Signatures: called heavily, parameters are numeric constants, returns string
2. Note the method token (e.g. 0x06000012)
3. Tell de4dot which decryptor:
   de4dot --strtyp delegate --strtok 0x06000012 target.exe -o target-clean.exe
```

If the decrypt method itself is obfuscated (control flow flattened), deobfuscate the control flow first, then locate the decryptor.

## Common anti-debug tricks

| Trick | Location | Bypass |
|------|------|------|
| `Debugger.IsAttached` check | any method | IL edit to `ldc.i4.0; ret` or patch the getter |
| `Debugger.IsLogging` | — | same as above |
| Timing checks (`DateTime.Now` deltas) | method entry | patch out the delta comparison |
| `CheckRemoteDebuggerPresent` P/Invoke | — | nop out the call |
| Exception-driven control flow (try/catch path selection)| main logic | cannot simply nop; analyze the real path in the catch blocks |

> .NET anti-debug is simpler than native —— most of it is managed API calls; a one-line dnSpyEx IL edit suffices.

## Fallbacks when de4dot fails

1. **de4dot --detect** to see the identification result, compare against the table above
2. **Runtime dump** (MegaDumper / ExtremeDumper / Process Hacker module export)
3. **dnlib scripts** to solve it manually (see the dnlib section of common-workflow.md)
4. **Dynamic first**: run it, break at the decryption point, read the plaintext directly — intelligence can be gathered without unpacking

Community references: Washi's blog "misconceptions-about-dotnet" (common IL analysis misconceptions), the Kanxue .NET reverse engineering board, Guided Hacking "Top 5 .NET RE Tools".
