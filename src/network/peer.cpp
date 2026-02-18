// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#include <shurium/network/peer.h>
#include <shurium/core/random.h>

#include <algorithm>
#include <cstring>

namespace shurium {

// ============================================================================
// Factory Methods
// ============================================================================

std::unique_ptr<Peer> Peer::CreateOutbound(Id id, const NetService& addr,
                                            ConnectionType connType) {
    auto peer = std::unique_ptr<Peer>(new Peer(id, addr, connType));
    peer->stats_.fInbound = false;
    return peer;
}

std::unique_ptr<Peer> Peer::CreateInbound(Id id, const NetService& addr) {
    auto peer = std::unique_ptr<Peer>(new Peer(id, addr, ConnectionType::INBOUND));
    peer->stats_.fInbound = true;
    return peer;
}

Peer::Peer(Id id, const NetService& addr, ConnectionType type)
    : id_(id), address_(addr), connType_(type) {
    stats_.connectedTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

Peer::~Peer() = default;

// ============================================================================
// State Management
// ============================================================================

void Peer::SetState(PeerState newState) {
    PeerState oldState = state_.exchange(newState);
    if (stateHandler_ && oldState != newState) {
        stateHandler_(*this, oldState, newState);
    }
}

void Peer::Disconnect(DisconnectReason reason) {
    disconnectReason_ = reason;
    fDisconnect_.store(true);
}

// ============================================================================
// Handshake
// ============================================================================

VersionMessage Peer::CreateVersionMessage(const NetService& ourAddr,
                                           int32_t ourHeight,
                                           ServiceFlags ourServices) const {
    VersionMessage ver;
    ver.version = PROTOCOL_VERSION;
    ver.services = ourServices;
    ver.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    ver.addrRecv = address_;
    ver.addrFrom = ourAddr;
    ver.nonce = GetRandUint64();
    ver.userAgent = "/SHURIUM:0.1.0/";
    ver.startHeight = ourHeight;
    ver.relay = true;
    return ver;
}

bool Peer::ProcessVersion(const VersionMessage& version) {
    if (fReceivedVersion_) {
        // Already received version
        Misbehaving(1, "Duplicate version message");
        return false;
    }
    
    // Check minimum protocol version
    if (version.version < MIN_PEER_PROTO_VERSION) {
        Disconnect(DisconnectReason::BAD_VERSION);
        return false;
    }
    
    // Store peer info
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.nVersion = version.version;
        stats_.services = version.services;
        stats_.userAgent = version.userAgent;
        if (stats_.userAgent.size() > MAX_SUBVERSION_LENGTH) {
            stats_.userAgent.resize(MAX_SUBVERSION_LENGTH);
        }
        stats_.startingHeight = version.startHeight;
        stats_.fRelayTxes = version.relay;
    }
    
    fReceivedVersion_ = true;
    
    // Update state
    if (fSentVersion_) {
        SetState(PeerState::VERSION_RECEIVED);
    }
    
    return true;
}

bool Peer::ProcessVerack() {
    if (!fReceivedVersion_) {
        Misbehaving(1, "Verack before version");
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.fSuccessfullyConnected = true;
    }
    
    SetState(PeerState::ESTABLISHED);
    return true;
}

void Peer::SetVersionSent() {
    fSentVersion_ = true;
    if (state_ == PeerState::CONNECTED) {
        SetState(PeerState::VERSION_SENT);
    }
}

// ============================================================================
// Ping/Pong
// ============================================================================

uint64_t Peer::SendPing() {
    pingNonce_ = GetRandUint64();
    pingStart_ = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.lastPingTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    PingMessage ping(pingNonce_);
    QueueMessage(NetMsgType::PING, ping);
    
    return pingNonce_;
}

bool Peer::ProcessPong(uint64_t nonce) {
    if (nonce != pingNonce_) {
        // Nonce doesn't match - could be stale
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(now - pingStart_);
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.pingLatencyMicros = latency.count();
        stats_.lastPongTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    pingNonce_ = 0;
    return true;
}

int64_t Peer::GetPingLatency() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    if (stats_.pingLatencyMicros == 0) {
        return -1;
    }
    return stats_.pingLatencyMicros / 1000;  // Convert to ms
}

// ============================================================================
// Statistics
// ============================================================================

PeerStats Peer::GetStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void Peer::RecordBytesSent(size_t bytes) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.bytesSent += bytes;
    stats_.lastSendTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void Peer::RecordBytesReceived(size_t bytes) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.bytesRecv += bytes;
    stats_.lastRecvTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void Peer::RecordMessageSent() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.messagesSent++;
}

void Peer::RecordMessageReceived() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.messagesRecv++;
}

// ============================================================================
// Inventory Tracking
// ============================================================================

bool Peer::HasAnnounced(const Inv& inv) const {
    std::lock_guard<std::mutex> lock(invMutex_);
    return announcedByUs_.count(inv) > 0;
}

void Peer::MarkAnnounced(const Inv& inv) {
    std::lock_guard<std::mutex> lock(invMutex_);
    announcedByUs_.insert(inv);
}

bool Peer::HasInventory(const Inv& inv) const {
    std::lock_guard<std::mutex> lock(invMutex_);
    return announcedToUs_.count(inv) > 0;
}

void Peer::AddInventory(const Inv& inv) {
    std::lock_guard<std::mutex> lock(invMutex_);
    announcedToUs_.insert(inv);
}

size_t Peer::GetAnnouncementQueueSize() const {
    std::lock_guard<std::mutex> lock(invMutex_);
    return announcementQueue_.size();
}

void Peer::QueueAnnouncement(const Inv& inv) {
    std::lock_guard<std::mutex> lock(invMutex_);
    // Don't queue if already announced
    if (announcedByUs_.count(inv) == 0) {
        announcementQueue_.push_back(inv);
    }
}

std::vector<Inv> Peer::GetAnnouncementsToSend(size_t maxCount) {
    std::lock_guard<std::mutex> lock(invMutex_);
    std::vector<Inv> result;
    
    while (!announcementQueue_.empty() && result.size() < maxCount) {
        Inv inv = announcementQueue_.front();
        announcementQueue_.pop_front();
        
        if (announcedByUs_.count(inv) == 0) {
            result.push_back(inv);
            announcedByUs_.insert(inv);
        }
    }
    
    return result;
}

// ============================================================================
// Misbehavior Tracking
// ============================================================================

bool Peer::Misbehaving(int howMuch, const std::string& /* reason */) {
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.misbehaviorScore += howMuch;
    }
    
    if (stats_.misbehaviorScore >= BAN_THRESHOLD) {
        Disconnect(DisconnectReason::MISBEHAVIOR);
        return true;
    }
    return false;
}

// ============================================================================
// Send/Receive Buffers
// ============================================================================

void Peer::QueueSend(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    sendBuffer_.insert(sendBuffer_.end(), data.begin(), data.end());
}

void Peer::QueueMessage(const std::string& command) {
    std::vector<uint8_t> emptyPayload;
    auto msg = CreateMessage(networkMagic_, command, emptyPayload);
    QueueSend(msg);
}

std::vector<uint8_t> Peer::GetSendData(size_t maxBytes) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    size_t toSend = std::min(maxBytes, sendBuffer_.size());
    std::vector<uint8_t> data(sendBuffer_.begin(), sendBuffer_.begin() + toSend);
    sendBuffer_.erase(sendBuffer_.begin(), sendBuffer_.begin() + toSend);
    return data;
}

bool Peer::HasDataToSend() const {
    std::lock_guard<std::mutex> lock(sendMutex_);
    return !sendBuffer_.empty();
}

void Peer::AddReceivedData(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(recvMutex_);
    recvBuffer_.insert(recvBuffer_.end(), data.begin(), data.end());
}

std::optional<std::pair<std::string, std::vector<uint8_t>>> Peer::GetNextMessage() {
    std::lock_guard<std::mutex> lock(recvMutex_);
    
    // Need at least a header
    if (recvBuffer_.size() < MESSAGE_HEADER_SIZE) {
        return std::nullopt;
    }
    
    // Parse header
    std::vector<uint8_t> headerBytes(recvBuffer_.begin(), 
                                      recvBuffer_.begin() + MESSAGE_HEADER_SIZE);
    auto headerOpt = ParseMessageHeader(headerBytes);
    if (!headerOpt) {
        // Invalid header - clear buffer and disconnect
        recvBuffer_.clear();
        Disconnect(DisconnectReason::PROTOCOL_ERROR);
        return std::nullopt;
    }
    
    const auto& header = *headerOpt;
    
    // Check magic
    if (!header.IsValidMagic(networkMagic_)) {
        recvBuffer_.clear();
        Disconnect(DisconnectReason::PROTOCOL_ERROR);
        return std::nullopt;
    }
    
    // Wait for full payload
    size_t totalSize = MESSAGE_HEADER_SIZE + header.payloadSize;
    if (recvBuffer_.size() < totalSize) {
        return std::nullopt;
    }
    
    // Extract payload
    std::vector<uint8_t> payload(recvBuffer_.begin() + MESSAGE_HEADER_SIZE,
                                  recvBuffer_.begin() + totalSize);
    
    // Verify checksum
    if (!VerifyChecksum(payload, header.checksum)) {
        recvBuffer_.clear();
        Disconnect(DisconnectReason::PROTOCOL_ERROR);
        return std::nullopt;
    }
    
    // Remove message from buffer
    recvBuffer_.erase(recvBuffer_.begin(), recvBuffer_.begin() + totalSize);
    
    return std::make_pair(header.GetCommand(), std::move(payload));
}

} // namespace shurium
