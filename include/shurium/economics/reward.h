// SHURIUM - Block Reward System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Defines the block reward calculation and distribution system.
// SHURIUM uses a unique reward distribution model:
// - 40% to miners (Proof of Useful Work)
// - 30% to UBI pool (distributed to verified humans)
// - 15% to human contributions
// - 10% to ecosystem development
// - 5% to stability reserve

#ifndef SHURIUM_ECONOMICS_REWARD_H
#define SHURIUM_ECONOMICS_REWARD_H

#include <shurium/core/types.h>
#include <shurium/consensus/params.h>

#include <cstdint>
#include <string>
#include <vector>

namespace shurium {
namespace economics {

// ============================================================================
// Reward Constants
// ============================================================================

/// Reward distribution percentages (must sum to 100)
namespace RewardPercentage {
    constexpr int WORK_REWARD = 40;        // Mining/useful work
    constexpr int UBI_POOL = 30;           // Universal Basic Income
    constexpr int CONTRIBUTIONS = 15;      // Human contributions
    constexpr int ECOSYSTEM = 10;          // Development fund
    constexpr int STABILITY = 5;           // Stability reserve
}

/// Initial block reward (500 NXS per block)
constexpr Amount INITIAL_BLOCK_REWARD = 500 * COIN;

/// Block reward halving interval (every ~4 years at 30s blocks)
constexpr int HALVING_INTERVAL = 4 * 365 * 24 * 120; // ~4,204,800 blocks

/// Minimum block reward (1 NXS - never goes to zero)
constexpr Amount MINIMUM_BLOCK_REWARD = 1 * COIN;

// ============================================================================
// Reward Distribution
// ============================================================================

/// Breakdown of a block reward into its component parts
struct RewardDistribution {
    /// Total block reward
    Amount total{0};
    
    /// Reward for the miner (useful work)
    Amount workReward{0};
    
    /// Amount added to UBI pool
    Amount ubiPool{0};
    
    /// Reward for human contributions
    Amount contributions{0};
    
    /// Amount for ecosystem development
    Amount ecosystem{0};
    
    /// Amount for stability reserve
    Amount stability{0};
    
    /// Check if distribution is valid (sums to total)
    bool IsValid() const {
        return workReward + ubiPool + contributions + ecosystem + stability == total;
    }
    
    /// Get string representation
    std::string ToString() const;
};

// ============================================================================
// Block Reward Calculator
// ============================================================================

/**
 * Calculates block rewards and their distribution.
 * 
 * The reward schedule follows a halving model similar to Bitcoin,
 * but with a minimum reward floor to ensure perpetual network security
 * and UBI distribution.
 */
class RewardCalculator {
public:
    /// Create calculator with consensus parameters
    explicit RewardCalculator(const consensus::Params& params);
    
    /// Calculate total block subsidy at a given height
    /// @param height Block height
    /// @return Total block reward
    Amount GetBlockSubsidy(int height) const;
    
    /// Calculate full reward distribution at a given height
    /// @param height Block height
    /// @return Complete reward breakdown
    RewardDistribution GetRewardDistribution(int height) const;
    
    /// Get work reward portion
    Amount GetWorkReward(int height) const;
    
    /// Get UBI pool portion
    Amount GetUBIPoolAmount(int height) const;
    
    /// Get contribution reward portion
    Amount GetContributionReward(int height) const;
    
    /// Get ecosystem development portion
    Amount GetEcosystemReward(int height) const;
    
    /// Get stability reserve portion
    Amount GetStabilityReward(int height) const;
    
    /// Calculate cumulative supply at a given height
    /// @param height Block height
    /// @return Total coins in circulation
    Amount GetCumulativeSupply(int height) const;
    
    /// Get height at which maximum supply is reached (approximately)
    int GetMaxSupplyHeight() const;
    
    /// Get halving count at a given height
    int GetHalvingCount(int height) const;
    
    /// Get next halving height from current height
    int GetNextHalvingHeight(int currentHeight) const;
    
    /// Get blocks until next halving
    int GetBlocksUntilHalving(int currentHeight) const;

private:
    const consensus::Params& params_;
};

// ============================================================================
// Epoch-Based Rewards
// ============================================================================

/// An epoch identifier (used for UBI distribution)
using EpochId = uint32_t;

/// Epoch duration in blocks (daily distribution)
constexpr int EPOCH_BLOCKS = 2880; // ~24 hours at 30s blocks

/// Calculate epoch from block height
inline EpochId HeightToEpoch(int height) {
    return static_cast<EpochId>(height / EPOCH_BLOCKS);
}

/// Calculate first block of an epoch
inline int EpochToHeight(EpochId epoch) {
    return static_cast<int>(epoch) * EPOCH_BLOCKS;
}

/// Calculate last block of an epoch
inline int EpochEndHeight(EpochId epoch) {
    return EpochToHeight(epoch + 1) - 1;
}

/// Check if height is last block of an epoch (distribution trigger)
inline bool IsEpochEnd(int height) {
    return (height + 1) % EPOCH_BLOCKS == 0;
}

// ============================================================================
// Epoch Reward Pool
// ============================================================================

/**
 * Tracks accumulated rewards for an epoch.
 * 
 * At the end of each epoch, the accumulated UBI pool is distributed
 * equally among all verified identities who claimed their share.
 */
struct EpochRewardPool {
    /// Epoch identifier
    EpochId epoch{0};
    
    /// First block height of this epoch
    int startHeight{0};
    
    /// Last block height of this epoch
    int endHeight{0};
    
    /// Total UBI pool accumulated in this epoch
    Amount ubiPool{0};
    
    /// Total contribution rewards accumulated
    Amount contributionPool{0};
    
    /// Number of blocks in this epoch so far
    int blockCount{0};
    
    /// Whether this epoch is complete
    bool isComplete{false};
    
    /// Add block rewards to the pool
    void AddBlockReward(const RewardDistribution& dist);
    
    /// Mark epoch as complete
    void Complete();
    
    /// Get average UBI pool per block
    Amount AverageUBIPerBlock() const;
    
    /// Get string representation
    std::string ToString() const;
};

// ============================================================================
// Coinbase Transaction Builder
// ============================================================================

/**
 * Builds coinbase transactions with proper reward distribution.
 */
class CoinbaseBuilder {
public:
    /// Create builder with calculator
    explicit CoinbaseBuilder(const RewardCalculator& calculator);
    
    /// Build coinbase outputs for a block
    /// @param height Block height
    /// @param minerAddress Address for work reward
    /// @param ubiPoolAddress Address for UBI pool
    /// @param ecosystemAddress Address for ecosystem fund
    /// @param stabilityAddress Address for stability reserve
    /// @return Vector of output scripts and amounts
    std::vector<std::pair<std::vector<Byte>, Amount>> BuildCoinbase(
        int height,
        const Hash160& minerAddress,
        const Hash160& ubiPoolAddress,
        const Hash160& ecosystemAddress,
        const Hash160& stabilityAddress
    ) const;
    
    /// Verify coinbase outputs match expected distribution
    bool VerifyCoinbase(
        int height,
        const std::vector<std::pair<std::vector<Byte>, Amount>>& outputs
    ) const;

private:
    const RewardCalculator& calculator_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Format an amount as a human-readable string (e.g., "500.00 NXS")
std::string FormatAmount(Amount amount, int decimals = 8);

/// Parse an amount from string
Amount ParseAmount(const std::string& str);

/// Calculate percentage of an amount
inline Amount CalculatePercentage(Amount total, int percentage) {
    return (total * percentage) / 100;
}

} // namespace economics
} // namespace shurium

#endif // SHURIUM_ECONOMICS_REWARD_H
