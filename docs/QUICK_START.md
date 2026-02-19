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
git clone https://github.com/FidaHussain87/shurium.git

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
# Use -j8 to speed up with parallel compilation
cmake --build . -j8
```

**What gets created in `build/` directory:**

| File | Purpose |
|------|---------|
| `shuriumd` | The main SHURIUM program (your node) |
| `shurium-cli` | Tool to control your node |
| `shurium-wallet-tool` | Offline wallet manager |

### Verify It Worked

```bash
# Make sure you're in the build directory
pwd
# Should show: /path/to/shurium/build

# Check the binary exists
ls -la shuriumd shurium-cli

# Check version
./shuriumd --version
```

You should see:
```
SHURIUM Core version v0.1.0-genesis
```

⚠️ **IMPORTANT:** All commands (`./shuriumd`, `./shurium-cli`) must be run from the `build/` directory!

---

## Step 4: Data Directories

SHURIUM automatically creates data directories when first run:

| Network | Data Directory | Use For |
|---------|----------------|---------|
| Mainnet | `~/.shurium/` | Real transactions (real money) |
| Testnet | `~/.shurium/testnet/` | Network testing (fake money) |
| Regtest | `~/.shurium/regtest/` | Local development (fake money, instant mining) |

**Data directory contents (created automatically):**
```
~/.shurium/
├── shurium.conf      # Configuration file (optional)
├── debug.log         # Debug log file
├── wallet.dat        # Encrypted wallet keys
├── wallet_data.dat   # Wallet transaction data
├── blocks/           # Block data
├── chainstate/       # UTXO database
└── peers.dat         # Known peers
```

## Step 5: Configuration File (Optional)

**The daemon works without a config file using sensible defaults.** Only create one if you need custom settings.

### Create Configuration File (Optional)

```bash
# Create SHURIUM data folder
mkdir -p ~/.shurium

cat > ~/.shurium/shurium.conf << 'EOF'
# =========================================
#        SHURIUM CONFIGURATION FILE
# =========================================

# RPC SERVER (for shurium-cli to communicate with daemon)
server=1
rpcuser=shuriumuser
rpcpassword=CHANGE_THIS_PASSWORD_123

# CONNECTIONS
listen=1
maxconnections=50

# LOGGING (1=verbose, 0=normal)
debug=0

EOF
```

⚠️ **Important:** If you create a config file, change `rpcpassword` to something unique!

---

## Step 6: Start Your Node

### Understanding the Two Programs

SHURIUM has **two main programs** that work together:

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│   shuriumd (Daemon)         shurium-cli (Client)                    │
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

### Choose Your Network

| Network | Flag | Best For |
|---------|------|----------|
| **Regtest** | `--regtest` | Local testing, instant mining, beginners |
| **Testnet** | `--testnet` | Network testing with other nodes |
| **Mainnet** | (none) | Real transactions with real money |

**Recommended for beginners:** Start with `--regtest` mode.

---

### Option A: Regtest Mode (Recommended for Learning)

Regtest is a local-only mode where you can mine blocks instantly. Perfect for learning!

```bash
# IMPORTANT: Make sure you're in the build directory
cd /path/to/shurium/build

# Start daemon in background
./shuriumd --regtest --daemon

# Wait for startup
sleep 3

# Verify it's running
./shurium-cli --regtest getblockchaininfo
```

**Mine some coins to play with:**
```bash
# Get a new address
MYADDR=$(./shurium-cli --regtest getnewaddress)
echo "Your address: $MYADDR"

# Mine 101 blocks (need 100+ for coins to be spendable)
./shurium-cli --regtest generatetoaddress 101 $MYADDR

# Check your balance (should be ~3400 SHR)
./shurium-cli --regtest getbalance
```

---

### Option B: Testnet Mode (Network Practice)

```bash
# IMPORTANT: Make sure you're in the build directory
cd /path/to/shurium/build

# Start daemon in background
./shuriumd --testnet --daemon

# Wait for startup
sleep 3

# Verify it's running
./shurium-cli --testnet getblockchaininfo
```

---

### Option C: Run in Foreground (See What's Happening)

This keeps the daemon visible so you can see what's happening:

```bash
# Go to build directory
cd /path/to/shurium/build

# Start SHURIUM (regtest mode)
./shuriumd --regtest
```

**What you'll see:**

```
SHURIUM Daemon v0.1.0 starting...
Data directory: ~/.shurium/regtest
Network: regtest
Opening block database...
Loading wallet...
RPC server listening on 127.0.0.1:18443
SHURIUM Daemon started successfully
```

**Problem:** You need to keep this terminal open. If you close it, SHURIUM stops.

**Solution:** Open a **new terminal window** for running commands, or use `--daemon` flag.

---

### Managing Your Daemon

Here are the essential commands you'll need:

**For Regtest (local testing):**

| What You Want | Command |
|---------------|---------|
| Start daemon | `./shuriumd --regtest --daemon` |
| Check if running | `./shurium-cli --regtest getblockchaininfo` |
| Stop daemon | `./shurium-cli --regtest stop` |
| Mine blocks | `./shurium-cli --regtest generatetoaddress 1 <address>` |
| View logs | `tail -f ~/.shurium/regtest/debug.log` |

**For Testnet (network testing):**

| What You Want | Command |
|---------------|---------|
| Start daemon | `./shuriumd --testnet --daemon` |
| Check if running | `./shurium-cli --testnet getblockchaininfo` |
| Stop daemon | `./shurium-cli --testnet stop` |
| View logs | `tail -f ~/.shurium/testnet/debug.log` |

**For Mainnet (real money):**

| What You Want | Command |
|---------------|---------|
| Start daemon | `./shuriumd --daemon` |
| Check if running | `./shurium-cli getblockchaininfo` |
| Stop daemon | `./shurium-cli stop` |
| View logs | `tail -f ~/.shurium/debug.log` |

**Force stop (any network):**
```bash
pkill -f shuriumd
```

---

### Important Ports

| Port | Purpose | Network |
|------|---------|---------|
| **18443** | RPC (commands) | Regtest |
| **18444** | P2P (other nodes) | Regtest |
| **18332** | RPC (commands) | Testnet |
| **18333** | P2P (other nodes) | Testnet |
| **8332** | RPC (commands) | Mainnet |
| **8333** | P2P (other nodes) | Mainnet |

The daemon automatically starts the RPC server - you don't need to run anything else!

---

### Verify It's Working

After starting the daemon, test it (using regtest as example):

```bash
# Check blockchain status
./shurium-cli --regtest getblockchaininfo

# Check wallet status
./shurium-cli --regtest getwalletinfo

# Check balance
./shurium-cli --regtest getbalance
```

If you see JSON output, everything is working.

**For testnet:** Replace `--regtest` with `--testnet`
**For mainnet:** Remove the flag entirely

---

## Step 7: Create Your Wallet

### Wallet vs Address

| Term | What It Is |
|------|------------|
| **Wallet** | Container holding all your keys/addresses. Stores your 24-word recovery phrase. |
| **Address** | A destination for receiving coins. You can have many addresses in one wallet. |
| **Recovery Phrase** | 24 words that can restore your entire wallet. **NEVER share these!** |

### Two Ways to Create a Wallet

**Option A: Automatic (Default)**
When the daemon starts for the first time, it automatically creates a wallet. However, this does **NOT** show you the recovery phrase.

**Option B: Manual with Recovery Phrase (Recommended)**
Create a wallet explicitly to see your 24-word recovery phrase:

```bash
# Stop daemon if running
./shurium-cli --regtest stop 2>/dev/null

# Start daemon
./shuriumd --regtest --daemon
sleep 3

# Create wallet and GET YOUR RECOVERY PHRASE
./shurium-cli --regtest createwallet "mywallet" "optional_password"
```

**Output:**
```json
{
  "name": "mywallet",
  "mnemonic": "word1 word2 word3 ... word24",
  "warning": "IMPORTANT: Write down these 24 words..."
}
```

⚠️ **CRITICAL - WRITE THESE DOWN:**
1. **Copy the 24 words** from the `mnemonic` field
2. **Write them on paper** (not digital!)
3. **Store securely** (safe, safety deposit box, etc.)
4. **Never share** with anyone
5. These words can **restore your wallet** if you lose access

### Wallet Files Location

| Network | Wallet Files |
|---------|--------------|
| Regtest | `~/.shurium/regtest/wallet.dat` |
| Testnet | `~/.shurium/testnet/wallet.dat` |
| Mainnet | `~/.shurium/wallet.dat` |

### Generate a New Address

Once you have a wallet, generate addresses to receive coins:

```bash
# Make sure you're in the build directory
cd /path/to/shurium/build

# Create your first receiving address (regtest example)
./shurium-cli --regtest getnewaddress
```

**Output:**
```
shr1q8w4j5k6n2m3v4c5x6z7a8s9d0f1g2h3j4k5l6
```

🎉 **This is YOUR address!** It's like an email address for money.

### Understand Your Address

```
shr1q8w4j5k6n2m3v4c5x6z7a8s9d0f1g2h3j4k5l6
│││
││└── The unique part (your specific address)
│└─── Address type (q = native segwit)
└──── Network prefix (shr = mainnet/regtest, tshr = testnet)
```

### Get Coins in Regtest Mode

In regtest, you can mine your own coins instantly:

```bash
# Get an address
ADDR=$(./shurium-cli --regtest getnewaddress)

# Mine 101 blocks to that address
./shurium-cli --regtest generatetoaddress 101 $ADDR

# Check balance (should be ~3400 SHR)
./shurium-cli --regtest getbalance
```

**Why 101 blocks?** Coinbase (mining) rewards require 100 confirmations before they can be spent. Mining 101 blocks means the first block's reward is now spendable.

---

## Step 8: Secure Your Wallet

### Understanding Wallet Security

| Security Layer | What It Protects Against | How to Set Up |
|----------------|-------------------------|---------------|
| **Recovery Phrase** | Complete loss (hardware failure, theft) | Write down 24 words from `createwallet` |
| **Encryption** | Unauthorized access to wallet file | Use `encryptwallet` command |
| **Backup** | File corruption, accidental deletion | Use `backupwallet` command |

### 1. Save Your Recovery Phrase (MOST IMPORTANT)

If you used `createwallet`, you received 24 words. These are your **recovery phrase**.

```
word1 word2 word3 word4 word5 word6
word7 word8 word9 word10 word11 word12
word13 word14 word15 word16 word17 word18
word19 word20 word21 word22 word23 word24
```

**How to store safely:**
- ✅ Write on paper, store in safe
- ✅ Engrave on metal plate (fire/water resistant)
- ✅ Split into parts, store in different locations
- ❌ Never store digitally (computer, phone, cloud)
- ❌ Never email or message to anyone
- ❌ Never take a photo

### 2. Encrypt Your Wallet

Add password protection to your wallet file:

```bash
./shurium-cli --regtest encryptwallet "your_strong_password_here"
```

⚠️ **Warning:** SHURIUM will restart after this. That's normal!

After encryption, you'll need to unlock before sending:
```bash
# Unlock for 300 seconds (5 minutes)
./shurium-cli --regtest walletpassphrase "your_password" 300

# Send transaction...

# Lock again
./shurium-cli --regtest walletlock
```

### 3. Backup Your Wallet File

Create a backup copy of your wallet:

```bash
./shurium-cli --regtest backupwallet ~/shurium-wallet-backup.dat
```

Store this backup file securely (USB drive in safe, etc.).

### Check Your Wallet Status

```bash
./shurium-cli --regtest getwalletinfo
```

Shows:
- `balance` - Your available balance
- `unlocked_until` - When wallet will auto-lock (0 = locked)
- `keypoolsize` - Number of pre-generated addresses

---

## Step 9: Test Sending Coins (Regtest)

```bash
# Get a second address
ADDR2=$(./shurium-cli --regtest getnewaddress)

# Send 100 SHR to it
./shurium-cli --regtest sendtoaddress $ADDR2 100

# Mine a block to confirm the transaction
./shurium-cli --regtest generatetoaddress 1 $ADDR2

# Check balance
./shurium-cli --regtest getbalance
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

All examples below use `--regtest`. For testnet use `--testnet`, for mainnet omit the flag.

### Generate More Addresses

```bash
# Address for rent payments
./shurium-cli --regtest getnewaddress

# List all addresses
./shurium-cli --regtest listaddresses
```

### Receive Money

1. Share your address with the sender
2. Wait for them to send (or mine in regtest)
3. Check your balance:

```bash
./shurium-cli --regtest getbalance
```

### Send Money

```bash
./shurium-cli --regtest sendtoaddress "RECIPIENT_ADDRESS" AMOUNT
```

**Example:**
```bash
./shurium-cli --regtest sendtoaddress "shr1qrecipient..." 10.5
```

### Check Transactions

```bash
# Recent transactions
./shurium-cli --regtest listtransactions

# Specific transaction
./shurium-cli --regtest gettransaction "TRANSACTION_ID"
```

---

## Common Commands Cheat Sheet

**Regtest (local development):**

| What You Want | Command |
|---------------|---------|
| Start daemon | `./shuriumd --regtest --daemon` |
| Stop daemon | `./shurium-cli --regtest stop` |
| Check balance | `./shurium-cli --regtest getbalance` |
| New address | `./shurium-cli --regtest getnewaddress` |
| Send coins | `./shurium-cli --regtest sendtoaddress "addr" amount` |
| Mine blocks | `./shurium-cli --regtest generatetoaddress 1 "addr"` |
| Transaction history | `./shurium-cli --regtest listtransactions` |
| Blockchain info | `./shurium-cli --regtest getblockchaininfo` |
| View logs | `tail -f ~/.shurium/regtest/debug.log` |
| All commands | `./shurium-cli --regtest help` |

**For testnet:** Replace `--regtest` with `--testnet`, logs are in `~/.shurium/testnet/`
**For mainnet:** Remove the flag, logs are in `~/.shurium/`

---

## Troubleshooting

### "no such file or directory: ./shuriumd"

**Problem:** You're not in the build directory or haven't built the project.

**Solution:**
```bash
# Navigate to the build directory
cd /path/to/shurium/build

# If binary doesn't exist, build it first
ls -la shuriumd || (cd .. && mkdir -p build && cd build && cmake .. && cmake --build . -j8)
```

### "Cannot connect to server" or "Connection refused"

**Problem:** The daemon (`shuriumd`) is not running.

**Solution:** Start the daemon first:

```bash
# Make sure you're in the build directory
cd /path/to/shurium/build

# Start daemon
./shuriumd --regtest --daemon

# Wait 2-3 seconds, then try your command again
sleep 3
./shurium-cli --regtest getblockchaininfo
```

### "Cannot obtain a lock on data directory"

**Problem:** Another daemon instance is already running.

**Solution:**
```bash
# Stop the existing daemon gracefully
./shurium-cli --regtest stop

# Or force kill if that doesn't work
pkill -f shuriumd
sleep 2

# Now start fresh
./shuriumd --regtest --daemon
```

### Balance shows 0 after mining

**Problem:** Coinbase rewards need 100 confirmations.

**Solution:** Mine more blocks:
```bash
# Mine 101 blocks total
./shurium-cli --regtest generatetoaddress 101 $(./shurium-cli --regtest getnewaddress)
```

### "Transaction rejected: mempool min fee not met"

**Problem:** Transaction fee too low.

**Solution:** This should be fixed in latest version. If using old version, the default fee was increased to meet minimum requirements.

### "Wallet is locked"

**Problem:** Your wallet is encrypted and locked.

**Solution:** Unlock it:
```bash
# Unlock for 5 minutes (300 seconds)
./shurium-cli --regtest walletpassphrase "your_password" 300
```

### How to View Logs

Logs help you understand what's happening:

```bash
# Regtest logs
tail -50 ~/.shurium/regtest/debug.log

# Watch logs in real-time (press Ctrl+C to stop)
tail -f ~/.shurium/regtest/debug.log

# Search for errors
grep -i error ~/.shurium/regtest/debug.log
```

### How to Completely Reset (Start Fresh)

If something is really broken:

```bash
# Stop daemon
./shurium-cli --regtest stop 2>/dev/null || pkill -f shuriumd

# WARNING: This deletes everything including your wallet!
# Only do this for regtest/testnet, never for mainnet with real money!

# Reset regtest
rm -rf ~/.shurium/regtest

# Reset testnet  
rm -rf ~/.shurium/testnet

# Start fresh
cd /path/to/shurium/build
./shuriumd --regtest --daemon
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
│   ./shuriumd --testnet                ./shuriumd                    │
│   ./shurium-cli --testnet ...         ./shurium-cli ...             │
│                                                                     │
│   Wallet location:                  Wallet location:                │
│   ~/.shurium/testnet/wallet.dat       ~/.shurium/wallet.dat         │
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
5. 🗳️ Participate in [Governance](COMMANDS.md#️-governance-commands) to vote on network decisions

---

<div align="center">

**Questions?** Check the [Troubleshooting Guide](TROUBLESHOOTING.md) or ask in the community!

</div>
