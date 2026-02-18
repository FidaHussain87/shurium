// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#pragma once

#include <shurium/network/address.h>
#include <shurium/network/protocol.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_set>

namespace shurium {

// ============================================================================
// Connection Types
// ============================================================================

/// Type of peer connection
enum class ConnectionType {
    INBOUND,            ///< Peer connected to us
    OUTBOUND_FULL_RELAY, ///< Full relay outbound connection
    MANUAL,             ///< User-specified connection (-addnode)
    FEELER,             ///< Test if peer is reachable
    BLOCK_RELAY,        ///< Block-only relay connection
    ADDR_FETCH,         ///< Connection only for getting addresses
};

/// State of handshake with peer
enum class PeerState {
    DISCONNECTED,       ///< Not connected
    CONNECTING,         ///< TCP connection in progress
    CONNECTED,          ///< TCP connected, waiting for version
    VERSION_SENT,       ///< We sent our version
    VERSION_RECEIVED,   ///< We received their version
    ESTABLISHED,        ///< Handshake complete (verack exchanged)
};

/// Reason for disconnection
enum class DisconnectReason {
    NONE,
    MANUALLY_REQUESTED,
    TIMEOUT,
    PROTOCOL_ERROR,
    BAD_VERSION,
    DUPLICATE,
    SELF_CONNECTION,
    NETWORK_ERROR,
    TOO_MANY_CONNECTIONS,
    BANNED,
    MISBEHAVIOR,
};

// ============================================================================
// Peer Statistics
// ============================================================================

/// Statistics about a peer connection
struct PeerStats {
    int64_t connectedTime{0};         ///< When connection was established
    int64_t lastSendTime{0};          ///< Last time we sent a message
    int64_t lastRecvTime{0};          ///< Last time we received a message
    int64_t lastPingTime{0};          ///< Last time we sent a ping
    int64_t lastPongTime{0};          ///< Last time we received matching pong
    int64_t pingWaitTime{0};          ///< Time since last ping (if waiting)
    
    uint64_t bytesSent{0};            ///< Total bytes sent
    uint64_t bytesRecv{0};            ///< Total bytes received
    uint64_t messagesSent{0};         ///< Total messages sent
    uint64_t messagesRecv{0};         ///< Total messages received
    
    int32_t nVersion{0};              ///< Peer's protocol version
    std::string userAgent;            ///< Peer's user agent string
    int32_t startingHeight{0};        ///< Peer's best block height at connect
    ServiceFlags services{ServiceFlags::NONE}; ///< Peer's service flags
    
    bool fInbound{false};             ///< True if inbound connection
    bool fSuccessfullyConnected{false}; ///< Handshake completed
    bool fRelayTxes{false};           ///< Peer wants transaction relay
    bool preferHeaders{false};        ///< Peer prefers headers announcements
    bool supportsAddrV2{false};       ///< Peer supports ADDRv2
    
    /// Current ping latency in microseconds
    int64_t pingLatencyMicros{0};
    
    /// Misbehavior score (for DoS protection)
    int32_t misbehaviorScore{0};
};

// ============================================================================
// Peer Class
// ============================================================================

/**
 * Represents a network peer.
 * 
 * Tracks connection state, handles protocol handshake, and manages
 * message exchange with a single peer.
 */
class Peer {
public:
    /// Unique identifier for this peer
    using Id = int64_t;
    
    /// Callback for processing received messages
    using MessageHandler = std::function<bool(Peer&, const std::string& cmd, 
                                               DataStream& payload)>;
    
    /// Callback for connection state changes
    using StateHandler = std::function<void(Peer&, PeerState oldState, 
                                            PeerState newState)>;
    
    // ========================================================================
    // Construction
    // ========================================================================
    
    /// Create a peer for an outbound connection
    static std::unique_ptr<Peer> CreateOutbound(Id id, const NetService& addr,
                                                 ConnectionType connType);
    
    /// Create a peer for an inbound connection
    static std::unique_ptr<Peer> CreateInbound(Id id, const NetService& addr);
    
    /// Destructor
    ~Peer();
    
    // ========================================================================
    // Identification
    // ========================================================================
    
    /// Get peer ID
    Id GetId() const { return id_; }
    
    /// Get peer address
    const NetService& GetAddress() const { return address_; }
    
    /// Get connection type
    ConnectionType GetConnectionType() const { return connType_; }
    
    /// Check if this is an inbound connection
    bool IsInbound() const { return connType_ == ConnectionType::INBOUND; }
    
    /// Check if this is an outbound connection
    bool IsOutbound() const { return !IsInbound(); }
    
    // ========================================================================
    // State Management
    // ========================================================================
    
    /// Get current connection state
    PeerState GetState() const { return state_; }
    
    /// Check if handshake is complete
    bool IsEstablished() const { return state_ == PeerState::ESTABLISHED; }
    
    /// Check if we should disconnect
    bool ShouldDisconnect() const { return fDisconnect_.load(); }
    
    /// Mark for disconnection
    void Disconnect(DisconnectReason reason = DisconnectReason::MANUALLY_REQUESTED);
    
    /// Get disconnection reason
    DisconnectReason GetDisconnectReason() const { return disconnectReason_; }
    
    // ========================================================================
    // Handshake
    // ========================================================================
    
    /// Create version message to send to this peer
    VersionMessage CreateVersionMessage(const NetService& ourAddr, 
                                         int32_t ourHeight,
                                         ServiceFlags ourServices) const;
    
    /// Process received version message
    bool ProcessVersion(const VersionMessage& version);
    
    /// Process received verack message
    bool ProcessVerack();
    
    /// Check if we've sent version
    bool HasSentVersion() const { return fSentVersion_; }
    
    /// Check if we've received version
    bool HasReceivedVersion() const { return fReceivedVersion_; }
    
    /// Mark version as sent
    void SetVersionSent();
    
    // ========================================================================
    // Services
    // ========================================================================
    
    /// Get peer's protocol version
    int32_t GetVersion() const { return stats_.nVersion; }
    
    /// Get peer's service flags
    ServiceFlags GetServices() const { return stats_.services; }
    
    /// Check if peer has a specific service
    bool HasService(ServiceFlags flag) const {
        return HasFlag(stats_.services, flag);
    }
    
    /// Get peer's user agent
    const std::string& GetUserAgent() const { return stats_.userAgent; }
    
    /// Get peer's starting height
    int32_t GetStartingHeight() const { return stats_.startingHeight; }
    
    /// Check if peer relays transactions
    bool RelaysTransactions() const { return stats_.fRelayTxes; }
    
    /// Set transaction relay preference
    void SetRelayTxes(bool relay) { stats_.fRelayTxes = relay; }
    
    /// Check if peer prefers header announcements
    bool PrefersHeaders() const { return stats_.preferHeaders; }
    
    /// Set header announcement preference
    void SetPrefersHeaders(bool prefer) { stats_.preferHeaders = prefer; }
    
    // ========================================================================
    // Fee Filter
    // ========================================================================
    
    /// Get peer's minimum fee rate for relay (satoshis per KB)
    int64_t GetFeeFilter() const { return feeFilter_.load(); }
    
    /// Set peer's minimum fee rate for relay (satoshis per KB)
    void SetFeeFilter(int64_t minFeeRate) { feeFilter_.store(minFeeRate); }
    
    // ========================================================================
    // Ping/Pong
    // ========================================================================
    
    /// Get the current ping nonce (0 if not waiting for pong)
    uint64_t GetPingNonce() const { return pingNonce_; }
    
    /// Send a ping, returns the nonce used
    uint64_t SendPing();
    
    /// Process received pong
    bool ProcessPong(uint64_t nonce);
    
    /// Get ping latency in milliseconds (-1 if unknown)
    int64_t GetPingLatency() const;
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get peer statistics (thread-safe copy)
    PeerStats GetStats() const;
    
    /// Record bytes sent
    void RecordBytesSent(size_t bytes);
    
    /// Record bytes received
    void RecordBytesReceived(size_t bytes);
    
    /// Record message sent
    void RecordMessageSent();
    
    /// Record message received  
    void RecordMessageReceived();
    
    // ========================================================================
    // Inventory Tracking
    // ========================================================================
    
    /// Check if we've announced an inventory item to this peer
    bool HasAnnounced(const Inv& inv) const;
    
    /// Mark inventory item as announced to this peer
    void MarkAnnounced(const Inv& inv);
    
    /// Check if peer knows about an inventory item
    bool HasInventory(const Inv& inv) const;
    
    /// Mark peer as having an inventory item
    void AddInventory(const Inv& inv);
    
    /// Get count of inventory items to announce
    size_t GetAnnouncementQueueSize() const;
    
    /// Add item to announcement queue
    void QueueAnnouncement(const Inv& inv);
    
    /// Get items to announce (up to maxCount)
    std::vector<Inv> GetAnnouncementsToSend(size_t maxCount);
    
    // ========================================================================
    // Misbehavior Tracking
    // ========================================================================
    
    /// Increase misbehavior score
    /// @param howMuch Amount to increase
    /// @return true if peer should be banned
    bool Misbehaving(int howMuch, const std::string& reason = "");
    
    /// Get current misbehavior score
    int32_t GetMisbehaviorScore() const { return stats_.misbehaviorScore; }
    
    /// Reset misbehavior score
    void ResetMisbehavior() { stats_.misbehaviorScore = 0; }
    
    /// Ban threshold
    static constexpr int32_t BAN_THRESHOLD = 100;
    
    // ========================================================================
    // Send/Receive Buffers
    // ========================================================================
    
    /// Queue data for sending
    void QueueSend(const std::vector<uint8_t>& data);
    
    /// Queue a message for sending
    template<typename T>
    void QueueMessage(const std::string& command, const T& payload);
    
    /// Queue a message with no payload
    void QueueMessage(const std::string& command);
    
    /// Get data from send buffer (for socket write)
    std::vector<uint8_t> GetSendData(size_t maxBytes);
    
    /// Check if there's data to send
    bool HasDataToSend() const;
    
    /// Add received data to buffer
    void AddReceivedData(const std::vector<uint8_t>& data);
    
    /// Try to extract a complete message from receive buffer
    /// @return pair of (command, payload) or nullopt if incomplete
    std::optional<std::pair<std::string, std::vector<uint8_t>>> GetNextMessage();
    
private:
    // Private constructor - use factory methods
    Peer(Id id, const NetService& addr, ConnectionType type);
    
    // Non-copyable
    Peer(const Peer&) = delete;
    Peer& operator=(const Peer&) = delete;
    
    /// Change state and notify handler
    void SetState(PeerState newState);
    
    // ========================================================================
    // Member Variables
    // ========================================================================
    
    Id id_;
    NetService address_;
    ConnectionType connType_;
    std::atomic<PeerState> state_{PeerState::DISCONNECTED};
    std::atomic<bool> fDisconnect_{false};
    DisconnectReason disconnectReason_{DisconnectReason::NONE};
    
    // Handshake state
    bool fSentVersion_{false};
    bool fReceivedVersion_{false};
    
    // Ping/pong
    uint64_t pingNonce_{0};
    std::chrono::steady_clock::time_point pingStart_;
    
    // Fee filter - minimum fee rate this peer will relay (satoshis per KB)
    std::atomic<int64_t> feeFilter_{0};
    
    // Statistics
    mutable std::mutex statsMutex_;
    PeerStats stats_;
    
    // Inventory tracking
    mutable std::mutex invMutex_;
    std::set<Inv> announcedToUs_;     // Items peer told us about
    std::set<Inv> announcedByUs_;     // Items we told peer about
    std::deque<Inv> announcementQueue_; // Items queued for announcement
    
    // Send/receive buffers
    mutable std::mutex sendMutex_;
    std::deque<uint8_t> sendBuffer_;
    
    mutable std::mutex recvMutex_;
    std::deque<uint8_t> recvBuffer_;
    
    // Network magic for this peer (set during handshake)
    std::array<uint8_t, 4> networkMagic_{NetworkMagic::MAINNET};
    
    // Callbacks
    MessageHandler messageHandler_;
    StateHandler stateHandler_;
};

// ============================================================================
// Template Implementation
// ============================================================================

template<typename T>
void Peer::QueueMessage(const std::string& command, const T& payload) {
    DataStream stream;
    // Use member Serialize if available, otherwise this won't compile
    // for types without it - caller must use appropriate types
    payload.Serialize(stream);
    std::vector<uint8_t> payloadBytes(stream.begin(), stream.end());
    auto msg = CreateMessage(networkMagic_, command, payloadBytes);
    QueueSend(msg);
}

} // namespace shurium
