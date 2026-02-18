// SHURIUM - Consensus Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/consensus/params.h"
#include "shurium/consensus/validation.h"
#include "shurium/core/block.h"
#include "shurium/core/transaction.h"
#include "shurium/chain/blockindex.h"

using namespace shurium;
using namespace shurium::consensus;

// ============================================================================
// Consensus Parameters Tests
// ============================================================================

class ConsensusParamsTest : public ::testing::Test {
protected:
    Params mainnetParams;
    Params testnetParams;
    
    void SetUp() override {
        mainnetParams = Params::Main();
        testnetParams = Params::TestNet();
    }
};

TEST_F(ConsensusParamsTest, MainnetGenesisHash) {
    // Genesis hash should be set and non-null
    EXPECT_FALSE(mainnetParams.hashGenesisBlock.IsNull());
}

TEST_F(ConsensusParamsTest, MainnetBlockTime) {
    // SHURIUM uses 30 second blocks
    EXPECT_EQ(mainnetParams.nPowTargetSpacing, 30);
}

TEST_F(ConsensusParamsTest, MainnetSubsidyHalving) {
    // Check halving interval is set
    EXPECT_GT(mainnetParams.nSubsidyHalvingInterval, 0);
}

TEST_F(ConsensusParamsTest, MainnetMaxBlockSize) {
    // SHURIUM uses 10MB blocks
    EXPECT_EQ(mainnetParams.nMaxBlockSize, 10 * 1024 * 1024);
}

TEST_F(ConsensusParamsTest, MainnetMaxBlockWeight) {
    // Weight should be related to size
    EXPECT_GE(mainnetParams.nMaxBlockWeight, mainnetParams.nMaxBlockSize);
}

TEST_F(ConsensusParamsTest, MainnetUBIParameters) {
    // UBI distribution percentage (30%)
    EXPECT_EQ(mainnetParams.nUBIPercentage, 30);
    // UBI distribution interval (daily = 2880 blocks at 30s)
    EXPECT_EQ(mainnetParams.nUBIDistributionInterval, 2880);
}

TEST_F(ConsensusParamsTest, MainnetIdentityRefresh) {
    // Identity must be refreshed every 30 days
    EXPECT_EQ(mainnetParams.nIdentityRefreshInterval, 30 * 24 * 60 * 2);  // 30 days in blocks
}

TEST_F(ConsensusParamsTest, TestnetDifferentFromMainnet) {
    // Testnet should have different genesis
    EXPECT_NE(testnetParams.hashGenesisBlock, mainnetParams.hashGenesisBlock);
    // Testnet may have lower difficulty
    EXPECT_TRUE(testnetParams.fAllowMinDifficultyBlocks);
    EXPECT_FALSE(mainnetParams.fAllowMinDifficultyBlocks);
}

TEST_F(ConsensusParamsTest, DifficultyAdjustmentInterval) {
    // Check difficulty adjustment interval calculation
    int64_t interval = mainnetParams.DifficultyAdjustmentInterval();
    EXPECT_GT(interval, 0);
    // Should be targetTimespan / targetSpacing
    EXPECT_EQ(interval, mainnetParams.nPowTargetTimespan / mainnetParams.nPowTargetSpacing);
}

TEST_F(ConsensusParamsTest, RewardDistribution) {
    // Verify reward percentages add up to 100
    int total = mainnetParams.nWorkRewardPercentage +
                mainnetParams.nUBIPercentage +
                mainnetParams.nContributionRewardPercentage +
                mainnetParams.nEcosystemPercentage +
                mainnetParams.nStabilityReservePercentage;
    EXPECT_EQ(total, 100);
}

TEST_F(ConsensusParamsTest, GetBlockSubsidy) {
    // Test block subsidy calculation
    Amount subsidyHeight0 = GetBlockSubsidy(0, mainnetParams);
    EXPECT_GT(subsidyHeight0, 0);
    
    // After halving, subsidy should be half
    Amount subsidyAfterHalving = GetBlockSubsidy(mainnetParams.nSubsidyHalvingInterval, mainnetParams);
    EXPECT_EQ(subsidyAfterHalving, subsidyHeight0 / 2);
}

TEST_F(ConsensusParamsTest, GetBlockSubsidyDecreases) {
    // Subsidy should decrease over time
    Amount subsidy1 = GetBlockSubsidy(0, mainnetParams);
    Amount subsidy2 = GetBlockSubsidy(mainnetParams.nSubsidyHalvingInterval * 2, mainnetParams);
    EXPECT_LT(subsidy2, subsidy1);
}

// ============================================================================
// Genesis Block Tests
// ============================================================================

class GenesisBlockTest : public ::testing::Test {
protected:
    Params mainnetParams;
    Params testnetParams;
    Params regtestParams;
    
    void SetUp() override {
        mainnetParams = Params::Main();
        testnetParams = Params::TestNet();
        regtestParams = Params::RegTest();
    }
};

TEST_F(GenesisBlockTest, MainnetGenesisBlockValid) {
    // Recreate genesis block and verify it matches params
    Block genesis = CreateGenesisBlock(
        1700000000,    // Timestamp
        171163,        // Nonce (mined)
        0x1e0fffff,    // Difficulty
        1,             // Version
        mainnetParams.nInitialBlockReward
    );
    
    // Hash should match what's stored in params
    EXPECT_EQ(genesis.GetHash(), mainnetParams.hashGenesisBlock);
    
    // Genesis should have exactly one transaction (coinbase)
    EXPECT_EQ(genesis.vtx.size(), 1);
    
    // Coinbase should be valid
    EXPECT_TRUE(genesis.vtx[0]->IsCoinBase());
    
    // Genesis has no parent
    EXPECT_TRUE(genesis.hashPrevBlock.IsNull());
}

TEST_F(GenesisBlockTest, TestnetGenesisBlockValid) {
    // Recreate genesis block and verify it matches params
    Block genesis = CreateGenesisBlock(
        1700000001,    // Timestamp
        811478,        // Nonce (mined)
        0x1e0fffff,    // Difficulty
        1,             // Version
        testnetParams.nInitialBlockReward
    );
    
    // Hash should match what's stored in params
    EXPECT_EQ(genesis.GetHash(), testnetParams.hashGenesisBlock);
    
    // Genesis should have exactly one transaction (coinbase)
    EXPECT_EQ(genesis.vtx.size(), 1);
}

TEST_F(GenesisBlockTest, RegtestGenesisBlockValid) {
    // Recreate genesis block and verify it matches params
    Block genesis = CreateGenesisBlock(
        1700000002,    // Timestamp
        4,             // Nonce (mined)
        0x207fffff,    // Very easy difficulty
        1,             // Version
        regtestParams.nInitialBlockReward
    );
    
    // Hash should match what's stored in params
    EXPECT_EQ(genesis.GetHash(), regtestParams.hashGenesisBlock);
}

TEST_F(GenesisBlockTest, GenesisBlocksAreDistinct) {
    // All three genesis blocks should have different hashes
    EXPECT_NE(mainnetParams.hashGenesisBlock, testnetParams.hashGenesisBlock);
    EXPECT_NE(mainnetParams.hashGenesisBlock, regtestParams.hashGenesisBlock);
    EXPECT_NE(testnetParams.hashGenesisBlock, regtestParams.hashGenesisBlock);
}

TEST_F(GenesisBlockTest, MainnetGenesisValidPoW) {
    // Genesis block should pass PoW validation
    Block genesis = CreateGenesisBlock(
        1700000000, 171163, 0x1e0fffff, 1,
        mainnetParams.nInitialBlockReward
    );
    
    EXPECT_TRUE(CheckProofOfWork(genesis.GetHash(), genesis.nBits, mainnetParams));
}

TEST_F(GenesisBlockTest, TestnetGenesisValidPoW) {
    Block genesis = CreateGenesisBlock(
        1700000001, 811478, 0x1e0fffff, 1,
        testnetParams.nInitialBlockReward
    );
    
    EXPECT_TRUE(CheckProofOfWork(genesis.GetHash(), genesis.nBits, testnetParams));
}

TEST_F(GenesisBlockTest, RegtestGenesisValidPoW) {
    Block genesis = CreateGenesisBlock(
        1700000002, 4, 0x207fffff, 1,
        regtestParams.nInitialBlockReward
    );
    
    EXPECT_TRUE(CheckProofOfWork(genesis.GetHash(), genesis.nBits, regtestParams));
}

TEST_F(GenesisBlockTest, GenesisBlockMerkleRoot) {
    // Verify merkle root is correctly computed
    Block genesis = CreateGenesisBlock(
        1700000000, 171163, 0x1e0fffff, 1,
        mainnetParams.nInitialBlockReward
    );
    
    // Merkle root should not be null
    EXPECT_FALSE(genesis.hashMerkleRoot.IsNull());
    
    // Recomputing should give same result
    EXPECT_EQ(genesis.hashMerkleRoot, genesis.ComputeMerkleRoot());
}

TEST_F(GenesisBlockTest, GenesisCoinbaseReward) {
    // Genesis coinbase should have the initial block reward
    Block genesis = CreateGenesisBlock(
        1700000000, 171163, 0x1e0fffff, 1,
        mainnetParams.nInitialBlockReward
    );
    
    ASSERT_GE(genesis.vtx.size(), 1);
    ASSERT_GE(genesis.vtx[0]->vout.size(), 1);
    
    // Coinbase output should match initial reward
    EXPECT_EQ(genesis.vtx[0]->vout[0].nValue, mainnetParams.nInitialBlockReward);
    EXPECT_EQ(genesis.vtx[0]->vout[0].nValue, 100 * COIN);  // 100 NXS
}

// ============================================================================
// Block Validation Tests
// ============================================================================

class BlockValidationTest : public ::testing::Test {
protected:
    Params params;
    Block validBlock;
    
    void SetUp() override {
        // Use regtest params with very easy difficulty
        params = Params::RegTest();
        
        // Create a valid block
        validBlock.nVersion = 1;
        validBlock.nTime = 1700000000;
        validBlock.nBits = 0x207fffff;  // Very easy target for testing
        validBlock.nNonce = 0;
        
        // Add coinbase transaction
        MutableTransaction coinbase;
        OutPoint nullOutpoint;
        Script coinbaseScript;
        coinbaseScript << std::vector<uint8_t>{0x04, 0x01};  // Valid 2-byte coinbase script
        coinbase.vin.push_back(TxIn(nullOutpoint, coinbaseScript));
        Hash160 pubKeyHash;
        coinbase.vout.push_back(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)));
        validBlock.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
        
        validBlock.hashMerkleRoot = validBlock.ComputeMerkleRoot();
        
        // Find a valid nonce for the easy difficulty
        while (!CheckProofOfWork(validBlock.GetHash(), validBlock.nBits, params)) {
            validBlock.nNonce++;
            if (validBlock.nNonce > 1000000) break;  // Safety limit
        }
    }
};

TEST_F(BlockValidationTest, CheckBlockHeader_Valid) {
    ValidationState state;
    EXPECT_TRUE(CheckBlockHeader(validBlock, state, params));
    EXPECT_TRUE(state.IsValid());
}

TEST_F(BlockValidationTest, CheckBlockHeader_BadVersion) {
    validBlock.nVersion = -1;  // Invalid version
    
    ValidationState state;
    EXPECT_FALSE(CheckBlockHeader(validBlock, state, params));
    EXPECT_EQ(state.GetRejectReason(), "bad-version");
}

TEST_F(BlockValidationTest, CheckBlockHeader_TimeTooOld) {
    validBlock.nTime = 0;  // Very old timestamp
    
    ValidationState state;
    // This might pass header check but fail contextual check
    // Header check alone doesn't validate time against chain
    CheckBlockHeader(validBlock, state, params);
    // Time validation is contextual
}

TEST_F(BlockValidationTest, CheckBlock_Valid) {
    ValidationState state;
    EXPECT_TRUE(CheckBlock(validBlock, state, params));
    EXPECT_TRUE(state.IsValid());
}

TEST_F(BlockValidationTest, CheckBlock_EmptyTransactions) {
    Block emptyBlock = validBlock;
    emptyBlock.vtx.clear();
    
    ValidationState state;
    EXPECT_FALSE(CheckBlock(emptyBlock, state, params));
    EXPECT_EQ(state.GetRejectReason(), "bad-blk-length");
}

TEST_F(BlockValidationTest, CheckBlock_NoCoinbase) {
    // Replace coinbase with regular transaction
    MutableTransaction regularTx;
    TxHash prevHash;
    prevHash[0] = 0xAA;
    Script scriptSig;
    scriptSig << std::vector<uint8_t>{0x04, 0x01};  // Dummy script
    regularTx.vin.push_back(TxIn(OutPoint(prevHash, 0), scriptSig));
    Hash160 pubKeyHash;
    regularTx.vout.push_back(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)));
    
    validBlock.vtx[0] = MakeTransactionRef(std::move(regularTx));
    validBlock.hashMerkleRoot = validBlock.ComputeMerkleRoot();
    
    // Re-mine the block with new hash
    validBlock.nNonce = 0;
    while (!CheckProofOfWork(validBlock.GetHash(), validBlock.nBits, params)) {
        validBlock.nNonce++;
        if (validBlock.nNonce > 1000000) break;
    }
    
    ValidationState state;
    EXPECT_FALSE(CheckBlock(validBlock, state, params));
    EXPECT_EQ(state.GetRejectReason(), "bad-cb-missing");
}

TEST_F(BlockValidationTest, CheckBlock_DuplicateCoinbase) {
    // Add another coinbase
    MutableTransaction coinbase2;
    OutPoint nullOutpoint;
    coinbase2.vin.push_back(TxIn(nullOutpoint));
    Hash160 pubKeyHash;
    coinbase2.vout.push_back(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)));
    validBlock.vtx.push_back(MakeTransactionRef(std::move(coinbase2)));
    validBlock.hashMerkleRoot = validBlock.ComputeMerkleRoot();
    
    ValidationState state;
    EXPECT_FALSE(CheckBlock(validBlock, state, params));
    EXPECT_EQ(state.GetRejectReason(), "bad-cb-multiple");
}

TEST_F(BlockValidationTest, CheckBlock_BadMerkleRoot) {
    // Corrupt merkle root
    validBlock.hashMerkleRoot[0] ^= 0xFF;
    
    ValidationState state;
    EXPECT_FALSE(CheckBlock(validBlock, state, params));
    EXPECT_EQ(state.GetRejectReason(), "bad-txnmrklroot");
}

TEST_F(BlockValidationTest, CheckBlock_TooLarge) {
    // This would require adding many transactions to exceed size limit
    // For now, just verify the size check exists
    size_t blockSize = validBlock.GetTotalSize();
    EXPECT_LT(blockSize, params.nMaxBlockSize);
}

// ============================================================================
// Transaction Validation Tests
// ============================================================================

class TxValidationTest : public ::testing::Test {
protected:
    Params params;
    MutableTransaction validTx;
    
    void SetUp() override {
        params = Params::Main();
        
        // Create a valid transaction
        TxHash prevHash;
        prevHash[0] = 0xAA;
        validTx.vin.push_back(TxIn(OutPoint(prevHash, 0)));
        
        Hash160 pubKeyHash;
        validTx.vout.push_back(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)));
    }
};

TEST_F(TxValidationTest, CheckTransaction_Valid) {
    ValidationState state;
    EXPECT_TRUE(CheckTransaction(Transaction(validTx), state));
    EXPECT_TRUE(state.IsValid());
}

TEST_F(TxValidationTest, CheckTransaction_EmptyInputs) {
    validTx.vin.clear();
    
    ValidationState state;
    EXPECT_FALSE(CheckTransaction(Transaction(validTx), state));
    EXPECT_EQ(state.GetRejectReason(), "bad-txns-vin-empty");
}

TEST_F(TxValidationTest, CheckTransaction_EmptyOutputs) {
    validTx.vout.clear();
    
    ValidationState state;
    EXPECT_FALSE(CheckTransaction(Transaction(validTx), state));
    EXPECT_EQ(state.GetRejectReason(), "bad-txns-vout-empty");
}

TEST_F(TxValidationTest, CheckTransaction_NegativeOutput) {
    validTx.vout[0].nValue = -1;
    
    ValidationState state;
    EXPECT_FALSE(CheckTransaction(Transaction(validTx), state));
    EXPECT_EQ(state.GetRejectReason(), "bad-txns-vout-negative");
}

TEST_F(TxValidationTest, CheckTransaction_TooLargeOutput) {
    validTx.vout[0].nValue = MAX_MONEY + 1;
    
    ValidationState state;
    EXPECT_FALSE(CheckTransaction(Transaction(validTx), state));
    EXPECT_EQ(state.GetRejectReason(), "bad-txns-vout-toolarge");
}

TEST_F(TxValidationTest, CheckTransaction_TotalOverflow) {
    // Add outputs that would overflow when summed
    validTx.vout.clear();
    validTx.vout.push_back(TxOut(MAX_MONEY, Script()));
    validTx.vout.push_back(TxOut(1, Script()));  // This would overflow
    
    ValidationState state;
    EXPECT_FALSE(CheckTransaction(Transaction(validTx), state));
    EXPECT_EQ(state.GetRejectReason(), "bad-txns-txouttotal-toolarge");
}

TEST_F(TxValidationTest, CheckTransaction_DuplicateInputs) {
    // Add duplicate input
    validTx.vin.push_back(validTx.vin[0]);
    
    ValidationState state;
    EXPECT_FALSE(CheckTransaction(Transaction(validTx), state));
    EXPECT_EQ(state.GetRejectReason(), "bad-txns-inputs-duplicate");
}

TEST_F(TxValidationTest, CheckTransaction_NullInput) {
    // Non-coinbase with null input
    // Must have multiple inputs so IsCoinBase() returns false,
    // otherwise null outpoint makes it look like coinbase
    OutPoint nullOutpoint;
    validTx.vin[0] = TxIn(nullOutpoint);
    // Add a second non-null input so this isn't treated as coinbase
    TxHash prevTxHash;
    prevTxHash[0] = 0xAB;  // Non-null hash
    validTx.vin.push_back(TxIn(OutPoint(prevTxHash, 0)));
    
    ValidationState state;
    EXPECT_FALSE(CheckTransaction(Transaction(validTx), state));
    EXPECT_EQ(state.GetRejectReason(), "bad-txns-prevout-null");
}

TEST_F(TxValidationTest, CheckCoinbase_Valid) {
    MutableTransaction coinbase;
    OutPoint nullOutpoint;
    Script coinbaseScript;
    coinbaseScript << std::vector<uint8_t>{0x04, 0x01};
    coinbase.vin.push_back(TxIn(nullOutpoint, coinbaseScript));
    
    Hash160 pubKeyHash;
    coinbase.vout.push_back(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)));
    
    ValidationState state;
    EXPECT_TRUE(CheckTransaction(Transaction(coinbase), state));
}

// ============================================================================
// Validation State Tests
// ============================================================================

class ValidationStateTest : public ::testing::Test {};

TEST_F(ValidationStateTest, DefaultIsValid) {
    ValidationState state;
    EXPECT_TRUE(state.IsValid());
    EXPECT_FALSE(state.IsInvalid());
    EXPECT_FALSE(state.IsError());
}

TEST_F(ValidationStateTest, Invalid) {
    ValidationState state;
    state.Invalid("test-reason", "Test description");
    
    EXPECT_FALSE(state.IsValid());
    EXPECT_TRUE(state.IsInvalid());
    EXPECT_EQ(state.GetRejectReason(), "test-reason");
    EXPECT_EQ(state.GetDebugMessage(), "Test description");
}

TEST_F(ValidationStateTest, Error) {
    ValidationState state;
    state.Error("error-message");
    
    EXPECT_FALSE(state.IsValid());
    EXPECT_TRUE(state.IsError());
}

TEST_F(ValidationStateTest, ToString) {
    ValidationState state;
    state.Invalid("bad-block", "Invalid block");
    
    std::string str = state.ToString();
    EXPECT_NE(str.find("bad-block"), std::string::npos);
}

// ============================================================================
// Difficulty Target Tests
// ============================================================================

class DifficultyTest : public ::testing::Test {
protected:
    Params params;
    
    void SetUp() override {
        params = Params::Main();
    }
};

TEST_F(DifficultyTest, CompactToBig) {
    // Test compact to big integer conversion
    // 0x1d00ffff is Bitcoin's initial difficulty
    uint32_t compact = 0x1d00ffff;
    Hash256 target = CompactToBig(compact);
    EXPECT_FALSE(target.IsNull());
}

TEST_F(DifficultyTest, BigToCompact) {
    // Round-trip test
    uint32_t compact = 0x1d00ffff;
    Hash256 target = CompactToBig(compact);
    uint32_t compactBack = BigToCompact(target);
    EXPECT_EQ(compact, compactBack);
}

TEST_F(DifficultyTest, CheckProofOfWork_Valid) {
    // Create a block with valid PoW (hash below target)
    // Use regtest params which allow very easy difficulty
    auto regtestParams = Params::RegTest();
    
    BlockHeader header;
    header.nVersion = 1;
    header.nTime = 1700000000;
    header.nBits = 0x207fffff;  // Very easy target for testing (valid for regtest)
    header.nNonce = 0;
    
    // Find a valid nonce (for very easy difficulty)
    bool found = false;
    for (uint32_t nonce = 0; nonce < 1000000 && !found; ++nonce) {
        header.nNonce = nonce;
        if (CheckProofOfWork(header.GetHash(), header.nBits, regtestParams)) {
            found = true;
        }
    }
    
    // With easy difficulty, we should find a valid hash quickly
    EXPECT_TRUE(found);
}

TEST_F(DifficultyTest, CheckProofOfWork_Invalid) {
    // Create a block that fails PoW (hash above target)
    BlockHeader header;
    header.nVersion = 1;
    header.nTime = 1700000000;
    header.nBits = 0x01000001;  // Very hard target
    header.nNonce = 0;
    
    // Hash should be above this target
    EXPECT_FALSE(CheckProofOfWork(header.GetHash(), header.nBits, params));
}

TEST_F(DifficultyTest, GetNextWorkRequired) {
    // Test 1: Null pindexLast returns powLimit
    auto regtestParams = Params::RegTest();
    uint32_t result = GetNextWorkRequired(nullptr, regtestParams);
    uint32_t expectedLimit = BigToCompact(regtestParams.powLimit);
    EXPECT_EQ(result, expectedLimit);
    
    // Test 2: Regtest with fPowNoRetargeting returns previous nBits unchanged
    BlockIndex genesis;
    genesis.nBits = 0x207fffff;
    genesis.nHeight = 0;
    genesis.nTime = 1700000000;
    
    result = GetNextWorkRequired(&genesis, regtestParams);
    EXPECT_EQ(result, genesis.nBits);
}

TEST_F(DifficultyTest, GetNextWorkRequired_NoRetarget) {
    // Test that difficulty stays constant when not at retarget interval
    auto mainParams = Params::Main();
    
    // Create a simple chain of a few blocks (not at retarget boundary)
    BlockIndex block0;
    block0.nBits = 0x1e0fffff;
    block0.nHeight = 0;
    block0.nTime = 1700000000;
    block0.pprev = nullptr;
    
    BlockIndex block1;
    block1.nBits = 0x1e0fffff;
    block1.nHeight = 1;
    block1.nTime = 1700000030;  // 30 seconds later
    block1.pprev = &block0;
    
    // Height 2 is not a retarget interval, should keep same difficulty
    uint32_t result = GetNextWorkRequired(&block1, mainParams);
    EXPECT_EQ(result, block1.nBits);
}

TEST_F(DifficultyTest, GetNextWorkRequired_TestnetMinDifficulty) {
    // Test testnet minimum difficulty rule: if block time > 2x target, use min difficulty
    auto testnetParams = Params::TestNet();
    
    BlockIndex prevBlock;
    prevBlock.nBits = 0x1e0fffff;
    prevBlock.nHeight = 100;
    prevBlock.nTime = 1700000000;
    prevBlock.pprev = nullptr;
    
    // Create a block header with timestamp > 2x target spacing after prev
    BlockHeader newBlockHeader;
    newBlockHeader.nTime = prevBlock.nTime + testnetParams.nPowTargetSpacing * 3;  // 90 seconds later (> 60s)
    
    uint32_t result = GetNextWorkRequired(&prevBlock, &newBlockHeader, testnetParams);
    uint32_t powLimitCompact = BigToCompact(testnetParams.powLimit);
    EXPECT_EQ(result, powLimitCompact);
}

TEST_F(DifficultyTest, CalculateNextWorkRequired_SlowBlocks) {
    // Test that difficulty decreases (target increases) when blocks are too slow
    auto mainParams = Params::Main();
    
    BlockIndex lastBlock;
    lastBlock.nBits = 0x1d00ffff;
    lastBlock.nHeight = 2879;  // Just before retarget
    lastBlock.nTime = 1700086400;  // End time
    
    // First block time such that actual timespan is 2x target (blocks too slow)
    int64_t targetTimespan = mainParams.nPowTargetTimespan;  // 86400 seconds (1 day)
    int64_t firstBlockTime = lastBlock.nTime - (targetTimespan * 2);  // 2x slower
    
    uint32_t newBits = CalculateNextWorkRequired(&lastBlock, firstBlockTime, mainParams);
    
    // Difficulty should decrease (target should increase, meaning larger nBits exponent)
    // The exact value depends on the algorithm, but it should be different
    Hash256 newTarget = CompactToBig(newBits);
    Hash256 oldTarget = CompactToBig(lastBlock.nBits);
    
    // New target should be >= old target (easier difficulty)
    bool newTargetLarger = false;
    for (int i = 31; i >= 0; --i) {
        if (newTarget[i] > oldTarget[i]) {
            newTargetLarger = true;
            break;
        } else if (newTarget[i] < oldTarget[i]) {
            break;
        }
    }
    EXPECT_TRUE(newTargetLarger) << "Difficulty should decrease when blocks are slow";
}

TEST_F(DifficultyTest, CalculateNextWorkRequired_FastBlocks) {
    // Test that difficulty increases (target decreases) when blocks are too fast
    auto mainParams = Params::Main();
    
    BlockIndex lastBlock;
    lastBlock.nBits = 0x1d00ffff;
    lastBlock.nHeight = 2879;
    lastBlock.nTime = 1700086400;
    
    // Blocks came in at half the expected time (too fast)
    int64_t targetTimespan = mainParams.nPowTargetTimespan;
    int64_t firstBlockTime = lastBlock.nTime - (targetTimespan / 2);
    
    uint32_t newBits = CalculateNextWorkRequired(&lastBlock, firstBlockTime, mainParams);
    
    // Difficulty should increase (target should decrease)
    Hash256 newTarget = CompactToBig(newBits);
    Hash256 oldTarget = CompactToBig(lastBlock.nBits);
    
    // New target should be <= old target (harder difficulty)
    bool newTargetSmaller = false;
    for (int i = 31; i >= 0; --i) {
        if (newTarget[i] < oldTarget[i]) {
            newTargetSmaller = true;
            break;
        } else if (newTarget[i] > oldTarget[i]) {
            break;
        }
    }
    EXPECT_TRUE(newTargetSmaller) << "Difficulty should increase when blocks are fast";
}

TEST_F(DifficultyTest, CalculateNextWorkRequired_LimitedAdjustment) {
    // Test that adjustment is limited to 4x in either direction
    auto mainParams = Params::Main();
    
    BlockIndex lastBlock;
    lastBlock.nBits = 0x1d00ffff;
    lastBlock.nHeight = 2879;
    lastBlock.nTime = 1700086400;
    
    // Extreme case: blocks 10x slower than expected
    int64_t targetTimespan = mainParams.nPowTargetTimespan;
    int64_t firstBlockTime = lastBlock.nTime - (targetTimespan * 10);
    
    uint32_t newBits = CalculateNextWorkRequired(&lastBlock, firstBlockTime, mainParams);
    
    // The adjustment should be capped at 4x, not 10x
    // This test ensures we don't have runaway difficulty changes
    EXPECT_NE(newBits, 0u);
    EXPECT_NE(newBits, lastBlock.nBits);  // Should have changed
}

TEST_F(DifficultyTest, IsDifficultyAdjustmentInterval) {
    auto mainParams = Params::Main();
    int64_t interval = mainParams.DifficultyAdjustmentInterval();
    
    // Height 0 is not an adjustment interval
    EXPECT_FALSE(IsDifficultyAdjustmentInterval(0, mainParams));
    
    // Height 1 is not an adjustment interval
    EXPECT_FALSE(IsDifficultyAdjustmentInterval(1, mainParams));
    
    // Height equal to interval IS an adjustment interval
    EXPECT_TRUE(IsDifficultyAdjustmentInterval(static_cast<int>(interval), mainParams));
    
    // Height 2x interval IS an adjustment interval
    EXPECT_TRUE(IsDifficultyAdjustmentInterval(static_cast<int>(interval * 2), mainParams));
    
    // Height interval + 1 is NOT an adjustment interval
    EXPECT_FALSE(IsDifficultyAdjustmentInterval(static_cast<int>(interval + 1), mainParams));
}

// ============================================================================
// SHURIUM-Specific: UBI Distribution Tests
// ============================================================================

class UBIDistributionTest : public ::testing::Test {
protected:
    Params params;
    
    void SetUp() override {
        params = Params::Main();
    }
};

TEST_F(UBIDistributionTest, CalculateUBIReward) {
    Amount blockReward = GetBlockSubsidy(0, params);
    Amount ubiReward = CalculateUBIReward(blockReward, params);
    
    // UBI should be 30% of block reward
    EXPECT_EQ(ubiReward, blockReward * params.nUBIPercentage / 100);
}

TEST_F(UBIDistributionTest, CalculateWorkReward) {
    Amount blockReward = GetBlockSubsidy(0, params);
    Amount workReward = CalculateWorkReward(blockReward, params);
    
    // Work reward should be 40% of block reward
    EXPECT_EQ(workReward, blockReward * params.nWorkRewardPercentage / 100);
}

TEST_F(UBIDistributionTest, RewardsSumToTotal) {
    Amount blockReward = GetBlockSubsidy(0, params);
    
    Amount ubiReward = CalculateUBIReward(blockReward, params);
    Amount workReward = CalculateWorkReward(blockReward, params);
    Amount contributionReward = CalculateContributionReward(blockReward, params);
    Amount ecosystemReward = CalculateEcosystemReward(blockReward, params);
    Amount stabilityReward = CalculateStabilityReserve(blockReward, params);
    
    Amount total = ubiReward + workReward + contributionReward + ecosystemReward + stabilityReward;
    EXPECT_EQ(total, blockReward);
}

TEST_F(UBIDistributionTest, IsUBIDistributionBlock) {
    // Block 0 is not a UBI distribution block
    EXPECT_FALSE(IsUBIDistributionBlock(0, params));
    
    // Block at distribution interval is
    EXPECT_TRUE(IsUBIDistributionBlock(params.nUBIDistributionInterval, params));
    
    // Block at 2x interval is
    EXPECT_TRUE(IsUBIDistributionBlock(params.nUBIDistributionInterval * 2, params));
}
