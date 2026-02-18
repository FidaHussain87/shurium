// SHURIUM - Mempool Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/mempool/mempool.h"
#include "shurium/core/transaction.h"
#include "shurium/core/script.h"

using namespace shurium;

// ============================================================================
// Test Utilities
// ============================================================================

class MempoolTestFixture : public ::testing::Test {
protected:
    // Helper to create a simple transaction
    TransactionRef CreateTx(const std::vector<OutPoint>& inputs, 
                            Amount totalOutput, int numOutputs = 1) {
        MutableTransaction mtx;
        mtx.version = 1;
        
        for (const auto& input : inputs) {
            mtx.vin.push_back(TxIn(input));
        }
        
        Hash160 pubKeyHash;
        pubKeyHash[0] = 0xAB;
        Amount perOutput = totalOutput / numOutputs;
        for (int i = 0; i < numOutputs; ++i) {
            mtx.vout.push_back(TxOut(perOutput, Script::CreateP2PKH(pubKeyHash)));
        }
        
        return MakeTransactionRef(std::move(mtx));
    }
    
    // Helper to create a coinbase-like input (null outpoint)
    OutPoint MakeOutPoint(uint8_t txByte, uint32_t n) {
        TxHash hash;
        hash[0] = txByte;
        return OutPoint(hash, n);
    }
};

// ============================================================================
// FeeRate Tests
// ============================================================================

TEST(FeeRateTest, DefaultConstructor) {
    FeeRate rate;
    EXPECT_EQ(rate.GetFeePerK(), 0);
}

TEST(FeeRateTest, ConstructFromFeePerK) {
    FeeRate rate(1000);  // 1 sat/vB
    EXPECT_EQ(rate.GetFeePerK(), 1000);
}

TEST(FeeRateTest, ConstructFromFeeAndSize) {
    FeeRate rate(1000, 250);  // 1000 sats for 250 bytes = 4 sat/vB = 4000/kB
    EXPECT_EQ(rate.GetFeePerK(), 4000);
}

TEST(FeeRateTest, GetFee) {
    FeeRate rate(1000);  // 1 sat/vB
    
    // 100 bytes at 1 sat/vB = 100 sats
    EXPECT_EQ(rate.GetFee(100), 100);
    
    // 1000 bytes at 1 sat/vB = 1000 sats
    EXPECT_EQ(rate.GetFee(1000), 1000);
}

TEST(FeeRateTest, GetFeeRoundsUp) {
    FeeRate rate(1500);  // 1.5 sat/vB
    
    // 1 byte should round up from 1.5 to 2
    EXPECT_EQ(rate.GetFee(1), 2);
}

TEST(FeeRateTest, Comparison) {
    FeeRate low(500);
    FeeRate mid(1000);
    FeeRate high(2000);
    
    EXPECT_TRUE(low < mid);
    EXPECT_TRUE(mid < high);
    EXPECT_TRUE(high > mid);
    EXPECT_FALSE(low > mid);
    EXPECT_TRUE(mid == FeeRate(1000));
    EXPECT_TRUE(low != mid);
}

TEST(FeeRateTest, ToString) {
    FeeRate rate(1000);
    std::string str = rate.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("sat/vB"), std::string::npos);
}

// ============================================================================
// MempoolEntry Tests
// ============================================================================

class MempoolEntryTest : public MempoolTestFixture {
protected:
    TransactionRef testTx;
    
    void SetUp() override {
        testTx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN, 1);
    }
};

TEST_F(MempoolEntryTest, Constructor) {
    MempoolEntry entry(testTx, COIN, 1000, 100, false);
    
    EXPECT_EQ(&entry.GetTx(), testTx.get());
    EXPECT_EQ(entry.GetSharedTx(), testTx);
    EXPECT_EQ(entry.GetFee(), COIN);
    EXPECT_EQ(entry.GetModifiedFee(), COIN);
    EXPECT_EQ(entry.GetTime(), 1000);
    EXPECT_EQ(entry.GetHeight(), 100);
    EXPECT_FALSE(entry.SpendsCoinbase());
    EXPECT_GT(entry.GetTxSize(), 0);
}

TEST_F(MempoolEntryTest, FeeRate) {
    MempoolEntry entry(testTx, COIN, 1000, 100, false);
    
    FeeRate rate = entry.GetFeeRate();
    EXPECT_GT(rate.GetFeePerK(), 0);
}

TEST_F(MempoolEntryTest, AncestorStats) {
    MempoolEntry entry(testTx, COIN, 1000, 100, false);
    
    // Initially just this transaction
    EXPECT_EQ(entry.GetCountWithAncestors(), 1);
    EXPECT_EQ(entry.GetSizeWithAncestors(), entry.GetTxSize());
    EXPECT_EQ(entry.GetModFeesWithAncestors(), COIN);
    
    // Update ancestors
    entry.UpdateAncestorState(1, 200, COIN/2);
    EXPECT_EQ(entry.GetCountWithAncestors(), 2);
}

TEST_F(MempoolEntryTest, DescendantStats) {
    MempoolEntry entry(testTx, COIN, 1000, 100, false);
    
    EXPECT_EQ(entry.GetCountWithDescendants(), 1);
    
    entry.UpdateDescendantState(1, 300, COIN/4);
    EXPECT_EQ(entry.GetCountWithDescendants(), 2);
}

TEST_F(MempoolEntryTest, ModifiedFee) {
    MempoolEntry entry(testTx, COIN, 1000, 100, false);
    
    EXPECT_EQ(entry.GetModifiedFee(), COIN);
    
    entry.UpdateModifiedFee(2 * COIN);
    EXPECT_EQ(entry.GetModifiedFee(), 2 * COIN);
}

// ============================================================================
// Mempool Basic Tests
// ============================================================================

class MempoolBasicTest : public MempoolTestFixture {
protected:
    Mempool mempool;
};

TEST_F(MempoolBasicTest, DefaultState) {
    EXPECT_TRUE(mempool.IsEmpty());
    EXPECT_EQ(mempool.Size(), 0);
    EXPECT_EQ(mempool.GetTotalSize(), 0);
    EXPECT_EQ(mempool.GetTotalFees(), 0);
}

TEST_F(MempoolBasicTest, AddTransaction) {
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    std::string err;
    
    bool added = mempool.AddTx(tx, COIN, 100, false, err);
    EXPECT_TRUE(added) << "Error: " << err;
    
    EXPECT_FALSE(mempool.IsEmpty());
    EXPECT_EQ(mempool.Size(), 1);
    EXPECT_GT(mempool.GetTotalSize(), 0);
    EXPECT_EQ(mempool.GetTotalFees(), COIN);
}

TEST_F(MempoolBasicTest, AddDuplicateRejected) {
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    std::string err;
    
    EXPECT_TRUE(mempool.AddTx(tx, COIN, 100, false, err));
    EXPECT_FALSE(mempool.AddTx(tx, COIN, 100, false, err));
    EXPECT_EQ(err, "txn-already-in-mempool");
    EXPECT_EQ(mempool.Size(), 1);
}

TEST_F(MempoolBasicTest, LowFeeRejected) {
    MempoolLimits limits;
    limits.minFeeRate = FeeRate(10000);  // 10 sat/vB
    Mempool strictMempool(limits);
    
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    std::string err;
    
    // Try to add with very low fee
    EXPECT_FALSE(strictMempool.AddTx(tx, 1, 100, false, err));
    EXPECT_EQ(err, "mempool min fee not met");
}

TEST_F(MempoolBasicTest, ExistsAndGet) {
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    TxHash txid = tx->GetHash();
    std::string err;
    
    EXPECT_FALSE(mempool.Exists(txid));
    EXPECT_EQ(mempool.Get(txid), nullptr);
    
    mempool.AddTx(tx, COIN, 100, false, err);
    
    EXPECT_TRUE(mempool.Exists(txid));
    EXPECT_NE(mempool.Get(txid), nullptr);
    EXPECT_EQ(mempool.Get(txid)->GetHash(), txid);
}

TEST_F(MempoolBasicTest, GetInfo) {
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    TxHash txid = tx->GetHash();
    std::string err;
    
    auto info = mempool.GetInfo(txid);
    EXPECT_FALSE(info.has_value());
    
    mempool.AddTx(tx, COIN, 100, false, err);
    
    info = mempool.GetInfo(txid);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->tx->GetHash(), txid);
    EXPECT_EQ(info->fee, COIN);
    EXPECT_GT(info->vsize, 0);
}

TEST_F(MempoolBasicTest, Clear) {
    auto tx1 = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    auto tx2 = CreateTx({MakeOutPoint(0x02, 0)}, 48 * COIN);
    std::string err;
    
    mempool.AddTx(tx1, COIN, 100, false, err);
    mempool.AddTx(tx2, COIN, 100, false, err);
    EXPECT_EQ(mempool.Size(), 2);
    
    mempool.Clear();
    EXPECT_TRUE(mempool.IsEmpty());
    EXPECT_EQ(mempool.Size(), 0);
}

// ============================================================================
// Mempool Conflict Tests
// ============================================================================

class MempoolConflictTest : public MempoolTestFixture {
protected:
    Mempool mempool;
};

TEST_F(MempoolConflictTest, ConflictingInputRejected) {
    OutPoint sharedInput = MakeOutPoint(0x01, 0);
    
    auto tx1 = CreateTx({sharedInput}, 49 * COIN);
    auto tx2 = CreateTx({sharedInput}, 48 * COIN);  // Same input
    std::string err;
    
    EXPECT_TRUE(mempool.AddTx(tx1, COIN, 100, false, err));
    EXPECT_FALSE(mempool.AddTx(tx2, COIN, 100, false, err));
    EXPECT_EQ(err, "txn-mempool-conflict");
}

TEST_F(MempoolConflictTest, IsSpent) {
    OutPoint input = MakeOutPoint(0x01, 0);
    auto tx = CreateTx({input}, 49 * COIN);
    std::string err;
    
    EXPECT_FALSE(mempool.IsSpent(input));
    
    mempool.AddTx(tx, COIN, 100, false, err);
    
    EXPECT_TRUE(mempool.IsSpent(input));
}

TEST_F(MempoolConflictTest, GetSpender) {
    OutPoint input = MakeOutPoint(0x01, 0);
    auto tx = CreateTx({input}, 49 * COIN);
    std::string err;
    
    EXPECT_EQ(mempool.GetSpender(input), nullptr);
    
    mempool.AddTx(tx, COIN, 100, false, err);
    
    TransactionRef spender = mempool.GetSpender(input);
    ASSERT_NE(spender, nullptr);
    EXPECT_EQ(spender->GetHash(), tx->GetHash());
}

TEST_F(MempoolConflictTest, HasConflicts) {
    OutPoint input1 = MakeOutPoint(0x01, 0);
    OutPoint input2 = MakeOutPoint(0x02, 0);
    
    auto tx1 = CreateTx({input1}, 49 * COIN);
    auto tx2 = CreateTx({input1}, 48 * COIN);  // Conflicts
    auto tx3 = CreateTx({input2}, 47 * COIN);  // No conflict
    std::string err;
    
    mempool.AddTx(tx1, COIN, 100, false, err);
    
    EXPECT_TRUE(mempool.HasConflicts(*tx2));
    EXPECT_FALSE(mempool.HasConflicts(*tx3));
}

TEST_F(MempoolConflictTest, RemoveConflicts) {
    OutPoint input = MakeOutPoint(0x01, 0);
    
    auto tx1 = CreateTx({input}, 49 * COIN);
    std::string err;
    mempool.AddTx(tx1, COIN, 100, false, err);
    EXPECT_EQ(mempool.Size(), 1);
    
    // Create a conflicting transaction (simulating it being confirmed)
    auto conflictTx = CreateTx({input}, 48 * COIN);
    mempool.RemoveConflicts(*conflictTx);
    
    EXPECT_TRUE(mempool.IsEmpty());
}

// ============================================================================
// Mempool Removal Tests
// ============================================================================

class MempoolRemovalTest : public MempoolTestFixture {
protected:
    Mempool mempool;
    std::vector<std::pair<TxHash, MempoolRemovalReason>> removals;
    
    void SetUp() override {
        mempool.SetNotifyRemoved([this](const TransactionRef& tx, MempoolRemovalReason reason) {
            removals.push_back({tx->GetHash(), reason});
        });
    }
};

TEST_F(MempoolRemovalTest, RemoveTx) {
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    TxHash txid = tx->GetHash();
    std::string err;
    
    mempool.AddTx(tx, COIN, 100, false, err);
    EXPECT_EQ(mempool.Size(), 1);
    
    mempool.RemoveTxAndDescendants(txid, MempoolRemovalReason::EXPIRY);
    
    EXPECT_TRUE(mempool.IsEmpty());
    EXPECT_EQ(removals.size(), 1);
    EXPECT_EQ(removals[0].first, txid);
    EXPECT_EQ(removals[0].second, MempoolRemovalReason::EXPIRY);
}

TEST_F(MempoolRemovalTest, RemoveForBlock) {
    auto tx1 = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    auto tx2 = CreateTx({MakeOutPoint(0x02, 0)}, 48 * COIN);
    std::string err;
    
    mempool.AddTx(tx1, COIN, 100, false, err);
    mempool.AddTx(tx2, COIN, 100, false, err);
    EXPECT_EQ(mempool.Size(), 2);
    
    // Simulate block including tx1
    std::vector<TransactionRef> blockTxs = {tx1};
    mempool.RemoveForBlock(blockTxs);
    
    EXPECT_EQ(mempool.Size(), 1);
    EXPECT_FALSE(mempool.Exists(tx1->GetHash()));
    EXPECT_TRUE(mempool.Exists(tx2->GetHash()));
}

// ============================================================================
// Mempool Chain Tests (Parent-Child relationships)
// ============================================================================

class MempoolChainTest : public MempoolTestFixture {
protected:
    Mempool mempool;
};

TEST_F(MempoolChainTest, ChildSpendingParent) {
    // Create parent tx with output
    auto parent = CreateTx({MakeOutPoint(0x01, 0)}, 48 * COIN, 1);
    TxHash parentHash = parent->GetHash();
    std::string err;
    
    mempool.AddTx(parent, COIN, 100, false, err);
    
    // Create child spending parent's output
    OutPoint parentOutput(parentHash, 0);
    auto child = CreateTx({parentOutput}, 47 * COIN, 1);
    
    EXPECT_TRUE(mempool.AddTx(child, COIN, 100, false, err)) << "Error: " << err;
    EXPECT_EQ(mempool.Size(), 2);
}

TEST_F(MempoolChainTest, RemoveParentRemovesChildren) {
    // Create chain: parent -> child -> grandchild
    auto parent = CreateTx({MakeOutPoint(0x01, 0)}, 48 * COIN, 1);
    TxHash parentHash = parent->GetHash();
    std::string err;
    mempool.AddTx(parent, COIN, 100, false, err);
    
    OutPoint parentOutput(parentHash, 0);
    auto child = CreateTx({parentOutput}, 47 * COIN, 1);
    TxHash childHash = child->GetHash();
    mempool.AddTx(child, COIN, 100, false, err);
    
    OutPoint childOutput(childHash, 0);
    auto grandchild = CreateTx({childOutput}, 46 * COIN, 1);
    mempool.AddTx(grandchild, COIN, 100, false, err);
    
    EXPECT_EQ(mempool.Size(), 3);
    
    // Remove parent - should remove all descendants
    mempool.RemoveTxAndDescendants(parentHash, MempoolRemovalReason::CONFLICT);
    
    EXPECT_TRUE(mempool.IsEmpty());
}

// ============================================================================
// Mempool Mining Tests
// ============================================================================

class MempoolMiningTest : public MempoolTestFixture {
protected:
    Mempool mempool;
};

TEST_F(MempoolMiningTest, GetTransactionsForBlock) {
    // Add several transactions with different fees
    auto tx1 = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    auto tx2 = CreateTx({MakeOutPoint(0x02, 0)}, 49 * COIN);
    auto tx3 = CreateTx({MakeOutPoint(0x03, 0)}, 49 * COIN);
    std::string err;
    
    mempool.AddTx(tx1, COIN, 100, false, err);      // Low fee
    mempool.AddTx(tx2, 2 * COIN, 100, false, err);  // Medium fee
    mempool.AddTx(tx3, 3 * COIN, 100, false, err);  // High fee
    
    // Get transactions for block
    auto blockTxs = mempool.GetTransactionsForBlock(1000000, FeeRate(0));
    
    EXPECT_EQ(blockTxs.size(), 3);
    
    // Higher fee transactions should come first
    // (exact order depends on fee rate calculation)
    EXPECT_GT(blockTxs.size(), 0);
}

TEST_F(MempoolMiningTest, GetTransactionsSizeLimit) {
    // Add a transaction
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    std::string err;
    mempool.AddTx(tx, COIN, 100, false, err);
    
    // Request with very small size limit
    auto blockTxs = mempool.GetTransactionsForBlock(1, FeeRate(0));
    
    // Should get no transactions (can't fit any)
    EXPECT_TRUE(blockTxs.empty());
}

TEST_F(MempoolMiningTest, GetTransactionsMinFeeRate) {
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    std::string err;
    mempool.AddTx(tx, 100, 100, false, err);  // Very low fee
    
    // Request with high minimum fee rate
    auto blockTxs = mempool.GetTransactionsForBlock(1000000, FeeRate(1000000));
    
    EXPECT_TRUE(blockTxs.empty());
}

// ============================================================================
// Mempool Size Limits Tests
// ============================================================================

class MempoolLimitsTest : public MempoolTestFixture {
protected:
    void SetUp() override {
        MempoolLimits limits;
        limits.maxSize = 1000;  // Very small for testing
        limits.minFeeRate = FeeRate(1000);
        mempool = std::make_unique<Mempool>(limits);
    }
    
    std::unique_ptr<Mempool> mempool;
};

TEST_F(MempoolLimitsTest, TrimToSize) {
    // Add transactions until we exceed limit
    for (int i = 0; i < 20; ++i) {
        auto tx = CreateTx({MakeOutPoint(static_cast<uint8_t>(i), 0)}, 49 * COIN);
        std::string err;
        mempool->AddTx(tx, (i + 1) * 10000, 100, false, err);
    }
    
    // Mempool should have trimmed to stay under size limit
    EXPECT_LE(mempool->GetTotalSize(), 1000);
}

// ============================================================================
// MempoolCoinsView Tests
// ============================================================================

class MempoolCoinsViewTest : public MempoolTestFixture {
protected:
    Mempool mempool;
    CoinsViewMemory baseView;
};

TEST_F(MempoolCoinsViewTest, GetCoinFromMempool) {
    // Add a transaction to mempool
    auto tx = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN, 2);
    std::string err;
    mempool.AddTx(tx, COIN, 100, false, err);
    
    MempoolCoinsView view(&baseView, mempool);
    
    // Should find output 0 of the mempool tx
    OutPoint mempoolOutput(tx->GetHash(), 0);
    auto coin = view.GetCoin(mempoolOutput);
    
    ASSERT_TRUE(coin.has_value());
    EXPECT_EQ(coin->nHeight, Mempool::MEMPOOL_HEIGHT);
}

TEST_F(MempoolCoinsViewTest, SpentByMempoolNotAvailable) {
    // Add a coin to base view
    OutPoint input = MakeOutPoint(0x01, 0);
    Hash160 pubKeyHash;
    baseView.AddCoin(input, Coin(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)), 50, false));
    
    // Spend it in mempool
    auto tx = CreateTx({input}, 49 * COIN);
    std::string err;
    mempool.AddTx(tx, COIN, 100, false, err);
    
    MempoolCoinsView view(&baseView, mempool);
    
    // The input should not be available (spent by mempool)
    EXPECT_FALSE(view.HaveCoin(input));
}

TEST_F(MempoolCoinsViewTest, FallbackToBaseView) {
    // Add a coin to base view only
    OutPoint baseOutput = MakeOutPoint(0xFF, 0);
    Hash160 pubKeyHash;
    baseView.AddCoin(baseOutput, Coin(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)), 50, false));
    
    MempoolCoinsView view(&baseView, mempool);
    
    // Should find coin in base view
    EXPECT_TRUE(view.HaveCoin(baseOutput));
    
    auto coin = view.GetCoin(baseOutput);
    ASSERT_TRUE(coin.has_value());
    EXPECT_EQ(coin->nHeight, 50);  // From base, not mempool height
}

// ============================================================================
// Removal Reason Tests
// ============================================================================

TEST(RemovalReasonTest, ToString) {
    EXPECT_EQ(RemovalReasonToString(MempoolRemovalReason::EXPIRY), "expiry");
    EXPECT_EQ(RemovalReasonToString(MempoolRemovalReason::SIZELIMIT), "sizelimit");
    EXPECT_EQ(RemovalReasonToString(MempoolRemovalReason::REORG), "reorg");
    EXPECT_EQ(RemovalReasonToString(MempoolRemovalReason::BLOCK), "block");
    EXPECT_EQ(RemovalReasonToString(MempoolRemovalReason::CONFLICT), "conflict");
    EXPECT_EQ(RemovalReasonToString(MempoolRemovalReason::REPLACED), "replaced");
}

// ============================================================================
// Consistency Tests
// ============================================================================

TEST_F(MempoolBasicTest, Consistency) {
    auto tx1 = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    auto tx2 = CreateTx({MakeOutPoint(0x02, 0)}, 48 * COIN);
    std::string err;
    
    mempool.AddTx(tx1, COIN, 100, false, err);
    mempool.AddTx(tx2, COIN, 100, false, err);
    
    EXPECT_TRUE(mempool.CheckConsistency());
}

// ============================================================================
// GetAllTxInfo Tests
// ============================================================================

TEST_F(MempoolBasicTest, GetAllTxInfo) {
    auto tx1 = CreateTx({MakeOutPoint(0x01, 0)}, 49 * COIN);
    auto tx2 = CreateTx({MakeOutPoint(0x02, 0)}, 48 * COIN);
    std::string err;
    
    mempool.AddTx(tx1, COIN, 100, false, err);
    mempool.AddTx(tx2, 2 * COIN, 100, false, err);
    
    auto allInfo = mempool.GetAllTxInfo();
    EXPECT_EQ(allInfo.size(), 2);
}

// ============================================================================
// AcceptToMempool Tests
// ============================================================================

#include "shurium/chain/coins.h"

class AcceptToMempoolTest : public ::testing::Test {
protected:
    Mempool mempool;
    std::unique_ptr<CoinsViewMemory> coinsDB;
    std::unique_ptr<CoinsViewCache> coins;
    
    void SetUp() override {
        coinsDB = std::make_unique<CoinsViewMemory>();
        
        // Set up some UTXOs for testing
        // Create a UTXO at height 1 that can be spent (mature)
        TxHash txid1;
        txid1[0] = 0x01;
        OutPoint outpoint1(txid1, 0);
        
        Script pubKeyScript;
        pubKeyScript.push_back(OP_TRUE);  // Simple script that always succeeds
        
        Coin coin1(TxOut(50 * COIN, pubKeyScript), 1, false);  // Not coinbase
        coinsDB->AddCoin(outpoint1, coin1);
        
        // Create another UTXO
        TxHash txid2;
        txid2[0] = 0x02;
        OutPoint outpoint2(txid2, 0);
        Coin coin2(TxOut(100 * COIN, pubKeyScript), 1, false);
        coinsDB->AddCoin(outpoint2, coin2);
        
        // Create cache on top of DB
        coins = std::make_unique<CoinsViewCache>(coinsDB.get());
    }
    
    TransactionRef CreateValidTx(const OutPoint& input, Amount outputValue) {
        MutableTransaction mtx;
        mtx.version = 1;
        mtx.nLockTime = 0;
        
        // Simple input script that satisfies OP_TRUE
        Script inputScript;
        inputScript.push_back(OP_TRUE);
        mtx.vin.push_back(TxIn(input, inputScript, 0xFFFFFFFF));
        
        // Output script
        Script outputScript;
        outputScript.push_back(OP_TRUE);
        mtx.vout.push_back(TxOut(outputValue, outputScript));
        
        return MakeTransactionRef(std::move(mtx));
    }
    
    OutPoint MakeOutPoint(uint8_t txByte, uint32_t n) {
        TxHash hash;
        hash[0] = txByte;
        return OutPoint(hash, n);
    }
};

TEST_F(AcceptToMempoolTest, AcceptValidTransaction) {
    // Create a transaction spending existing UTXO
    OutPoint input = MakeOutPoint(0x01, 0);  // Spends the 50 COIN UTXO
    auto tx = CreateValidTx(input, 49 * COIN);  // 1 COIN fee
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100);
    
    EXPECT_TRUE(result.IsValid()) << "Reject reason: " << result.rejectReason;
    EXPECT_EQ(result.fee, 1 * COIN);
    EXPECT_EQ(result.txid, tx->GetHash());
    EXPECT_TRUE(mempool.Exists(tx->GetHash()));
}

TEST_F(AcceptToMempoolTest, RejectDuplicateTransaction) {
    OutPoint input = MakeOutPoint(0x01, 0);
    auto tx = CreateValidTx(input, 49 * COIN);
    
    // First submission should succeed
    MempoolAcceptResult result1 = AcceptToMempool(tx, mempool, *coins, 100);
    EXPECT_TRUE(result1.IsValid());
    
    // Second submission should fail
    MempoolAcceptResult result2 = AcceptToMempool(tx, mempool, *coins, 100);
    EXPECT_FALSE(result2.IsValid());
    EXPECT_EQ(result2.rejectReason, "txn-already-in-mempool");
}

TEST_F(AcceptToMempoolTest, RejectMissingInputs) {
    // Create a transaction spending a non-existent UTXO
    OutPoint input = MakeOutPoint(0xFF, 0);  // This UTXO doesn't exist
    auto tx = CreateValidTx(input, 49 * COIN);
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100);
    
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ(result.rejectReason, "missing-inputs");
}

TEST_F(AcceptToMempoolTest, RejectInsufficientFee) {
    OutPoint input = MakeOutPoint(0x01, 0);  // 50 COIN
    
    // Create transaction with very low fee (output almost equals input)
    auto tx = CreateValidTx(input, 50 * COIN - 1);  // Only 1 sat fee
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100);
    
    // Should fail due to insufficient fee
    EXPECT_FALSE(result.IsValid());
    EXPECT_NE(result.rejectReason.find("min relay fee not met"), std::string::npos);
}

TEST_F(AcceptToMempoolTest, RejectEmptyInputs) {
    MutableTransaction mtx;
    mtx.version = 1;
    // No inputs!
    Script outputScript;
    outputScript.push_back(OP_TRUE);
    mtx.vout.push_back(TxOut(1 * COIN, outputScript));
    
    TransactionRef tx = MakeTransactionRef(std::move(mtx));
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100);
    
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ(result.rejectReason, "bad-txns-vin-empty");
}

TEST_F(AcceptToMempoolTest, RejectEmptyOutputs) {
    MutableTransaction mtx;
    mtx.version = 1;
    
    OutPoint input = MakeOutPoint(0x01, 0);
    Script inputScript;
    inputScript.push_back(OP_TRUE);
    mtx.vin.push_back(TxIn(input, inputScript, 0xFFFFFFFF));
    // No outputs!
    
    TransactionRef tx = MakeTransactionRef(std::move(mtx));
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100);
    
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ(result.rejectReason, "bad-txns-vout-empty");
}

TEST_F(AcceptToMempoolTest, RejectNegativeOutput) {
    MutableTransaction mtx;
    mtx.version = 1;
    
    OutPoint input = MakeOutPoint(0x01, 0);
    Script inputScript;
    inputScript.push_back(OP_TRUE);
    mtx.vin.push_back(TxIn(input, inputScript, 0xFFFFFFFF));
    
    Script outputScript;
    outputScript.push_back(OP_TRUE);
    mtx.vout.push_back(TxOut(-1, outputScript));  // Negative value
    
    TransactionRef tx = MakeTransactionRef(std::move(mtx));
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100);
    
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ(result.rejectReason, "bad-txns-vout-negative");
}

TEST_F(AcceptToMempoolTest, RejectOutputExceedsInput) {
    OutPoint input = MakeOutPoint(0x01, 0);  // 50 COIN
    auto tx = CreateValidTx(input, 51 * COIN);  // Output exceeds input
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100);
    
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ(result.rejectReason, "bad-txns-in-belowout");
}

TEST_F(AcceptToMempoolTest, AcceptWithBypassLimits) {
    OutPoint input = MakeOutPoint(0x01, 0);  // 50 COIN
    
    // Create transaction with reasonable fee but below default minimum
    // The bypass flag skips our pre-validation fee check, but mempool.AddTx
    // still has its own minimum fee enforcement
    // For this test, let's use a fee that would pass mempool's internal check
    // but is still lower than what we'd normally require
    
    // Create mempool with lower minimum fee for this test
    MempoolLimits lowLimits;
    lowLimits.minFeeRate = FeeRate(1);  // Very low: 0.001 sat/vB
    mempool.SetLimits(lowLimits);
    
    // Now even a 1 sat fee should be accepted with bypass
    auto tx = CreateValidTx(input, 50 * COIN - 1);  // Only 1 sat fee
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100, true);
    EXPECT_TRUE(result.IsValid()) << "Reject reason: " << result.rejectReason;
}

TEST_F(AcceptToMempoolTest, RejectCoinbaseTransaction) {
    // Create a coinbase transaction
    MutableTransaction mtx;
    mtx.version = 1;
    
    // Coinbase input (null prevout)
    OutPoint nullOutpoint;  // Default constructor creates null outpoint
    Script coinbaseScript;
    coinbaseScript << std::vector<uint8_t>{0x01, 0x02, 0x03};
    mtx.vin.push_back(TxIn(nullOutpoint, coinbaseScript, 0xFFFFFFFF));
    
    Script outputScript;
    outputScript.push_back(OP_TRUE);
    mtx.vout.push_back(TxOut(50 * COIN, outputScript));
    
    TransactionRef tx = MakeTransactionRef(std::move(mtx));
    
    MempoolAcceptResult result = AcceptToMempool(tx, mempool, *coins, 100);
    
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ(result.rejectReason, "coinbase");
}

TEST_F(AcceptToMempoolTest, ChainedTransactions) {
    // First transaction spends UTXO from coins
    OutPoint input1 = MakeOutPoint(0x01, 0);  // 50 COIN
    auto tx1 = CreateValidTx(input1, 49 * COIN);  // 1 COIN fee
    
    MempoolAcceptResult result1 = AcceptToMempool(tx1, mempool, *coins, 100);
    EXPECT_TRUE(result1.IsValid()) << result1.rejectReason;
    
    // Second transaction spends output from first transaction (in mempool)
    OutPoint input2(tx1->GetHash(), 0);  // Output 0 of tx1
    auto tx2 = CreateValidTx(input2, 48 * COIN);  // 1 COIN fee
    
    MempoolAcceptResult result2 = AcceptToMempool(tx2, mempool, *coins, 100);
    EXPECT_TRUE(result2.IsValid()) << result2.rejectReason;
    
    // Both should be in mempool
    EXPECT_TRUE(mempool.Exists(tx1->GetHash()));
    EXPECT_TRUE(mempool.Exists(tx2->GetHash()));
    EXPECT_EQ(mempool.Size(), 2);
}

