<div align="center">

# 🌟 SHURIUM

### Governed by People. Powered by Useful Work.

[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)](https://github.com/FidaHussain87/shurium)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

**Financial freedom for everyone • Democratic governance • Mining that helps humanity**

[Quick Start](#-quick-start) •
[Documentation](docs/) •
[FAQ](#-faq)

</div>

---

## 🎯 What Can You Do With SHURIUM?

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│   💸 SEND          Send money to anyone, anywhere, in seconds       │
│                                                                     │
│   📥 RECEIVE       Get paid without banks or middlemen              │
│                                                                     │
│   💰 SAVE          Store value securely on your computer            │
│                                                                     │
│   ⛏️  MINE          Earn coins by helping the network               │
│                                                                     │
│   🥩 STAKE         Earn passive income from your holdings           │
│                                                                     │
│   🎁 CLAIM UBI     Get free daily coins just for being human        │
│                                                                     │
│   🗳️  VOTE          Have a say in how SHURIUM evolves               │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## ⚡ Quick Start

### 1️⃣ Install (5 minutes)

```bash
# Download
git clone https://github.com/FidaHussain87/shurium.git
cd shurium

# Build
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

**Build outputs** are in `build/` directory:
- `shuriumd` - Main daemon
- `shurium-cli` - Command-line interface
- `shurium-wallet-tool` - Wallet utility

### 2️⃣ Choose Your Network

| Network | Command | Money | Use For |
|---------|---------|-------|---------|
| **Mainnet** | `./shuriumd` | Real | Actual transactions |
| **Testnet** | `./shuriumd --testnet` | Fake | Learning & testing |
| **Regtest** | `./shuriumd --regtest` | Fake | Local development |

**New users:** Start with regtest for local testing, then testnet for network testing.

### 3️⃣ Data Directories

SHURIUM stores data in these locations:

| Network | Data Directory | Config File |
|---------|----------------|-------------|
| Mainnet | `~/.shurium/` | `~/.shurium/shurium.conf` |
| Testnet | `~/.shurium/testnet/` | `~/.shurium/shurium.conf` |
| Regtest | `~/.shurium/regtest/` | `~/.shurium/shurium.conf` |

**Data directory contents:**
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

### 4️⃣ Configuration (Optional)

Create `~/.shurium/shurium.conf` if you need custom settings:

```bash
# Create config directory and file
mkdir -p ~/.shurium
cat > ~/.shurium/shurium.conf << 'EOF'
# RPC settings (for remote access)
rpcuser=shurium
rpcpassword=yourpassword
rpcallowip=127.0.0.1

# Network settings
listen=1
maxconnections=125

# Logging
debug=1
EOF
```

**Note:** The daemon works without a config file using sensible defaults.

### 5️⃣ Start Your Node

**IMPORTANT:** Run commands from the `build/` directory:

```bash
cd /path/to/shurium/build
```

**For Local Development (Regtest) - Recommended for beginners:**
```bash
# Start daemon (wallet is created automatically on first run)
./shuriumd --regtest --daemon

# Wait 3 seconds for startup
sleep 3

# Verify it's running
./shurium-cli --regtest getblockchaininfo

# Generate a new address in your wallet
./shurium-cli --regtest getnewaddress

# Mine 101 blocks (to get spendable coins)
./shurium-cli --regtest generatetoaddress 101 <your-address>

# Check balance (should be ~3400 SHR)
./shurium-cli --regtest getbalance

# Stop daemon when done
./shurium-cli --regtest stop
```

> **Note:** The wallet (`~/.shurium/regtest/wallet.dat`) is created automatically when the daemon starts for the first time. You don't need to create it manually.

**For Practice (Testnet):**
```bash
# Start daemon with --testnet
./shuriumd --testnet --daemon

# Use wallet with --testnet
./shurium-cli --testnet getbalance
./shurium-cli --testnet getnewaddress
```

**For Real Money (Mainnet):**
```bash
# Start daemon
./shuriumd --daemon

# Use wallet
./shurium-cli getbalance
./shurium-cli getnewaddress
```

### 6️⃣ Essential Commands

| What You Want | Regtest | Mainnet |
|---------------|---------|---------|
| Check balance | `./shurium-cli --regtest getbalance` | `./shurium-cli getbalance` |
| New address | `./shurium-cli --regtest getnewaddress` | `./shurium-cli getnewaddress` |
| Send coins | `./shurium-cli --regtest sendtoaddress <addr> <amount>` | `./shurium-cli sendtoaddress <addr> <amount>` |
| Mine blocks | `./shurium-cli --regtest generatetoaddress <n> <addr>` | N/A (use miner) |
| Wallet info | `./shurium-cli --regtest getwalletinfo` | `./shurium-cli getwalletinfo` |
| Blockchain info | `./shurium-cli --regtest getblockchaininfo` | `./shurium-cli getblockchaininfo` |
| Stop daemon | `./shurium-cli --regtest stop` | `./shurium-cli stop` |
| View logs | `tail -f ~/.shurium/regtest/debug.log` | `tail -f ~/.shurium/debug.log` |
| All commands | `./shurium-cli --regtest help` | `./shurium-cli help` |

### 7️⃣ Troubleshooting

**"no such file or directory: ./shuriumd"**
```bash
# You're not in the build directory. Navigate there first:
cd /path/to/shurium/build
ls -la shuriumd  # Verify binary exists
```

**"Cannot obtain a lock on data directory"**
```bash
# Another instance is running. Stop it first:
./shurium-cli --regtest stop  # or --testnet, or no flag for mainnet
# Or kill it manually:
pkill shuriumd
```

**"Connection refused" when using shurium-cli**
```bash
# Daemon isn't running. Start it:
./shuriumd --regtest --daemon
sleep 3
./shurium-cli --regtest getblockchaininfo
```

**Balance shows 0 after mining**
```bash
# Coinbase rewards need 100 confirmations. Mine more blocks:
./shurium-cli --regtest generatetoaddress 101 $(./shurium-cli --regtest getnewaddress)
```

**Transaction rejected: "mempool min fee not met"**
```bash
# Transaction fee too low. This is now fixed in default settings.
# If using custom fee, ensure feeRate >= 1000 sat/kvB
```

**🎉 That's it! You're running SHURIUM!**

> 📚 **Need more help?** See the [Complete Quick Start Guide](docs/QUICK_START.md)

---

## 📖 Documentation

### Understanding SHURIUM

| Document | Description | Audience |
|----------|-------------|----------|
| 📜 [Whitepaper](docs/WHITEPAPER.md) | Core vision, design & innovation | Everyone |
| 🏗️ [Architecture](docs/ARCHITECTURE.md) | How the 18 modules work together | Technical |
| ⚖️ [Comparison](docs/COMPARISON.md) | SHURIUM vs Bitcoin, Ethereum, etc. | Everyone |

### For Different Audiences

| Document | Description | Audience |
|----------|-------------|----------|
| 💼 [For Investors](docs/FOR_INVESTORS.md) | Investment thesis & tokenomics | Investors |
| ⛏️ [For Miners](docs/FOR_MINERS.md) | Proof of Useful Work explained | Miners |
| 🎁 [UBI Explained](docs/UBI_EXPLAINED.md) | Universal Basic Income deep dive | Users |

### User Guides

| Guide | Description | Difficulty |
|-------|-------------|------------|
| 📘 [Quick Start](docs/QUICK_START.md) | Get running in 10 minutes | ⭐ Beginner |
| 💰 [Wallet Guide](docs/WALLET_GUIDE.md) | Create, secure & backup your wallet | ⭐ Beginner |
| ⛏️ [Mining Guide](docs/MINING_GUIDE.md) | Earn coins by mining | ⭐⭐ Intermediate |
| 🥩 [Staking Guide](docs/STAKING_GUIDE.md) | Earn passive income | ⭐⭐ Intermediate |
| 🔧 [Troubleshooting](docs/TROUBLESHOOTING.md) | Fix common problems | ⭐ Beginner |
| 📋 [Command Reference](docs/COMMANDS.md) | All available commands | ⭐⭐⭐ Advanced |

---

## 🌟 What Makes SHURIUM Special?

### 💵 Universal Basic Income (UBI)

Every day, **30% of all new coins** are shared equally among verified users.

```
┌────────────────────────────────────────────────┐
│         HOW UBI DISTRIBUTION WORKS             │
├────────────────────────────────────────────────┤
│                                                │
│   New coins created today: 288,000 SHR         │
│                    │                           │
│                    ▼                           │
│   ┌────────────────────────────────┐           │
│   │     UBI POOL (30%)             │           │
│   │     86,400 SHR                 │           │
│   └────────────────────────────────┘           │
│                    │                           │
│         ┌─────────┼─────────┐                  │
│         ▼         ▼         ▼                  │
│       👤        👤        👤                   │
│    Person 1  Person 2  Person 3  ...           │
│                                                │
│   Each verified person gets equal share!       │
│                                                │
└────────────────────────────────────────────────┘
```

### ⚡ Fast Transactions

| Cryptocurrency | Block Time | Your Wait |
|---------------|------------|-----------|
| Bitcoin | 10 minutes | ☕ Coffee break |
| Ethereum | 12 seconds | 🚶 Quick stretch |
| **SHURIUM** | **30 seconds** | **👍 Almost instant** |

### 🧠 Useful Mining

Bitcoin mining wastes electricity on meaningless puzzles. SHURIUM miners solve **real problems**:

- 🔬 Scientific research
- 🤖 AI/ML training
- 📊 Data analysis

**Get paid to help humanity!**

---

## 💰 Understanding Your Money

### Your Wallet Explained

```
┌─────────────────────────────────────────────────────────────────┐
│                        YOUR WALLET                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  🔐 PRIVATE KEY (Secret - NEVER share!)                  │   │
│  │                                                          │   │
│  │  Like your bank PIN × 1000                               │   │
│  │  Anyone with this can take ALL your money                │   │
│  │                                                          │   │
│  │  Example: L4rK1yDtCWekvXuE54F...                         │   │
│  └──────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  📫 PUBLIC ADDRESS (Safe to share)                       │   │
│  │                                                          │   │
│  │  Like your email address for money                       │   │
│  │  Give this to anyone who wants to pay you                │   │
│  │                                                          │   │
│  │  Example: shr1q8w4j5k6n2m3v4c5x6z7...                    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  💵 BALANCE                                              │   │
│  │                                                          │   │
│  │  Available:    150.50 SHR  ← Ready to spend              │   │
│  │  Pending:       10.00 SHR  ← Coming soon                 │   │
│  │  Staking:    1,000.00 SHR  ← Locked, earning rewards     │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Transaction Flow

```
    YOU                                              RECIPIENT
     │                                                  │
     │  1. You send 10 SHR                              │
     │  ─────────────────────►                          │
     │                                                  │
     │            ┌─────────────────────┐               │
     │            │   SHURIUM NETWORK     │             │
     │            │                     │               │
     │            │  ✓ Check balance    │               │
     │            │  ✓ Verify signature │               │
     │            │  ✓ Add to block     │               │
     │            │                     │               │
     │            └─────────────────────┘               │
     │                                                  │
     │  2. Confirmed in ~30 seconds                     │
     │                          ─────────────────────►  │
     │                                                  │
     │  Your balance: -10 SHR       Their balance: +10 SHR
```

---

## 🛡️ Security Checklist

Before you start, understand these **critical** rules:

### ✅ DO:

| Action | Why |
|--------|-----|
| ✅ Write down your recovery phrase on paper | Only way to recover if computer dies |
| ✅ Use a strong, unique password | Protects your wallet file |
| ✅ Backup wallet.dat regularly | Extra protection |
| ✅ Double-check addresses before sending | Transactions can't be reversed |
| ✅ Start with testnet | Practice with fake money first |

### ❌ DON'T:

| Action | Consequence |
|--------|-------------|
| ❌ Share your private key | Lose ALL your money |
| ❌ Store recovery phrase digitally | Hackers can find it |
| ❌ Send to unverified addresses | Money gone forever |
| ❌ Skip backups | Risk losing everything |
| ❌ Use public WiFi for transactions | Can be intercepted |

---

## 📊 SHURIUM at a Glance

| Property | Value |
|----------|-------|
| **Coin Symbol** | SHR |
| **Total Supply** | ~21 Million (Bitcoin-like) |
| **Block Time** | 30 seconds |
| **Initial Block Reward** | 50 SHR |
| **Halving Interval** | ~4 years (4,207,680 blocks) |
| **Consensus** | Proof of Useful Work + Delegated PoS |

### Block Reward Distribution

Every block reward is automatically split among 5 recipients:

```
                    BLOCK REWARD: 50 SHR
    ┌─────────────────────────────────────────────────┐
    │                                                 │
    │  ████████████████░░░░░░░░░░░░░░  40% Miners     │  20 SHR
    │  ████████████░░░░░░░░░░░░░░░░░░  30% UBI Pool   │  15 SHR
    │  ██████░░░░░░░░░░░░░░░░░░░░░░░░  15% Contrib.   │  7.5 SHR
    │  ████░░░░░░░░░░░░░░░░░░░░░░░░░░  10% Ecosystem  │  5 SHR
    │  ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░   5% Stability  │  2.5 SHR
    │                                                 │
    └─────────────────────────────────────────────────┘
```

### Fund Security

> **WARNING: Configure Your Own Fund Addresses Before Production Use!**
>
> By default, SHURIUM uses **demo addresses** generated from public seeds. **Nobody controls the private keys** for these addresses - any funds sent to them are **permanently lost**.
>
> Before mining in production, configure your own addresses using `setfundaddress` or `shurium.conf`.

The 60% allocated to protocol funds is secured by **2-of-3 multisig** addresses:

| Fund | % | Governance Required | Purpose |
|------|---|---------------------|---------|
| **UBI Pool** | 30% | Yes (always) | Universal Basic Income for verified humans |
| **Contribution Fund** | 15% | No (up to 1,000 SHR) | Rewards for human contributions |
| **Ecosystem Fund** | 10% | No (up to 5,000 SHR) | Development and ecosystem growth |
| **Stability Reserve** | 5% | Yes (always) | Price stability mechanism |

Each fund is controlled by 3 key holders (Governance, Foundation, Community) - any 2 must sign to spend.

### View Fund Information

```bash
# View all fund details (addresses, governance rules, multisig info)
./shurium-cli getfundinfo

# Check balance for a specific fund
./shurium-cli getfundbalance ubi
./shurium-cli getfundbalance contribution
./shurium-cli getfundbalance ecosystem
./shurium-cli getfundbalance stability

# Set your own fund address (IMPORTANT for production!)
./shurium-cli setfundaddress ubi shr1qyouraddress...

# Get fund address with multisig public keys
./shurium-cli getfundaddress ubi

# List fund transactions
./shurium-cli listfundtransactions ubi 10
```

**Example output from `getfundbalance ubi` (with default demo address):**
```json
{
  "WARNING": "Using DEFAULT DEMO ADDRESS. NOBODY controls the private keys!",
  "fund": "UBI Pool",
  "address": "shr1q902acb56b202b7f43cf605da3ad7aa1e5abbb9fb",
  "address_source": "default (demo)",
  "is_custom_address": false,
  "balance": 0.00,
  "note": "Configure your own address before mining!"
}
```

View detailed fund documentation: [Fund Management Commands](docs/COMMANDS.md#-fund-management-commands)

---

## ❓ FAQ

<details>
<summary><b>🤔 Is SHURIUM real money?</b></summary>

SHURIUM has real value on **mainnet**. For learning, use **testnet** which has fake money that's safe to experiment with.

```bash
# Testnet (fake money - for learning)
./shuriumd --testnet

# Mainnet (real money - be careful!)
./shuriumd
```
</details>

<details>
<summary><b>💸 How do I get my first coins?</b></summary>

Several ways:
1. **Testnet faucet** - Get free testnet coins for practice
2. **Mining** - Earn by helping verify transactions
3. **Staking** - Earn passive income from existing coins
4. **UBI** - Claim daily share after identity verification
5. **Buy/Trade** - Exchange with others
</details>

<details>
<summary><b>😰 What if I lose my password?</b></summary>

- **If you have your 24-word recovery phrase:** You can restore everything!
- **If you lost both password AND recovery phrase:** Your coins are lost forever. No one can help - not even SHURIUM developers.

**Always backup your recovery phrase!**
</details>

<details>
<summary><b>⏱️ How long do transactions take?</b></summary>

| Confirmations | Time | Safety Level |
|---------------|------|--------------|
| 0 | Instant | Unconfirmed - risky |
| 1 | ~30 sec | Basic confirmation |
| 6 | ~3 min | Safe for most transactions |
| 100 | ~50 min | Required for mining rewards |
</details>

<details>
<summary><b>🎁 How does UBI work?</b></summary>

1. **Register identity** - Prove you're a unique human (using ZK proofs)
2. **Wait for verification** - System confirms your uniqueness
3. **Claim daily** - Get your share of the UBI pool each day

The pool is 30% of all new coins, split equally among all verified users.
</details>

<details>
<summary><b>⛏️ Mining vs 🥩 Staking - Which should I choose?</b></summary>

| | Mining | Staking |
|---|--------|---------|
| **Need hardware?** | Yes (good CPU) | No |
| **Energy cost?** | Higher | Minimal |
| **Technical skill?** | Medium | Low |
| **Earnings** | Higher but variable | Lower but steady |
| **Risk** | Hardware failure | Slashing if you misbehave |

**Recommendation:** Start with staking - it's easier and less risky!
</details>

---

## 🚀 Ready to Start?

### Option A: Just Want to Use It (Easiest)

1. [Create a wallet](docs/WALLET_GUIDE.md)
2. Get some coins (testnet faucet or buy)
3. Start sending and receiving!

### Option B: Want to Earn Passive Income

1. Complete Option A first
2. [Set up staking](docs/STAKING_GUIDE.md)
3. Earn rewards while you sleep!

### Option C: Want to Mine

1. Complete Option A first
2. [Set up mining](docs/MINING_GUIDE.md)
3. Earn by solving useful problems!

### Option D: Want Everything

1. [Read the complete guide](docs/QUICK_START.md)
2. Set up node, wallet, mining, and staking
3. Claim your daily UBI
4. Participate in governance

---

## 🏗️ Project Structure

```
shurium/
├── 📁 src/                    # Source code
│   ├── shuriumd.cpp            # Main daemon
│   ├── shurium-cli.cpp         # Command line interface
│   ├── shurium-wallet.cpp      # Wallet tool
│   ├── 📁 consensus/         # Consensus rules, checkpoints
│   ├── 📁 economics/         # Rewards, UBI, oracles, stability
│   ├── 📁 network/           # P2P networking, DNS seeding
│   ├── 📁 rpc/               # RPC server (37+ commands)
│   ├── 📁 util/              # Config parsing, logging, helpers
│   └── ...                   # Other modules
├── 📁 include/shurium/          # Header files (18 modules)
├── 📁 tests/                  # Test suite (400+ tests)
│   ├── 📁 consensus/         # Consensus & checkpoint tests
│   ├── 📁 economics/         # Oracle & economics tests
│   ├── 📁 network/           # Network & DNS seeder tests
│   ├── 📁 util/              # Config & utility tests
│   └── ...                   # Other test modules
├── 📁 docs/                   # Documentation
│   ├── ARCHITECTURE.md       # Technical architecture (18 modules)
│   ├── QUICK_START.md        # Getting started guide
│   ├── WALLET_GUIDE.md       # Wallet tutorial
│   ├── MINING_GUIDE.md       # Mining tutorial
│   ├── STAKING_GUIDE.md      # Staking tutorial
│   ├── COMMANDS.md           # RPC command reference
│   └── TROUBLESHOOTING.md    # Common issues
├── 📄 CMakeLists.txt          # Build configuration
├── 📄 README.md               # This file
└── 📄 LICENSE                 # License information
```

---

## 📞 Need Help?

| Resource | Use For |
|----------|---------|
| 📚 [Documentation](docs/) | Step-by-step guides |
| 🔧 [Troubleshooting](docs/TROUBLESHOOTING.md) | Fix common problems |
| 💬 Community Chat | Ask questions, get help |
| 🐛 GitHub Issues | Report bugs |

---

<div align="center">

### Built with ❤️ for Everyone

**SHURIUM** - Cryptocurrency that works for people, not against them.

*Start with testnet. Backup your wallet. Never share private keys.*

</div>
