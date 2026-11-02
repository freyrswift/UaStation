# APK Security Testing Quick Reference

> Compiled from OWASP MASTG (Mobile Application Security Testing Guide).
> Covers six dimensions: static analysis, dynamic analysis, network communication, data storage, authentication/authorization, and code protection.

---

## Static Analysis Checklist

### Manifest Audit

```text
□ android:debuggable="true" → debuggable (should never appear in production)
□ android:allowBackup="true" → data can be backed up and extracted
□ Components with android:exported="true" → exposed Activity/Service/Receiver/Provider
□ Custom permission protectionLevel → whether it is normal (should be signature)
□ scheme in intent-filters → whether custom deeplinks can be hijacked
□ android:usesCleartextTraffic="true" → allows cleartext HTTP
□ minSdkVersion too low → may be missing security features
```

### Code Audit Key Points

```text
□ Hardcoded keys/tokens (search "key", "secret", "password", "api_key")
□ Insecure random numbers (java.util.Random instead of SecureRandom)
□ Insecure crypto (ECB mode, DES, MD5 for passwords)
□ WebView configuration (setJavaScriptEnabled + addJavascriptInterface = RCE risk)
□ SQL injection (rawQuery concatenating user input)
□ Path traversal (ContentProvider openFile without path validation)
□ Log leakage (Log.d/Log.i outputting sensitive information)
□ Clipboard leakage (ClipboardManager storing sensitive data)
□ Implicit Intent leakage (sendBroadcast without a specified package name)
```

### Third-Party Library Audit

```text
□ Outdated OkHttp/Retrofit versions (known vulnerabilities)
□ Outdated WebView engine
□ SDKs with known vulnerabilities (check CVEs)
□ Ad SDK data collection scope
□ Push SDK configuration (does it leak tokens)
```

---

## Dynamic Analysis Checklist

### Priority Frida Hook Targets

| Target | Hook Point | Purpose |
|------|---------|------|
| Login authentication | `LoginActivity.login()` | Observe credential handling |
| Signature generation | `*Sign*`, `*sign*`, `*encrypt*` | Recover the signature algorithm |
| SSL Pinning | `CertificatePinner.check` | Bypass for traffic capture |
| Root detection | `*root*`, `*su*`, `*magisk*` | Bypass detection |
| Crypto operations | `javax.crypto.Cipher` | Extract keys/IVs |
| Token storage | `SharedPreferences.getString` | Observe token reads/writes |
| Network requests | `OkHttpClient.newCall` | Observe request construction |

### Common One-Line Frida Commands

```bash
# Trace all crypto operations
frida-trace -U -f com.target.app -j '*Cipher*!*'

# Trace all HTTP requests
frida-trace -U -f com.target.app -j '*OkHttp*!*'

# Trace SharedPreferences reads/writes
frida-trace -U -f com.target.app -j '*SharedPreferences*!*'

# Trace all native function calls
frida-trace -U -f com.target.app -i 'Java_*'
```

### Quick Objection Commands

```bash
# Connect
objection -g com.target.app explore

# Common commands
android hooking list activities
android hooking list services
android sslpinning disable
android root disable
android clipboard monitor
env                              # view app directories
sqlite connect <db_path>         # connect to a database
```

---

## Network Communication Security

### Traffic Capture Setup

```text
Method 1: system proxy + Burp/mitmproxy
- Set the WiFi proxy → Burp listener address
- Install the CA certificate on the device
- Android 7+ requires network_security_config or a Frida bypass

Method 2: VPN mode (recommended)
- Use HttpCanary / Packet Capture
- No root needed, no proxy configuration needed
- But cannot decrypt SSL-pinned traffic

Method 3: Frida + r2frida
- Intercept network calls directly inside the process
- Unaffected by proxy/VPN limitations
```

### Check Items

```text
□ Is HTTPS used (for all API calls)
□ Is there SSL Pinning (certificate pinning)
□ Is certificate verification correct (does not accept self-signed)
□ Is there a Certificate Transparency (CT) check
□ Are API keys transmitted in cleartext in requests
□ Do tokens have an expiry mechanism
□ Is there request signing against tampering
□ Is there replay attack protection (nonce/timestamp)
□ Are WebSockets encrypted
□ Is sensitive data in URL parameters (will get logged)
```

---

## Data Storage Security

### Locations to Check

| Location | Risk | Check Command |
|------|------|---------|
| SharedPreferences | Cleartext tokens/passwords | `adb shell cat /data/data/pkg/shared_prefs/*.xml` |
| SQLite databases | Unencrypted sensitive data | `adb pull /data/data/pkg/databases/` |
| External storage | Readable by any app | `adb shell ls /sdcard/Android/data/pkg/` |
| App logs | Debug information leakage | `adb logcat \| grep pkg` |
| Backup files | allowBackup=true | `adb backup -f backup.ab pkg` |
| Keyboard cache | Input history | Check whether `inputType` is `textPassword` |
| Screenshot protection | Sensitive pages screenshottable | Check `FLAG_SECURE` |

### Encrypted Storage Options Compared

| Option | Security | Notes |
|------|--------|------|
| Plaintext SharedPreferences | ❌ | Directly readable after root |
| EncryptedSharedPreferences | ✓ | AndroidX Security library |
| SQLCipher | ✓ | Encrypted SQLite |
| Android Keystore | ✓✓ | Hardware-backed key protection |
| Custom AES encryption | ⚠️ | Depends on key management |

---

## Authentication and Authorization

### Common Vulnerabilities

| Vulnerability | Testing Method |
|------|---------|
| Weak password policy | Try 123456, password, etc. |
| No lockout mechanism | Brute force the login endpoint |
| Tokens never expire | Replay an old token after logging out |
| Broken access control | Modify user_id in requests |
| SMS codes brute-forceable | 4/6-digit numbers with no rate limit |
| OAuth misconfiguration | redirect_uri tamperable |
| Biometric auth bypass | Hook BiometricPrompt |
| Device binding bypass | Modify device_id |

### Test Payloads

```bash
# Broken access control test
curl -H "Authorization: Bearer USER_A_TOKEN" \
     "https://api.target.com/users/USER_B_ID/profile"

# Token replay
# 1. Log in normally to obtain a token
# 2. Log out
# 3. Request with the old token → should return 401

# SMS verification code brute force
for code in $(seq 0000 9999); do
    curl -X POST "https://api.target.com/verify" \
         -d "phone=13800138000&code=$code"
done
```

---

## Code Protection Assessment

| Protection | Detection Method | Bypass Difficulty |
|---------|---------|---------|
| ProGuard obfuscation | Check with jadx whether class names are a/b/c | Low (renaming only) |
| String encryption | Find the decryption function, hook to get plaintext | Medium |
| Anti-debugging | Try attaching a debugger | Medium (Frida can bypass) |
| Root detection | Run on a rooted device | Medium (generic script bypass) |
| Emulator detection | Run on an emulator | Low-Medium |
| Integrity checks | Install after modifying the APK | Medium (patch the check function) |
| Packing/hardening | Inspect the entry class and .so | Medium-High (unpacking needed) |
| Native protection | Core logic in .so | High (IDA analysis needed) |
| VMP virtualization | Code runs virtualized | Extremely high |

---

## Quick Test Workflow (30 Minutes)

```text
1. [5min] Unpack + Manifest audit
   apktool d app.apk
   Check debuggable/allowBackup/exported/cleartext

2. [10min] Quick code audit
   jadx -d out app.apk
   Search: password, key, secret, token, http://

3. [5min] Network testing
   Configure a proxy → operate the app → check for cleartext/weak crypto

4. [5min] Storage check
   adb shell → inspect shared_prefs and databases

5. [5min] Dynamic verification
   Frida hook key functions → confirm findings
```
