# Vendor Report Rules (Professional Vendor Report Structure Overlay)

> Issue #65, problem 2.  
> **Extract only structure and writing rules; copying any vendor report body text, figures, real IOC examples, or large passages is forbidden.**  
> This file is an **overlay**: it does not replace the task templates in `security-report-templates.md`, nor does it weaken §0 Evidence→Finding→Path.

Structural references (public samples, skeletons only):

| Flavor | Primary reference | Scenarios |
|--------|--------|------|
| `malware` | Huorong Security virus/technical analysis reports | Clear-cut ordinary trojans, white-plus-black abuse, phishing payloads, malicious samples |
| `apt` | Kaspersky Securelist / APT campaign reports (e.g., MATA) | APTs, gang campaigns, multi-stage infection chains, industry targeting |

Principle: **quality over quantity in templates** — only 2 vendor full flavors (`malware` / `apt`) + Base common elements + **optional thin overlay** (e.g., `vuln` vulnerability technical analysis). Ordinary reverse engineering, penetration testing, CTF, and JS reports keep their task templates and are not dressed up as malware reports by default; `vuln` is **NOT** a third default full flavor.

---

## 0. When to Activate

When `docs-generator` produces **security-class** reports (reverse engineering / malware / pentest wrap-up / user explicitly requests a "professional report" or "vendor style"), you **MUST** read this file. Choose a vendor flavor only when task evidence or an explicit user request supports it; otherwise use `flavor = null` and apply only the common professional elements and the original task template.

| Signal | Flavor / Overlay |
|------|------------------|
| APT / gang / campaign / multi-stage C2 / industry targeting / ICS / spear-phish campaign | `apt` |
| Clear-cut malicious samples, trojans, stealers, white-plus-black abuse, fake sites | `malware` |
| User explicitly requests vulnerability/patch/CVE technical analysis, or task evidence is OS/component vulnerability research | `flavor = null` + **thin overlay `vuln`** (see §3b) |
| Ordinary APK/ELF/PE/Mach-O reverse engineering, algorithm analysis, firmware analysis, penetration testing, CTF, JS signature work | `flavor = null`; use the original task template and the minimal set of common professional elements |

When the user explicitly specifies "Kaspersky/APT style", "Huorong/virus report style", or "vulnerability technical analysis style", that overrides automatic selection.  
**Forbidden** to force ordinary malware/APT/ordinary reverse engineering tasks into the `vuln` outline by default.

---

## 1. Common Professional Elements (Base)

Apply the following Base elements according to report type. Those marked **MUST** cannot be omitted; elements tied to a specific flavor must not appear in unrelated tasks just to fill the template. When nothing applies, use `n/a` and state why.

| # | Element | Requirement |
|---|------|------|
| G1 | Executive summary / overview | **MUST**: 3–8 sentences: what was analyzed, the most severe conclusion, impact scope, recommended actions |
| G2 | Scope and authorization | **MUST**: link to the case `scope.md` (see template §0.1) |
| G3 | Evidence→Finding→Path | **MUST**: see `security-report-templates.md` §0 and `skills/ops/evidence-finding-path.md` |
| G4 | IOC table | **MUST** for `malware` / `apt`; for other tasks only when relevant indicators exist |
| G5 | Recommendations / handling | **MUST** for `malware` / `apt`: at least 1 actionable recommendation; other tasks follow the original task template |
| G6 | Appendix metadata | **SHOULD**: tools and versions, sample hashes, full reproduction commands |
| G7 | ATT&CK mapping | **MUST** (under `apt`; `n/a` + reason when no applicable techniques); **SHOULD** for other tasks |

### 1.1 IOC Table Minimum Columns

```markdown
| Type | Value | Context | First/last seen | Source evidence | Confidence |
|------|----|--------|---------------|----------|--------|
| file_sha256 / file_md5 / domain / ip:port / url / mutex / path / registry | … | where found | YYYY-MM-DD / n/a | E-id | high/med/low |
```

### 1.2 Copyright and Safety Boundaries

- Do not paste paragraphs or figure captions from vendor PDFs/web pages as your own analysis.
- Use placeholders for real tokens, intranet URLs, and customer identifiers.
- For unauthorized targets, do not output directly exploitable attack-step details (follow case scope / RULES).

---

## 2. Flavor: `malware` (Huorong style · explicit choice)

**Narrative goal**: let the reader understand within 5 minutes "what it is → how it arrived → what the sample does → how to handle it → which IOCs exist".

### 2.1 Recommended Section Order

```markdown
# [Title: one-sentence threat characterization]

> Analysis date / analyst / sample identifier (hash)

## 1. Overview
(G1: discovery channel, disguise techniques, core technical points, whether the product can detect/kill it — write n/a if unknown)

## 2. Attack / infection flow
(Flow diagram: Mermaid or a step list; corresponds to Path `path_type=attack`)

## 3. Sample analysis
### 3.1 Sample provenance
### 3.2 Static analysis
(**MUST** include import table / basic identity Evidence: E-imports or equivalent; see radare2/ida/malware hard gates)
### 3.3 Dynamic analysis / behavior
(If no dynamic capability, n/a + reason)
### 3.4 Core findings (Findings table or numbered list, with evidence_ids attached)

## 4. Incident handling
(Execute only within authorization: confirm scope and preserve first — samples, memory, process trees, network connections, and logs — then isolate the host; after approval by the responsible party, terminate processes, quarantine/remove files, check hosts/startup items, run a full scan and re-verify. You must not delete files before evidence preservation.)

## 5. Summary notes
(Risk reminders and prevention for ordinary users/ops)

## 6. IOC information
(G4 table)

## 7. Evidence chain summary
(§0: E / F / P / Timeline; may merge with §3.4 but keep all fields)

## 8. Appendix
(tool versions, reproduction commands, script paths)
```

### 2.2 Writing Style

- Chinese by default for Chinese-speaking users; conclusions before details.
- Layer static analysis by "component/stage"; avoid pasturing unstructured long logs.
- Handling steps must be independently executable; empty platitudes like "raise security awareness" are forbidden.

---

## 3. Flavor: `apt` (Kaspersky Securelist style)

**Narrative goal**: tell the campaign-level story — who hit whom, when, with what chain, how the investigation advanced, how the components divided the work, and what defenders can use to detect.

### 3.1 Recommended Section Order

```markdown
# [Campaign/cluster name]: [one-sentence impact]

> Date / team / industry and regional scope (if known)

## 1. Executive summary
(G1: time window, victim profile, entry point, family/cluster attribution, duration, most important conclusion)

## 2. The infection chain
(By stage: delivery → exploit/loader → main trojan → post-exploitation/exfiltration; mark unknown segments clearly as "limited visibility"
corresponding Path; a chain diagram is recommended)

## 3. Incident investigation
(Investigation narrative: key turning points, intranet proxy/C2 characteristics, how scope was broadened; attach Timeline)

## 4. Interesting findings
(3–7 non-obvious points, each with an E-id / F-id where possible)

## 5. Technical analysis
### 5.1 Component overview table (loader / trojan / stealer / …)
### 5.2 Per-component behavior and configuration
### 5.3 Static highlights (including import table/packing/persistence Evidence)
### 5.4 Network and C2
(an ATT&CK table G7 may be attached)

## 6. Detection and mitigation
(Detection ideas / hunting leads / mitigation priorities; no empty slogans)

## 7. IOC
(G4; grouped by type)

## 8. Evidence chain summary
(§0 fields)

## 9. Appendix
(sample list and hashes, tool versions, public reference IDs; do not copy external report body text)
```

### 3.2 Writing Style

- Be honest about the timeline and "visibility limitations".
- Interesting findings ≠ repeating the summary; write the genuinely key anomalies from the investigation.
- Use tables for component analysis: role / persistence / C2 / dependencies, then expand.

---


## 3b. Thin overlay: `vuln` (vulnerability technical analysis · optional)

> Issue #65 supplement. Structure references the section outlines of public "OS/component vulnerability technical analysis" reports, **extracting only the section skeleton**; copying PoC traffic, exploitation details, or unauthorized attack steps from screenshots/body text is forbidden.  
> **NOT** a third default vendor full flavor; apply only for vulnerability research tasks or on explicit user request.

**Narrative goal**: the reader can quickly see "who is affected → how to confirm/reproduce (within authorization) → root cause and patch delta → how to mitigate".

### Recommended Section Order

```markdown
## 1. Vulnerability overview
### 1.1 Impact scope (versions/components/configuration prerequisites)
### 1.2 Vulnerability reproduction (authorized environment; steps repeatable by third parties; no weaponization-tutorial tone)

## 2. Vulnerability analysis
### 2.1 Crash / anomaly analysis (Evidence: crash logs, trigger conditions)
### 2.2 Patch analysis (diff/guard conditions/fix points — attach E-*)
### 2.3 PoC or trigger analysis (only material already available within the authorized scope; protocol/input construction layers suffice)

## 3. Protection recommendations
### 3.1 Mitigation measures (configuration/mitigation switches, etc.)
### 3.2 Official patch and verification

## 4. Evidence → Finding → Path (may be folded into each section or a standalone table)
```

### Hard Constraints

- **MUST** scope/authorization: reproduction and PoC expansion forbidden for unauthorized targets
- **MUST** E/F/P: reproduction, crash, and patch conclusions all carry evidence_ids
- **MUST NOT** treat `vuln` as the default shell for malware/APT
- **MUST NOT** copy exploit code or full attack weaponization steps from external reports/screenshots
- IOC table: appears only when network/file indicators exist; otherwise n/a or omit

---
## 4. Hooking Into Existing Task Templates

| Task template (`security-report-templates.md`) | Overlay method |
|------------------------------------------|----------|
| 1. Reverse engineering report | Default `flavor = null`, keep the original "static/dynamic/reproduction" skeleton and hard-gate Evidence like the import table; apply §2 only for clearly malicious samples |
| 2. Penetration testing report | `flavor = null`; add applicable G1–G3 from Base, align attack paths with §0 Path, IOC not mandatory |
| 3. CTF write-up | `flavor = null`; keep the original problem statement, solution approach, and reproduction structure, IOC/ATT&CK not mandatory |
| 4. JS/Web signature reverse engineering | `flavor = null`; use the original overview → locate → algorithm → reproduce skeleton, do not apply malware |
| Malware / APT special cases | Explicitly choose the `malware` or `apt` full-text skeleton |

**Conflict resolution**: §0 Evidence chain fields and scope gates **always take precedence**; a flavor only changes narrative order and professional packaging and must not remove E/F/P.

---

## 5. Selection Pseudocode

```
if user_requests_kaspersky or apt or threat_campaign:
    flavor = apt
elif user_requests_huorong or vir_report or explicit_malware:
    flavor = malware
else:
    flavor = null  # original task template + applicable elements from Base
overlay = null
if user_requests_vuln_tech_report or cve_patch_analysis:
    overlay = vuln  # thin only; never a third default full flavor
emit(base_report)
if flavor in (malware, apt):
    emit(report with flavor outline)
elif overlay == vuln:
    emit(report with vuln thin outline)
```

---

## 6. Completion Checklist (self-check at the end of report writing)

- [ ] Flavor selected or explicit "task template + minimal set"
- [ ] G1 overview exists and is not empty talk
- [ ] §0 E/F/P fields complete
- [ ] `malware` / `apt` reports have an IOC table (or n/a + reason)
- [ ] `malware` / `apt` reports have actionable recommendations/handling
- [ ] Tasks without a flavor were not forced into malware/APT-specific sections
- [ ] vuln enabled only for vulnerability tasks; contains overview/analysis/protection skeleton and E/F/P; no unauthorized PoC weaponization
- [ ] No vendor original-text pasting, no placeholder/TODO
- [ ] Hard-gate Evidence such as the import table made it into static/technical analysis (if this task involved binary analysis)

---

## 7. Source Register

- Kaspersky Securelist, "Updated MATA attacks industrial companies in Eastern Europe": <https://securelist.com/updated-mata-attacks-industrial-companies-in-eastern-europe/110829> (structural reference; accessed: 2026-08-11)
- Huorong Security public technical article portal: <https://www.huorong.cn/> (site portal; accessed: 2026-08-11. Specific article URLs, titles, and access dates should be registered when actually cited)
- ATT&CK technique IDs serve only as normalized mappings and must be supported by this engagement's Evidence; do not automatically carry IOCs from external reports into the current report.

---

## 8. Non-Goals

- Do not maintain additional full-text templates for Mandiant/CrowdStrike/QiAnXin, etc. (the dual flavor + optional thin overlay already covers common needs).
- Do not promote `vuln` to a default full-text flavor on par with malware/apt.
- Do not automatically crawl vendor sites to fill reports.
- Do not weaken the Evidence contract or authorization scope because of a flavor.
