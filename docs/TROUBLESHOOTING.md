# 🔧 SHURIUM Troubleshooting Guide

## Solutions to Common Problems

---

## 📖 Quick Navigation

| Problem | Jump To |
|---------|---------|
| Node won't start | [Node Issues](#-node-issues) |
| Can't connect to node | [Connection Issues](#-connection-issues) |
| Wallet problems | [Wallet Issues](#-wallet-issues) |
| Transaction issues | [Transaction Issues](#-transaction-issues) |
| Mining not working | [Mining Issues](#-mining-issues) |
| Staking problems | [Staking Issues](#-staking-issues) |
| Sync problems | [Sync Issues](#-sync-issues) |

---

## 🖥️ Node Issues

### Node won't start

**Symptoms:**
```
Error: Cannot start shuriumd
```

**Solutions:**

```bash
# 1. Check if already running
ps aux | grep shuriumd

# If running, stop it first
./shurium-cli stop
# Wait 30 seconds, then try again

# 2. Check for lock file
ls ~/.shurium/.lock
# If exists, remove it (only if node isn't running!)
rm ~/.shurium/.lock

# 3. Check disk space
df -h ~/.shurium
# Need at least 50GB free

# 4. Check permissions
ls -la ~/.shurium/
# Should be owned by your user

# 5. Try starting with debug
./shuriumd -debug=all
# Watch for error messages
```

### Node crashes immediately

**Solutions:**

```bash
# 1. Check the log
tail -100 ~/.shurium/debug.log

# 2. Try reindexing (takes time!)
./shuriumd -reindex

# 3. Check for corrupted database
./shuriumd -checkblocks=1000

# 4. Last resort: reset data (keeps wallet!)
# Backup wallet first!
cp ~/.shurium/wallet.dat ~/wallet-backup.dat
rm -rf ~/.shurium/blocks ~/.shurium/chainstate
./shuriumd  # Will resync from scratch
```

---

## 🔌 Connection Issues

### "Cannot connect to server"

**Symptoms:**
```
error: couldn't connect to server: unknown (code -1)
```

**Solution Checklist:**

```
┌─────────────────────────────────────────────────────────────────────┐
│             CONNECTION TROUBLESHOOTING CHECKLIST                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   □ Is shuriumd running?                                              │
│     → ps aux | grep shuriumd                                          │
│                                                                     │
│   □ Is RPC server enabled?                                          │
│     → Check ~/.shurium/shurium.conf has: server=1                       │
│                                                                     │
│   □ Do RPC credentials match?                                       │
│     → Check rpcuser and rpcpassword in config                       │
│     → Use same values in CLI command                                │
│                                                                     │
│   □ Is RPC port correct?                                            │
│     → Mainnet: 8332                                                 │
│     → Testnet: 18332                                                │
│     → Check rpcport in config                                       │
│                                                                     │
│   □ Are you on the right network?                                   │
│     → If node is testnet, use: --testnet in CLI                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

**Quick Fix:**

```bash
# Make sure config is correct
cat ~/.shurium/shurium.conf

# Should contain:
# server=1
# rpcuser=youruser
# rpcpassword=yourpassword

# Connect with explicit credentials
./shurium-cli -rpcuser=youruser -rpcpassword=yourpassword getblockchaininfo
```

### No peer connections

**Symptoms:**
```bash
./shurium-cli getconnectioncount
0
```

**Solutions:**

```bash
# 1. Check if listening is enabled
cat ~/.shurium/shurium.conf | grep listen
# Should be: listen=1

# 2. Add seed nodes manually
./shurium-cli addnode "seed1.shurium.io" add
./shurium-cli addnode "seed2.shurium.io" add

# 3. Check firewall
# Port 8333 (mainnet) or 18333 (testnet) must be open

# 4. Check if behind NAT
# May need port forwarding on router

# 5. Restart with DNS seeds
./shuriumd -dnsseed=1
```

---

## 💰 Wallet Issues

### "Wallet is locked"

**Symptoms:**
```
error: wallet is locked
```

**Solution:**

```bash
# Unlock for 5 minutes (300 seconds)
./shurium-cli walletpassphrase "your_password" 300

# Unlock for staking only (more secure for long-term)
./shurium-cli walletpassphrase "your_password" 0 true
```

### "Wallet is already unlocked"

This is actually fine! Your wallet is ready to use.

### Wrong password

**Symptoms:**
```
Error: The wallet passphrase entered was incorrect
```

**Solutions:**

```
┌─────────────────────────────────────────────────────────────────────┐
│                  PASSWORD RECOVERY OPTIONS                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   1. Try common variations:                                         │
│      • Did you use caps?                                            │
│      • Special characters?                                          │
│      • Extra spaces?                                                │
│                                                                     │
│   2. Check password manager                                         │
│                                                                     │
│   3. If you have 24-word recovery phrase:                           │
│      • Create new wallet from phrase                                │
│      • All funds will be accessible                                 │
│                                                                     │
│   4. If you have wallet.dat backup:                                 │
│      • You still need the password for that backup                  │
│                                                                     │
│   ⚠️ Without password AND without recovery phrase = funds lost      │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Wallet won't load

```bash
# 1. Check wallet file exists
ls -la ~/.shurium/wallet.dat

# 2. Try loading explicitly
./shuriumd -wallet=wallet.dat

# 3. Restore from backup
cp ~/backup/wallet.dat ~/.shurium/wallet.dat
./shuriumd
```

---

## 💸 Transaction Issues

### "Insufficient funds"

**Check your actual balance:**

```bash
# Total balance
./shurium-cli getbalance

# Detailed breakdown
./shurium-cli getwalletinfo

# See individual UTXOs
./shurium-cli listunspent
```

**Common causes:**

| Cause | Solution |
|-------|----------|
| Unconfirmed incoming tx | Wait for confirmations |
| Coins still in stake | Unstake first |
| Immature mining reward | Wait 100 blocks |
| Already spent (double-spend attempt) | Check tx history |

### Transaction stuck (unconfirmed)

**Check status:**

```bash
./shurium-cli gettransaction "YOUR_TX_ID"
```

**Solutions:**

```
┌─────────────────────────────────────────────────────────────────────┐
│                  STUCK TRANSACTION OPTIONS                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Option 1: WAIT                                                    │
│   • Most transactions confirm within 1 hour                         │
│   • Network might just be busy                                      │
│                                                                     │
│   Option 2: Replace-By-Fee (if enabled)                             │
│   • Send same transaction with higher fee                           │
│   • ./shurium-cli bumpfee "TX_ID"                                     │
│                                                                     │
│   Option 3: Wait for expiry                                         │
│   • Unconfirmed txs eventually drop from mempool                    │
│   • Usually after 14 days                                           │
│   • Funds return to your wallet                                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### "Fee too low"

```bash
# Set higher fee rate
./shurium-cli settxfee 0.001

# Or specify when sending
./shurium-cli sendtoaddress "address" 10 "" "" true true 6 "CONSERVATIVE"
```

### Sent to wrong address

```
┌─────────────────────────────────────────────────────────────────────┐
│              ⚠️ SENT TO WRONG ADDRESS                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Unfortunately, blockchain transactions CANNOT be reversed.        │
│                                                                     │
│   If you sent to:                                                   │
│   • Wrong address you control → Move it back                        │
│   • Someone else's address → Contact them, hope they're honest      │
│   • Invalid address → Transaction should have failed                │
│   • Burn address → Funds are gone forever                           │
│                                                                     │
│   PREVENTION:                                                       │
│   ✓ Always copy-paste addresses                                     │
│   ✓ Verify first few and last few characters                        │
│   ✓ Send small test amount first                                    │
│   ✓ Double-check before confirming                                  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## ⛏️ Mining Issues

### Mining not starting

```bash
# Check mining status
./shurium-cli getmininginfo

# Look for:
# "generate": true    ← Should be true
# "genthreads": 4     ← Should be > 0
```

**Enable mining:**

```bash
# Method 1: Config file
echo "gen=1" >> ~/.shurium/shurium.conf
echo "genthreads=4" >> ~/.shurium/shurium.conf
# Restart shuriumd

# Method 2: Runtime
./shurium-cli setgenerate true 4
```

### Zero hashrate

**Causes:**
1. Node not synced yet
2. Not enough memory
3. CPU throttling

**Solutions:**

```bash
# 1. Check sync status
./shurium-cli getblockchaininfo
# verificationprogress should be 1.0

# 2. Check system resources
top  # or htop

# 3. Reduce threads
./shurium-cli setgenerate false
./shurium-cli setgenerate true 1

# 4. Increase database cache
# Edit config: dbcache=1024
```

### Not finding any blocks

```
┌─────────────────────────────────────────────────────────────────────┐
│                 BLOCK FINDING IS RANDOM                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Finding blocks is probabilistic, not guaranteed.                  │
│                                                                     │
│   If network hashrate is 1,000,000 H/s and you have 1,000 H/s:      │
│   • Your share: 0.1%                                                │
│   • Expected blocks per day: ~2.88 × 0.001 = ~0.003                 │
│   • That's about 1 block every 3-4 days ON AVERAGE                  │
│                                                                     │
│   But randomness means:                                             │
│   • You might find 2 blocks in one day                              │
│   • Then nothing for a week                                         │
│   • This is NORMAL                                                  │
│                                                                     │
│   If you want steady income: Consider STAKING instead               │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🥩 Staking Issues

### Not earning rewards

**Check:**

```bash
# 1. Is staking enabled?
./shurium-cli getstakinginfo

# 2. Are you delegated?
./shurium-cli listdelegations

# 3. Is validator active?
./shurium-cli getvalidatorinfo "VALIDATOR_ID"
```

**Common causes:**

| Cause | Solution |
|-------|----------|
| Delegation just made | Wait one epoch (~24h) |
| Validator jailed | Choose different validator |
| Validator inactive | Redelegate to active one |
| Rewards not claimed | Run `claimrewards` |

### Validator jailed

```bash
# Check why
./shurium-cli getvalidatorinfo "YOUR_VALIDATOR_ID"
# Look for "jailed_reason"

# Fix the issue, then:
./shurium-cli unjailvalidator "YOUR_VALIDATOR_ID"
```

### Unbonding taking forever

This is **by design** - typically 21 days.

```bash
# Check remaining time
./shurium-cli listdelegations
# Look for "unbonding_completion_time"
```

---

## 🔄 Sync Issues

### Node won't sync

**Symptoms:**
- Block height stuck
- "Connecting to peers..." forever

**Solutions:**

```bash
# 1. Check peer count
./shurium-cli getconnectioncount

# 2. If 0 peers, add manually
./shurium-cli addnode "seed1.shurium.io" onetry
./shurium-cli addnode "seed2.shurium.io" onetry

# 3. Check if blockchain is corrupted
./shuriumd -checkblocks=100

# 4. Reindex blockchain
./shuriumd -reindex

# 5. Complete resync (nuclear option)
# Backup wallet first!
rm -rf ~/.shurium/blocks ~/.shurium/chainstate
./shuriumd
```

### Sync very slow

**Causes:**
- Slow internet
- Slow disk (use SSD!)
- Low memory

**Solutions:**

```bash
# Increase cache (edit config)
dbcache=2048  # 2GB, adjust based on RAM

# Use SSD for data directory
# Move ~/.shurium to SSD if on HDD
```

### "Block not found" errors

```bash
# Try reindex
./shuriumd -reindex

# If persists, resync from scratch
rm -rf ~/.shurium/blocks ~/.shurium/chainstate
./shuriumd
```

---

## 🔍 Debug Mode

### Enable detailed logging

```bash
# Start with debug
./shuriumd -debug=all

# Or specific categories
./shuriumd -debug=net -debug=rpc -debug=wallet

# View logs
tail -f ~/.shurium/debug.log
```

### Useful log commands

```bash
# Last 100 errors
grep -i error ~/.shurium/debug.log | tail -100

# Connection issues
grep -i "connection\|peer\|socket" ~/.shurium/debug.log | tail -50

# Wallet issues
grep -i wallet ~/.shurium/debug.log | tail -50
```

---

## 🆘 Getting More Help

### Gather info for bug reports

```bash
# System info
uname -a

# SHURIUM version
./shuriumd --version

# Recent logs
tail -500 ~/.shurium/debug.log > debug-snippet.txt

# Config (remove passwords!)
cat ~/.shurium/shurium.conf | grep -v password
```

### Where to get help

| Resource | Best For |
|----------|----------|
| This guide | Common issues |
| GitHub Issues | Bug reports |
| Community chat | Quick questions |
| Documentation | Learning |

---

## 📋 Quick Reference

### Emergency Commands

| Situation | Command |
|-----------|---------|
| Stop node safely | `./shurium-cli stop` |
| Force stop | `pkill shuriumd` |
| Check if running | `ps aux \| grep shuriumd` |
| View errors | `tail -100 ~/.shurium/debug.log` |
| Backup wallet | `cp ~/.shurium/wallet.dat ~/backup/` |

### Health Check Commands

| Command | What it Checks |
|---------|----------------|
| `getblockchaininfo` | Sync status |
| `getnetworkinfo` | Network connectivity |
| `getwalletinfo` | Wallet status |
| `getmininginfo` | Mining status |
| `getstakinginfo` | Staking status |
| `getconnectioncount` | Peer connections |

---

<div align="center">

**Still stuck?** Don't panic! Most issues have solutions.

Take a break, re-read the error message, and try again.

[← Back to README](../README.md)

</div>
