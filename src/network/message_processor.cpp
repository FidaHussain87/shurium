// SHURIUM - Message Processor Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/network/message_processor.h>
#include <shurium/network/addrman.h>
#include <shurium/node/context.h>
#include <shurium/mempool/mempool.h>
#include <shurium/chain/chainstate.h>
#include <shurium/db/blockdb.h>
#include <shurium/util/logging.h>
#include <shurium/util/time.h>

#include <algorithm>
#include <cstring>

namespace shurium {

// ============================================================================
// Construction / Destruction
// ============================================================================

MessageProcessor::MessageProcessor(const Options& opts)
    : options_(opts)
{
}

MessageProcessor::~MessageProcessor() {
    Stop();
}

// ============================================================================
// Initialization
// ============================================================================

void MessageProcessor::Initialize(ConnectionManager* connman, BlockSynchronizer* sync) {
    connman_ = connman;
    sync_ = sync;
}

void MessageProcessor::SetMempool(Mempool* mempool) {
    mempool_ = mempool;
}

void MessageProcessor::SetChainManager(ChainStateManager* chainman) {
    chainman_ = chainman;
}

void MessageProcessor::SetCoinsView(CoinsView* coins) {
    coins_ = coins;
}

void MessageProcessor::SetAddressManager(AddressManager* addrman) {
    addrman_ = addrman;
}

void MessageProcessor::SetBlockDB(db::BlockDB* blockdb) {
    blockdb_ = blockdb;
}

void MessageProcessor::SetLocalAddress(const NetService& addr) {
    localAddress_ = addr;
}

void MessageProcessor::SetChainHeight(int32_t height) {
    chainHeight_.store(height);
}

void MessageProcessor::InitializeFromContext(NodeContext& ctx) {
    connman_ = ctx.connman.get();
    mempool_ = ctx.mempool.get();
    chainman_ = ctx.chainman.get();
    
    // Get chain height
    if (ctx.chainman) {
        chainHeight_.store(ctx.chainman->GetActiveHeight());
    }
    
    // Note: BlockSynchronizer needs to be created and passed separately
    // since it's not currently part of NodeContext
}

// ============================================================================
// Lifecycle
// ============================================================================

bool MessageProcessor::Start() {
    if (!connman_) {
        LOG_ERROR(util::LogCategory::NET) << "MessageProcessor: No connection manager set";
        return false;
    }
    
    if (running_.exchange(true)) {
        return true;  // Already running
    }
    
    LOG_INFO(util::LogCategory::NET) << "Starting message processor...";
    
    processingThread_ = std::thread(&MessageProcessor::ProcessingLoop, this);
    
    return true;
}

void MessageProcessor::Stop() {
    if (!running_.exchange(false)) {
        return;  // Not running
    }
    
    LOG_INFO(util::LogCategory::NET) << "Stopping message processor...";
    
    if (processingThread_.joinable()) {
        processingThread_.join();
    }
    
    LOG_INFO(util::LogCategory::NET) << "Message processor stopped";
}

// ============================================================================
// Background Thread
// ============================================================================

void MessageProcessor::ProcessingLoop() {
    LOG_DEBUG(util::LogCategory::NET) << "Message processing loop started";
    
    auto lastPingCheck = std::chrono::steady_clock::now();
    
    while (running_.load()) {
        // Process messages from all peers
        ProcessMessages();
        
        // Periodic tasks
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastPing = std::chrono::duration_cast<std::chrono::seconds>(
            now - lastPingCheck).count();
        
        if (timeSinceLastPing >= options_.pingIntervalSec / 4) {
            SendPings();
            CheckPingTimeouts();
            lastPingCheck = now;
        }
        
        // Sleep for the processing interval
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options_.processingIntervalMs));
    }
    
    LOG_DEBUG(util::LogCategory::NET) << "Message processing loop exited";
}

// ============================================================================
// Message Processing (Main Loop)
// ============================================================================

void MessageProcessor::ProcessMessages() {
    if (!connman_) return;
    
    auto peers = connman_->GetAllPeers();
    
    for (auto& peer : peers) {
        if (!peer) continue;
        ProcessPeerMessages(peer);
    }
}

void MessageProcessor::ProcessPeerMessages(std::shared_ptr<Peer> peer) {
    if (!peer || peer->ShouldDisconnect()) return;
    
    int messagesProcessed = 0;
    
    while (messagesProcessed < options_.maxMessagesPerPeer) {
        auto msgOpt = peer->GetNextMessage();
        if (!msgOpt) {
            break;  // No more messages
        }
        
        auto& [command, payload] = *msgOpt;
        
        // Dispatch the message
        bool ok = DispatchMessage(*peer, command, payload);
        
        if (!ok) {
            // Message handling failed - may have disconnected peer
            if (peer->ShouldDisconnect()) {
                break;
            }
        }
        
        peer->RecordMessageReceived();
        messagesProcessed++;
        
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesProcessed++;
        }
    }
}

// ============================================================================
// Message Dispatch
// ============================================================================

bool MessageProcessor::DispatchMessage(Peer& peer, const std::string& command,
                                        const std::vector<uint8_t>& payload) {
    // === Pre-dispatch Validation ===
    
    // Validate command name
    auto cmdValidation = ValidateCommand(command);
    if (!cmdValidation.valid) {
        LOG_WARN(util::LogCategory::NET) << "Invalid command from peer " << peer.GetId()
                                         << ": " << cmdValidation.reason;
        if (cmdValidation.misbehaviorScore > 0) {
            peer.Misbehaving(cmdValidation.misbehaviorScore, cmdValidation.reason);
        }
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.invalidMessages++;
        }
        return false;
    }
    
    // Validate payload size for this command type
    auto sizeValidation = ValidatePayloadSize(command, payload.size());
    if (!sizeValidation.valid) {
        LOG_WARN(util::LogCategory::NET) << "Invalid payload size for " << command 
                                         << " from peer " << peer.GetId()
                                         << ": " << sizeValidation.reason
                                         << " (size=" << payload.size() << ")";
        if (sizeValidation.misbehaviorScore > 0) {
            peer.Misbehaving(sizeValidation.misbehaviorScore, sizeValidation.reason);
        }
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.invalidMessages++;
        }
        return false;
    }
    
    // Create a stream from the payload for deserialization
    DataStream stream(payload);
    
    LOG_DEBUG(util::LogCategory::NET) << "Processing " << command 
                                      << " from peer " << peer.GetId();
    
    try {
        // Handshake messages (always accepted)
        if (command == NetMsgType::VERSION) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.versionMessages++;
            return HandleVersion(peer, stream);
        }
        if (command == NetMsgType::VERACK) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.verackMessages++;
            return HandleVerack(peer);
        }
        
        // All other messages require established connection
        if (!peer.IsEstablished()) {
            LOG_DEBUG(util::LogCategory::NET) << "Ignoring " << command 
                                              << " from non-established peer " << peer.GetId();
            return true;  // Don't disconnect, just ignore
        }
        
        // Keepalive
        if (command == NetMsgType::PING) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.pingMessages++;
            return HandlePing(peer, stream);
        }
        if (command == NetMsgType::PONG) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.pongMessages++;
            return HandlePong(peer, stream);
        }
        
        // Inventory / data
        if (command == NetMsgType::INV) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.invMessages++;
            return HandleInv(peer, stream);
        }
        if (command == NetMsgType::GETDATA) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.getdataMessages++;
            return HandleGetData(peer, stream);
        }
        if (command == NetMsgType::NOTFOUND) {
            return HandleNotFound(peer, stream);
        }
        
        // Blocks / headers
        if (command == NetMsgType::HEADERS) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.headersMessages++;
            return HandleHeaders(peer, stream);
        }
        if (command == NetMsgType::BLOCK) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.blockMessages++;
            return HandleBlock(peer, stream);
        }
        if (command == NetMsgType::GETHEADERS) {
            return HandleGetHeaders(peer, stream);
        }
        if (command == NetMsgType::GETBLOCKS) {
            return HandleGetBlocks(peer, stream);
        }
        
        // Transactions
        if (command == NetMsgType::TX) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.txMessages++;
            return HandleTx(peer, stream);
        }
        if (command == NetMsgType::MEMPOOL) {
            return HandleMempool(peer);
        }
        if (command == NetMsgType::FEEFILTER) {
            return HandleFeeFilter(peer, stream);
        }
        
        // Address relay
        if (command == NetMsgType::ADDR) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.addrMessages++;
            return HandleAddr(peer, stream);
        }
        if (command == NetMsgType::GETADDR) {
            // Serve addresses from our address manager
            if (addrman_) {
                // Get a random selection of addresses
                std::vector<PeerAddress> addresses = addrman_->GetAddr(MAX_ADDR_TO_SEND);
                
                if (!addresses.empty()) {
                    AddrMessage addrMsg;
                    addrMsg.addresses = std::move(addresses);
                    peer.QueueMessage(NetMsgType::ADDR, addrMsg);
                    
                    LOG_DEBUG(util::LogCategory::NET) << "Sent " << addrMsg.addresses.size() 
                                                      << " addresses to peer " << peer.GetId();
                }
            }
            return true;
        }
        
        // Feature negotiation
        if (command == NetMsgType::SENDHEADERS) {
            return HandleSendHeaders(peer);
        }
        if (command == NetMsgType::SENDADDRV2) {
            // Mark peer as supporting ADDRv2
            // peer.SetSupportsAddrV2(true);
            return true;
        }
        
        // Unknown message
        LOG_DEBUG(util::LogCategory::NET) << "Unknown message type: " << command;
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.unknownMessages++;
        }
        return true;  // Don't disconnect for unknown messages
        
    } catch (const std::exception& e) {
        LOG_ERROR(util::LogCategory::NET) << "Error processing " << command 
                                          << " from peer " << peer.GetId() 
                                          << ": " << e.what();
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.invalidMessages++;
        }
        peer.Misbehaving(10, "Message processing error");
        return false;
    }
}

// ============================================================================
// Handshake Handlers
// ============================================================================

bool MessageProcessor::HandleVersion(Peer& peer, DataStream& payload) {
    if (peer.HasReceivedVersion()) {
        LOG_DEBUG(util::LogCategory::NET) << "Duplicate version from peer " << peer.GetId();
        peer.Misbehaving(1, "Duplicate version");
        return true;
    }
    
    VersionMessage version;
    version.Unserialize(payload);
    
    // Validate version message contents
    auto validation = ValidateVersionMessage(version);
    if (!validation.valid) {
        LOG_WARN(util::LogCategory::NET) << "Invalid version from peer " << peer.GetId()
                                         << ": " << validation.reason;
        if (validation.misbehaviorScore > 0) {
            peer.Misbehaving(validation.misbehaviorScore, validation.reason);
        }
        peer.Disconnect(DisconnectReason::BAD_VERSION);
        return false;
    }
    
    // Sanitize user agent to prevent log injection and display issues
    version.userAgent = SanitizeUserAgent(version.userAgent);
    
    LOG_INFO(util::LogCategory::NET) << "Received version from peer " << peer.GetId()
                                     << ": version=" << version.version
                                     << ", services=" << static_cast<uint64_t>(version.services)
                                     << ", height=" << version.startHeight
                                     << ", agent=" << version.userAgent;
    
    // Process the version (minimum version already validated above)
    if (!peer.ProcessVersion(version)) {
        LOG_DEBUG(util::LogCategory::NET) << "Failed to process version from peer " << peer.GetId();
        return false;
    }
    
    // If we haven't sent our version yet, send it now
    if (!peer.HasSentVersion()) {
        SendVersion(peer);
    }
    
    // Send verack
    SendVerack(peer);
    
    // Notify sync manager about the new peer
    if (sync_) {
        sync_->OnPeerConnected(peer.GetId(), version.startHeight, version.services);
    }
    
    return true;
}

bool MessageProcessor::HandleVerack(Peer& peer) {
    if (!peer.HasReceivedVersion()) {
        LOG_DEBUG(util::LogCategory::NET) << "Verack before version from peer " << peer.GetId();
        peer.Misbehaving(10, "Verack before version");
        return false;
    }
    
    if (!peer.ProcessVerack()) {
        LOG_DEBUG(util::LogCategory::NET) << "Failed to process verack from peer " << peer.GetId();
        return false;
    }
    
    LOG_INFO(util::LogCategory::NET) << "Handshake complete with peer " << peer.GetId();
    
    // Notify callback
    if (handshakeCallback_) {
        handshakeCallback_(peer.GetId());
    }
    
    // Send sendheaders to request header announcements
    peer.QueueMessage(NetMsgType::SENDHEADERS);
    
    // Start sync if we need blocks
    if (sync_ && sync_->GetState() == SyncState::NOT_SYNCING) {
        // Get locator from chain state
        BlockLocator locator;
        if (chainman_) {
            BlockIndex* tip = chainman_->GetActiveTip();
            if (tip) {
                locator = GetLocator(tip);
            }
        }
        sync_->RequestHeaders(peer.GetId(), locator);
    }
    
    return true;
}

// ============================================================================
// Ping/Pong Handlers
// ============================================================================

bool MessageProcessor::HandlePing(Peer& peer, DataStream& payload) {
    PingMessage ping;
    ping.Unserialize(payload);
    
    // Send pong with same nonce
    SendPong(peer, ping.nonce);
    
    return true;
}

bool MessageProcessor::HandlePong(Peer& peer, DataStream& payload) {
    PongMessage pong;
    pong.Unserialize(payload);
    
    if (!peer.ProcessPong(pong.nonce)) {
        LOG_DEBUG(util::LogCategory::NET) << "Invalid pong from peer " << peer.GetId();
        // Don't disconnect, just ignore
    } else {
        LOG_DEBUG(util::LogCategory::NET) << "Pong from peer " << peer.GetId() 
                                          << ", latency=" << peer.GetPingLatency() << "ms";
    }
    
    return true;
}

// ============================================================================
// Inventory Handlers
// ============================================================================

bool MessageProcessor::HandleInv(Peer& peer, DataStream& payload) {
    uint64_t count;
    Unserialize(payload, count);
    
    if (count > MAX_INV_SZ) {
        LOG_DEBUG(util::LogCategory::NET) << "Peer " << peer.GetId() 
                                          << " sent too many inv items: " << count;
        peer.Misbehaving(20, "Oversized inv");
        return false;
    }
    
    std::vector<Inv> inv;
    inv.reserve(static_cast<size_t>(count));
    
    int invalidInvCount = 0;
    for (uint64_t i = 0; i < count; i++) {
        Inv item;
        item.Unserialize(payload);
        
        // Validate inventory type
        if (!IsValidInvType(item.type)) {
            invalidInvCount++;
            // Skip invalid inventory items but don't abort
            continue;
        }
        
        inv.push_back(item);
        
        // Track that peer has this inventory
        peer.AddInventory(item);
    }
    
    // Penalize if too many invalid inventory types
    if (invalidInvCount > 10) {
        LOG_WARN(util::LogCategory::NET) << "Peer " << peer.GetId() 
                                         << " sent " << invalidInvCount << " invalid inv types";
        peer.Misbehaving(10, "Multiple invalid inv types");
    }
    
    if (inv.empty()) {
        return true;
    }
    
    LOG_DEBUG(util::LogCategory::NET) << "Received " << inv.size() 
                                      << " inv items from peer " << peer.GetId();
    
    // Forward to sync manager for blocks
    if (sync_) {
        sync_->ProcessInv(peer.GetId(), inv);
    }
    
    // Request transactions we don't have
    if (options_.relayTransactions && mempool_) {
        std::vector<Inv> txToRequest;
        for (const auto& item : inv) {
            if (item.type == InvType::MSG_TX) {
                // Check if we already have this tx
                // Convert Hash256 to TxHash for mempool lookup
                TxHash txid(item.hash);
                if (!mempool_->Exists(txid)) {
                    txToRequest.push_back(item);
                }
            }
        }
        if (!txToRequest.empty()) {
            SendGetData(peer, txToRequest);
        }
    }
    
    return true;
}

bool MessageProcessor::HandleGetData(Peer& peer, DataStream& payload) {
    uint64_t count;
    Unserialize(payload, count);
    
    if (count > MAX_INV_SZ) {
        LOG_DEBUG(util::LogCategory::NET) << "Peer " << peer.GetId() 
                                          << " requested too many items: " << count;
        peer.Misbehaving(20, "Oversized getdata");
        return false;
    }
    
    std::vector<Inv> notFound;
    
    for (uint64_t i = 0; i < count; i++) {
        Inv item;
        item.Unserialize(payload);
        
        bool served = false;
        
        // Try callback first
        if (getDataCallback_) {
            served = getDataCallback_(peer.GetId(), item);
        }
        
        // If callback didn't serve, try our own data sources
        if (!served) {
            if (item.type == InvType::MSG_TX && mempool_) {
                TxHash txid(item.hash);
                auto tx = mempool_->Get(txid);
                if (tx) {
                    // Transaction uses free Serialize function, so serialize manually
                    DataStream stream;
                    Serialize(stream, *tx);
                    std::vector<uint8_t> payload(stream.begin(), stream.end());
                    auto msg = CreateMessage(NetworkMagic::MAINNET, NetMsgType::TX, payload);
                    peer.QueueSend(msg);
                    served = true;
                }
            } else if (item.type == InvType::MSG_BLOCK && chainman_ && blockdb_) {
                // Serve block from chain state
                BlockHash blockHash(item.hash);
                BlockIndex* pindex = chainman_->LookupBlockIndex(blockHash);
                
                if (pindex && HasStatus(pindex->nStatus, BlockStatus::HAVE_DATA)) {
                    // Read block from disk
                    db::DiskBlockPos pos(pindex->nFile, pindex->nDataPos);
                    Block block;
                    db::Status status = blockdb_->ReadBlock(pos, block);
                    
                    if (status.ok()) {
                        // Serialize and send the block
                        DataStream stream;
                        Serialize(stream, block);
                        std::vector<uint8_t> blockPayload(stream.begin(), stream.end());
                        auto msg = CreateMessage(NetworkMagic::MAINNET, NetMsgType::BLOCK, blockPayload);
                        peer.QueueSend(msg);
                        served = true;
                        
                        LOG_DEBUG(util::LogCategory::NET) << "Sent block " 
                            << blockHash.ToHex().substr(0, 16) << "... to peer " << peer.GetId();
                    } else {
                        LOG_WARN(util::LogCategory::NET) << "Failed to read block from disk: " 
                            << status.ToString();
                    }
                }
            }
        }
        
        if (!served) {
            notFound.push_back(item);
        }
    }
    
    // Send notfound for items we couldn't provide
    if (!notFound.empty()) {
        DataStream stream;
        Serialize(stream, static_cast<uint64_t>(notFound.size()));
        for (const auto& item : notFound) {
            item.Serialize(stream);
        }
        std::vector<uint8_t> data(stream.begin(), stream.end());
        auto msg = CreateMessage(NetworkMagic::MAINNET, NetMsgType::NOTFOUND, data);
        peer.QueueSend(msg);
    }
    
    return true;
}

bool MessageProcessor::HandleNotFound(Peer& peer, DataStream& payload) {
    uint64_t count;
    Unserialize(payload, count);
    
    if (count > MAX_INV_SZ) {
        return true;  // Just ignore oversized
    }
    
    std::vector<Inv> inv;
    for (uint64_t i = 0; i < count; i++) {
        Inv item;
        item.Unserialize(payload);
        inv.push_back(item);
    }
    
    // Notify sync manager
    if (sync_ && !inv.empty()) {
        sync_->ProcessNotFound(peer.GetId(), inv);
    }
    
    return true;
}

// ============================================================================
// Block/Header Handlers
// ============================================================================

bool MessageProcessor::HandleHeaders(Peer& peer, DataStream& payload) {
    uint64_t count;
    Unserialize(payload, count);
    
    if (count > MAX_HEADERS_RESULTS) {
        LOG_DEBUG(util::LogCategory::NET) << "Peer " << peer.GetId() 
                                          << " sent too many headers: " << count;
        peer.Misbehaving(20, "Oversized headers");
        return false;
    }
    
    std::vector<BlockHeader> headers;
    headers.reserve(static_cast<size_t>(count));
    
    for (uint64_t i = 0; i < count; i++) {
        BlockHeader header;
        Unserialize(payload, header);
        
        // Headers message includes a tx count after each header (always 0)
        uint64_t txCount;
        Unserialize(payload, txCount);
        
        headers.push_back(header);
    }
    
    if (headers.empty()) {
        return true;
    }
    
    LOG_DEBUG(util::LogCategory::NET) << "Received " << headers.size() 
                                      << " headers from peer " << peer.GetId();
    
    // Forward to sync manager
    if (sync_) {
        if (!sync_->ProcessHeaders(peer.GetId(), headers)) {
            LOG_DEBUG(util::LogCategory::NET) << "Failed to process headers from peer " 
                                              << peer.GetId();
            // Don't disconnect - sync manager may have already handled it
        }
    }
    
    return true;
}

bool MessageProcessor::HandleBlock(Peer& peer, DataStream& payload) {
    Block block;
    Unserialize(payload, block);
    
    LOG_DEBUG(util::LogCategory::NET) << "Received block " 
                                      << block.GetHash().ToHex().substr(0, 16)
                                      << " from peer " << peer.GetId();
    
    // Forward to sync manager
    if (sync_) {
        if (!sync_->ProcessBlock(peer.GetId(), block)) {
            LOG_DEBUG(util::LogCategory::NET) << "Failed to process block from peer " 
                                              << peer.GetId();
            // Sync manager decides whether to penalize
        }
    }
    
    return true;
}

bool MessageProcessor::HandleGetHeaders(Peer& peer, DataStream& payload) {
    GetHeadersMessage msg;
    msg.Unserialize(payload);
    
    LOG_DEBUG(util::LogCategory::NET) << "Received getheaders from peer " << peer.GetId()
                                      << " with " << msg.locator.vHave.size() << " locator hashes";
    
    // Use callback if available
    if (getHeadersCallback_) {
        auto headers = getHeadersCallback_(msg.locator, msg.hashStop);
        if (!headers.empty()) {
            SendHeaders(peer, headers);
        }
    }
    
    return true;
}

bool MessageProcessor::HandleGetBlocks(Peer& peer, DataStream& payload) {
    GetBlocksMessage msg;
    msg.Unserialize(payload);
    
    LOG_DEBUG(util::LogCategory::NET) << "Received getblocks from peer " << peer.GetId();
    
    if (!chainman_) {
        return true;  // Can't serve without chain state
    }
    
    // Find the best known block from the locator
    const Chain& chain = chainman_->GetActiveChain();
    BlockIndex* pindex = nullptr;
    
    // Find first matching block in locator
    for (const auto& hash : msg.locator.vHave) {
        BlockMap& blockIndex = chainman_->GetBlockIndex();
        auto it = blockIndex.find(hash);
        if (it != blockIndex.end()) {
            pindex = it->second.get();
            // Make sure it's on the main chain
            if (chain.Contains(pindex)) {
                break;
            }
            pindex = nullptr;
        }
    }
    
    // If no match, start from genesis
    if (!pindex) {
        pindex = chain.Genesis();
    }
    
    if (!pindex) {
        return true;  // No blocks to serve
    }
    
    // Send inventory for blocks after the locator match
    std::vector<Inv> vInv;
    int nLimit = 500;  // Maximum blocks to announce
    
    pindex = chain.Next(pindex);
    while (pindex && nLimit-- > 0) {
        // Stop at hashStop if specified
        if (!msg.hashStop.IsNull() && pindex->GetBlockHash() == msg.hashStop) {
            break;
        }
        
        vInv.push_back(Inv(InvType::MSG_BLOCK, pindex->GetBlockHash()));
        pindex = chain.Next(pindex);
    }
    
    if (!vInv.empty()) {
        SendInv(peer, vInv);
        LOG_DEBUG(util::LogCategory::NET) << "Sent " << vInv.size() 
                                          << " block inv to peer " << peer.GetId();
    }
    
    return true;
}

// ============================================================================
// Transaction Handler
// ============================================================================

bool MessageProcessor::HandleTx(Peer& peer, DataStream& payload) {
    if (!options_.relayTransactions) {
        return true;  // Tx relay disabled
    }
    
    // Deserialize into mutable transaction first
    MutableTransaction mtx;
    Unserialize(payload, mtx);
    
    auto txHash = mtx.GetHash();
    LOG_DEBUG(util::LogCategory::NET) << "Received tx " << txHash.ToHex().substr(0, 16)
                                      << " from peer " << peer.GetId();
    
    // Mark that peer has this tx
    peer.AddInventory(Inv(InvType::MSG_TX, txHash));
    
    // Try to accept to mempool
    if (mempool_ && coins_) {
        auto txRef = MakeTransactionRef(std::move(mtx));
        auto result = AcceptToMempool(txRef, *mempool_, *coins_, chainHeight_.load());
        
        if (result.IsValid()) {
            LOG_DEBUG(util::LogCategory::NET) << "Accepted tx " << txHash.ToHex().substr(0, 16)
                                              << " to mempool, fee=" << result.fee;
            // Relay to other peers (exclude the sender)
            RelayTransaction(txHash, peer.GetId());
        } else {
            LOG_DEBUG(util::LogCategory::NET) << "Rejected tx " << txHash.ToHex().substr(0, 16)
                                              << ": " << result.rejectReason;
            // Don't penalize for invalid txs from inv (they might just be outdated)
        }
    }
    
    return true;
}

bool MessageProcessor::HandleMempool(Peer& peer) {
    if (!options_.relayTransactions || !mempool_) {
        return true;
    }
    
    // Get all tx info from mempool and send as inv
    auto txInfos = mempool_->GetAllTxInfo();
    
    if (txInfos.empty()) {
        return true;
    }
    
    std::vector<Inv> inv;
    inv.reserve(txInfos.size());
    for (const auto& info : txInfos) {
        inv.emplace_back(InvType::MSG_TX, info.tx->GetHash());
    }
    
    SendInv(peer, inv);
    
    return true;
}

bool MessageProcessor::HandleFeeFilter(Peer& peer, DataStream& payload) {
    FeeFilterMessage msg;
    msg.Unserialize(payload);
    
    // Sanity check - reject obviously invalid values
    if (msg.minFeeRate < 0) {
        LOG_DEBUG(util::LogCategory::NET) << "Peer " << peer.GetId() 
                                          << " sent invalid negative fee filter: " << msg.minFeeRate;
        return true;  // Don't disconnect, just ignore
    }
    
    // Cap at a reasonable maximum (1 BTC/kB = 100,000,000 satoshi/kB)
    static constexpr int64_t MAX_FEE_FILTER = 100000000;
    int64_t feeFilter = std::min(msg.minFeeRate, MAX_FEE_FILTER);
    
    LOG_DEBUG(util::LogCategory::NET) << "Peer " << peer.GetId() 
                                      << " set fee filter to " << feeFilter << " sat/kB";
    
    // Store fee filter for this peer
    peer.SetFeeFilter(feeFilter);
    
    return true;
}

// ============================================================================
// Address Handler
// ============================================================================

bool MessageProcessor::HandleAddr(Peer& peer, DataStream& payload) {
    AddrMessage msg;
    msg.Unserialize(payload);
    
    if (msg.addresses.size() > MAX_ADDR_TO_SEND) {
        LOG_DEBUG(util::LogCategory::NET) << "Peer " << peer.GetId() 
                                          << " sent too many addresses: " << msg.addresses.size();
        peer.Misbehaving(20, "Oversized addr");
        return false;
    }
    
    LOG_DEBUG(util::LogCategory::NET) << "Received " << msg.addresses.size() 
                                      << " addresses from peer " << peer.GetId();
    
    // Add addresses to address manager for future connections
    if (addrman_ && !msg.addresses.empty()) {
        // Get peer's address as source
        NetService source(peer.GetAddress());
        
        // Add addresses with a small time penalty (1 hour) since they came from the network
        size_t added = addrman_->Add(msg.addresses, source, 3600);
        
        LOG_DEBUG(util::LogCategory::NET) << "Added " << added << " new addresses from peer " 
                                          << peer.GetId();
    }
    
    return true;
}

// ============================================================================
// Feature Negotiation
// ============================================================================

bool MessageProcessor::HandleSendHeaders(Peer& peer) {
    peer.SetPrefersHeaders(true);
    LOG_DEBUG(util::LogCategory::NET) << "Peer " << peer.GetId() << " prefers headers";
    return true;
}

// ============================================================================
// Periodic Tasks
// ============================================================================

void MessageProcessor::SendPings() {
    if (!connman_) return;
    
    auto now = std::chrono::steady_clock::now();
    auto peers = connman_->GetAllPeers();
    
    for (auto& peer : peers) {
        if (!peer || !peer->IsEstablished()) continue;
        
        // Check if we need to send a ping
        std::lock_guard<std::mutex> lock(pingMutex_);
        auto it = lastPingTime_.find(peer->GetId());
        
        bool shouldPing = false;
        if (it == lastPingTime_.end()) {
            shouldPing = true;
        } else {
            auto timeSincePing = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second).count();
            shouldPing = (timeSincePing >= options_.pingIntervalSec);
        }
        
        if (shouldPing && peer->GetPingNonce() == 0) {
            peer->SendPing();
            lastPingTime_[peer->GetId()] = now;
            LOG_DEBUG(util::LogCategory::NET) << "Sent ping to peer " << peer->GetId();
        }
    }
}

void MessageProcessor::CheckPingTimeouts() {
    if (!connman_) return;
    
    auto now = std::chrono::steady_clock::now();
    auto peers = connman_->GetAllPeers();
    
    for (auto& peer : peers) {
        if (!peer || !peer->IsEstablished()) continue;
        
        if (peer->GetPingNonce() != 0) {
            // Waiting for pong - check timeout
            std::lock_guard<std::mutex> lock(pingMutex_);
            auto it = lastPingTime_.find(peer->GetId());
            if (it != lastPingTime_.end()) {
                auto waitTime = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->second).count();
                if (waitTime > options_.pingTimeoutSec) {
                    LOG_INFO(util::LogCategory::NET) << "Peer " << peer->GetId() 
                                                     << " ping timeout after " << waitTime << "s";
                    peer->Disconnect(DisconnectReason::TIMEOUT);
                }
            }
        }
    }
}

void MessageProcessor::SendAnnouncements() {
    if (!connman_) return;
    
    auto peers = connman_->GetAllPeers();
    
    for (auto& peer : peers) {
        if (!peer || !peer->IsEstablished()) continue;
        
        // Get announcements queued for this peer
        std::vector<Inv> announcements = peer->GetAnnouncementsToSend(MAX_INV_SZ);
        
        if (announcements.empty()) continue;
        
        // Separate blocks and transactions for different handling
        std::vector<Inv> blockInvs;
        std::vector<Inv> txInvs;
        
        for (const auto& inv : announcements) {
            if (inv.type == InvType::MSG_BLOCK) {
                blockInvs.push_back(inv);
            } else if (inv.type == InvType::MSG_TX) {
                txInvs.push_back(inv);
            }
        }
        
        // For blocks, check if peer prefers headers
        if (!blockInvs.empty()) {
            if (peer->PrefersHeaders() && chainman_) {
                // Send headers instead of inv for peers that prefer it
                std::vector<BlockHeader> headers;
                for (const auto& inv : blockInvs) {
                    BlockHash hash(inv.hash);
                    BlockIndex* pindex = chainman_->LookupBlockIndex(hash);
                    if (pindex) {
                        headers.push_back(pindex->GetBlockHeader());
                    }
                }
                if (!headers.empty()) {
                    SendHeaders(*peer, headers);
                }
            } else {
                // Send inv for blocks
                SendInv(*peer, blockInvs);
            }
        }
        
        // Send inv for transactions
        if (!txInvs.empty()) {
            SendInv(*peer, txInvs);
        }
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

void MessageProcessor::SendVersion(Peer& peer) {
    int32_t ourHeight = chainHeight_.load();
    
    auto version = peer.CreateVersionMessage(localAddress_, ourHeight, ourServices_);
    peer.QueueMessage(NetMsgType::VERSION, version);
    peer.SetVersionSent();
    
    LOG_DEBUG(util::LogCategory::NET) << "Sent version to peer " << peer.GetId();
}

void MessageProcessor::SendVerack(Peer& peer) {
    peer.QueueMessage(NetMsgType::VERACK);
    LOG_DEBUG(util::LogCategory::NET) << "Sent verack to peer " << peer.GetId();
}

void MessageProcessor::SendPong(Peer& peer, uint64_t nonce) {
    PongMessage pong(nonce);
    peer.QueueMessage(NetMsgType::PONG, pong);
}

void MessageProcessor::SendHeaders(Peer& peer, const std::vector<BlockHeader>& headers) {
    if (headers.empty()) return;
    
    DataStream stream;
    Serialize(stream, static_cast<uint64_t>(headers.size()));
    for (const auto& header : headers) {
        Serialize(stream, header);
        // Headers message includes tx count (always 0) after each header
        Serialize(stream, static_cast<uint64_t>(0));
    }
    
    std::vector<uint8_t> payload(stream.begin(), stream.end());
    auto msg = CreateMessage(NetworkMagic::MAINNET, NetMsgType::HEADERS, payload);
    peer.QueueSend(msg);
}

void MessageProcessor::SendInv(Peer& peer, const std::vector<Inv>& inv) {
    if (inv.empty()) return;
    
    // Don't send more than MAX_INV_SZ at once
    size_t offset = 0;
    while (offset < inv.size()) {
        size_t count = std::min(inv.size() - offset, MAX_INV_SZ);
        
        DataStream stream;
        Serialize(stream, static_cast<uint64_t>(count));
        for (size_t i = 0; i < count; i++) {
            inv[offset + i].Serialize(stream);
        }
        
        std::vector<uint8_t> payload(stream.begin(), stream.end());
        auto msg = CreateMessage(NetworkMagic::MAINNET, NetMsgType::INV, payload);
        peer.QueueSend(msg);
        
        offset += count;
    }
}

void MessageProcessor::SendGetData(Peer& peer, const std::vector<Inv>& inv) {
    if (inv.empty()) return;
    
    DataStream stream;
    Serialize(stream, static_cast<uint64_t>(inv.size()));
    for (const auto& item : inv) {
        item.Serialize(stream);
    }
    
    std::vector<uint8_t> payload(stream.begin(), stream.end());
    auto msg = CreateMessage(NetworkMagic::MAINNET, NetMsgType::GETDATA, payload);
    peer.QueueSend(msg);
}

// ============================================================================
// Transaction and Block Relay
// ============================================================================

void MessageProcessor::RelayTransaction(const TxHash& txid, Peer::Id excludePeer) {
    if (!connman_ || !options_.relayTransactions) return;
    
    LOG_DEBUG(util::LogCategory::NET) << "Relaying tx " << txid.ToHex().substr(0, 16) 
                                      << " to peers";
    
    // Get transaction fee rate from mempool for fee filter comparison
    int64_t txFeeRatePerK = 0;  // satoshis per KB
    if (mempool_) {
        auto txInfo = mempool_->GetInfo(txid);
        if (txInfo) {
            txFeeRatePerK = txInfo->feeRate.GetFeePerK();
        }
    }
    
    auto peers = connman_->GetAllPeers();
    
    std::vector<Inv> invVec;
    invVec.emplace_back(InvType::MSG_TX, txid);
    
    int relayCount = 0;
    int filteredCount = 0;
    for (auto& peer : peers) {
        if (!peer || !peer->IsEstablished()) continue;
        if (peer->GetId() == excludePeer) continue;
        
        // Check if peer already has this transaction
        if (peer->HasInventory(invVec[0])) continue;
        
        // Check peer's fee filter - only relay if tx fee rate meets their minimum
        int64_t peerFeeFilter = peer->GetFeeFilter();
        if (peerFeeFilter > 0 && txFeeRatePerK < peerFeeFilter) {
            ++filteredCount;
            continue;  // Skip this peer - tx doesn't meet their fee threshold
        }
        
        // Mark peer as having this tx (so we don't send again)
        peer->AddInventory(invVec[0]);
        
        // Send inv
        SendInv(*peer, invVec);
        ++relayCount;
    }
    
    if (filteredCount > 0) {
        LOG_DEBUG(util::LogCategory::NET) << "Skipped " << filteredCount 
                                          << " peers due to fee filter (tx fee: " 
                                          << txFeeRatePerK << " sat/kB)";
    }
    LOG_DEBUG(util::LogCategory::NET) << "Relayed tx to " << relayCount << " peers";
}

void MessageProcessor::RelayBlock(const BlockHash& blockHash, Peer::Id excludePeer) {
    if (!connman_) return;
    
    LOG_DEBUG(util::LogCategory::NET) << "Relaying block " << blockHash.ToHex().substr(0, 16) 
                                      << " to peers";
    
    // Get block header for peers that prefer headers-first
    BlockHeader blockHeader;
    bool haveHeader = false;
    if (chainman_) {
        BlockIndex* pindex = chainman_->LookupBlockIndex(blockHash);
        if (pindex) {
            blockHeader = pindex->GetBlockHeader();
            haveHeader = true;
        }
    }
    
    auto peers = connman_->GetAllPeers();
    
    std::vector<Inv> invVec;
    invVec.emplace_back(InvType::MSG_BLOCK, blockHash);
    
    int relayCount = 0;
    int headersCount = 0;
    for (auto& peer : peers) {
        if (!peer || !peer->IsEstablished()) continue;
        if (peer->GetId() == excludePeer) continue;
        
        // Check if peer already has this block
        if (peer->HasInventory(invVec[0])) continue;
        
        // Mark peer as having this block
        peer->AddInventory(invVec[0]);
        
        // For peers that prefer headers-first, send headers instead of inv
        if (peer->PrefersHeaders() && haveHeader) {
            std::vector<BlockHeader> headers;
            headers.push_back(blockHeader);
            SendHeaders(*peer, headers);
            ++headersCount;
        } else {
            SendInv(*peer, invVec);
        }
        ++relayCount;
    }
    
    LOG_DEBUG(util::LogCategory::NET) << "Relayed block to " << relayCount << " peers"
                                      << " (" << headersCount << " via headers)";
}

void MessageProcessor::QueueTransactionRelay(const TxHash& txid) {
    std::lock_guard<std::mutex> lock(relayMutex_);
    pendingTxRelay_.push_back(txid);
}

void MessageProcessor::FlushRelayQueue() {
    std::vector<TxHash> txsToRelay;
    std::vector<BlockHash> blocksToRelay;
    
    {
        std::lock_guard<std::mutex> lock(relayMutex_);
        txsToRelay = std::move(pendingTxRelay_);
        blocksToRelay = std::move(pendingBlockRelay_);
        pendingTxRelay_.clear();
        pendingBlockRelay_.clear();
    }
    
    // Relay all queued transactions
    for (const auto& txid : txsToRelay) {
        RelayTransaction(txid);
    }
    
    // Relay all queued blocks
    for (const auto& blockHash : blocksToRelay) {
        RelayBlock(blockHash);
    }
}

// ============================================================================
// Statistics
// ============================================================================

MessageStats MessageProcessor::GetStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void MessageProcessor::ResetStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = MessageStats{};
}

// ============================================================================
// Integration Helper
// ============================================================================

std::unique_ptr<MessageProcessor> StartMessageProcessor(NodeContext& node) {
    auto processor = std::make_unique<MessageProcessor>();
    
    processor->InitializeFromContext(node);
    
    // Create and configure block synchronizer if needed
    // Note: BlockSynchronizer should be created separately and wired up
    
    if (!processor->Start()) {
        LOG_ERROR(util::LogCategory::NET) << "Failed to start message processor";
        return nullptr;
    }
    
    return processor;
}

} // namespace shurium
