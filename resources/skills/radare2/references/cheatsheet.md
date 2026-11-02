# radare2 Cheat Sheet

## Basic Reconnaissance

```powershell
rabin2 -I sample.exe
rabin2 -S sample.exe
rabin2 -i sample.exe
rabin2 -E sample.exe
rabin2 -zz sample.exe
```

## Entering the Interactive Session

```powershell
r2 sample.exe
```

```text
aaa
afl
iz
iS
is
s entry0
pdf
q
```

## Strings and References

```text
iz~http
iz~error
axt <addr>
s <addr>
pdf
```

## Common Viewing

```text
px 64
pd 20
psz
pxa
```

## Patching

```powershell
r2 -w sample.exe
```

```text
s 0x401000
wa nop
wx 9090
wq
```

## Non-Interactive Mode

```powershell
r2 -A -q -c "afl;iz;ii;q" sample.exe
```

## Other Tools

### rasm2

```powershell
rasm2 -d "9090"
rasm2 -a x86 -b 64 "xor eax, eax"
```

### radiff2

```powershell
radiff2 old.exe new.exe
radiff2 -C old.exe new.exe
```

### rahash2

```powershell
rahash2 -a md5 sample.exe
rahash2 -a sha256 sample.exe
```

### rax2

```powershell
rax2 0x401000
rax2 4198400
rax2 -s hello
```

## radare2-skills Ecosystem Commands

### r2xsql query examples

```powershell
r2xsql -s sample.exe -q "SELECT name, module FROM imports WHERE name LIKE '%Crypt%'"
r2xsql -s sample.exe -q "SELECT addr, content FROM strings WHERE content LIKE '%http%'"
```

### r2http / r2mcp session

```powershell
r2 -N -e http.bind=localhost -e http.port=9393 -e http.sandbox=false -q -c=h sample.exe
curl.exe -sS --data-binary 'aaa' http://127.0.0.1:9393/cmd
curl.exe -sS --data-binary 'aflj' http://127.0.0.1:9393/cmd
```

### radius2 symbolic execution

```powershell
radius2 -p sample.exe -s stdin 96 -X Incorrect
radius2 -p sample.exe -s flag 256 -A . flag -B Correct -X Wrong -j
```

### r2pm plugin installation

```powershell
r2pm -ci r2ghidra
r2pm -ci r2dec
r2pm -l
```
