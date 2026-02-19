<div align="center">

# Universal Basic Income in SHURIUM

## Free Money? Yes, Really. Here's How.

*Every verified human gets a daily share of new coins*

---

</div>

## The Big Idea

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│                    WHAT IS UBI IN SHURIUM?                              │
│                                                                         │
│   Every day, 30% of ALL new SHURIUM coins are shared equally            │
│   among ALL verified human users.                                       │
│                                                                         │
│   Not corporations. Not bots. Not whales.                               │
│   PEOPLE. Real human beings.                                            │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │                        THE SIMPLE VERSION                       │   │
│   │                                                                 │   │
│   │   1. SHURIUM creates new coins every 30 seconds                 │   │
│   │   2. 30% goes to a shared pool                                  │   │
│   │   3. Pool is divided equally among verified users               │   │
│   │   4. Claim your share daily                                     │   │
│   │   5. Repeat forever                                             │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## How Much Can You Get?

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│                    UBI MATH MADE SIMPLE                                 │
│                                                                         │
│   DAILY NUMBERS                                                         │
│   ─────────────                                                         │
│                                                                         │
│   Blocks per day:     2,880 (one every 30 seconds)                      │
│   Coins per block:    50 SHR (initial reward)                           │
│   Total daily coins:  144,000 SHR                                       │
│   UBI share (30%):    43,200 SHR                                        │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   YOUR DAILY UBI = 43,200 ÷ (number of verified users)          │   │
│   │                                                                 │   │
│   │   If 10,000 users:     4.32 SHR/day/person                      │   │
│   │   If 100,000 users:    0.432 SHR/day/person                     │   │
│   │   If 1,000,000 users:  0.0432 SHR/day/person                    │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   KEY INSIGHT:                                                          │
│   More users = smaller individual share, BUT also more adoption,        │
│   which typically increases token value. It balances out!               │
│                                                                         │
│   UBI POOL SECURITY:                                                    │
│   The UBI Pool is secured by a 2-of-3 multisig address.                 │
│   View details: ./shurium-cli getfundaddress ubi                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## How to Get Your UBI

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│                    3 STEPS TO FREE MONEY                                │
│                                                                         │
│   STEP 1: PROVE YOU'RE HUMAN (one time)                                 │
│   ──────────────────────────────────────                                │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   # Register your identity                                      │   │
│   │   ./shurium-cli registeridentity                                │   │
│   │                                                                 │   │
│   │   This creates a special cryptographic commitment that:         │   │
│   │   • Proves you're a unique person                               │   │
│   │   • Does NOT reveal who you are                                 │   │
│   │   • Cannot be faked or duplicated                               │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   STEP 2: WAIT FOR VERIFICATION                                         │
│   ─────────────────────────────                                         │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   # Check your status                                           │   │
│   │   ./shurium-cli getidentitystatus                               │   │
│   │                                                                 │   │
│   │   Status: Pending → Active                                      │   │
│   │                                                                 │   │
│   │   Verification confirms you're unique (not a duplicate)         │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   STEP 3: CLAIM DAILY                                                   │
│   ───────────────────                                                   │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   # Claim today's UBI                                           │   │
│   │   ./shurium-cli claimubi                                        │   │
│   │                                                                 │   │
│   │   ✓ Generating proof...                                         │   │
│   │   ✓ Submitting claim...                                         │   │
│   │   ✓ Claimed 4.32 SHR!                                           │   │
│   │                                                                 │   │
│   │   Your balance: 104.32 SHR                                      │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   That's it! Claim every day to maximize your earnings.                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## The Magic: Zero-Knowledge Identity

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│                    HOW CAN THIS BE PRIVATE?                             │
│                                                                         │
│   THE CHALLENGE:                                                        │
│   We need to know you're a unique human to give you UBI.                │
│   But we DON'T want to know WHO you are.                                │
│                                                                         │
│   THE SOLUTION: ZERO-KNOWLEDGE PROOFS                                   │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   REAL-WORLD ANALOGY                                            │   │
│   │                                                                 │   │
│   │   Imagine proving you're a member of a club without showing     │   │
│   │   your membership card, name, or face.                          │   │
│   │                                                                 │   │
│   │   You just show a mathematical proof that says:                 │   │
│   │   "Trust me, I'm definitely a member, but I won't tell you who" │   │
│   │                                                                 │   │
│   │   And the math is so good, it's IMPOSSIBLE to fake.             │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   HOW IT WORKS IN SHURIUM:                                              │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   REGISTRATION (one time)                                       │   │
│   │   ────────────────────────                                      │   │
│   │                                                                 │   │
│   │   You create SECRET values on your device:                      │   │
│   │   • Secret Key (like a password, but stronger)                  │   │
│   │   • Nullifier Key (prevents double-claiming)                    │   │
│   │   • Trapdoor (extra randomness)                                 │   │
│   │                                                                 │   │
│   │   From these, you create a PUBLIC commitment:                   │   │
│   │   Commitment = Hash(secretKey, nullifierKey, trapdoor)          │   │
│   │                                                                 │   │
│   │   Only the commitment goes on the blockchain.                   │   │
│   │   Your secrets NEVER leave your device.                         │   │
│   │                                                                 │   │
│   │                         ┌────────────┐                          │   │
│   │   YOUR SECRETS ───────▶ │    HASH    │ ───────▶ COMMITMENT      │   │
│   │   (stay with you)       │ (one-way)  │         (on blockchain)  │   │
│   │                         └────────────┘                          │   │
│   │                                                                 │   │
│   │   Nobody can reverse the hash to get your secrets!              │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   CLAIMING UBI (daily)                                          │   │
│   │   ────────────────────                                          │   │
│   │                                                                 │   │
│   │   You create a NULLIFIER (unique per day):                      │   │
│   │   Nullifier = Hash(nullifierKey, todayDate, "UBI")              │   │
│   │                                                                 │   │
│   │   You create a ZERO-KNOWLEDGE PROOF that proves:                │   │
│   │                                                                 │   │
│   │   ✓ "I know the secrets for a registered identity"              │   │
│   │   ✓ "My commitment is in the Merkle tree of identities"         │   │
│   │   ✓ "My nullifier is correctly calculated"                      │   │
│   │   ✓ "I haven't claimed today yet" (nullifier not seen before)   │   │
│   │                                                                 │   │
│   │   WITHOUT REVEALING:                                            │   │
│   │   ✗ Which identity you are                                      │   │
│   │   ✗ Your secret keys                                            │   │
│   │   ✗ Any personal information                                    │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   THE RESULT:                                                           │
│   You get your UBI. Nobody knows it was you. Can't be traced.           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Preventing Cheating

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│                    "WHAT IF SOMEONE TRIES TO CHEAT?"                    │
│                                                                         │
│   ATTACK 1: CLAIM TWICE IN ONE DAY                                      │
│   ────────────────────────────────                                      │
│                                                                         │
│   Attempt: Use the same identity to claim twice                         │
│                                                                         │
│   Defense: NULLIFIERS                                                   │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   Your nullifier for today is ALWAYS THE SAME:                  │   │
│   │                                                                 │   │
│   │   Nullifier = Hash(nullifierKey, "2024-01-15", "UBI")           │   │
│   │                                                                 │   │
│   │   First claim: Network stores nullifier ✓                       │   │
│   │   Second claim: "Hey, I've seen this nullifier!" ✗ REJECTED     │   │
│   │                                                                 │   │
│   │   You CAN'T create a different nullifier without different      │   │
│   │   secrets, and different secrets = different identity.          │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   ATTACK 2: CREATE MULTIPLE FAKE IDENTITIES                             │
│   ─────────────────────────────────────────                             │
│                                                                         │
│   Attempt: Register 100 identities to get 100x UBI                      │
│                                                                         │
│   Defense: VERIFICATION PROCESS                                         │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   Registration requires proof of unique humanness:              │   │
│   │                                                                 │   │
│   │   • Biometric verification (privacy-preserving)                 │   │
│   │   • Social vouching systems                                     │   │
│   │   • Economic stake requirements                                 │   │
│   │   • Other anti-Sybil mechanisms                                 │   │
│   │                                                                 │   │
│   │   Creating fake identities is expensive/impossible.             │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   ATTACK 3: STEAL SOMEONE'S IDENTITY                                    │
│   ──────────────────────────────────                                    │
│                                                                         │
│   Attempt: Get someone else's secrets to claim their UBI                │
│                                                                         │
│   Defense: SECRETS NEVER LEAVE YOUR DEVICE                              │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   Your secrets are generated and stored LOCALLY.                │   │
│   │   They're never transmitted anywhere.                           │   │
│   │                                                                 │   │
│   │   The only way to steal them:                                   │   │
│   │   • Physical access to your device                              │   │
│   │   • Your device is compromised by malware                       │   │
│   │                                                                 │   │
│   │   Solution: Standard security practices                         │   │
│   │   • Encrypt your wallet                                         │   │
│   │   • Use strong passwords                                        │   │
│   │   • Keep software updated                                       │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   ATTACK 4: LINK CLAIMS TO IDENTITIES                                   │
│   ───────────────────────────────────                                   │
│                                                                         │
│   Attempt: Track who claimed what by analyzing the blockchain           │
│                                                                         │
│   Defense: ZERO-KNOWLEDGE PROOFS                                        │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   The math makes this IMPOSSIBLE:                               │   │
│   │                                                                 │   │
│   │   What's on blockchain:                                         │   │
│   │   • Nullifier (random-looking hash)                             │   │
│   │   • ZK Proof (mathematical gibberish)                           │   │
│   │   • Receiving address (can be fresh each time)                  │   │
│   │                                                                 │   │
│   │   What's NOT on blockchain:                                     │   │
│   │   • Which identity claimed                                      │   │
│   │   • Any link between claims                                     │   │
│   │   • Personal information                                        │   │
│   │                                                                 │   │
│   │   Even if someone has ALL blockchain data, they CAN'T           │   │
│   │   figure out who claimed what.                                  │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Why UBI Matters

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│                    THE BIGGER PICTURE                                   │
│                                                                         │
│   PROBLEM WITH TRADITIONAL CRYPTO:                                      │
│   ────────────────────────────────                                      │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   BITCOIN DISTRIBUTION:                                         │   │
│   │                                                                 │   │
│   │   • Top 0.01% own 27% of all Bitcoin                            │   │
│   │   • Average person can't afford mining hardware                 │   │
│   │   • Early adopters got rich, latecomers struggle                │   │
│   │   • Wealth concentrates over time                               │   │
│   │                                                                 │   │
│   │   This mirrors (or worsens) real-world inequality.              │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   SHURIUM SOLUTION:                                                     │
│   ───────────────                                                       │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   SHURIUM DISTRIBUTION:                                         │   │
│   │                                                                 │   │
│   │   • 30% of new coins go to ALL verified humans equally          │   │
│   │   • Don't need expensive hardware                               │   │
│   │   • Late adopters get same daily share as early ones            │   │
│   │   • Wealth spreads instead of concentrating                     │   │
│   │                                                                 │   │
│   │   This creates a more fair and inclusive economy.               │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   THE RIPPLE EFFECTS:                                                   │
│   ──────────────────                                                    │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                                                                 │   │
│   │   1. MORE ADOPTION                                              │   │
│   │      Free money → More users → Stronger network                 │   │
│   │                                                                 │   │
│   │   2. LOYAL COMMUNITY                                            │   │
│   │      Users have stake in success → Defend & promote it          │   │
│   │                                                                 │   │
│   │   3. REAL UTILITY                                               │   │
│   │      People actually USE the currency (not just hold)           │   │
│   │                                                                 │   │
│   │   4. ECONOMIC FLOOR                                             │   │
│   │      Nobody in the ecosystem starts from zero                   │   │
│   │                                                                 │   │
│   │   5. GLOBAL REACH                                               │   │
│   │      Works anywhere with internet                               │   │
│   │      Especially powerful in developing economies                │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## UBI Commands Reference

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│                    UBI COMMANDS                                         │
│                                                                         │
│   REGISTRATION                                                          │
│   ────────────                                                          │
│                                                                         │
│   # Register for UBI (one time)                                         │
│   ./shurium-cli registeridentity                                        │
│                                                                         │
│   # Check registration status                                           │
│   ./shurium-cli getidentitystatus                                       │
│   # Returns: pending, active, suspended, or revoked                     │
│                                                                         │
│   CLAIMING                                                              │
│   ────────                                                              │
│                                                                         │
│   # Claim today's UBI                                                   │
│   ./shurium-cli claimubi                                                │
│                                                                         │
│   # Check if you've claimed today                                       │
│   ./shurium-cli getubistatus                                            │
│                                                                         │
│   # See UBI history                                                     │
│   ./shurium-cli listubihistory                                          │
│                                                                         │
│   POOL INFO                                                             │
│   ─────────                                                             │
│                                                                         │
│   # See current UBI pool                                                │
│   ./shurium-cli getubipoolinfo                                          │
│                                                                         │
│   # Returns:                                                            │
│   # - Current pool balance                                              │
│   # - Number of verified users                                          │
│   # - Estimated payout per person                                       │
│   # - Time until next distribution                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## FAQ

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│                    FREQUENTLY ASKED QUESTIONS                           │
│                                                                         │
│   Q: Is this really free money?                                         │
│   ─────────────────────────────                                         │
│   A: Yes! As long as you're a verified unique human, you get a daily    │
│      share of new coins. No catch.                                      │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   Q: How much will I get?                                               │
│   ───────────────────────                                               │
│   A: Depends on how many people are verified. Early on, more per        │
│      person. As adoption grows, less per person but token value         │
│      typically increases.                                               │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   Q: What if I forget to claim?                                         │
│   ─────────────────────────────                                         │
│   A: Unclaimed UBI stays in the pool and gets distributed to those      │
│      who do claim. Don't miss out!                                      │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   Q: Can I claim for my family members?                                 │
│   ─────────────────────────────────────                                 │
│   A: Each family member needs their own identity. One person = one      │
│      share. No pooling allowed.                                         │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   Q: What if I lose my identity secrets?                                │
│   ──────────────────────────────────────                                │ 
│   A: Back them up! If lost, you'd need to re-register with a new        │
│      identity (verification process again).                             │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   Q: Is UBI taxable?                                                    │
│   ─────────────────                                                     │
│   A: Depends on your jurisdiction. Consult a tax professional.          │
│      In most places, received cryptocurrency is taxable income.         │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────     │
│                                                                         │
│   Q: Will UBI ever run out?                                             │
│   ─────────────────────────                                             │
│   A: No! SHURIUM has a minimum block reward of 1 SHR forever.           │
│      UBI continues indefinitely.                                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

<div align="center">

## Ready to Claim Your Share?

1. [Set up your wallet](WALLET_GUIDE.md)
2. Register your identity: `./shurium-cli registeridentity`
3. Claim daily: `./shurium-cli claimubi`

---

**Learn more:** [Whitepaper](WHITEPAPER.md) | [Architecture](ARCHITECTURE.md)

*Your daily income awaits!*

</div>
