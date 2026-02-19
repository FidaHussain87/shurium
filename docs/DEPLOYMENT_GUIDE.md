# SHURIUM Deployment Guide

## Complete Setup for Governments, Organizations, and Citizens

This guide covers:
1. [Government/Organization Setup](#part-1-governmentorganization-setup) - Deploy SHURIUM at country/organization level
2. [Citizen/Miner Setup](#part-2-citizenminer-setup) - Individual mining and reward monitoring

---

# Part 1: Government/Organization Setup

## Overview

As a government deploying SHURIUM, you will:
1. Set up the core infrastructure (nodes)
2. Configure fund addresses YOU control
3. Define tokenomics parameters
4. Launch the network
5. Enable citizens to participate

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GOVERNMENT SHURIUM ARCHITECTURE                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐   │
│   │  GOVERNMENT     │     │   FOUNDATION    │     │   COMMUNITY     │   │
│   │  NODE (Primary) │     │   NODE          │     │   NODES         │   │
│   └────────┬────────┘     └────────┬────────┘     └────────┬────────┘   │
│            │                       │                       │            │
│            └───────────────────────┼───────────────────────┘            │
│                                    │                                    │
│                           ┌────────▼────────┐                           │
│                           │   SHURIUM       │                           │
│                           │   BLOCKCHAIN    │                           │
│                           └────────┬────────┘                           │
│                                    │                                    │
│            ┌───────────────────────┼───────────────────────┐            │
│            │                       │                       │            │
│   ┌────────▼────────┐     ┌────────▼────────┐     ┌────────▼────────┐   │
│   │  CITIZEN        │     │  CITIZEN        │     │  CITIZEN        │   │
│   │  MINER 1        │     │  MINER 2        │     │  MINER N        │   │
│   └─────────────────┘     └─────────────────┘     └─────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Step 1: Infrastructure Setup

### 1.1 System Requirements

**Government Primary Node:**
```
- CPU: 8+ cores
- RAM: 32GB+
- Storage: 1TB+ SSD
- Network: 1Gbps dedicated
- OS: Ubuntu 22.04 LTS or similar
```

### 1.2 Install SHURIUM

```bash
# Clone repository
git clone https://github.com/FidaHussain87/shurium.git
cd shurium

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build . -j$(nproc)

# Verify installation
./shuriumd --version
./shurium-cli --version
```

### 1.3 Create Data Directory

```bash
# Create SHURIUM data directory
mkdir -p ~/.shurium

# Set proper permissions
chmod 700 ~/.shurium
```

---

## Step 2: Generate Government-Controlled Fund Addresses

**CRITICAL: You must create addresses YOUR organization controls before launching!**

### 2.1 Create Government Wallet

```bash
# Start daemon temporarily to create wallet
./shuriumd --daemon

# Wait for startup
sleep 5

# Create a new wallet for government funds
./shurium-cli createwallet "GovernmentFunds"

# Generate addresses for each fund
echo "=== GENERATING GOVERNMENT FUND ADDRESSES ==="

# UBI Pool Address (30% of block rewards)
UBI_ADDRESS=$(./shurium-cli getnewaddress "UBI_Pool_Government")
echo "UBI Pool Address: $UBI_ADDRESS"

# Contribution Fund Address (15% of block rewards)
CONTRIB_ADDRESS=$(./shurium-cli getnewaddress "Contribution_Fund_Government")
echo "Contribution Fund Address: $CONTRIB_ADDRESS"

# Ecosystem Fund Address (10% of block rewards)
ECO_ADDRESS=$(./shurium-cli getnewaddress "Ecosystem_Fund_Government")
echo "Ecosystem Fund Address: $ECO_ADDRESS"

# Stability Reserve Address (5% of block rewards)
STABILITY_ADDRESS=$(./shurium-cli getnewaddress "Stability_Reserve_Government")
echo "Stability Reserve Address: $STABILITY_ADDRESS"

# Stop daemon
./shurium-cli stop
```

### 2.2 Backup Wallet (CRITICAL!)

```bash
# Backup wallet file
cp ~/.shurium/wallets/GovernmentFunds/wallet.dat ~/government_wallet_backup.dat

# Store backup securely (hardware security module, safe deposit, etc.)
# NEVER lose this file - it contains the private keys to all fund addresses!
```

### 2.3 Record Your Addresses

Save these addresses securely:
```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GOVERNMENT FUND ADDRESSES                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   UBI Pool (30%):        shr1q_____________________________             │
│   Contribution (15%):    shr1q_____________________________             │
│   Ecosystem (10%):       shr1q_____________________________             │
│   Stability (5%):        shr1q_____________________________             │
│                                                                         │
│   BACKUP LOCATION: _________________________________                    │
│   DATE CREATED: ____________________________________                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Step 3: Configure SHURIUM for Your Country

### 3.1 Create Configuration File

```bash
cat > ~/.shurium/shurium.conf << 'EOF'
# ============================================================================
# SHURIUM GOVERNMENT CONFIGURATION
# Country: [YOUR COUNTRY NAME]
# Date: [DATE]
# ============================================================================

# Network Settings
server=1
daemon=1
listen=1
port=9333
rpcport=9332

# RPC Settings (for administration)
rpcuser=government_admin
rpcpassword=CHANGE_THIS_TO_SECURE_PASSWORD_123!
rpcallowip=127.0.0.1
rpcbind=127.0.0.1

# ============================================================================
# FUND ADDRESSES - REPLACE WITH YOUR ADDRESSES FROM STEP 2
# ============================================================================

# UBI Pool - 30% of block rewards
# Purpose: Universal Basic Income distribution to verified citizens
ubiaddress=shr1qYOUR_UBI_ADDRESS_HERE

# Contribution Fund - 15% of block rewards  
# Purpose: Rewards for citizens who contribute to the nation
contributionaddress=shr1qYOUR_CONTRIBUTION_ADDRESS_HERE

# Ecosystem Fund - 10% of block rewards
# Purpose: Infrastructure, development, public services
ecosystemaddress=shr1qYOUR_ECOSYSTEM_ADDRESS_HERE

# Stability Reserve - 5% of block rewards
# Purpose: Economic stability, emergency fund
stabilityaddress=shr1qYOUR_STABILITY_ADDRESS_HERE

# ============================================================================
# NETWORK PARAMETERS
# ============================================================================

# For mainnet launch (production)
# network=mainnet

# For testing (use this first!)
network=regtest

# DNS seeds for peer discovery (add your government nodes)
# dnsseed=seed1.yourcountry.gov
# dnsseed=seed2.yourcountry.gov

# Logging
debug=1
printtoconsole=0
logfile=1

EOF
```

### 3.2 Update Configuration with Your Addresses

```bash
# Edit the configuration file
nano ~/.shurium/shurium.conf

# Replace these lines with your actual addresses from Step 2:
# ubiaddress=shr1qYOUR_UBI_ADDRESS_HERE
# contributionaddress=shr1qYOUR_CONTRIBUTION_ADDRESS_HERE
# ecosystemaddress=shr1qYOUR_ECOSYSTEM_ADDRESS_HERE
# stabilityaddress=shr1qYOUR_STABILITY_ADDRESS_HERE
```

---

## Step 4: Launch the Network

### 4.1 Start Government Node

```bash
# Start the daemon
./shuriumd --daemon

# Wait for startup
sleep 5

# Verify it's running
./shurium-cli getblockchaininfo
```

### 4.2 Verify Fund Configuration

```bash
# Check that YOUR addresses are configured (not default demo addresses!)
./shurium-cli getfundinfo
```

**Expected Output (CORRECT - no WARNING):**
```json
{
  "funds": [
    {
      "name": "UBI Pool",
      "address": "shr1qYOUR_ACTUAL_ADDRESS...",
      "address_source": "configuration file",
      "is_custom_address": true
    }
  ],
  "note": "Use 'setfundaddress...' or configure in shurium.conf for production."
}
```

**If you see WARNING or `address_source: "default (demo)"`, STOP and fix configuration!**

### 4.3 Mine Genesis Blocks (Initial Setup)

```bash
# Generate initial blocks to bootstrap the network
./shurium-cli generatetoaddress 100 $(./shurium-cli getnewaddress "Genesis")

# Verify blocks were created
./shurium-cli getblockcount
# Should show: 100

# Check fund balances
./shurium-cli getfundbalance ubi
./shurium-cli getfundbalance contribution
./shurium-cli getfundbalance ecosystem
./shurium-cli getfundbalance stability
```

---

## Step 5: Monitor Government Funds

### 5.1 Create Monitoring Script

```bash
cat > ~/monitor_funds.sh << 'EOF'
#!/bin/bash
# SHURIUM Government Fund Monitor

echo "╔════════════════════════════════════════════════════════════════════╗"
echo "║           SHURIUM GOVERNMENT FUND STATUS                           ║"
echo "║           $(date)                              ║"
echo "╚════════════════════════════════════════════════════════════════════╝"
echo ""

# Get blockchain info
BLOCK_HEIGHT=$(./shurium-cli getblockcount)
echo "Current Block Height: $BLOCK_HEIGHT"
echo ""

echo "┌────────────────────────────────────────────────────────────────────┐"
echo "│ FUND BALANCES                                                      │"
echo "├────────────────────────────────────────────────────────────────────┤"

for fund in ubi contribution ecosystem stability; do
    BALANCE=$(./shurium-cli getfundbalance $fund 2>/dev/null | grep '"balance"' | awk -F: '{print $2}' | tr -d ' ,')
    ADDRESS=$(./shurium-cli getfundbalance $fund 2>/dev/null | grep '"address"' | awk -F: '{print $2}' | tr -d ' ",')
    SOURCE=$(./shurium-cli getfundbalance $fund 2>/dev/null | grep '"address_source"' | awk -F: '{print $2}' | tr -d ' ",')
    
    printf "│ %-15s: %15s SHR\n" "$fund" "$BALANCE"
    printf "│   Address: %s\n" "$ADDRESS"
    printf "│   Source:  %s\n" "$SOURCE"
    echo "├────────────────────────────────────────────────────────────────────┤"
done

echo "└────────────────────────────────────────────────────────────────────┘"

# Check for warnings
WARNING=$(./shurium-cli getfundinfo 2>/dev/null | grep -i "WARNING")
if [ ! -z "$WARNING" ]; then
    echo ""
    echo "⚠️  WARNING: Some funds are using default demo addresses!"
    echo "    Configure your own addresses in shurium.conf"
fi

EOF

chmod +x ~/monitor_funds.sh
```

### 5.2 Run Monitor

```bash
cd /path/to/shurium/build
~/monitor_funds.sh
```

### 5.3 Set Up Automated Monitoring (Optional)

```bash
# Add to crontab for hourly monitoring
(crontab -l 2>/dev/null; echo "0 * * * * cd /path/to/shurium/build && ~/monitor_funds.sh >> ~/fund_monitor.log 2>&1") | crontab -
```

---

## Step 6: Publish Network Information for Citizens

Create a public document with:

```
╔════════════════════════════════════════════════════════════════════════╗
║                    [YOUR COUNTRY] SHURIUM NETWORK                       ║
╠════════════════════════════════════════════════════════════════════════╣
║                                                                         ║
║  OFFICIAL NETWORK INFORMATION                                           ║
║  ────────────────────────────                                           ║
║                                                                         ║
║  Network:        mainnet (or testnet for pilot)                         ║
║  Symbol:         SHR                                                    ║
║  Block Time:     30 seconds                                             ║
║  Block Reward:   50 SHR (halves every 4 years)                          ║
║                                                                         ║
║  REWARD DISTRIBUTION                                                    ║
║  ───────────────────                                                    ║
║  Miners:              40% (20 SHR per block)                            ║
║  UBI Pool:            30% (15 SHR per block)                            ║
║  Contribution Fund:   15% (7.5 SHR per block)                           ║
║  Ecosystem Fund:      10% (5 SHR per block)                             ║
║  Stability Reserve:    5% (2.5 SHR per block)                           ║
║                                                                         ║
║  GOVERNMENT SEED NODES                                                  ║
║  ─────────────────────                                                  ║
║  seed1.shurium.yourcountry.gov:9333                                     ║
║  seed2.shurium.yourcountry.gov:9333                                     ║
║  seed3.shurium.yourcountry.gov:9333                                     ║
║                                                                         ║
║  HOW TO PARTICIPATE                                                     ║
║  ──────────────────                                                     ║
║  1. Download SHURIUM from: https://shurium.yourcountry.gov/download     ║
║  2. Follow citizen setup guide                                          ║
║  3. Start mining and earning rewards!                                   ║
║                                                                         ║
╚════════════════════════════════════════════════════════════════════════╝
```

---

# Part 2: Citizen/Miner Setup

## Overview

As a citizen, you will:
1. Install SHURIUM
2. Connect to your country's network
3. Create your wallet
4. Start mining
5. Monitor your rewards

---

## Step 1: Install SHURIUM

### 1.1 Download and Build

```bash
# Clone repository
git clone https://github.com/FidaHussain87/shurium.git
cd shurium

# Build
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)

# Verify
./shuriumd --version
```

### 1.2 Create Data Directory

```bash
mkdir -p ~/.shurium
chmod 700 ~/.shurium
```

---

## Step 2: Configure for Your Country's Network

### 2.1 Create Configuration File

```bash
cat > ~/.shurium/shurium.conf << 'EOF'
# ============================================================================
# SHURIUM CITIZEN/MINER CONFIGURATION
# ============================================================================

# Connect to your country's network
server=1
listen=1

# Government seed nodes (get these from your government's announcement)
# addnode=seed1.shurium.yourcountry.gov:9333
# addnode=seed2.shurium.yourcountry.gov:9333

# For testing on regtest (local network)
# Remove these lines when connecting to real network
regtest=1

# RPC for local commands
rpcuser=miner
rpcpassword=yourpassword123
rpcallowip=127.0.0.1

EOF
```

---

## Step 3: Create Your Wallet

### 3.1 Start SHURIUM

```bash
# Start the daemon
./shuriumd --daemon

# Wait for startup
sleep 5

# Check status
./shurium-cli getblockchaininfo
```

### 3.2 Create Wallet and Mining Address

```bash
# Create your personal wallet
./shurium-cli createwallet "MyWallet"

# Generate your mining reward address
MINING_ADDRESS=$(./shurium-cli getnewaddress "MiningRewards")
echo "Your Mining Address: $MINING_ADDRESS"

# SAVE THIS ADDRESS! All your mining rewards will go here.
echo $MINING_ADDRESS > ~/my_mining_address.txt
```

### 3.3 Backup Your Wallet (IMPORTANT!)

```bash
# Backup wallet
cp ~/.shurium/wallets/MyWallet/wallet.dat ~/wallet_backup_$(date +%Y%m%d).dat

# Store this backup safely!
echo "Wallet backed up to: ~/wallet_backup_$(date +%Y%m%d).dat"
```

---

## Step 4: Start Mining

### 4.1 Basic Mining Command

```bash
# Start mining to your address
# Replace with YOUR mining address from Step 3
./shurium-cli generatetoaddress 1 shr1qYOUR_MINING_ADDRESS

# To mine continuously (10 blocks at a time)
./shurium-cli generatetoaddress 10 shr1qYOUR_MINING_ADDRESS
```

### 4.2 Create Mining Script

```bash
cat > ~/mine.sh << 'EOF'
#!/bin/bash
# SHURIUM Mining Script

# Your mining address (REPLACE WITH YOUR ADDRESS!)
MINING_ADDRESS="shr1qYOUR_MINING_ADDRESS_HERE"

# Number of blocks to mine per batch
BATCH_SIZE=10

echo "Starting SHURIUM Mining..."
echo "Mining Address: $MINING_ADDRESS"
echo "Batch Size: $BATCH_SIZE blocks"
echo ""

while true; do
    echo "[$(date)] Mining $BATCH_SIZE blocks..."
    RESULT=$(./shurium-cli generatetoaddress $BATCH_SIZE $MINING_ADDRESS 2>&1)
    
    if [ $? -eq 0 ]; then
        BLOCK_COUNT=$(./shurium-cli getblockcount)
        BALANCE=$(./shurium-cli getbalance)
        echo "[$(date)] Success! Block height: $BLOCK_COUNT, Your balance: $BALANCE SHR"
    else
        echo "[$(date)] Mining paused: $RESULT"
        sleep 10
    fi
    
    # Small delay between batches
    sleep 1
done
EOF

chmod +x ~/mine.sh

# Edit to add your mining address
nano ~/mine.sh
```

### 4.3 Start Mining

```bash
cd /path/to/shurium/build
~/mine.sh
```

---

## Step 5: Monitor Your Rewards

### 5.1 Check Your Balance

```bash
# Check your wallet balance
./shurium-cli getbalance

# Get detailed balance info
./shurium-cli getwalletinfo
```

### 5.2 View Your Transactions

```bash
# List recent transactions
./shurium-cli listtransactions "*" 20

# Get specific transaction details
./shurium-cli gettransaction "TXID_HERE"
```

### 5.3 Create Monitoring Dashboard

```bash
cat > ~/my_mining_status.sh << 'EOF'
#!/bin/bash
# Personal Mining Status Dashboard

clear
echo "╔════════════════════════════════════════════════════════════════════╗"
echo "║               MY SHURIUM MINING DASHBOARD                          ║"
echo "║               $(date)                              ║"
echo "╚════════════════════════════════════════════════════════════════════╝"
echo ""

# Blockchain Status
BLOCK_HEIGHT=$(./shurium-cli getblockcount 2>/dev/null || echo "N/A")
CONNECTIONS=$(./shurium-cli getconnectioncount 2>/dev/null || echo "N/A")

echo "┌────────────────────────────────────────────────────────────────────┐"
echo "│ NETWORK STATUS                                                     │"
echo "├────────────────────────────────────────────────────────────────────┤"
echo "│ Block Height:     $BLOCK_HEIGHT"
echo "│ Peer Connections: $CONNECTIONS"
echo "└────────────────────────────────────────────────────────────────────┘"
echo ""

# Wallet Status
BALANCE=$(./shurium-cli getbalance 2>/dev/null || echo "N/A")
UNCONFIRMED=$(./shurium-cli getunconfirmedbalance 2>/dev/null || echo "0")

echo "┌────────────────────────────────────────────────────────────────────┐"
echo "│ MY WALLET                                                          │"
echo "├────────────────────────────────────────────────────────────────────┤"
echo "│ Confirmed Balance:   $BALANCE SHR"
echo "│ Unconfirmed:         $UNCONFIRMED SHR"
echo "│ Total:               $(echo "$BALANCE + $UNCONFIRMED" | bc) SHR"
echo "└────────────────────────────────────────────────────────────────────┘"
echo ""

# Recent Transactions
echo "┌────────────────────────────────────────────────────────────────────┐"
echo "│ RECENT TRANSACTIONS (Last 5)                                       │"
echo "├────────────────────────────────────────────────────────────────────┤"
./shurium-cli listtransactions "*" 5 2>/dev/null | grep -E '"amount"|"category"|"confirmations"' | head -15
echo "└────────────────────────────────────────────────────────────────────┘"
echo ""

# Fund Status (see where protocol funds are going)
echo "┌────────────────────────────────────────────────────────────────────┐"
echo "│ PROTOCOL FUND STATUS (Your contributions to the nation)            │"
echo "├────────────────────────────────────────────────────────────────────┤"
for fund in ubi contribution ecosystem stability; do
    FUND_BAL=$(./shurium-cli getfundbalance $fund 2>/dev/null | grep '"balance"' | awk -F: '{print $2}' | tr -d ' ,')
    printf "│ %-20s: %15s SHR\n" "$fund" "$FUND_BAL"
done
echo "└────────────────────────────────────────────────────────────────────┘"
echo ""

# Mining Reward Explanation
echo "┌────────────────────────────────────────────────────────────────────┐"
echo "│ HOW YOUR MINING REWARDS ARE DISTRIBUTED                            │"
echo "├────────────────────────────────────────────────────────────────────┤"
echo "│                                                                    │"
echo "│   Per 50 SHR Block Reward:                                         │"
echo "│   ├── 20 SHR (40%) → YOUR WALLET (mining reward)                   │"
echo "│   ├── 15 SHR (30%) → UBI Pool (for all citizens)                   │"
echo "│   ├── 7.5 SHR (15%) → Contribution Fund (civic rewards)            │"
echo "│   ├── 5 SHR (10%) → Ecosystem (infrastructure)                     │"
echo "│   └── 2.5 SHR (5%) → Stability Reserve                             │"
echo "│                                                                    │"
echo "│   Your mining helps fund public services while earning rewards!    │"
echo "│                                                                    │"
echo "└────────────────────────────────────────────────────────────────────┘"

EOF

chmod +x ~/my_mining_status.sh
```

### 5.4 Run Your Dashboard

```bash
cd /path/to/shurium/build
~/my_mining_status.sh
```

---

## Step 6: Understanding Your Contribution

When you mine a block, here's what happens:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    BLOCK REWARD DISTRIBUTION                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   YOU MINE A BLOCK (50 SHR Total Reward)                                │
│   │                                                                     │
│   ├──► 20 SHR (40%) ──► YOUR WALLET                                     │
│   │    This is your direct mining reward!                               │
│   │                                                                     │
│   ├──► 15 SHR (30%) ──► GOVERNMENT UBI POOL                             │
│   │    Distributed to all verified citizens as basic income             │
│   │                                                                     │
│   ├──► 7.5 SHR (15%) ──► CONTRIBUTION FUND                              │
│   │    Rewards citizens who contribute to society                       │
│   │                                                                     │
│   ├──► 5 SHR (10%) ──► ECOSYSTEM FUND                                   │
│   │    Infrastructure, hospitals, schools, development                  │
│   │                                                                     │
│   └──► 2.5 SHR (5%) ──► STABILITY RESERVE                               │
│        Emergency fund for economic stability                            │
│                                                                         │
│   TOTAL: You get 40% + contribute 60% to your nation's funds            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Reference Commands

### For Citizens/Miners

| Command | Description |
|---------|-------------|
| `./shurium-cli getbalance` | Check your wallet balance |
| `./shurium-cli getnewaddress "label"` | Generate new address |
| `./shurium-cli listtransactions "*" 10` | List last 10 transactions |
| `./shurium-cli generatetoaddress 1 ADDRESS` | Mine 1 block |
| `./shurium-cli getblockcount` | Current blockchain height |
| `./shurium-cli getfundbalance ubi` | Check UBI pool balance |
| `./shurium-cli getfundinfo` | View all fund information |
| `./shurium-cli sendtoaddress ADDRESS AMOUNT` | Send SHR to someone |

### For Government Administrators

| Command | Description |
|---------|-------------|
| `./shurium-cli getfundinfo` | View all fund details |
| `./shurium-cli getfundbalance FUND` | Check specific fund balance |
| `./shurium-cli setfundaddress FUND ADDRESS` | Configure fund address |
| `./shurium-cli getblockchaininfo` | Network status |
| `./shurium-cli getpeerinfo` | Connected nodes |
| `./shurium-cli getmininginfo` | Mining statistics |

---

## Troubleshooting

### Common Issues

**1. "Connection refused" error**
```bash
# Make sure daemon is running
./shuriumd --daemon
sleep 5
./shurium-cli getblockcount
```

**2. "WARNING: Using DEFAULT DEMO ADDRESS"**
```bash
# You need to configure fund addresses!
# For government: Edit ~/.shurium/shurium.conf
# For testing: ./shurium-cli setfundaddress ubi YOUR_ADDRESS
```

**3. Balance shows 0 after mining**
```bash
# Check if blocks were mined
./shurium-cli getblockcount

# Check your address received rewards
./shurium-cli listtransactions "*" 5
```

**4. Can't connect to network**
```bash
# Check peer connections
./shurium-cli getconnectioncount

# Add seed nodes manually
./shurium-cli addnode "seed1.example.com:9333" "add"
```

---

## Security Best Practices

### For Government
1. Store wallet backups in hardware security modules (HSM)
2. Use multi-signature for fund addresses
3. Implement key ceremony for fund key generation
4. Regular security audits
5. Monitor fund balances 24/7

### For Citizens
1. Backup wallet.dat immediately after creation
2. Never share your private keys
3. Use strong RPC passwords
4. Keep software updated
5. Verify you're connected to official network

---

## Support

- Documentation: [docs/COMMANDS.md](COMMANDS.md)
- Whitepaper: [docs/WHITEPAPER.md](WHITEPAPER.md)
- GitHub: https://github.com/FidaHussain87/shurium

---

*This guide is part of the SHURIUM project - "Governed by People. Powered by Useful Work."*
