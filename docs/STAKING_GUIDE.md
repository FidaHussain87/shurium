# 🥩 SHURIUM Staking Guide

## Earn Passive Income From Your Holdings

---

## 📖 Table of Contents

1. [What is Staking?](#-what-is-staking)
2. [Staking vs Mining](#-staking-vs-mining)
3. [Types of Staking](#-types-of-staking)
4. [Delegating (Easy Method)](#-delegating-easy-method)
5. [Running a Validator (Advanced)](#-running-a-validator-advanced)
6. [Rewards and Earnings](#-rewards-and-earnings)
7. [Risks and Slashing](#-risks-and-slashing)
8. [Troubleshooting](#-troubleshooting)

---

## 🤔 What is Staking?

### Simple Explanation

Staking is like putting your money in a **special savings account** that:
- Helps secure the network
- Earns you interest
- Doesn't require expensive hardware

```
┌─────────────────────────────────────────────────────────────────────┐
│                      HOW STAKING WORKS                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   TRADITIONAL SAVINGS                SHURIUM STAKING                │
│   ──────────────────                ──────────────                  │
│                                                                     │
│   💰 Deposit money in bank          🔒 Lock coins as collateral     │
│         │                                  │                        │
│         ▼                                  ▼                        │
│   🏦 Bank lends your money          ✅ Network uses your stake      │
│      to others                         to validate transactions     │
│         │                                  │                        │
│         ▼                                  ▼                        │
│   📈 Bank pays you interest         💰 Network pays you rewards     │
│      (~0.5% per year)                  (~5-15% per year)            │
│                                                                     │
│   Key difference: YOU control your coins, not a bank!               │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Why Does Staking Exist?

```
┌─────────────────────────────────────────────────────────────────────┐
│                    THE PURPOSE OF STAKING                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Problem: How do we prevent bad actors from attacking the network? │
│                                                                     │
│   Solution: Require validators to put up "collateral" (stake)       │
│                                                                     │
│   • If you validate correctly → Get rewards 💰                      │
│   • If you try to cheat → Lose your stake 😱                        │
│                                                                     │
│   This makes attacking expensive and unprofitable!                  │
│                                                                     │
│   ┌─────────────────┐                                               │
│   │ 10,000 SHR      │ ← Your stake                                  │
│   │ at risk         │                                               │
│   └────────┬────────┘                                               │
│            │                                                        │
│     ┌──────┴──────┐                                                 │
│     ▼             ▼                                                 │
│   ✅ Honest    ❌ Dishonest                                         │
│   +500 SHR     -5,000 SHR                                           │
│   (reward)     (slashed!)                                           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## ⚖️ Staking vs Mining

### Comparison Table

| Factor | Mining ⛏️ | Staking 🥩 |
|--------|----------|----------|
| **What you need** | Powerful CPU | Just coins |
| **Electricity** | High | Minimal |
| **Technical skill** | Medium | Low |
| **Upfront cost** | Hardware | Coins |
| **Earnings style** | Random/lucky | Steady/predictable |
| **Risk** | Hardware failure | Slashing |
| **Environmental** | Energy intensive | Green/efficient |
| **Barrier to entry** | Hardware purchase | Minimum stake |

### When to Choose Each

```
┌─────────────────────────────────────────────────────────────────────┐
│                    CHOOSE YOUR PATH                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   CHOOSE MINING IF:                 CHOOSE STAKING IF:              │
│   ─────────────────                 ──────────────────              │
│                                                                     │
│   ✓ You have good hardware         ✓ You want passive income        │
│   ✓ Electricity is cheap           ✓ You prefer simplicity          │
│   ✓ You enjoy tinkering            ✓ You're environmentally aware   │
│   ✓ You want higher (variable)     ✓ You want steady returns        │
│     returns                         ✓ You have coins to stake       │
│                                                                     │
│   Or do BOTH! They're not mutually exclusive. 🎯                    │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 📊 Types of Staking

### Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                      STAKING OPTIONS                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   OPTION 1: DELEGATION                OPTION 2: RUN VALIDATOR       │
│   (Easy - Recommended for beginners)  (Advanced)                    │
│                                                                     │
│   • Give voting power to validator   • Run your own node 24/7       │
│   • They do the technical work       • Validate transactions        │
│   • You share in rewards             • Earn full rewards + fees     │
│   • No 24/7 uptime needed           • Need technical skills         │
│   • Lower rewards (they take cut)   • Higher rewards (keep all)     │
│   • Minimum: small amount           • Minimum: large stake          │
│                                                                     │
│   Best for: Most people             Best for: Technical users       │
│                                       with large stake              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🎯 Delegating (Easy Method)

### What is Delegation?

Instead of running your own validator, you **delegate** your coins to an existing validator. They do the technical work; you share the rewards.

```
┌─────────────────────────────────────────────────────────────────────┐
│                    HOW DELEGATION WORKS                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│        YOU                          VALIDATOR                       │
│        ───                          ─────────                       │
│                                                                     │
│   💰 1,000 SHR ─────delegate────►  🖥️ Validator Node                │
│                                         │                           │
│                                         │ Validates                 │
│                                         │ blocks                    │
│                                         ▼                           │
│                                    💵 Earns 50 SHR                  │
│                                         │                           │
│                 ◄───────────────────────┘                           │
│                                                                     │
│   📥 You receive:                                                   │
│      50 SHR × (your stake / total stake) × (1 - commission)         │
│                                                                     │
│   Example:                                                          │
│   • Your stake: 1,000 SHR                                           │
│   • Validator total: 100,000 SHR                                    │
│   • Your share: 1%                                                  │
│   • Reward: 50 × 1% × 95% = 0.475 SHR (if 5% commission)            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Step 1: Find a Validator

```bash
# List all active validators
./shurium-cli listvalidators active
```

**Output:**
```
┌──────────────┬─────────────────┬────────────┬──────────────┬────────┐
│ ID           │ Name            │ Commission │ Total Staked │ Status │
├──────────────┼─────────────────┼────────────┼──────────────┼────────┤
│ val1abc...   │ TrustNode       │ 5%         │ 500,000 SHR  │ Active │
│ val2def...   │ SecureStake     │ 8%         │ 300,000 SHR  │ Active │
│ val3ghi...   │ CommunityVal    │ 3%         │ 750,000 SHR  │ Active │
│ val4jkl...   │ ReliableNode    │ 10%        │ 200,000 SHR  │ Active │
└──────────────┴─────────────────┴────────────┴──────────────┴────────┘
```

### Choosing a Validator

```
┌─────────────────────────────────────────────────────────────────────┐
│                 HOW TO CHOOSE A VALIDATOR                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   FACTOR              LOOK FOR                    WHY               │
│   ──────              ────────                    ───               │
│                                                                     │
│   Commission          Lower is better             More rewards      │
│                       (3-10% typical)             for you           │
│                                                                     │
│   Total Staked        Medium-high                 Shows trust,      │
│                       (not too concentrated)      but avoid #1      │
│                                                                     │
│   Uptime              >99%                        Consistent        │
│                                                   rewards           │
│                                                                     │
│   History             No slashing events          Reliable          │
│                                                                     │
│   Self-stake          High                        Validator has     │
│                                                   skin in game      │
│                                                                     │
│   ⚠️ Don't put all eggs in one basket - consider multiple           │
│      validators to spread risk!                                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Step 2: Delegate Your Coins

```bash
# Delegate 1000 SHR to a validator
./shurium-cli delegate "VALIDATOR_ID" 1000
```

**Example:**
```bash
./shurium-cli delegate "val1abc123..." 1000
```

**Output:**
```
Delegation successful!
Validator: TrustNode (val1abc123...)
Amount: 1,000 SHR
Expected annual return: ~5-10%
Transaction ID: tx789xyz...
```

### Step 3: Monitor Your Delegation

```bash
# View all your delegations
./shurium-cli listdelegations
```

**Output:**
```
┌──────────────┬─────────────────┬────────────┬────────────────┐
│ Validator    │ Your Stake      │ Rewards    │ Status         │
├──────────────┼─────────────────┼────────────┼────────────────┤
│ TrustNode    │ 1,000 SHR       │ 12.5 SHR   │ Active         │
│ SecureStake  │ 500 SHR         │ 5.8 SHR    │ Active         │
└──────────────┴─────────────────┴────────────┴────────────────┘
Total Staked: 1,500 SHR
Total Pending Rewards: 18.3 SHR
```

### Step 4: Claim Your Rewards

```bash
# Claim all pending rewards
./shurium-cli claimrewards

# Claim from specific validator
./shurium-cli claimrewards "val1abc123..."
```

### Step 5: Undelegate (When You Want Out)

```bash
# Remove delegation
./shurium-cli undelegate "val1abc123..." 500
```

⚠️ **Important:** Undelegating has a **waiting period** (usually 21 days)!

```
┌─────────────────────────────────────────────────────────────────────┐
│                    UNBONDING PERIOD                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Day 0: You call undelegate                                        │
│      │   └── Coins enter "unbonding" state                          │
│      │       └── Can't use them                                     │
│      │       └── Can't earn rewards                                 │
│      ▼                                                              │
│   Day 1-20: Waiting...                                              │
│      │   └── This protects the network from attacks                 │
│      │       └── Lets slashing catch bad actors                     │
│      ▼                                                              │
│   Day 21: Coins returned to your wallet! 🎉                         │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🖥️ Running a Validator (Advanced)

### Requirements

```
┌─────────────────────────────────────────────────────────────────────┐
│                  VALIDATOR REQUIREMENTS                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   HARDWARE                                                          │
│   • CPU: 4+ cores                                                   │
│   • RAM: 8 GB minimum                                               │
│   • Storage: 100 GB SSD                                             │
│   • Internet: 100 Mbps, static IP preferred                         │
│                                                                     │
│   SOFTWARE                                                          │
│   • SHURIUM node (latest version)                                   │
│   • Linux recommended (Ubuntu 20.04+)                               │
│                                                                     │
│   STAKE                                                             │
│   • Minimum stake: Check ./shurium-cli getstakinginfo               │
│   • Recommended: 2-3x minimum for buffer                            │
│                                                                     │
│   COMMITMENT                                                        │
│   • 24/7 uptime required                                            │
│   • Quick response to issues                                        │
│   • Keep software updated                                           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Step 1: Configure Your Node

Edit `~/.shurium/shurium.conf`:

```bash
# Validator Configuration

# Enable staking
staking=1

# Your validator name (shown to delegators)
validatorname=MyAwesomeValidator

# Commission rate (500 = 5%)
commission=500

# Ensure node is publicly accessible
listen=1
maxconnections=100
```

### Step 2: Start Your Node

```bash
./shuriumd
```

Wait for full sync:
```bash
./shurium-cli getblockchaininfo
# Wait until verificationprogress = 1.0
```

### Step 3: Create Your Validator

```bash
./shurium-cli createvalidator STAKE_AMOUNT COMMISSION_BPS "MONIKER"
```

**Parameters:**
- `STAKE_AMOUNT`: How many SHR to stake
- `COMMISSION_BPS`: Commission in basis points (500 = 5%)
- `MONIKER`: Your validator's display name

**Example:**
```bash
./shurium-cli createvalidator 10000 500 "CryptoKing Validator"
```

**Output:**
```
Validator created successfully!
Validator ID: val1abc123def456...
Moniker: CryptoKing Validator
Self-stake: 10,000 SHR
Commission: 5%
Status: Pending (will activate next epoch)
```

### Step 4: Verify Your Validator

```bash
./shurium-cli getvalidatorinfo "YOUR_VALIDATOR_ID"
```

### Step 5: Monitor Performance

```bash
# Check staking status
./shurium-cli getstakinginfo

# See validator-specific info
./shurium-cli getvalidatorinfo "YOUR_VALIDATOR_ID"
```

### Validator Best Practices

```
┌─────────────────────────────────────────────────────────────────────┐
│                 VALIDATOR BEST PRACTICES                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ✅ DO:                                                            │
│   • Monitor your node 24/7 (use alerts)                             │
│   • Keep software updated                                           │
│   • Have backup infrastructure ready                                │
│   • Maintain good uptime (>99.9%)                                   │
│   • Communicate with your delegators                                │
│   • Set reasonable commission (3-10%)                               │
│                                                                     │
│   ❌ DON'T:                                                         │
│   • Run on unreliable hardware                                      │
│   • Ignore updates                                                  │
│   • Double sign (run same key on two machines)                      │
│   • Go offline frequently                                           │
│   • Set commission too high (delegators will leave)                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 💰 Rewards and Earnings

### How Rewards Work

```
┌─────────────────────────────────────────────────────────────────────┐
│                    STAKING REWARDS FLOW                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│                    Block Reward (100 SHR)                           │
│                           │                                         │
│                           ▼                                         │
│              ┌────────────────────────┐                             │
│              │  Staking Reward Pool   │                             │
│              │  (varies by config)    │                             │
│              └───────────┬────────────┘                             │
│                          │                                          │
│           ┌──────────────┼──────────────┐                           │
│           ▼              ▼              ▼                           │
│      Validator 1    Validator 2    Validator 3                      │
│      (30% stake)    (50% stake)    (20% stake)                      │
│           │              │              │                           │
│           ▼              ▼              ▼                           │
│      30% rewards    50% rewards    20% rewards                      │
│           │              │              │                           │
│    ┌──────┴──────┐      etc.          etc.                          │
│    ▼             ▼                                                  │
│  Validator    Delegators                                            │
│  (commission) (remainder)                                           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Reward Calculator

```
┌─────────────────────────────────────────────────────────────────────┐
│                 ESTIMATE YOUR EARNINGS                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Formula for delegators:                                           │
│                                                                     │
│   Annual Reward = Your Stake × Network APR × (1 - Commission)       │
│                                                                     │
│   Example:                                                          │
│   • Your stake: 10,000 SHR                                          │
│   • Network APR: 8%                                                 │
│   • Validator commission: 5%                                        │
│                                                                     │
│   Annual Reward = 10,000 × 0.08 × 0.95                              │
│                 = 760 SHR per year                                  │
│                 = ~63 SHR per month                                 │
│                 = ~2.08 SHR per day                                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Check Current APR

```bash
./shurium-cli getstakinginfo
```

Look for `expectedannualreturn` or similar field.

---

## ⚠️ Risks and Slashing

### What is Slashing?

Slashing is the **penalty** for validators who misbehave. Part of their stake (and delegators' stake!) gets destroyed.

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SLASHING CONDITIONS                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   OFFENSE             SLASH RATE    ADDITIONAL PENALTY              │
│   ───────             ──────────    ──────────────────              │
│                                                                     │
│   Double signing      5%            Tombstoned (permanent ban)      │
│   (signing 2 blocks                                                 │
│    at same height)                                                  │
│                                                                     │
│   Downtime            1%            Jailed (temporary)              │
│   (offline too long)                                                │
│                                                                     │
│   Invalid blocks      2%            Jailed                          │
│   (submitting bad                                                   │
│    transactions)                                                    │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### How Slashing Affects Delegators

```
┌─────────────────────────────────────────────────────────────────────┐
│                 ⚠️ DELEGATOR WARNING                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   If your validator gets slashed, YOU LOSE COINS TOO!               │
│                                                                     │
│   Example:                                                          │
│   • You delegated 1,000 SHR to ValidatorX                           │
│   • ValidatorX double-signs (5% slash)                              │
│   • You lose 50 SHR (5% of your delegation)                         │
│                                                                     │
│   This is why choosing a reliable validator matters!                │
│                                                                     │
│   PROTECTION STRATEGIES:                                            │
│   ✓ Research validator history                                      │
│   ✓ Spread stake across multiple validators                         │
│   ✓ Choose validators with high self-stake                          │
│   ✓ Monitor for slashing events                                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Validator Jail

```bash
# If jailed, check status
./shurium-cli getvalidatorinfo "YOUR_VALIDATOR_ID"

# Unjail (after fixing issue and waiting period)
./shurium-cli unjailvalidator "YOUR_VALIDATOR_ID"
```

---

## ❌ Troubleshooting

### Problem: Not earning rewards

**Check:**

```bash
# Is staking enabled?
./shurium-cli getstakinginfo

# Is your delegation active?
./shurium-cli listdelegations

# Is your validator active?
./shurium-cli listvalidators active
```

**Common causes:**
- Validator is jailed
- Delegation hasn't activated yet (wait one epoch)
- Not enough total stake

### Problem: Can't delegate

**Error:** "Insufficient balance"

```bash
# Check your balance
./shurium-cli getbalance

# You need coins that aren't already staked
./shurium-cli listunspent
```

### Problem: Validator keeps getting jailed

**For validators:**
- Check uptime - need >99%
- Verify server stability
- Check network connectivity
- Update to latest software

```bash
# Check your uptime
./shurium-cli getvalidatorinfo "YOUR_ID" | grep uptime

# Check for errors
tail -100 ~/.shurium/debug.log | grep -i error
```

### Problem: Long unbonding time

This is **normal**! The unbonding period is a security feature.

```bash
# Check unbonding status
./shurium-cli listdelegations

# See remaining time
# Look for "unbonding_completion_time"
```

---

## 🗳️ Staking and Governance Voting

Your staked SHR gives you **voting power** in SHURIUM's democratic governance system. The more you stake, the more influence you have on network decisions.

### How Voting Power Works

```
┌─────────────────────────────────────────────────────────────────────┐
│                    VOTING POWER CALCULATION                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Voting Power = sqrt(balance) × 1000                               │
│                                                                     │
│   This "square root" scaling prevents whales from dominating:       │
│                                                                     │
│   Balance        │ Voting Power │ Notes                             │
│   ───────────────┼──────────────┼───────────────────────────────────│
│   100 SHR        │ 10,000       │ Minimum to vote                   │
│   1,000 SHR      │ 31,623       │ 10x more coins = 3x more power    │
│   10,000 SHR     │ 100,000      │ 100x more coins = 10x more power  │
│   1,000,000 SHR  │ 1,000,000    │ 10000x more coins = 100x power    │
│                                                                     │
│   This ensures small holders still have meaningful influence!       │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Voting Requirements

| Requirement | Amount |
|-------------|--------|
| Minimum to vote | 100 SHR |
| Minimum to create proposal | 10,000 SHR |

### What Can You Vote On?

| Proposal Type | Voting Period | Approval Needed | Example |
|---------------|---------------|-----------------|---------|
| **Parameter** | ~3 days | 50% | Change block size, fees |
| **Protocol** | ~14 days | 66% | Add new features |
| **Constitutional** | ~30 days | 75% | Change UBI rate |
| **Signal** | ~3 days | 50% | Non-binding community polls |
| **Emergency** | ~1 day | 66% | Critical security fixes |

### Basic Governance Commands

```bash
# Check your voting power (based on balance)
./shurium-cli getbalance

# View current governance state
./shurium-cli getgovernanceinfo

# List active proposals
./shurium-cli listproposals "active"

# Vote on a proposal
./shurium-cli vote "PROPOSAL_ID" yes "I support this change"

# Create a proposal (requires 10,000+ SHR)
./shurium-cli createproposal signal "My Proposal" "Description here" 10000
```

### Vote Delegation

Don't have time to research every proposal? Delegate your voting power to a trusted expert:

```
┌─────────────────────────────────────────────────────────────────────┐
│                    VOTE DELEGATION                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   YOU ──────────► TRUSTED DELEGATE ──────────► VOTES FOR YOU        │
│                                                                     │
│   • Keep your coins                                                 │
│   • Delegate chooses on your behalf                                 │
│   • Revoke anytime                                                  │
│   • Different delegates for different topics                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

For complete governance documentation, see [COMMANDS.md](COMMANDS.md#️-governance-commands).

---

## 📋 Quick Reference

### Delegation Commands

| Command | Description |
|---------|-------------|
| `listvalidators active` | See available validators |
| `delegate VAL_ID AMOUNT` | Stake with a validator |
| `undelegate VAL_ID AMOUNT` | Remove stake (21 day wait) |
| `listdelegations` | See your stakes |
| `claimrewards` | Claim pending rewards |
| `getvalidatorinfo VAL_ID` | Validator details |

### Validator Commands

| Command | Description |
|---------|-------------|
| `createvalidator AMT COM NAME` | Become a validator |
| `getstakinginfo` | Your staking status |
| `unjailvalidator VAL_ID` | Exit jail |
| `editvalidator ...` | Update validator info |

### Key Terms

| Term | Meaning |
|------|---------|
| **Stake** | Coins locked as collateral |
| **Delegate** | Give voting power to validator |
| **Commission** | % validators keep from rewards |
| **Slashing** | Penalty for misbehavior |
| **Unbonding** | Waiting period when withdrawing |
| **Jailed** | Temporarily disabled validator |
| **Tombstoned** | Permanently banned validator |

---

<div align="center">

**Happy Staking! 🥩**

Set it up once, earn rewards forever.

[← Mining Guide](MINING_GUIDE.md) | [Troubleshooting →](TROUBLESHOOTING.md)

</div>
