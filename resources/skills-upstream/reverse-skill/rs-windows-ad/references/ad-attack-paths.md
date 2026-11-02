# AD Attack Path Quick Reference

| Path | Prerequisite | Tool hints |
|------|------|----------|
| Kerberoast | SPN account | GetUserSPNs / Rubeus |
| AS-REP Roast | Pre-authentication not required | GetNPUsers |
| ESC1 | Enrollable template + forgeable SAN | Certipy |
| ESC8 | HTTP enrollment + relay | ntlmrelayx |
| ACL → DA | GenericAll on user/group | BloodHound |
| NTLM Relay | Signing not enforced | Responder + relay |

Always: authorization → enumeration → path scoring → minimal validation → cleanup.
