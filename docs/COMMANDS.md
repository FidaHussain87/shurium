# SHURIUM Command Reference

## Complete List of All Commands

---

## Quick Navigation

| Category | Commands |
|----------|----------|
| [Starting the Daemon](#starting-the-daemon) | How to start, stop, and manage shuriumd |
| [Wallet Tool](#wallet-tool-shurium-wallet-tool) | create, import, restore, backup wallets |
| [Blockchain](#-blockchain-commands) | getblockchaininfo, getblock, getblockhash... |
| [Network](#-network-commands) | getnetworkinfo, getpeerinfo, addnode... |
| [Wallet](#-wallet-commands) | getbalance, sendtoaddress, sendfrom, getnewaddress... |
| [Mining](#️-mining-commands) | getmininginfo, setgenerate, getwork... |
| [Fund Management](#-fund-management-commands) | getfundinfo, getfundbalance, getfundaddress... |
| [Staking](#-staking-commands) | getstakinginfo, delegate, createvalidator... |
| [Identity/UBI](#-identityubi-commands) | createidentity, claimubi, getidentityinfo... |
| [Governance](#️-governance-commands) | listproposals, vote, createproposal... |
| [Utility](#-utility-commands) | help, stop, validateaddress... ||

---

## Starting the Daemon

### Understanding the Two Programs

SHURIUM uses two programs that work together:

| Program | What It Does | When to Use |
|---------|--------------|-------------|
| `shuriumd` | The daemon (server) - runs in background, manages blockchain, wallet, network | Start once, let it run |
| `shurium-cli` | The client - sends commands to the daemon | Use anytime to interact |

### Choosing Your Network (IMPORTANT!)

SHURIUM has different networks for different purposes:

| Network | Flag | Money | Purpose | Wallet Location |
|---------|------|-------|---------|-----------------|
| **Mainnet** | (none) | **REAL** | Actual transactions | `~/.shurium/wallet.dat` |
| **Testnet** | `--testnet` | Fake | Learning & testing | `~/.shurium/testnet/wallet.dat` |
| **Regtest** | `--regtest` | Fake | Development | `~/.shurium/regtest/wallet.dat` |

**Important:** Mainnet and Testnet are completely separate! They have different wallets, different addresses, and different blockchains.

### When to Use Each Network

| Your Goal | Network | Flag |
|-----------|---------|------|
| Send/receive real money | Mainnet | (none) |
| Learn how SHURIUM works | Testnet | `--testnet` |
| Test before real transactions | Testnet | `--testnet` |
| Develop applications | Testnet or Regtest | `--testnet` or `--regtest` |

### Starting the Daemon

**For Real Money (Mainnet):**

```bash
cd ~/shurium/build

# Start mainnet daemon (NO flag)
nohup ./shuriumd > /dev/null 2>&1 &

# Verify it's running
ps aux | grep shuriumd | grep -v grep

# Use commands (NO --testnet flag)
./shurium-cli getbalance
./shurium-cli getnewaddress
```

**For Practice (Testnet):**

```bash
cd ~/shurium/build

# Start testnet daemon
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Verify it's running
ps aux | grep shuriumd | grep -v grep

# Use commands WITH --testnet flag
./shurium-cli --testnet getbalance
./shurium-cli --testnet getnewaddress
```

### Daemon Management Commands

**Mainnet:**

| What You Want | Command |
|---------------|---------|
| Start daemon (background) | `nohup ./shuriumd > /dev/null 2>&1 &` |
| Start daemon (foreground) | `./shuriumd` |
| Check if running | `ps aux \| grep shuriumd \| grep -v grep` |
| Stop daemon gracefully | `./shurium-cli stop` |
| Force stop daemon | `pkill -f shuriumd` |
| View live logs | `tail -f ~/.shurium/debug.log` |
| View last 50 log lines | `tail -50 ~/.shurium/debug.log` |

**Testnet:** Add `--testnet` to daemon and CLI commands, logs are in `~/.shurium/testnet/debug.log`

### Daemon Startup Options

| Option | Description | Example |
|--------|-------------|---------|
| `--testnet` | Use test network (fake money) | `./shuriumd --testnet` |
| `--regtest` | Use regression test network | `./shuriumd --regtest` |
| `--datadir=` | Custom data directory | `./shuriumd --datadir=/path/to/data` |
| `--conf=` | Custom config file | `./shuriumd --conf=/path/to/shurium.conf` |
| `--daemon` | Fork to background (alternative to nohup) | `./shuriumd --testnet --daemon` |

### Important Ports

| Network | RPC Port | P2P Port |
|---------|----------|----------|
| Mainnet | 8332 | 8333 |
| Testnet | 18332 | 18333 |
| Regtest | 18443 | 18444 |

### Data Directories

| Network | Default Location |
|---------|------------------|
| Mainnet | `~/.shurium/` |
| Testnet | `~/.shurium/testnet/` |
| Regtest | `~/.shurium/regtest/` |

### Common Issues

**"Connection refused" when using shurium-cli:**
- The daemon is not running. Start it first.

**"Database lock" error:**

For Mainnet:
```bash
pkill -f shuriumd
rm -f ~/.shurium/blocks/blocks/index/LOCK
nohup ./shuriumd > /dev/null 2>&1 &
```

For Testnet:
```bash
pkill -f shuriumd
rm -f ~/.shurium/testnet/blocks/blocks/index/LOCK
nohup ./shuriumd --testnet > /dev/null 2>&1 &
```

---

## Wallet Tool (shurium-wallet-tool)

The `shurium-wallet-tool` is a separate program for offline wallet management. Use it to create wallets, restore from recovery phrase, and manage wallets without running the daemon.

### Available Commands

| Command | Description |
|---------|-------------|
| `create` | Create a new wallet with a new 24-word recovery phrase |
| `import` | Restore wallet from existing recovery phrase |
| `info` | Display wallet information |
| `address new` | Generate a new receiving address |
| `address list` | List all wallet addresses |
| `dump` | Export wallet data (master public key) |
| `sign` | Sign a raw transaction offline |
| `verify` | Verify wallet integrity and password |
| `passwd` | Change wallet password |

### Options

| Option | Description |
|--------|-------------|
| `--wallet=<path>` | Path to wallet file (default: ~/.shurium/wallet.dat) |
| `--testnet` | Use testnet instead of mainnet |
| `--words=<n>` | Word count for new mnemonic (12, 15, 18, 21, or 24) |
| `--label=<text>` | Label for new address |
| `--show-seed` | Show recovery phrase (DANGEROUS - only use in private) |

### Create a New Wallet

**For Mainnet (Real Money):**
```bash
# Create wallet with 24-word recovery phrase
./shurium-wallet-tool create --words=24 --wallet=~/.shurium/wallet.dat

# IMPORTANT: Write down the 24 words shown!
# Store them safely - this is your ONLY way to recover funds!
```

**For Testnet (Fake Money):**
```bash
# Create wallet with 24-word recovery phrase
./shurium-wallet-tool create --testnet --words=24 --wallet=~/.shurium/testnet/wallet.dat

# IMPORTANT: Write down the 24 words shown!
```

### Restore Wallet from Recovery Phrase

**For Mainnet (Real Money):**

```bash
# Step 1: Stop daemon if running
./shurium-cli stop
pkill -f shuriumd

# Step 2: Backup old wallet
mv ~/.shurium/wallet.dat ~/.shurium/wallet.dat.old

# Step 3: Import from recovery phrase
./shurium-wallet-tool import --wallet=~/.shurium/wallet.dat

# When prompted:
#   - Enter your 24 words separated by SPACES (not commas)
#   - BIP39 Passphrase: Press Enter if you didn't use one
#   - Wallet Password: Enter a new password

# Step 4: Start daemon
nohup ./shuriumd > /dev/null 2>&1 &

# Step 5: Verify restoration
./shurium-cli listaddresses
./shurium-cli getbalance
```

**For Testnet (Fake Money):**

```bash
# Step 1: Stop daemon if running
./shurium-cli --testnet stop
pkill -f shuriumd

# Step 2: Backup old wallet
mv ~/.shurium/testnet/wallet.dat ~/.shurium/testnet/wallet.dat.old

# Step 3: Import from recovery phrase
./shurium-wallet-tool import --testnet --wallet=~/.shurium/testnet/wallet.dat

# When prompted:
#   - Enter your 24 words separated by SPACES (not commas)
#   - BIP39 Passphrase: Press Enter if you didn't use one
#   - Wallet Password: Enter a new password

# Step 4: Start daemon
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Step 5: Verify restoration
./shurium-cli --testnet listaddresses
./shurium-cli --testnet getbalance
```

### View Wallet Info

**For Mainnet:**
```bash
./shurium-wallet-tool info --wallet=~/.shurium/wallet.dat
```

**For Testnet:**
```bash
./shurium-wallet-tool info --testnet --wallet=~/.shurium/testnet/wallet.dat
```

### Generate New Address (Offline)

**For Mainnet:**
```bash
./shurium-wallet-tool address new --wallet=~/.shurium/wallet.dat --label="Savings"
```

**For Testnet:**
```bash
./shurium-wallet-tool address new --testnet --wallet=~/.shurium/testnet/wallet.dat --label="Savings"
```

### Change Wallet Password

**For Mainnet:**
```bash
./shurium-wallet-tool passwd --wallet=~/.shurium/wallet.dat
```

**For Testnet:**
```bash
./shurium-wallet-tool passwd --testnet --wallet=~/.shurium/testnet/wallet.dat
```

---

## How to Use Commands

### Basic Syntax

```bash
./shurium-cli [options] <command> [params]
```

### Common Options

| Option | Description | Example |
|--------|-------------|---------|
| `--testnet` | Use testnet | `./shurium-cli --testnet getbalance` |
| `--regtest` | Use regtest | `./shurium-cli --regtest getbalance` |
| `-rpcuser=` | RPC username | `-rpcuser=myuser` |
| `-rpcpassword=` | RPC password | `-rpcpassword=mypass` |
| `-rpcport=` | RPC port | `-rpcport=8332` |

### Getting Help

```bash
# List all commands
./shurium-cli help

# Help for specific command
./shurium-cli help <command>
```

---

## 🔗 Blockchain Commands

### getblockchaininfo

Returns information about the blockchain.

```bash
./shurium-cli getblockchaininfo
```

**Returns:**
```json
{
  "chain": "main",
  "blocks": 123456,
  "headers": 123456,
  "bestblockhash": "0000000000000...",
  "difficulty": 12345678.90,
  "verificationprogress": 1.0,
  "chainwork": "0000000000000..."
}
```

---

### getbestblockhash

Returns hash of the best (tip) block.

```bash
./shurium-cli getbestblockhash
```

**Returns:** `"0000000000000abc123..."`

---

### getblockcount

Returns current block height.

```bash
./shurium-cli getblockcount
```

**Returns:** `123456`

---

### getblock

Returns block data for given hash.

```bash
./shurium-cli getblock "BLOCKHASH" [verbosity]
```

| Param | Type | Description |
|-------|------|-------------|
| blockhash | string | The block hash |
| verbosity | number | 0=hex, 1=json, 2=json+tx |

**Example:**
```bash
./shurium-cli getblock "0000000000000abc..." 1
```

---

### getblockhash

Returns block hash at given height.

```bash
./shurium-cli getblockhash HEIGHT
```

**Example:**
```bash
./shurium-cli getblockhash 1000
# Returns: "0000000000000..."
```

---

### getblockheader

Returns block header data.

```bash
./shurium-cli getblockheader "BLOCKHASH" [verbose]
```

---

### getchaintips

Returns information about chain tips.

```bash
./shurium-cli getchaintips
```

---

### getdifficulty

Returns current mining difficulty.

```bash
./shurium-cli getdifficulty
# Returns: 12345678.90123456
```

---

### getmempoolinfo

Returns mempool statistics.

```bash
./shurium-cli getmempoolinfo
```

**Returns:**
```json
{
  "size": 42,
  "bytes": 12345,
  "usage": 45678,
  "maxmempool": 300000000,
  "mempoolminfee": 0.00001
}
```

---

### getrawmempool

Returns all transaction IDs in mempool.

```bash
./shurium-cli getrawmempool [verbose]
```

---

### gettransaction

Returns transaction details (wallet transactions only).

```bash
./shurium-cli gettransaction "TXID" [include_watchonly]
```

---

### getrawtransaction

Returns raw transaction data.

```bash
./shurium-cli getrawtransaction "TXID" [verbose] [blockhash]
```

---

### sendrawtransaction

Broadcasts a raw transaction.

```bash
./shurium-cli sendrawtransaction "HEXSTRING"
```

---

## 🌐 Network Commands

### getnetworkinfo

Returns network state information.

```bash
./shurium-cli getnetworkinfo
```

---

### getpeerinfo

Returns information about connected peers.

```bash
./shurium-cli getpeerinfo
```

---

### getconnectioncount

Returns number of peer connections.

```bash
./shurium-cli getconnectioncount
# Returns: 8
```

---

### addnode

Add/remove/try a node.

```bash
./shurium-cli addnode "IP:PORT" "COMMAND"
```

| Command | Description |
|---------|-------------|
| `add` | Add to list |
| `remove` | Remove from list |
| `onetry` | Try connecting once |

**Example:**
```bash
./shurium-cli addnode "192.168.1.100:8333" add
```

---

### disconnectnode

Disconnect from a peer.

```bash
./shurium-cli disconnectnode "IP:PORT"
```

---

### ping

Ping all connected peers.

```bash
./shurium-cli ping
```

---

## 💰 Wallet Commands

### getwalletinfo

Returns wallet state information.

```bash
./shurium-cli getwalletinfo
```

**Returns:**
```json
{
  "walletname": "wallet.dat",
  "balance": 100.50000000,
  "unconfirmed_balance": 5.00000000,
  "immature_balance": 0.00000000,
  "txcount": 150,
  "keypoolsize": 1000
}
```

---

### getbalance

Returns total available balance.

```bash
./shurium-cli getbalance [account] [minconf]
```

**Example:**
```bash
./shurium-cli getbalance
# Returns: 100.50000000
```

---

### getnewaddress

Creates new receiving address.

```bash
./shurium-cli getnewaddress [label] [address_type]
```

| Param | Description |
|-------|-------------|
| label | Optional label for address |
| address_type | "legacy", "p2sh-segwit", "bech32" |

**Example:**
```bash
./shurium-cli getnewaddress "savings"
# Returns: shr1q8w4j5k6n2m3v4c5...
```

---

### listaddresses

Lists all wallet addresses.

```bash
./shurium-cli listaddresses
```

---

### sendtoaddress

Sends coins to an address.

```bash
./shurium-cli sendtoaddress "ADDRESS" AMOUNT [comment] [comment_to]
```

**Example:**
```bash
./shurium-cli sendtoaddress "shr1qrecipient..." 25.5 "Payment"
```

---

### sendmany

Sends to multiple addresses.

```bash
./shurium-cli sendmany "" '{"addr1":amount1,"addr2":amount2}'
```

**Example:**
```bash
./shurium-cli sendmany "" '{"shr1qa...":10,"shr1qb...":20}'
```

---

### sendfrom

Sends coins from a specific address to another address. Unlike `sendtoaddress` which picks UTXOs from any address in the wallet, `sendfrom` only uses coins from the specified source address. This is essential for fund management, accounting transparency, and address-level control.

```bash
./shurium-cli sendfrom "FROM_ADDRESS" "TO_ADDRESS" AMOUNT [comment]
```

**Arguments:**

| Argument | Type | Required | Description |
|----------|------|----------|-------------|
| from_address | string | Yes | Source address (must be in your wallet with spendable UTXOs) |
| to_address | string | Yes | Destination address (any valid SHURIUM address) |
| amount | numeric | Yes | Amount to send in SHR (e.g., 50 or 50.5) |
| comment | string | No | Optional comment stored in wallet (not on blockchain) |

**Returns:**
```json
"txid_hex_string"   // Transaction ID on success
```

**Examples:**

```bash
# Basic transfer between addresses
./shurium-cli sendfrom "shr1qsource123..." "shr1qdest456..." 100

# Transfer with comment
./shurium-cli sendfrom "shr1qmining..." "shr1qsavings..." 50 "Monthly savings transfer"

# Transfer from fund address
./shurium-cli sendfrom "shr1qubifund..." "shr1qrecipient..." 100 "UBI distribution"

# Regtest example with port
./shurium-cli --regtest --rpcport=18443 sendfrom "shr1qabc..." "shr1qdef..." 25
```

**Use Cases:**

| Use Case | Description |
|----------|-------------|
| **Fund Management** | Spend from specific fund addresses (UBI, Contribution, Ecosystem, Stability) while keeping other funds untouched |
| **Organizational Accounting** | Track spending per address for transparency and audit trails |
| **Segregated Wallets** | Keep funds organized by purpose (operations, savings, payroll, etc.) |
| **Change Control** | Ensure change returns to the source address, maintaining fund isolation |
| **Government/Institutional Use** | Distribute funds from designated treasury addresses with clear provenance |

**How It Works:**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SENDFROM UTXO SELECTION                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Wallet UTXOs:                                                         │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
│   │ Address A   │  │ Address A   │  │ Address B   │  │ Address C   │    │
│   │ 50 SHR      │  │ 30 SHR      │  │ 100 SHR     │  │ 25 SHR      │    │
│   └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘    │
│                                                                         │
│   sendfrom "Address A" "Address X" 60 SHR                               │
│                                                                         │
│   ✓ Uses: Address A UTXOs ONLY (50 + 30 = 80 SHR available)             │
│   ✗ Ignores: Address B, Address C UTXOs                                 │
│                                                                         │
│   Result:                                                               │
│   ├── Output 1: Address X receives 60 SHR                               │
│   ├── Output 2: Address A receives ~19.99 SHR (change)                  │
│   └── Fee: ~0.01 SHR deducted                                           │
│                                                                         │
│   Key Difference from sendtoaddress:                                    │
│   sendtoaddress would pick ANY UTXOs, potentially mixing addresses      │
│   sendfrom GUARANTEES source address isolation                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Transaction Details:**

| Aspect | Behavior |
|--------|----------|
| **Input Selection** | Greedy selection from source address UTXOs only (largest first) |
| **Change Address** | Always returns to the FROM address (not a new wallet address) |
| **Fee Estimation** | ~1000 satoshis base + ~150 satoshis per input |
| **Dust Threshold** | Change outputs < 546 satoshis are dropped (added to fee) |
| **RBF** | Enabled (nSequence = 0xFFFFFFFE) for fee bumping if needed |
| **Version** | Transaction version 2 |

**Error Handling:**

| Error | Cause | Solution |
|-------|-------|----------|
| `Wallet is locked` | Wallet encryption active | Run `walletpassphrase "pass" timeout` first |
| `Invalid from address` | Malformed address string | Check address format (shr1q... or tshr1q...) |
| `Invalid to address` | Malformed destination | Verify destination address |
| `Amount must be positive` | Zero or negative amount | Specify positive amount |
| `No funds available at the specified from address` | Source address has no UTXOs | Verify address has received funds and they're confirmed |
| `Insufficient funds at from address` | Not enough balance | Check available balance; may need to consolidate |
| `Insufficient funds after accounting for fee` | Balance covers amount but not fee | Send slightly less or add funds |

**Verification Workflow:**

```bash
# 1. Check source address balance first
./shurium-cli listunspent 1 9999999 '["shr1qsource..."]'

# 2. Execute transfer
TXID=$(./shurium-cli sendfrom "shr1qsource..." "shr1qdest..." 100)

# 3. Verify transaction
./shurium-cli gettransaction "$TXID"

# 4. Confirm change returned to source
./shurium-cli listunspent 0 9999999 '["shr1qsource..."]'
```

**Comparison with Other Send Commands:**

| Command | Source Selection | Change Address | Use Case |
|---------|------------------|----------------|----------|
| `sendtoaddress` | Any wallet UTXO | New wallet address | General payments |
| `sendfrom` | Specific address only | Source address | Fund management, accounting |
| `sendmany` | Any wallet UTXO | New wallet address | Batch payments |

**Security Considerations:**

- Wallet must be unlocked to sign transaction
- Comment is stored locally only (not broadcast)
- Transaction is broadcast to network immediately on success
- Verify recipient address carefully before sending

---

### listtransactions

Lists wallet transactions.

```bash
./shurium-cli listtransactions [label] [count] [skip]
```

**Example:**
```bash
./shurium-cli listtransactions "*" 10 0
```

---

### listunspent

Lists unspent transaction outputs.

```bash
./shurium-cli listunspent [minconf] [maxconf] [addresses]
```

---

### signmessage

Signs a message with address private key.

```bash
./shurium-cli signmessage "ADDRESS" "MESSAGE"
```

---

### verifymessage

Verifies a signed message.

```bash
./shurium-cli verifymessage "ADDRESS" "SIGNATURE" "MESSAGE"
```

---

### walletlock

Locks the wallet.

```bash
./shurium-cli walletlock
```

---

### walletpassphrase

Unlocks the wallet.

```bash
./shurium-cli walletpassphrase "PASSPHRASE" TIMEOUT [staking_only]
```

| Param | Description |
|-------|-------------|
| passphrase | Your wallet password |
| timeout | Seconds to stay unlocked (0 = until lock) |
| staking_only | If true, only allows staking |

**Example:**
```bash
./shurium-cli walletpassphrase "mypassword" 300
```

---

### backupwallet

Backs up wallet to file.

```bash
./shurium-cli backupwallet "DESTINATION"
```

**Example:**
```bash
./shurium-cli backupwallet "/home/user/wallet-backup.dat"
```

---

### dumpprivkey

Exports private key for address. ⚠️ **DANGEROUS**

```bash
./shurium-cli dumpprivkey "ADDRESS"
```

---

### importprivkey

Imports a private key.

```bash
./shurium-cli importprivkey "PRIVKEY" [label] [rescan]
```

---

### encryptwallet

Encrypts the wallet with password.

```bash
./shurium-cli encryptwallet "PASSPHRASE"
```

⚠️ **Note:** Requires restart after encryption.

---

### walletpassphrasechange

Changes wallet password.

```bash
./shurium-cli walletpassphrasechange "OLD" "NEW"
```

---

### loadwallet

Loads a wallet from a wallet file.

```bash
./shurium-cli loadwallet "FILENAME"
```

**Example:**
```bash
./shurium-cli loadwallet "mywallet.dat"
```

**Returns:**
```json
{
  "name": "mywallet.dat",
  "warning": ""
}
```

---

### createwallet

Creates a new wallet and displays the 24-word recovery phrase.

```bash
./shurium-cli createwallet "WALLET_NAME" [passphrase]
```

| Param | Description |
|-------|-------------|
| wallet_name | Name for the new wallet |
| passphrase | Optional passphrase to encrypt |

**Example:**
```bash
./shurium-cli --regtest createwallet "mywallet" "mypassword"
```

**Returns:**
```json
{
  "name": "mywallet",
  "mnemonic": "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art",
  "warning": "IMPORTANT: Write down these 24 words and store them securely! This is the ONLY way to recover your wallet. Anyone with these words can access your funds."
}
```

⚠️ **CRITICAL:** 
- **Write down the 24 words immediately!** This is your recovery phrase.
- **Store it securely offline** (paper, metal backup, NOT on computer).
- **Never share these words** with anyone - they give full access to your funds.
- This is the **only time** the mnemonic is shown. It cannot be retrieved later.

---

### unloadwallet

Unloads the current wallet.

```bash
./shurium-cli unloadwallet
```

---

### restorewallet

Restores a wallet from a 24-word mnemonic recovery phrase.

```bash
./shurium-cli restorewallet "WALLET_NAME" "MNEMONIC" [passphrase]
```

| Param | Description |
|-------|-------------|
| wallet_name | Name for the restored wallet |
| mnemonic | 24-word recovery phrase (space-separated) |
| passphrase | Optional BIP39 passphrase (if one was used during creation) |

**Example:**
```bash
# Unload current wallet first
./shurium-cli unloadwallet

# Restore from mnemonic
./shurium-cli restorewallet "restored_wallet" "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art"
```

**With BIP39 passphrase:**
```bash
./shurium-cli restorewallet "restored_wallet" "word1 word2 ... word24" "my_bip39_passphrase"
```

**Returns:**
```json
{
  "name": "restored_wallet",
  "path": "/home/user/.shurium/restored_wallet.dat",
  "warning": "Wallet restored successfully. Note: Imported keys from the original wallet (if any) are NOT recovered. You may need to rescan the blockchain to find existing transactions."
}
```

**Error (wrong word count):**
```json
{
  "error": "Invalid mnemonic: expected 24 words, got 12"
}
```

⚠️ **Important Notes:**
- The mnemonic must be exactly **24 words** separated by spaces
- If you used a BIP39 passphrase when creating the wallet, you **must** provide it
- Imported private keys (via `importprivkey`) are NOT restored - only HD-derived keys
- You may need to rescan the blockchain to see existing transactions

---

## ⛏️ Mining Commands

SHURIUM supports CPU mining to earn SHR coins. There are multiple ways to configure and control mining.

### Mining Setup Methods

#### Method 1: Runtime Control with setgenerate (Recommended)

Control mining without restarting the daemon.

**Prerequisites:** Start daemon with `--miningaddress` OR have a wallet loaded.

```bash
# Start daemon with mining address (mining not started yet)
./shuriumd --daemon --miningaddress=shr1qyouraddress...

# Enable mining at runtime
./shurium-cli setgenerate true

# Disable mining
./shurium-cli setgenerate false
```

#### Method 2: Daemon Startup Arguments

Start daemon with mining enabled immediately.

```bash
# Start with mining enabled
./shuriumd --daemon --gen=1 --genthreads=2 --miningaddress=shr1qyouraddress...
```

| Argument | Description |
|----------|-------------|
| `--gen=1` | Enable mining (1=on, 0=off) |
| `--genthreads=N` | Number of mining threads |
| `--miningaddress=ADDR` | Address to receive rewards |

#### Method 3: Configuration File (shurium.conf)

Add to `~/.shurium/shurium.conf`:

```ini
# Mining Configuration
gen=1
genthreads=2
miningaddress=shr1qyouraddress...
```

#### Method 4: Hybrid Approach (Best Practice)

Set address in config, control mining via RPC:

**~/.shurium/shurium.conf:**
```ini
gen=0
miningaddress=shr1qyouraddress...
```

**Then control mining:**
```bash
./shurium-cli setgenerate true   # Start when ready
./shurium-cli setgenerate false  # Stop when done
```

---

### setgenerate

Enable or disable mining at runtime.

```bash
./shurium-cli setgenerate GENERATE [genproclimit]
```

**Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| GENERATE | boolean | Yes | `true` to enable, `false` to disable |
| genproclimit | number | No | Number of threads (-1 for all cores) |

**Examples:**
```bash
# Enable mining
./shurium-cli setgenerate true

# Enable mining with 4 threads
./shurium-cli setgenerate true 4

# Disable mining
./shurium-cli setgenerate false
```

**Returns:**
```json
// Enable success:
{
  "mining": true,
  "message": "Mining enabled"
}

// Already enabled:
{
  "mining": true,
  "message": "Mining is already enabled",
  "hashrate": 5910.50
}

// Disable success:
{
  "mining": false,
  "message": "Mining disabled",
  "blocks_found": 1234
}

// Error (no mining address):
{
  "error": "Mining not available. The daemon was started without mining support. Please restart with --miningaddress=<your_address>"
}
```

---

### getmininginfo

Returns mining state information.

```bash
./shurium-cli getmininginfo
```

**Returns:**
```json
{
  "blocks": 123456,
  "chain": "main",
  "difficulty": 0.00024414,
  "networkhashps": 5910.50,
  "pooledtx": 5,
  "pouw_enabled": true,
  "active_problems": 0,
  "solved_problems": 0
}
```

| Field | Description |
|-------|-------------|
| blocks | Current block height |
| chain | Network (main, test, regtest) |
| difficulty | Current mining difficulty |
| networkhashps | Network hash rate |
| pooledtx | Transactions in mempool |
| pouw_enabled | Proof of Useful Work status |

---

### generatetoaddress

Mine blocks immediately to a specified address. **Only works in regtest mode.**

```bash
./shurium-cli --regtest generatetoaddress NBLOCKS ADDRESS
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| NBLOCKS | Number of blocks to generate (1-10000) |
| ADDRESS | Address to send coinbase rewards to |

**Examples:**
```bash
# Mine 1 block
./shurium-cli --regtest generatetoaddress 1 shr1qexampleaddress...

# Mine 101 blocks (get spendable coins)
ADDR=$(./shurium-cli --regtest getnewaddress)
./shurium-cli --regtest generatetoaddress 101 $ADDR

# Check balance after mining
./shurium-cli --regtest getbalance
```

**Returns:** Array of block hashes
```json
[
  "1a2b3c4d5e6f...",
  "2b3c4d5e6f7a...",
  ...
]
```

**Notes:**
- Coinbase rewards need 100 confirmations before they're spendable
- This command is only available in regtest mode for testing

---

### getblocktemplate

Gets block template for external mining software.

```bash
./shurium-cli getblocktemplate [template_request]
```

---

### submitblock

Submits a mined block to the network.

```bash
./shurium-cli submitblock "HEXDATA"
```

---

## 🧮 Proof of Useful Work (PoUW) Marketplace

SHURIUM's PoUW system replaces wasteful hash-based mining with a marketplace where computational problems are solved for real value. Users submit computational problems (with rewards), and miners solve them to earn rewards.

### Supported Problem Types

| Type | Description | Use Case |
|------|-------------|----------|
| `ml_training` | Machine learning model training | Train neural networks |
| `ml_inference` | ML model inference/benchmarking | Run model predictions |
| `linear_algebra` | Matrix operations | Scientific computing |
| `hash_pow` | Hash-based proof (fallback) | Traditional mining |
| `simulation` | Scientific simulations | Physics, chemistry, etc. |
| `data_processing` | Data ETL/transformation | Big data processing |
| `optimization` | Optimization problems | Find optimal solutions |
| `cryptographic` | Cryptographic computations | Key generation, etc. |
| `custom` | Custom verifiable computation | Any verifiable task |

---

### getmarketplaceinfo

Returns marketplace statistics and configuration.

```bash
./shurium-cli getmarketplaceinfo
```

**Example Output:**
```json
{
  "running": true,
  "statistics": {
    "total_problems": 5,
    "total_solved": 3,
    "total_expired": 1,
    "pending_problems": 1,
    "total_rewards": "250.00000000"
  },
  "configuration": {
    "min_problem_reward": "0.00001000",
    "max_problem_reward": "10.00000000",
    "min_deadline_seconds": 60,
    "max_deadline_seconds": 2592000
  },
  "supported_problem_types": [
    "ml_training", "ml_inference", "linear_algebra", "hash_pow",
    "simulation", "data_processing", "optimization", "cryptographic", "custom"
  ]
}
```

---

### createproblem

Create a new computational problem in the marketplace.

```bash
./shurium-cli createproblem TYPE "DESCRIPTION" REWARD [DEADLINE] [INPUT_DATA_HEX] [PARAMS_JSON] [BONUS]
```

**Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| TYPE | string | Yes | Problem type (see table above) |
| DESCRIPTION | string | Yes | Problem description |
| REWARD | number | Yes | Reward amount in SHR |
| DEADLINE | number | No | Deadline in seconds (default: 86400) |
| INPUT_DATA_HEX | string | No | Input data as hex string |
| PARAMS_JSON | string | No | JSON parameters |
| BONUS | number | No | Bonus reward for early submission |

**Examples:**
```bash
# Create simple optimization problem
./shurium-cli createproblem "optimization" "Find minimum of f(x) = x^2 + 2x + 1" 10 3600

# Create ML training problem with 24-hour deadline
./shurium-cli createproblem "ml_training" "Train sentiment classifier on dataset" 50 86400

# Create problem with input data
./shurium-cli createproblem "data_processing" "Process CSV data" 5 7200 "64617461"
```

**Response:**
```json
{
  "success": true,
  "problemId": "1",
  "hash": "5cb4323036546d8fe68c90f2b41219d0ee666e815b7148dd4bc168f95dbb4dc9",
  "type": "optimization",
  "description": "Find minimum of f(x) = x^2 + 2x + 1",
  "reward": "10.00000000",
  "deadline": 1771515656,
  "expires_in": 3600,
  "status": "pending"
}
```

---

### listproblems

Lists PoUW problems in the marketplace.

```bash
./shurium-cli listproblems [status]
```

| Status | Description |
|--------|-------------|
| `pending` | Waiting for solution (default) |
| `solved` | Completed |
| `expired` | Timed out |
| `all` | All problems |

**Example:**
```bash
./shurium-cli listproblems "pending"
```

**Response:**
```json
[
  {
    "id": "1",
    "hash": "5cb4323036546d8fe68c90f2b41219d0ee666e815b7148dd4bc168f95dbb4dc9",
    "type": "optimization",
    "status": "pending",
    "creator": "shr1q54jrl9vd3dx8jdm2f6gkx3pqkzzjfcemretx85",
    "reward": "10.00000000",
    "deadline": 1771515656,
    "expires_in": 3500,
    "difficulty": {
      "target": 4294967295,
      "estimated_time": 60,
      "operations": 1000000
    }
  }
]
```

---

### getproblem

Get detailed information about a specific problem.

```bash
./shurium-cli getproblem PROBLEM_ID
```

**Example:**
```bash
./shurium-cli getproblem 1
```

---

### getwork

Get a problem to work on. Returns the highest-reward pending problem.

```bash
./shurium-cli getwork
```

**Response:**
```json
{
  "problemId": "5cb4323036546d8fe68c90f2b41219d0ee666e815b7148dd4bc168f95dbb4dc9",
  "problemType": "optimization",
  "difficulty": 1.0,
  "data": "4d696e696d697a652066287829203d20785e32202b203278202b2031",
  "target": "00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
  "expires": 1771515656,
  "reward": "10.00000000",
  "description": "Find minimum of f(x) = x^2 + 2x + 1",
  "id": "1"
}
```

If no work is available:
```json
{
  "problemId": "0000000000000000000000000000000000000000000000000000000000000000",
  "problemType": "none",
  "message": "No work available"
}
```

---

### submitwork

Submit a solution to a problem.

```bash
./shurium-cli submitwork PROBLEM_ID SOLUTION_HEX
```

**Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| PROBLEM_ID | string/number | Yes | Problem ID or hash |
| SOLUTION_HEX | string | Yes | Solution data as hex |

**Example:**
```bash
# Submit solution (x = -1 for minimization problem)
./shurium-cli submitwork 1 "2d31"
```

**Response:**
```json
{
  "accepted": true,
  "solution_id": "1",
  "problem_id": "1",
  "status": "pending_verification",
  "message": "Solution submitted for verification",
  "solver": "shr1q..."
}
```

---

### Complete PoUW Workflow Example

```bash
# 1. Check marketplace status
./shurium-cli getmarketplaceinfo

# 2. Create a computational problem (as problem creator)
./shurium-cli createproblem "optimization" "Find x where f(x)=0 for f(x)=x+5" 10 7200

# 3. List available problems
./shurium-cli listproblems "pending"

# 4. Get work (as miner)
./shurium-cli getwork

# 5. Submit solution (x = -5)
./shurium-cli submitwork 1 "2d35"

# 6. Check problem status
./shurium-cli getproblem 1
```

---

### Mining Rewards and Maturity

- **Block Reward:** Mining rewards are paid in the coinbase transaction
- **Maturity:** Mined coins need **100 confirmations** before they're spendable
- **Check Immature Balance:**
  ```bash
  ./shurium-cli getwalletinfo | grep -E "balance|immature"
  ```

---

### Complete Mining Example

```bash
# 1. Get your mining address
MINING_ADDR=$(./shurium-cli getnewaddress "mining")
echo "Mining address: $MINING_ADDR"

# 2. Add to config for persistence
echo "miningaddress=$MINING_ADDR" >> ~/.shurium/shurium.conf

# 3. Restart daemon (or start fresh)
./shurium-cli stop
sleep 3
./shuriumd --daemon

# 4. Enable mining
./shurium-cli setgenerate true

# 5. Monitor mining
watch -n 10 './shurium-cli getmininginfo; echo ""; ./shurium-cli getwalletinfo | grep balance'

# 6. Stop mining when done
./shurium-cli setgenerate false
```

---

## 🥩 Staking Commands

### getstakinginfo

Returns staking state information.

```bash
./shurium-cli getstakinginfo
```

---

### listvalidators

Lists validators.

```bash
./shurium-cli listvalidators [status]
```

| Status | Description |
|--------|-------------|
| active | Currently validating |
| inactive | Below minimum stake |
| jailed | Penalized |
| all | All validators |

---

### getvalidatorinfo

Returns validator details.

```bash
./shurium-cli getvalidatorinfo "VALIDATOR_ID"
```

---

### createvalidator

Registers as validator.

```bash
./shurium-cli createvalidator AMOUNT COMMISSION_BPS "MONIKER"
```

| Param | Description |
|-------|-------------|
| amount | Stake amount in SHR |
| commission_bps | Commission in basis points (500 = 5%) |
| moniker | Display name |

**Example:**
```bash
./shurium-cli createvalidator 10000 500 "MyValidator"
```

---

### delegate

Delegates stake to validator.

```bash
./shurium-cli delegate "VALIDATOR_ID" AMOUNT
```

**Example:**
```bash
./shurium-cli delegate "val1abc..." 1000
```

---

### undelegate

Removes delegation.

```bash
./shurium-cli undelegate "VALIDATOR_ID" AMOUNT
```

⚠️ **Note:** Unbonding period applies (usually 21 days).

---

### listdelegations

Lists your delegations.

```bash
./shurium-cli listdelegations
```

---

### claimrewards

Claims staking rewards.

```bash
./shurium-cli claimrewards [validator_id]
```

---

### getpendingrewards

Shows unclaimed rewards.

```bash
./shurium-cli getpendingrewards
```

---

### unjailvalidator

Releases jailed validator.

```bash
./shurium-cli unjailvalidator "VALIDATOR_ID"
```

---

## 🆔 Identity/UBI Commands

### createidentity

Creates a new identity for UBI participation. The identity starts in `pending` status and becomes `active` after the activation delay (10 blocks in regtest, 100 in mainnet).

```bash
./shurium-cli createidentity
```

**Example Output:**
```json
{
  "identityId": "fcb78ec755251b5e...",
  "message": "Identity created successfully. It will become active after activation delay.",
  "registrationHeight": 0,
  "status": "pending",
  "treeIndex": 0
}
```

**Notes:**
- Identity secrets are stored in your wallet for later UBI claims
- One identity per wallet
- Identity becomes claimable after activation delay

---

### getidentityinfo

Returns detailed identity information.

```bash
./shurium-cli getidentityinfo "IDENTITY_ID"
```

**Example Output:**
```json
{
  "canClaimUBI": true,
  "identityId": "fcb78ec755251b5e...",
  "identityStatus": "active",
  "registrationHeight": 0,
  "treeIndex": 0,
  "verified": true
}
```

---

### verifyidentity

Verifies identity proof.

```bash
./shurium-cli verifyidentity "IDENTITY_ID" "PROOF"
```

---

### claimubi

Claims UBI for the previous finalized epoch. Each identity can only claim once per epoch.

```bash
./shurium-cli claimubi "IDENTITY_ID" [recipient_address]
```

**Example Output (Success):**
```json
{
  "amount": 150,
  "claimStatus": "Valid",
  "epoch": 2,
  "identityId": "fcb78ec755251b5e...",
  "message": "UBI claimed successfully",
  "success": true
}
```

**Example Output (Double-claim):**
```json
{
  "amount": 0,
  "claimStatus": "DoubleClaim",
  "message": "Already claimed UBI for this epoch",
  "success": false
}
```

**Notes:**
- UBI is claimed from the PREVIOUS epoch (which is finalized)
- Amount = (epoch's UBI pool) / (number of eligible identities)
- Each epoch is 10 blocks in current configuration

---

### getubiinfo

Returns UBI distribution information for an identity.

```bash
./shurium-cli getubiinfo "IDENTITY_ID"
```

**Example Output:**
```json
{
  "activeIdentities": 1,
  "amountPerPerson": 150,
  "averageClaimRate": 50,
  "currentEpoch": 4,
  "eligible": true,
  "identityStatus": "active",
  "poolTotal": 90,
  "totalClaimsAllTime": 2,
  "totalDistributed": 300
}
```

---

### getubihistory

Returns UBI claim history.

```bash
./shurium-cli getubihistory "IDENTITY_ID"
```

---

## 🗳️ Governance Commands

SHURIUM features a complete on-chain democratic governance system that allows stakeholders to propose and vote on changes to network parameters, protocol upgrades, and constitutional amendments.

### Key Features

- **Square-root voting power**: Prevents plutocracy - large holders get diminishing returns
- **Multiple proposal types**: Parameter changes, protocol upgrades, constitutional amendments, emergency actions, signal proposals
- **Quorum requirements**: Different thresholds for different proposal types
- **Vote delegation**: Delegate your voting power to trusted representatives
- **Guardian system**: Emergency veto capability for critical issues

### Voting Requirements

| Requirement | Amount |
|-------------|--------|
| Minimum stake to vote | 100 SHR |
| Minimum stake to create proposal | 10,000 SHR |
| Maximum active proposals per user | 3 |

### Voting Thresholds by Proposal Type

| Type | Quorum | Approval | Voting Period |
|------|--------|----------|---------------|
| Parameter | 10% | 50% | ~3 days (8,640 blocks) |
| Protocol | 20% | 66% | ~14 days (40,320 blocks) |
| Constitutional | 33% | 75% | ~30 days (86,400 blocks) |
| Signal | 10% | 50% | ~3 days (8,640 blocks) |
| Emergency | 5% | 66% | ~1 day (2,880 blocks) |

---

### getgovernanceinfo

Returns comprehensive governance state including voting power statistics and thresholds.

```bash
./shurium-cli getgovernanceinfo
```

**Example Output:**
```json
{
  "enabled": true,
  "votingEnabled": true,
  "currentHeight": 601,
  "totalVotingPower": 108166,
  "voterCount": 1,
  "totalProposals": 1,
  "activeProposals": 1,
  "activeDelegations": 0,
  "minProposalStake": 10000,
  "minVotingStake": 100,
  "maxActiveProposalsPerUser": 3,
  "thresholds": {
    "parameterQuorum": 10,
    "parameterApproval": 50,
    "protocolQuorum": 20,
    "protocolApproval": 66,
    "constitutionalQuorum": 33,
    "constitutionalApproval": 75
  },
  "votingPeriods": {
    "parameterVotingPeriod": 8640,
    "protocolVotingPeriod": 40320,
    "constitutionalVotingPeriod": 86400
  }
}
```

---

### listproposals

Lists governance proposals filtered by status.

```bash
./shurium-cli listproposals [status]
```

| Status | Description |
|--------|-------------|
| `pending` | Submitted but voting not yet started |
| `active` | Currently in voting period |
| `approved` | Passed and awaiting execution |
| `executed` | Successfully executed |
| `rejected` | Failed to meet requirements |
| `expired` | Voting period ended without quorum |
| `all` | All proposals (default) |

**Example:**
```bash
# List all active proposals
./shurium-cli listproposals "active"

# List all proposals
./shurium-cli listproposals "all"
```

---

### getproposal

Returns detailed information about a specific proposal.

```bash
./shurium-cli getproposal "PROPOSAL_ID"
```

**Example Output:**
```json
{
  "proposalId": "ffd0e191084e29d9efe535572c02729c177ed4c99421d46e1f4397b6a0221c4e",
  "type": "signal",
  "title": "Complete Voting Test",
  "description": "Full end-to-end voting test",
  "status": "active",
  "submitHeight": 600,
  "votingStartHeight": 601,
  "votingEndHeight": 9241,
  "currentHeight": 601,
  "deposit": 10000,
  "totalVotingPower": 108166,
  "votes": {
    "yes": 108166,
    "no": 0,
    "abstain": 0,
    "veto": 0,
    "total": 108166
  },
  "progress": {
    "approvalPercent": 100,
    "approvalRequired": 50,
    "hasApproval": true,
    "participationPercent": 100,
    "quorumRequired": 10,
    "hasQuorum": true,
    "isVetoed": false
  },
  "isVotingActive": true,
  "isReadyForExecution": false
}
```

---

### createproposal

Creates a new governance proposal. Requires minimum deposit of 10,000 SHR.

```bash
./shurium-cli createproposal TYPE "TITLE" "DESCRIPTION" DEPOSIT
```

| Type | Description | Use Case |
|------|-------------|----------|
| `signal` | Non-binding community poll | Gauge community sentiment |
| `parameter` | Change network parameters | Adjust fees, block size, etc. |
| `protocol` | Protocol upgrade proposal | Software upgrades, new features |
| `constitutional` | Fundamental rule changes | Core governance changes |
| `emergency` | Urgent action required | Security responses |

**Examples:**
```bash
# Create a signal proposal
./shurium-cli createproposal signal "Increase Block Size" "Proposal to increase max block size for better scalability" 10000

# Create a parameter change proposal
./shurium-cli createproposal parameter "Lower Minimum Fee" "Reduce MinTransactionFee from 1000 to 500" 10000
```

**Response:**
```json
{
  "proposalId": "ffd0e191084e29d9efe535572c02729c177ed4c99421d46e1f4397b6a0221c4e",
  "type": "signal",
  "title": "Increase Block Size",
  "status": "pending",
  "submitHeight": 600,
  "votingStartHeight": 601,
  "votingEndHeight": 9241,
  "deposit": 10000
}
```

---

### vote

Cast your vote on an active proposal. Your voting power is automatically calculated based on your wallet balance using square-root scaling.

```bash
./shurium-cli vote "PROPOSAL_ID" CHOICE ["REASON"]
```

| Choice | Description |
|--------|-------------|
| `yes` | Support the proposal |
| `no` | Oppose the proposal |
| `abstain` | No opinion (counts toward quorum) |
| `veto` | Strong opposition (>33% vetoes = rejected) |

**Examples:**
```bash
# Vote yes with a reason
./shurium-cli vote "ffd0e191..." yes "I support better scalability"

# Vote no
./shurium-cli vote "ffd0e191..." no

# Abstain (still counts toward quorum)
./shurium-cli vote "ffd0e191..." abstain
```

**Response:**
```json
{
  "success": true,
  "proposalId": "ffd0e191...",
  "voter": "347261676e6568366b6e6c6b3765657131726873",
  "choice": "yes",
  "votingPower": 108166,
  "voteHeight": 601,
  "currentApprovalPercent": 100,
  "currentParticipationPercent": 100
}
```

**Note:** Your voting power is calculated as `sqrt(balance) * 1000`. This means:
- 100 SHR = 10,000 voting power
- 10,000 SHR = 100,000 voting power
- 1,000,000 SHR = 1,000,000 voting power

---

### getvoteinfo

Returns vote information for a proposal, optionally for a specific voter.

```bash
./shurium-cli getvoteinfo "PROPOSAL_ID" ["VOTER_ID"]
```

**Example:**
```bash
# Get overall vote info
./shurium-cli getvoteinfo "ffd0e191..."

# Get specific voter's vote
./shurium-cli getvoteinfo "ffd0e191..." "347261676e6568366b6e6c6b3765657131726873"
```

---

### listparameters

Lists all governable parameters with their current values and constraints.

```bash
./shurium-cli listparameters
```

**Example Output (partial):**
```json
[
  {
    "name": "TransactionFeeMultiplier",
    "value": 100,
    "type": "integer",
    "modifiable": true,
    "description": "Fee multiplier (100 = 1x)"
  },
  {
    "name": "BlockSizeLimit",
    "value": 4194304,
    "type": "integer",
    "modifiable": true,
    "description": "Maximum block size in bytes (4MB)"
  },
  {
    "name": "UBIDistributionRate",
    "value": 1000,
    "type": "integer",
    "modifiable": true,
    "description": "UBI rate in basis points (1000 = 10%)"
  }
]
```

### Governable Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `TransactionFeeMultiplier` | 100 | Fee multiplier (100 = 1x) |
| `BlockSizeLimit` | 4,194,304 | Max block size (4MB) |
| `MinTransactionFee` | 1,000 | Minimum tx fee in base units |
| `BlockRewardAdjustment` | 100 | Block reward adjustment (bps) |
| `UBIDistributionRate` | 1,000 | UBI rate (bps, 10%) |
| `OracleMinStake` | 10,000 SHR | Minimum oracle stake |
| `OracleSlashingRate` | 500 | Oracle slashing (bps, 5%) |
| `TreasuryAllocationDev` | 30 | Dev treasury (30%) |
| `TreasuryAllocationSecurity` | 15 | Security treasury (15%) |
| `TreasuryAllocationMarketing` | 10 | Marketing treasury (10%) |
| `StabilityFeeRate` | 50 | Stability fee (bps, 0.5%) |
| `PriceDeviationThreshold` | 500 | Price deviation (bps, 5%) |
| `ProposalDepositAmount` | 1,000 SHR | Proposal deposit |
| `VotingPeriodBlocks` | 8,640 | Default voting period |

---

### Complete Governance Workflow Example

```bash
# 1. Check your balance (need 100+ SHR to vote, 10,000+ to propose)
./shurium-cli getbalance

# 2. View current governance state
./shurium-cli getgovernanceinfo

# 3. List governable parameters
./shurium-cli listparameters

# 4. Create a signal proposal
./shurium-cli createproposal signal "Lower Transaction Fees" "Proposal to reduce minimum transaction fee from 1000 to 500 base units" 10000

# 5. Wait one block for voting to start
./shurium-cli generatetoaddress 1 "YOUR_ADDRESS"

# 6. Vote on the proposal
./shurium-cli vote "PROPOSAL_ID" yes "Lower fees will increase adoption"

# 7. Check proposal status
./shurium-cli getproposal "PROPOSAL_ID"

# 8. List all active proposals
./shurium-cli listproposals "active"
```

---

## 💰 Fund Management Commands

SHURIUM allocates 60% of every block reward to protocol funds that support the ecosystem. These funds are secured by 2-of-3 multisig addresses and are automatically funded with every new block.

### Block Reward Distribution

| Recipient | Percentage | Purpose |
|-----------|------------|---------|
| Miner | 40% | Reward for useful work |
| UBI Pool | 30% | Universal Basic Income distribution |
| Contribution Fund | 15% | Human contribution rewards |
| Ecosystem Fund | 10% | Development and ecosystem growth |
| Stability Reserve | 5% | Price stability mechanism |

### Understanding Fund Addresses

> **CRITICAL WARNING: DEFAULT ADDRESSES ARE FOR DEMO ONLY**
> 
> When you first run SHURIUM, all fund addresses are **DEFAULT DEMO ADDRESSES**. These addresses are generated from public seed strings like `"SHURIUM_regtest_UBI_KEY_GOVERNANCE_V1"`.
>
> **NOBODY CONTROLS THE PRIVATE KEYS FOR DEFAULT ADDRESSES!**
>
> Any SHR sent to default addresses is **permanently lost/unspendable**. Before mining or using SHURIUM in production:
> 1. Configure your own addresses using `setfundaddress` or `shurium.conf`
> 2. Verify with `getfundinfo` that `address_source` shows `RPC command` or `configuration file`
> 3. Never mine to default addresses in production

Each fund has a **deterministic P2SH (Pay-to-Script-Hash) multisig address** that:
- Is generated from seed strings unique to each network (mainnet/testnet/regtest)
- Remains consistent across node restarts
- Requires 2-of-3 signatures to spend

```
┌─────────────────────────────────────────────────────────────────────┐
│                      FUND SECURITY MODEL                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Each fund address is controlled by 3 KEY HOLDERS:                 │
│                                                                     │
│   🏛️  GOVERNANCE KEY  ─┐                                            │
│                        │                                            │
│   🏢 FOUNDATION KEY   ─┼─► ANY 2 of 3 must sign to spend            │
│                        │                                            │
│   👥 COMMUNITY KEY    ─┘                                            │
│                                                                     │
│   This prevents:                                                    │
│   • Single point of failure                                         │
│   • Unilateral fund access                                          │
│   • Theft by compromising one key                                   │
│                                                                     │
│   ⚠️  DEFAULT ADDRESSES: Keys are derived from PUBLIC seeds!        │
│       Configure your own addresses before production use.           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

### getfundinfo

Returns comprehensive information about all protocol funds including addresses, governance rules, and multisig details.

```bash
./shurium-cli getfundinfo
```

**Example Output:**
```json
{
  "funds": [
    {
      "name": "UBI Pool",
      "percentage": 30,
      "address": "shr1q902acb56b202b7f43cf605da3ad7aa1e5abbb9fb",
      "multisig_required": 2,
      "multisig_total": 3,
      "requires_governance_vote": true,
      "max_spend_without_vote": 0
    },
    {
      "name": "Contribution Fund",
      "percentage": 15,
      "address": "shr1q2e62cce363b43714d1a34472a7e1f80d7194072c",
      "multisig_required": 2,
      "multisig_total": 3,
      "requires_governance_vote": false,
      "max_spend_without_vote": 1000
    },
    {
      "name": "Ecosystem Fund",
      "percentage": 10,
      "address": "shr1q4354e7b2a4b8b9fbd98a8bffc620a52d2c31fdab",
      "multisig_required": 2,
      "multisig_total": 3,
      "requires_governance_vote": false,
      "max_spend_without_vote": 5000
    },
    {
      "name": "Stability Reserve",
      "percentage": 5,
      "address": "shr1qd1d806d087f75504d925d502a3629941fc55af2f",
      "multisig_required": 2,
      "multisig_total": 3,
      "requires_governance_vote": true,
      "max_spend_without_vote": 0
    }
  ],
  "total_fund_percentage": 60,
  "miner_percentage": 40
}
```

---

### getfundbalance

Returns the balance for a specific fund, calculated from block rewards received.

```bash
./shurium-cli getfundbalance <fundtype>
```

**Parameters:**

| Parameter | Required | Description |
|-----------|----------|-------------|
| `fundtype` | **Yes** | The fund to check (see valid values below) |

**Valid Fund Types:**

| Fund Type | Aliases | Description |
|-----------|---------|-------------|
| `ubi` | `UBI` | Universal Basic Income pool |
| `contribution` | `contributions` | Human contribution rewards |
| `ecosystem` | `eco` | Development and ecosystem growth |
| `stability` | `reserve` | Price stability reserve |

**Examples:**
```bash
# Check UBI Pool balance
./shurium-cli getfundbalance ubi

# Check Contribution Fund balance
./shurium-cli getfundbalance contribution

# Check Ecosystem Fund balance
./shurium-cli getfundbalance ecosystem

# Check Stability Reserve balance
./shurium-cli getfundbalance stability
```

**Output (with default demo address - shows WARNING):**
```json
{
  "WARNING": "Using DEFAULT DEMO ADDRESS. These addresses are generated from public seeds - NOBODY controls the private keys! Any funds sent here are UNSPENDABLE. For production, use 'setfundaddress' or configure addresses in shurium.conf.",
  "fund": "UBI Pool",
  "address": "shr1q902acb56b202b7f43cf605da3ad7aa1e5abbb9fb",
  "address_source": "default (demo)",
  "is_custom_address": false,
  "balance": 0.00000000,
  "total_received": 0.00000000,
  "total_spent": 0.00000000,
  "chain_height": 0,
  "note": "Balance calculated from block rewards (no spending tracked yet)."
}
```

**Output (after setting custom address - no WARNING):**
```json
{
  "fund": "UBI Pool",
  "address": "shr1qyourorganizationaddress12345...",
  "address_source": "RPC command",
  "is_custom_address": true,
  "balance": 15375.00000000,
  "total_received": 15375.00000000,
  "total_spent": 0.00000000,
  "chain_height": 1050,
  "note": "Balance calculated from block rewards (no spending tracked yet)."
}
```

**Common Error:**
```bash
# ERROR: Missing required parameter
./shurium-cli getfundbalance
# Returns: error code: -32603, Missing required parameter at index 0

# CORRECT: Specify fund type
./shurium-cli getfundbalance ubi
```

**How Balance is Calculated:**

The balance is calculated based on:
1. Current blockchain height
2. Block reward at each halving period
3. Fund percentage allocation

```
Example at height 1050 (regtest with 1000 block halving):
├── Blocks 1-1000:    1000 × 50 SHR × 30% = 15,000 SHR
├── Blocks 1001-1050:   50 × 25 SHR × 30% =    375 SHR (after halving)
└── Total UBI Balance: 15,375 SHR
```

---

### getfundaddress

Returns fund addresses with full multisig details including public keys and redeem script.

```bash
./shurium-cli getfundaddress [fundtype]
```

If `fundtype` is omitted, returns all fund addresses.

**Example - Get specific fund:**
```bash
./shurium-cli getfundaddress ubi
```

**Example - Get all funds:**
```bash
./shurium-cli getfundaddress
```

**Output:**
```json
[
  {
    "fund": "UBI Pool",
    "type": "UBI Pool",
    "address": "shr1q902acb56b202b7f43cf605da3ad7aa1e5abbb9fb",
    "multisig": "2-of-3",
    "pubkeys": [
      "038092b7621df265c8da7548afcf29b5a303d974cb2654c19316aa7b336afe4f07",
      "0290a7400f39f9bb796ddc13d156d5093060856456472b9bda128d3b5f4285cb51",
      "0395f38c70806c1e080d5d674f6102c1ec097e825fd8ae750c94b0da5c127a3731"
    ],
    "redeem_script": "52210290a7400f39f9bb796ddc13d156d5093060856456472b9bda128d3b5f4285cb51..."
  }
]
```

**Understanding the Output:**

| Field | Description |
|-------|-------------|
| `fund` | Human-readable fund name |
| `address` | P2SH multisig address receiving block rewards |
| `multisig` | Signature requirement (e.g., "2-of-3") |
| `pubkeys` | Array of 3 public keys controlling the fund |
| `redeem_script` | The raw redeem script (for advanced users) |

---

### listfundtransactions

Lists transactions for a specific fund (incoming block rewards).

```bash
./shurium-cli listfundtransactions "FUNDTYPE" [count] [skip]
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| fundtype | Fund type (ubi, contribution, ecosystem, stability) | Required |
| count | Number of transactions to return | 10 |
| skip | Number of transactions to skip | 0 |

**Example:**
```bash
./shurium-cli listfundtransactions ecosystem 20 0
```

**Output:**
```json
{
  "fund": "Ecosystem Fund",
  "address": "shr1q4354e7b2a4b8b9fbd98a8bffc620a52d2c31fdab",
  "transactions": [
    {
      "txid": "abc123...",
      "block_height": 1050,
      "amount": 2.50000000,
      "type": "block_reward"
    }
  ],
  "total_count": 1050,
  "note": "Full transaction listing requires UTXO scanning (not yet implemented)."
}
```

---

### Viewing All Fund Balances

To get a quick overview of all fund balances:

```bash
# View all fund balances at once
echo "=== SHURIUM Fund Balances ===" && \
./shurium-cli getfundbalance ubi && \
./shurium-cli getfundbalance contribution && \
./shurium-cli getfundbalance ecosystem && \
./shurium-cli getfundbalance stability
```

Or create a simple script:

```bash
#!/bin/bash
# fund-status.sh - Display all fund balances

echo "╔════════════════════════════════════════════════════════════╗"
echo "║              SHURIUM PROTOCOL FUND STATUS                  ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

for fund in ubi contribution ecosystem stability; do
    ./shurium-cli getfundbalance $fund 2>/dev/null | \
    grep -E '"fund"|"balance"|"address"' | \
    sed 's/[",]//g' | sed 's/^/  /'
    echo "---"
done
```

---

### Fund Governance Rules

Each fund has different governance requirements for spending:

| Fund | Governance Vote Required | Max Spend Without Vote | Typical Use |
|------|--------------------------|------------------------|-------------|
| **UBI Pool** | Yes (always) | 0 SHR | UBI distribution to verified humans |
| **Contribution Fund** | No | 1,000 SHR | Bounties, contributor rewards |
| **Ecosystem Fund** | No | 5,000 SHR | Grants, partnerships, development |
| **Stability Reserve** | Yes (always) | 0 SHR | Buy/sell SHR for price stability |

### Multisig Key Holders

Each fund is controlled by a 2-of-3 multisig with three key holders:

| Role | Description | Selection Method |
|------|-------------|------------------|
| 🏛️ **Governance** | Elected governance council member | Community vote |
| 🏢 **Foundation** | SHURIUM foundation key | Foundation board |
| 👥 **Community** | Community-elected guardian | Community vote |

**To spend from any fund, any 2 of these 3 key holders must sign the transaction.**

---

### Fund Address Prefixes by Network

| Network | Address Prefix | Example |
|---------|---------------|---------|
| Mainnet | `shr1q...` | `shr1q902acb56b202b7f43cf605da3ad7aa1e5abbb9fb` |
| Testnet | `tshr1q...` | `tshr1q902acb56b202b7f43cf605da3ad7aa1e5abbb9fb` |
| Regtest | `shr1q...` | `shr1q902acb56b202b7f43cf605da3ad7aa1e5abbb9fb` |

---

### Verifying Fund Addresses

You can verify that a coinbase transaction includes the correct fund outputs:

```bash
# Get the latest block
BLOCKHASH=$(./shurium-cli getbestblockhash)

# Get block details with transactions
./shurium-cli getblock $BLOCKHASH 2

# The coinbase transaction (first tx) should have 5 outputs:
# Output 0: Miner reward (40%)
# Output 1: UBI Pool (30%)
# Output 2: Contribution Fund (15%)
# Output 3: Ecosystem Fund (10%)
# Output 4: Stability Reserve (5%)
```

---

### setfundaddress

**For Organizations/Governments:** Set a custom address for a fund to receive block rewards.

```bash
./shurium-cli setfundaddress <fundtype> <address>
```

**Parameters:**

| Parameter | Required | Description |
|-----------|----------|-------------|
| `fundtype` | **Yes** | Fund type (ubi, contribution, ecosystem, stability) |
| `address` | **Yes** | Your organization's address to receive rewards |

**Example:**
```bash
# Set custom UBI address for your organization
./shurium-cli setfundaddress ubi shr1qyourorganizationaddress12345abcdef

# Set custom ecosystem fund address
./shurium-cli setfundaddress ecosystem shr1qecosystemfundaddress67890ghijkl
```

**Output:**
```json
{
  "success": true,
  "fund": "UBI Pool",
  "new_address": "shr1qyourorganizationaddress12345abcdef",
  "address_source": "RPC command",
  "warning": "This change is temporary (RPC command). For persistent changes, add to shurium.conf: ubiaddress=shr1q..."
}
```

**Important Notes:**
- RPC changes persist **within the current daemon session** (across multiple RPC calls)
- RPC changes are **lost on daemon restart** - use `shurium.conf` for permanent changes
- Genesis block addresses **cannot** be overridden (except via governance)
- You can set multiple fund addresses in a single session:
  ```bash
  ./shurium-cli setfundaddress ubi shr1q...
  ./shurium-cli setfundaddress ecosystem shr1q...
  ./shurium-cli getfundinfo  # Both changes are visible
  ```

---

### Configuring Fund Addresses (For Organizations)

SHURIUM supports multiple ways to configure fund recipient addresses. This is essential for governments, organizations, or private networks that want to control their own fund distribution.

#### Address Source Priority (Highest to Lowest)

| Priority | Source | Persistence | Use Case |
|----------|--------|-------------|----------|
| 1 | Genesis Block | Permanent (immutable) | Production mainnet |
| 2 | Governance Vote | Until next vote | Democratic changes |
| 3 | Configuration File | Until config change | Organizations, private networks |
| 4 | RPC Command | Until daemon restart | Testing, temporary changes |
| 5 | Default | Always available | Demo, development |

#### Method 1: Configuration File (Recommended for Organizations)

Add fund addresses to `~/.shurium/shurium.conf`:

```ini
# Fund recipient addresses
# Replace with your organization's actual addresses

# UBI Pool (30% of block rewards)
ubiaddress=shr1qyour_ubi_address_here

# Contribution Fund (15% of block rewards)
contributionaddress=shr1qyour_contribution_address_here

# Ecosystem Fund (10% of block rewards)
ecosystemaddress=shr1qyour_ecosystem_address_here

# Stability Reserve (5% of block rewards)
stabilityaddress=shr1qyour_stability_address_here
```

Then restart the daemon:
```bash
./shurium-cli stop
./shuriumd --daemon
```

#### Method 2: RPC Command (For Testing)

Use RPC commands for quick testing. Changes persist within the daemon session but are lost on restart.

```bash
# Set addresses at runtime (persists until daemon restart)
./shurium-cli setfundaddress ubi shr1q...
./shurium-cli setfundaddress contribution shr1q...
./shurium-cli setfundaddress ecosystem shr1q...
./shurium-cli setfundaddress stability shr1q...

# Verify changes - all 4 addresses will show "RPC command" as source
./shurium-cli getfundinfo
```

#### Verifying Address Configuration

```bash
# Check address source for each fund
./shurium-cli getfundinfo
```

Output includes `address_source` field:
```json
{
  "funds": [
    {
      "name": "UBI Pool",
      "address": "shr1qyour_ubi_address_here",
      "address_source": "configuration file",
      "is_custom_address": true
    }
  ]
}
```

#### Example: Government UBI Distribution Setup

A government wanting to distribute UBI would:

1. **Create addresses** they control for each fund:
   ```bash
   # Generate addresses using your organization's wallet
   ./shurium-cli getnewaddress "Government UBI Pool"
   ./shurium-cli getnewaddress "Government Contribution Fund"
   # etc.
   ```

2. **Configure in shurium.conf**:
   ```ini
   ubiaddress=shr1qgovernment_ubi_12345...
   contributionaddress=shr1qgovernment_contrib_67890...
   ecosystemaddress=shr1qgovernment_eco_abcdef...
   stabilityaddress=shr1qgovernment_stability_ghijk...
   ```

3. **Start the daemon** - all block rewards will now flow to your addresses

4. **Verify setup**:
   ```bash
   ./shurium-cli getfundinfo
   # Check that address_source shows "configuration file"
   ```

---

## 🔧 Utility Commands

### help

Shows help.

```bash
./shurium-cli help [command]
```

---

### stop

Stops the daemon.

```bash
./shurium-cli stop
```

---

### uptime

Returns daemon uptime.

```bash
./shurium-cli uptime
```

---

### validateaddress

Validates an address.

```bash
./shurium-cli validateaddress "ADDRESS"
```

---

### estimatefee

Estimates transaction fee.

```bash
./shurium-cli estimatefee NBLOCKS
```

**Example:**
```bash
./shurium-cli estimatefee 6
# Returns fee rate for confirmation in ~6 blocks
```

---

### getinfo (deprecated)

Returns various state info. Use specific commands instead.

---

## 📊 Quick Reference Tables

### Most Common Commands

| Task | Command |
|------|---------|
| Check balance | `getbalance` |
| Send coins | `sendtoaddress "addr" amount` |
| Send from address | `sendfrom "from" "to" amount` |
| New address | `getnewaddress "label"` |
| Unlock wallet | `walletpassphrase "pass" 300` |
| Start mining | `setgenerate true 4` |
| Check sync | `getblockchaininfo` |
| Delegate stake | `delegate "val_id" amount` |
| Claim UBI | `claimubi "identity_id"` |
| Vote | `vote "prop_id" yes` |

### Status Check Commands

| What | Command |
|------|---------|
| Blockchain | `getblockchaininfo` |
| Network | `getnetworkinfo` |
| Wallet | `getwalletinfo` |
| Mining | `getmininginfo` |
| Staking | `getstakinginfo` |
| Peers | `getpeerinfo` |

---

<div align="center">

**Full documentation for every command!**

Use `./shurium-cli help <command>` for the most current information.

[← Back to README](../README.md)

</div>
