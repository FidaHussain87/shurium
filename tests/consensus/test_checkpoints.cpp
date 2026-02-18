// SHURIUM - Checkpoints Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include <shurium/consensus/checkpoints.h>
#include <shurium/core/hex.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

namespace shurium {
namespace consensus {
namespace test {

// ============================================================================
// Test Fixtures
// ============================================================================

class CheckpointTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<CheckpointManager>();
    }
    
    void TearDown() override {
        manager_.reset();
    }
    
    // Helper to create a BlockHash from hex string
    static BlockHash HashFromHex(const std::string& hex) {
        auto bytes = HexToBytes(hex);
        BlockHash hash;
        if (bytes.size() == 32) {
            // Reverse for internal representation
            std::reverse(bytes.begin(), bytes.end());
            std::copy(bytes.begin(), bytes.end(), hash.data());
        }
        return hash;
    }
    
    // Helper to create a BlockHash with a specific pattern
    static BlockHash MakeHash(uint8_t fillByte) {
        BlockHash hash;
        std::fill(hash.begin(), hash.end(), fillByte);
        return hash;
    }
    
    // Helper to create a null/zero hash
    static BlockHash NullHash() {
        return MakeHash(0);
    }
    
    std::unique_ptr<CheckpointManager> manager_;
};

// ============================================================================
// Checkpoint Structure Tests
// ============================================================================

TEST_F(CheckpointTest, DefaultConstruction) {
    Checkpoint cp;
    EXPECT_EQ(cp.height, 0);
    EXPECT_EQ(cp.timestamp, 0);
    EXPECT_EQ(cp.totalTxs, 0);
    EXPECT_TRUE(cp.description.empty());
}

TEST_F(CheckpointTest, ConstructionWithHeightAndHash) {
    BlockHash hash = MakeHash(0xAB);
    Checkpoint cp(100, hash);
    
    EXPECT_EQ(cp.height, 100);
    EXPECT_EQ(cp.hash, hash);
}

TEST_F(CheckpointTest, ConstructionWithTimestampAndTxs) {
    BlockHash hash = MakeHash(0xCD);
    Checkpoint cp(1000, hash, 1700000000, 50000);
    
    EXPECT_EQ(cp.height, 1000);
    EXPECT_EQ(cp.hash, hash);
    EXPECT_EQ(cp.timestamp, 1700000000);
    EXPECT_EQ(cp.totalTxs, 50000u);
}

TEST_F(CheckpointTest, ConstructionFromHexString) {
    // Valid 64-character hex string (all zeros)
    std::string validHex = std::string(64, '0');
    Checkpoint cp(500, validHex);
    
    EXPECT_EQ(cp.height, 500);
}

TEST_F(CheckpointTest, ConstructionFromHexStringWithValidData) {
    // Create a hex string with known pattern
    std::string hex = "0000000000000000000000000000000000000000000000000000000000000001";
    Checkpoint cp(1, hex);
    
    EXPECT_EQ(cp.height, 1);
    // Hash is reversed internally, so the last byte (01) becomes the first byte
    EXPECT_EQ(cp.hash[0], 0x01);
    for (size_t i = 1; i < 32; ++i) {
        EXPECT_EQ(cp.hash[i], 0x00);
    }
}

TEST_F(CheckpointTest, ConstructionFromInvalidHexLength) {
    // Too short
    EXPECT_THROW(Checkpoint(1, "00"), std::invalid_argument);
    
    // Too long
    std::string tooLong(66, '0');
    EXPECT_THROW(Checkpoint(1, tooLong), std::invalid_argument);
}

TEST_F(CheckpointTest, MatchesMethod) {
    BlockHash hash = MakeHash(0x11);
    Checkpoint cp(100, hash);
    
    EXPECT_TRUE(cp.Matches(100, hash));
    EXPECT_FALSE(cp.Matches(101, hash));          // Wrong height
    EXPECT_FALSE(cp.Matches(100, MakeHash(0x22))); // Wrong hash
    EXPECT_FALSE(cp.Matches(99, MakeHash(0x22)));  // Both wrong
}

TEST_F(CheckpointTest, IsAtHeightMethod) {
    Checkpoint cp(100, MakeHash(0x00));
    
    EXPECT_TRUE(cp.IsAtHeight(100));
    EXPECT_FALSE(cp.IsAtHeight(99));
    EXPECT_FALSE(cp.IsAtHeight(101));
}

// ============================================================================
// CheckpointResult Tests
// ============================================================================

TEST_F(CheckpointTest, ResultToString) {
    EXPECT_STREQ(CheckpointResultToString(CheckpointResult::VALID), "VALID");
    EXPECT_STREQ(CheckpointResultToString(CheckpointResult::HASH_MISMATCH), "HASH_MISMATCH");
    EXPECT_STREQ(CheckpointResultToString(CheckpointResult::FORK_BEFORE_CHECKPOINT), "FORK_BEFORE_CHECKPOINT");
    EXPECT_STREQ(CheckpointResultToString(CheckpointResult::INVALID_HEIGHT), "INVALID_HEIGHT");
}

// ============================================================================
// CheckpointManager - Basic Operations
// ============================================================================

TEST_F(CheckpointTest, InitiallyEmpty) {
    EXPECT_EQ(manager_->NumCheckpoints(), 0u);
    EXPECT_FALSE(manager_->HasCheckpoints());
    EXPECT_TRUE(manager_->GetCheckpoints().empty());
}

TEST_F(CheckpointTest, AddSingleCheckpoint) {
    manager_->AddCheckpoint(Checkpoint(100, MakeHash(0x01)));
    
    EXPECT_EQ(manager_->NumCheckpoints(), 1u);
    EXPECT_TRUE(manager_->HasCheckpoints());
}

TEST_F(CheckpointTest, AddCheckpointByHeightAndHash) {
    BlockHash hash = MakeHash(0x02);
    manager_->AddCheckpoint(500, hash);
    
    EXPECT_EQ(manager_->NumCheckpoints(), 1u);
    auto cp = manager_->GetCheckpoint(500);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->hash, hash);
}

TEST_F(CheckpointTest, AddCheckpointByHeightAndHexString) {
    std::string hex(64, '0');
    manager_->AddCheckpoint(1000, hex);
    
    EXPECT_EQ(manager_->NumCheckpoints(), 1u);
    EXPECT_TRUE(manager_->HasCheckpoint(1000));
}

TEST_F(CheckpointTest, AddMultipleCheckpoints) {
    std::vector<Checkpoint> checkpoints;
    checkpoints.emplace_back(100, MakeHash(0x01));
    checkpoints.emplace_back(200, MakeHash(0x02));
    checkpoints.emplace_back(300, MakeHash(0x03));
    
    manager_->AddCheckpoints(checkpoints);
    
    EXPECT_EQ(manager_->NumCheckpoints(), 3u);
    EXPECT_TRUE(manager_->HasCheckpoint(100));
    EXPECT_TRUE(manager_->HasCheckpoint(200));
    EXPECT_TRUE(manager_->HasCheckpoint(300));
}

TEST_F(CheckpointTest, RemoveCheckpoint) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(200, MakeHash(0x02));
    
    EXPECT_TRUE(manager_->RemoveCheckpoint(100));
    EXPECT_EQ(manager_->NumCheckpoints(), 1u);
    EXPECT_FALSE(manager_->HasCheckpoint(100));
    EXPECT_TRUE(manager_->HasCheckpoint(200));
}

TEST_F(CheckpointTest, RemoveNonExistentCheckpoint) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    
    EXPECT_FALSE(manager_->RemoveCheckpoint(999));
    EXPECT_EQ(manager_->NumCheckpoints(), 1u);
}

TEST_F(CheckpointTest, ClearCheckpoints) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(200, MakeHash(0x02));
    
    manager_->Clear();
    
    EXPECT_EQ(manager_->NumCheckpoints(), 0u);
    EXPECT_FALSE(manager_->HasCheckpoints());
}

TEST_F(CheckpointTest, CheckpointOverwrite) {
    BlockHash hash1 = MakeHash(0x01);
    BlockHash hash2 = MakeHash(0x02);
    
    manager_->AddCheckpoint(100, hash1);
    manager_->AddCheckpoint(100, hash2);  // Same height, different hash
    
    // Should overwrite
    EXPECT_EQ(manager_->NumCheckpoints(), 1u);
    auto cp = manager_->GetCheckpoint(100);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->hash, hash2);
}

// ============================================================================
// CheckpointManager - Query Operations
// ============================================================================

TEST_F(CheckpointTest, GetCheckpointExists) {
    manager_->AddCheckpoint(500, MakeHash(0xAA));
    
    auto cp = manager_->GetCheckpoint(500);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->height, 500);
}

TEST_F(CheckpointTest, GetCheckpointNotExists) {
    manager_->AddCheckpoint(500, MakeHash(0xAA));
    
    auto cp = manager_->GetCheckpoint(600);
    EXPECT_FALSE(cp.has_value());
}

TEST_F(CheckpointTest, HasCheckpoint) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    
    EXPECT_TRUE(manager_->HasCheckpoint(100));
    EXPECT_FALSE(manager_->HasCheckpoint(200));
}

TEST_F(CheckpointTest, GetLastCheckpointEmpty) {
    auto last = manager_->GetLastCheckpoint();
    EXPECT_FALSE(last.has_value());
}

TEST_F(CheckpointTest, GetLastCheckpoint) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(500, MakeHash(0x02));
    manager_->AddCheckpoint(200, MakeHash(0x03));  // Add out of order
    
    auto last = manager_->GetLastCheckpoint();
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->height, 500);  // Should be highest
}

TEST_F(CheckpointTest, GetLastCheckpointBefore) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(200, MakeHash(0x02));
    manager_->AddCheckpoint(300, MakeHash(0x03));
    
    // Exactly at checkpoint
    auto cp = manager_->GetLastCheckpointBefore(200);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->height, 200);
    
    // Between checkpoints
    cp = manager_->GetLastCheckpointBefore(250);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->height, 200);
    
    // After all checkpoints
    cp = manager_->GetLastCheckpointBefore(1000);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->height, 300);
    
    // Before all checkpoints
    cp = manager_->GetLastCheckpointBefore(50);
    EXPECT_FALSE(cp.has_value());
}

TEST_F(CheckpointTest, GetFirstCheckpointAfter) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(200, MakeHash(0x02));
    manager_->AddCheckpoint(300, MakeHash(0x03));
    
    // Before first checkpoint
    auto cp = manager_->GetFirstCheckpointAfter(50);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->height, 100);
    
    // Exactly at checkpoint - should get next
    cp = manager_->GetFirstCheckpointAfter(100);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->height, 200);
    
    // Between checkpoints
    cp = manager_->GetFirstCheckpointAfter(150);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->height, 200);
    
    // After all checkpoints
    cp = manager_->GetFirstCheckpointAfter(500);
    EXPECT_FALSE(cp.has_value());
}

TEST_F(CheckpointTest, GetCheckpointsMap) {
    manager_->AddCheckpoint(300, MakeHash(0x03));
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(200, MakeHash(0x02));
    
    const auto& checkpoints = manager_->GetCheckpoints();
    EXPECT_EQ(checkpoints.size(), 3u);
    
    // Map should be ordered by height
    auto it = checkpoints.begin();
    EXPECT_EQ(it->first, 100);
    ++it;
    EXPECT_EQ(it->first, 200);
    ++it;
    EXPECT_EQ(it->first, 300);
}

// ============================================================================
// CheckpointManager - Block Validation
// ============================================================================

TEST_F(CheckpointTest, ValidateBlockNoCheckpoints) {
    // Without checkpoints, all blocks are valid
    auto result = manager_->ValidateBlock(100, MakeHash(0x01));
    EXPECT_EQ(result, CheckpointResult::VALID);
}

TEST_F(CheckpointTest, ValidateBlockMatchingCheckpoint) {
    BlockHash hash = MakeHash(0xAB);
    manager_->AddCheckpoint(500, hash);
    
    auto result = manager_->ValidateBlock(500, hash);
    EXPECT_EQ(result, CheckpointResult::VALID);
}

TEST_F(CheckpointTest, ValidateBlockMismatchedCheckpoint) {
    manager_->AddCheckpoint(500, MakeHash(0xAB));
    
    // Different hash at checkpoint height
    auto result = manager_->ValidateBlock(500, MakeHash(0xCD));
    EXPECT_EQ(result, CheckpointResult::HASH_MISMATCH);
}

TEST_F(CheckpointTest, ValidateBlockNonCheckpointHeight) {
    manager_->AddCheckpoint(500, MakeHash(0xAB));
    
    // Height without checkpoint - always valid
    auto result = manager_->ValidateBlock(100, MakeHash(0x01));
    EXPECT_EQ(result, CheckpointResult::VALID);
    
    result = manager_->ValidateBlock(600, MakeHash(0x02));
    EXPECT_EQ(result, CheckpointResult::VALID);
}

TEST_F(CheckpointTest, ValidateBlockInvalidHeight) {
    auto result = manager_->ValidateBlock(-1, MakeHash(0x01));
    EXPECT_EQ(result, CheckpointResult::INVALID_HEIGHT);
}

// ============================================================================
// CheckpointManager - Reorganization Protection
// ============================================================================

TEST_F(CheckpointTest, CanReorgNoCheckpoints) {
    // Without checkpoints, any reorg is allowed
    EXPECT_TRUE(manager_->CanReorgAtHeight(0));
    EXPECT_TRUE(manager_->CanReorgAtHeight(100));
    EXPECT_TRUE(manager_->CanReorgAtHeight(1000000));
}

TEST_F(CheckpointTest, CanReorgWithCheckpoints) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(200, MakeHash(0x02));
    
    // Cannot reorg at or before last checkpoint
    EXPECT_FALSE(manager_->CanReorgAtHeight(50));
    EXPECT_FALSE(manager_->CanReorgAtHeight(100));
    EXPECT_FALSE(manager_->CanReorgAtHeight(150));
    EXPECT_FALSE(manager_->CanReorgAtHeight(200));
    
    // Can reorg after last checkpoint
    EXPECT_TRUE(manager_->CanReorgAtHeight(201));
    EXPECT_TRUE(manager_->CanReorgAtHeight(1000));
}

TEST_F(CheckpointTest, GetReorgProtectionHeightNoCheckpoints) {
    EXPECT_EQ(manager_->GetReorgProtectionHeight(), -1);
}

TEST_F(CheckpointTest, GetReorgProtectionHeight) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(500, MakeHash(0x02));
    
    EXPECT_EQ(manager_->GetReorgProtectionHeight(), 500);
}

TEST_F(CheckpointTest, IsPastLastCheckpointNoCheckpoints) {
    // Without checkpoints, always "past"
    EXPECT_TRUE(manager_->IsPastLastCheckpoint(0));
    EXPECT_TRUE(manager_->IsPastLastCheckpoint(100));
}

TEST_F(CheckpointTest, IsPastLastCheckpoint) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(500, MakeHash(0x02));
    
    EXPECT_FALSE(manager_->IsPastLastCheckpoint(0));
    EXPECT_FALSE(manager_->IsPastLastCheckpoint(100));
    EXPECT_FALSE(manager_->IsPastLastCheckpoint(500));
    EXPECT_TRUE(manager_->IsPastLastCheckpoint(501));
    EXPECT_TRUE(manager_->IsPastLastCheckpoint(1000));
}

// ============================================================================
// CheckpointManager - Initial Sync Support
// ============================================================================

TEST_F(CheckpointTest, CanSkipScriptVerificationNoCheckpoints) {
    EXPECT_FALSE(manager_->CanSkipScriptVerification(0));
    EXPECT_FALSE(manager_->CanSkipScriptVerification(100));
}

TEST_F(CheckpointTest, CanSkipScriptVerification) {
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(500, MakeHash(0x02));
    
    // Can skip before last checkpoint
    EXPECT_TRUE(manager_->CanSkipScriptVerification(0));
    EXPECT_TRUE(manager_->CanSkipScriptVerification(100));
    EXPECT_TRUE(manager_->CanSkipScriptVerification(499));
    
    // Cannot skip at or after last checkpoint
    EXPECT_FALSE(manager_->CanSkipScriptVerification(500));
    EXPECT_FALSE(manager_->CanSkipScriptVerification(501));
}

TEST_F(CheckpointTest, EstimateSyncProgressNoCheckpoints) {
    EXPECT_DOUBLE_EQ(manager_->EstimateSyncProgress(0), 0.0);
    EXPECT_DOUBLE_EQ(manager_->EstimateSyncProgress(100), 0.0);
}

TEST_F(CheckpointTest, EstimateSyncProgress) {
    manager_->AddCheckpoint(1000, MakeHash(0x01));
    
    EXPECT_NEAR(manager_->EstimateSyncProgress(0), 0.0, 0.01);
    EXPECT_NEAR(manager_->EstimateSyncProgress(100), 10.0, 0.01);
    EXPECT_NEAR(manager_->EstimateSyncProgress(500), 50.0, 0.01);
    EXPECT_NEAR(manager_->EstimateSyncProgress(1000), 100.0, 0.01);
    EXPECT_NEAR(manager_->EstimateSyncProgress(2000), 100.0, 0.01);  // Past checkpoint
}

TEST_F(CheckpointTest, EstimateTimeRemainingNoCheckpoints) {
    EXPECT_EQ(manager_->EstimateTimeRemaining(0, 10.0), 0);
}

TEST_F(CheckpointTest, EstimateTimeRemaining) {
    manager_->AddCheckpoint(10000, MakeHash(0x01));
    
    // At height 0, 10000 blocks remaining at 10 blocks/sec = 1000 seconds
    EXPECT_EQ(manager_->EstimateTimeRemaining(0, 10.0), 1000);
    
    // At height 5000, 5000 blocks remaining at 10 blocks/sec = 500 seconds
    EXPECT_EQ(manager_->EstimateTimeRemaining(5000, 10.0), 500);
    
    // Past checkpoint
    EXPECT_EQ(manager_->EstimateTimeRemaining(15000, 10.0), 0);
    
    // Invalid rate
    EXPECT_EQ(manager_->EstimateTimeRemaining(0, 0.0), 0);
    EXPECT_EQ(manager_->EstimateTimeRemaining(0, -1.0), 0);
}

// ============================================================================
// CheckpointManager - Statistics
// ============================================================================

TEST_F(CheckpointTest, GetTotalTxsAtLastCheckpointEmpty) {
    EXPECT_EQ(manager_->GetTotalTxsAtLastCheckpoint(), 0u);
}

TEST_F(CheckpointTest, GetTotalTxsAtLastCheckpoint) {
    Checkpoint cp1(100, MakeHash(0x01), 1000, 10000);
    Checkpoint cp2(200, MakeHash(0x02), 2000, 50000);
    
    manager_->AddCheckpoint(cp1);
    manager_->AddCheckpoint(cp2);
    
    EXPECT_EQ(manager_->GetTotalTxsAtLastCheckpoint(), 50000u);
}

TEST_F(CheckpointTest, GetLastCheckpointTimeEmpty) {
    EXPECT_EQ(manager_->GetLastCheckpointTime(), 0);
}

TEST_F(CheckpointTest, GetLastCheckpointTime) {
    Checkpoint cp1(100, MakeHash(0x01), 1700000000, 0);
    Checkpoint cp2(200, MakeHash(0x02), 1700100000, 0);
    
    manager_->AddCheckpoint(cp1);
    manager_->AddCheckpoint(cp2);
    
    EXPECT_EQ(manager_->GetLastCheckpointTime(), 1700100000);
}

// ============================================================================
// Predefined Checkpoints Tests
// ============================================================================

TEST_F(CheckpointTest, MainnetCheckpointsNotEmpty) {
    auto checkpoints = Checkpoints::GetMainnetCheckpoints();
    // Should have at least the genesis checkpoint
    EXPECT_FALSE(checkpoints.empty());
    
    // First checkpoint should be genesis (height 0)
    EXPECT_EQ(checkpoints[0].height, 0);
}

TEST_F(CheckpointTest, TestnetCheckpointsNotEmpty) {
    auto checkpoints = Checkpoints::GetTestnetCheckpoints();
    // Should have at least the genesis checkpoint
    EXPECT_FALSE(checkpoints.empty());
    
    // First checkpoint should be genesis (height 0)
    EXPECT_EQ(checkpoints[0].height, 0);
}

TEST_F(CheckpointTest, GetCheckpointsForMainnet) {
    auto checkpoints = Checkpoints::GetCheckpointsForNetwork("main");
    EXPECT_FALSE(checkpoints.empty());
    
    auto checkpoints2 = Checkpoints::GetCheckpointsForNetwork("mainnet");
    EXPECT_EQ(checkpoints.size(), checkpoints2.size());
}

TEST_F(CheckpointTest, GetCheckpointsForTestnet) {
    auto checkpoints = Checkpoints::GetCheckpointsForNetwork("test");
    EXPECT_FALSE(checkpoints.empty());
    
    auto checkpoints2 = Checkpoints::GetCheckpointsForNetwork("testnet");
    EXPECT_EQ(checkpoints.size(), checkpoints2.size());
}

TEST_F(CheckpointTest, GetCheckpointsForRegtest) {
    // Regtest has no checkpoints
    auto checkpoints = Checkpoints::GetCheckpointsForNetwork("regtest");
    EXPECT_TRUE(checkpoints.empty());
}

TEST_F(CheckpointTest, GetCheckpointsForUnknownNetwork) {
    auto checkpoints = Checkpoints::GetCheckpointsForNetwork("unknown");
    EXPECT_TRUE(checkpoints.empty());
}

// ============================================================================
// CheckpointManager - Loading Predefined Checkpoints
// ============================================================================

TEST_F(CheckpointTest, LoadMainnetCheckpoints) {
    manager_->LoadMainnetCheckpoints();
    
    EXPECT_TRUE(manager_->HasCheckpoints());
    EXPECT_TRUE(manager_->HasCheckpoint(0));  // Genesis
}

TEST_F(CheckpointTest, LoadTestnetCheckpoints) {
    manager_->LoadTestnetCheckpoints();
    
    EXPECT_TRUE(manager_->HasCheckpoints());
    EXPECT_TRUE(manager_->HasCheckpoint(0));  // Genesis
}

TEST_F(CheckpointTest, LoadCheckpointsReplacesExisting) {
    // Add a custom checkpoint
    manager_->AddCheckpoint(99999, MakeHash(0xFF));
    EXPECT_TRUE(manager_->HasCheckpoint(99999));
    
    // Load predefined checkpoints - should clear existing
    manager_->LoadMainnetCheckpoints();
    
    EXPECT_FALSE(manager_->HasCheckpoint(99999));
    EXPECT_TRUE(manager_->HasCheckpoint(0));
}

TEST_F(CheckpointTest, LoadCheckpointsByNetworkId) {
    manager_->LoadCheckpoints("main");
    EXPECT_TRUE(manager_->HasCheckpoints());
    
    manager_->LoadCheckpoints("regtest");
    EXPECT_FALSE(manager_->HasCheckpoints());  // Regtest has none
}

// ============================================================================
// Global Checkpoint Manager Tests
// ============================================================================

TEST(GlobalCheckpointManagerTest, GetGlobalManager) {
    // Should return a valid reference
    CheckpointManager& mgr1 = GetCheckpointManager();
    CheckpointManager& mgr2 = GetCheckpointManager();
    
    // Should be the same instance
    EXPECT_EQ(&mgr1, &mgr2);
}

TEST(GlobalCheckpointManagerTest, InitCheckpoints) {
    // Initialize for mainnet
    InitCheckpoints("main");
    
    CheckpointManager& mgr = GetCheckpointManager();
    EXPECT_TRUE(mgr.HasCheckpoints());
    EXPECT_TRUE(mgr.HasCheckpoint(0));
}

// ============================================================================
// Edge Cases and Stress Tests
// ============================================================================

TEST_F(CheckpointTest, ManyCheckpoints) {
    // Add many checkpoints
    for (int i = 0; i < 1000; ++i) {
        BlockHash hash;
        std::fill(hash.begin(), hash.end(), static_cast<uint8_t>(i & 0xFF));
        manager_->AddCheckpoint(i * 100, hash);
    }
    
    EXPECT_EQ(manager_->NumCheckpoints(), 1000u);
    
    // Verify lookups still work
    EXPECT_TRUE(manager_->HasCheckpoint(50000));  // Height 500 * 100
    EXPECT_FALSE(manager_->HasCheckpoint(50001)); // Not a checkpoint
    
    auto last = manager_->GetLastCheckpoint();
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->height, 99900);  // 999 * 100
}

TEST_F(CheckpointTest, HeightZeroCheckpoint) {
    manager_->AddCheckpoint(0, MakeHash(0x00));
    
    EXPECT_TRUE(manager_->HasCheckpoint(0));
    auto cp = manager_->GetCheckpoint(0);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(cp->height, 0);
}

TEST_F(CheckpointTest, NegativeHeight) {
    // Validation with negative height should fail
    auto result = manager_->ValidateBlock(-1, MakeHash(0x01));
    EXPECT_EQ(result, CheckpointResult::INVALID_HEIGHT);
    
    result = manager_->ValidateBlock(-1000, MakeHash(0x01));
    EXPECT_EQ(result, CheckpointResult::INVALID_HEIGHT);
}

TEST_F(CheckpointTest, MaxHeightCheckpoint) {
    // Test with maximum valid height
    int32_t maxHeight = std::numeric_limits<int32_t>::max();
    manager_->AddCheckpoint(maxHeight, MakeHash(0xFF));
    
    EXPECT_TRUE(manager_->HasCheckpoint(maxHeight));
    auto last = manager_->GetLastCheckpoint();
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->height, maxHeight);
}

TEST_F(CheckpointTest, DescriptionPreserved) {
    Checkpoint cp;
    cp.height = 100;
    cp.hash = MakeHash(0x01);
    cp.description = "Test checkpoint description";
    
    manager_->AddCheckpoint(cp);
    
    auto retrieved = manager_->GetCheckpoint(100);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->description, "Test checkpoint description");
}

TEST_F(CheckpointTest, CheckpointOrdering) {
    // Add checkpoints in random order
    manager_->AddCheckpoint(500, MakeHash(0x05));
    manager_->AddCheckpoint(100, MakeHash(0x01));
    manager_->AddCheckpoint(300, MakeHash(0x03));
    manager_->AddCheckpoint(200, MakeHash(0x02));
    manager_->AddCheckpoint(400, MakeHash(0x04));
    
    // Verify they're stored in order
    const auto& checkpoints = manager_->GetCheckpoints();
    int32_t prevHeight = -1;
    for (const auto& [height, cp] : checkpoints) {
        EXPECT_GT(height, prevHeight);
        EXPECT_EQ(height, cp.height);
        prevHeight = height;
    }
}

// ============================================================================
// Thread Safety Tests (Basic)
// ============================================================================

TEST_F(CheckpointTest, ConcurrentReads) {
    // Add some checkpoints
    for (int i = 0; i < 100; ++i) {
        manager_->AddCheckpoint(i * 10, MakeHash(static_cast<uint8_t>(i)));
    }
    
    // Multiple threads reading should be safe
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([this, &successCount]() {
            for (int i = 0; i < 100; ++i) {
                auto cp = manager_->GetCheckpoint(i * 10);
                if (cp.has_value()) {
                    ++successCount;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All reads should succeed (10 threads * 100 reads)
    EXPECT_EQ(successCount.load(), 1000);
}

} // namespace test
} // namespace consensus
} // namespace shurium
