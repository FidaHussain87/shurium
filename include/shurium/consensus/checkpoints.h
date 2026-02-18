// SHURIUM - Block Checkpoints
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Provides block hash checkpoints for chain validation security.
// Checkpoints are known-good block hashes at specific heights that help:
// - Prevent deep reorganization attacks
// - Speed up initial block download
// - Validate chain integrity
//
// Checkpoints are hardcoded for mainnet/testnet and can be
// dynamically loaded for custom deployments.

#ifndef SHURIUM_CONSENSUS_CHECKPOINTS_H
#define SHURIUM_CONSENSUS_CHECKPOINTS_H

#include <shurium/core/types.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace shurium {

// Forward declarations
class BlockIndex;

namespace consensus {

// ============================================================================
// Checkpoint Data Structure
// ============================================================================

/**
 * A single checkpoint entry.
 */
struct Checkpoint {
    /// Block height
    int32_t height{0};
    
    /// Expected block hash at this height
    BlockHash hash;
    
    /// Timestamp of the checkpoint block (for initial download estimation)
    int64_t timestamp{0};
    
    /// Total transactions up to this block (for progress estimation)
    uint64_t totalTxs{0};
    
    /// Human-readable description (optional)
    std::string description;
    
    Checkpoint() = default;
    
    Checkpoint(int32_t h, const BlockHash& bh)
        : height(h), hash(bh) {}
    
    Checkpoint(int32_t h, const BlockHash& bh, int64_t ts, uint64_t txs = 0)
        : height(h), hash(bh), timestamp(ts), totalTxs(txs) {}
    
    Checkpoint(int32_t h, const std::string& hashHex);
    
    /// Check if this checkpoint matches a given block
    bool Matches(int32_t blockHeight, const BlockHash& blockHash) const {
        return height == blockHeight && hash == blockHash;
    }
    
    /// Check if a height matches this checkpoint height
    bool IsAtHeight(int32_t blockHeight) const {
        return height == blockHeight;
    }
};

// ============================================================================
// Checkpoint Validation Result
// ============================================================================

/**
 * Result of checkpoint validation.
 */
enum class CheckpointResult {
    /// Block matches checkpoint (or no checkpoint at this height)
    VALID,
    
    /// Block hash doesn't match checkpoint at this height
    HASH_MISMATCH,
    
    /// Block is on a fork that conflicts with checkpoint
    FORK_BEFORE_CHECKPOINT,
    
    /// Block height is invalid for checkpoint validation
    INVALID_HEIGHT,
};

/// Convert checkpoint result to string
const char* CheckpointResultToString(CheckpointResult result);

// ============================================================================
// Checkpoint Manager
// ============================================================================

/**
 * Manages block checkpoints for chain validation.
 * 
 * Checkpoints are verified block hashes at known heights that provide:
 * - Protection against deep reorganization attacks
 * - Faster initial sync (can skip script verification before last checkpoint)
 * - Chain integrity validation
 * 
 * Usage:
 *   CheckpointManager mgr;
 *   mgr.LoadMainnetCheckpoints();
 *   
 *   // During block validation:
 *   auto result = mgr.ValidateBlock(height, hash);
 *   if (result != CheckpointResult::VALID) {
 *       // Reject block
 *   }
 */
class CheckpointManager {
public:
    CheckpointManager() = default;
    
    // ========================================================================
    // Checkpoint Loading
    // ========================================================================
    
    /**
     * Load built-in mainnet checkpoints.
     */
    void LoadMainnetCheckpoints();
    
    /**
     * Load built-in testnet checkpoints.
     */
    void LoadTestnetCheckpoints();
    
    /**
     * Load checkpoints for a specific network.
     * @param networkId Network identifier ("main", "test", "regtest")
     */
    void LoadCheckpoints(const std::string& networkId);
    
    /**
     * Add a single checkpoint.
     * @param checkpoint The checkpoint to add
     */
    void AddCheckpoint(const Checkpoint& checkpoint);
    
    /**
     * Add a checkpoint by height and hash.
     * @param height Block height
     * @param hash Block hash
     */
    void AddCheckpoint(int32_t height, const BlockHash& hash);
    
    /**
     * Add a checkpoint by height and hex hash string.
     * @param height Block height
     * @param hashHex Block hash in hex format
     */
    void AddCheckpoint(int32_t height, const std::string& hashHex);
    
    /**
     * Add multiple checkpoints.
     * @param checkpoints Vector of checkpoints to add
     */
    void AddCheckpoints(const std::vector<Checkpoint>& checkpoints);
    
    /**
     * Remove a checkpoint at a specific height.
     * @param height Height to remove
     * @return True if checkpoint was removed
     */
    bool RemoveCheckpoint(int32_t height);
    
    /**
     * Clear all checkpoints.
     */
    void Clear();
    
    // ========================================================================
    // Checkpoint Queries
    // ========================================================================
    
    /**
     * Get checkpoint at a specific height.
     * @param height Height to query
     * @return Checkpoint if one exists at this height
     */
    std::optional<Checkpoint> GetCheckpoint(int32_t height) const;
    
    /**
     * Check if a checkpoint exists at a specific height.
     * @param height Height to check
     * @return True if checkpoint exists
     */
    bool HasCheckpoint(int32_t height) const;
    
    /**
     * Get the last (highest) checkpoint.
     * @return Last checkpoint, or nullopt if no checkpoints
     */
    std::optional<Checkpoint> GetLastCheckpoint() const;
    
    /**
     * Get the last checkpoint before or at a given height.
     * @param height Maximum height
     * @return Checkpoint if found
     */
    std::optional<Checkpoint> GetLastCheckpointBefore(int32_t height) const;
    
    /**
     * Get the first checkpoint after a given height.
     * @param height Minimum height (exclusive)
     * @return Checkpoint if found
     */
    std::optional<Checkpoint> GetFirstCheckpointAfter(int32_t height) const;
    
    /**
     * Get all checkpoints.
     * @return Map of height to checkpoint
     */
    const std::map<int32_t, Checkpoint>& GetCheckpoints() const { return checkpoints_; }
    
    /**
     * Get number of checkpoints.
     */
    size_t NumCheckpoints() const { return checkpoints_.size(); }
    
    /**
     * Check if there are any checkpoints.
     */
    bool HasCheckpoints() const { return !checkpoints_.empty(); }
    
    // ========================================================================
    // Block Validation
    // ========================================================================
    
    /**
     * Validate a block against checkpoints.
     * @param height Block height
     * @param hash Block hash
     * @return Validation result
     */
    CheckpointResult ValidateBlock(int32_t height, const BlockHash& hash) const;
    
    /**
     * Validate a block index against checkpoints.
     * @param pindex Block index to validate
     * @return Validation result
     */
    CheckpointResult ValidateBlock(const BlockIndex* pindex) const;
    
    /**
     * Check if a block at this height can be reorganized.
     * Blocks at or before the last checkpoint cannot be reorganized.
     * @param height Block height to check
     * @return True if reorganization is allowed at this height
     */
    bool CanReorgAtHeight(int32_t height) const;
    
    /**
     * Get the height below which reorgs are not allowed.
     * @return Last checkpoint height, or -1 if no checkpoints
     */
    int32_t GetReorgProtectionHeight() const;
    
    /**
     * Check if we are past all checkpoints.
     * @param currentHeight Current chain height
     * @return True if current height is past the last checkpoint
     */
    bool IsPastLastCheckpoint(int32_t currentHeight) const;
    
    // ========================================================================
    // Initial Sync Support
    // ========================================================================
    
    /**
     * Check if script verification can be skipped for a block.
     * Script verification can be skipped for blocks before the last checkpoint
     * during initial sync (assume-valid optimization).
     * @param height Block height
     * @return True if script verification can be skipped
     */
    bool CanSkipScriptVerification(int32_t height) const;
    
    /**
     * Estimate sync progress based on checkpoints.
     * @param currentHeight Current sync height
     * @return Progress percentage (0.0 to 100.0)
     */
    double EstimateSyncProgress(int32_t currentHeight) const;
    
    /**
     * Estimate remaining time for sync.
     * @param currentHeight Current sync height
     * @param blocksPerSecond Current sync speed
     * @return Estimated seconds remaining
     */
    int64_t EstimateTimeRemaining(int32_t currentHeight, 
                                   double blocksPerSecond) const;
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /**
     * Get total transactions up to the last checkpoint.
     * Useful for initial sync progress estimation.
     * @return Total transactions, or 0 if unknown
     */
    uint64_t GetTotalTxsAtLastCheckpoint() const;
    
    /**
     * Get timestamp of the last checkpoint.
     * @return Unix timestamp, or 0 if no checkpoints
     */
    int64_t GetLastCheckpointTime() const;

private:
    /// Checkpoints indexed by height
    std::map<int32_t, Checkpoint> checkpoints_;
};

// ============================================================================
// Predefined Checkpoints
// ============================================================================

namespace Checkpoints {

/**
 * Get mainnet checkpoints.
 */
std::vector<Checkpoint> GetMainnetCheckpoints();

/**
 * Get testnet checkpoints.
 */
std::vector<Checkpoint> GetTestnetCheckpoints();

/**
 * Get checkpoints for a network.
 * @param networkId Network identifier
 */
std::vector<Checkpoint> GetCheckpointsForNetwork(const std::string& networkId);

} // namespace Checkpoints

// ============================================================================
// Global Checkpoint Manager
// ============================================================================

/**
 * Get the global checkpoint manager instance.
 */
CheckpointManager& GetCheckpointManager();

/**
 * Initialize global checkpoints for a network.
 * @param networkId Network identifier ("main", "test", "regtest")
 */
void InitCheckpoints(const std::string& networkId);

} // namespace consensus
} // namespace shurium

#endif // SHURIUM_CONSENSUS_CHECKPOINTS_H
