# SHURIUM Command Reference

## Complete List of All Commands

---

## Quick Navigation

| Category | Commands |
|----------|----------|
| [Starting the Daemon](#-starting-the-daemon) | How to start, stop, and manage shuriumd |
| [Wallet Tool](#-wallet-tool-shurium-wallet-tool) | create, import, restore, backup wallets |
| [Blockchain](#-blockchain-commands) | getblockchaininfo, getblock, getblockhash... |
| [Network](#-network-commands) | getnetworkinfo, getpeerinfo, addnode... |
| [Wallet](#-wallet-commands) | getbalance, sendtoaddress, getnewaddress... |
| [Mining](#️-mining-commands) | getmininginfo, setgenerate, getwork... |
| [Staking](#-staking-commands) | getstakinginfo, delegate, createvalidator... |
| [Identity/UBI](#-identityubi-commands) | createidentity, claimubi, getidentityinfo... |
| [Governance](#️-governance-commands) | listproposals, vote, createproposal... |
| [Utility](#-utility-commands) | help, stop, validateaddress... |

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

Creates a new wallet.

```bash
./shurium-cli createwallet "WALLET_NAME" [passphrase]
```

| Param | Description |
|-------|-------------|
| wallet_name | Name for the new wallet |
| passphrase | Optional passphrase to encrypt |

**Example:**
```bash
./shurium-cli createwallet "savings" "mypassword"
```

**Returns:**
```json
{
  "name": "savings",
  "warning": ""
}
```

---

### unloadwallet

Unloads the current wallet.

```bash
./shurium-cli unloadwallet
```

---

## ⛏️ Mining Commands

### getmininginfo

Returns mining state information.

```bash
./shurium-cli getmininginfo
```

**Returns:**
```json
{
  "blocks": 123456,
  "difficulty": 12345.67,
  "networkhashps": 98765432,
  "generate": true,
  "genthreads": 4,
  "hashespersec": 12345
}
```

---

### setgenerate

Enables/disables mining.

```bash
./shurium-cli setgenerate GENERATE [threads]
```

**Examples:**
```bash
# Start mining with 4 threads
./shurium-cli setgenerate true 4

# Stop mining
./shurium-cli setgenerate false
```

---

### getblocktemplate

Gets block template for mining.

```bash
./shurium-cli getblocktemplate [template_request]
```

---

### submitblock

Submits a mined block.

```bash
./shurium-cli submitblock "HEXDATA"
```

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
- Each block rewards 40 SHR to the miner (40% of 100 SHR block reward)
- This command is only available in regtest mode for testing

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
