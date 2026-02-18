// SHURIUM - Network Manager
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// High-level network management facade for RPC commands.
// Wraps ConnectionManager, AddressManager, and Peer management.

#ifndef SHURIUM_NETWORK_NETWORK_MANAGER_H
#define SHURIUM_NETWORK_NETWORK_MANAGER_H

#include <shurium/network/connection.h>
#include <shurium/network/peer.h>
#include <shurium/network/addrman.h>
#include <shurium/network/address.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace shurium {
namespace network {

// ============================================================================
// Added Node Info
// ============================================================================

/// Information about a manually added node
struct AddedNodeInfo {
    std::string address;
    bool connected{false};
    std::vector<std::pair<std::string, bool>> addresses; // (addr, connected)
};

// ============================================================================
// Network Manager
// ============================================================================

/**
 * High-level network management for RPC commands.
 * 
 * Provides a unified interface for:
 * - Manual node connections (addnode)
 * - Peer disconnection
 * - Ping/pong management
 * - Network activity control
 * - Peer information retrieval
 */
class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /// Set the connection manager (must be called before use)
    void SetConnectionManager(ConnectionManager* connman);
    
    /// Set the address manager (must be called before use)
    void SetAddressManager(AddressManager* addrman);
    
    /// Get connection manager
    ConnectionManager* GetConnectionManager() const { return connman_; }
    
    /// Get address manager
    AddressManager* GetAddressManager() const { return addrman_; }
    
    // ========================================================================
    // Manual Node Management (addnode RPC)
    // ========================================================================
    
    /**
     * Add a node to the manual connection list.
     * @param address Node address (host:port or host)
     * @return true if successfully added
     */
    bool AddNode(const std::string& address);
    
    /**
     * Remove a node from the manual connection list.
     * @param address Node address
     * @return true if successfully removed
     */
    bool RemoveNode(const std::string& address);
    
    /**
     * Try to connect to a node once.
     * @param address Node address
     * @return true if connection attempt started
     */
    bool TryConnectNode(const std::string& address);
    
    /**
     * Get information about manually added nodes.
     * @return Vector of added node info
     */
    std::vector<AddedNodeInfo> GetAddedNodeInfo() const;
    
    /**
     * Get information about a specific added node.
     * @param address Node address
     * @return Node info if found
     */
    std::optional<AddedNodeInfo> GetAddedNodeInfo(const std::string& address) const;
    
    // ========================================================================
    // Peer Disconnection
    // ========================================================================
    
    /**
     * Disconnect a node by address.
     * @param address Node address (host:port)
     * @return true if found and disconnected
     */
    bool DisconnectNode(const std::string& address);
    
    /**
     * Disconnect a node by peer ID.
     * @param peerId Peer identifier
     * @return true if found and disconnected
     */
    bool DisconnectNode(Peer::Id peerId);
    
    // ========================================================================
    // Ping Management
    // ========================================================================
    
    /**
     * Send ping to all connected peers.
     * Used to measure latency and verify connections.
     */
    void PingAll();
    
    /**
     * Send ping to a specific peer.
     * @param peerId Peer identifier
     * @return true if ping was sent
     */
    bool PingPeer(Peer::Id peerId);
    
    // ========================================================================
    // Network Activity Control
    // ========================================================================
    
    /**
     * Enable or disable network activity.
     * When disabled, no new connections will be made.
     * @param active New state
     */
    void SetNetworkActive(bool active);
    
    /**
     * Check if network is active.
     * @return true if network activity is enabled
     */
    bool IsNetworkActive() const { return networkActive_.load(); }
    
    // ========================================================================
    // Peer Information
    // ========================================================================
    
    /**
     * Get number of connected peers.
     * @return Total connection count
     */
    size_t GetConnectionCount() const;
    
    /**
     * Get number of inbound connections.
     * @return Inbound connection count
     */
    size_t GetInboundCount() const;
    
    /**
     * Get number of outbound connections.
     * @return Outbound connection count
     */
    size_t GetOutboundCount() const;
    
    /**
     * Get all connected peers.
     * @return Vector of peer pointers
     */
    std::vector<std::shared_ptr<Peer>> GetPeers() const;
    
    /**
     * Get peer by ID.
     * @param peerId Peer identifier
     * @return Peer pointer or nullptr
     */
    std::shared_ptr<Peer> GetPeer(Peer::Id peerId) const;
    
    /**
     * Find peer by address.
     * @param address Node address
     * @return Peer pointer or nullptr
     */
    std::shared_ptr<Peer> FindPeerByAddress(const std::string& address) const;
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get total bytes sent across all connections
    uint64_t GetTotalBytesSent() const;
    
    /// Get total bytes received across all connections
    uint64_t GetTotalBytesReceived() const;
    
private:
    /// Parse address string to NetService
    std::optional<NetService> ParseAddress(const std::string& address) const;
    
    /// Check if address is in manual node list
    bool IsManualNode(const std::string& address) const;
    
    // External references (not owned)
    ConnectionManager* connman_{nullptr};
    AddressManager* addrman_{nullptr};
    
    // Network state
    std::atomic<bool> networkActive_{true};
    
    // Manual node list
    mutable std::mutex manualNodesMutex_;
    std::set<std::string> manualNodes_;
    
    // Default port for address parsing
    uint16_t defaultPort_{8333};
};

} // namespace network
} // namespace shurium

#endif // SHURIUM_NETWORK_NETWORK_MANAGER_H
