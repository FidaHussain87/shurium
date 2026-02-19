# SHURIUM UBI System - Technical Reference

## Production-Level Technical Documentation (EL10)

Version 1.0 | Last Updated: 2024

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [System Parameters](#system-parameters)
4. [Identity System](#identity-system)
5. [Epoch Management](#epoch-management)
6. [UBI Distribution](#ubi-distribution)
7. [Claim Process](#claim-process)
8. [Security Model](#security-model)
9. [RPC Commands Reference](#rpc-commands-reference)
10. [Configuration](#configuration)
11. [Troubleshooting](#troubleshooting)
12. [Source Code Reference](#source-code-reference)

---

## Overview

The SHURIUM Universal Basic Income (UBI) system provides automatic, privacy-preserving distribution of cryptocurrency to verified unique humans. 30% of every block reward flows to the UBI pool, which is distributed equally among all active identities at the end of each epoch.

### Key Properties

| Property | Description |
|----------|-------------|
| **Privacy-Preserving** | Zero-knowledge proofs enable anonymous claims without revealing identity |
| **Sybil-Resistant** | One identity per human via commitment-based registration |
| **Double-Claim Prevention** | Nullifier system prevents multiple claims per epoch |
| **Epoch-Based** | Distribution occurs in fixed-length epochs (configurable) |
| **Automatic Funding** | 30% of every block reward automatically accumulates in UBI pool |

### Block Reward Distribution

```
Block Reward (50 SHR initial)
├── 40% → Miner Address (20 SHR)
├── 30% → UBI Pool (15 SHR)        ← This document
├── 15% → Contribution Fund (7.5 SHR)
├── 10% → Ecosystem Fund (5 SHR)
└──  5% → Stability Reserve (2.5 SHR)
```

---

## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           SHURIUM DAEMON                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐      │
│  │   IdentityMgr   │───▶│  UBIDistributor │───▶│     Wallet      │      │
│  │                 │    │                 │    │                 │      │
│  │ - Registration  │    │ - Pool tracking │    │ - Claim signing │      │
│  │ - Tree mgmt     │    │ - Epoch finalize│    │ - Secret store  │      │
│  │ - Status update │    │ - Claim process │    │ - TX creation   │      │
│  └─────────────────┘    └─────────────────┘    └─────────────────┘      │
│           │                      │                      │               │
│           └──────────────────────┼──────────────────────┘               │
│                                  │                                      │
│                                  ▼                                      │
│                       ┌─────────────────┐                               │
│                       │  Block Callback │                               │
│                       │                 │                               │
│                       │ - Add rewards   │                               │
│                       │ - Epoch check   │                               │
│                       │ - ID activation │                               │
│                       └─────────────────┘                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Core Components

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `IdentityManager` | `src/identity/identity.cpp` | Identity registration, tree management, status tracking |
| `UBIDistributor` | `src/economics/ubi.cpp` | Pool accumulation, epoch finalization, claim processing |
| `IdentitySecrets` | Wallet storage | Private identity data (never transmitted) |
| `RewardCalculator` | `src/economics/reward.cpp` | Block reward calculations |

### Initialization Flow

```cpp
// In shuriumd.cpp - Daemon startup
IdentityManager g_identityManager(config);  // Step 1: Create identity manager
UBIDistributor g_ubiDistributor(calculator); // Step 2: Create UBI distributor

// Block callback registration
RegisterBlockCallback([](int height, Amount reward) {
    // 1. Update identity manager block context (activates pending identities)
    g_identityManager.SetBlockContext(height, timestamp);
    
    // 2. Add UBI portion to current epoch's pool
    Amount ubiAmount = reward * 30 / 100;
    g_ubiDistributor.AddBlockReward(height, ubiAmount);
    
    // 3. Check for epoch transition and finalize if needed
    EpochId newEpoch = HeightToEpoch(height);
    if (newEpoch != currentEpoch) {
        uint32_t activeCount = g_identityManager.GetActiveIdentityCount();
        g_ubiDistributor.FinalizeEpoch(previousEpoch, activeCount);
    }
});
```

---

## System Parameters

### Network-Specific Constants

| Parameter | Regtest | Testnet | Mainnet | Description |
|-----------|---------|---------|---------|-------------|
| `EPOCH_BLOCKS` | 10 | 2880 | 2880 | Blocks per epoch (~24h on mainnet) |
| `ACTIVATION_DELAY` | 10 | 100 | 100 | Blocks before identity becomes active |
| `MIN_IDENTITIES_FOR_UBI` | 1 | 10 | 100 | Minimum active identities for distribution |
| `HALVING_INTERVAL` | 1000 | 4,207,680 | 4,207,680 | Blocks between halvings |
| `INITIAL_REWARD` | 50 SHR | 50 SHR | 50 SHR | Initial block reward |

### UBI-Specific Constants

```cpp
// From include/shurium/economics/ubi.h
constexpr uint32_t MIN_IDENTITIES_FOR_UBI = 1;      // Production: 100
constexpr Amount MAX_UBI_PER_PERSON = 10000 * COIN; // Safety cap
constexpr int UBI_CLAIM_WINDOW = 2880;              // ~24 hours to claim
constexpr int UBI_GRACE_EPOCHS = 7;                 // ~1 week grace period

// From include/shurium/economics/reward.h
constexpr int EPOCH_BLOCKS = 10;                    // Production: 2880
```

### Configuration File Options

Add to `~/.shurium/shurium.conf`:

```ini
# UBI System Configuration (future expansion)
# Currently, parameters are compile-time constants

# Fund addresses (required for production)
ubiaddress=shr1qyour_ubi_pool_address...
```

---

## Identity System

### Identity Lifecycle

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        IDENTITY STATE MACHINE                            │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   createidentity                                                         │
│        │                                                                 │
│        ▼                                                                 │
│   ┌─────────┐     +N blocks      ┌─────────┐     governance    ┌───────┐ │
│   │ PENDING │ ─────────────────▶ │ ACTIVE  │ ─────────────────▶│REVOKED│ │
│   │         │                    │         │                   │       │ │
│   └─────────┘                    └─────────┘                   └───────┘ │
│        │                              │                                  │
│        │                              │ governance                       │
│        │                              ▼                                  │
│        │                        ┌───────────┐                            │
│        └───────────────────────▶│ SUSPENDED │                            │
│            governance           │           │                            │
│                                 └───────────┘                            │
│                                                                          │
│   N = ACTIVATION_DELAY (10 regtest, 100 mainnet)                         │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### Identity Status

| Status | Can Claim UBI | Description |
|--------|---------------|-------------|
| `Pending` | No | Newly created, waiting for activation delay |
| `Active` | Yes | Fully active, can claim UBI |
| `Suspended` | No | Temporarily disabled (governance action) |
| `Revoked` | No | Permanently disabled |
| `Expired` | No | Identity lifetime exceeded (if configured) |

### IdentitySecrets Structure

```cpp
// From include/shurium/identity/identity.h
struct IdentitySecrets {
    std::array<Byte, 32> masterSeed;    // Root entropy
    FieldElement secretKey;              // Identity commitment secret
    FieldElement nullifierKey;           // Nullifier derivation key
    FieldElement trapdoor;               // Additional randomness
    uint64_t treeIndex;                  // Position in identity tree
    
    // Methods
    IdentityCommitment GetCommitment() const;
    Nullifier DeriveNullifier(EpochId epoch, const FieldElement& domain) const;
};
```

**Security**: `IdentitySecrets` are stored encrypted in the wallet and NEVER transmitted over the network. Only the commitment (hash) is registered on-chain.

### Identity Registration Process

```
User                           Daemon                         Blockchain
  │                              │                                │
  │  createidentity              │                                │
  │ ────────────────────────────▶│                                │
  │                              │                                │
  │                              │ 1. Generate IdentitySecrets    │
  │                              │ 2. Compute commitment          │
  │                              │ 3. Store secrets in wallet     │
  │                              │ 4. Add to IdentityManager      │
  │                              │    (status: pending)           │
  │                              │                                │
  │  ◀───────────────────────────│                                │
  │  {identityId, status:pending}│                                │
  │                              │                                │
  │                              │ ... mine N blocks ...          │
  │                              │                                │
  │                              │ SetBlockContext(height)        │
  │                              │ ────────────────────────────▶  │
  │                              │                                │
  │                              │ Identity activated!            │
  │                              │ (status: active)               │
  │                              │                                │
```

---

## Epoch Management

### Epoch Calculation

```cpp
// From include/shurium/economics/reward.h

// Convert block height to epoch number
EpochId HeightToEpoch(int height) {
    return static_cast<EpochId>(height / EPOCH_BLOCKS);
}

// Get first block of an epoch
int EpochToHeight(EpochId epoch) {
    return static_cast<int>(epoch) * EPOCH_BLOCKS;
}

// Get last block of an epoch
int EpochEndHeight(EpochId epoch) {
    return EpochToHeight(epoch + 1) - 1;
}

// Check if this block ends an epoch
bool IsEpochEnd(int height) {
    return (height + 1) % EPOCH_BLOCKS == 0;
}
```

### Epoch Example (EPOCH_BLOCKS = 10)

```
Epoch 0: Blocks 0-9      (ends at block 9)
Epoch 1: Blocks 10-19    (ends at block 19)
Epoch 2: Blocks 20-29    (ends at block 29)
...
```

### EpochUBIPool Structure

```cpp
struct EpochUBIPool {
    EpochId epoch;                       // Epoch identifier
    Amount totalPool;                    // Accumulated UBI funds
    uint32_t eligibleCount;              // Active identities at epoch end
    Amount amountPerPerson;              // totalPool / eligibleCount
    Amount amountClaimed;                // Total claimed so far
    uint32_t claimCount;                 // Number of successful claims
    std::set<Nullifier> usedNullifiers;  // Prevent double-claims
    bool isFinalized;                    // Epoch complete?
    int endHeight;                       // Block when epoch ended
    int claimDeadline;                   // Last block to claim
};
```

### Epoch Finalization

When a new epoch begins, the previous epoch is finalized:

```cpp
void UBIDistributor::FinalizeEpoch(EpochId epoch, uint32_t identityCount) {
    EpochUBIPool& pool = GetOrCreatePool(epoch);
    
    if (identityCount < MIN_IDENTITIES_FOR_UBI) {
        // Not enough identities - pool carries over
        pool.eligibleCount = 0;
        pool.amountPerPerson = 0;
    } else {
        pool.eligibleCount = identityCount;
        pool.amountPerPerson = pool.totalPool / identityCount;
        
        // Apply safety cap
        if (pool.amountPerPerson > MAX_UBI_PER_PERSON) {
            pool.amountPerPerson = MAX_UBI_PER_PERSON;
        }
    }
    
    pool.isFinalized = true;
    pool.claimDeadline = pool.endHeight + UBI_CLAIM_WINDOW;
}
```

---

## UBI Distribution

### Pool Accumulation

For every block mined:

```cpp
// Called for each new block
void UBIDistributor::AddBlockReward(int height, Amount amount) {
    EpochId epoch = HeightToEpoch(height);
    EpochUBIPool& pool = GetOrCreatePool(epoch);
    
    pool.totalPool += amount;
    
    // Track epoch boundaries
    if (IsEpochEnd(height)) {
        pool.endHeight = height;
    }
}
```

### Distribution Formula

```
Per-Person UBI = Floor(Epoch_UBI_Pool / Active_Identities)

Where:
  Epoch_UBI_Pool = Sum(BlockReward * 30%) for all blocks in epoch
  Active_Identities = Count of identities with status=Active at epoch end
```

### Example Calculation

```
Configuration:
  - EPOCH_BLOCKS = 10
  - Initial block reward = 50 SHR
  - UBI percentage = 30%
  - Active identities = 2

UBI per block:    50 SHR * 30% = 15 SHR
Epoch UBI pool:   15 SHR * 10 blocks = 150 SHR
Per-person share: 150 SHR / 2 identities = 75 SHR per person
```

### Claim Window

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         CLAIM TIMELINE                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Epoch N              │  Claim Window  │  Grace Period │               │
│   (accumulating)       │  (claimable)   │  (extended)   │  Expired      │
│                        │                │               │               │
│   ─────────────────────┼────────────────┼───────────────┼──────────────▶│
│   Block 0           Block 10         Block 2890      Block 20170        │
│                    (epoch end)                                          │
│                                                                         │
│   EPOCH_BLOCKS=10                                                       │
│   UBI_CLAIM_WINDOW=2880 blocks (~24 hours)                              │
│   UBI_GRACE_EPOCHS=7 epochs (~7 days)                                   │
│                                                                         │
│   NOTE: Claims are from PREVIOUS epoch. Current epoch is accumulating.  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Claim Process

### Claim Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          UBI CLAIM PROCESS                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. User calls `claimubi "identity_id"`                                │
│      │                                                                  │
│      ▼                                                                  │
│   2. Wallet retrieves IdentitySecrets for identity_id                   │
│      │                                                                  │
│      ▼                                                                  │
│   3. Determine claimable epoch (previous finalized epoch)               │
│      │                                                                  │
│      ▼                                                                  │
│   4. Derive nullifier: Hash(nullifierKey, epoch, "UBI")                 │
│      │                                                                  │
│      ▼                                                                  │
│   5. Check: Is nullifier already used? → REJECT (DoubleClaim)           │
│      │                                                                  │
│      ▼                                                                  │
│   6. Verify: Is identity active? → REJECT if not                        │
│      │                                                                  │
│      ▼                                                                  │
│   7. Generate ZK proof of identity membership                           │
│      │                                                                  │
│      ▼                                                                  │
│   8. Process claim: Record nullifier, calculate amount                  │
│      │                                                                  │
│      ▼                                                                  │
│   9. Return: {success: true, amount: X, epoch: N}                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Claim Status Codes

```cpp
enum class ClaimStatus {
    Pending,           // Claim submitted, awaiting verification
    Valid,             // Claim approved and processed
    InvalidProof,      // ZK proof failed verification
    DoubleClaim,       // Nullifier already used this epoch
    IdentityNotFound,  // Identity not in tree
    EpochExpired,      // Claim window closed
    EpochNotComplete,  // Epoch still accumulating
    PoolEmpty          // No funds in pool
};
```

### Double-Claim Prevention

The nullifier system ensures one claim per identity per epoch:

```cpp
Nullifier IdentitySecrets::DeriveNullifier(EpochId epoch, const FieldElement& domain) {
    // Deterministic: same inputs always produce same nullifier
    return Hash(nullifierKey, epoch, domain);
}

// In UBIDistributor::ProcessClaim()
if (pool.usedNullifiers.count(claim.nullifier) > 0) {
    return ClaimStatus::DoubleClaim;  // Already claimed!
}
pool.usedNullifiers.insert(claim.nullifier);  // Record for future checks
```

---

## Security Model

### Zero-Knowledge Properties

| Property | Guarantee |
|----------|-----------|
| **Anonymity** | Claims cannot be linked to specific identities |
| **Uniqueness** | Each identity can claim exactly once per epoch |
| **Membership** | Only registered identities can claim |
| **Non-transferable** | Identity secrets cannot be sold (no value without secrets) |

### Threat Model

| Threat | Mitigation |
|--------|------------|
| Sybil attack (fake identities) | Registration requires verification |
| Double claiming | Nullifier uniqueness per epoch |
| Identity theft | Secrets never leave user device |
| Claim linkability | Zero-knowledge proofs hide identity |
| Pool manipulation | Automatic funding from block rewards |

### Key Security Practices

1. **Never share IdentitySecrets** - These control your UBI claims
2. **Backup wallet securely** - Identity secrets are stored in wallet
3. **Use encrypted wallet** - Protect secrets at rest
4. **Verify daemon authenticity** - Only use official releases

---

## RPC Commands Reference

### createidentity

Creates a new identity for UBI participation.

**Synopsis:**
```bash
shurium-cli createidentity
```

**Parameters:** None

**Returns:**
```json
{
  "identityId": "fcb78ec755251b5e6b59c7e2a85e36921234abcd...",
  "message": "Identity created successfully. It will become active after activation delay.",
  "registrationHeight": 150,
  "status": "pending",
  "treeIndex": 3
}
```

**Notes:**
- One identity per wallet
- Identity starts as `pending`
- Becomes `active` after ACTIVATION_DELAY blocks
- Secrets automatically stored in wallet

---

### getidentityinfo

Retrieves detailed information about an identity.

**Synopsis:**
```bash
shurium-cli getidentityinfo "identity_id"
```

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| identity_id | string | Yes | The identity ID (hex) |

**Returns:**
```json
{
  "identityId": "fcb78ec755251b5e6b59c7e2a85e36921234abcd...",
  "identityStatus": "active",
  "registrationHeight": 150,
  "treeIndex": 3,
  "canClaimUBI": true,
  "verified": true
}
```

---

### claimubi

Claims UBI for the previous finalized epoch.

**Synopsis:**
```bash
shurium-cli claimubi "identity_id" [recipient_address]
```

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| identity_id | string | Yes | The identity ID (hex) |
| recipient_address | string | No | Address to receive UBI (default: new wallet address) |

**Returns (Success):**
```json
{
  "success": true,
  "identityId": "fcb78ec755251b5e...",
  "epoch": 5,
  "amount": 75.00000000,
  "claimStatus": "Valid",
  "message": "UBI claimed successfully"
}
```

**Returns (Double Claim):**
```json
{
  "success": false,
  "identityId": "fcb78ec755251b5e...",
  "epoch": 5,
  "amount": 0,
  "claimStatus": "DoubleClaim",
  "message": "Already claimed UBI for this epoch"
}
```

**Returns (Not Active):**
```json
{
  "success": false,
  "message": "Identity is not active (current status: pending)"
}
```

**Notes:**
- Claims from PREVIOUS finalized epoch (not current)
- Requires identity to be `active`
- One claim per identity per epoch
- Amount depends on pool size and identity count

---

### getubiinfo

Returns UBI distribution statistics for an identity.

**Synopsis:**
```bash
shurium-cli getubiinfo "identity_id"
```

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| identity_id | string | Yes | The identity ID (hex) |

**Returns:**
```json
{
  "identityStatus": "active",
  "eligible": true,
  "currentEpoch": 7,
  "poolTotal": 150.00000000,
  "amountPerPerson": 75.00000000,
  "activeIdentities": 2,
  "totalDistributed": 450.00000000,
  "totalClaimsAllTime": 6,
  "averageClaimRate": 85.5
}
```

---

## Configuration

### Recommended Production Settings

For `~/.shurium/shurium.conf`:

```ini
# ============================================================================
# SHURIUM Production Configuration
# ============================================================================

# Network (remove for mainnet)
# testnet=1

# RPC Configuration
rpcuser=your_secure_username
rpcpassword=your_secure_password
rpcallowip=127.0.0.1

# Fund Addresses (REQUIRED for production)
# Generate your own addresses - DO NOT use defaults!
ubiaddress=shr1qyour_organization_ubi_address...
contributionaddress=shr1qyour_organization_contrib_address...
ecosystemaddress=shr1qyour_organization_eco_address...
stabilityaddress=shr1qyour_organization_stability_address...

# Mining (optional)
# gen=1
# genthreads=4
# miningaddress=shr1qyour_mining_address...
```

### Network-Specific Data Directories

| Network | Data Directory | RPC Port |
|---------|----------------|----------|
| Mainnet | `~/.shurium/` | 8332 |
| Testnet | `~/.shurium/testnet/` | 18332 |
| Regtest | `~/.shurium/regtest/` | 18443 |

---

## Troubleshooting

### Common Issues

#### "Identity is not active"

**Cause:** Identity hasn't passed activation delay.

**Solution:**
```bash
# Check current block height
./shurium-cli getblockcount

# Check identity registration height
./shurium-cli getidentityinfo "your_identity_id"

# Wait for: currentHeight - registrationHeight >= ACTIVATION_DELAY
# Regtest: 10 blocks, Mainnet: 100 blocks
```

#### "Already claimed UBI for this epoch"

**Cause:** You've already claimed for this epoch.

**Solution:** Wait for next epoch to start. Each epoch is EPOCH_BLOCKS (10 regtest, 2880 mainnet).

#### "Epoch not complete"

**Cause:** Attempting to claim from current (still-accumulating) epoch.

**Solution:** Claims are always from the PREVIOUS finalized epoch. Mine more blocks to advance epochs.

#### "No identity secrets found"

**Cause:** Identity was created on different wallet or secrets lost.

**Solution:** 
- Ensure you're using the same wallet that created the identity
- Restore wallet from backup if needed
- Create new identity if secrets are lost

#### "Pool empty" or "Not enough identities"

**Cause:** MIN_IDENTITIES_FOR_UBI not met.

**Solution:**
- Regtest: Only 1 identity needed (MIN_IDENTITIES_FOR_UBI=1)
- Mainnet: 100 identities needed
- Create more identities or wait for more users

### Debug Logging

Enable debug logging for UBI system:

```bash
# View daemon logs
tail -f ~/.shurium/regtest/debug.log | grep -i "ubi\|identity\|epoch"
```

### Manual Verification

```bash
# 1. Check identity status
./shurium-cli getidentityinfo "identity_id"

# 2. Check UBI pool info
./shurium-cli getubiinfo "identity_id"

# 3. Verify epoch calculation
# Current epoch = floor(block_height / EPOCH_BLOCKS)
./shurium-cli getblockcount
# If height=35, epoch=3 (35/10=3)

# 4. Check fund balances
./shurium-cli getfundbalance ubi
```

---

## Source Code Reference

### Core Files

| File | Purpose |
|------|---------|
| `include/shurium/economics/ubi.h` | UBI constants, ClaimStatus, UBIDistributor class |
| `include/shurium/economics/reward.h` | EPOCH_BLOCKS, RewardCalculator, epoch utilities |
| `include/shurium/identity/identity.h` | IdentityManager, IdentitySecrets, IdentityRecord |
| `src/economics/ubi.cpp` | UBIDistributor implementation |
| `src/identity/identity.cpp` | IdentityManager implementation |
| `src/wallet/wallet.cpp` | RegisterIdentity, CreateUBIClaim |
| `src/shuriumd.cpp` | System initialization, block callbacks |
| `src/rpc/commands.cpp` | RPC command implementations |

### Key Functions

```cpp
// Identity creation
Wallet::RegisterIdentity()                    // src/wallet/wallet.cpp

// Identity activation (on each block)
IdentityManager::SetBlockContext(height, ts)  // src/identity/identity.cpp

// Pool accumulation (on each block)  
UBIDistributor::AddBlockReward(height, amt)   // src/economics/ubi.cpp

// Epoch finalization (on epoch boundary)
UBIDistributor::FinalizeEpoch(epoch, count)   // src/economics/ubi.cpp

// Claim processing
UBIDistributor::ProcessClaim(claim, root, h)  // src/economics/ubi.cpp

// RPC handlers
cmd_createidentity(params)                    // src/rpc/commands.cpp:~4027
cmd_claimubi(params)                          // src/rpc/commands.cpp:~4180
cmd_getubiinfo(params)                        // src/rpc/commands.cpp:~4333
```

### Important Line Numbers

| Function | File | Approximate Line |
|----------|------|------------------|
| `cmd_createidentity` | src/rpc/commands.cpp | ~4027 |
| `cmd_claimubi` | src/rpc/commands.cpp | ~4180 |
| `cmd_getubiinfo` | src/rpc/commands.cpp | ~4333 |
| `EPOCH_BLOCKS` definition | include/shurium/economics/reward.h | ~152 |
| `MIN_IDENTITIES_FOR_UBI` | include/shurium/economics/ubi.h | ~47 |

---

## Appendix: Testing Workflow

### Complete Regtest UBI Test

```bash
# 1. Start fresh regtest
pkill -f shuriumd
rm -rf ~/.shurium/regtest
./shuriumd --regtest --daemon

# 2. Create wallet and get address
./shurium-cli --regtest createwallet "test"
ADDR=$(./shurium-cli --regtest getnewaddress)

# 3. Create identity
./shurium-cli --regtest createidentity
# Note the identityId

# 4. Mine past activation delay (10 blocks)
./shurium-cli --regtest generatetoaddress 15 $ADDR

# 5. Verify identity is active
./shurium-cli --regtest getidentityinfo "YOUR_IDENTITY_ID"
# Should show: "identityStatus": "active"

# 6. Mine to complete epoch 1 and start epoch 2
./shurium-cli --regtest generatetoaddress 10 $ADDR
# Now at block 25, epoch 2

# 7. Claim UBI for epoch 1
./shurium-cli --regtest claimubi "YOUR_IDENTITY_ID"
# Should succeed with amount > 0

# 8. Try to claim again (should fail)
./shurium-cli --regtest claimubi "YOUR_IDENTITY_ID"
# Should fail: DoubleClaim

# 9. Check UBI stats
./shurium-cli --regtest getubiinfo "YOUR_IDENTITY_ID"

# 10. Mine next epoch and claim again
./shurium-cli --regtest generatetoaddress 10 $ADDR
./shurium-cli --regtest claimubi "YOUR_IDENTITY_ID"
# Should succeed for new epoch
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2024 | Initial release with complete UBI system |

---

**Related Documentation:**
- [UBI Explained (User Guide)](UBI_EXPLAINED.md)
- [Commands Reference](COMMANDS.md)
- [Whitepaper](WHITEPAPER.md)
- [Deployment Guide](DEPLOYMENT_GUIDE.md)

