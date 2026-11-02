# Code Audit Checklist (Condensed)

- [ ] List of all external input entry points
- [ ] Authn/authz middleware coverage
- [ ] Whether multi-tenant IDs are bound to the session
- [ ] Deserialization / pickle / YAML load
- [ ] SSRF egress and protocol restrictions
- [ ] Key and token storage
- [ ] File upload paths and types
- [ ] Dangerous exec/system/Runtime
