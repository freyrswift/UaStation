# CI/CD Pipeline Security Audit

## Pipeline Attack Surface

```text
Threat model (STRIDE):
□ Spoofing: forged builds/signatures/provenance
□ Tampering: modifying source code/build artifacts/dependencies
□ Repudiation: malicious operations without audit logs
□ Information disclosure: pipeline logs/build artifacts leaking secrets
□ Denial of service: exhausting CI resources/breaking builds
□ Elevation of privilege: runner escape/secret theft
```

## Audit Checklist

### 1. Pipeline as Code configuration

```yaml
# GitHub Actions audit points
# ❌ Dangerous pattern
on:
  pull_request_target:  # PR trigger with secrets access
    types: [opened]

# ❌ Script injection
- run: echo "${{ github.event.issue.title }}"  # user input → shell

# ❌ Unrestricted token permissions
permissions: write-all

# ✅ Safe pattern
on:
  pull_request:  # no secrets access
    types: [opened]

# ✅ Pinned to SHA
- uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683

# ✅ Least privilege
permissions:
  contents: read
```

### 2. Secret management

```bash
# Scan historical commits for secrets
gitleaks detect --source . --verbose
trufflehog git file://. --only-verified

# Check Actions Secrets usage
gh secret list
# Confirm: no hardcoded secrets, regular rotation, least privilege

# Runtime secret injection
# ✅ Use OIDC instead of long-lived secrets
# ✅ Secrets exposed only to the specific steps that need them
```

### 3. Build integrity

```bash
# Build provenance
# Generate tamper-proof build records (SLSA L2+)
slsa-provenance generate --source . --output provenance.json

# Artifact signing
cosign sign-blob --key cosign.key artifact.tar.gz

# Verify
cosign verify-blob --key cosign.pub --signature artifact.tar.gz.sig artifact.tar.gz
```

### 4. Runner security

```text
□ Using GitHub-hosted runners? (recommended, fresh environment each time)
□ Self-hosted runners: running inside isolated VMs/containers?
□ Have fork PRs ever run? (extremely high risk for self-hosted runners)
□ Do runners have network egress restrictions?
□ Could build caches leak across builds?
```

### 5. Dependency fetching security

```text
□ npm: is package-lock.json committed? No --force / --legacy-peer-deps
□ pip: is requirements.txt version-frozen? No pip install <unverified source>
□ Docker: is FROM pinned to a digest? No latest tag
□ Go: is go.sum committed?
□ Private packages: does registry auth use short-lived tokens?
```

## Automated Checking Pipeline

```yaml
# .github/workflows/supply-chain.yml
name: Supply Chain Security
on: [push, pull_request]

jobs:
  sca:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: SBOM Generate
        run: |
          npm install -g @cyclonedx/cdxgen
          cdxgen -o sbom.json
      
      - name: OSV Scan
        run: |
          go install github.com/google/osv-scanner/cmd/osv-scanner@latest
          osv-scanner scan --sbom sbom.json --format sarif > osv-results.sarif
      
      - name: Trivy Scan
        uses: aquasecurity/trivy-action@master
        with:
          scan-type: fs
          severity: CRITICAL,HIGH
          exit-code: 1
      
      - name: Secret Scan
        run: |
          docker run --rm -v $PWD:/src ghcr.io/gitleaks/gitleaks:latest \
            detect --source /src --verbose
      
      - name: Dependency-Track Upload
        run: |
          curl -X POST https://dtrack.example.com/api/v1/bom \
            -H "X-Api-Key: ${{ secrets.DTRACK_API_KEY }}" \
            -F "autoCreate=true" -F "project=myapp" -F "bom=@sbom.json"
```

Source: SLSA Framework, OWASP CI/CD Top 10, GitHub Security Lab
