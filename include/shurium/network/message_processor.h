// SHURIUM - Message Processor
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the MessageProcessor class that routes received P2P
// messages to their appropriate handlers (sync, mempool, ping/pong, etc.).

#ifndef SHURIUM_NETWORK_MESSAGE_PROCESSOR_H
#define SHURIUM_NETWORK_MESSAGE_PROCESSOR_H

#include <shurium/network/connection.h>
#include <shurium/network/peer.h>
#include <shurium/network/protocol.h>
#include <shurium/network/sync.h>
#include <shurium/core/serialize.h>
#include <shurium/core/block.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace shurium {

// Forward declarations
class Mempool;
class ChainStateManager;
class CoinsView;
class AddressManager;
struct NodeContext;

namespace db {
class BlockDB;
}

// ============================================================================
// Message Processing Statistics
// ============================================================================

struct MessageStats {
    uint64_t messagesProcessed{0};
    uint64_t versionMessages{0};
    uint64_t verackMessages{0};
    uint64_t pingMessages{0};
    uint64_t pongMessages{0};
    uint64_t invMessages{0};
    uint64_t getdataMessages{0};
    uint64_t headersMessages{0};
    uint64_t blockMessages{0};
    uint64_t txMessages{0};
    uint64_t addrMessages{0};
    uint64_t unknownMessages{0};
    uint64_t invalidMessages{0};
};

// ============================================================================
// Message Processor
// ============================================================================

// ============================================================================
// Message Processor Options
// ============================================================================

struct MessageProcessorOptions {
    /// Interval between message processing cycles (milliseconds)
    int processingIntervalMs{100};
    
    /// Interval between ping sends (seconds)
    int pingIntervalSec{120};
    
    /// Ping timeout (seconds)
    int pingTimeoutSec{30};
    
    /// Maximum messages to process per peer per cycle
    int maxMessagesPerPeer{100};
    
    /// Enable transaction relay
    bool relayTransactions{true};
};

// ============================================================================
// Message Processor
// ============================================================================

/**
 * Processes incoming P2P messages and routes them to handlers.
 * 
 * The MessageProcessor:
 * - Polls peers for received messages via Peer::GetNextMessage()
 * - Dispatches messages to appropriate handlers based on command type
 * - Manages handshake flow (version/verack)
 * - Coordinates with BlockSynchronizer for block/header sync
 * - Handles transaction relay via mempool
 * - Manages ping/pong keepalive
 * 
 * Thread safety: All public methods are thread-safe.
 */
class MessageProcessor {
public:
    using Options = MessageProcessorOptions;
    
    /// Callback when handshake completes
    using HandshakeCallback = std::function<void(Peer::Id peerId)>;
    
    /// Callback for serving getdata requests (block/tx)
    using GetDataCallback = std::function<bool(Peer::Id peerId, const Inv& inv)>;
    
    /// Callback for serving getheaders requests
    using GetHeadersCallback = std::function<std::vector<BlockHeader>(
        const BlockLocator& locator, const Hash256& stopHash)>;
    
    // ========================================================================
    // Construction
    // ========================================================================
    
    explicit MessageProcessor(const Options& opts = Options{});
    ~MessageProcessor();
    
    // Non-copyable
    MessageProcessor(const MessageProcessor&) = delete;
    MessageProcessor& operator=(const MessageProcessor&) = delete;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /**
     * Initialize with required components.
     * Must be called before Start().
     * 
     * @param connman Connection manager for peer access
     * @param sync Block synchronizer for block/header handling
     */
    void Initialize(ConnectionManager* connman, BlockSynchronizer* sync);
    
    /**
     * Set optional mempool for transaction handling.
     * If not set, received transactions are ignored.
     */
    void SetMempool(Mempool* mempool);
    
    /**
     * Set optional chain state manager for getdata/getheaders serving.
     */
    void SetChainManager(ChainStateManager* chainman);
    
    /**
     * Set optional coins view for transaction validation.
     */
    void SetCoinsView(CoinsView* coins);
    
    /**
     * Set optional address manager for peer discovery.
     */
    void SetAddressManager(AddressManager* addrman);
    
    /**
     * Set optional block database for serving block data.
     */
    void SetBlockDB(db::BlockDB* blockdb);
    
    /**
     * Set our local address for version messages.
     */
    void SetLocalAddress(const NetService& addr);
    
    /**
     * Set current chain height (for coinbase maturity checks).
     */
    void SetChainHeight(int32_t height);
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    /**
     * Start the message processing thread.
     */
    bool Start();
    
    /**
     * Stop the message processing thread.
     */
    void Stop();
    
    /**
     * Check if running.
     */
    bool IsRunning() const { return running_.load(); }
    
    /**
     * Process messages for all peers (single iteration).
     * Called automatically by the background thread, but can also
     * be called manually for testing or single-threaded use.
     */
    void ProcessMessages();
    
    // ========================================================================
    // Callbacks
    // ========================================================================
    
    /// Set callback for handshake completion
    void SetHandshakeCallback(HandshakeCallback cb) { handshakeCallback_ = std::move(cb); }
    
    /// Set callback for getdata requests
    void SetGetDataCallback(GetDataCallback cb) { getDataCallback_ = std::move(cb); }
    
    /// Set callback for getheaders requests
    void SetGetHeadersCallback(GetHeadersCallback cb) { getHeadersCallback_ = std::move(cb); }
    
    // ========================================================================
    // Transaction and Block Relay
    // ========================================================================
    
    /**
     * Relay a transaction to all connected peers.
     * Sends an inv message announcing the transaction.
     * 
     * @param txid Transaction hash to relay
     * @param excludePeer Optional peer ID to exclude from relay (e.g., the sender)
     */
    void RelayTransaction(const TxHash& txid, Peer::Id excludePeer = 0);
    
    /**
     * Relay a block to all connected peers.
     * Sends an inv message announcing the block.
     * 
     * @param blockHash Block hash to relay
     * @param excludePeer Optional peer ID to exclude from relay
     */
    void RelayBlock(const BlockHash& blockHash, Peer::Id excludePeer = 0);
    
    /**
     * Queue a transaction for relay (batched).
     * More efficient than calling RelayTransaction for each tx.
     * Call FlushRelayQueue() to send all queued announcements.
     */
    void QueueTransactionRelay(const TxHash& txid);
    
    /**
     * Flush the relay queue, sending all pending inv announcements.
     */
    void FlushRelayQueue();
    
    // ========================================================================
    // Node Context Integration
    // ========================================================================
    
    /**
     * Convenience method to initialize from NodeContext.
     * Extracts connman, mempool, chainman, coins from the context.
     */
    void InitializeFromContext(NodeContext& ctx);
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get message processing statistics
    MessageStats GetStats() const;
    
    /// Reset statistics
    void ResetStats();

private:
    // ========================================================================
    // Message Processing (Internal)
    // ========================================================================
    
    /// Process messages for a single peer
    void ProcessPeerMessages(std::shared_ptr<Peer> peer);
    
    /// Dispatch a single message to its handler
    bool DispatchMessage(Peer& peer, const std::string& command, 
                         const std::vector<uint8_t>& payload);
    
    // ========================================================================
    // Message Handlers
    // ========================================================================
    
    /// Handle version message
    bool HandleVersion(Peer& peer, DataStream& payload);
    
    /// Handle verack message
    bool HandleVerack(Peer& peer);
    
    /// Handle ping message
    bool HandlePing(Peer& peer, DataStream& payload);
    
    /// Handle pong message
    bool HandlePong(Peer& peer, DataStream& payload);
    
    /// Handle inv message
    bool HandleInv(Peer& peer, DataStream& payload);
    
    /// Handle getdata message
    bool HandleGetData(Peer& peer, DataStream& payload);
    
    /// Handle headers message
    bool HandleHeaders(Peer& peer, DataStream& payload);
    
    /// Handle block message
    bool HandleBlock(Peer& peer, DataStream& payload);
    
    /// Handle tx message
    bool HandleTx(Peer& peer, DataStream& payload);
    
    /// Handle getheaders message
    bool HandleGetHeaders(Peer& peer, DataStream& payload);
    
    /// Handle getblocks message
    bool HandleGetBlocks(Peer& peer, DataStream& payload);
    
    /// Handle addr message
    bool HandleAddr(Peer& peer, DataStream& payload);
    
    /// Handle sendheaders message
    bool HandleSendHeaders(Peer& peer);
    
    /// Handle feefilter message
    bool HandleFeeFilter(Peer& peer, DataStream& payload);
    
    /// Handle mempool message
    bool HandleMempool(Peer& peer);
    
    /// Handle notfound message
    bool HandleNotFound(Peer& peer, DataStream& payload);
    
    // ========================================================================
    // Periodic Tasks
    // ========================================================================
    
    /// Send ping to peers that need it
    void SendPings();
    
    /// Check for ping timeouts
    void CheckPingTimeouts();
    
    /// Announce pending inventory
    void SendAnnouncements();
    
    // ========================================================================
    // Helper Methods
    // ========================================================================
    
    /// Create and send version message to peer
    void SendVersion(Peer& peer);
    
    /// Send verack to peer
    void SendVerack(Peer& peer);
    
    /// Send pong response
    void SendPong(Peer& peer, uint64_t nonce);
    
    /// Send headers to peer
    void SendHeaders(Peer& peer, const std::vector<BlockHeader>& headers);
    
    /// Send inv message to peer
    void SendInv(Peer& peer, const std::vector<Inv>& inv);
    
    /// Send getdata message to peer  
    void SendGetData(Peer& peer, const std::vector<Inv>& inv);
    
    // ========================================================================
    // Background Thread
    // ========================================================================
    
    void ProcessingLoop();
    
    // ========================================================================
    // Member Variables
    // ========================================================================
    
    Options options_;
    std::atomic<bool> running_{false};
    std::thread processingThread_;
    
    // Component references (not owned)
    ConnectionManager* connman_{nullptr};
    BlockSynchronizer* sync_{nullptr};
    Mempool* mempool_{nullptr};
    ChainStateManager* chainman_{nullptr};
    CoinsView* coins_{nullptr};
    AddressManager* addrman_{nullptr};
    db::BlockDB* blockdb_{nullptr};
    
    // Chain height for tx validation
    std::atomic<int32_t> chainHeight_{0};
    
    // Our local address for version messages
    NetService localAddress_;
    
    // Our service flags
    ServiceFlags ourServices_{ServiceFlags::NETWORK};
    
    // Callbacks
    HandshakeCallback handshakeCallback_;
    GetDataCallback getDataCallback_;
    GetHeadersCallback getHeadersCallback_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    MessageStats stats_;
    
    // Per-peer state for ping tracking
    mutable std::mutex pingMutex_;
    std::map<Peer::Id, std::chrono::steady_clock::time_point> lastPingTime_;
    
    // Transaction relay queue
    mutable std::mutex relayMutex_;
    std::vector<TxHash> pendingTxRelay_;
    std::vector<BlockHash> pendingBlockRelay_;
};

// ============================================================================
// Integration Helper
// ============================================================================

/**
 * Start message processing for the node.
 * Creates and configures a MessageProcessor and starts it.
 * 
 * @param node The node context
 * @return Pointer to the created processor (owned by caller)
 */
std::unique_ptr<MessageProcessor> StartMessageProcessor(NodeContext& node);

} // namespace shurium

#endif // SHURIUM_NETWORK_MESSAGE_PROCESSOR_H
