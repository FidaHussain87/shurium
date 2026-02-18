// SHURIUM - Connection Management
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// TCP socket connection management with event-driven I/O.
// Provides platform abstraction for async networking.

#ifndef SHURIUM_NETWORK_CONNECTION_H
#define SHURIUM_NETWORK_CONNECTION_H

#include <shurium/network/address.h>
#include <shurium/network/peer.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace shurium {

// ============================================================================
// Forward Declarations
// ============================================================================

class Connection;
class ConnectionManager;
class EventLoop;

// ============================================================================
// Socket Handle
// ============================================================================

#ifdef _WIN32
using SocketHandle = uintptr_t;
constexpr SocketHandle INVALID_SOCKET_HANDLE = ~static_cast<uintptr_t>(0);
#else
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

// ============================================================================
// Connection State
// ============================================================================

enum class ConnState {
    DISCONNECTED,   ///< Not connected
    CONNECTING,     ///< Connection in progress
    CONNECTED,      ///< Connection established
    CLOSING,        ///< Graceful shutdown in progress
    ERROR           ///< Connection error occurred
};

// ============================================================================
// Connection Events
// ============================================================================

/// Event types for connection callbacks
enum class ConnEvent {
    CONNECTED,      ///< Connection established
    DISCONNECTED,   ///< Connection closed
    DATA_RECEIVED,  ///< Data available to read
    DATA_SENT,      ///< Write buffer space available
    ERROR           ///< Error occurred
};

// ============================================================================
// Connection Options
// ============================================================================

struct ConnectionOptions {
    /// Connect timeout in milliseconds
    int connectTimeoutMs{10000};
    
    /// Read buffer size
    size_t readBufferSize{65536};
    
    /// Write buffer size
    size_t writeBufferSize{65536};
    
    /// Enable TCP keepalive
    bool keepAlive{true};
    
    /// Keepalive idle time (seconds)
    int keepAliveIdle{60};
    
    /// Keepalive interval (seconds)
    int keepAliveInterval{10};
    
    /// Keepalive probe count
    int keepAliveCount{5};
    
    /// Enable TCP_NODELAY (disable Nagle)
    bool noDelay{true};
    
    /// Receive buffer size (SO_RCVBUF, 0 = system default)
    int recvBufSize{0};
    
    /// Send buffer size (SO_SNDBUF, 0 = system default)
    int sendBufSize{0};
};

// ============================================================================
// Connection Class
// ============================================================================

/**
 * Represents a single TCP connection.
 * 
 * Handles async read/write operations and connection lifecycle.
 * Typically managed by ConnectionManager.
 */
class Connection {
public:
    using EventCallback = std::function<void(Connection&, ConnEvent)>;
    using DataCallback = std::function<void(Connection&, const uint8_t*, size_t)>;
    using ErrorCallback = std::function<void(Connection&, int errorCode, const std::string& msg)>;
    
    // ========================================================================
    // Construction/Destruction
    // ========================================================================
    
    /// Create connection for outbound connection to address
    static std::unique_ptr<Connection> Create(const NetService& addr,
                                               const ConnectionOptions& opts = {});
    
    /// Create connection from accepted socket
    static std::unique_ptr<Connection> FromSocket(SocketHandle socket,
                                                   const NetService& remoteAddr,
                                                   const ConnectionOptions& opts = {});
    
    /// Destructor - closes connection
    ~Connection();
    
    // Non-copyable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    
    // ========================================================================
    // Connection Management
    // ========================================================================
    
    /// Initiate async connection
    /// @return true if connect initiated, false on immediate error
    bool Connect();
    
    /// Close the connection
    /// @param graceful If true, attempt graceful shutdown
    void Close(bool graceful = true);
    
    /// Get current connection state
    ConnState GetState() const { return state_.load(); }
    
    /// Check if connected
    bool IsConnected() const { return state_.load() == ConnState::CONNECTED; }
    
    /// Check if connection is usable (connected or connecting)
    bool IsActive() const {
        auto s = state_.load();
        return s == ConnState::CONNECTED || s == ConnState::CONNECTING;
    }
    
    /// Get remote address
    const NetService& GetRemoteAddress() const { return remoteAddr_; }
    
    /// Get local address (only valid after connected)
    NetService GetLocalAddress() const;
    
    /// Get underlying socket handle
    SocketHandle GetSocket() const { return socket_; }
    
    // ========================================================================
    // Data Transfer
    // ========================================================================
    
    /// Queue data for sending
    /// @param data Data to send
    /// @param len Length of data
    /// @return Number of bytes queued (may be less if buffer full)
    size_t Send(const uint8_t* data, size_t len);
    
    /// Queue data vector for sending
    size_t Send(const std::vector<uint8_t>& data) {
        return Send(data.data(), data.size());
    }
    
    /// Read available data from receive buffer
    /// @param buffer Output buffer
    /// @param maxLen Maximum bytes to read
    /// @return Number of bytes read
    size_t Recv(uint8_t* buffer, size_t maxLen);
    
    /// Read all available data
    std::vector<uint8_t> RecvAll();
    
    /// Get number of bytes in send buffer
    size_t GetSendBufferSize() const;
    
    /// Get number of bytes in receive buffer
    size_t GetRecvBufferSize() const;
    
    /// Check if there's data to send
    bool HasPendingData() const { return GetSendBufferSize() > 0; }
    
    // ========================================================================
    // Callbacks
    // ========================================================================
    
    /// Set event callback
    void SetEventCallback(EventCallback cb) { eventCallback_ = std::move(cb); }
    
    /// Set data received callback
    void SetDataCallback(DataCallback cb) { dataCallback_ = std::move(cb); }
    
    /// Set error callback
    void SetErrorCallback(ErrorCallback cb) { errorCallback_ = std::move(cb); }
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get total bytes sent
    uint64_t GetBytesSent() const { return bytesSent_.load(); }
    
    /// Get total bytes received
    uint64_t GetBytesRecv() const { return bytesRecv_.load(); }
    
    /// Get connection time
    std::chrono::steady_clock::time_point GetConnectTime() const { return connectTime_; }
    
    /// Get last activity time
    std::chrono::steady_clock::time_point GetLastActivityTime() const { return lastActivity_; }
    
    // ========================================================================
    // Internal - called by EventLoop
    // ========================================================================
    
    /// Handle socket readable event
    void OnReadable();
    
    /// Handle socket writable event
    void OnWritable();
    
    /// Handle socket error
    void OnError(int errorCode);
    
    /// Handle connect completion
    void OnConnectComplete(bool success);

private:
    Connection(const NetService& addr, const ConnectionOptions& opts);
    Connection(SocketHandle socket, const NetService& addr, const ConnectionOptions& opts);
    
    /// Set socket options
    bool ConfigureSocket();
    
    /// Set connection state
    void SetState(ConnState newState);
    
    // Socket and addressing
    SocketHandle socket_{INVALID_SOCKET_HANDLE};
    NetService remoteAddr_;
    ConnectionOptions options_;
    
    // State
    std::atomic<ConnState> state_{ConnState::DISCONNECTED};
    
    // Buffers
    mutable std::mutex sendMutex_;
    std::vector<uint8_t> sendBuffer_;
    size_t sendOffset_{0};  // Offset of first unsent byte
    
    mutable std::mutex recvMutex_;
    std::vector<uint8_t> recvBuffer_;
    
    // Statistics
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> bytesRecv_{0};
    std::chrono::steady_clock::time_point connectTime_;
    std::chrono::steady_clock::time_point lastActivity_;
    
    // Callbacks
    EventCallback eventCallback_;
    DataCallback dataCallback_;
    ErrorCallback errorCallback_;
};

// ============================================================================
// Listener Class
// ============================================================================

/**
 * TCP listener for accepting incoming connections.
 */
class Listener {
public:
    using AcceptCallback = std::function<void(std::unique_ptr<Connection>)>;
    
    /// Create listener on specified address and port
    static std::unique_ptr<Listener> Create(const NetAddress& bindAddr, uint16_t port);
    
    /// Create listener on all interfaces
    static std::unique_ptr<Listener> Create(uint16_t port);
    
    ~Listener();
    
    /// Start listening
    /// @param backlog Connection backlog size
    bool Start(int backlog = 128);
    
    /// Stop listening
    void Stop();
    
    /// Check if listening
    bool IsListening() const { return listening_.load(); }
    
    /// Get the listening address
    NetService GetListenAddress() const;
    
    /// Get socket handle
    SocketHandle GetSocket() const { return socket_; }
    
    /// Set callback for accepted connections
    void SetAcceptCallback(AcceptCallback cb) { acceptCallback_ = std::move(cb); }
    
    /// Handle incoming connection (called by EventLoop)
    void OnAccept();

private:
    Listener(const NetAddress& addr, uint16_t port);
    
    SocketHandle socket_{INVALID_SOCKET_HANDLE};
    NetAddress bindAddr_;
    uint16_t port_;
    std::atomic<bool> listening_{false};
    AcceptCallback acceptCallback_;
};

// ============================================================================
// Event Loop
// ============================================================================

/**
 * Event loop for managing async I/O on multiple connections.
 * 
 * Uses platform-specific mechanisms (epoll/kqueue/select) internally.
 */
class EventLoop {
public:
    EventLoop();
    ~EventLoop();
    
    /// Start the event loop in a background thread
    void Start();
    
    /// Stop the event loop
    void Stop();
    
    /// Check if running
    bool IsRunning() const { return running_.load(); }
    
    /// Add connection to event loop
    void AddConnection(Connection* conn);
    
    /// Remove connection from event loop
    void RemoveConnection(Connection* conn);
    
    /// Add listener to event loop
    void AddListener(Listener* listener);
    
    /// Remove listener from event loop
    void RemoveListener(Listener* listener);
    
    /// Schedule a callback to run on the event loop thread
    void Post(std::function<void()> callback);
    
    /// Run a single iteration (for manual polling)
    void Poll(int timeoutMs = 0);

private:
    void Run();
    void ProcessEvents(int timeoutMs);
    void ProcessPostedCallbacks();
    
    std::atomic<bool> running_{false};
    std::thread eventThread_;
    
    // Platform-specific event mechanism
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    // Posted callbacks
    std::mutex callbackMutex_;
    std::queue<std::function<void()>> postedCallbacks_;
    
    // Wake-up mechanism
    SocketHandle wakeupSend_{INVALID_SOCKET_HANDLE};
    SocketHandle wakeupRecv_{INVALID_SOCKET_HANDLE};
};

// ============================================================================
// Connection Manager
// ============================================================================

/**
 * Manages multiple peer connections.
 * 
 * Handles connection limits, peer selection, and lifecycle management.
 */
// ============================================================================
// Connection Manager Options
// ============================================================================

struct ConnectionManagerOptions {
    /// Maximum total connections
    size_t maxConnections{125};
    
    /// Maximum inbound connections
    size_t maxInbound{117};
    
    /// Maximum outbound full-relay connections
    size_t maxOutboundFullRelay{8};
    
    /// Number of block-relay-only connections
    size_t blockRelayOnly{2};
    
    /// Number of feeler connections
    size_t maxFeelers{1};
    
    /// Connection timeout (ms)
    int connectTimeoutMs{5000};
    
    /// Bind address for listening (empty = all interfaces)
    std::string bindAddress;
    
    /// Listening port
    uint16_t listenPort{8433};
    
    /// Whether to accept inbound connections
    bool acceptInbound{true};
};

// ============================================================================
// Connection Manager
// ============================================================================

/**
 * Manages multiple peer connections.
 * 
 * Handles connection limits, peer selection, and lifecycle management.
 */
class ConnectionManager {
public:
    using Options = ConnectionManagerOptions;
    using NewPeerCallback = std::function<void(std::shared_ptr<Peer>)>;
    using PeerDisconnectedCallback = std::function<void(Peer::Id, DisconnectReason)>;
    
    explicit ConnectionManager(const Options& opts = Options{});
    ~ConnectionManager();
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    /// Start the connection manager
    bool Start();
    
    /// Stop and disconnect all peers
    void Stop();
    
    /// Check if running
    bool IsRunning() const { return running_.load(); }
    
    // ========================================================================
    // Connection Management
    // ========================================================================
    
    /// Attempt to connect to a peer
    /// @return Peer ID on success, -1 on failure
    Peer::Id ConnectTo(const NetService& addr, ConnectionType type = ConnectionType::OUTBOUND_FULL_RELAY);
    
    /// Disconnect a peer
    void DisconnectPeer(Peer::Id id, DisconnectReason reason = DisconnectReason::MANUALLY_REQUESTED);
    
    /// Disconnect all peers
    void DisconnectAll(DisconnectReason reason = DisconnectReason::MANUALLY_REQUESTED);
    
    /// Ban an address
    void Ban(const NetAddress& addr, int64_t banTimeSeconds = 86400);
    
    /// Unban an address
    void Unban(const NetAddress& addr);
    
    /// Check if address is banned
    bool IsBanned(const NetAddress& addr) const;
    
    // ========================================================================
    // Peer Access
    // ========================================================================
    
    /// Get peer by ID (thread-safe)
    std::shared_ptr<Peer> GetPeer(Peer::Id id) const;
    
    /// Get all connected peers
    std::vector<std::shared_ptr<Peer>> GetAllPeers() const;
    
    /// Get peers by connection type
    std::vector<std::shared_ptr<Peer>> GetPeersByType(ConnectionType type) const;
    
    /// Get number of connected peers
    size_t GetPeerCount() const;
    
    /// Get number of inbound/outbound peers
    size_t GetInboundCount() const;
    size_t GetOutboundCount() const;
    
    // ========================================================================
    // Message Broadcasting
    // ========================================================================
    
    /// Send message to a specific peer
    template<typename T>
    bool SendMessage(Peer::Id id, const std::string& command, const T& payload);
    
    /// Broadcast message to all peers
    template<typename T>
    void BroadcastMessage(const std::string& command, const T& payload);
    
    /// Broadcast message to peers matching predicate
    template<typename T, typename Pred>
    void BroadcastMessageIf(const std::string& command, const T& payload, Pred predicate);
    
    // ========================================================================
    // Callbacks
    // ========================================================================
    
    /// Set callback for new peer connections
    void SetNewPeerCallback(NewPeerCallback cb) { newPeerCallback_ = std::move(cb); }
    
    /// Set callback for peer disconnections
    void SetDisconnectedCallback(PeerDisconnectedCallback cb) { disconnectedCallback_ = std::move(cb); }
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get total bytes sent across all peers
    uint64_t GetTotalBytesSent() const;
    
    /// Get total bytes received across all peers
    uint64_t GetTotalBytesRecv() const;
    
private:
    void AcceptConnection(std::unique_ptr<Connection> conn);
    void HandlePeerDisconnect(Peer::Id id, DisconnectReason reason);
    Peer::Id AllocatePeerId();
    bool CanAcceptConnection(bool inbound) const;
    
    Options options_;
    std::atomic<bool> running_{false};
    
    // Event loop for I/O
    std::unique_ptr<EventLoop> eventLoop_;
    std::unique_ptr<Listener> listener_;
    
    // Peer management
    mutable std::mutex peersMutex_;
    std::unordered_map<Peer::Id, std::shared_ptr<Peer>> peers_;
    std::unordered_map<Peer::Id, std::unique_ptr<Connection>> connections_;
    std::atomic<Peer::Id> nextPeerId_{1};
    
    // Ban list
    mutable std::mutex banMutex_;
    std::map<NetAddress, int64_t> banList_;  // Address -> ban expiry time
    
    // Callbacks
    NewPeerCallback newPeerCallback_;
    PeerDisconnectedCallback disconnectedCallback_;
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T>
bool ConnectionManager::SendMessage(Peer::Id id, const std::string& command, const T& payload) {
    auto peer = GetPeer(id);
    if (!peer) return false;
    peer->QueueMessage(command, payload);
    return true;
}

template<typename T>
void ConnectionManager::BroadcastMessage(const std::string& command, const T& payload) {
    BroadcastMessageIf(command, payload, [](const Peer&) { return true; });
}

template<typename T, typename Pred>
void ConnectionManager::BroadcastMessageIf(const std::string& command, const T& payload, Pred predicate) {
    auto peers = GetAllPeers();
    for (auto& peer : peers) {
        if (peer->IsEstablished() && predicate(*peer)) {
            peer->QueueMessage(command, payload);
        }
    }
}

} // namespace shurium

#endif // SHURIUM_NETWORK_CONNECTION_H
