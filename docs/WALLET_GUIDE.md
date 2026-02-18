# SHURIUM Wallet Guide

## Everything You Need to Know About Your Wallet

---

## Table of Contents

1. [What is a Wallet?](#what-is-a-wallet)
2. [Understanding Keys](#understanding-keys-important) - Private Key, Public Key, Address
3. [Creating Your Wallet](#creating-your-wallet)
4. [Understanding Addresses](#understanding-addresses)
5. [Sending Money](#sending-money)
6. [Receiving Money](#receiving-money)
7. [Security Best Practices](#security-best-practices)
8. [Backup and Recovery](#backup-and-recovery)
9. [Advanced Features](#advanced-features)

---

## What is a Wallet?

### The Simple Explanation

A wallet is **NOT** like a physical wallet that holds cash.

Think of it more like a **keychain** that holds the keys to your money:

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│   PHYSICAL WALLET                    SHURIUM WALLET                   │
│   ──────────────                    ─────────────                   │
│                                                                     │
│   💵 Contains actual cash           🔑 Contains KEYS to your money  │
│                                                                     │
│   💸 Money is inside                💰 Money is on the blockchain   │
│                                                                     │
│   🏃 Thief takes wallet = loses $   🔐 Thief needs your KEY         │
│                                                                     │
│   📍 Limited capacity               ♾️  Unlimited addresses         │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### What's Actually in Your Wallet?

```
┌─────────────────────────────────────────────────────────────────────┐
│                         YOUR WALLET FILE                            │
│                         (~/.shurium/wallet.dat)                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │ 🌱 MASTER SEED                                                │  │
│  │                                                               │  │
│  │ The root of everything. From this one seed, we can generate:  │  │
│  │ • Unlimited private keys                                      │  │
│  │ • Unlimited public addresses                                  │  │
│  │ • Your entire wallet can be recreated from this               │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                              │                                      │
│                              ▼                                      │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │ 🔑 PRIVATE KEYS                                               │  │
│  │                                                               │  │
│  │ Key 1: L4rK1yDtCWekvXuE54F...  → Controls Address 1           │  │
│  │ Key 2: KxFC1jmwwCoACiCAWZ3...  → Controls Address 2           │  │
│  │ Key 3: 5HueCGU8rMjxEXxiPu...   → Controls Address 3           │  │
│  │ ...                                                           │  │
│  │                                                               │  │
│  │ ⚠️  NEVER SHARE THESE - Anyone with a key owns those coins    │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                              │                                      │
│                              ▼                                      │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │ 📫 PUBLIC ADDRESSES                                           │  │
│  │                                                               │  │
│  │ Address 1: shr1q8w4j5k6n2m3v4c5x6z7a8s9d0f1g2h3j4k5l6         │  │
│  │ Address 2: shr1qm9n8b7v6c5x4z3a2s1d0f9g8h7j6k5l4m3n2          │  │
│  │ Address 3: shr1qa1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8          │  │
│  │                                                               │  │
│  │ ✅ SAFE TO SHARE - This is how people send you money          │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │ 📝 LABELS & METADATA                                          │  │
│  │                                                               │  │
│  │ Address 1 → "Main savings"                                    │  │
│  │ Address 2 → "Rent payments"                                   │  │
│  │ Address 3 → "Business income"                                 │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Understanding Keys (IMPORTANT!)

This section explains the most critical concept in cryptocurrency: **keys**.

### The Three Key Components

Your wallet has THREE different but related components:

```
┌─────────────────────────────────────────────────────────────────────┐
│           PRIVATE KEY vs PUBLIC KEY vs ADDRESS                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  PRIVATE KEY              PUBLIC KEY              ADDRESS           │
│  ════════════             ══════════              ═══════           │
│                                                                     │
│  What: Secret number      What: Public point      What: Identifier  │
│  Size: 32 bytes           Size: 33 bytes          Size: 42 chars    │
│  Format: L4rK1yD...       Format: 02a1b2c3...     Format: nx1q...   │
│                                                                     │
│  ┌─────────────┐          ┌─────────────┐         ┌─────────────┐   │
│  │             │          │             │         │             │   │
│  │   SECRET    │    →     │   DERIVED   │    →    │   PUBLIC    │   │
│  │   DO NOT    │  math    │   CAN BE    │  hash   │   SHARE     │   │
│  │   SHARE!    │          │   SHARED    │         │   FREELY    │   │
│  │             │          │             │         │             │   │
│  └─────────────┘          └─────────────┘         └─────────────┘   │
│                                                                     │
│  Purpose:                 Purpose:                Purpose:          │
│  • Sign transactions      • Verify signatures     • Receive money   │
│  • Prove ownership        • Derive address        • Identity        │
│  • Spend funds            • Encryption            • Share with      │
│                                                     others          │
│                                                                     │
│  If stolen:               If stolen:              If stolen:        │
│  LOSE ALL FUNDS!          No direct risk          No risk           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Key Comparison Table

| Property | Private Key | Public Key | Address |
|----------|-------------|------------|---------|
| **What is it?** | Secret 256-bit number | Point on elliptic curve | Hash of public key |
| **Size** | 32 bytes (64 hex chars) | 33 bytes (compressed) | ~42 characters |
| **Example** | `L4rK1yDtCWekvXuE54...` | `02a1b2c3d4e5f6...` | `nx1qkjh8y4w7...` |
| **Can share?** | NEVER! | Yes (rarely needed) | Yes (for receiving) |
| **Purpose** | Control funds | Verify ownership | Receive payments |
| **If lost?** | Funds gone forever | Can regenerate | Can regenerate |
| **If stolen?** | Funds stolen! | Minor risk | No risk |

### How Keys Are Generated

```
┌─────────────────────────────────────────────────────────────────────┐
│                    KEY GENERATION FLOW                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  STEP 1: Random Entropy (256 bits)                                  │
│          Computer generates random numbers                          │
│                          │                                          │
│                          ▼                                          │
│  STEP 2: Recovery Phrase (24 words)                                 │
│          "photo unable season ready subject ocean ..."              │
│                          │                                          │
│                          ▼                                          │
│  STEP 3: Master Seed (512 bits)                                     │
│          PBKDF2(words + passphrase, 2048 iterations)                │
│                          │                                          │
│                          ▼                                          │
│  STEP 4: Master Private Key + Chain Code                            │
│          HMAC-SHA512 with "Bitcoin seed"                            │
│                          │                                          │
│           ┌──────────────┴──────────────┐                           │
│           ▼                             ▼                           │
│  STEP 5a: Child Key 1          STEP 5b: Child Key 2                 │
│           (m/44'/0'/0'/0/0)             (m/44'/0'/0'/0/1)            │
│           │                             │                           │
│           ▼                             ▼                           │
│  STEP 6a: Private Key 1        STEP 6b: Private Key 2               │
│           │                             │                           │
│           │ × G (curve math)            │ × G (curve math)          │
│           ▼                             ▼                           │
│  STEP 7a: Public Key 1         STEP 7b: Public Key 2                │
│           │                             │                           │
│           │ RIPEMD160(SHA256())         │ RIPEMD160(SHA256())       │
│           ▼                             ▼                           │
│  STEP 8a: Address 1            STEP 8b: Address 2                   │
│           nx1qkjh8y4w7...              nx1qa5gjxzlm...              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### The Math Behind Keys (Simplified)

```
┌─────────────────────────────────────────────────────────────────────┐
│                    PRIVATE KEY → PUBLIC KEY                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Private Key (secret number):  k = 12345...  (very large number)   │
│                                                                     │
│   Generator Point (known):      G = (x, y) on secp256k1 curve       │
│                                                                     │
│   Public Key (calculation):     P = k × G                           │
│                                                                     │
│   This is EASY to calculate:    k × G → P  (milliseconds)           │
│   This is IMPOSSIBLE to reverse: P → k     (billions of years)      │
│                                                                     │
│   Why? Because elliptic curve math is a "one-way function"          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    PUBLIC KEY → ADDRESS                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Public Key:     02a1b2c3d4e5f6789012345678901234567890123456...   │
│                              │                                      │
│                              ▼                                      │
│   SHA256 Hash:    7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3...     │
│                              │                                      │
│                              ▼                                      │
│   RIPEMD160:      89abcdef012345678901234567890123456789ab          │
│                              │                                      │
│                              ▼                                      │
│   Add prefix:     nx (mainnet) or tshr (testnet)                    │
│                              │                                      │
│                              ▼                                      │
│   Bech32 encode:  nx1qkjh8y4w7dqav3mhswd6uftuupsyg4a4d7fzfcq       │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Working with Keys - Commands

#### Generate New Keys (via new address)

Every time you create a new address, a new key pair is generated:

**For Mainnet:**
```bash
# Generate new address (creates new private/public key pair internally)
./shurium-cli getnewaddress "my savings"

# Output: nx1qnewaddress...
```

**For Testnet:**
```bash
./shurium-cli --testnet getnewaddress "test address"

# Output: tshr1qnewaddress...
```

#### Export Private Key (DANGEROUS!)

You can export a private key, but be extremely careful:

**For Mainnet:**
```bash
# Step 1: Unlock wallet
./shurium-cli walletpassphrase "your_password" 60

# Step 2: Export private key for an address
./shurium-cli dumpprivkey "nx1qyouraddress..."

# Output: L4rK1yDtCWekvXuE54FRgbQCGVn5HsWJgEwBMdXDMJpA3s4kZm
# WARNING: Anyone with this string can steal funds from this address!

# Step 3: Lock wallet immediately
./shurium-cli walletlock
```

**For Testnet:**
```bash
./shurium-cli --testnet walletpassphrase "your_password" 60
./shurium-cli --testnet dumpprivkey "tshr1qyouraddress..."
./shurium-cli --testnet walletlock
```

#### Import Private Key

If you have a private key from elsewhere:

**For Mainnet:**
```bash
# Unlock wallet
./shurium-cli walletpassphrase "your_password" 60

# Import private key
./shurium-cli importprivkey "L4rK1yDtCWekvXuE54..." "imported address"

# Lock wallet
./shurium-cli walletlock
```

**For Testnet:**
```bash
./shurium-cli --testnet walletpassphrase "your_password" 60
./shurium-cli --testnet importprivkey "cPrivateKey..." "imported address"
./shurium-cli --testnet walletlock
```

### Key Recovery Scenarios

```
┌─────────────────────────────────────────────────────────────────────┐
│                    KEY RECOVERY GUIDE                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  SCENARIO 1: Lost wallet.dat but have 24 words                      │
│  ─────────────────────────────────────────────                      │
│  Result: FULL RECOVERY POSSIBLE                                     │
│  Action: ./shurium-wallet-tool import --wallet=~/.shurium/wallet.dat    │
│  What recovers: All private keys, all addresses, all funds          │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  SCENARIO 2: Lost 24 words but have wallet.dat                      │
│  ─────────────────────────────────────────────                      │
│  Result: FUNDS SAFE (but backup immediately!)                       │
│  Action: ./shurium-cli backupwallet ~/backup.dat                      │
│  Risk: If wallet.dat is lost/corrupted, funds are GONE              │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  SCENARIO 3: Lost both 24 words and wallet.dat                      │
│  ─────────────────────────────────────────────                      │
│  Result: FUNDS LOST FOREVER                                         │
│  Action: None possible                                              │
│  Prevention: Always have multiple backups!                          │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  SCENARIO 4: Lost wallet password                                   │
│  ────────────────────────────────                                   │
│  Result: RECOVERY POSSIBLE with 24 words                            │
│  Action: Create new wallet from 24 words with new password          │
│  Note: Password protects file, 24 words generate the keys           │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  SCENARIO 5: Single private key lost (not 24 words)                 │
│  ─────────────────────────────────────────────────                  │
│  Result: That address's funds MAY be recoverable                    │
│  If you have 24 words: Regenerate from seed                         │
│  If you don't: Those specific funds are LOST                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Security Best Practices for Keys

```
┌─────────────────────────────────────────────────────────────────────┐
│                    KEY SECURITY CHECKLIST                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  DO:                                                                │
│  ────                                                               │
│  ✓ Write 24 words on paper and store in safe                        │
│  ✓ Make multiple copies in different locations                      │
│  ✓ Use strong wallet password (12+ characters)                      │
│  ✓ Lock wallet when not in use                                      │
│  ✓ Verify addresses character-by-character before sending           │
│  ✓ Test recovery process with small amount first                    │
│                                                                     │
│  DON'T:                                                             │
│  ──────                                                             │
│  ✗ Never share private key with anyone                              │
│  ✗ Never store 24 words digitally (no photos, no cloud)             │
│  ✗ Never enter 24 words on any website                              │
│  ✗ Never export private key unless absolutely necessary             │
│  ✗ Never send private key via email/chat                            │
│  ✗ Never use same private key on multiple wallets                   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Common Questions About Keys

**Q: Is my address the same as my private key?**
> NO! Your address is PUBLIC (share it to receive money). Your private key is SECRET (never share it).

**Q: Can someone steal my funds if they know my address?**
> NO. They can only send you money. They need your private key to take money.

**Q: Can I recover my private key from my address?**
> NO. This is mathematically impossible. That's what makes cryptocurrency secure.

**Q: Why do I need 24 words if I have a private key?**
> The 24 words can generate ALL your private keys (unlimited addresses). A single private key only controls one address.

**Q: What if I lose one private key but have my 24 words?**
> You can regenerate that private key from your 24 words. The same 24 words always generate the same keys in the same order.

**Q: Can two people have the same private key?**
> Theoretically possible but practically impossible. There are more possible keys than atoms in the universe.

---

## Creating Your Wallet

### Method 1: Automatic (When You Start SHURIUM)

When you first run `shuriumd`, it creates a wallet automatically:

**For Mainnet (Real Money):**
```bash
./shuriumd
# Wallet created at: ~/.shurium/wallet.dat
```

**For Testnet (Fake Money for Practice):**
```bash
./shuriumd --testnet
# Wallet created at: ~/.shurium/testnet/wallet.dat
```

### Method 2: Using the Wallet Tool (Recommended - More Secure)

**For Mainnet (Real Money):**
```bash
./shurium-wallet-tool create --words=24 --wallet=~/.shurium/wallet.dat
```

**For Testnet (Fake Money for Practice):**
```bash
./shurium-wallet-tool create --testnet --words=24 --wallet=~/.shurium/testnet/wallet.dat
```

**This gives you a 24-word recovery phrase:**

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│   🌱 YOUR RECOVERY PHRASE                                           │
│                                                                     │
│   abandon   ability   able     about    above    absent             │
│   absorb    abstract  absurd   abuse    access   accident           │
│   account   accuse    achieve  acid     acoustic acquire            │
│   across    act       action   actor    actress  actual             │
│                                                                     │
│   ⚠️  WRITE THESE DOWN ON PAPER - NOT ON YOUR COMPUTER!             │
│                                                                     │
│   These 24 words = Your entire wallet                               │
│   Anyone with these words can take ALL your money                   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### How Many Words Should I Choose?

| Words | Security Level | Use Case |
|-------|---------------|----------|
| 12 | Good | Small amounts, daily use |
| 15 | Better | Medium amounts |
| 18 | Strong | Larger amounts |
| 24 | Maximum | Long-term savings |

**Recommendation:** Use 24 words for any significant amount.

---

## 📫 Understanding Addresses

### Address Anatomy

```
shr1q8w4j5k6n2m3v4c5x6z7a8s9d0f1g2h3j4k5l6
│  │ └────────────────────────────────────────┘
│  │                    │
│  │                    └── Unique identifier (your specific address)
│  │
│  └── Address type
│      • q = Native SegWit (modern, lower fees) ← Recommended
│      • p = Pay to Script Hash
│      • 1 = Legacy (old style, higher fees)
│
└── Network prefix
    • shr  = Mainnet (real money)
    • tshr = Testnet (practice money)
```

### Creating New Addresses

**Best Practice:** Use a new address for each transaction for privacy.

**For Mainnet (Real Money):**
```bash
# Create address with a label
./shurium-cli getnewaddress "Salary from Company X"

# Create address without label
./shurium-cli getnewaddress

# List all your addresses
./shurium-cli listaddresses
```

**For Testnet (Fake Money for Practice):**
```bash
# Create address with a label
./shurium-cli --testnet getnewaddress "Test Address"

# Create address without label
./shurium-cli --testnet getnewaddress

# List all your addresses
./shurium-cli --testnet listaddresses
```

### Why Use Multiple Addresses?

```
┌─────────────────────────────────────────────────────────────────────┐
│                         PRIVACY COMPARISON                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ❌ BAD: One address for everything                                │
│                                                                     │
│   Employer → shr1abc... (sees your salary)                          │
│   Friend   → shr1abc... (sees employer payment!)                    │
│   Shop     → shr1abc... (sees everything!)                          │
│                                                                     │
│   Everyone can see your complete financial history! 😰              │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ✅ GOOD: Different address for each purpose                       │
│                                                                     │
│   Employer → shr1abc... (only sees salary)                          │
│   Friend   → shr1def... (only sees their payment)                   │
│   Shop     → shr1ghi... (only sees purchase)                        │
│                                                                     │
│   Nobody can link your transactions! 🔒                             │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Sending Money

### Step-by-Step Guide

#### 1. Unlock Your Wallet

If your wallet is encrypted (it should be!):

**For Mainnet:**
```bash
./shurium-cli walletpassphrase "your_password" 300
#                                             └── Seconds to stay unlocked
```

**For Testnet:**
```bash
./shurium-cli --testnet walletpassphrase "your_password" 300
```

#### 2. Verify Recipient Address

**CRITICAL:** Triple-check the address!

```
┌─────────────────────────────────────────────────────────────────────┐
│   ⚠️  TRANSACTIONS CANNOT BE REVERSED!                              │
│                                                                     │
│   Wrong address = Money gone FOREVER                                │
│   No customer service to call                                       │
│   No bank to reverse the charge                                     │
│   No way to get it back                                             │
│                                                                     │
│   ALWAYS: Copy-paste addresses. Never type manually.                │
└─────────────────────────────────────────────────────────────────────┘
```

#### 3. Check Your Balance

**For Mainnet:**
```bash
./shurium-cli getbalance
```

**For Testnet:**
```bash
./shurium-cli --testnet getbalance
```

Make sure you have enough for:
- Amount you want to send
- Transaction fee (usually 0.0001 - 0.001 SHR)

#### 4. Send the Transaction

**For Mainnet:**
```bash
./shurium-cli sendtoaddress "RECIPIENT_ADDRESS" AMOUNT
```

**For Testnet:**
```bash
./shurium-cli --testnet sendtoaddress "RECIPIENT_ADDRESS" AMOUNT
```

**Examples (Mainnet):**

```bash
# Send exactly 10 SHR
./shurium-cli sendtoaddress "shr1qrecipient..." 10

# Send with decimal
./shurium-cli sendtoaddress "shr1qrecipient..." 25.5

# Send with a comment (for your records)
./shurium-cli sendtoaddress "shr1qrecipient..." 10 "Payment for laptop"
```

**Examples (Testnet):**

```bash
# Send exactly 10 SHR
./shurium-cli --testnet sendtoaddress "tshr1qrecipient..." 10

# Send with decimal
./shurium-cli --testnet sendtoaddress "tshr1qrecipient..." 25.5
```

#### 5. Get Transaction ID

The command returns a transaction ID:
```
a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7b8c9d0e1f2
```

Save this! You can use it to track your transaction.

#### 6. Lock Your Wallet Again

**For Mainnet:**
```bash
./shurium-cli walletlock
```

**For Testnet:**
```bash
./shurium-cli --testnet walletlock
```

### Sending to Multiple Recipients

**For Mainnet:**
```bash
./shurium-cli sendmany "" '{
  "shr1qperson1...": 10.0,
  "shr1qperson2...": 15.5,
  "shr1qperson3...": 5.0
}'
```

This is more efficient (lower fees) than sending 3 separate transactions.

### Transaction Status

```bash
./shurium-cli gettransaction "YOUR_TRANSACTION_ID"
```

**Understanding confirmations:**

```
┌─────────────────────────────────────────────────────────────────────┐
│                    CONFIRMATION LEVELS                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Confirmations     Status              Wait Time    Safety         │
│   ─────────────     ──────              ─────────    ──────         │
│                                                                     │
│   0                 Unconfirmed         -            ⚠️ Risky       │
│   │                 (in mempool)                                    │
│   ▼                                                                 │
│   1                 Confirmed           ~30 sec      ⭐ Basic       │
│   │                 (in 1 block)                                    │
│   ▼                                                                 │
│   6                 Well-confirmed      ~3 min       ⭐⭐ Good      │
│   │                                                                 │
│   ▼                                                                 │
│   20+               Very secure         ~10 min      ⭐⭐⭐ Great   │
│   │                                                                 │
│   ▼                                                                 │
│   100               Fully mature        ~50 min      ⭐⭐⭐⭐ Max   │
│                     (mining rewards                                 │
│                      can be spent)                                  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Receiving Money

### Step 1: Generate an Address

**For Mainnet:**
```bash
./shurium-cli getnewaddress "Payment from Alice"
```

**For Testnet:**
```bash
./shurium-cli --testnet getnewaddress "Payment from Alice"
```

### Step 2: Share the Address

Give the address to whoever is paying you. Safe ways to share:

- ✅ Copy-paste in secure chat
- ✅ Show QR code (if available)
- ✅ Email (for non-sensitive amounts)
- ❌ Don't post publicly with your real identity

### Step 3: Wait for Payment

Check if payment arrived:

**For Mainnet:**
```bash
# Check overall balance
./shurium-cli getbalance

# Check specific address
./shurium-cli getreceivedbyaddress "shr1q..."

# List recent incoming transactions
./shurium-cli listtransactions "*" 10
```

**For Testnet:**
```bash
# Check overall balance
./shurium-cli --testnet getbalance

# Check specific address
./shurium-cli --testnet getreceivedbyaddress "tshr1q..."

# List recent incoming transactions
./shurium-cli --testnet listtransactions "*" 10
```

### Step 4: Wait for Confirmations

For important payments, wait for enough confirmations:

| Amount | Recommended Confirmations |
|--------|---------------------------|
| < 10 SHR | 1 confirmation |
| 10-100 SHR | 3 confirmations |
| 100-1000 SHR | 6 confirmations |
| > 1000 SHR | 10+ confirmations |

---

## Security Best Practices

### The Golden Rules

```
┌─────────────────────────────────────────────────────────────────────┐
│                     🛡️ SECURITY GOLDEN RULES                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. NEVER share your private keys or recovery phrase                │
│     └── Anyone with these can take EVERYTHING                       │
│                                                                     │
│  2. ALWAYS encrypt your wallet                                      │
│     └── Protects if someone gets your wallet file                   │
│                                                                     │
│  3. ALWAYS backup your wallet                                       │
│     └── Computer dies = lose everything without backup              │
│                                                                     │
│  4. NEVER store recovery phrase digitally                           │
│     └── Computers get hacked, cloud accounts get breached           │
│                                                                     │
│  5. ALWAYS verify addresses before sending                          │
│     └── Wrong address = money gone forever                          │
│                                                                     │
│  6. ALWAYS lock wallet when not in use                              │
│     └── Limits damage if computer is compromised                    │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Encrypt Your Wallet

**For Mainnet:**
```bash
./shurium-cli encryptwallet "your_strong_password_here"
```

**For Testnet:**
```bash
./shurium-cli --testnet encryptwallet "your_strong_password_here"
```

**Password requirements:**
- At least 12 characters
- Mix of letters, numbers, symbols
- NOT used anywhere else
- Something you can remember

### Lock/Unlock Your Wallet

```bash
# Lock (always do this when done)
./shurium-cli walletlock

# Unlock for 5 minutes (300 seconds)
./shurium-cli walletpassphrase "your_password" 300

# Unlock for staking only (safer for long-term)
./shurium-cli walletpassphrase "your_password" 0 true
```

### Secure Password Storage

```
┌─────────────────────────────────────────────────────────────────────┐
│                    WHERE TO STORE PASSWORDS                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ✅ GOOD                          ❌ BAD                           │
│   ────────                         ──────                           │
│                                                                     │
│   • Password manager (1Password,   • Sticky notes on monitor        │
│     Bitwarden, KeePass)           • Plain text file on desktop      │
│                                    • Email to yourself              │
│   • Written on paper in safe      • Cloud storage (Dropbox, etc)    │
│                                    • Browser "remember password"    │
│   • Metal backup plate            • Same password as other sites    │
│     (fire/water resistant)                                          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Backup and Recovery

### What to Backup

| Item | How to Backup | Priority |
|------|---------------|----------|
| Recovery phrase (24 words) | Write on paper | CRITICAL |
| wallet.dat file | Copy to USB drive | Important |
| Wallet password | Password manager | Important |

### Wallet File Locations

| Network | Wallet Location |
|---------|-----------------|
| Mainnet | `~/.shurium/wallet.dat` |
| Testnet | `~/.shurium/testnet/wallet.dat` |

### Backup Your Wallet File

**For Mainnet:**
```bash
# Using shurium-cli
./shurium-cli backupwallet ~/Desktop/shurium-mainnet-backup-$(date +%Y%m%d).dat

# Manual copy
cp ~/.shurium/wallet.dat ~/Desktop/shurium-mainnet-backup.dat
```

**For Testnet:**
```bash
# Using shurium-cli
./shurium-cli --testnet backupwallet ~/Desktop/shurium-testnet-backup-$(date +%Y%m%d).dat

# Manual copy
cp ~/.shurium/testnet/wallet.dat ~/Desktop/shurium-testnet-backup.dat
```

### Recovery Phrase Backup

```
┌─────────────────────────────────────────────────────────────────────┐
│               RECOVERY PHRASE BACKUP CHECKLIST                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Write on paper (pen, not pencil)                                  │
│   Write clearly and legibly                                         │
│   Double-check every word                                           │
│   Number the words (1-24)                                           │
│   Make 2-3 copies                                                   │
│   Store in different physical locations                             │
│   Consider fireproof/waterproof storage                             │
│   Tell trusted family member where it is                            │
│   NEVER take a photo                                                │
│   NEVER type into any website                                       │
│   NEVER store in cloud/email/notes app                              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Storage Locations (Multiple!)

```
┌─────────────────────────────────────────────────────────────────────┐
│                   BACKUP STORAGE STRATEGY                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Location 1: Home                                                  │
│   └── Fireproof safe                                                │
│       └── Recovery phrase + wallet.dat on USB                       │
│                                                                     │
│   Location 2: Bank                                                  │
│   └── Safe deposit box                                              │
│       └── Recovery phrase (paper only)                              │
│                                                                     │
│   Location 3: Family member's home                                  │
│   └── Sealed envelope with instructions                             │
│       └── Recovery phrase                                           │
│                                                                     │
│   Never store all backups in one place!                             │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Restoring From Backup

#### Method 1: Restore from wallet.dat file

**For Mainnet (Real Money):**

```bash
# Step 1: Stop the daemon
./shurium-cli stop

# Step 2: Backup current wallet (just in case)
mv ~/.shurium/wallet.dat ~/.shurium/wallet.dat.old

# Step 3: Copy your backup wallet file
cp /path/to/your/backup/wallet.dat ~/.shurium/wallet.dat

# Step 4: Start daemon again
nohup ./shuriumd > /dev/null 2>&1 &

# Step 5: Verify your addresses are back
./shurium-cli listaddresses
```

**For Testnet (Fake Money):**

```bash
# Step 1: Stop the daemon
./shurium-cli --testnet stop

# Step 2: Backup current wallet (just in case)
mv ~/.shurium/testnet/wallet.dat ~/.shurium/testnet/wallet.dat.old

# Step 3: Copy your backup wallet file
cp /path/to/your/backup/wallet.dat ~/.shurium/testnet/wallet.dat

# Step 4: Start daemon again
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Step 5: Verify your addresses are back
./shurium-cli --testnet listaddresses
```

#### Method 2: Restore from Recovery Phrase (24 words)

**For Mainnet (Real Money):**

```bash
# Step 1: Stop the daemon (if running)
./shurium-cli stop
pkill -f shuriumd

# Step 2: Backup current wallet (just in case)
mv ~/.shurium/wallet.dat ~/.shurium/wallet.dat.old

# Step 3: Use the wallet tool to import from recovery phrase
cd ~/shurium/build
./shurium-wallet-tool import --wallet=~/.shurium/wallet.dat

# When prompted:
#   - Enter your 24 words separated by SPACES (not commas)
#   - BIP39 Passphrase: Press Enter if you didn't use one when creating
#   - Wallet Password: Enter a new password to encrypt

# Step 4: Start daemon
nohup ./shuriumd > /dev/null 2>&1 &

# Step 5: Verify your addresses are restored
./shurium-cli listaddresses
./shurium-cli getbalance
```

**For Testnet (Fake Money):**

```bash
# Step 1: Stop the daemon (if running)
./shurium-cli --testnet stop
pkill -f shuriumd

# Step 2: Backup current wallet (just in case)
mv ~/.shurium/testnet/wallet.dat ~/.shurium/testnet/wallet.dat.old

# Step 3: Use the wallet tool to import from recovery phrase
cd ~/shurium/build
./shurium-wallet-tool import --testnet --wallet=~/.shurium/testnet/wallet.dat

# When prompted:
#   - Enter your 24 words separated by SPACES (not commas)
#   - BIP39 Passphrase: Press Enter if you didn't use one when creating
#   - Wallet Password: Enter a new password to encrypt

# Step 4: Start daemon
nohup ./shuriumd --testnet > /dev/null 2>&1 &

# Step 5: Verify your addresses are restored
./shurium-cli --testnet listaddresses
./shurium-cli --testnet getbalance
```

#### Important Notes About Recovery

| What You Have | What You Can Recover |
|---------------|---------------------|
| wallet.dat file | Everything (keys, labels, transaction history) |
| 24-word phrase | All addresses and funds (but not labels/history) |
| Only password | Nothing - password alone cannot recover wallet |
| Nothing | Funds are lost forever |

**Recovery Phrase Format:**
- Words must be separated by SPACES, not commas
- Example: `word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12`

**BIP39 Passphrase vs Wallet Password:**
- **BIP39 Passphrase**: Optional extra word used during wallet creation - changes all addresses
- **Wallet Password**: Encrypts your wallet file on disk - does NOT affect addresses

**If recovered address doesn't match original:** You may have used a BIP39 passphrase when creating the wallet. Try entering your password as the BIP39 passphrase.

---

## Advanced Features

### Check Transaction History

**For Mainnet:**
```bash
# Last 10 transactions
./shurium-cli listtransactions "*" 10

# All transactions for specific address
./shurium-cli listreceivedbyaddress
```

**For Testnet:**
```bash
# Last 10 transactions
./shurium-cli --testnet listtransactions "*" 10

# All transactions for specific address
./shurium-cli --testnet listreceivedbyaddress
```

### Export Private Key (Dangerous!)

**For Mainnet:**
```bash
# Only do this if you know what you're doing!
./shurium-cli dumpprivkey "shr1qyouraddress..."
```

**For Testnet:**
```bash
./shurium-cli --testnet dumpprivkey "tshr1qyouraddress..."
```

### Import Private Key

**For Mainnet:**
```bash
./shurium-cli importprivkey "YOUR_PRIVATE_KEY" "label"
```

**For Testnet:**
```bash
./shurium-cli --testnet importprivkey "YOUR_PRIVATE_KEY" "label"
```

### Sign a Message (Prove Ownership)

**For Mainnet:**
```bash
# Sign
./shurium-cli signmessage "shr1qyouraddress..." "I own this address"

# Verify
./shurium-cli verifymessage "shr1qaddress..." "signature" "message"
```

**For Testnet:**
```bash
# Sign
./shurium-cli --testnet signmessage "tshr1qyouraddress..." "I own this address"

# Verify
./shurium-cli --testnet verifymessage "tshr1qaddress..." "signature" "message"
```

### Change Wallet Password

**For Mainnet:**
```bash
./shurium-cli walletpassphrasechange "old_password" "new_password"
```

---

## 🆘 Common Issues

### "Wallet is locked"

```bash
./shurium-cli walletpassphrase "your_password" 300
```

### "Insufficient funds"

Check your balance:
```bash
./shurium-cli getbalance
./shurium-cli listunspent  # Shows individual UTXOs
```

### "Fee too low"

The network is busy. Either:
- Wait for less busy time
- Use `settxfee` to increase fee:
  ```bash
  ./shurium-cli settxfee 0.001
  ```

### Forgot password but have recovery phrase

1. Create new wallet from recovery phrase
2. New wallet will have same addresses and funds

---

## 📋 Quick Reference

| Task | Command |
|------|---------|
| Check balance | `getbalance` |
| New address | `getnewaddress "label"` |
| List addresses | `listaddresses` |
| Send | `sendtoaddress "addr" amount` |
| Unlock | `walletpassphrase "pass" seconds` |
| Lock | `walletlock` |
| Backup | `backupwallet "/path/to/backup.dat"` |
| Transactions | `listtransactions` |
| Encrypt | `encryptwallet "password"` |

---

<div align="center">

**Remember:** Your keys, your coins. Not your keys, not your coins.

[← Back to README](../README.md) | [Mining Guide →](MINING_GUIDE.md)

</div>
