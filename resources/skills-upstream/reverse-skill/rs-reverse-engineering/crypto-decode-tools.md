# Encryption/Decryption and Encoding/Decoding Tools Quick Reference

> Reverse engineering and CTFs constantly run into encrypted/encoded/hashed data. This document lists the most useful tools by scenario.

---

## Auto-Detect + Decrypt (when you don't know what cipher was used)

| Tool | Stars | Purpose | Link |
|------|-------|------|------|
| **Ciphey** | 18k+ | AI auto-detection and decryption (supports 50+ encodings/ciphers/hashes) | https://github.com/Ciphey/Ciphey |
| **CyberChef** | 29k+ | Online/offline encoding Swiss army knife (drag-and-drop) | https://github.com/gchq/CyberChef |
| **dcode.fr** | — | 900+ online cipher/encoding/math tools | https://www.dcode.fr/ |

### Using Ciphey

```bash
pip install ciphey
# Auto-detect and decrypt
ciphey -t "ciphertext"
# Read from a file
ciphey -f encrypted.txt
```

Ciphey supports: Base64/32/16, Caesar, Vigenere, XOR, AES (weak keys), Morse, Binary, Hex, URL encoding, HTML entities, hash identification, and more.

### Using CyberChef

```text
Online: https://gchq.github.io/CyberChef/
Offline: download the HTML file from the GitHub Release and open it directly

Common recipes:
- From Base64 → decode Base64
- XOR → XOR decryption (can brute force the key)
- AES Decrypt → AES decryption
- Magic → auto-detect the encoding type
```

---

## Hash Identification and Cracking

| Tool | Purpose | Link |
|------|------|------|
| **hashID** | Identify hash types (MD5/SHA/bcrypt etc.) | https://github.com/psypanda/hashID |
| **hash-identifier** | Same, Python version | https://github.com/blackploit/hash-identifier |
| **haiti** | Modern hash identification tool (more accurate) | `gem install haiti` |
| **Hashcat** | GPU hash cracking | https://hashcat.net/ |
| **John the Ripper** | CPU hash cracking | https://www.openwall.com/john/ |
| **hashes.com** | Online hash lookup (rainbow tables) | https://hashes.com/ |

```bash
# Identify hash type
hashid '5f4dcc3b5aa765d61d8327deb882cf99'
# Output: [+] MD5

# haiti (more accurate)
haiti '5f4dcc3b5aa765d61d8327deb882cf99'

# Crack with Hashcat
hashcat -m 0 hash.txt rockyou.txt  # MD5
hashcat -m 1000 hash.txt rockyou.txt  # NTLM
```

---

## RSA Attacks

| Tool | Purpose | Link |
|------|------|------|
| **RsaCtfTool** | Automated RSA attacks (20+ attack methods) | https://github.com/Ganapati/RsaCtfTool |
| **SageMath** | Math computation (big-integer factorization/elliptic curves) | https://www.sagemath.org/ |
| **factordb.com** | Online big-integer factorization lookup | http://factordb.com/ |
| **yafu** | Local big-integer factorization | https://github.com/bbuhrow/yafu |

```bash
# Automated attacks with RsaCtfTool
python RsaCtfTool.py --publickey pub.pem --private
python RsaCtfTool.py --publickey pub.pem --uncipherfile cipher.txt

# Supported attacks:
# Wiener, Boneh-Durfee, Fermat, Pollard p-1, Williams p+1
# Common modulus, Small q, Hastads, Noveltyprimes, etc.
```

---

## XOR Analysis

| Tool | Purpose | Link |
|------|------|------|
| **xortool** | XOR key-length guessing + known-plaintext attack | https://github.com/hellman/xortool |
| **CyberChef XOR** | Visual XOR operations | Built into CyberChef |

```bash
# Guess the XOR key length
xortool encrypted_file
# Decrypt with the guessed key length
xortool -l 4 -c 00 encrypted_file

# Known-plaintext attack (some plaintext known)
xortool-xor -f encrypted -s "known_plaintext"
```

---

## Classical Ciphers

| Cipher Type | Tool | Notes |
|---------|------|------|
| Caesar | CyberChef / dcode.fr | Brute force 25 shifts |
| Vigenere | dcode.fr / Ciphey | Requires guessing the key length |
| Substitution | quipqiup.com | Frequency-analysis auto-solve |
| Enigma | dcode.fr | Online simulator |
| Rail Fence | dcode.fr / CyberChef | Rail fence cipher |
| Playfair | dcode.fr | Requires a key |
| Morse | CyberChef | Dots/dashes to text |
| Bacon | dcode.fr | Binary steganography |
| ROT13/47 | CyberChef / `tr` | Simple substitution |

---

## Encoding Identification and Conversion

| Encoding | Identification Signature | Decoding Method |
|------|---------|---------|
| Base64 | Trailing `=` or `==`, charset A-Za-z0-9+/ | `base64 -d` / CyberChef |
| Base32 | Uppercase letters + 2-7, trailing `=` | CyberChef |
| Base58 | No 0/O/I/l, common in short identifier encoding | CyberChef |
| Hex | Only 0-9a-f, even length | `xxd -r -p` / CyberChef |
| URL encoding | `%XX` format | `urldecode` / CyberChef |
| HTML entities | `&#XX;` or `&amp;` format | CyberChef |
| Unicode escape | `\uXXXX` format | Python `decode('unicode_escape')` |
| JWT | `xxxxx.yyyyy.zzzzz` (three Base64URL segments) | jwt.io / CyberChef |
| Brainfuck | Only the eight characters `><+-.,[]` | Online interpreter |
| Ook! | Only `Ook.` `Ook!` `Ook?` | Online interpreter |

---

## Cipher Identification During Reverse Engineering

### Identifying Algorithms via Constants

| Constant/Signature | Algorithm |
|-----------|------|
| `0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476` | MD5 |
| `0x6A09E667, 0xBB67AE85, 0x3C6EF372` | SHA-256 |
| `0x63, 0x7C, 0x77, 0x7B` (S-Box start) | AES |
| `0x243F6A88` (hex of π) | Blowfish |
| `0xB7E15163, 0x9E3779B9` | RC5/RC6/TEA |
| `0x61707865` ("expa") | ChaCha20/Salsa20 |
| `0xC6EF3720` | XTEA |

### Identifying via Behavior

| Behavioral Signature | Possible Algorithm |
|---------|-----------|
| 256-byte lookup table + swap operations | RC4 |
| 16-byte blocks + multiple permutation rounds | AES |
| Feistel structure (left/right swap) | DES/Blowfish/TEA |
| Big-integer multiplication/modular exponentiation | RSA |
| Elliptic curve point operations | ECDSA/ECDH |
| Fixed 64-round loop | TEA/XTEA |
| 32 rounds + delta constant | XTEA |

---

## Automated Cryptanalysis

| Tool | Purpose | Link |
|------|------|------|
| **FeatherDuster** | Automated cryptanalysis framework | https://github.com/nccgroup/featherduster |
| **PkCrack** | ZIP known-plaintext attack | https://www.unix-ag.uni-kl.de/~conrad/krypto/pkcrack.html |
| **bkcrack** | ZIP known-plaintext attack (modern) | https://github.com/kimci86/bkcrack |
| **z3** | SMT solver (constraint solving) | https://github.com/Z3Prover/z3 |
| **angr** | Symbolic execution (auto-solve inputs) | https://angr.io/ |

---

## Quick Decision Tree

```text
Given a blob of unknown data:

1. Look at length and charset
   - Only hex characters → possibly hex encoding or a hash
   - Trailing = → Base64
   - Three dot-separated segments → JWT
   - 32/40/64 hex chars → hash (MD5/SHA1/SHA256)

2. Auto-try with Ciphey
   ciphey -t "data"

3. If Ciphey fails → use CyberChef Magic mode

4. If it's a hash → hashID to identify the type → crack with Hashcat/John

5. If it's RSA → automated attacks with RsaCtfTool

6. If it's XOR → analyze the key with xortool

7. If it's traditional ZIP encryption → prefer the bkcrack known-plaintext attack; don't start with evidence-free password brute force

8. If it's a custom cipher → reverse the algorithm in IDA/Ghidra → hand-write the decryption script
```

---

## Online Resources

| Resource | Link | Purpose |
|------|------|------|
| CyberChef | https://gchq.github.io/CyberChef/ | Universal encoding/decoding |
| dcode.fr | https://www.dcode.fr/ | 900+ cipher tools |
| quipqiup | https://quipqiup.com/ | Substitution cipher auto-solve |
| factordb | http://factordb.com/ | RSA big-integer factorization |
| jwt.io | https://jwt.io/ | JWT decoding/verification |
| hashes.com | https://hashes.com/ | Hash reverse lookup |
| crackstation | https://crackstation.net/ | Online hash cracking |
