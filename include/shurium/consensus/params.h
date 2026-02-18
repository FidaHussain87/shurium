// SHURIUM - Consensus Parameters Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the consensus parameters for SHURIUM networks.
// These parameters control block timing, rewards, difficulty, and
// SHURIUM-specific features like UBI distribution.

#ifndef SHURIUM_CONSENSUS_PARAMS_H
#define SHURIUM_CONSENSUS_PARAMS_H

#include "shurium/core/types.h"
#include <cstdint>
#include <string>

namespace shurium {
namespace consensus {

// ============================================================================
// Consensus Parameters
// ============================================================================

/// Parameters that influence chain consensus
struct Params {
    // ========================================================================
    // Network Identification
    // ========================================================================
    
    /// Network name (mainnet, testnet, regtest)
    std::string strNetworkID;
    
    /// Genesis block hash
    BlockHash hashGenesisBlock;
    
    // ========================================================================
    // Block Parameters
    // ========================================================================
    
    /// Target time between blocks in seconds (SHURIUM: 30 seconds)
    int64_t nPowTargetSpacing;
    
    /// Time period for difficulty adjustment in seconds
    int64_t nPowTargetTimespan;
    
    /// Maximum block size in bytes (SHURIUM: 10 MB)
    uint32_t nMaxBlockSize;
    
    /// Maximum block weight (for future SegWit-like features)
    uint32_t nMaxBlockWeight;
    
    /// Maximum number of signature operations per block
    uint32_t nMaxBlockSigOps;
    
    // ========================================================================
    // Proof of Work Parameters
    // ========================================================================
    
    /// Proof-of-work difficulty limit (minimum difficulty)
    Hash256 powLimit;
    
    /// Allow minimum difficulty blocks (for testnet)
    bool fAllowMinDifficultyBlocks;
    
    /// Disable difficulty retargeting (for regtest)
    bool fPowNoRetargeting;
    
    // ========================================================================
    // Reward Distribution (SHURIUM-Specific)
    // ========================================================================
    
    /// Block subsidy halving interval (in blocks)
    int nSubsidyHalvingInterval;
    
    /// Initial block reward in base units
    Amount nInitialBlockReward;
    
    /// Percentage of block reward for useful work (40%)
    int nWorkRewardPercentage;
    
    /// Percentage of block reward for UBI distribution (30%)
    int nUBIPercentage;
    
    /// Percentage of block reward for human contributions (15%)
    int nContributionRewardPercentage;
    
    /// Percentage of block reward for ecosystem development (10%)
    int nEcosystemPercentage;
    
    /// Percentage of block reward for stability reserve (5%)
    int nStabilityReservePercentage;
    
    // ========================================================================
    // Fund Addresses (SHURIUM-Specific)
    // ========================================================================
    
    /// Address for UBI pool collection
    Hash160 ubiPoolAddress;
    
    /// Address for ecosystem development fund
    Hash160 ecosystemAddress;
    
    /// Address for stability reserve fund
    Hash160 stabilityAddress;
    
    /// Address for human contribution rewards
    Hash160 contributionAddress;
    
    // ========================================================================
    // UBI Parameters (SHURIUM-Specific)
    // ========================================================================
    
    /// UBI distribution interval in blocks (daily = 2880 blocks at 30s)
    int nUBIDistributionInterval;
    
    /// Minimum verified identities for UBI distribution
    int nMinIdentitiesForUBI;
    
    // ========================================================================
    // Identity Parameters (SHURIUM-Specific)
    // ========================================================================
    
    /// Identity verification refresh interval in blocks (30 days)
    int nIdentityRefreshInterval;
    
    /// Maximum age for identity proof in blocks
    int nMaxIdentityAge;
    
    // ========================================================================
    // Helper Methods
    // ========================================================================
    
    /// Calculate the difficulty adjustment interval in blocks
    int64_t DifficultyAdjustmentInterval() const {
        return nPowTargetTimespan / nPowTargetSpacing;
    }
    
    // ========================================================================
    // Network Configurations
    // ========================================================================
    
    /// Get mainnet parameters
    static Params Main();
    
    /// Get testnet parameters
    static Params TestNet();
    
    /// Get regtest parameters
    static Params RegTest();
};

// ============================================================================
// Block Subsidy Calculation
// ============================================================================

/// Calculate the block subsidy at a given height
/// @param nHeight Block height
/// @param params Consensus parameters
/// @return Block subsidy in base units
Amount GetBlockSubsidy(int nHeight, const Params& params);

// ============================================================================
// Reward Distribution Functions (SHURIUM-Specific)
// ============================================================================

/// Calculate the UBI portion of a block reward
/// @param blockReward Total block reward
/// @param params Consensus parameters
/// @return Amount allocated for UBI
Amount CalculateUBIReward(Amount blockReward, const Params& params);

/// Calculate the useful work portion of a block reward
/// @param blockReward Total block reward
/// @param params Consensus parameters
/// @return Amount allocated for work reward
Amount CalculateWorkReward(Amount blockReward, const Params& params);

/// Calculate the human contribution portion of a block reward
/// @param blockReward Total block reward
/// @param params Consensus parameters
/// @return Amount allocated for contributions
Amount CalculateContributionReward(Amount blockReward, const Params& params);

/// Calculate the ecosystem development portion of a block reward
/// @param blockReward Total block reward
/// @param params Consensus parameters
/// @return Amount allocated for ecosystem
Amount CalculateEcosystemReward(Amount blockReward, const Params& params);

/// Calculate the stability reserve portion of a block reward
/// @param blockReward Total block reward
/// @param params Consensus parameters
/// @return Amount allocated for stability reserve
Amount CalculateStabilityReserve(Amount blockReward, const Params& params);

/// Check if a given height is a UBI distribution block
/// @param nHeight Block height
/// @param params Consensus parameters
/// @return True if this height triggers UBI distribution
bool IsUBIDistributionBlock(int nHeight, const Params& params);

// ============================================================================
// Difficulty Functions
// ============================================================================

/// Convert compact (nBits) format to 256-bit target
/// @param nCompact Compact difficulty representation
/// @return 256-bit target value
Hash256 CompactToBig(uint32_t nCompact);

/// Convert 256-bit target to compact (nBits) format
/// @param target 256-bit target value
/// @return Compact difficulty representation
uint32_t BigToCompact(const Hash256& target);

/// Check if hash meets proof-of-work target
/// @param hash Block hash to check
/// @param nBits Difficulty target in compact format
/// @param params Consensus parameters
/// @return True if hash is below target (valid PoW)
bool CheckProofOfWork(const BlockHash& hash, uint32_t nBits, const Params& params);

} // namespace consensus

// Forward declarations for difficulty adjustment (these are in nexus namespace)
class BlockIndex;
class BlockHeader;

namespace consensus {

/// Calculate the next work required (difficulty adjustment)
/// 
/// This implements SHURIUM's difficulty adjustment algorithm:
/// - Retarget every nPowTargetTimespan / nPowTargetSpacing blocks (~2880 blocks = 1 day)
/// - Adjust difficulty to maintain 30-second block time
/// - On testnet, allow minimum difficulty if block time exceeds 2x target
/// - On regtest, no retargeting (constant difficulty)
/// 
/// @param pindexLast The block index of the last block in the chain
/// @param params Consensus parameters
/// @return The nBits value for the next block
uint32_t GetNextWorkRequired(const BlockIndex* pindexLast, const Params& params);

/// Calculate the next work required with optional header (for special rules)
/// 
/// @param pindexLast The block index of the last block in the chain
/// @param pblock Optional block header (used for testnet min difficulty rules)
/// @param params Consensus parameters
/// @return The nBits value for the next block
uint32_t GetNextWorkRequired(const BlockIndex* pindexLast, const BlockHeader* pblock,
                             const Params& params);

/// Calculate the actual work required at a retarget boundary
/// 
/// @param pindexLast The block index of the last block before retarget
/// @param nFirstBlockTime Timestamp of the first block in the retarget period
/// @param params Consensus parameters
/// @return The nBits value after adjustment
uint32_t CalculateNextWorkRequired(const BlockIndex* pindexLast, int64_t nFirstBlockTime,
                                   const Params& params);

/// Check if we are at a difficulty adjustment boundary
/// @param nHeight Block height to check
/// @param params Consensus parameters
/// @return True if this height is a retarget point
inline bool IsDifficultyAdjustmentInterval(int nHeight, const Params& params) {
    return nHeight > 0 && nHeight % params.DifficultyAdjustmentInterval() == 0;
}

} // namespace consensus
} // namespace shurium

#endif // SHURIUM_CONSENSUS_PARAMS_H
