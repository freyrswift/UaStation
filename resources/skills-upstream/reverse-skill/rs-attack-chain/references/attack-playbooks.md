# Attack Chain Playbook Quick Reference

> Pick the playbook matching the target type; each playbook defines the standard path from initial access to objective.

---

## Playbook 1: Internet-Facing Web App → Domain Controller

```
1. Subdomain enumeration + port scanning
2. Web fingerprinting → find known vulnerable components
3. Exploit the vulnerability for a webshell / RCE
4. Internal network reconnaissance (ipconfig/ifconfig, arp, net user)
5. Set up tunneling (frp/chisel/ssh)
6. Internal network scanning (live hosts, open ports)
7. Credential acquisition (mimikatz/hashdump/config files)
8. Lateral movement (PTH/WMI/PsExec)
9. Domain reconnaissance (BloodHound)
10. Domain privilege escalation (Kerberoasting/DCSync/constrained delegation)
11. Obtain domain controller access
```

**Key toolchain**: subfinder → httpx → nuclei → sqlmap/sstimap → frp → nmap → mimikatz → crackmapexec → bloodhound → certipy

---

## Playbook 2: Phishing → Internal Network Penetration

```
1. Target employee reconnaissance (LinkedIn/Maimai)
2. Craft the phishing email (spoofed sender/legitimate subject)
3. Build the payload (macro document/LNK/ISO/HTML smuggling)
4. Send the phishing email
5. Wait for the check-in (C2 beacon)
6. Local reconnaissance + privilege escalation
7. Credential extraction
8. Lateral movement
9. Persistence
10. Objective achieved
```

**Key toolchain**: theHarvester → gophish → msfvenom/cobalt-strike → mimikatz → bloodhound

---

## Playbook 3: Near-Source Penetration → Internal Network

```
1. Physical reconnaissance (WiFi signals, gate types, USB ports)
2. WiFi attack (Fluxion rogue AP / WPA cracking)
   or BadUSB implant (Rubber Ducky keyboard injection)
   or network implant (Raspberry Pi / LAN Turtle)
3. Obtain an internal network access point
4. Internal network scanning
5. Continue with steps 5-11 of Playbook 1
```

**Key toolchain**: fluxion/aircrack-ng → rubber-ducky → frp → nmap → crackmapexec

---

## Playbook 4: Cloud Environment Penetration

```
1. Cloud asset discovery (subdomains → CNAME → cloud provider)
2. Storage bucket enumeration (S3/OSS/Blob public access)
3. SSRF → cloud metadata (169.254.169.254)
4. Obtain temporary credentials (AK/SK/Token)
5. Cloud API enumeration (IAM/EC2/Lambda/RDS)
6. Privilege escalation (PassRole/AssumeRole)
7. Lateral movement (cross-account/cross-region)
8. Data acquisition
```

**Key toolchain**: subfinder → nuclei(ssrf) → aws-cli → pacu → ScoutSuite

---

## Playbook 5: Bug Bounty / SRC Rapid Initial Foothold

```
1. Asset collection (subdomains + ports + JS files)
2. Fingerprinting → rapid verification of known vulnerabilities (nuclei)
3. Parameter discovery (arjun/paramspider)
4. Test by category:
   - IDOR/broken access control (change IDs/change roles)
   - SSRF (internal network probing/cloud metadata)
   - SQL injection (sqlmap)
   - XSS (xsstrike)
   - File upload (bypass detection)
   - Logic vulnerabilities (payment/captcha/password reset)
5. Write PoCs + submit reports
```

**Key toolchain**: subfinder → httpx → nuclei → arjun → sqlmap → xsstrike → burpsuite

---

## Playbook 6: AD CS Certificate Attacks

```
1. Discover AD CS services (certipy find)
2. Identify vulnerable templates (ESC1-ESC8)
3. Request a malicious certificate
4. Authenticate as the target user with the certificate
5. Obtain the NTLM hash or a TGT
6. DCSync to export all credentials
```

**Key toolchain**: certipy → rubeus → mimikatz → secretsdump

---

## General Decision Matrix

| Current state | Next-step priority |
|---------|-------------|
| Only the target domain | Subdomain enumeration → port scanning → web fingerprinting |
| Have a web vulnerability | Get a shell → internal network reconnaissance |
| Have a low-privilege shell | Privilege escalation → credential extraction |
| Have one internal network machine | Set up tunneling → internal scanning → lateral movement |
| Have domain user credentials | BloodHound → find attack paths |
| Have a domain admin hash | DCSync → Golden Ticket |
| Have cloud AK/SK | Enumerate permissions → escalate → data acquisition |
| Phishing beacon checked in | Local privilege escalation → credentials → lateral movement |
| Near-source access | Internal network scanning → same as above |
