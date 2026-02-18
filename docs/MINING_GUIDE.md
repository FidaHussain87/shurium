# ⛏️ SHURIUM Mining Guide

## Earn Coins by Helping the Network

---

## 📖 Table of Contents

1. [What is Mining?](#-what-is-mining)
2. [Should I Mine?](#-should-i-mine)
3. [Getting Started](#-getting-started)
4. [Mining Configuration](#️-mining-configuration)
5. [Proof of Useful Work](#-proof-of-useful-work)
6. [Monitoring Your Mining](#-monitoring-your-mining)
7. [Optimizing Performance](#-optimizing-performance)
8. [Troubleshooting](#-troubleshooting)

---

## 🤔 What is Mining?

### Simple Explanation

Mining is like being a **security guard + accountant** for the SHURIUM network:

```
┌─────────────────────────────────────────────────────────────────────┐
│                       WHAT MINERS DO                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   1. COLLECT    Gather pending transactions from the network        │
│        │                                                            │
│        ▼                                                            │
│   2. VERIFY     Check that all transactions are valid               │
│        │        • Sender has enough balance?                        │
│        │        • Signatures correct?                               │
│        │        • No double-spending?                               │
│        ▼                                                            │
│   3. SOLVE      Complete a computational challenge                  │
│        │        (In SHURIUM: useful real-world problems!)             │
│        │                                                            │
│        ▼                                                            │
│   4. BROADCAST  Announce new block to network                       │
│        │                                                            │
│        ▼                                                            │
│   5. REWARD     Receive newly created coins! 💰                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### How Rewards are Split

When you mine a block worth 100 SHR:

```
┌─────────────────────────────────────────────────────────────────────┐
│                   BLOCK REWARD DISTRIBUTION                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ████████████████████████████████████████  40 SHR (40%)            │
│   ⛏️ MINER - This is what YOU get!                                  │
│                                                                     │
│   ████████████████████████████             30 SHR (30%)             │
│   🎁 UBI POOL - Shared with all verified users                      │
│                                                                     │
│   ███████████████                          15 SHR (15%)             │
│   👥 CONTRIBUTORS - Human contribution rewards                      │
│                                                                     │
│   ██████████                               10 SHR (10%)             │
│   🏛️ ECOSYSTEM - Development fund                                   │
│                                                                     │
│   █████                                     5 SHR (5%)              │
│   🏦 STABILITY - Price stability reserve                            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🤷 Should I Mine?

### Mining Profitability Check

```
┌─────────────────────────────────────────────────────────────────────┐
│                    IS MINING RIGHT FOR YOU?                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Answer these questions:                                           │
│                                                                     │
│   □ Do you have a decent CPU? (4+ cores recommended)                │
│   □ Is your electricity cheap? (< $0.15/kWh ideal)                  │
│   □ Can you run your computer 24/7?                                 │
│   □ Do you have good internet (always connected)?                   │
│   □ Are you okay with higher electricity bills?                     │
│                                                                     │
│   ─────────────────────────────────────────────────────────────     │
│                                                                     │
│   If YES to all: Mining could be profitable! ✅                     │
│   If NO to some: Consider staking instead 🥩                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Mining vs Staking Comparison

| Factor | Mining ⛏️ | Staking 🥩 |
|--------|----------|----------|
| **Hardware needed** | Good CPU | Any computer |
| **Electricity cost** | High | Minimal |
| **Technical knowledge** | Medium | Low |
| **Minimum investment** | Hardware + electricity | Minimum stake |
| **Earnings** | Variable (luck-based) | Steady |
| **Risk** | Hardware failure, high bills | Slashing (if misbehave) |
| **Maintenance** | Regular monitoring | Set and forget |

---

## 🚀 Getting Started

### Prerequisites

Before mining:
1. ✅ SHURIUM is installed and running
2. ✅ Node is fully synchronized with network
3. ✅ Wallet is created

### Check Sync Status

```bash
./shurium-cli getblockchaininfo
```

Look for:
```json
{
  "blocks": 12345,
  "headers": 12345,    // Should match blocks
  "verificationprogress": 1.0  // Should be 1.0 (100%)
}
```

⚠️ **Don't mine until fully synced!** You'll waste electricity.

### Enable Mining

#### Option 1: Configuration File (Recommended)

Edit `~/.shurium/shurium.conf`:

```bash
# Mining settings
gen=1                  # Enable mining (0=off, 1=on)
genthreads=2           # Number of CPU cores to use
```

Then restart SHURIUM:
```bash
./shurium-cli stop
./shuriumd
```

#### Option 2: Command Line

```bash
./shuriumd --gen=1 --genthreads=2
```

#### Option 3: While Running (No Restart)

```bash
# Start mining with 2 threads
./shurium-cli setgenerate true 2

# Stop mining
./shurium-cli setgenerate false
```

---

## ⚙️ Mining Configuration

### How Many Threads?

```
┌─────────────────────────────────────────────────────────────────────┐
│                    THREAD CONFIGURATION GUIDE                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Your CPU Cores    Recommended Threads    Notes                    │
│   ──────────────    ───────────────────    ─────                    │
│                                                                     │
│   2 cores           1 thread               Leave 1 for system       │
│   4 cores           2-3 threads            Good balance             │
│   6 cores           4-5 threads            Strong mining            │
│   8+ cores          6-7 threads            Maximum mining           │
│                                                                     │
│   ⚠️ NEVER use ALL cores - your computer will become unusable!      │
│                                                                     │
│   Rule of thumb: Use (Total Cores - 1) or 75% of cores              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Find Your Core Count

```bash
# macOS
sysctl -n hw.ncpu

# Linux
nproc

# Windows (PowerShell)
(Get-WmiObject Win32_Processor).NumberOfCores
```

### Complete Mining Configuration

```bash
# ~/.shurium/shurium.conf

# ===================
# MINING CONFIGURATION
# ===================

# Enable mining
gen=1

# Number of mining threads (adjust based on your CPU)
genthreads=4

# Mining address (optional - defaults to wallet)
# miningaddress=shr1qyouraddress...

# ===================
# PERFORMANCE TUNING
# ===================

# Database cache (MB) - more = faster, uses more RAM
dbcache=1024

# Maximum memory pool size (MB)
maxmempool=512
```

---

## 🧠 Proof of Useful Work

### What Makes SHURIUM Mining Different

```
┌─────────────────────────────────────────────────────────────────────┐
│              TRADITIONAL MINING vs SHURIUM MINING                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   BITCOIN (Proof of Work)         SHURIUM (Proof of Useful Work)      │
│   ───────────────────────         ─────────────────────────────     │
│                                                                     │
│   🎲 Solve meaningless puzzles    🔬 Solve REAL problems            │
│                                                                     │
│   ⚡ Waste electricity            🌍 Benefit humanity                │
│                                                                     │
│   🗑️ Results thrown away          📊 Results are valuable           │
│                                                                     │
│   Examples:                        Examples:                        │
│   • Random number guessing         • Scientific calculations        │
│   • SHA256 hash grinding           • Machine learning training      │
│   • No real-world value            • Data analysis                  │
│                                    • Protein folding                │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### How PoUW Works

```
┌─────────────────────────────────────────────────────────────────────┐
│                    PROOF OF USEFUL WORK FLOW                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   1. PROBLEM SUBMISSION                                             │
│      ┌─────────────────────────────────────────────────┐            │
│      │ Scientists/Companies submit problems:           │            │
│      │ • "Calculate this protein structure"            │            │
│      │ • "Train this ML model"                         │            │
│      │ • "Analyze this dataset"                        │            │
│      └─────────────────────────────────────────────────┘            │
│                              │                                      │
│                              ▼                                      │
│   2. PROBLEM MARKETPLACE                                            │
│      ┌─────────────────────────────────────────────────┐            │
│      │ Network manages a pool of problems              │            │
│      │ Problems are divided into work units            │            │
│      └─────────────────────────────────────────────────┘            │
│                              │                                      │
│                              ▼                                      │
│   3. MINERS WORK                                                    │
│      ┌─────────────────────────────────────────────────┐            │
│      │ Miners pick up work units                       │            │
│      │ Solve them using their CPU                      │            │
│      │ Submit solutions back to network                │            │
│      └─────────────────────────────────────────────────┘            │
│                              │                                      │
│                              ▼                                      │
│   4. VERIFICATION & REWARD                                          │
│      ┌─────────────────────────────────────────────────┐            │
│      │ Network verifies solution is correct            │            │
│      │ Miner gets block reward                         │            │
│      │ Problem submitter gets their result             │            │
│      └─────────────────────────────────────────────────┘            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Interacting with PoUW

```bash
# Get current work (problem to solve)
./shurium-cli getwork

# List available problems
./shurium-cli listproblems pending

# Submit a solution
./shurium-cli submitwork "problem_id" "solution_data"
```

---

## 📊 Monitoring Your Mining

### Check Mining Status

```bash
./shurium-cli getmininginfo
```

**Output explained:**

```json
{
  "blocks": 12345,           // Current block height
  "currentblockweight": 4000, // Current block size
  "currentblocktx": 5,       // Transactions in current block
  "difficulty": 1234567.89,  // Current mining difficulty
  "networkhashps": 98765432, // Total network hash rate
  "pooledtx": 10,            // Pending transactions
  "generate": true,          // Are you mining? ✅
  "genthreads": 4,           // Mining threads in use
  "hashespersec": 12345      // Your hash rate
}
```

### Important Metrics

```
┌─────────────────────────────────────────────────────────────────────┐
│                    MINING METRICS EXPLAINED                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   METRIC              MEANING                    GOOD VALUE         │
│   ──────              ───────                    ──────────         │
│                                                                     │
│   hashespersec        Your mining speed          Higher = better    │
│                                                                     │
│   difficulty          How hard to find blocks    Adjusts auto       │
│                                                                     │
│   networkhashps       Total network power        Info only          │
│                                                                     │
│   genthreads          CPUs you're using          Your setting       │
│                                                                     │
│                                                                     │
│   YOUR SHARE OF NETWORK = hashespersec / networkhashps              │
│                                                                     │
│   Example: 10,000 / 100,000,000 = 0.0001 = 0.01% of network         │
│   (You'd find ~0.01% of all blocks)                                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Check Your Mining Rewards

```bash
# See your total balance
./shurium-cli getbalance

# See detailed balance including immature (pending) rewards
./shurium-cli getwalletinfo
```

**Balance types:**

| Type | Meaning |
|------|---------|
| `balance` | Spendable right now |
| `unconfirmed_balance` | Received, waiting for confirmations |
| `immature_balance` | Mining rewards waiting for 100 blocks |

### See Mined Blocks

```bash
# List recent transactions including mining rewards
./shurium-cli listtransactions "*" 20

# Look for "category": "generate" - those are your mining rewards!
```

---

## 🚀 Optimizing Performance

### Hardware Recommendations

```
┌─────────────────────────────────────────────────────────────────────┐
│                    HARDWARE FOR MINING                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   COMPONENT        MINIMUM        RECOMMENDED      IDEAL            │
│   ─────────        ───────        ───────────      ─────            │
│                                                                     │
│   CPU              4 cores        8 cores          16+ cores        │
│                    2 GHz          3 GHz            3.5+ GHz         │
│                                                                     │
│   RAM              4 GB           8 GB             16+ GB           │
│                                                                     │
│   Storage          HDD            SSD              NVMe SSD         │
│                    50 GB          100 GB           200+ GB          │
│                                                                     │
│   Internet         10 Mbps        50 Mbps          100+ Mbps        │
│                    (stable)       (stable)         (stable)         │
│                                                                     │
│   Power            Regular        Good PSU         Quality PSU      │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Software Optimization

```bash
# ~/.shurium/shurium.conf optimization

# Increase database cache (uses RAM, speeds up mining)
dbcache=2048  # 2 GB - adjust based on your RAM

# Increase mempool
maxmempool=1024

# Enable transaction index (helps with some operations)
txindex=1
```

### System Optimization

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SYSTEM OPTIMIZATION TIPS                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ✅ DO:                                                            │
│   • Keep system updated                                             │
│   • Close unnecessary applications                                  │
│   • Use wired internet (not WiFi)                                   │
│   • Ensure good ventilation/cooling                                 │
│   • Monitor temperatures                                            │
│   • Use SSD for blockchain data                                     │
│                                                                     │
│   ❌ DON'T:                                                         │
│   • Run on laptop (overheating risk)                                │
│   • Use all CPU cores                                               │
│   • Mine on shared/cloud computers (usually against ToS)            │
│   • Ignore high temperatures                                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Monitor System Resources

```bash
# macOS/Linux - watch CPU and memory
top

# Or more detailed
htop

# Check temperature (Linux)
sensors

# Check temperature (macOS)
sudo powermetrics --samplers smc
```

---

## ❌ Troubleshooting

### Problem: Mining not starting

**Symptoms:** `generate: false` in getmininginfo

**Solutions:**

```bash
# 1. Check if mining is enabled
./shurium-cli getmininginfo
# Look for "generate": should be true

# 2. Enable mining
./shurium-cli setgenerate true 2

# 3. Check config file has gen=1
cat ~/.shurium/shurium.conf | grep gen
```

### Problem: Zero hashrate

**Symptoms:** `hashespersec: 0`

**Solutions:**

```bash
# 1. Make sure node is synced
./shurium-cli getblockchaininfo
# verificationprogress should be 1.0

# 2. Restart mining
./shurium-cli setgenerate false
./shurium-cli setgenerate true 4

# 3. Check for errors in log
tail -100 ~/.shurium/debug.log
```

### Problem: Computer too slow

**Symptoms:** System unresponsive while mining

**Solutions:**

```bash
# Reduce mining threads
./shurium-cli setgenerate false
./shurium-cli setgenerate true 1  # Use fewer threads
```

### Problem: High electricity bill

**Solutions:**

```
┌─────────────────────────────────────────────────────────────────────┐
│                 REDUCING POWER CONSUMPTION                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   1. Mine only during off-peak hours (cheaper electricity)          │
│                                                                     │
│   2. Reduce mining threads:                                         │
│      ./shurium-cli setgenerate true 2  (instead of 4+)                │
│                                                                     │
│   3. Consider staking instead - uses minimal power                  │
│                                                                     │
│   4. Calculate profitability:                                       │
│      Daily earnings (SHR) × Price > Daily electricity cost?         │
│      If NO → mining is losing money                                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Problem: Not finding any blocks

**Reality check:**

```
┌─────────────────────────────────────────────────────────────────────┐
│                    UNDERSTANDING BLOCK FINDING                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Finding blocks is RANDOM and based on your share of network.      │
│                                                                     │
│   Example calculation:                                              │
│   • Network hashrate: 1,000,000 H/s                                 │
│   • Your hashrate: 1,000 H/s                                        │
│   • Your share: 0.1%                                                │
│   • Blocks per day: ~2,880                                          │
│   • Expected blocks for you: ~2.88 per day                          │
│                                                                     │
│   But! This is AVERAGE. You might find:                             │
│   • 0 blocks one day                                                │
│   • 5 blocks the next day                                           │
│   • 3 blocks the day after                                          │
│                                                                     │
│   Don't expect steady income - it's like lottery with better odds.  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 📋 Quick Reference

### Essential Commands

| Command | Description |
|---------|-------------|
| `setgenerate true N` | Start mining with N threads |
| `setgenerate false` | Stop mining |
| `getmininginfo` | View mining status |
| `getblocktemplate` | Get block template |
| `submitblock HEX` | Submit mined block |
| `getwork` | Get PoUW problem |
| `submitwork ID SOL` | Submit PoUW solution |

### Key Metrics

| Metric | Good Sign |
|--------|-----------|
| `hashespersec` | Higher = more chances |
| `difficulty` | Lower = easier (but changes) |
| `generate` | Should be `true` |
| `blocks` | Should increase over time |

---

<div align="center">

**Happy Mining! ⛏️**

Remember: Mining is a marathon, not a sprint. Be patient!

[← Wallet Guide](WALLET_GUIDE.md) | [Staking Guide →](STAKING_GUIDE.md)

</div>
