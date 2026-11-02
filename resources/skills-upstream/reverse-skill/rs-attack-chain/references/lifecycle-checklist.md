# Penetration / Attack-Chain Lifecycle Checklist

> Cross-references community pentest skill packages (such as the Orizon claude-code-pentest six phases) with this package's `attack-chain` + `ops` integration.  
> Source inspiration: public Claude pentest lifecycle skills (retrieved 2026-07); **commands and authorization defer to this package's scope**.  
> Date: 2026-07-17

## Before Use

- [ ] `case-init` done, `auth.status=granted`
- [ ] `network_profile` ≠ unrestricted misused against production
- [ ] `lead` has assigned specialist_roles (`ops/role-map.md`)

## Stage Gates

| Stage | Role | Skill in this package | Completion criteria |
|------|------|------------|----------|
| 0 Scope | lead | ops/scope-contract | ready_for_act |
| 1 Recon | cie | pentest-tools | assets list + timeline |
| 2 Enum/Vuln | cpe | pentest-tools / api-security | candidate F-* drafts |
| 3 Validate | cpe | pentest-tools | E-* + validated Finding |
| 4 Post-ex (if authorized) | cpe/lead | attack-chain latter half | stays within out_of_scope |
| 5 RE support | cre | ida/apk/js/… | only when clients/binaries are needed |
| 6 Report | doc | docs-generator | Evidence→Finding→Path |
| 7 Journal | lead | field-journal | sanitized |

## Differences from "give a domain and auto-pwn it" style skills (our edge)

| Common in external automation packages | reverse-skill |
|------------------|---------------|
| Wild scanning of the domain by default | Mandatory scoped asset list |
| Weak evidence written straight into reports | Enforced E/F/P chain |
| Single session, no roles | role-map handoffs |
| No tool index | tool-index + bootstrap |

## At Least One Timeline Entry per Stage

Format in `ops/timeline-workitem.md`.
