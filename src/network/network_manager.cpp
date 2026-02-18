// SHURIUM - Network Manager Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/network/network_manager.h>

#include <sstream>

namespace shurium {
namespace network {

// ============================================================================
// Constructor / Destructor
// ============================================================================

NetworkManager::NetworkManager() = default;
NetworkManager::~NetworkManager() = default;

// ============================================================================
// Initialization
// ============================================================================

void NetworkManager::SetConnectionManager(ConnectionManager* connman) {
    connman_ = connman;
}

void NetworkManager::SetAddressManager(AddressManager* addrman) {
    addrman_ = addrman;
}

// ============================================================================
// Manual Node Management
// ============================================================================

bool NetworkManager::AddNode(const std::string& address) {
    std::lock_guard<std::mutex> lock(manualNodesMutex_);
    
    // Check if already in list
    if (manualNodes_.count(address) > 0) {
        return false; // Already exists
    }
    
    manualNodes_.insert(address);
    
    // Try to connect if we have a connection manager
    if (connman_ && networkActive_.load()) {
        auto addr = ParseAddress(address);
        if (addr) {
            connman_->ConnectTo(*addr, ConnectionType::MANUAL);
        }
    }
    
    return true;
}

bool NetworkManager::RemoveNode(const std::string& address) {
    std::lock_guard<std::mutex> lock(manualNodesMutex_);
    
    auto it = manualNodes_.find(address);
    if (it == manualNodes_.end()) {
        return false; // Not found
    }
    
    manualNodes_.erase(it);
    
    // Disconnect if currently connected
    DisconnectNode(address);
    
    return true;
}

bool NetworkManager::TryConnectNode(const std::string& address) {
    if (!connman_ || !networkActive_.load()) {
        return false;
    }
    
    auto addr = ParseAddress(address);
    if (!addr) {
        return false;
    }
    
    // Try a one-time connection
    Peer::Id peerId = connman_->ConnectTo(*addr, ConnectionType::MANUAL);
    return peerId != -1;
}

std::vector<AddedNodeInfo> NetworkManager::GetAddedNodeInfo() const {
    std::lock_guard<std::mutex> lock(manualNodesMutex_);
    
    std::vector<AddedNodeInfo> result;
    result.reserve(manualNodes_.size());
    
    for (const auto& node : manualNodes_) {
        AddedNodeInfo info;
        info.address = node;
        info.connected = false;
        
        // Check if we're connected to this node
        if (connman_) {
            auto peer = FindPeerByAddress(node);
            if (peer && peer->IsEstablished()) {
                info.connected = true;
                info.addresses.emplace_back(peer->GetAddress().ToString(), true);
            }
        }
        
        result.push_back(std::move(info));
    }
    
    return result;
}

std::optional<AddedNodeInfo> NetworkManager::GetAddedNodeInfo(const std::string& address) const {
    std::lock_guard<std::mutex> lock(manualNodesMutex_);
    
    if (manualNodes_.count(address) == 0) {
        return std::nullopt;
    }
    
    AddedNodeInfo info;
    info.address = address;
    info.connected = false;
    
    if (connman_) {
        auto peer = FindPeerByAddress(address);
        if (peer && peer->IsEstablished()) {
            info.connected = true;
            info.addresses.emplace_back(peer->GetAddress().ToString(), true);
        }
    }
    
    return info;
}

// ============================================================================
// Peer Disconnection
// ============================================================================

bool NetworkManager::DisconnectNode(const std::string& address) {
    if (!connman_) {
        return false;
    }
    
    auto peer = FindPeerByAddress(address);
    if (!peer) {
        return false;
    }
    
    connman_->DisconnectPeer(peer->GetId(), DisconnectReason::MANUALLY_REQUESTED);
    return true;
}

bool NetworkManager::DisconnectNode(Peer::Id peerId) {
    if (!connman_) {
        return false;
    }
    
    auto peer = connman_->GetPeer(peerId);
    if (!peer) {
        return false;
    }
    
    connman_->DisconnectPeer(peerId, DisconnectReason::MANUALLY_REQUESTED);
    return true;
}

// ============================================================================
// Ping Management
// ============================================================================

void NetworkManager::PingAll() {
    if (!connman_) {
        return;
    }
    
    auto peers = connman_->GetAllPeers();
    for (auto& peer : peers) {
        if (peer->IsEstablished()) {
            peer->SendPing();
        }
    }
}

bool NetworkManager::PingPeer(Peer::Id peerId) {
    if (!connman_) {
        return false;
    }
    
    auto peer = connman_->GetPeer(peerId);
    if (!peer || !peer->IsEstablished()) {
        return false;
    }
    
    peer->SendPing();
    return true;
}

// ============================================================================
// Network Activity Control
// ============================================================================

void NetworkManager::SetNetworkActive(bool active) {
    bool wasActive = networkActive_.exchange(active);
    
    // If transitioning to inactive, disconnect all peers
    if (wasActive && !active && connman_) {
        connman_->DisconnectAll(DisconnectReason::MANUALLY_REQUESTED);
    }
}

// ============================================================================
// Peer Information
// ============================================================================

size_t NetworkManager::GetConnectionCount() const {
    if (!connman_) {
        return 0;
    }
    return connman_->GetPeerCount();
}

size_t NetworkManager::GetInboundCount() const {
    if (!connman_) {
        return 0;
    }
    return connman_->GetInboundCount();
}

size_t NetworkManager::GetOutboundCount() const {
    if (!connman_) {
        return 0;
    }
    return connman_->GetOutboundCount();
}

std::vector<std::shared_ptr<Peer>> NetworkManager::GetPeers() const {
    if (!connman_) {
        return {};
    }
    return connman_->GetAllPeers();
}

std::shared_ptr<Peer> NetworkManager::GetPeer(Peer::Id peerId) const {
    if (!connman_) {
        return nullptr;
    }
    return connman_->GetPeer(peerId);
}

std::shared_ptr<Peer> NetworkManager::FindPeerByAddress(const std::string& address) const {
    if (!connman_) {
        return nullptr;
    }
    
    auto targetAddr = ParseAddress(address);
    if (!targetAddr) {
        return nullptr;
    }
    
    auto peers = connman_->GetAllPeers();
    for (auto& peer : peers) {
        const NetService& peerAddr = peer->GetAddress();
        // Compare address and port
        if (peerAddr.ToString() == targetAddr->ToString()) {
            return peer;
        }
        // Also check without port for host-only matches
        if (static_cast<const NetAddress&>(peerAddr).ToString() == 
            static_cast<const NetAddress&>(*targetAddr).ToString()) {
            return peer;
        }
    }
    
    return nullptr;
}

// ============================================================================
// Statistics
// ============================================================================

uint64_t NetworkManager::GetTotalBytesSent() const {
    if (!connman_) {
        return 0;
    }
    return connman_->GetTotalBytesSent();
}

uint64_t NetworkManager::GetTotalBytesReceived() const {
    if (!connman_) {
        return 0;
    }
    return connman_->GetTotalBytesRecv();
}

// ============================================================================
// Private Methods
// ============================================================================

std::optional<NetService> NetworkManager::ParseAddress(const std::string& address) const {
    // First try parsing as a full service (host:port)
    auto service = NetService::FromString(address);
    if (service) {
        return service;
    }
    
    // Try parsing as just an address and add default port
    auto addr = NetAddress::FromString(address);
    if (addr) {
        return NetService(*addr, defaultPort_);
    }
    
    return std::nullopt;
}

bool NetworkManager::IsManualNode(const std::string& address) const {
    std::lock_guard<std::mutex> lock(manualNodesMutex_);
    return manualNodes_.count(address) > 0;
}

} // namespace network
} // namespace shurium
