# Cloud / K8s Checklist (Condensed)

## IMDS
- [ ] Can SSRF reach 169.254.169.254
- [ ] Is IMDSv2 enforced
- [ ] IAM role permission surface returned

## K8s High Risk
- [ ] Too many cluster-admin bindings
- [ ] Secrets as cleartext environment variables
- [ ] privileged + hostPID/hostPath combinations
- [ ] Anonymous auth / insecure apiserver port

## Containers
- [ ] Running as root
- [ ] Can load kernel modules / docker.sock mounted
