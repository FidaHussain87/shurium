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
| [Wallet](#-wallet-commands) | getbalance, sendtoaddress, getnewaddress... |
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

### getwork

Gets PoUW (Proof of Useful Work) problem.

```bash
./shurium-cli getwork
```

---

### submitwork

Submits PoUW solution.

```bash
./shurium-cli submitwork "PROBLEM_ID" "SOLUTION"
```

---

### listproblems

Lists PoUW problems.

```bash
./shurium-cli listproblems [status]
```

| Status | Description |
|--------|-------------|
| pending | Waiting for solution |
| solved | Completed |
| expired | Timed out |

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

Creates new identity.

```bash
./shurium-cli createidentity "PROOF_DATA"
```

---

### getidentityinfo

Returns identity information.

```bash
./shurium-cli getidentityinfo "ADDRESS"
```

---

### verifyidentity

Verifies identity proof.

```bash
./shurium-cli verifyidentity "IDENTITY_ID" "PROOF"
```

---

### claimubi

Claims UBI for identity.

```bash
./shurium-cli claimubi "IDENTITY_ID"
```

---

### getubiinfo

Returns UBI information.

```bash
./shurium-cli getubiinfo "IDENTITY_ID"
```

---

### getubihistory

Returns UBI claim history.

```bash
./shurium-cli getubihistory "IDENTITY_ID"
```

---

## 🗳️ Governance Commands

### getgovernanceinfo

Returns governance state.

```bash
./shurium-cli getgovernanceinfo
```

---

### listproposals

Lists governance proposals.

```bash
./shurium-cli listproposals [status]
```

| Status | Description |
|--------|-------------|
| voting | Currently voting |
| passed | Approved |
| rejected | Failed |
| all | All proposals |

---

### getproposal

Returns proposal details.

```bash
./shurium-cli getproposal "PROPOSAL_ID"
```

---

### createproposal

Creates new proposal.

```bash
./shurium-cli createproposal TYPE "TITLE" "DESCRIPTION" DEPOSIT
```

| Type | Description |
|------|-------------|
| parameter | Change network parameter |
| upgrade | Software upgrade |
| treasury | Spend from treasury |
| text | Non-binding proposal |

**Example:**
```bash
./shurium-cli createproposal parameter "Increase Block Size" "Proposal to increase max block size" 1000
```

---

### vote

Votes on proposal.

```bash
./shurium-cli vote "PROPOSAL_ID" OPTION
```

| Option | Description |
|--------|-------------|
| yes | Support |
| no | Oppose |
| abstain | No opinion |
| veto | Strong opposition |

**Example:**
```bash
./shurium-cli vote "prop123" yes
```

---

### getvoteinfo

Returns vote information.

```bash
./shurium-cli getvoteinfo "PROPOSAL_ID"
```

---

### listparameters

Lists governance parameters.

```bash
./shurium-cli listparameters
```

---

## 💰 Fund Management Commands

SHURIUM allocates 60% of every block reward to protocol funds that support the ecosystem. These funds are secured by 2-of-3 multisig addresses.

### Block Reward Distribution

| Recipient | Percentage | Purpose |
|-----------|------------|---------|
| Miner | 40% | Reward for useful work |
| UBI Pool | 30% | Universal Basic Income distribution |
| Contribution Fund | 15% | Human contribution rewards |
| Ecosystem Fund | 10% | Development and ecosystem growth |
| Stability Reserve | 5% | Price stability mechanism |

### getfundinfo

Returns information about all protocol funds including addresses, governance rules, and multisig details.

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
    }
    // ... other funds
  ],
  "total_fund_percentage": 60,
  "miner_percentage": 40
}
```

---

### getfundbalance

Returns the balance for a specific fund.

```bash
./shurium-cli getfundbalance "FUNDTYPE"
```

| Fund Type | Description |
|-----------|-------------|
| `ubi` | Universal Basic Income pool |
| `contribution` | Human contribution rewards |
| `ecosystem` | Development and ecosystem growth |
| `stability` | Price stability reserve |

**Example:**
```bash
./shurium-cli getfundbalance ubi
```

**Output:**
```json
{
  "fund": "UBI Pool",
  "address": "shr1q902acb56b202b7f43cf605da3ad7aa1e5abbb9fb",
  "balance": 15000.00000000,
  "total_received": 15000.00000000,
  "total_spent": 0.00000000
}
```

---

### listfundtransactions

Lists transactions for a specific fund.

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

---

### getfundaddress

Returns fund addresses with full multisig details including public keys and redeem script.

```bash
./shurium-cli getfundaddress [fundtype]
```

If `fundtype` is omitted, returns all fund addresses.

**Example:**
```bash
./shurium-cli getfundaddress ubi
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

---

### Fund Governance

Each fund has different governance requirements:

| Fund | Governance Vote Required | Max Spend Without Vote |
|------|--------------------------|------------------------|
| UBI Pool | Yes (always) | 0 SHR |
| Contribution Fund | No | 1,000 SHR |
| Ecosystem Fund | No | 5,000 SHR |
| Stability Reserve | Yes (always) | 0 SHR |

### Multisig Key Holders

Each fund is controlled by a 2-of-3 multisig with three key holders:

| Role | Description |
|------|-------------|
| Governance | Elected governance council member |
| Foundation | SHURIUM foundation key |
| Community | Community-elected guardian |

Any 2 of these 3 key holders must sign to spend from a fund.

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
