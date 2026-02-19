// SHURIUM - Node Context Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/node/context.h"
#include "shurium/network/addrman.h"
#include "shurium/core/block.h"
#include "shurium/util/logging.h"
#include "shurium/db/database.h"
#include "shurium/economics/funds.h"

#include <sys/stat.h>

namespace shurium {

// ============================================================================
// Directory Creation Helper
// ============================================================================

static bool CreateDirectoryIfNeeded(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return std::filesystem::is_directory(path, ec);
    }
    return std::filesystem::create_directories(path, ec);
}

// ============================================================================
// Get Consensus Parameters
// ============================================================================

static std::unique_ptr<consensus::Params> GetConsensusParams(const std::string& network) {
    auto params = std::make_unique<consensus::Params>();
    
    if (network == "testnet") {
        *params = consensus::Params::TestNet();
    } else if (network == "regtest") {
        *params = consensus::Params::RegTest();
    } else {
        *params = consensus::Params::Main();
    }
    
    return params;
}

// ============================================================================
// Get Genesis Block for Network
// ============================================================================

static Block GetGenesisBlock(const std::string& network, Amount blockReward) {
    if (network == "testnet") {
        // Genesis hash: 000001b2150a56cc228d9b60fedaace333bb67b4ef168ef1e01e29b6ce61ae75
        return CreateGenesisBlock(
            1700000001,    // Timestamp
            2015211,       // Nonce (mined for 50 SHR reward)
            0x1e0fffff,    // Initial difficulty
            1,             // Version
            blockReward
        );
    } else if (network == "regtest") {
        // Genesis hash: 277a4081985b8800293bf3cda91202c6b761a8b8de4f5fcc018d6cf14f60737c
        return CreateGenesisBlock(
            1700000002,    // Timestamp
            6,             // Nonce (mined for 50 SHR reward)
            0x207fffff,    // Very easy difficulty
            1,             // Version
            blockReward
        );
    } else {
        // Mainnet
        // Genesis hash: 0000090f1d7ccd5f0b91be5a92cfa9e075c6af443594f33f7c2238c3626f3172
        return CreateGenesisBlock(
            1700000000,    // Timestamp
            586684,        // Nonce (mined for 50 SHR reward)
            0x1e0fffff,    // Initial difficulty
            1,             // Version
            blockReward
        );
    }
}

// ============================================================================
// InitializeNode - Main initialization function
// ============================================================================

bool InitializeNode(NodeContext& node, const NodeInitOptions& options) {
    LOG_INFO(util::LogCategory::DEFAULT) << "Initializing node...";
    
    // Store options
    node.dataDir = options.dataDir;
    node.reindex = options.reindex;
    
    // ========================================================================
    // Step 1: Create directories
    // ========================================================================
    
    node.blocksDir = node.dataDir / "blocks";
    node.chainstateDir = node.dataDir / "chainstate";
    
    if (!CreateDirectoryIfNeeded(node.blocksDir)) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to create blocks directory: " 
                                              << node.blocksDir.string();
        return false;
    }
    
    if (!CreateDirectoryIfNeeded(node.chainstateDir)) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to create chainstate directory: " 
                                              << node.chainstateDir.string();
        return false;
    }
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Blocks directory: " << node.blocksDir.string();
    LOG_INFO(util::LogCategory::DEFAULT) << "Chainstate directory: " << node.chainstateDir.string();
    
    // ========================================================================
    // Step 2: Get consensus parameters
    // ========================================================================
    
    node.params = GetConsensusParams(options.network);
    LOG_INFO(util::LogCategory::DEFAULT) << "Network: " << options.network 
                                         << " (genesis: " << node.params->hashGenesisBlock.ToHex().substr(0, 16) << "...)";
    
    // ========================================================================
    // Step 2b: Initialize fund manager
    // ========================================================================
    
    if (!economics::InitializeFundManager(options.network)) {
        LOG_WARN(util::LogCategory::DEFAULT) << "Failed to initialize fund manager";
        // Non-fatal - continue without fund manager
    } else {
        LOG_INFO(util::LogCategory::DEFAULT) << "Fund manager initialized for " << options.network;
    }
    
    // ========================================================================
    // Step 3: Open databases
    // ========================================================================
    
    // Database options
    db::Options dbOptions;
    dbOptions.create_if_missing = true;
    dbOptions.write_buffer_size = static_cast<size_t>(options.dbCacheMB) * 1024 * 1024 / 4;
    dbOptions.max_open_files = 64;
    
    // Open block database
    try {
        LOG_INFO(util::LogCategory::DEFAULT) << "Opening block database...";
        node.blockDB = std::make_unique<db::BlockDB>(node.blocksDir, dbOptions);
        
        if (!node.blockDB->IsOpen()) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to open block database";
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to open block database: " << e.what();
        return false;
    }
    
    // Open UTXO database
    try {
        LOG_INFO(util::LogCategory::DEFAULT) << "Opening UTXO database...";
        node.coinsDB = std::make_unique<db::CoinsViewDB>(
            node.chainstateDir,
            dbOptions,
            options.reindex  // wipe if reindexing
        );
        
        if (!node.coinsDB->IsOpen()) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to open UTXO database";
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to open UTXO database: " << e.what();
        return false;
    }
    
    // Open transaction index if enabled
    if (options.txIndex) {
        try {
            LOG_INFO(util::LogCategory::DEFAULT) << "Opening transaction index...";
            auto txIndexPath = node.dataDir / "txindex";
            CreateDirectoryIfNeeded(txIndexPath);
            node.txIndex = std::make_unique<db::TxIndex>(txIndexPath, dbOptions);
            node.txIndex->SetEnabled(true);
        } catch (const std::exception& e) {
            LOG_WARN(util::LogCategory::DEFAULT) << "Failed to open transaction index: " << e.what();
            // Non-fatal, continue without txindex
        }
    }
    
    // ========================================================================
    // Step 4: Create chain state manager
    // ========================================================================
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Initializing chain state manager...";
    
    try {
        node.chainman = std::make_unique<ChainStateManager>(*node.params);
        
        // Set the block database for storing blocks
        if (node.blockDB) {
            node.chainman->SetBlockDB(node.blockDB.get());
        }
        
        if (!node.chainman->Initialize(node.coinsDB.get())) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to initialize chain state manager";
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to create chain state manager: " << e.what();
        return false;
    }
    
    // ========================================================================
    // Step 5: Load block index
    // ========================================================================
    
    int nLoaded = LoadBlockIndex(node);
    if (nLoaded < 0) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to load block index";
        return false;
    }
    
    if (nLoaded > 0) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Loaded " << nLoaded << " block index entries";
    } else {
        // No blocks loaded - this is a fresh node, add genesis block
        LOG_INFO(util::LogCategory::DEFAULT) << "No block index found, initializing with genesis block";
        
        // Get genesis block for this network
        Block genesis = GetGenesisBlock(options.network, node.params->nInitialBlockReward);
        
        // Add genesis block to the chain state
        BlockIndex* genesisIndex = node.chainman->ProcessBlockHeader(genesis);
        
        if (!genesisIndex) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to add genesis block to index";
            return false;
        }
        
        // Process the full genesis block
        if (!node.chainman->ProcessNewBlock(genesis, true)) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to process genesis block";
            return false;
        }
        
        LOG_INFO(util::LogCategory::DEFAULT) << "Genesis block activated at height 0";
    }
    
    // ========================================================================
    // Step 6: Verify block index (if check enabled)
    // ========================================================================
    
    if (options.checkBlocks && nLoaded > 0) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Verifying block index...";
        if (!VerifyBlockIndex(node, options.checkLevel)) {
            LOG_WARN(util::LogCategory::DEFAULT) << "Block index verification found issues";
            // Non-fatal for now
        }
    }
    
    // ========================================================================
    // Step 7: Activate best chain
    // ========================================================================
    
    if (!ActivateBestChain(node)) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to activate best chain";
        return false;
    }
    
    BlockIndex* tip = node.GetTip();
    if (tip) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Best chain tip: height=" << tip->nHeight 
                                             << " hash=" << tip->GetBlockHash().ToHex().substr(0, 16) << "...";
    }
    
    // ========================================================================
    // Step 8: Initialize mempool
    // ========================================================================
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Initializing mempool...";
    
    MempoolLimits mempoolLimits;
    mempoolLimits.maxSize = 300 * 1000 * 1000;  // 300 MB
    mempoolLimits.maxAge = 14 * 24 * 60 * 60;   // 14 days
    
    node.mempool = std::make_unique<Mempool>(mempoolLimits);
    
    // Set up mempool notifications
    node.mempool->SetNotifyRemoved([](const TransactionRef& tx, MempoolRemovalReason reason) {
        LOG_DEBUG(util::LogCategory::MEMPOOL) << "Tx removed from mempool: " 
                                              << tx->GetHash().ToHex().substr(0, 16) 
                                              << " reason=" << RemovalReasonToString(reason);
    });
    
    // ========================================================================
    // Done
    // ========================================================================
    
    node.initialized.store(true);
    LOG_INFO(util::LogCategory::DEFAULT) << "Node initialization complete";
    
    return true;
}

// ============================================================================
// StartNetwork - Start P2P network
// ============================================================================

bool StartNetwork(NodeContext& node, const NodeInitOptions& options) {
    if (!options.listen && options.connectNodes.empty() && options.addNodes.empty()) {
        LOG_INFO(util::LogCategory::NET) << "Network disabled (no listen, no connect nodes)";
        return true;
    }
    
    LOG_INFO(util::LogCategory::NET) << "Starting P2P network...";
    
    ConnectionManagerOptions connOptions;
    connOptions.maxConnections = static_cast<size_t>(options.maxConnections);
    connOptions.acceptInbound = options.listen;
    connOptions.listenPort = options.port;
    
    if (!options.bindAddress.empty()) {
        connOptions.bindAddress = options.bindAddress;
    }
    
    try {
        // Create address manager for peer discovery
        node.addrman = std::make_unique<AddressManager>(options.network);
        
        // Load saved addresses if available
        auto addrPath = node.dataDir / "peers.dat";
        if (std::filesystem::exists(addrPath)) {
            node.addrman->Load(addrPath.string());
            LOG_INFO(util::LogCategory::NET) << "Loaded " << node.addrman->Size() 
                                             << " peer addresses from disk";
        }
        
        // Create connection manager
        node.connman = std::make_unique<ConnectionManager>(connOptions);
        
        // Create block synchronizer
        node.syncman = std::make_unique<BlockSynchronizer>();
        
        // Create message processor
        MessageProcessorOptions msgOptions;
        msgOptions.relayTransactions = true;
        node.msgproc = std::make_unique<MessageProcessor>(msgOptions);
        
        // Initialize message processor with components
        node.msgproc->Initialize(node.connman.get(), node.syncman.get());
        node.msgproc->SetMempool(node.mempool.get());
        node.msgproc->SetChainManager(node.chainman.get());
        node.msgproc->SetAddressManager(node.addrman.get());
        
        // Set chain height for tx validation
        if (node.chainman) {
            node.msgproc->SetChainHeight(node.chainman->GetActiveHeight());
        }
        
        // Set up sync manager callbacks
        node.syncman->SetBlockCallback([&node](const Block& block, Peer::Id fromPeer) -> bool {
            // Process received block through chain state
            if (!node.chainman) return false;
            
            bool accepted = node.chainman->ProcessNewBlock(block);
            
            if (accepted) {
                // Relay the block to other peers (exclude the sender)
                if (node.msgproc) {
                    node.msgproc->RelayBlock(block.GetHash(), fromPeer);
                }
            } else {
                LOG_DEBUG(util::LogCategory::NET) << "Block rejected from peer " << fromPeer;
            }
            
            return accepted;
        });
        
        node.syncman->SetHeaderCallback([&node](const std::vector<BlockHeader>& headers, 
                                                 Peer::Id fromPeer) -> bool {
            // Process headers through chain state
            if (!node.chainman) return false;
            
            // For now, just log that we received headers
            LOG_DEBUG(util::LogCategory::NET) << "Received " << headers.size() 
                                              << " headers from peer " << fromPeer;
            return true;
        });
        
        // Set up peer callbacks
        node.connman->SetNewPeerCallback([](std::shared_ptr<Peer> peer) {
            LOG_INFO(util::LogCategory::NET) << "New peer connected: " << peer->GetId() 
                                             << " " << peer->GetAddress().ToString();
        });
        
        node.connman->SetDisconnectedCallback([&node](Peer::Id id, DisconnectReason reason) {
            LOG_INFO(util::LogCategory::NET) << "Peer disconnected: " << id;
            
            // Notify sync manager
            if (node.syncman) {
                node.syncman->OnPeerDisconnected(id);
            }
        });
        
        // Set up message processor handshake callback to start sync
        node.msgproc->SetHandshakeCallback([&node](Peer::Id peerId) {
            LOG_DEBUG(util::LogCategory::NET) << "Handshake complete with peer " << peerId;
            
            // Update chain height in message processor
            if (node.chainman && node.msgproc) {
                node.msgproc->SetChainHeight(node.chainman->GetActiveHeight());
            }
        });
        
        // Start connection manager
        if (!node.connman->Start()) {
            LOG_ERROR(util::LogCategory::NET) << "Failed to start connection manager";
            return false;
        }
        
        // Start message processor
        if (!node.msgproc->Start()) {
            LOG_ERROR(util::LogCategory::NET) << "Failed to start message processor";
            return false;
        }
        
        // Start sync manager
        node.syncman->Start();
        
        // Connect to specified nodes
        for (const auto& addr : options.connectNodes) {
            // Parse address:port
            std::string host = addr;
            uint16_t port = options.port;
            
            size_t colonPos = addr.rfind(':');
            if (colonPos != std::string::npos) {
                host = addr.substr(0, colonPos);
                port = static_cast<uint16_t>(std::stoi(addr.substr(colonPos + 1)));
            }
            
            LOG_INFO(util::LogCategory::NET) << "Connecting to: " << host << ":" << port;
            // Note: Actual connection would require DNS resolution
            // For now, just log the intention
        }
        
        // Add nodes to connect to
        for (const auto& addr : options.addNodes) {
            LOG_INFO(util::LogCategory::NET) << "Added node: " << addr;
        }
        
        if (options.listen) {
            LOG_INFO(util::LogCategory::NET) << "Listening on port " << options.port;
        }
        
        // Resolve DNS seeds in the background if enabled
        if (options.dnsSeed && node.addrman) {
            LOG_INFO(util::LogCategory::NET) << "Resolving DNS seeds...";
            node.addrman->Start();
            node.addrman->ResolveSeeds([&node](const std::vector<NetService>& addresses) {
                LOG_INFO(util::LogCategory::NET) << "DNS seed resolution complete: " 
                                                 << addresses.size() << " addresses found";
                if (node.addrman) {
                    LOG_INFO(util::LogCategory::NET) << "Total known addresses: " 
                                                     << node.addrman->Size();
                }
            });
        }
        
        LOG_INFO(util::LogCategory::NET) << "P2P network started, max connections: " 
                                         << options.maxConnections;
        
    } catch (const std::exception& e) {
        LOG_ERROR(util::LogCategory::NET) << "Failed to start network: " << e.what();
        return false;
    }
    
    return true;
}

// ============================================================================
// StartSync - Start block synchronization
// ============================================================================

bool StartSync(NodeContext& node) {
    if (!node.connman) {
        LOG_DEBUG(util::LogCategory::NET) << "No connection manager, sync skipped";
        return true;
    }
    
    LOG_INFO(util::LogCategory::NET) << "Starting block synchronization...";
    
    // Check if we need to sync
    BlockIndex* tip = node.GetTip();
    if (!tip) {
        LOG_ERROR(util::LogCategory::NET) << "No chain tip, cannot start sync";
        return false;
    }
    
    // For now, just mark IBD as complete if we have the genesis block
    // In a real implementation, this would start the sync state machine
    if (tip->nHeight == 0) {
        LOG_INFO(util::LogCategory::NET) << "At genesis block, need to sync blockchain";
        node.ibdComplete.store(false);
    } else {
        // Check if tip is recent (within last 24 hours)
        auto now = std::chrono::system_clock::now();
        auto tipTime = std::chrono::system_clock::from_time_t(static_cast<time_t>(tip->GetBlockTime()));
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - tipTime);
        
        if (age.count() > 24) {
            LOG_INFO(util::LogCategory::NET) << "Chain tip is " << age.count() 
                                             << " hours old, need to sync";
            node.ibdComplete.store(false);
        } else {
            LOG_INFO(util::LogCategory::NET) << "Chain tip is recent, IBD complete";
            node.ibdComplete.store(true);
        }
    }
    
    return true;
}

// ============================================================================
// ShutdownNode - Clean shutdown
// ============================================================================

void ShutdownNode(NodeContext& node) {
    LOG_INFO(util::LogCategory::DEFAULT) << "Shutting down node...";
    
    node.initialized.store(false);
    
    // ========================================================================
    // Step 1: Stop message processor first (before connman)
    // ========================================================================
    
    if (node.msgproc) {
        LOG_INFO(util::LogCategory::NET) << "Stopping message processor...";
        node.msgproc->Stop();
        node.msgproc.reset();
    }
    
    // ========================================================================
    // Step 2: Stop sync manager
    // ========================================================================
    
    if (node.syncman) {
        LOG_INFO(util::LogCategory::NET) << "Stopping block synchronizer...";
        node.syncman->Stop();
        node.syncman.reset();
    }
    
    // ========================================================================
    // Step 3: Stop network
    // ========================================================================
    
    if (node.connman) {
        LOG_INFO(util::LogCategory::NET) << "Stopping P2P network...";
        node.connman->Stop();
        node.connman.reset();
    }
    
    // ========================================================================
    // Step 3.5: Save and stop address manager
    // ========================================================================
    
    if (node.addrman) {
        LOG_INFO(util::LogCategory::NET) << "Saving peer addresses...";
        auto addrPath = node.dataDir / "peers.dat";
        node.addrman->Save(addrPath.string());
        node.addrman->Stop();
        node.addrman.reset();
    }
    
    // ========================================================================
    // Step 4: Clear mempool
    // ========================================================================
    
    if (node.mempool) {
        LOG_INFO(util::LogCategory::MEMPOOL) << "Clearing mempool (" 
                                             << node.mempool->Size() << " transactions)...";
        node.mempool->Clear();
        node.mempool.reset();
    }
    
    // ========================================================================
    // Step 5: Flush chain state
    // ========================================================================
    
    if (node.chainman) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Flushing chain state...";
        auto& chainstate = node.chainman->GetActiveChainState();
        chainstate.FlushStateToDisk();
        
        // Save best chain tip
        BlockIndex* tip = node.chainman->GetActiveTip();
        if (tip && node.blockDB) {
            node.blockDB->WriteBestChainTip(tip->GetBlockHash());
        }
        
        node.chainman.reset();
    }
    
    // ========================================================================
    // Step 6: Close databases
    // ========================================================================
    
    if (node.txIndex) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Closing transaction index...";
        node.txIndex.reset();
    }
    
    if (node.coinsDB) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Closing UTXO database...";
        node.coinsDB.reset();
    }
    
    if (node.blockDB) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Closing block database...";
        node.blockDB->Flush();
        node.blockDB.reset();
    }
    
    // ========================================================================
    // Step 5: Clean up params
    // ========================================================================
    
    node.params.reset();
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Node shutdown complete";
}

// ============================================================================
// FlushNodeState - Flush without shutdown
// ============================================================================

bool FlushNodeState(NodeContext& node) {
    if (!node.IsReady()) {
        return false;
    }
    
    LOG_DEBUG(util::LogCategory::DEFAULT) << "Flushing node state...";
    
    // Flush chain state
    auto& chainstate = node.chainman->GetActiveChainState();
    if (!chainstate.FlushStateToDisk()) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to flush chain state";
        return false;
    }
    
    // Flush block database
    if (node.blockDB) {
        auto status = node.blockDB->Flush();
        if (!status.ok()) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to flush block database: " 
                                                  << status.message();
            return false;
        }
    }
    
    // Update best chain tip
    BlockIndex* tip = node.chainman->GetActiveTip();
    if (tip && node.blockDB) {
        node.blockDB->WriteBestChainTip(tip->GetBlockHash());
    }
    
    return true;
}

// ============================================================================
// LoadBlockIndex - Load block index from database
// ============================================================================

int LoadBlockIndex(NodeContext& node) {
    if (!node.blockDB || !node.chainman) {
        return -1;
    }
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Loading block index from database...";
    
    BlockMap& blockIndex = node.chainman->GetBlockIndex();
    
    // LoadBlockIndexMap now also wires up parent pointers
    int nLoaded = node.blockDB->LoadBlockIndexMap(blockIndex);
    
    if (nLoaded < 0) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Error loading block index";
        return -1;
    }
    
    // Count orphans (blocks with missing parent)
    int nOrphans = 0;
    for (const auto& [hash, indexPtr] : blockIndex) {
        if (indexPtr->nHeight > 0 && indexPtr->pprev == nullptr) {
            ++nOrphans;
            LOG_WARN(util::LogCategory::DEFAULT) << "Orphan block at height " 
                                                 << indexPtr->nHeight << ": " 
                                                 << hash.ToHex().substr(0, 16);
        }
    }
    
    if (nOrphans > 0) {
        LOG_WARN(util::LogCategory::DEFAULT) << "Found " << nOrphans << " orphan blocks";
    }
    
    return nLoaded;
}

// ============================================================================
// VerifyBlockIndex - Verify block index integrity
// ============================================================================

bool VerifyBlockIndex(const NodeContext& node, int checkLevel) {
    if (!node.chainman) {
        return false;
    }
    
    const BlockMap& blockIndex = node.chainman->GetBlockIndex();
    
    if (blockIndex.empty()) {
        return true;  // Nothing to verify
    }
    
    LOG_DEBUG(util::LogCategory::DEFAULT) << "Verifying " << blockIndex.size() 
                                          << " block index entries (level " << checkLevel << ")...";
    
    int nErrors = 0;
    
    // Level 1: Check block header validity
    if (checkLevel >= 1) {
        for (const auto& [hash, indexPtr] : blockIndex) {
            // Verify hash matches
            if (indexPtr->GetBlockHash() != hash) {
                LOG_ERROR(util::LogCategory::DEFAULT) << "Block hash mismatch at height " 
                                                      << indexPtr->nHeight;
                ++nErrors;
            }
            
            // Verify height consistency
            if (indexPtr->pprev && indexPtr->nHeight != indexPtr->pprev->nHeight + 1) {
                LOG_ERROR(util::LogCategory::DEFAULT) << "Height inconsistency at " 
                                                      << indexPtr->nHeight;
                ++nErrors;
            }
        }
    }
    
    // Level 2: Check chain connectivity
    if (checkLevel >= 2) {
        // Find genesis block
        const BlockIndex* genesis = nullptr;
        for (const auto& [hash, indexPtr] : blockIndex) {
            if (indexPtr->nHeight == 0) {
                genesis = indexPtr.get();
                break;
            }
        }
        
        if (!genesis) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "No genesis block found";
            ++nErrors;
        } else {
            // Verify genesis matches consensus
            if (genesis->GetBlockHash() != node.params->hashGenesisBlock) {
                LOG_ERROR(util::LogCategory::DEFAULT) << "Genesis block hash mismatch";
                ++nErrors;
            }
        }
    }
    
    // Level 3: Check proof of work
    if (checkLevel >= 3) {
        for (const auto& [hash, indexPtr] : blockIndex) {
            // Basic PoW check - hash should be below target
            // (Full validation would check actual difficulty)
            if (indexPtr->nBits == 0) {
                LOG_WARN(util::LogCategory::DEFAULT) << "Block with zero difficulty at height " 
                                                     << indexPtr->nHeight;
            }
        }
    }
    
    if (nErrors > 0) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Block index verification found " 
                                              << nErrors << " errors";
        return false;
    }
    
    LOG_DEBUG(util::LogCategory::DEFAULT) << "Block index verification passed";
    return true;
}

// ============================================================================
// ActivateBestChain - Find and activate the best chain
// ============================================================================

bool ActivateBestChain(NodeContext& node) {
    if (!node.chainman) {
        return false;
    }
    
    LOG_DEBUG(util::LogCategory::DEFAULT) << "Activating best chain...";
    
    // Find the block with most work
    const BlockMap& blockIndex = node.chainman->GetBlockIndex();
    
    if (blockIndex.empty()) {
        LOG_WARN(util::LogCategory::DEFAULT) << "No blocks in index, cannot activate chain";
        return true;  // Not an error, just empty
    }
    
    // Find block with most work (highest height for now, should use chainwork)
    BlockIndex* bestBlock = nullptr;
    for (const auto& [hash, indexPtr] : blockIndex) {
        if (!bestBlock || indexPtr->nHeight > bestBlock->nHeight) {
            bestBlock = indexPtr.get();
        }
    }
    
    if (!bestBlock) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to find best block";
        return false;
    }
    
    // Activate the best chain
    if (!node.chainman->ActivateBestChain()) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to activate best chain";
        return false;
    }
    
    return true;
}

// ============================================================================
// Shutdown Control
// ============================================================================

// Global shutdown state
static std::atomic<bool> g_shutdownRequested{false};

void RequestShutdown() {
    g_shutdownRequested.store(true);
}

bool ShutdownRequested() {
    return g_shutdownRequested.load();
}

} // namespace shurium
