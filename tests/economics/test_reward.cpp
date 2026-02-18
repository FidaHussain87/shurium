// SHURIUM - Reward System Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include <shurium/economics/reward.h>
#include <shurium/consensus/params.h>

namespace shurium {
namespace economics {
namespace {

// ============================================================================
// Test Fixtures
// ============================================================================

class RewardTest : public ::testing::Test {
protected:
    consensus::Params params_;
    std::unique_ptr<RewardCalculator> calculator_;
    
    void SetUp() override {
        params_ = consensus::Params::Main();
        calculator_ = std::make_unique<RewardCalculator>(params_);
    }
};

// ============================================================================
// RewardDistribution Tests
// ============================================================================

TEST_F(RewardTest, RewardDistributionIsValid) {
    RewardDistribution dist;
    dist.total = 500 * COIN;
    dist.workReward = 200 * COIN;      // 40%
    dist.ubiPool = 150 * COIN;          // 30%
    dist.contributions = 75 * COIN;     // 15%
    dist.ecosystem = 50 * COIN;         // 10%
    dist.stability = 25 * COIN;         // 5%
    
    EXPECT_TRUE(dist.IsValid());
}

TEST_F(RewardTest, RewardDistributionInvalidSum) {
    RewardDistribution dist;
    dist.total = 500 * COIN;
    dist.workReward = 200 * COIN;
    dist.ubiPool = 100 * COIN;  // Wrong - should be 150
    dist.contributions = 75 * COIN;
    dist.ecosystem = 50 * COIN;
    dist.stability = 25 * COIN;
    
    EXPECT_FALSE(dist.IsValid());
}

TEST_F(RewardTest, RewardDistributionToString) {
    RewardDistribution dist;
    dist.total = 500 * COIN;
    std::string str = dist.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("RewardDistribution"), std::string::npos);
}

// ============================================================================
// RewardCalculator Tests
// ============================================================================

TEST_F(RewardTest, InitialBlockSubsidy) {
    Amount subsidy = calculator_->GetBlockSubsidy(0);
    EXPECT_EQ(subsidy, INITIAL_BLOCK_REWARD);
}

TEST_F(RewardTest, BlockSubsidyNeverNegative) {
    for (int height = 0; height < 100000; height += 10000) {
        Amount subsidy = calculator_->GetBlockSubsidy(height);
        EXPECT_GT(subsidy, 0);
    }
}

TEST_F(RewardTest, BlockSubsidyNeverBelowMinimum) {
    // Test at very high heights where many halvings have occurred
    for (int height = 0; height < 100 * HALVING_INTERVAL; height += HALVING_INTERVAL) {
        Amount subsidy = calculator_->GetBlockSubsidy(height);
        EXPECT_GE(subsidy, MINIMUM_BLOCK_REWARD);
    }
}

TEST_F(RewardTest, FirstHalving) {
    Amount beforeHalving = calculator_->GetBlockSubsidy(HALVING_INTERVAL - 1);
    Amount atHalving = calculator_->GetBlockSubsidy(HALVING_INTERVAL);
    
    // Before halving should be initial reward
    EXPECT_EQ(beforeHalving, INITIAL_BLOCK_REWARD);
    
    // After halving should be half
    EXPECT_EQ(atHalving, INITIAL_BLOCK_REWARD / 2);
}

TEST_F(RewardTest, MultipleHalvings) {
    Amount expected = INITIAL_BLOCK_REWARD;
    
    for (int i = 0; i < 10; ++i) {
        int height = i * HALVING_INTERVAL;
        Amount subsidy = calculator_->GetBlockSubsidy(height);
        
        expected = std::max(expected, MINIMUM_BLOCK_REWARD);
        EXPECT_EQ(subsidy, expected);
        
        expected = expected / 2;
    }
}

TEST_F(RewardTest, GetRewardDistribution) {
    RewardDistribution dist = calculator_->GetRewardDistribution(0);
    
    EXPECT_EQ(dist.total, INITIAL_BLOCK_REWARD);
    EXPECT_TRUE(dist.IsValid());
    
    // Check approximate percentages
    EXPECT_GT(dist.workReward, 0);
    EXPECT_GT(dist.ubiPool, 0);
    EXPECT_GT(dist.contributions, 0);
    EXPECT_GT(dist.ecosystem, 0);
    EXPECT_GT(dist.stability, 0);
}

TEST_F(RewardTest, RewardPercentagesCorrect) {
    RewardDistribution dist = calculator_->GetRewardDistribution(0);
    
    // Work reward should be ~40%
    double workPercent = static_cast<double>(dist.workReward) / dist.total * 100;
    EXPECT_NEAR(workPercent, 40.0, 1.0);
    
    // UBI pool should be ~30%
    double ubiPercent = static_cast<double>(dist.ubiPool) / dist.total * 100;
    EXPECT_NEAR(ubiPercent, 30.0, 1.0);
    
    // Contributions should be ~15%
    double contribPercent = static_cast<double>(dist.contributions) / dist.total * 100;
    EXPECT_NEAR(contribPercent, 15.0, 1.0);
    
    // Ecosystem should be ~10%
    double ecoPercent = static_cast<double>(dist.ecosystem) / dist.total * 100;
    EXPECT_NEAR(ecoPercent, 10.0, 1.0);
    
    // Stability should be ~5%
    double stabPercent = static_cast<double>(dist.stability) / dist.total * 100;
    EXPECT_NEAR(stabPercent, 5.0, 1.0);
}

TEST_F(RewardTest, GetWorkReward) {
    Amount work = calculator_->GetWorkReward(0);
    EXPECT_EQ(work, CalculatePercentage(INITIAL_BLOCK_REWARD, RewardPercentage::WORK_REWARD));
}

TEST_F(RewardTest, GetUBIPoolAmount) {
    Amount ubi = calculator_->GetUBIPoolAmount(0);
    EXPECT_EQ(ubi, CalculatePercentage(INITIAL_BLOCK_REWARD, RewardPercentage::UBI_POOL));
}

TEST_F(RewardTest, GetCumulativeSupply) {
    Amount supply0 = calculator_->GetCumulativeSupply(0);
    EXPECT_EQ(supply0, INITIAL_BLOCK_REWARD);
    
    Amount supply100 = calculator_->GetCumulativeSupply(100);
    EXPECT_EQ(supply100, INITIAL_BLOCK_REWARD * 101); // Blocks 0-100
}

TEST_F(RewardTest, GetHalvingCount) {
    EXPECT_EQ(calculator_->GetHalvingCount(0), 0);
    EXPECT_EQ(calculator_->GetHalvingCount(HALVING_INTERVAL - 1), 0);
    EXPECT_EQ(calculator_->GetHalvingCount(HALVING_INTERVAL), 1);
    EXPECT_EQ(calculator_->GetHalvingCount(2 * HALVING_INTERVAL), 2);
}

TEST_F(RewardTest, GetNextHalvingHeight) {
    EXPECT_EQ(calculator_->GetNextHalvingHeight(0), HALVING_INTERVAL);
    EXPECT_EQ(calculator_->GetNextHalvingHeight(HALVING_INTERVAL - 1), HALVING_INTERVAL);
    EXPECT_EQ(calculator_->GetNextHalvingHeight(HALVING_INTERVAL), 2 * HALVING_INTERVAL);
}

TEST_F(RewardTest, GetBlocksUntilHalving) {
    EXPECT_EQ(calculator_->GetBlocksUntilHalving(0), HALVING_INTERVAL);
    EXPECT_EQ(calculator_->GetBlocksUntilHalving(100), HALVING_INTERVAL - 100);
    EXPECT_EQ(calculator_->GetBlocksUntilHalving(HALVING_INTERVAL), HALVING_INTERVAL);
}

// ============================================================================
// Epoch Tests
// ============================================================================

TEST_F(RewardTest, HeightToEpoch) {
    EXPECT_EQ(HeightToEpoch(0), 0);
    EXPECT_EQ(HeightToEpoch(EPOCH_BLOCKS - 1), 0);
    EXPECT_EQ(HeightToEpoch(EPOCH_BLOCKS), 1);
    EXPECT_EQ(HeightToEpoch(2 * EPOCH_BLOCKS), 2);
}

TEST_F(RewardTest, EpochToHeight) {
    EXPECT_EQ(EpochToHeight(0), 0);
    EXPECT_EQ(EpochToHeight(1), EPOCH_BLOCKS);
    EXPECT_EQ(EpochToHeight(10), 10 * EPOCH_BLOCKS);
}

TEST_F(RewardTest, EpochEndHeight) {
    EXPECT_EQ(EpochEndHeight(0), EPOCH_BLOCKS - 1);
    EXPECT_EQ(EpochEndHeight(1), 2 * EPOCH_BLOCKS - 1);
}

TEST_F(RewardTest, IsEpochEnd) {
    EXPECT_FALSE(IsEpochEnd(0));
    EXPECT_TRUE(IsEpochEnd(EPOCH_BLOCKS - 1));
    EXPECT_FALSE(IsEpochEnd(EPOCH_BLOCKS));
    EXPECT_TRUE(IsEpochEnd(2 * EPOCH_BLOCKS - 1));
}

// ============================================================================
// EpochRewardPool Tests
// ============================================================================

TEST_F(RewardTest, EpochRewardPoolAddBlockReward) {
    EpochRewardPool pool;
    pool.epoch = 0;
    
    RewardDistribution dist = calculator_->GetRewardDistribution(0);
    pool.AddBlockReward(dist);
    
    EXPECT_EQ(pool.blockCount, 1);
    EXPECT_EQ(pool.ubiPool, dist.ubiPool);
    EXPECT_EQ(pool.contributionPool, dist.contributions);
}

TEST_F(RewardTest, EpochRewardPoolAverageUBI) {
    EpochRewardPool pool;
    pool.epoch = 0;
    
    for (int i = 0; i < 10; ++i) {
        RewardDistribution dist = calculator_->GetRewardDistribution(i);
        pool.AddBlockReward(dist);
    }
    
    Amount avg = pool.AverageUBIPerBlock();
    EXPECT_GT(avg, 0);
    EXPECT_EQ(avg, pool.ubiPool / 10);
}

TEST_F(RewardTest, EpochRewardPoolComplete) {
    EpochRewardPool pool;
    EXPECT_FALSE(pool.isComplete);
    
    pool.Complete();
    EXPECT_TRUE(pool.isComplete);
}

// ============================================================================
// CoinbaseBuilder Tests
// ============================================================================

TEST_F(RewardTest, CoinbaseBuilderBuildCoinbase) {
    CoinbaseBuilder builder(*calculator_);
    
    // Create test addresses
    std::array<Byte, 20> minerData{}, ubiData{}, ecoData{}, stabData{};
    std::fill(minerData.begin(), minerData.end(), 0x01);
    std::fill(ubiData.begin(), ubiData.end(), 0x02);
    std::fill(ecoData.begin(), ecoData.end(), 0x03);
    std::fill(stabData.begin(), stabData.end(), 0x04);
    
    Hash160 miner(minerData);
    Hash160 ubi(ubiData);
    Hash160 eco(ecoData);
    Hash160 stab(stabData);
    
    auto outputs = builder.BuildCoinbase(0, miner, ubi, eco, stab);
    
    // Should have outputs for work, ubi, contributions, ecosystem, stability
    EXPECT_GE(outputs.size(), 4);
    
    // Total should equal block reward
    Amount total = 0;
    for (const auto& [script, amount] : outputs) {
        total += amount;
    }
    EXPECT_EQ(total, INITIAL_BLOCK_REWARD);
}

TEST_F(RewardTest, CoinbaseBuilderVerifyCoinbase) {
    CoinbaseBuilder builder(*calculator_);
    
    // Create test addresses using std::array
    std::array<Byte, 20> minerData{}, ubiData{}, ecoData{}, stabData{};
    std::fill(minerData.begin(), minerData.end(), 0x01);
    std::fill(ubiData.begin(), ubiData.end(), 0x02);
    std::fill(ecoData.begin(), ecoData.end(), 0x03);
    std::fill(stabData.begin(), stabData.end(), 0x04);
    
    Hash160 miner(minerData);
    Hash160 ubi(ubiData);
    Hash160 eco(ecoData);
    Hash160 stab(stabData);
    
    auto outputs = builder.BuildCoinbase(0, miner, ubi, eco, stab);
    
    EXPECT_TRUE(builder.VerifyCoinbase(0, outputs));
}

TEST_F(RewardTest, CoinbaseBuilderVerifyCoinbaseInvalid) {
    CoinbaseBuilder builder(*calculator_);
    
    // Create invalid outputs (wrong total)
    std::vector<std::pair<std::vector<Byte>, Amount>> outputs;
    outputs.emplace_back(std::vector<Byte>{0x76, 0xa9}, INITIAL_BLOCK_REWARD + 1);
    
    EXPECT_FALSE(builder.VerifyCoinbase(0, outputs));
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(RewardTest, FormatAmount) {
    EXPECT_EQ(FormatAmount(1 * COIN), "1.00000000 NXS");
    EXPECT_EQ(FormatAmount(0), "0.00000000 NXS");
    EXPECT_EQ(FormatAmount(INITIAL_BLOCK_REWARD), "500.00000000 NXS");
}

TEST_F(RewardTest, ParseAmount) {
    EXPECT_EQ(ParseAmount("1 NXS"), 1 * COIN);
    EXPECT_EQ(ParseAmount("500 NXS"), 500 * COIN);
    EXPECT_EQ(ParseAmount("1.5 NXS"), 150000000);
    EXPECT_EQ(ParseAmount("0.00000001"), 1);
}

TEST_F(RewardTest, CalculatePercentage) {
    EXPECT_EQ(CalculatePercentage(100, 50), 50);
    EXPECT_EQ(CalculatePercentage(1000, 10), 100);
    EXPECT_EQ(CalculatePercentage(INITIAL_BLOCK_REWARD, 40), INITIAL_BLOCK_REWARD * 40 / 100);
}

} // namespace
} // namespace economics
} // namespace shurium
