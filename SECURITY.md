# SHURIUM Security Policy & Bug Bounty Program

## Reporting Security Issues

**DO NOT** create public GitHub issues for security vulnerabilities.

### Responsible Disclosure

If you discover a security vulnerability in SHURIUM:

1. **Email**: security@shurium.io
2. **Subject**: [SECURITY] Brief description
3. **Include**:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

### Response Timeline

| Stage | Timeframe |
|-------|-----------|
| Initial response | 24 hours |
| Severity assessment | 48 hours |
| Fix development | 1-2 weeks (critical), 2-4 weeks (high) |
| Public disclosure | After fix deployed |

## Bug Bounty Program

### Eligibility

**Eligible:**
- Security vulnerabilities in SHURIUM core software
- Issues in consensus, cryptography, networking
- Smart contract vulnerabilities (if applicable)
- Identity/UBI system vulnerabilities
- Privacy-breaking attacks

**Not Eligible:**
- Issues already known or reported
- Social engineering attacks
- Denial of service (resource exhaustion only)
- Issues requiring physical access
- Third-party services or websites

### Reward Tiers

| Severity | Description | Reward Range |
|----------|-------------|--------------|
| **Critical** | Funds theft, consensus break, privacy breach | 10,000 - 50,000 SHR |
| **High** | Significant security risk, DoS attacks | 5,000 - 10,000 SHR |
| **Medium** | Limited impact vulnerabilities | 1,000 - 5,000 SHR |
| **Low** | Minor issues, hardening opportunities | 100 - 1,000 SHR |
| **Informational** | Best practices, documentation | Recognition only |

### Severity Definitions

#### Critical
- Remote code execution
- Unauthorized fund transfers
- Consensus failure (chain split)
- Identity system compromise (Sybil attack enabling)
- Privacy breach (identity linking)
- Cryptographic breaks

#### High
- Denial of service (network-wide)
- Significant fund manipulation
- UBI double-claiming bypass
- Marketplace reward theft
- Governance manipulation

#### Medium
- Node crashes (specific conditions)
- Transaction malleability
- Information disclosure (limited)
- Rate limiting bypasses

#### Low
- UI/UX issues with security implications
- Minor information leaks
- Documentation errors about security

## Scope

### In Scope

```
Repository: https://github.com/FidaHussain87/shurium

Critical Components:
├── src/consensus/         # Consensus rules
├── src/crypto/            # Cryptographic operations
├── src/identity/          # Identity & ZK proofs
├── src/wallet/            # Key management
├── src/economics/         # UBI distribution
├── src/marketplace/       # Compute marketplace
├── src/network/           # P2P networking
└── src/rpc/               # RPC interface
```

### Out of Scope

- Third-party dependencies (report upstream)
- Test code and examples
- Documentation websites
- Social media accounts
- Marketing materials

## Vulnerability Categories

### Consensus
- [ ] Block validation bypass
- [ ] Double-spend attacks
- [ ] Difficulty manipulation
- [ ] Timestamp manipulation
- [ ] Chain reorganization attacks

### Cryptography
- [ ] Hash collision exploitation
- [ ] Signature forgery
- [ ] Key extraction
- [ ] Randomness weaknesses
- [ ] ZK proof soundness breaks

### Identity System
- [ ] Identity forgery
- [ ] Commitment collision
- [ ] Nullifier manipulation
- [ ] Merkle tree attacks
- [ ] Privacy linking attacks

### UBI Distribution
- [ ] Double-claiming
- [ ] Sybil attacks
- [ ] Reward manipulation
- [ ] Epoch timing attacks

### Marketplace
- [ ] Solution theft
- [ ] Reward hijacking
- [ ] Problem manipulation
- [ ] Verification bypass

### Network
- [ ] Eclipse attacks
- [ ] Message manipulation
- [ ] Node fingerprinting
- [ ] Traffic analysis

### Wallet
- [ ] Key extraction
- [ ] Transaction manipulation
- [ ] Address reuse exploitation
- [ ] Backup vulnerabilities

### RPC
- [ ] Authentication bypass
- [ ] Injection attacks
- [ ] Information disclosure
- [ ] Denial of service

## Disclosure Policy

### Coordinated Disclosure

We follow responsible disclosure:

1. **Report received** - We acknowledge within 24 hours
2. **Assessment** - We evaluate severity within 48 hours
3. **Fix development** - We develop a patch
4. **Pre-disclosure** - We notify major stakeholders
5. **Patch release** - We deploy the fix
6. **Public disclosure** - We publish details after deployment

### Credit

Researchers who report valid vulnerabilities will receive:
- Public acknowledgment (if desired)
- Hall of Fame listing
- Bug bounty reward
- CVE credit (if applicable)

## Legal Safe Harbor

We will not pursue legal action against researchers who:
- Follow this disclosure policy
- Act in good faith
- Do not access user data
- Do not disrupt services
- Do not profit from vulnerabilities

## Contact

- **Security Email**: security@shurium.io
- **PGP Key**: [Available on keybase.io/shurium]
- **Response Time**: 24-48 hours

## Hall of Fame

Researchers who have helped improve SHURIUM security:

| Researcher | Contribution | Date |
|------------|--------------|------|
| (Your name here) | Be the first! | - |

---

Thank you for helping keep SHURIUM secure!
