// SHURIUM - Block Synchronization Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/network/sync.h>
#include <shurium/network/peer.h>
#include <shurium/core/block.h>

#include <algorithm>
#include <random>

namespace shurium {

// ============================================================================
// Utility Functions
// ============================================================================

const char* SyncStateToString(SyncState state) {
    switch (state) {
        case SyncState::NOT_SYNCING: return "not_syncing";
        case SyncState::HEADERS_SYNC: return "headers_sync";
        case SyncState::BLOCKS_DOWNLOAD: return "blocks_download";
        case SyncState::BLOCKS_VERIFY: return "blocks_verify";
        case SyncState::NEARLY_SYNCED: return "nearly_synced";
        case SyncState::SYNCED: return "synced";
        default: return "unknown";
    }
}

// ============================================================================
// BlockSynchronizer Implementation
// ============================================================================

BlockSynchronizer::BlockSynchronizer(size_t maxBlocksInFlight,
                                     size_t maxBlocksPerPeer,
                                     int blockTimeoutSec)
    : maxBlocksInFlight_(maxBlocksInFlight)
    , maxBlocksPerPeer_(maxBlocksPerPeer)
    , blockTimeoutSec_(blockTimeoutSec) {
}

BlockSynchronizer::~BlockSynchronizer() {
    Stop();
}

void BlockSynchronizer::Start() {
    if (running_.exchange(true)) return;
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.startTime = std::chrono::steady_clock::now();
    }
    
    SetState(SyncState::HEADERS_SYNC);
}

void BlockSynchronizer::Stop() {
    if (!running_.exchange(false)) return;
    SetState(SyncState::NOT_SYNCING);
}

void BlockSynchronizer::SetState(SyncState newState) {
    SyncState oldState = state_.exchange(newState);
    if (oldState != newState) {
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.state = newState;
        }
        
        if (stateCallback_) {
            stateCallback_(oldState, newState);
        }
    }
}

void BlockSynchronizer::OnPeerConnected(Peer::Id id, int32_t height, ServiceFlags services) {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    PeerSyncState peerState;
    peerState.chainHeight = height;
    peerState.supportsHeaders = HasFlag(services, ServiceFlags::NETWORK);
    peerStates_[id] = peerState;
    
    // Update network height if this peer is higher
    if (height > networkHeight_.load()) {
        networkHeight_ = height;
    }
}

void BlockSynchronizer::OnPeerDisconnected(Peer::Id id) {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    // Get blocks that were being downloaded from this peer
    auto it = peerStates_.find(id);
    if (it != peerStates_.end()) {
        const auto& peerState = it->second;
        
        // Re-queue blocks that were in flight from this peer
        {
            std::lock_guard<std::mutex> reqLock(requestsMutex_);
            for (const auto& hash : peerState.requestedBlocks) {
                auto reqIt = pendingRequests_.find(hash);
                if (reqIt != pendingRequests_.end() && reqIt->second.peerId == id) {
                    // Re-add to download queue
                    downloadQueue_.push_back(hash);
                    pendingRequests_.erase(reqIt);
                }
            }
        }
        
        peerStates_.erase(it);
    }
}

void BlockSynchronizer::UpdatePeerBest(Peer::Id id, const Hash256& hash, int32_t height) {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    auto it = peerStates_.find(id);
    if (it != peerStates_.end()) {
        it->second.bestKnownBlock = hash;
        it->second.chainHeight = height;
    }
    
    if (height > networkHeight_.load()) {
        networkHeight_ = height;
    }
}

const PeerSyncState* BlockSynchronizer::GetPeerState(Peer::Id id) const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    auto it = peerStates_.find(id);
    return (it != peerStates_.end()) ? &it->second : nullptr;
}

bool BlockSynchronizer::ProcessHeaders(Peer::Id fromPeer, const std::vector<BlockHeader>& headers) {
    if (headers.empty()) return true;
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.headersReceived += headers.size();
    }
    
    // Update peer state
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peerStates_.find(fromPeer);
        if (it != peerStates_.end()) {
            it->second.lastHeaderRequest = std::chrono::steady_clock::time_point();
            it->second.isStalling = false;
        }
    }
    
    // Notify callback
    if (headerCallback_) {
        return headerCallback_(headers, fromPeer);
    }
    
    return true;
}

bool BlockSynchronizer::ProcessBlock(Peer::Id fromPeer, const Block& block) {
    // For now, we just track statistics - actual block processing
    // would be handled by the consensus layer through the callback
    
    // Compute block hash to track completion
    Hash256 blockHash = block.GetHash();
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.blocksDownloaded++;
    }
    
    // Update peer state
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peerStates_.find(fromPeer);
        if (it != peerStates_.end()) {
            it->second.blocksInFlight--;
            it->second.isStalling = false;
        }
    }
    
    // Remove from pending requests and mark as downloaded
    {
        std::lock_guard<std::mutex> lock(requestsMutex_);
        pendingRequests_.erase(blockHash);
        downloadedBlocks_.insert(blockHash);
    }
    
    // Notify callback
    if (blockCallback_) {
        return blockCallback_(block, fromPeer);
    }
    
    return true;
}

void BlockSynchronizer::ProcessInv(Peer::Id fromPeer, const std::vector<Inv>& inv) {
    std::vector<Hash256> blocksToRequest;
    
    for (const auto& item : inv) {
        if (item.IsBlock()) {
            // Check if we need this block
            if (NeedBlock(item.hash)) {
                blocksToRequest.push_back(item.hash);
            }
        }
    }
    
    if (!blocksToRequest.empty()) {
        // Queue blocks for download
        std::lock_guard<std::mutex> lock(requestsMutex_);
        for (const auto& hash : blocksToRequest) {
            if (pendingRequests_.find(hash) == pendingRequests_.end() &&
                downloadedBlocks_.find(hash) == downloadedBlocks_.end()) {
                downloadQueue_.push_back(hash);
            }
        }
    }
    
    // Update peer state with announced blocks
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peerStates_.find(fromPeer);
        if (it != peerStates_.end()) {
            for (const auto& item : inv) {
                if (item.IsBlock()) {
                    it->second.bestKnownBlock = item.hash;
                }
            }
        }
    }
}

void BlockSynchronizer::ProcessNotFound(Peer::Id fromPeer, const std::vector<Inv>& inv) {
    std::lock_guard<std::mutex> lock(requestsMutex_);
    
    for (const auto& item : inv) {
        auto it = pendingRequests_.find(item.hash);
        if (it != pendingRequests_.end() && it->second.peerId == fromPeer) {
            // Re-queue for different peer
            downloadQueue_.push_front(item.hash);
            pendingRequests_.erase(it);
        }
    }
    
    // Update peer state
    {
        std::lock_guard<std::mutex> peerLock(peersMutex_);
        auto it = peerStates_.find(fromPeer);
        if (it != peerStates_.end()) {
            it->second.blocksInFlight -= inv.size();
            if (it->second.blocksInFlight < 0) {
                it->second.blocksInFlight = 0;
            }
        }
    }
}

void BlockSynchronizer::RequestHeaders(Peer::Id peerId, const BlockLocator& locator,
                                        const Hash256& stopHash) {
    if (!requestCallback_) return;
    
    GetHeadersMessage msg;
    msg.locator = locator;
    msg.hashStop = stopHash;
    
    DataStream stream;
    msg.Serialize(stream);
    std::vector<uint8_t> payload(stream.begin(), stream.end());
    
    requestCallback_(peerId, NetMsgType::GETHEADERS, payload);
    
    // Update peer state
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peerStates_.find(peerId);
        if (it != peerStates_.end()) {
            it->second.lastHeaderRequest = std::chrono::steady_clock::now();
        }
    }
}

void BlockSynchronizer::RequestBlocks(Peer::Id peerId, const std::vector<Hash256>& hashes) {
    if (!requestCallback_ || hashes.empty()) return;
    
    // Build getdata message
    std::vector<Inv> invs;
    invs.reserve(hashes.size());
    for (const auto& hash : hashes) {
        invs.emplace_back(InvType::MSG_BLOCK, hash);
    }
    
    DataStream stream;
    ::shurium::Serialize(stream, invs);
    std::vector<uint8_t> payload(stream.begin(), stream.end());
    
    requestCallback_(peerId, NetMsgType::GETDATA, payload);
    
    // Record requests
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(requestsMutex_);
        for (const auto& hash : hashes) {
            BlockRequest req;
            req.hash = hash;
            req.peerId = peerId;
            req.requestTime = now;
            pendingRequests_[hash] = req;
        }
    }
    
    // Update peer state
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peerStates_.find(peerId);
        if (it != peerStates_.end()) {
            it->second.blocksInFlight += hashes.size();
            it->second.lastBlockRequest = now;
            for (const auto& hash : hashes) {
                it->second.requestedBlocks.insert(hash);
            }
        }
    }
}

void BlockSynchronizer::Tick() {
    if (!running_) return;
    
    // Check for stalled peers
    CheckStalls();
    
    // Schedule new downloads
    ScheduleBlockDownloads();
    
    // Update progress statistics
    UpdateProgress();
    
    // Cleanup old requests
    CleanupRequests();
}

void BlockSynchronizer::ResyncFrom(int32_t height) {
    // Clear state and restart sync from given height
    {
        std::lock_guard<std::mutex> lock(requestsMutex_);
        pendingRequests_.clear();
        downloadQueue_.clear();
        downloadedBlocks_.clear();
        verifiedBlocks_.clear();
    }
    
    chainHeight_ = height;
    SetState(SyncState::HEADERS_SYNC);
}

SyncStats BlockSynchronizer::GetStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

size_t BlockSynchronizer::GetBlocksInFlight() const {
    std::lock_guard<std::mutex> lock(requestsMutex_);
    return pendingRequests_.size();
}

std::vector<Peer::Id> BlockSynchronizer::GetSyncPeers() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    std::vector<Peer::Id> result;
    for (const auto& [id, state] : peerStates_) {
        if (state.blocksInFlight > 0) {
            result.push_back(id);
        }
    }
    return result;
}

void BlockSynchronizer::SetChainHeight(int32_t height) {
    chainHeight_ = height;
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.chainHeight = height;
    }
    
    // Check if we're synced
    int32_t network = networkHeight_.load();
    if (height >= network - 1) {
        SetState(SyncState::SYNCED);
    } else if (height >= network - 10) {
        SetState(SyncState::NEARLY_SYNCED);
    }
}

void BlockSynchronizer::SetBestHeaderHeight(int32_t height) {
    bestHeaderHeight_ = height;
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.bestHeaderHeight = height;
    }
    
    // Transition to block download phase if we have enough headers
    if (state_ == SyncState::HEADERS_SYNC && height > chainHeight_.load()) {
        SetState(SyncState::BLOCKS_DOWNLOAD);
    }
}

void BlockSynchronizer::MarkBlockVerified(const Hash256& hash) {
    std::lock_guard<std::mutex> lock(requestsMutex_);
    verifiedBlocks_.insert(hash);
    
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.blocksVerified++;
    }
}

bool BlockSynchronizer::NeedBlock(const Hash256& hash) const {
    std::lock_guard<std::mutex> lock(requestsMutex_);
    return downloadedBlocks_.find(hash) == downloadedBlocks_.end() &&
           verifiedBlocks_.find(hash) == verifiedBlocks_.end();
}

Peer::Id BlockSynchronizer::SelectHeaderPeer() {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    Peer::Id bestPeer = -1;
    int32_t bestHeight = chainHeight_.load();
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& [id, state] : peerStates_) {
        // Skip peers that we recently requested from
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - state.lastHeaderRequest).count();
        if (elapsed < 2) continue;
        
        // Skip stalling peers
        if (state.isStalling) continue;
        
        // Prefer peers with higher chain
        if (state.chainHeight > bestHeight && state.supportsHeaders) {
            bestPeer = id;
            bestHeight = state.chainHeight;
        }
    }
    
    return bestPeer;
}

std::vector<Peer::Id> BlockSynchronizer::SelectBlockPeers() {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    std::vector<Peer::Id> result;
    for (const auto& [id, state] : peerStates_) {
        if (state.isStalling) continue;
        if (state.blocksInFlight >= maxBlocksPerPeer_) continue;
        result.push_back(id);
    }
    
    // Shuffle to distribute load
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(result.begin(), result.end(), g);
    
    return result;
}

void BlockSynchronizer::ScheduleBlockDownloads() {
    std::vector<Peer::Id> peers = SelectBlockPeers();
    if (peers.empty()) return;
    
    std::lock_guard<std::mutex> lock(requestsMutex_);
    
    for (Peer::Id peerId : peers) {
        if (downloadQueue_.empty()) break;
        if (pendingRequests_.size() >= maxBlocksInFlight_) break;
        
        // Get peer state to check capacity
        size_t peerCapacity = maxBlocksPerPeer_;
        {
            std::lock_guard<std::mutex> peerLock(peersMutex_);
            auto it = peerStates_.find(peerId);
            if (it != peerStates_.end()) {
                peerCapacity = maxBlocksPerPeer_ - it->second.blocksInFlight;
            }
        }
        
        // Collect blocks to request from this peer
        std::vector<Hash256> toRequest;
        while (!downloadQueue_.empty() && 
               toRequest.size() < peerCapacity &&
               pendingRequests_.size() + toRequest.size() < maxBlocksInFlight_) {
            Hash256 hash = downloadQueue_.front();
            downloadQueue_.pop_front();
            
            // Skip if already downloaded or being downloaded
            if (downloadedBlocks_.find(hash) != downloadedBlocks_.end()) continue;
            if (pendingRequests_.find(hash) != pendingRequests_.end()) continue;
            
            toRequest.push_back(hash);
        }
        
        if (!toRequest.empty()) {
            RequestBlocks(peerId, toRequest);
        }
    }
}

void BlockSynchronizer::CheckStalls() {
    auto now = std::chrono::steady_clock::now();
    int stallTimeout = blockTimeoutSec_;
    
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    for (auto& [id, state] : peerStates_) {
        if (state.blocksInFlight == 0) continue;
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - state.lastBlockRequest).count();
        
        if (elapsed > stallTimeout) {
            state.isStalling = true;
            state.stallSince = now;
            
            // Re-queue blocks from stalling peer
            std::lock_guard<std::mutex> reqLock(requestsMutex_);
            for (const auto& hash : state.requestedBlocks) {
                auto it = pendingRequests_.find(hash);
                if (it != pendingRequests_.end() && it->second.peerId == id) {
                    downloadQueue_.push_front(hash);
                    pendingRequests_.erase(it);
                }
            }
            state.requestedBlocks.clear();
            state.blocksInFlight = 0;
        }
    }
}

void BlockSynchronizer::HandleTimeout(Peer::Id peerId, const Hash256& hash) {
    {
        std::lock_guard<std::mutex> lock(requestsMutex_);
        auto it = pendingRequests_.find(hash);
        if (it != pendingRequests_.end() && it->second.peerId == peerId) {
            downloadQueue_.push_front(hash);
            pendingRequests_.erase(it);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peerStates_.find(peerId);
        if (it != peerStates_.end()) {
            it->second.blocksInFlight--;
            it->second.requestedBlocks.erase(hash);
        }
    }
}

void BlockSynchronizer::UpdateProgress() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    stats_.chainHeight = chainHeight_.load();
    stats_.bestHeaderHeight = bestHeaderHeight_.load();
    stats_.networkHeight = networkHeight_.load();
    
    // Count downloading peers
    {
        std::lock_guard<std::mutex> peerLock(peersMutex_);
        stats_.downloadingPeers = 0;
        for (const auto& [id, state] : peerStates_) {
            if (state.blocksInFlight > 0) {
                stats_.downloadingPeers++;
            }
        }
    }
    
    // Calculate download rate
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - stats_.startTime).count();
    if (elapsed > 0) {
        stats_.downloadRate = static_cast<double>(stats_.blocksDownloaded) / elapsed;
        stats_.verifyRate = static_cast<double>(stats_.blocksVerified) / elapsed;
    }
    
    // Estimate time remaining
    int32_t blocksRemaining = stats_.networkHeight - stats_.chainHeight;
    if (stats_.downloadRate > 0 && blocksRemaining > 0) {
        stats_.estimatedTimeRemaining = static_cast<int64_t>(blocksRemaining / stats_.downloadRate);
    }
}

void BlockSynchronizer::CleanupRequests() {
    auto now = std::chrono::steady_clock::now();
    int timeout = blockTimeoutSec_;
    
    std::lock_guard<std::mutex> lock(requestsMutex_);
    
    for (auto it = pendingRequests_.begin(); it != pendingRequests_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.requestTime).count();
        
        if (elapsed > timeout) {
            downloadQueue_.push_back(it->first);
            it = pendingRequests_.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// HeaderSync Implementation
// ============================================================================

HeaderSync::HeaderSync() = default;
HeaderSync::~HeaderSync() = default;

size_t HeaderSync::AddHeaders(const std::vector<BlockHeader>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t added = 0;
    for (const auto& header : headers) {
        // Compute header hash
        Hash256 hash = header.GetHash();
        
        // Skip if we already have this header
        if (headers_.find(hash) != headers_.end()) {
            continue;
        }
        
        // Validate that this header connects to our chain
        // (either to the tip or is the genesis header)
        if (!headerChain_.empty()) {
            // Header must connect to our tip
            if (header.hashPrevBlock != headerChain_.back()) {
                // Could be a fork or out-of-order delivery
                // For now, skip non-connecting headers
                continue;
            }
        }
        
        // Store the header
        headers_[hash] = std::make_unique<BlockHeader>(header);
        headerChain_.push_back(hash);
        added++;
    }
    
    return added;
}

BlockLocator HeaderSync::GetLocator() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    BlockLocator locator;
    
    // Build locator from header chain
    // Exponentially decreasing step size: 1, 2, 4, 8, 16, ...
    size_t step = 1;
    for (size_t i = headerChain_.size(); i > 0; ) {
        i = (i > step) ? i - step : 0;
        locator.vHave.push_back(BlockHash(headerChain_[i]));
        if (locator.vHave.size() >= 10) {
            step *= 2;
        }
    }
    
    return locator;
}

std::vector<Hash256> HeaderSync::GetHeadersToDownload(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Hash256> result;
    result.reserve(maxCount);
    
    for (const auto& hash : headerChain_) {
        if (downloadedHeaders_.find(hash) == downloadedHeaders_.end()) {
            result.push_back(hash);
            if (result.size() >= maxCount) break;
        }
    }
    
    return result;
}

Hash256 HeaderSync::GetBestHeader() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return headerChain_.empty() ? Hash256() : headerChain_.back();
}

int32_t HeaderSync::GetBestHeight() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int32_t>(headerChain_.size());
}

bool HeaderSync::HasHeader(const Hash256& hash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return headers_.find(hash) != headers_.end();
}

const BlockHeader* HeaderSync::GetHeader(const Hash256& hash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = headers_.find(hash);
    return (it != headers_.end()) ? it->second.get() : nullptr;
}

void HeaderSync::MarkDownloaded(const Hash256& hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    downloadedHeaders_.insert(hash);
}

void HeaderSync::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    headers_.clear();
    headerChain_.clear();
    downloadedHeaders_.clear();
}

// ============================================================================
// InvRelay Implementation
// ============================================================================

InvRelay::InvRelay() = default;
InvRelay::~InvRelay() = default;

void InvRelay::QueueAnnouncement(Peer::Id peerId, const Inv& inv) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Don't announce if peer already has it
    auto& peerInv = peerInventory_[peerId];
    if (peerInv.find(inv) != peerInv.end()) {
        return;
    }
    
    announceQueue_[peerId].push_back(inv);
}

std::vector<Inv> InvRelay::GetAnnouncements(Peer::Id peerId, size_t maxCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Inv> result;
    auto it = announceQueue_.find(peerId);
    if (it == announceQueue_.end()) return result;
    
    auto& queue = it->second;
    while (!queue.empty() && result.size() < maxCount) {
        result.push_back(queue.front());
        peerInventory_[peerId].insert(queue.front());
        queue.pop_front();
    }
    
    return result;
}

void InvRelay::RecordReceived(Peer::Id peerId, const Inv& inv) {
    std::lock_guard<std::mutex> lock(mutex_);
    peerInventory_[peerId].insert(inv);
}

bool InvRelay::PeerHasInv(Peer::Id peerId, const Inv& inv) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peerInventory_.find(peerId);
    if (it == peerInventory_.end()) return false;
    return it->second.find(inv) != it->second.end();
}

void InvRelay::OnPeerDisconnected(Peer::Id peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    peerInventory_.erase(peerId);
    announceQueue_.erase(peerId);
}

void InvRelay::Cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Would implement LRU cleanup here to limit memory usage
    recentlyAnnounced_.clear();
}

} // namespace shurium
