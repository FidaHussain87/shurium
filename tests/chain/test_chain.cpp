// SHURIUM - Chain State Module Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/chain/coins.h"
#include "shurium/chain/blockindex.h"
#include "shurium/chain/chainstate.h"
#include "shurium/core/block.h"
#include "shurium/core/transaction.h"

using namespace shurium;

// ============================================================================
// Coin Tests
// ============================================================================

class CoinTest : public ::testing::Test {
protected:
    TxOut testOutput;
    
    void SetUp() override {
        Hash160 pubKeyHash;
        pubKeyHash[0] = 0xAB;
        testOutput = TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash));
    }
};

TEST_F(CoinTest, DefaultConstructor) {
    Coin coin;
    EXPECT_TRUE(coin.IsSpent());
    EXPECT_FALSE(coin.IsCoinBase());
    EXPECT_EQ(coin.nHeight, 0);
}

TEST_F(CoinTest, ConstructFromTxOut) {
    Coin coin(testOutput, 100, true);
    EXPECT_FALSE(coin.IsSpent());
    EXPECT_TRUE(coin.IsCoinBase());
    EXPECT_EQ(coin.nHeight, 100);
    EXPECT_EQ(coin.GetAmount(), 50 * COIN);
}

TEST_F(CoinTest, MoveConstruct) {
    TxOut output = testOutput;
    Coin coin(std::move(output), 200, false);
    EXPECT_FALSE(coin.IsSpent());
    EXPECT_FALSE(coin.IsCoinBase());
    EXPECT_EQ(coin.nHeight, 200);
}

TEST_F(CoinTest, Clear) {
    Coin coin(testOutput, 100, true);
    EXPECT_FALSE(coin.IsSpent());
    
    coin.Clear();
    EXPECT_TRUE(coin.IsSpent());
    EXPECT_FALSE(coin.IsCoinBase());
    EXPECT_EQ(coin.nHeight, 0);
}

TEST_F(CoinTest, CoinbaseMaturity) {
    // Coinbase coin at height 100
    Coin coinbaseCoin(testOutput, 100, true);
    
    // Should not be mature until 100 + MATURITY (100) = 200
    EXPECT_FALSE(coinbaseCoin.IsMature(150));
    EXPECT_FALSE(coinbaseCoin.IsMature(199));
    EXPECT_TRUE(coinbaseCoin.IsMature(200));
    EXPECT_TRUE(coinbaseCoin.IsMature(300));
    
    // Non-coinbase is always mature
    Coin regularCoin(testOutput, 100, false);
    EXPECT_TRUE(regularCoin.IsMature(100));
    EXPECT_TRUE(regularCoin.IsMature(0));
}

TEST_F(CoinTest, Equality) {
    Coin coin1(testOutput, 100, true);
    Coin coin2(testOutput, 100, true);
    Coin coin3(testOutput, 100, false);
    Coin coin4(testOutput, 101, true);
    
    EXPECT_EQ(coin1, coin2);
    EXPECT_NE(coin1, coin3);  // Different coinbase flag
    EXPECT_NE(coin1, coin4);  // Different height
}

// ============================================================================
// CoinsViewMemory Tests
// ============================================================================

class CoinsViewMemoryTest : public ::testing::Test {
protected:
    CoinsViewMemory view;
    OutPoint testOutpoint;
    Coin testCoin;
    
    void SetUp() override {
        TxHash txhash;
        txhash[0] = 0xDE;
        txhash[1] = 0xAD;
        testOutpoint = OutPoint(txhash, 0);
        
        Hash160 pubKeyHash;
        testCoin = Coin(TxOut(100 * COIN, Script::CreateP2PKH(pubKeyHash)), 50, false);
    }
};

TEST_F(CoinsViewMemoryTest, EmptyView) {
    EXPECT_FALSE(view.HaveCoin(testOutpoint));
    EXPECT_FALSE(view.GetCoin(testOutpoint).has_value());
    EXPECT_TRUE(view.GetBestBlock().IsNull());
    EXPECT_EQ(view.EstimateSize(), 0);
}

TEST_F(CoinsViewMemoryTest, AddAndRetrieveCoin) {
    view.AddCoin(testOutpoint, testCoin);
    
    EXPECT_TRUE(view.HaveCoin(testOutpoint));
    EXPECT_EQ(view.EstimateSize(), 1);
    
    auto retrieved = view.GetCoin(testOutpoint);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->GetAmount(), 100 * COIN);
    EXPECT_EQ(retrieved->nHeight, 50);
}

TEST_F(CoinsViewMemoryTest, RemoveCoin) {
    view.AddCoin(testOutpoint, testCoin);
    EXPECT_TRUE(view.HaveCoin(testOutpoint));
    
    view.RemoveCoin(testOutpoint);
    EXPECT_FALSE(view.HaveCoin(testOutpoint));
}

TEST_F(CoinsViewMemoryTest, SetBestBlock) {
    BlockHash hash;
    hash[0] = 0xBE;
    hash[1] = 0xEF;
    
    view.SetBestBlock(hash);
    EXPECT_EQ(view.GetBestBlock(), hash);
}

TEST_F(CoinsViewMemoryTest, Clear) {
    view.AddCoin(testOutpoint, testCoin);
    BlockHash hash;
    hash[0] = 0xBE;
    view.SetBestBlock(hash);
    
    view.Clear();
    EXPECT_FALSE(view.HaveCoin(testOutpoint));
    EXPECT_TRUE(view.GetBestBlock().IsNull());
}

// ============================================================================
// CoinsViewCache Tests
// ============================================================================

class CoinsViewCacheTest : public ::testing::Test {
protected:
    std::unique_ptr<CoinsViewMemory> baseView;
    std::unique_ptr<CoinsViewCache> cache;
    OutPoint outpoint1, outpoint2;
    Coin coin1, coin2;
    
    void SetUp() override {
        baseView = std::make_unique<CoinsViewMemory>();
        cache = std::make_unique<CoinsViewCache>(baseView.get());
        
        TxHash txhash1, txhash2;
        txhash1[0] = 0x01;
        txhash2[0] = 0x02;
        
        outpoint1 = OutPoint(txhash1, 0);
        outpoint2 = OutPoint(txhash2, 1);
        
        Hash160 pubKeyHash;
        coin1 = Coin(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)), 100, false);
        coin2 = Coin(TxOut(75 * COIN, Script::CreateP2PKH(pubKeyHash)), 150, true);
    }
};

TEST_F(CoinsViewCacheTest, FetchFromBase) {
    // Add to base view
    baseView->AddCoin(outpoint1, coin1);
    
    // Should be accessible through cache
    EXPECT_TRUE(cache->HaveCoin(outpoint1));
    auto retrieved = cache->GetCoin(outpoint1);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->GetAmount(), 50 * COIN);
}

TEST_F(CoinsViewCacheTest, AddCoin) {
    cache->AddCoin(outpoint1, Coin(coin1), false);
    
    EXPECT_TRUE(cache->HaveCoin(outpoint1));
    EXPECT_TRUE(cache->HaveCoinInCache(outpoint1));
    
    // Base view should not have it yet
    EXPECT_FALSE(baseView->HaveCoin(outpoint1));
}

TEST_F(CoinsViewCacheTest, SpendCoin) {
    cache->AddCoin(outpoint1, Coin(coin1), false);
    EXPECT_TRUE(cache->HaveCoin(outpoint1));
    
    Coin spentCoin;
    bool success = cache->SpendCoin(outpoint1, &spentCoin);
    EXPECT_TRUE(success);
    EXPECT_EQ(spentCoin.GetAmount(), 50 * COIN);
    EXPECT_FALSE(cache->HaveCoin(outpoint1));
}

TEST_F(CoinsViewCacheTest, SpendNonexistent) {
    EXPECT_FALSE(cache->SpendCoin(outpoint1));
}

TEST_F(CoinsViewCacheTest, AccessCoin) {
    cache->AddCoin(outpoint1, Coin(coin1), false);
    
    const Coin& accessed = cache->AccessCoin(outpoint1);
    EXPECT_FALSE(accessed.IsSpent());
    EXPECT_EQ(accessed.GetAmount(), 50 * COIN);
    
    // Accessing nonexistent returns empty coin
    const Coin& empty = cache->AccessCoin(outpoint2);
    EXPECT_TRUE(empty.IsSpent());
}

TEST_F(CoinsViewCacheTest, BestBlock) {
    BlockHash hash;
    hash[0] = 0xCA;
    hash[1] = 0xFE;
    
    cache->SetBestBlock(hash);
    EXPECT_EQ(cache->GetBestBlock(), hash);
}

TEST_F(CoinsViewCacheTest, GetCacheSize) {
    EXPECT_EQ(cache->GetCacheSize(), 0);
    
    cache->AddCoin(outpoint1, Coin(coin1), false);
    EXPECT_EQ(cache->GetCacheSize(), 1);
    
    cache->AddCoin(outpoint2, Coin(coin2), false);
    EXPECT_EQ(cache->GetCacheSize(), 2);
}

TEST_F(CoinsViewCacheTest, Reset) {
    cache->AddCoin(outpoint1, Coin(coin1), false);
    BlockHash hash;
    hash[0] = 0xCA;
    cache->SetBestBlock(hash);
    
    cache->Reset();
    EXPECT_FALSE(cache->HaveCoinInCache(outpoint1));
    EXPECT_EQ(cache->GetCacheSize(), 0);
}

TEST_F(CoinsViewCacheTest, HaveInputs) {
    // Create a transaction
    MutableTransaction mtx;
    mtx.vin.push_back(TxIn(outpoint1));
    mtx.vout.push_back(TxOut(25 * COIN, Script()));
    Transaction tx(mtx);
    
    // Inputs not available
    EXPECT_FALSE(cache->HaveInputs(tx));
    
    // Add the input
    cache->AddCoin(outpoint1, Coin(coin1), false);
    EXPECT_TRUE(cache->HaveInputs(tx));
}

TEST_F(CoinsViewCacheTest, GetValueIn) {
    cache->AddCoin(outpoint1, Coin(coin1), false);
    cache->AddCoin(outpoint2, Coin(coin2), false);
    
    MutableTransaction mtx;
    mtx.vin.push_back(TxIn(outpoint1));
    mtx.vin.push_back(TxIn(outpoint2));
    mtx.vout.push_back(TxOut(100 * COIN, Script()));
    Transaction tx(mtx);
    
    Amount valueIn = cache->GetValueIn(tx);
    EXPECT_EQ(valueIn, 50 * COIN + 75 * COIN);  // 125 COIN total
}

// ============================================================================
// BlockIndex Tests
// ============================================================================

class BlockIndexTest : public ::testing::Test {
protected:
    BlockHeader testHeader;
    
    void SetUp() override {
        testHeader.nVersion = 1;
        testHeader.nTime = 1700000000;
        testHeader.nBits = 0x1d00ffff;
        testHeader.nNonce = 12345;
    }
};

TEST_F(BlockIndexTest, DefaultConstructor) {
    BlockIndex index;
    EXPECT_EQ(index.nHeight, 0);
    EXPECT_EQ(index.pprev, nullptr);
    EXPECT_EQ(index.nStatus, BlockStatus::UNKNOWN);
}

TEST_F(BlockIndexTest, ConstructFromHeader) {
    BlockIndex index(testHeader);
    EXPECT_EQ(index.nVersion, testHeader.nVersion);
    EXPECT_EQ(index.nTime, testHeader.nTime);
    EXPECT_EQ(index.nBits, testHeader.nBits);
    EXPECT_EQ(index.nNonce, testHeader.nNonce);
}

TEST_F(BlockIndexTest, GetBlockTime) {
    BlockIndex index(testHeader);
    EXPECT_EQ(index.GetBlockTime(), 1700000000);
}

TEST_F(BlockIndexTest, ValidityLevels) {
    BlockIndex index;
    
    // Initially unknown
    EXPECT_FALSE(index.IsValid(BlockStatus::VALID_HEADER));
    
    // Raise to VALID_HEADER
    EXPECT_TRUE(index.RaiseValidity(BlockStatus::VALID_HEADER));
    EXPECT_TRUE(index.IsValid(BlockStatus::VALID_HEADER));
    EXPECT_FALSE(index.IsValid(BlockStatus::VALID_TRANSACTIONS));
    
    // Can't raise twice to same level
    EXPECT_FALSE(index.RaiseValidity(BlockStatus::VALID_HEADER));
    
    // Raise to VALID_TRANSACTIONS
    EXPECT_TRUE(index.RaiseValidity(BlockStatus::VALID_TRANSACTIONS));
    EXPECT_TRUE(index.IsValid(BlockStatus::VALID_HEADER));
    EXPECT_TRUE(index.IsValid(BlockStatus::VALID_TRANSACTIONS));
}

TEST_F(BlockIndexTest, FailedBlock) {
    BlockIndex index;
    index.RaiseValidity(BlockStatus::VALID_HEADER);
    
    // Mark as failed
    index.nStatus = index.nStatus | BlockStatus::FAILED_VALID;
    
    EXPECT_TRUE(index.IsFailed());
    EXPECT_FALSE(index.IsValid(BlockStatus::VALID_HEADER));
    EXPECT_FALSE(index.RaiseValidity(BlockStatus::VALID_TRANSACTIONS));
}

TEST_F(BlockIndexTest, DataFlags) {
    BlockIndex index;
    EXPECT_FALSE(index.HaveData());
    EXPECT_FALSE(index.HaveUndo());
    
    index.nStatus = index.nStatus | BlockStatus::HAVE_DATA;
    EXPECT_TRUE(index.HaveData());
    EXPECT_FALSE(index.HaveUndo());
    
    index.nStatus = index.nStatus | BlockStatus::HAVE_UNDO;
    EXPECT_TRUE(index.HaveData());
    EXPECT_TRUE(index.HaveUndo());
}

// ============================================================================
// Chain Tests
// ============================================================================

class ChainTest : public ::testing::Test {
protected:
    BlockMap blockMap;
    std::vector<BlockIndex*> indices;
    
    void SetUp() override {
        // Create a chain of 10 blocks
        BlockHash prevHash;
        for (int i = 0; i < 10; ++i) {
            BlockHeader header;
            header.nVersion = 1;
            header.hashPrevBlock = prevHash;
            header.nTime = 1700000000 + i * 30;
            header.nBits = 0x1d00ffff;
            header.nNonce = i;
            
            BlockHash hash = header.GetHash();
            
            auto pindex = std::make_unique<BlockIndex>(header);
            pindex->nHeight = i;
            
            if (!indices.empty()) {
                pindex->pprev = indices.back();
            }
            
            auto [it, inserted] = blockMap.emplace(hash, std::move(pindex));
            it->second->phashBlock = &it->first;
            indices.push_back(it->second.get());
            
            prevHash = hash;
        }
    }
};

TEST_F(ChainTest, EmptyChain) {
    Chain chain;
    EXPECT_TRUE(chain.empty());
    EXPECT_EQ(chain.Height(), -1);
    EXPECT_EQ(chain.Genesis(), nullptr);
    EXPECT_EQ(chain.Tip(), nullptr);
}

TEST_F(ChainTest, SetTip) {
    Chain chain;
    chain.SetTip(indices[5]);
    
    EXPECT_FALSE(chain.empty());
    EXPECT_EQ(chain.Height(), 5);
    EXPECT_EQ(chain.Genesis(), indices[0]);
    EXPECT_EQ(chain.Tip(), indices[5]);
}

TEST_F(ChainTest, AccessByHeight) {
    Chain chain;
    chain.SetTip(indices.back());
    
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(chain[i], indices[i]);
    }
    
    EXPECT_EQ(chain[-1], nullptr);
    EXPECT_EQ(chain[100], nullptr);
}

TEST_F(ChainTest, Contains) {
    Chain chain;
    chain.SetTip(indices[5]);
    
    EXPECT_TRUE(chain.Contains(indices[0]));
    EXPECT_TRUE(chain.Contains(indices[5]));
    EXPECT_FALSE(chain.Contains(indices[6]));
    EXPECT_FALSE(chain.Contains(nullptr));
}

TEST_F(ChainTest, Next) {
    Chain chain;
    chain.SetTip(indices[5]);
    
    EXPECT_EQ(chain.Next(indices[0]), indices[1]);
    EXPECT_EQ(chain.Next(indices[4]), indices[5]);
    EXPECT_EQ(chain.Next(indices[5]), nullptr);  // Tip has no next
    EXPECT_EQ(chain.Next(indices[6]), nullptr);  // Not in chain
}

TEST_F(ChainTest, FindFork) {
    // Set up main chain to index 5
    Chain chain;
    chain.SetTip(indices[5]);
    
    // Fork at index 3
    const BlockIndex* fork = chain.FindFork(indices[9]);
    EXPECT_EQ(fork, indices[5]);  // Last common block
    
    // Direct match
    fork = chain.FindFork(indices[3]);
    EXPECT_EQ(fork, indices[3]);
}

TEST_F(ChainTest, Clear) {
    Chain chain;
    chain.SetTip(indices[5]);
    EXPECT_FALSE(chain.empty());
    
    chain.Clear();
    EXPECT_TRUE(chain.empty());
}

// ============================================================================
// Block Ancestor Tests
// ============================================================================

TEST_F(ChainTest, GetAncestor) {
    // Set up skip pointers
    for (auto* pindex : indices) {
        pindex->BuildSkip();
    }
    
    BlockIndex* tip = indices.back();
    
    // Test ancestor lookups
    EXPECT_EQ(tip->GetAncestor(0), indices[0]);
    EXPECT_EQ(tip->GetAncestor(5), indices[5]);
    EXPECT_EQ(tip->GetAncestor(9), indices[9]);
    EXPECT_EQ(tip->GetAncestor(10), nullptr);  // Too high
    EXPECT_EQ(tip->GetAncestor(-1), nullptr);  // Negative
}

TEST_F(ChainTest, LastCommonAncestor) {
    // Build skip pointers
    for (auto* pindex : indices) {
        pindex->BuildSkip();
    }
    
    // Same chain - common ancestor is the lower one
    const BlockIndex* lca = LastCommonAncestor(indices[3], indices[7]);
    EXPECT_EQ(lca, indices[3]);
    
    lca = LastCommonAncestor(indices[7], indices[3]);
    EXPECT_EQ(lca, indices[3]);
    
    // Both same
    lca = LastCommonAncestor(indices[5], indices[5]);
    EXPECT_EQ(lca, indices[5]);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST(BlockProofTest, GetBlockProof) {
    // Easy difficulty
    uint64_t proof = GetBlockProof(0x1d00ffff);
    EXPECT_GT(proof, 0);
    
    // Harder difficulty (lower bits = more work)
    uint64_t harderProof = GetBlockProof(0x1c00ffff);
    EXPECT_GT(harderProof, proof);
}

TEST(LocatorTest, GetLocator) {
    // Test with nullptr
    BlockLocator locator = GetLocator(nullptr);
    EXPECT_TRUE(locator.vHave.empty());
}

// ============================================================================
// ChainStateManager Tests
// ============================================================================

class ChainStateManagerTest : public ::testing::Test {
protected:
    std::unique_ptr<CoinsViewMemory> coinsDB;
    std::unique_ptr<ChainStateManager> manager;
    
    void SetUp() override {
        coinsDB = std::make_unique<CoinsViewMemory>();
        manager = std::make_unique<ChainStateManager>(consensus::Params::RegTest());
        manager->Initialize(coinsDB.get());
    }
};

TEST_F(ChainStateManagerTest, InitialState) {
    EXPECT_EQ(manager->GetActiveHeight(), -1);
    EXPECT_EQ(manager->GetActiveTip(), nullptr);
    EXPECT_EQ(manager->GetBestHeader(), nullptr);
}

TEST_F(ChainStateManagerTest, AddBlockIndex) {
    BlockHeader header;
    header.nVersion = 1;
    header.nTime = 1700000000;
    header.nBits = 0x1d00ffff;
    header.nNonce = 123;
    
    BlockHash hash = header.GetHash();
    
    BlockIndex* pindex = manager->AddBlockIndex(hash, header);
    ASSERT_NE(pindex, nullptr);
    EXPECT_EQ(pindex->nVersion, 1);
    EXPECT_EQ(pindex->nHeight, 0);
    EXPECT_EQ(pindex->pprev, nullptr);
    
    // Adding same hash should return existing
    BlockIndex* pindex2 = manager->AddBlockIndex(hash, header);
    EXPECT_EQ(pindex, pindex2);
}

TEST_F(ChainStateManagerTest, LookupBlockIndex) {
    BlockHeader header;
    header.nVersion = 1;
    header.nTime = 1700000000;
    header.nBits = 0x1d00ffff;
    
    BlockHash hash = header.GetHash();
    
    // Not found initially
    EXPECT_EQ(manager->LookupBlockIndex(hash), nullptr);
    
    // Add and find
    manager->AddBlockIndex(hash, header);
    BlockIndex* found = manager->LookupBlockIndex(hash);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->GetBlockHash(), hash);
}

TEST_F(ChainStateManagerTest, ProcessBlockHeader) {
    // Create genesis
    BlockHeader genesis;
    genesis.nVersion = 1;
    genesis.nTime = 1700000000;
    genesis.nBits = 0x207fffff;  // Easy regtest difficulty
    genesis.nNonce = 0;
    
    BlockIndex* pindex = manager->ProcessBlockHeader(genesis);
    ASSERT_NE(pindex, nullptr);
    EXPECT_EQ(pindex->nHeight, 0);
    
    // Process again - should return same
    BlockIndex* pindex2 = manager->ProcessBlockHeader(genesis);
    EXPECT_EQ(pindex, pindex2);
}

TEST_F(ChainStateManagerTest, ChainOfHeaders) {
    // Create a chain of headers
    BlockHash prevHash;
    BlockIndex* lastIndex = nullptr;
    
    for (int i = 0; i < 5; ++i) {
        BlockHeader header;
        header.nVersion = 1;
        header.hashPrevBlock = prevHash;
        header.nTime = 1700000000 + i * 30;
        header.nBits = 0x207fffff;
        header.nNonce = i;
        
        BlockIndex* pindex = manager->ProcessBlockHeader(header);
        ASSERT_NE(pindex, nullptr);
        EXPECT_EQ(pindex->nHeight, i);
        
        if (lastIndex) {
            EXPECT_EQ(pindex->pprev, lastIndex);
        }
        
        prevHash = header.GetHash();
        lastIndex = pindex;
    }
    
    // Best header should be the last one
    EXPECT_EQ(manager->GetBestHeader(), lastIndex);
}

// ============================================================================
// BlockUndo Tests
// ============================================================================

TEST(BlockUndoTest, TxUndo) {
    TxUndo txundo;
    EXPECT_TRUE(txundo.empty());
    
    Hash160 pubKeyHash;
    txundo.vprevout.push_back(Coin(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)), 100, false));
    EXPECT_FALSE(txundo.empty());
    EXPECT_EQ(txundo.size(), 1);
    
    txundo.Clear();
    EXPECT_TRUE(txundo.empty());
}

TEST(BlockUndoTest, BlockUndo) {
    BlockUndo blockundo;
    EXPECT_TRUE(blockundo.empty());
    
    blockundo.vtxundo.resize(2);
    EXPECT_FALSE(blockundo.empty());
    EXPECT_EQ(blockundo.size(), 2);
    
    blockundo.Clear();
    EXPECT_TRUE(blockundo.empty());
}

// ============================================================================
// ConnectResult Tests
// ============================================================================

TEST(ConnectResultTest, IsSuccess) {
    EXPECT_TRUE(IsSuccess(ConnectResult::OK));
    EXPECT_FALSE(IsSuccess(ConnectResult::INVALID));
    EXPECT_FALSE(IsSuccess(ConnectResult::FAILED));
    EXPECT_FALSE(IsSuccess(ConnectResult::CONSENSUS_ERROR));
    EXPECT_FALSE(IsSuccess(ConnectResult::MISSING_INPUTS));
    EXPECT_FALSE(IsSuccess(ConnectResult::PREMATURE_SPEND));
    EXPECT_FALSE(IsSuccess(ConnectResult::DOUBLE_SPEND));
}

// ============================================================================
// Cache Entry Flags Tests
// ============================================================================

TEST(CoinsCacheEntryTest, Flags) {
    CoinsCacheEntry entry;
    EXPECT_FALSE(entry.IsDirty());
    EXPECT_FALSE(entry.IsFresh());
    
    entry.SetDirty();
    EXPECT_TRUE(entry.IsDirty());
    EXPECT_FALSE(entry.IsFresh());
    
    entry.SetFresh();
    EXPECT_TRUE(entry.IsDirty());
    EXPECT_TRUE(entry.IsFresh());
    
    entry.ClearFlags();
    EXPECT_FALSE(entry.IsDirty());
    EXPECT_FALSE(entry.IsFresh());
}

// ============================================================================
// OutPoint Hasher Tests
// ============================================================================

TEST(OutPointHasherTest, DifferentHashes) {
    OutPointHasher hasher;
    
    TxHash hash1, hash2;
    hash1[0] = 0x01;
    hash2[0] = 0x02;
    
    OutPoint op1(hash1, 0);
    OutPoint op2(hash2, 0);
    OutPoint op3(hash1, 1);
    
    // Different outpoints should have different hashes
    EXPECT_NE(hasher(op1), hasher(op2));
    EXPECT_NE(hasher(op1), hasher(op3));
    
    // Same outpoint should have same hash
    OutPoint op1_copy(hash1, 0);
    EXPECT_EQ(hasher(op1), hasher(op1_copy));
}
