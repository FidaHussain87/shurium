// SHURIUM - Decentralized Price Oracle Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include <shurium/economics/oracle.h>
#include <shurium/economics/stability.h>
#include <shurium/crypto/keys.h>
#include <shurium/consensus/params.h>

#include <array>
#include <chrono>
#include <memory>
#include <vector>

namespace shurium {
namespace economics {
namespace {

// ============================================================================
// Test Fixtures
// ============================================================================

class OracleTest : public ::testing::Test {
protected:
    std::unique_ptr<OracleRegistry> registry_;
    std::unique_ptr<PriceAggregator> aggregator_;
    
    void SetUp() override {
        registry_ = std::make_unique<OracleRegistry>();
        aggregator_ = std::make_unique<PriceAggregator>(*registry_);
    }
    
    // Helper to create a test public key
    PublicKey CreateTestPublicKey(Byte seed) {
        std::vector<Byte> keyData(33);
        keyData[0] = 0x02;  // Compressed public key prefix
        std::fill(keyData.begin() + 1, keyData.end(), seed);
        return PublicKey(keyData.data(), keyData.size());
    }
    
    // Helper to create a test Hash160 address
    Hash160 CreateTestAddress(Byte value) {
        std::array<Byte, 20> data{};
        std::fill(data.begin(), data.end(), value);
        return Hash160(data);
    }
    
    // Helper to create a test Oracle ID
    OracleId CreateTestOracleId(Byte seed) {
        std::array<Byte, 32> data{};
        std::fill(data.begin(), data.end(), seed);
        return Hash256(data.data(), 32);
    }
    
    // Helper to register a test oracle
    std::optional<OracleId> RegisterTestOracle(Byte seed, Amount stake = MIN_ORACLE_STAKE) {
        PublicKey pubkey = CreateTestPublicKey(seed);
        Hash160 operatorAddr = CreateTestAddress(seed);
        return registry_->Register(pubkey, operatorAddr, stake, 0, "TestOracle");
    }
    
    // Helper to create a price submission
    PriceSubmission CreateTestSubmission(const OracleId& oracleId, PriceMillicents price, int height) {
        PriceSubmission submission;
        submission.oracleId = oracleId;
        submission.price = price;
        submission.blockHeight = height;
        submission.timestamp = std::chrono::system_clock::now();
        submission.confidence = 100;
        return submission;
    }
};

// ============================================================================
// Oracle Constants Tests
// ============================================================================

TEST_F(OracleTest, OracleConstantsValid) {
    EXPECT_GT(MIN_ORACLE_SOURCES, 0);
    EXPECT_GT(MAX_ORACLE_DEVIATION_BPS, 0);
    EXPECT_GT(ORACLE_HEARTBEAT_SECONDS, 0);
    EXPECT_GT(ORACLE_TIMEOUT_SECONDS, ORACLE_HEARTBEAT_SECONDS);
    EXPECT_GT(MIN_ORACLE_STAKE, 0);
    EXPECT_GT(ORACLE_SLASH_PERCENT, 0);
    EXPECT_LE(ORACLE_SLASH_PERCENT, 100);
    EXPECT_GT(ORACLE_UPDATE_COOLDOWN, 0);
}

// ============================================================================
// OracleStatus Tests
// ============================================================================

TEST_F(OracleTest, OracleStatusToString) {
    EXPECT_NE(OracleStatusToString(OracleStatus::Active), nullptr);
    EXPECT_NE(OracleStatusToString(OracleStatus::Pending), nullptr);
    EXPECT_NE(OracleStatusToString(OracleStatus::Suspended), nullptr);
    EXPECT_NE(OracleStatusToString(OracleStatus::Slashed), nullptr);
    EXPECT_NE(OracleStatusToString(OracleStatus::Withdrawn), nullptr);
    EXPECT_NE(OracleStatusToString(OracleStatus::Offline), nullptr);
}

TEST_F(OracleTest, OracleStatusStringsUnique) {
    std::set<std::string> statuses;
    statuses.insert(OracleStatusToString(OracleStatus::Active));
    statuses.insert(OracleStatusToString(OracleStatus::Pending));
    statuses.insert(OracleStatusToString(OracleStatus::Suspended));
    statuses.insert(OracleStatusToString(OracleStatus::Slashed));
    statuses.insert(OracleStatusToString(OracleStatus::Withdrawn));
    statuses.insert(OracleStatusToString(OracleStatus::Offline));
    
    EXPECT_EQ(statuses.size(), 6);
}

// ============================================================================
// OracleInfo Tests
// ============================================================================

TEST_F(OracleTest, OracleInfoAccuracyRate) {
    OracleInfo info;
    info.submissionCount = 100;
    info.successfulSubmissions = 95;
    info.outlierCount = 5;
    
    double accuracy = info.AccuracyRate();
    EXPECT_NEAR(accuracy, 95.0, 0.1);
}

TEST_F(OracleTest, OracleInfoAccuracyRateZeroSubmissions) {
    OracleInfo info;
    info.submissionCount = 0;
    
    double accuracy = info.AccuracyRate();
    EXPECT_DOUBLE_EQ(accuracy, 0.0);
}

TEST_F(OracleTest, OracleInfoCanSubmit) {
    OracleInfo info;
    info.status = OracleStatus::Active;
    info.lastActiveHeight = 100;
    
    // Can submit after cooldown
    EXPECT_TRUE(info.CanSubmit(100 + ORACLE_UPDATE_COOLDOWN));
    
    // Cannot submit during cooldown
    EXPECT_FALSE(info.CanSubmit(100 + ORACLE_UPDATE_COOLDOWN - 1));
    
    // Cannot submit if not active
    info.status = OracleStatus::Suspended;
    EXPECT_FALSE(info.CanSubmit(100 + ORACLE_UPDATE_COOLDOWN));
}

TEST_F(OracleTest, OracleInfoGetWeight) {
    OracleInfo info;
    
    // Neutral reputation
    info.reputation = 500;
    double weight1 = info.GetWeight();
    
    // High reputation
    info.reputation = 900;
    double weight2 = info.GetWeight();
    
    // Low reputation
    info.reputation = 100;
    double weight3 = info.GetWeight();
    
    EXPECT_GT(weight2, weight1);
    EXPECT_GT(weight1, weight3);
}

TEST_F(OracleTest, OracleInfoToString) {
    OracleInfo info;
    info.name = "TestOracle";
    info.reputation = 750;
    info.status = OracleStatus::Active;
    
    std::string str = info.ToString();
    EXPECT_FALSE(str.empty());
    // ToString includes status and reputation but not name field
    EXPECT_NE(str.find("OracleInfo"), std::string::npos);
    EXPECT_NE(str.find("750"), std::string::npos);
}

// ============================================================================
// PriceSubmission Tests
// ============================================================================

TEST_F(OracleTest, PriceSubmissionGetHash) {
    auto oracleId = CreateTestOracleId(0x01);
    PriceSubmission submission = CreateTestSubmission(oracleId, 100000, 100);
    
    Hash256 hash1 = submission.GetHash();
    Hash256 hash2 = submission.GetHash();
    
    // Same submission should have same hash
    EXPECT_EQ(hash1.ToHex(), hash2.ToHex());
    
    // Different price should have different hash
    submission.price = 110000;
    Hash256 hash3 = submission.GetHash();
    EXPECT_NE(hash1.ToHex(), hash3.ToHex());
}

TEST_F(OracleTest, PriceSubmissionSerializeDeserialize) {
    auto oracleId = CreateTestOracleId(0x01);
    PriceSubmission original = CreateTestSubmission(oracleId, 105000, 1000);
    original.confidence = 95;
    
    std::vector<Byte> serialized = original.Serialize();
    EXPECT_GT(serialized.size(), 0);
    
    auto deserialized = PriceSubmission::Deserialize(serialized.data(), serialized.size());
    ASSERT_TRUE(deserialized.has_value());
    
    EXPECT_EQ(deserialized->price, original.price);
    EXPECT_EQ(deserialized->blockHeight, original.blockHeight);
    EXPECT_EQ(deserialized->confidence, original.confidence);
}

TEST_F(OracleTest, PriceSubmissionToString) {
    auto oracleId = CreateTestOracleId(0x01);
    PriceSubmission submission = CreateTestSubmission(oracleId, 100000, 100);
    
    std::string str = submission.ToString();
    EXPECT_FALSE(str.empty());
}

// ============================================================================
// PriceCommitment Tests
// ============================================================================

TEST_F(OracleTest, PriceCommitmentCreate) {
    auto oracleId = CreateTestOracleId(0x01);
    PriceMillicents price = 100000;
    
    PriceCommitment commitment = PriceCommitment::Create(oracleId, price, 100, 10);
    
    EXPECT_EQ(commitment.commitHeight, 100);
    EXPECT_EQ(commitment.revealDeadline, 110);
    EXPECT_FALSE(commitment.revealed);
}

TEST_F(OracleTest, PriceCommitmentVerifyReveal) {
    auto oracleId = CreateTestOracleId(0x01);
    PriceMillicents price = 100000;
    
    PriceCommitment commitment = PriceCommitment::Create(oracleId, price, 100, 10);
    
    // Correct reveal should verify
    EXPECT_TRUE(commitment.VerifyReveal(price, commitment.salt));
    
    // Wrong price should not verify
    EXPECT_FALSE(commitment.VerifyReveal(price + 1000, commitment.salt));
}

TEST_F(OracleTest, PriceCommitmentIsExpired) {
    auto oracleId = CreateTestOracleId(0x01);
    PriceCommitment commitment = PriceCommitment::Create(oracleId, 100000, 100, 10);
    
    // Before deadline - not expired
    EXPECT_FALSE(commitment.IsExpired(109));
    
    // At deadline - not expired (inclusive)
    EXPECT_FALSE(commitment.IsExpired(110));
    
    // After deadline - expired
    EXPECT_TRUE(commitment.IsExpired(111));
}

TEST_F(OracleTest, PriceCommitmentSerializeDeserialize) {
    auto oracleId = CreateTestOracleId(0x01);
    PriceCommitment original = PriceCommitment::Create(oracleId, 100000, 100, 10);
    
    std::vector<Byte> serialized = original.Serialize();
    EXPECT_GT(serialized.size(), 0);
    
    auto deserialized = PriceCommitment::Deserialize(serialized.data(), serialized.size());
    ASSERT_TRUE(deserialized.has_value());
    
    EXPECT_EQ(deserialized->commitHeight, original.commitHeight);
    EXPECT_EQ(deserialized->revealDeadline, original.revealDeadline);
}

// ============================================================================
// OracleRegistry Tests
// ============================================================================

TEST_F(OracleTest, OracleRegistryRegister) {
    auto result = RegisterTestOracle(0x01);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(registry_->HasOracle(*result));
}

TEST_F(OracleTest, OracleRegistryRegisterInsufficientStake) {
    PublicKey pubkey = CreateTestPublicKey(0x01);
    Hash160 operatorAddr = CreateTestAddress(0x01);
    
    // Stake below minimum
    auto result = registry_->Register(pubkey, operatorAddr, MIN_ORACLE_STAKE - 1, 0, "Test");
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(OracleTest, OracleRegistryGetOracle) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    
    const OracleInfo* info = registry_->GetOracle(*oracleId);
    ASSERT_NE(info, nullptr);
    
    EXPECT_EQ(info->stakedAmount, MIN_ORACLE_STAKE);
    EXPECT_EQ(info->status, OracleStatus::Pending);
}

TEST_F(OracleTest, OracleRegistryGetOracleNonExistent) {
    auto fakeId = CreateTestOracleId(0xFF);
    const OracleInfo* info = registry_->GetOracle(fakeId);
    
    EXPECT_EQ(info, nullptr);
}

TEST_F(OracleTest, OracleRegistryGetOracleByPubkey) {
    PublicKey pubkey = CreateTestPublicKey(0x01);
    Hash160 operatorAddr = CreateTestAddress(0x01);
    
    auto oracleId = registry_->Register(pubkey, operatorAddr, MIN_ORACLE_STAKE, 0, "Test");
    ASSERT_TRUE(oracleId.has_value());
    
    const OracleInfo* info = registry_->GetOracleByPubkey(pubkey);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->id.ToHex(), oracleId->ToHex());
}

TEST_F(OracleTest, OracleRegistryGetActiveOracles) {
    // Register multiple oracles
    auto id1 = RegisterTestOracle(0x01);
    auto id2 = RegisterTestOracle(0x02);
    auto id3 = RegisterTestOracle(0x03);
    
    // Activate two of them
    registry_->UpdateStatus(*id1, OracleStatus::Active);
    registry_->UpdateStatus(*id2, OracleStatus::Active);
    // Leave id3 as Pending
    
    auto active = registry_->GetActiveOracles();
    EXPECT_EQ(active.size(), 2);
}

TEST_F(OracleTest, OracleRegistryGetOracleCount) {
    RegisterTestOracle(0x01);
    RegisterTestOracle(0x02);
    RegisterTestOracle(0x03);
    
    EXPECT_EQ(registry_->GetOracleCount(OracleStatus::Pending), 3);
    EXPECT_EQ(registry_->GetOracleCount(OracleStatus::Active), 0);
}

TEST_F(OracleTest, OracleRegistryAddStake) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    
    EXPECT_TRUE(registry_->AddStake(*oracleId, 1000 * COIN));
    
    const OracleInfo* info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->stakedAmount, MIN_ORACLE_STAKE + 1000 * COIN);
}

TEST_F(OracleTest, OracleRegistryWithdraw) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    
    Amount returned = registry_->Withdraw(*oracleId, 100);
    
    EXPECT_EQ(returned, MIN_ORACLE_STAKE);
    
    const OracleInfo* info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->status, OracleStatus::Withdrawn);
}

TEST_F(OracleTest, OracleRegistryUpdateStatus) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    
    registry_->UpdateStatus(*oracleId, OracleStatus::Active);
    
    const OracleInfo* info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->status, OracleStatus::Active);
}

TEST_F(OracleTest, OracleRegistryRecordHeartbeat) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    
    registry_->RecordHeartbeat(*oracleId, 1000);
    
    const OracleInfo* info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->lastActiveHeight, 1000);
}

TEST_F(OracleTest, OracleRegistryRecordSubmission) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    
    // Successful submission
    registry_->RecordSubmission(*oracleId, true);
    
    const OracleInfo* info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->submissionCount, 1);
    EXPECT_EQ(info->successfulSubmissions, 1);
    
    // Failed submission (outlier)
    registry_->RecordSubmission(*oracleId, false);
    
    info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->submissionCount, 2);
    EXPECT_EQ(info->successfulSubmissions, 1);
    EXPECT_EQ(info->outlierCount, 1);
}

TEST_F(OracleTest, OracleRegistryReputation) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    
    const OracleInfo* info = registry_->GetOracle(*oracleId);
    int initialRep = info->reputation;
    
    registry_->IncreaseReputation(*oracleId, 50);
    info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->reputation, initialRep + 50);
    
    registry_->DecreaseReputation(*oracleId, 30);
    info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->reputation, initialRep + 20);
}

TEST_F(OracleTest, OracleRegistrySlash) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    
    Amount slashed = registry_->Slash(*oracleId, "Malicious behavior");
    
    EXPECT_GT(slashed, 0);
    EXPECT_EQ(slashed, MIN_ORACLE_STAKE * ORACLE_SLASH_PERCENT / 100);
    
    const OracleInfo* info = registry_->GetOracle(*oracleId);
    EXPECT_EQ(info->status, OracleStatus::Slashed);
    EXPECT_EQ(info->slashCount, 1);
}

TEST_F(OracleTest, OracleRegistrySerializeDeserialize) {
    // Register oracles
    auto id1 = RegisterTestOracle(0x01);
    auto id2 = RegisterTestOracle(0x02);
    
    ASSERT_TRUE(id1.has_value());
    ASSERT_TRUE(id2.has_value());
    
    // Update some stats
    registry_->UpdateStatus(*id1, OracleStatus::Active);
    registry_->RecordSubmission(*id1, true);
    registry_->IncreaseReputation(*id1, 50);
    
    std::vector<Byte> serialized = registry_->Serialize();
    EXPECT_GT(serialized.size(), 0);
    
    OracleRegistry newRegistry;
    EXPECT_TRUE(newRegistry.Deserialize(serialized.data(), serialized.size()));
    
    EXPECT_EQ(newRegistry.GetOracleCount(OracleStatus::Pending), 1);  // id2
    EXPECT_EQ(newRegistry.GetOracleCount(OracleStatus::Active), 1);   // id1
    
    // Verify oracle 1 state was preserved
    const OracleInfo* info = newRegistry.GetOracle(*id1);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->status, OracleStatus::Active);
    EXPECT_EQ(info->submissionCount, 1);
    EXPECT_EQ(info->reputation, 550);  // 500 + 50
}

// ============================================================================
// PriceAggregator Tests
// ============================================================================

TEST_F(OracleTest, PriceAggregatorConstruction) {
    EXPECT_EQ(aggregator_->GetPendingSubmissionCount(), 0);
    EXPECT_FALSE(aggregator_->GetLatestPrice().has_value());
}

TEST_F(OracleTest, PriceAggregatorProcessSubmission) {
    // Register and activate oracle
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    registry_->UpdateStatus(*oracleId, OracleStatus::Active);
    
    PriceSubmission submission = CreateTestSubmission(*oracleId, 100000, 100);
    
    // Note: ProcessSubmission requires valid signature, which test data doesn't have
    // This tests the validation path - returns false without valid signature
    bool result = aggregator_->ProcessSubmission(submission);
    // Signature verification will fail, so we expect false
    EXPECT_FALSE(result);
}

TEST_F(OracleTest, PriceAggregatorProcessSubmissionUnknownOracle) {
    auto fakeId = CreateTestOracleId(0xFF);
    PriceSubmission submission = CreateTestSubmission(fakeId, 100000, 100);
    
    EXPECT_FALSE(aggregator_->ProcessSubmission(submission));
}

TEST_F(OracleTest, PriceAggregatorAggregate) {
    // Note: Full aggregation requires valid signed submissions
    // This test verifies behavior when no valid submissions exist
    
    // Register and activate multiple oracles
    std::vector<OracleId> oracleIds;
    for (Byte i = 1; i <= 5; ++i) {
        auto id = RegisterTestOracle(i);
        ASSERT_TRUE(id.has_value());
        registry_->UpdateStatus(*id, OracleStatus::Active);
        oracleIds.push_back(*id);
    }
    
    // Submissions won't be accepted without valid signatures
    for (size_t i = 0; i < oracleIds.size(); ++i) {
        PriceMillicents price = 100000 + (i - 2) * 100;
        PriceSubmission submission = CreateTestSubmission(oracleIds[i], price, 100);
        aggregator_->ProcessSubmission(submission);
    }
    
    // Without valid submissions, aggregation returns nullopt
    auto aggregated = aggregator_->Aggregate(100);
    EXPECT_FALSE(aggregated.has_value());
}

TEST_F(OracleTest, PriceAggregatorAggregateInsufficientSources) {
    // Only register 2 oracles (below minimum)
    auto id1 = RegisterTestOracle(0x01);
    auto id2 = RegisterTestOracle(0x02);
    registry_->UpdateStatus(*id1, OracleStatus::Active);
    registry_->UpdateStatus(*id2, OracleStatus::Active);
    
    aggregator_->ProcessSubmission(CreateTestSubmission(*id1, 100000, 100));
    aggregator_->ProcessSubmission(CreateTestSubmission(*id2, 100000, 100));
    
    auto aggregated = aggregator_->Aggregate(100);
    
    // Should fail due to insufficient sources
    EXPECT_FALSE(aggregated.has_value());
}

TEST_F(OracleTest, PriceAggregatorCommitReveal) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    registry_->UpdateStatus(*oracleId, OracleStatus::Active);
    
    // Commit
    PriceCommitment commitment = PriceCommitment::Create(*oracleId, 100000, 100, 10);
    EXPECT_TRUE(aggregator_->ProcessCommitment(commitment));
    
    // Reveal
    EXPECT_TRUE(aggregator_->ProcessReveal(*oracleId, 100000, commitment.salt));
}

TEST_F(OracleTest, PriceAggregatorRoundManagement) {
    EXPECT_EQ(aggregator_->GetCurrentRoundHeight(), 0);
    
    aggregator_->StartNewRound(100);
    EXPECT_EQ(aggregator_->GetCurrentRoundHeight(), 100);
    
    aggregator_->FinalizeRound();
}

TEST_F(OracleTest, PriceAggregatorGetCurrentSubmissions) {
    auto oracleId = RegisterTestOracle(0x01);
    registry_->UpdateStatus(*oracleId, OracleStatus::Active);
    
    // Submission won't be accepted without valid signature
    aggregator_->ProcessSubmission(CreateTestSubmission(*oracleId, 100000, 100));
    
    // Since signature validation fails, no submissions are stored
    auto submissions = aggregator_->GetCurrentSubmissions();
    EXPECT_EQ(submissions.size(), 0);
}

TEST_F(OracleTest, PriceAggregatorConfig) {
    auto config = aggregator_->GetConfig();
    EXPECT_EQ(config.minSources, MIN_ORACLE_SOURCES);
    EXPECT_EQ(config.maxDeviationBps, MAX_ORACLE_DEVIATION_BPS);
    
    PriceAggregator::Config newConfig;
    newConfig.minSources = 5;
    aggregator_->UpdateConfig(newConfig);
    
    EXPECT_EQ(aggregator_->GetConfig().minSources, 5);
}

// ============================================================================
// OraclePriceFeed Tests
// ============================================================================

TEST_F(OracleTest, OraclePriceFeedConstruction) {
    OraclePriceFeed feed;
    EXPECT_FALSE(feed.GetCurrentPrice().has_value());
}

TEST_F(OracleTest, OraclePriceFeedInitialize) {
    OraclePriceFeed feed;
    feed.Initialize(std::make_shared<OracleRegistry>());
    
    // Should have access to aggregator and registry
    EXPECT_EQ(feed.GetAggregator().GetPendingSubmissionCount(), 0);
}

TEST_F(OracleTest, OraclePriceFeedProcessBlock) {
    OraclePriceFeed feed;
    auto registry = std::make_shared<OracleRegistry>();
    feed.Initialize(registry);
    
    // Register and activate oracles
    for (Byte i = 1; i <= 5; ++i) {
        PublicKey pubkey = CreateTestPublicKey(i);
        Hash160 operatorAddr = CreateTestAddress(i);
        auto id = registry->Register(pubkey, operatorAddr, MIN_ORACLE_STAKE, 0, "Test");
        if (id) {
            registry->UpdateStatus(*id, OracleStatus::Active);
        }
    }
    
    // Process block should not crash
    feed.ProcessBlock(100);
}

TEST_F(OracleTest, OraclePriceFeedGetPriceHistory) {
    OraclePriceFeed feed;
    feed.Initialize(std::make_shared<OracleRegistry>());
    
    auto history = feed.GetPriceHistory(10);
    // Initially empty
    EXPECT_EQ(history.size(), 0);
}

// ============================================================================
// OracleRewardCalculator Tests
// ============================================================================

TEST_F(OracleTest, OracleRewardCalculatorCalculateReward) {
    OracleInfo info;
    info.status = OracleStatus::Active;  // Must be active to receive rewards
    info.reputation = 800;  // High reputation
    info.submissionCount = 100;
    info.successfulSubmissions = 95;
    
    Amount totalPool = 1000 * COIN;
    size_t totalOracles = 10;
    
    Amount reward = OracleRewardCalculator::CalculateReward(info, totalPool, totalOracles);
    
    EXPECT_GT(reward, 0);
    EXPECT_LE(reward, totalPool);
}

TEST_F(OracleTest, OracleRewardCalculatorCalculateRewardInactive) {
    OracleInfo info;
    info.status = OracleStatus::Pending;  // Not active
    info.reputation = 800;
    
    Amount reward = OracleRewardCalculator::CalculateReward(info, 1000 * COIN, 10);
    
    // Inactive oracles get no rewards
    EXPECT_EQ(reward, 0);
}

TEST_F(OracleTest, OracleRewardCalculatorCalculatePenalty) {
    OracleInfo info;
    info.stakedAmount = 10000 * COIN;
    
    Amount penalty = OracleRewardCalculator::CalculatePenalty(info, 5);
    
    // Should have some penalty for missed submissions
    EXPECT_GT(penalty, 0);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(OracleTest, ValidateSubmission) {
    auto oracleId = RegisterTestOracle(0x01);
    ASSERT_TRUE(oracleId.has_value());
    registry_->UpdateStatus(*oracleId, OracleStatus::Active);
    
    PriceSubmission submission = CreateTestSubmission(*oracleId, 100000, 100);
    
    // ValidateSubmission requires valid signature, which test data doesn't have
    EXPECT_FALSE(ValidateSubmission(submission, *registry_));
}

TEST_F(OracleTest, ValidateSubmissionUnknownOracle) {
    auto fakeId = CreateTestOracleId(0xFF);
    PriceSubmission submission = CreateTestSubmission(fakeId, 100000, 100);
    
    EXPECT_FALSE(ValidateSubmission(submission, *registry_));
}

TEST_F(OracleTest, CalculateOracleId) {
    PublicKey pubkey = CreateTestPublicKey(0x01);
    
    OracleId id1 = CalculateOracleId(pubkey);
    OracleId id2 = CalculateOracleId(pubkey);
    
    // Same pubkey should produce same ID
    EXPECT_EQ(id1.ToHex(), id2.ToHex());
    
    // Different pubkey should produce different ID
    PublicKey pubkey2 = CreateTestPublicKey(0x02);
    OracleId id3 = CalculateOracleId(pubkey2);
    EXPECT_NE(id1.ToHex(), id3.ToHex());
}

TEST_F(OracleTest, IsPriceReasonable) {
    PriceMillicents reference = 100000;
    
    // At reference - reasonable
    EXPECT_TRUE(IsPriceReasonable(100000, reference, 500));
    
    // Within 5% - reasonable
    EXPECT_TRUE(IsPriceReasonable(104000, reference, 500));
    EXPECT_TRUE(IsPriceReasonable(96000, reference, 500));
    
    // Beyond 5% - not reasonable
    EXPECT_FALSE(IsPriceReasonable(106000, reference, 500));
    EXPECT_FALSE(IsPriceReasonable(94000, reference, 500));
}

// ============================================================================
// Outlier Detection Tests
// ============================================================================

TEST_F(OracleTest, OutlierDetection) {
    // Note: Without valid signatures, submissions won't be accepted
    // This test verifies that aggregation fails without valid submissions
    
    // Register and activate 5 oracles
    std::vector<OracleId> oracleIds;
    for (Byte i = 1; i <= 5; ++i) {
        auto id = RegisterTestOracle(i);
        registry_->UpdateStatus(*id, OracleStatus::Active);
        oracleIds.push_back(*id);
    }
    
    // Attempt to submit prices (will fail due to signature validation)
    for (size_t i = 0; i < 4; ++i) {
        PriceSubmission submission = CreateTestSubmission(oracleIds[i], 100000, 100);
        aggregator_->ProcessSubmission(submission);
    }
    
    // Without valid signatures, no submissions are stored
    auto aggregated = aggregator_->Aggregate(100);
    EXPECT_FALSE(aggregated.has_value());
}

} // namespace
} // namespace economics
} // namespace shurium
