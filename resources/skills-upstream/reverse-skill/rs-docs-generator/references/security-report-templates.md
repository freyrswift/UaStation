# Security / Reverse Engineering / Penetration Testing Document Templates

This file provides document templates for security-related projects such as reverse engineering, penetration testing, and vulnerability analysis. After completing a task, the AI should create a new document in the user's project directory and output it according to the corresponding template.

---

## 0. Evidence Chain (all security reports MUST include it)

> Full contract: `skills/ops/evidence-finding-path.md`
> Case directory: `work/<case>/` (`case-init.ps1`)

The report body **MUST** contain the following sections (they may be merged into "Key Findings" but no fields may be omitted):

### 0.1 Scope Summary
- Link to `scope.md`: `auth` / `in_scope` / `network_profile`
- No scope → the task cannot be claimed as complete

### 0.2 Evidence
At least 1 entry, fields: `E-id` / `source_ref` / `repro_command` / `content_hash|n/a`

### 0.3 Findings
Each entry: `F-id` / `severity|n/a_re` / `evidence_ids` / `confidence` / `location` / `status`

### 0.4 Path
At least 1 `P-id`: `path_type=attack|callflow|solve`; steps may reference E/F

### 0.5 Timeline Summary
Link to `timeline.md` or embed the 3–10 key appended records

---

---

## 0.6 Vendor structure overlay (professional vendor report structure)

> Full rules: `references/vendor-report-rules.md` (Issue #65)
> **MUST** be read and applied when generating formal security reports; **extract structure only — copying vendor original text / IOC examples is forbidden**.

| Flavor / Overlay | Scenario | Skeleton in one sentence |
|------------------|------|------------|
| `malware` | confirmed malicious sample / ordinary trojan / white-plus-black | Huorong style: overview→workflow→sample analysis→incident handling→IOC |
| `apt` | APT / campaign / multi-stage chain | Kaspersky style: summary→infection chain→investigation→interesting findings→technical analysis→detection & mitigation→IOC |
| `flavor = null` | ordinary reversing / pentest / CTF / JS signatures | this section's task templates + applicable Base common elements |
| thin `vuln` | vulnerability / patch / CVE technical analysis (explicit) | overview→impact/reproduction→crash and patch analysis→protection recommendations |

**Common elements (G1–G7) summary**: G1 executive summary MUST · G2 Scope MUST · G3 E/F/P MUST · G4 IOC only `malware`/`apt` MUST · G5 recommendations MUST for `malware`/`apt`/`vuln` · G6 appendix SHOULD · G7 ATT&CK MUST for `apt`

Selection and section order follow `vendor-report-rules.md`; when it conflicts with §0.1–0.5, **the Evidence contract takes precedence**.

## 1. Reverse Engineering Report Template

```markdown
# [Target Name] Reverse Engineering Analysis Report

> Analysis date: YYYY-MM-DD
> Analyst: [AI / human]
> Toolchain: [jadx / IDA / radare2 / Frida / ...]

## 1. Target Overview

| Attribute | Value |
|------|---|
| File name | |
| File type | APK / ELF / PE / Mach-O / ... |
| Size | |
| MD5 | |
| SHA256 | |
| Package name / entry | |

## 2. Analysis Objectives

<!-- The core questions this reverse engineering effort should answer -->

## 3. Static Analysis

### 3.1 Basic Information
<!-- Architecture, compiler, protections, string characteristics -->

### 3.1.1 Import table / dependencies (MUST for binaries)
<!-- Record an E-imports / E-triage-imports summary; record Evidence even on failure — skipping is forbidden -->

### 3.2 Key Functions/Classes
<!-- List the located key logic, with code snippets -->

### 3.3 Encryption/Signing Algorithms
<!-- If encryption is involved, describe the algorithm, key source, and parameter construction -->

## 4. Dynamic Analysis

### 4.1 Hook Records
<!-- Targets and results of Frida / xposed / other hooks -->

### 4.2 Runtime Behavior
<!-- Network requests, file operations, process behavior -->

## 5. Key Findings

<!-- List key conclusions with numbering -->

1. ...
2. ...
3. ...

## 6. Reproduction Steps

<!-- So that others can reproduce your analysis results -->

```bash
# Key commands
```

## 7. Open Issues

<!-- Points not fully resolved -->

## 8. Attachments

<!-- Hook scripts, decryption code, screenshots, etc. -->
```

---

---

## 1b. Malware / APT Report (vendor flavor)

When the task is malware analysis, a virus report, or APT/campaign analysis, do **not** just deliver the "reverse engineering" skeleton above; ordinary reversing tasks keep the original template and do not automatically select a vendor flavor:

1. Read `vendor-report-rules.md` and select `malware` or `apt`
2. Output following the corresponding section order
3. Still **MUST** include the §0 Evidence chain; the `malware` / `apt` flavors additionally **MUST** include an IOC table
4. Static analysis of binary samples **MUST** include an import-table Evidence (consistent with the radare2/ida/malware hard gate)

## 1c. Vulnerability Technical Analysis Report (thin `vuln` overlay)

When the task is an **OS/component vulnerability, patch comparison, or CVE technical analysis**, or the user explicitly requests a "vulnerability technical analysis report":

1. Read `vendor-report-rules.md` §3b and use the thin `vuln` section order (**not** the full malware/apt flavor)
2. **MUST** include: impact scope, in-scope reproduction or an explicit n/a, crash/root-cause or patch-diff Evidence, protection/patch recommendations
3. **MUST** include §0 Evidence→Finding→Path
4. **MUST NOT** extend the PoC against unauthorized targets, or transcribe external weaponization details

## 2. Penetration Testing Report Template

```markdown
# [Target] Penetration Test Report

> Test date: YYYY-MM-DD
> Test scope: [URL / IP / application name]
> Authorization status: [authorized / CTF / learning environment]

## 1. Executive Summary

<!-- One paragraph summarizing: what was tested, what was found, risk level -->

## 2. Test Scope

| Item | Details |
|------|------|
| Target | |
| Test type | black box / gray box / white box |
| Test period | |
| Tools | |

## 3. Findings Summary

| # | Vulnerability Name | Risk Level | Status |
|---|---------|---------|------|
| 1 | | High/Medium/Low/Info | Verified/To be confirmed |

## 4. Vulnerability Details

### 4.1 [Vulnerability Name]

**Risk level**: High / Medium / Low

**Description**:

**Impact**:

**Reproduction steps**:

1. ...
2. ...
3. ...

**Evidence**:

```
<!-- Requests/responses/screenshots/payloads -->
```

**Remediation recommendations**:

## 5. Attack Path

<!-- If there is a complete attack chain, draw the path -->

```
Entry point → reconnaissance → exploitation → privilege escalation → objective achieved
```

## 6. Tools and Environment

| Tool | Version | Purpose |
|------|------|------|
| | | |

## 7. Remediation Recommendations Summary

| Priority | Recommendation |
|--------|------|
| P0 | |
| P1 | |
| P2 | |

## 8. Appendix

<!-- Full payloads, scripts, configuration files, etc. -->
```

---

## 3. CTF Writeup Template

```markdown
# [Competition Name] - [Challenge Name] Writeup

> Category: Web / Reverse / Pwn / Crypto / Misc / Forensics
> Difficulty: Easy / Medium / Hard
> Points: N pts
> Solve time:

## Challenge Description

<!-- Original challenge description -->

## Solution Approach

### Step 1: Reconnaissance
<!-- What was observed -->

### Step 2: Vulnerability / Breakthrough
<!-- What key point was found -->

### Step 3: Exploitation
<!-- How it was exploited -->

## Key Code/Payload

```python
# exploit code
```

## Flag

```
flag{...}
```

## Pitfalls

<!-- Dead ends taken along the way -->

## Knowledge Points

<!-- Knowledge points involved in this challenge, useful for later review -->
```

---

## 4. JS/Web Signature Reverse Engineering Report Template

```markdown
# [Site/Application] Signature Parameter Reverse Engineering Report

> Analysis date: YYYY-MM-DD
> Target endpoint: [URL]
> Signature field: [field name]

## 1. Target Request

```http
POST /api/xxx HTTP/1.1
Host: example.com

param1=xxx&sign=<target field>
```

## 2. Location Process

### 2.1 Breakpoint/Hook Method
<!-- How the signature generation location was found -->

### 2.2 Call Stack
<!-- The key call chain -->

## 3. Algorithm Recovery

### 3.1 Algorithm Type
<!-- HMAC-SHA256 / AES / custom / ... -->

### 3.2 Parameter Construction
<!-- Which fields participate in the signature, ordering rules, separators -->

### 3.3 Key Source
<!-- Hardcoded / returned by an API / derived from a timestamp / ... -->

## 4. Local Reproduction Code

```javascript
// Node.js reproduction
```

## 5. Verification Results

<!-- Compare the signature generated by the reproduction code against actual requests -->

## 6. Anti-Crawler / Risk-Control Notes

<!-- Rate limits, device fingerprinting, environment detection, etc. -->
```

---

## 5. Document Output Conventions

### Output Location

- Documents are written by default to the **user's current project directory** (not the skill package directory)
- File name format: `YYYY-MM-DD_[type]-[target-short-name]-report.md`
- If the user's project has a `docs/` directory, prefer placing it under `docs/`

### Output Timing

The AI automatically invokes this skill to generate documents at the following moments:

1. A reversing task is complete with key conclusions produced
2. A penetration test is complete with vulnerabilities found and verified
3. A CTF challenge is solved and the flag obtained
4. The user explicitly requests "write a report/document"

### Quality Requirements

- All code blocks must be directly runnable or have clear context
- No placeholders/TODOs (if a part is genuinely unfinished, mark it "to be added" and explain why)
- Key findings must be backed by evidence (command output, screenshot descriptions, code snippets)
- Reproduction steps must allow a third party to independently reproduce them
