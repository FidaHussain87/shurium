// SHURIUM - Block Synchronization
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Block synchronization logic for initial block download (IBD)
// and ongoing chain synchronization with peers.

#ifndef SHURIUM_NETWORK_SYNC_H
#define SHURIUM_NETWORK_SYNC_H

#include <shurium/network/peer.h>
#include <shurium/network/protocol.h>
#include <shurium/core/types.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace shurium {

// Forward declarations
class Block;
class BlockHeader;
class BlockIndex;
class ChainState;

// ============================================================================
// Sync State
// ============================================================================

/// Current state of chain synchronization
enum class SyncState {
    NOT_SYNCING,        ///< Not currently synchronizing
    HEADERS_SYNC,       ///< Downloading headers
    BLOCKS_DOWNLOAD,    ///< Downloading blocks
    BLOCKS_VERIFY,      ///< Verifying downloaded blocks
    NEARLY_SYNCED,      ///< Within a few blocks of tip
    SYNCED              ///< Fully synchronized
};

/// Convert sync state to string
const char* SyncStateToString(SyncState state);

// ============================================================================
// Download Statistics
// ============================================================================

struct SyncStats {
    /// Current sync state
    SyncState state{SyncState::NOT_SYNCING};
    
    /// Number of headers received
    uint64_t headersReceived{0};
    
    /// Number of blocks downloaded
    uint64_t blocksDownloaded{0};
    
    /// Number of blocks verified
    uint64_t blocksVerified{0};
    
    /// Current best header height
    int32_t bestHeaderHeight{0};
    
    /// Current chain height
    int32_t chainHeight{0};
    
    /// Network best height (highest seen)
    int32_t networkHeight{0};
    
    /// Download rate (blocks per second)
    double downloadRate{0.0};
    
    /// Verification rate (blocks per second)
    double verifyRate{0.0};
    
    /// Estimated time to sync (seconds)
    int64_t estimatedTimeRemaining{0};
    
    /// Number of peers we're downloading from
    size_t downloadingPeers{0};
    
    /// Sync start time
    std::chrono::steady_clock::time_point startTime;
};

// ============================================================================
// Peer Sync State
// ============================================================================

/// Per-peer synchronization state
struct PeerSyncState {
    /// Peer's best known header
    Hash256 bestKnownHeader;
    
    /// Peer's best known block
    Hash256 bestKnownBlock;
    
    /// Peer's chain height
    int32_t chainHeight{0};
    
    /// Whether peer supports headers-first sync
    bool supportsHeaders{true};
    
    /// Number of blocks in flight from this peer
    size_t blocksInFlight{0};
    
    /// Maximum blocks in flight per peer
    static constexpr size_t MAX_BLOCKS_IN_FLIGHT = 16;
    
    /// Time of last header request
    std::chrono::steady_clock::time_point lastHeaderRequest;
    
    /// Time of last block request
    std::chrono::steady_clock::time_point lastBlockRequest;
    
    /// Stall detection timestamp
    std::chrono::steady_clock::time_point stallSince;
    
    /// Whether peer is stalling
    bool isStalling{false};
    
    /// Headers we've requested from this peer
    std::set<Hash256> requestedHeaders;
    
    /// Blocks we've requested from this peer
    std::set<Hash256> requestedBlocks;
};

// ============================================================================
// Block Request
// ============================================================================

/// A pending block download request
struct BlockRequest {
    Hash256 hash;
    Peer::Id peerId{-1};
    std::chrono::steady_clock::time_point requestTime;
    int32_t height{0};
    bool received{false};
};

// ============================================================================
// Block Synchronizer
// ============================================================================

/**
 * Coordinates block synchronization with multiple peers.
 * 
 * Implements headers-first synchronization:
 * 1. Download headers to find the best chain
 * 2. Download blocks in parallel from multiple peers
 * 3. Verify and connect blocks to the chain
 */
class BlockSynchronizer {
public:
    /// Callback for new headers
    using HeaderCallback = std::function<bool(const std::vector<BlockHeader>& headers, 
                                               Peer::Id fromPeer)>;
    
    /// Callback for new blocks
    using BlockCallback = std::function<bool(const Block& block, Peer::Id fromPeer)>;
    
    /// Callback for requesting data from peers
    using RequestCallback = std::function<void(Peer::Id peerId, 
                                                const std::string& command,
                                                const std::vector<uint8_t>& payload)>;
    
    /// Callback for sync state changes
    using StateCallback = std::function<void(SyncState oldState, SyncState newState)>;
    
    explicit BlockSynchronizer(size_t maxBlocksInFlight = 1024,
                               size_t maxBlocksPerPeer = 16,
                               int blockTimeoutSec = 60);
    ~BlockSynchronizer();
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    /// Start synchronization
    void Start();
    
    /// Stop synchronization
    void Stop();
    
    /// Check if actively syncing
    bool IsSyncing() const { return state_ != SyncState::NOT_SYNCING && 
                                    state_ != SyncState::SYNCED; }
    
    /// Check if fully synced
    bool IsSynced() const { return state_ == SyncState::SYNCED; }
    
    /// Get current sync state
    SyncState GetState() const { return state_.load(); }
    
    // ========================================================================
    // Peer Management
    // ========================================================================
    
    /// Register a new peer
    void OnPeerConnected(Peer::Id id, int32_t height, ServiceFlags services);
    
    /// Handle peer disconnection
    void OnPeerDisconnected(Peer::Id id);
    
    /// Update peer's best known block
    void UpdatePeerBest(Peer::Id id, const Hash256& hash, int32_t height);
    
    /// Get sync state for a peer
    const PeerSyncState* GetPeerState(Peer::Id id) const;
    
    // ========================================================================
    // Message Processing
    // ========================================================================
    
    /// Process received headers message
    bool ProcessHeaders(Peer::Id fromPeer, const std::vector<BlockHeader>& headers);
    
    /// Process received block message
    bool ProcessBlock(Peer::Id fromPeer, const Block& block);
    
    /// Process received inventory message
    void ProcessInv(Peer::Id fromPeer, const std::vector<Inv>& inv);
    
    /// Process block not found
    void ProcessNotFound(Peer::Id fromPeer, const std::vector<Inv>& inv);
    
    // ========================================================================
    // Sync Control
    // ========================================================================
    
    /// Request headers from peer
    void RequestHeaders(Peer::Id peerId, const BlockLocator& locator, 
                        const Hash256& stopHash = Hash256());
    
    /// Request specific blocks
    void RequestBlocks(Peer::Id peerId, const std::vector<Hash256>& hashes);
    
    /// Periodic tick - check timeouts, request new data
    void Tick();
    
    /// Force resync from a specific height
    void ResyncFrom(int32_t height);
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get sync statistics
    SyncStats GetStats() const;
    
    /// Get number of blocks in flight
    size_t GetBlocksInFlight() const;
    
    /// Get list of peers we're syncing with
    std::vector<Peer::Id> GetSyncPeers() const;
    
    // ========================================================================
    // Callbacks
    // ========================================================================
    
    /// Set header received callback
    void SetHeaderCallback(HeaderCallback cb) { headerCallback_ = std::move(cb); }
    
    /// Set block received callback
    void SetBlockCallback(BlockCallback cb) { blockCallback_ = std::move(cb); }
    
    /// Set request callback
    void SetRequestCallback(RequestCallback cb) { requestCallback_ = std::move(cb); }
    
    /// Set state change callback
    void SetStateCallback(StateCallback cb) { stateCallback_ = std::move(cb); }
    
    // ========================================================================
    // Chain Interface
    // ========================================================================
    
    /// Set the chain height (called by consensus layer)
    void SetChainHeight(int32_t height);
    
    /// Set the best header height
    void SetBestHeaderHeight(int32_t height);
    
    /// Mark block as verified
    void MarkBlockVerified(const Hash256& hash);
    
    /// Check if we need a block
    bool NeedBlock(const Hash256& hash) const;

private:
    /// Set sync state
    void SetState(SyncState newState);
    
    /// Select best peer for header download
    Peer::Id SelectHeaderPeer();
    
    /// Select peers for block download
    std::vector<Peer::Id> SelectBlockPeers();
    
    /// Schedule block downloads
    void ScheduleBlockDownloads();
    
    /// Check for stalled peers
    void CheckStalls();
    
    /// Handle request timeout
    void HandleTimeout(Peer::Id peerId, const Hash256& hash);
    
    /// Calculate sync progress
    void UpdateProgress();
    
    /// Cleanup completed/stale requests
    void CleanupRequests();
    
    // Configuration
    size_t maxBlocksInFlight_{1024};
    size_t maxBlocksPerPeer_{16};
    int blockTimeoutSec_{60};
    
    // Sync state
    std::atomic<SyncState> state_{SyncState::NOT_SYNCING};
    std::atomic<bool> running_{false};
    
    // Heights
    std::atomic<int32_t> chainHeight_{0};
    std::atomic<int32_t> bestHeaderHeight_{0};
    std::atomic<int32_t> networkHeight_{0};
    
    // Statistics
    mutable std::mutex statsMutex_;
    SyncStats stats_;
    
    // Per-peer state
    mutable std::mutex peersMutex_;
    std::unordered_map<Peer::Id, PeerSyncState> peerStates_;
    
    // Block requests
    mutable std::mutex requestsMutex_;
    std::map<Hash256, BlockRequest> pendingRequests_;
    std::deque<Hash256> downloadQueue_;  // Blocks to download
    std::set<Hash256> downloadedBlocks_; // Successfully downloaded
    std::set<Hash256> verifiedBlocks_;   // Verified by consensus
    
    // Callbacks
    HeaderCallback headerCallback_;
    BlockCallback blockCallback_;
    RequestCallback requestCallback_;
    StateCallback stateCallback_;
};

// ============================================================================
// Header Sync Helper
// ============================================================================

/**
 * Manages header chain during synchronization.
 * 
 * Tracks the best header chain before blocks are downloaded and verified.
 */
class HeaderSync {
public:
    HeaderSync();
    ~HeaderSync();
    
    /// Add headers to the chain
    /// @return Number of headers that were new
    size_t AddHeaders(const std::vector<BlockHeader>& headers);
    
    /// Get locator for getheaders request
    BlockLocator GetLocator() const;
    
    /// Get headers to download blocks for
    std::vector<Hash256> GetHeadersToDownload(size_t maxCount) const;
    
    /// Get best header hash
    Hash256 GetBestHeader() const;
    
    /// Get best header height
    int32_t GetBestHeight() const;
    
    /// Check if header exists
    bool HasHeader(const Hash256& hash) const;
    
    /// Get header by hash
    const BlockHeader* GetHeader(const Hash256& hash) const;
    
    /// Mark headers as having blocks downloaded
    void MarkDownloaded(const Hash256& hash);
    
    /// Clear all headers
    void Clear();

private:
    mutable std::mutex mutex_;
    std::map<Hash256, std::unique_ptr<BlockHeader>> headers_;
    std::vector<Hash256> headerChain_;  // Ordered header hashes
    std::set<Hash256> downloadedHeaders_; // Headers with blocks downloaded
};

// ============================================================================
// Inventory Relay Manager
// ============================================================================

/**
 * Manages block/transaction announcements and relay.
 */
class InvRelay {
public:
    InvRelay();
    ~InvRelay();
    
    /// Queue inventory for announcement to peer
    void QueueAnnouncement(Peer::Id peerId, const Inv& inv);
    
    /// Get pending announcements for peer
    std::vector<Inv> GetAnnouncements(Peer::Id peerId, size_t maxCount);
    
    /// Record inventory received from peer
    void RecordReceived(Peer::Id peerId, const Inv& inv);
    
    /// Check if peer already has inventory
    bool PeerHasInv(Peer::Id peerId, const Inv& inv) const;
    
    /// Clean up state for disconnected peer
    void OnPeerDisconnected(Peer::Id peerId);
    
    /// Cleanup old entries
    void Cleanup();

private:
    mutable std::mutex mutex_;
    
    // What we know each peer has
    std::unordered_map<Peer::Id, std::set<Inv>> peerInventory_;
    
    // Pending announcements per peer
    std::unordered_map<Peer::Id, std::deque<Inv>> announceQueue_;
    
    // Recently announced (for deduplication)
    std::set<Inv> recentlyAnnounced_;
};

} // namespace shurium

#endif // SHURIUM_NETWORK_SYNC_H
