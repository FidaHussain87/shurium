// SHURIUM - CPU Miner
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the Miner class for CPU mining operations.
// The miner creates block templates, searches for valid nonces,
// and submits valid blocks to the chain.

#ifndef SHURIUM_MINER_MINER_H
#define SHURIUM_MINER_MINER_H

#include "shurium/miner/blockassembler.h"
#include "shurium/chain/chainstate.h"
#include "shurium/mempool/mempool.h"
#include "shurium/consensus/params.h"
#include "shurium/core/types.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace shurium {

// Forward declarations
class ChainStateManager;
class MessageProcessor;

namespace miner {

// ============================================================================
// Mining Statistics
// ============================================================================

/**
 * Statistics about mining operations.
 */
struct MiningStats {
    /// Total hashes computed
    std::atomic<uint64_t> hashesComputed{0};
    
    /// Blocks found
    std::atomic<uint32_t> blocksFound{0};
    
    /// Blocks accepted by chain
    std::atomic<uint32_t> blocksAccepted{0};
    
    /// Start time of mining
    int64_t startTime{0};
    
    /// Get hash rate (hashes per second)
    double GetHashRate() const;
    
    /// Reset statistics
    void Reset();
};

// ============================================================================
// Miner Options
// ============================================================================

/**
 * Configuration options for the miner.
 */
struct MinerOptions {
    /// Number of mining threads (0 = use hardware concurrency)
    int numThreads{1};
    
    /// Coinbase destination address
    Hash160 coinbaseAddress;
    
    /// Maximum nonces to try before getting new template
    uint32_t maxNoncesPerTemplate{0x10000};
    
    /// Minimum time between block template updates (seconds)
    int templateRefreshInterval{30};
    
    /// Enable extra nonce in coinbase for more nonce space
    bool useExtraNonce{true};
};

// ============================================================================
// Miner Class
// ============================================================================

/**
 * CPU miner for SHURIUM.
 * 
 * Manages mining threads that:
 * 1. Get block templates from BlockAssembler
 * 2. Search for valid nonces
 * 3. Submit valid blocks via ChainStateManager
 */
class Miner {
public:
    /// Callback when a block is found
    using BlockFoundCallback = std::function<void(const Block& block, bool accepted)>;
    
    /**
     * Construct a miner.
     * 
     * @param chainman Chain state manager for block submission
     * @param mempool Mempool for transaction selection
     * @param params Consensus parameters
     * @param options Miner configuration
     */
    Miner(ChainStateManager& chainman,
          Mempool& mempool,
          const consensus::Params& params,
          const MinerOptions& options = {});
    
    /// Destructor (stops mining if running)
    ~Miner();
    
    // Prevent copying
    Miner(const Miner&) = delete;
    Miner& operator=(const Miner&) = delete;
    
    // ========================================================================
    // Control
    // ========================================================================
    
    /**
     * Start mining with the configured number of threads.
     * @return true if mining started successfully
     */
    bool Start();
    
    /**
     * Stop all mining threads.
     */
    void Stop();
    
    /**
     * Check if mining is running.
     */
    bool IsRunning() const { return running_.load(); }
    
    /**
     * Set the message processor for block relay.
     */
    void SetMessageProcessor(MessageProcessor* msgproc) { msgproc_ = msgproc; }
    
    /**
     * Update the coinbase address.
     */
    void SetCoinbaseAddress(const Hash160& address);
    
    /**
     * Set callback for block found events.
     */
    void SetBlockFoundCallback(BlockFoundCallback callback);
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /**
     * Get current mining statistics.
     */
    const MiningStats& GetStats() const { return stats_; }
    
    /**
     * Reset mining statistics.
     */
    void ResetStats() { stats_.Reset(); }
    
    /**
     * Get current hash rate across all threads.
     */
    double GetHashRate() const { return stats_.GetHashRate(); }
    
    /**
     * Check if hash meets target (hash <= target).
     * Made public for use by CheckProofOfWork.
     */
    static bool MeetsTarget(const Hash256& hash, const Hash256& target);
    
private:
    /// Mining thread function
    void MiningThread(int threadId);
    
    /// Try to mine a single block template
    bool TryMineBlock(BlockTemplate& tmpl, int threadId);
    
    /// Submit a valid block
    bool SubmitBlock(Block& block);
    
    // References
    ChainStateManager& chainman_;
    Mempool& mempool_;
    const consensus::Params& params_;
    MessageProcessor* msgproc_{nullptr};
    
    // Configuration
    MinerOptions options_;
    
    // State
    std::atomic<bool> running_{false};
    std::atomic<bool> shouldStop_{false};
    std::vector<std::thread> threads_;
    
    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    // Extra nonce for expanding nonce space
    std::atomic<uint32_t> extraNonce_{0};
    
    // Statistics
    MiningStats stats_;
    
    // Callbacks
    BlockFoundCallback blockFoundCallback_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Generate miner threads based on hardware.
 * @param requestedThreads User-requested threads (0 for auto)
 * @return Actual number of threads to use
 */
int GetMiningThreadCount(int requestedThreads);

/**
 * Hash a block header and check against target.
 * @param header Block header to hash
 * @param target Target hash
 * @return true if hash <= target
 */
bool CheckProofOfWork(const BlockHeader& header, const Hash256& target);

} // namespace miner
} // namespace shurium

#endif // SHURIUM_MINER_MINER_H
