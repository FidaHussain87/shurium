# 🚀 SHURIUM Quick Start Guide

## Get Running in 10 Minutes

This guide takes you from zero to a working SHURIUM wallet.

---

## 📋 Before You Begin

### What You'll Need

| Item | Why |
|------|-----|
| Computer | Mac, Windows, or Linux |
| 30 minutes | For first-time setup |
| Internet connection | To sync with the network |
| Pen and paper | To write down your recovery phrase |

### What You'll Have After

- ✅ SHURIUM node running
- ✅ Your own wallet with addresses
- ✅ Ability to send and receive SHR
- ✅ Ready to mine or stake

---

## Step 1: Install Dependencies

### 🍎 macOS

Open Terminal and run:

```bash
# Install Homebrew (if you don't have it)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required tools
brew install cmake git openssl
```

### 🐧 Linux (Ubuntu/Debian)

Open Terminal and run:

```bash
sudo apt update
sudo apt install -y cmake build-essential git libssl-dev
```

### 🪟 Windows

1. Download and install [Visual Studio](https://visualstudio.microsoft.com/) with "Desktop development with C++"
2. Download and install [CMake](https://cmake.org/download/)
3. Download and install [Git](https://git-scm.com/download/win)

---

## Step 2: Download SHURIUM

```bash
# Clone the repository
git clone https://github.com/shurium/shurium.git

# Enter the directory
cd shurium
```

**What you downloaded:**

```
shurium/
├── src/           ← Source code
├── include/       ← Header files
├── tests/         ← Test files
├── docs/          ← Documentation (you're reading it!)
└── CMakeLists.txt ← Build instructions
```

---

## Step 3: Build SHURIUM

```bash
# Create a build directory
mkdir build
cd build

# Configure the build
cmake ..

# Compile (takes 5-15 minutes)
cmake --build .
```

**What gets created:**

| File | Purpose |
|------|---------|
| `shuriumd` | The main SHURIUM program (your node) |
| `shurium-cli` | Tool to control your node |
| `shurium-wallet-tool` | Offline wallet manager |

### Verify It Worked

```bash
./shuriumd --version
```

You should see:
```
SHURIUM Core version v0.1.0-genesis
```

---

## Step 4: Configure Your Node

### Create the Data Directory

```bash
# Go back to home directory
cd ~

# Create SHURIUM data folder
mkdir -p .shurium
```

### Create Configuration File

```bash
cat > ~/.shurium/shurium.conf << 'EOF'
# =========================================
#        SHURIUM CONFIGURATION FILE
# =========================================

# NETWORK
# -------
# testnet=1  means practice mode (fake money)
# testnet=0  means real mode (real money!)
testnet=1

# RPC SERVER
# ----------
# This lets you control SHURIUM from command line
server=1
rpcuser=shuriumuser
rpcpassword=CHANGE_THIS_PASSWORD_123

# CONNECTIONS
# -----------
listen=1
maxconnections=50

# FEATURES
# --------
# Mining (0=off, 1=on)
gen=0

# Staking (0=off, 1=on)  
staking=0

EOF
```

⚠️ **Important:** Change `rpcpassword` to something unique!

---

## Step 5: Start Your Node

### Understanding the Two Programs

SHURIUM has **two main programs** that work together:

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│   shuriumd (Daemon)         shurium-cli (Client)                        │
│   ================        ==================                        │
│                                                                     │
│   - Runs in background    - Sends commands to daemon                │
│   - Manages blockchain    - Shows you information                   │
│   - Handles your wallet   - Used to send/receive money              │
│   - Connects to network   - Can be run anytime                      │
│                                                                     │
│   START ONCE              USE ANYTIME                               │
│   (keeps running)         (run commands as needed)                  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

**Think of it like this:**
- `shuriumd` = Your bank (runs 24/7, handles everything)
- `shurium-cli` = Your banking app (use it when you need to check balance, send money, etc.)

---

### Option A: Run in Foreground (Good for Learning)

This keeps the daemon visible so you can see what's happening:

```bash
# Go to build directory
cd ~/shurium/build

# Start SHURIUM on testnet
./shuriumd --testnet
```

**What you'll see:**

```
SHURIUM Daemon v0.1.0 starting...
Data directory: ~/.shurium/testnet
Network: testnet
Opening block database...
Loading wallet...
RPC server listening on 127.0.0.1:18332
P2P network started, max connections: 125
SHURIUM Daemon started successfully
```

**Problem:** You need to keep this terminal open. If you close it, SHURIUM stops.

**Solution:** Open a **new terminal window** for running commands.

---

### Option B: Run in Background (Recommended)

This lets SHURIUM run even after you close the terminal:

```bash
# Go to build directory
cd ~/shurium/build

# Start SHURIUM in background (daemon keeps running after terminal closes)
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Check it's running
ps aux | grep shuriumd | grep -v grep
```

**What this means:**
- `nohup` = "No hang up" - keeps running even if terminal closes
- `> /dev/null 2>&1` = Don't show output in terminal
- `&` = Run in background

**You should see something like:**
```
yourname   12345   0.0  0.1  123456  7890  ??  S  10:30AM  0:00.05 ./shuriumd --testnet
```

This means SHURIUM is running. You can close the terminal and it will keep running.

---

### Managing Your Daemon

Here are the essential commands you'll need:

| What You Want | Command |
|---------------|---------|
| Start daemon (background) | `nohup ./shuriumd --testnet > /dev/null 2>&1 &` |
| Check if running | `ps aux \| grep shuriumd \| grep -v grep` |
| Stop daemon (graceful) | `./shurium-cli --testnet stop` |
| Stop daemon (force) | `pkill -f shuriumd` |
| View logs | `tail -f ~/.shurium/testnet/debug.log` |
| View recent logs | `tail -50 ~/.shurium/testnet/debug.log` |

---

### Important Ports

| Port | Purpose | Network |
|------|---------|---------|
| **18332** | RPC (commands) | Testnet |
| **18333** | P2P (other nodes) | Testnet |
| **8332** | RPC (commands) | Mainnet |
| **8333** | P2P (other nodes) | Mainnet |

The daemon automatically starts the RPC server - you don't need to run anything else!

---

### Verify It's Working

After starting the daemon, test it:

```bash
# Check blockchain status
./shurium-cli --testnet getblockchaininfo

# Check network status  
./shurium-cli --testnet getnetworkinfo

# Check wallet status
./shurium-cli --testnet getwalletinfo
```

If you see JSON output, everything is working.

---

## Step 6: Create Your Wallet

In your **new terminal**:

```bash
cd ~/shurium/build

# Create your first receiving address
./shurium-cli --testnet getnewaddress "My First Wallet"
```

**Output:**
```
tshr1q8w4j5k6n2m3v4c5x6z7a8s9d0f1g2h3j4k5l6
```

🎉 **This is YOUR address!** It's like an email address for money.

### Understand Your Address

```
tshr1q8w4j5k6n2m3v4c5x6z7a8s9d0f1g2h3j4k5l6
│││
││└── The unique part (your specific address)
│└─── Address type (q = native segwit)
└──── Network (t = testnet, n = mainnet)
```

---

## Step 7: Secure Your Wallet

### Encrypt Your Wallet

```bash
./shurium-cli --testnet encryptwallet "your_strong_password_here"
```

⚠️ **Warning:** SHURIUM will restart after this. That's normal!

### Create a Backup

```bash
./shurium-cli --testnet backupwallet ~/shurium-wallet-backup.dat
```

---

## Step 8: Check Everything Works

### See Your Balance

```bash
./shurium-cli --testnet getbalance
```

Output: `0.00000000` (you start with zero!)

### See Wallet Info

```bash
./shurium-cli --testnet getwalletinfo
```

### See Network Status

```bash
./shurium-cli --testnet getnetworkinfo
```

### See Blockchain Status

```bash
./shurium-cli --testnet getblockchaininfo
```

---

## 🎉 Congratulations!

You now have a working SHURIUM node and wallet!

### What's Next?

| Goal | Guide |
|------|-------|
| Send/receive money | Continue below ↓ |
| Earn by mining | [Mining Guide](MINING_GUIDE.md) |
| Earn passive income | [Staking Guide](STAKING_GUIDE.md) |
| Secure your wallet | [Wallet Guide](WALLET_GUIDE.md) |

---

## Basic Operations

### Generate More Addresses

```bash
# Address for rent payments
./shurium-cli --testnet getnewaddress "rent"

# Address for savings
./shurium-cli --testnet getnewaddress "savings"

# List all addresses
./shurium-cli --testnet listaddresses
```

### Receive Money

1. Share your address with the sender
2. Wait for them to send
3. Check your balance:

```bash
./shurium-cli --testnet getbalance
```

### Send Money

```bash
./shurium-cli --testnet sendtoaddress "RECIPIENT_ADDRESS" AMOUNT
```

**Example:**
```bash
./shurium-cli --testnet sendtoaddress "tshr1qrecipient..." 10.5
```

### Check Transactions

```bash
# Recent transactions
./shurium-cli --testnet listtransactions

# Specific transaction
./shurium-cli --testnet gettransaction "TRANSACTION_ID"
```

---

## Common Commands Cheat Sheet

| What You Want | Command |
|---------------|---------|
| Check balance | `./shurium-cli --testnet getbalance` |
| New address | `./shurium-cli --testnet getnewaddress "label"` |
| Send coins | `./shurium-cli --testnet sendtoaddress "addr" amount` |
| Transaction history | `./shurium-cli --testnet listtransactions` |
| Unlock wallet | `./shurium-cli --testnet walletpassphrase "pass" 300` |
| Lock wallet | `./shurium-cli --testnet walletlock` |
| Stop node | `./shurium-cli --testnet stop` |
| Start daemon (background) | `nohup ./shuriumd --testnet > /dev/null 2>&1 &` |
| Check if daemon running | `ps aux \| grep shuriumd \| grep -v grep` |
| View logs | `tail -f ~/.shurium/testnet/debug.log` |

---

## Troubleshooting

### "Cannot connect to server" or "Connection refused"

**Problem:** The daemon (`shuriumd`) is not running.

**Solution:** Start the daemon first:

```bash
# Check if daemon is running
ps aux | grep shuriumd | grep -v grep

# If nothing shows up, start it:
cd ~/shurium/build
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Wait 2-3 seconds, then try your command again
sleep 3
./shurium-cli --testnet getbalance
```

### "Database lock" or "Resource temporarily unavailable"

**Problem:** A previous daemon didn't shut down properly.

**Solution:** Kill old processes and clean up:

```bash
# Kill any running daemon
pkill -f shuriumd

# Wait a moment
sleep 2

# Remove stale lock file
rm -f ~/.shurium/testnet/blocks/blocks/index/LOCK

# Start daemon again
nohup ./shuriumd --testnet > /dev/null 2>&1 &
```

### "Wallet is locked"

**Problem:** Your wallet is encrypted and locked.

**Solution:** Unlock it:
```bash
# Unlock for 5 minutes (300 seconds)
./shurium-cli --testnet walletpassphrase "your_password" 300
```

### Node won't sync (0 connections)

**Problem:** Can't find other nodes on the network.

**Solution:**
```bash
# Check connection count
./shurium-cli --testnet getconnectioncount

# If 0, the network might be new or seeds aren't available
# This is normal for testnet - your node still works locally
```

### How to View Logs

Logs help you understand what's happening:

```bash
# See last 50 lines of log
tail -50 ~/.shurium/testnet/debug.log

# Watch logs in real-time (press Ctrl+C to stop)
tail -f ~/.shurium/testnet/debug.log

# Search for errors
grep -i error ~/.shurium/testnet/debug.log
```

### How to Completely Reset (Start Fresh)

If something is really broken:

```bash
# Stop daemon
pkill -f shuriumd

# WARNING: This deletes everything including your wallet!
# Only do this if you haven't received any real money!
rm -rf ~/.shurium/testnet

# Start fresh
./shuriumd --testnet
```

---

## Restore Your Wallet

If you lost your wallet but have your **24-word recovery phrase** or a **backup file**, you can restore it.

### Option 1: Restore from Recovery Phrase (24 words)

**For Mainnet (Real Money):**

```bash
# Step 1: Stop daemon
./shurium-cli stop
pkill -f shuriumd

# Step 2: Move old wallet out of the way
mv ~/.shurium/wallet.dat ~/.shurium/wallet.dat.old

# Step 3: Restore using your 24 words
cd ~/shurium/build
./shurium-wallet-tool import --wallet=~/.shurium/wallet.dat

# When prompted:
#   - Enter your 24 words (separated by SPACES, not commas)
#   - BIP39 Passphrase: Press Enter if you didn't use one
#   - Wallet Password: Enter a new password to encrypt the wallet

# Step 4: Start daemon
nohup ./shuriumd > /dev/null 2>&1 &

# Step 5: Verify
./shurium-cli listaddresses
```

**For Testnet (Fake Money):**

```bash
# Step 1: Stop daemon
./shurium-cli --testnet stop
pkill -f shuriumd

# Step 2: Move old wallet out of the way
mv ~/.shurium/testnet/wallet.dat ~/.shurium/testnet/wallet.dat.old

# Step 3: Restore using your 24 words
cd ~/shurium/build
./shurium-wallet-tool import --testnet --wallet=~/.shurium/testnet/wallet.dat

# When prompted:
#   - Enter your 24 words (separated by SPACES, not commas)
#   - BIP39 Passphrase: Press Enter if you didn't use one
#   - Wallet Password: Enter a new password to encrypt the wallet

# Step 4: Start daemon
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Step 5: Verify
./shurium-cli --testnet listaddresses
```

### Option 2: Restore from wallet.dat Backup

**For Mainnet (Real Money):**

```bash
# Step 1: Stop daemon
./shurium-cli stop

# Step 2: Replace with your backup
cp /path/to/backup/wallet.dat ~/.shurium/wallet.dat

# Step 3: Start daemon
nohup ./shuriumd > /dev/null 2>&1 &

# Step 4: Verify
./shurium-cli listaddresses
```

**For Testnet (Fake Money):**

```bash
# Step 1: Stop daemon
./shurium-cli --testnet stop

# Step 2: Replace with your backup
cp /path/to/backup/wallet.dat ~/.shurium/testnet/wallet.dat

# Step 3: Start daemon
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Step 4: Verify
./shurium-cli --testnet listaddresses
```

### Important Notes About Recovery

| If You Have | You Can Recover |
|-------------|-----------------|
| 24-word phrase | All addresses and funds |
| wallet.dat backup | Everything including labels |
| Only password | Nothing (password alone is useless) |

**Recovery Phrase Format:**
- Enter words separated by **SPACES** (not commas)
- Example: `word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12`

**BIP39 Passphrase vs Wallet Password:**
- **BIP39 Passphrase**: Used during wallet creation - affects which addresses are generated
- **Wallet Password**: Encrypts your wallet file - can be changed anytime

If your recovered address doesn't match, you may have used a BIP39 passphrase when creating the original wallet.

For more details, see [Wallet Guide - Backup and Recovery](WALLET_GUIDE.md#-backup-and-recovery)

---

## Testnet vs Mainnet (IMPORTANT!)

### What's the Difference?

```
┌─────────────────────────────────────────────────────────────────────┐
│                    TESTNET vs MAINNET                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   TESTNET (--testnet flag)          MAINNET (no flag)               │
│   ========================          =================               │
│                                                                     │
│   - FAKE money (worthless)          - REAL money (valuable)         │
│   - For learning & testing          - For actual transactions       │
│   - Zero risk                       - Real risk                     │
│   - Separate wallet                 - Separate wallet               │
│                                                                     │
│   Command:                          Command:                        │
│   ./shuriumd --testnet                ./shuriumd                        │
│   ./shurium-cli --testnet ...         ./shurium-cli ...                 │
│                                                                     │
│   Wallet location:                  Wallet location:                │
│   ~/.shurium/testnet/wallet.dat       ~/.shurium/wallet.dat             │
│                                                                     │
│   Address prefix: tshr1...          Address prefix: shr1...         │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Which Should You Use?

| Your Goal | Use | Command |
|-----------|-----|---------|
| Learning how SHURIUM works | Testnet | `./shuriumd --testnet` |
| Testing before real transactions | Testnet | `./shuriumd --testnet` |
| Developing applications | Testnet | `./shuriumd --testnet` |
| **Real transactions with real money** | **Mainnet** | `./shuriumd` |
| **Actually using SHURIUM** | **Mainnet** | `./shuriumd` |

### Starting Mainnet (Real Money)

```bash
cd ~/shurium/build

# Start mainnet daemon (NO --testnet flag!)
nohup ./shuriumd > /dev/null 2>&1 &

# Use CLI commands (NO --testnet flag!)
./shurium-cli getbalance
./shurium-cli getnewaddress "my wallet"
./shurium-cli getwalletinfo
```

### Starting Testnet (Fake Money for Practice)

```bash
cd ~/shurium/build

# Start testnet daemon
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Use CLI commands with --testnet
./shurium-cli --testnet getbalance
./shurium-cli --testnet getnewaddress "test wallet"
```

### Important: They Are Completely Separate!

- Testnet and Mainnet have **different wallets**
- Testnet and Mainnet have **different addresses**
- Testnet coins have **no value**
- You cannot send testnet coins to mainnet or vice versa

### Common Mistake

If you created a wallet without `--testnet`, your wallet is on **mainnet**:
```
~/.shurium/wallet.dat              <-- Your mainnet wallet
```

If you then run commands with `--testnet`, you're using a **different wallet**:
```
~/.shurium/testnet/wallet.dat      <-- Different testnet wallet
```

**Solution:** Be consistent! Always use the same network.

---

## Network Reference

| Network | Flag | Money | Wallet Location | Address Prefix | RPC Port | P2P Port |
|---------|------|-------|-----------------|----------------|----------|----------|
| **Mainnet** | (none) | Real | `~/.shurium/wallet.dat` | `shr1...` | 8332 | 8333 |
| **Testnet** | `--testnet` | Fake | `~/.shurium/testnet/wallet.dat` | `tshr1...` | 18332 | 18333 |
| **Regtest** | `--regtest` | Fake | `~/.shurium/regtest/wallet.dat` | `ncrt1...` | 18443 | 18444 |

---

## Next Steps

1. 📖 Read the [Wallet Security Guide](WALLET_GUIDE.md)
2. ⛏️ Try [Mining](MINING_GUIDE.md) to earn coins
3. 🥩 Set up [Staking](STAKING_GUIDE.md) for passive income
4. 🎁 Register for [UBI](#ubi) to claim daily coins

---

<div align="center">

**Questions?** Check the [Troubleshooting Guide](TROUBLESHOOTING.md) or ask in the community!

</div>
