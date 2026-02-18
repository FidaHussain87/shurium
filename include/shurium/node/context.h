// SHURIUM - Node Context
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the NodeContext structure that holds all
// node state for the SHURIUM daemon.

#ifndef SHURIUM_NODE_CONTEXT_H
#define SHURIUM_NODE_CONTEXT_H

#include "shurium/consensus/params.h"
#include "shurium/chain/chainstate.h"
#include "shurium/mempool/mempool.h"
#include "shurium/db/blockdb.h"
#include "shurium/db/utxodb.h"
#include "shurium/network/connection.h"
#include "shurium/network/sync.h"
#include "shurium/network/message_processor.h"
#include "shurium/network/addrman.h"

#include <memory>
#include <string>
#include <atomic>
#include <filesystem>

namespace shurium {

// Forward declarations
class ChainStateManager;
class ConnectionManager;
class BlockSynchronizer;
class MessageProcessor;
class AddressManager;
class Mempool;

namespace db {
class BlockDB;
class CoinsViewDB;
class TxIndex;
}

// ============================================================================
// Node Initialization Options
// ============================================================================

/**
 * Options for node initialization.
 * Populated from command-line and config file.
 */
struct NodeInitOptions {
    /// Data directory path
    std::filesystem::path dataDir;
    
    /// Network type (main, testnet, regtest)
    std::string network{"main"};
    
    /// Database cache size in MB
    int dbCacheMB{450};
    
    /// Enable transaction index
    bool txIndex{false};
    
    /// Reindex blockchain
    bool reindex{false};
    
    /// Prune mode settings
    bool prune{false};
    int pruneSizeMB{550};
    
    /// P2P settings
    bool listen{true};
    std::string bindAddress{"0.0.0.0"};
    uint16_t port{8333};
    int maxConnections{125};
    std::vector<std::string> addNodes;
    std::vector<std::string> connectNodes;
    bool dnsSeed{true};
    
    /// Mining/staking
    bool mining{false};
    bool staking{false};
    int miningThreads{1};
    std::string miningAddress;
    
    /// Whether to check blocks on startup
    bool checkBlocks{true};
    int checkLevel{3};
    
    /// AssumeValid block hash (skip signature validation for blocks before this)
    std::string assumeValidBlock;
};

// ============================================================================
// Node Context - Holds all node state
// ============================================================================

/**
 * NodeContext holds all the components of a running node.
 * 
 * This structure owns all the major subsystems:
 * - Blockchain database
 * - UTXO database  
 * - Chain state manager
 * - Memory pool
 * - P2P network manager
 * - Transaction index (optional)
 */
struct NodeContext {
    // ========================================================================
    // Consensus
    // ========================================================================
    
    /// Consensus parameters for the network
    std::unique_ptr<consensus::Params> params;
    
    // ========================================================================
    // Databases
    // ========================================================================
    
    /// Block storage and index database
    std::unique_ptr<db::BlockDB> blockDB;
    
    /// UTXO database
    std::unique_ptr<db::CoinsViewDB> coinsDB;
    
    /// Transaction index (optional, may be nullptr)
    std::unique_ptr<db::TxIndex> txIndex;
    
    // ========================================================================
    // Chain State
    // ========================================================================
    
    /// Chain state manager (owns active chain and UTXO cache)
    std::unique_ptr<ChainStateManager> chainman;
    
    // ========================================================================
    // Memory Pool
    // ========================================================================
    
    /// Transaction memory pool
    std::unique_ptr<Mempool> mempool;
    
    // ========================================================================
    // Network
    // ========================================================================
    
    /// P2P connection manager
    std::unique_ptr<ConnectionManager> connman;
    
    /// Block synchronizer
    std::unique_ptr<BlockSynchronizer> syncman;
    
    /// Message processor (routes P2P messages to handlers)
    std::unique_ptr<MessageProcessor> msgproc;
    
    /// Address manager (peer discovery and storage)
    std::unique_ptr<AddressManager> addrman;
    
    // ========================================================================
    // State Flags
    // ========================================================================
    
    /// Whether the node is fully initialized
    std::atomic<bool> initialized{false};
    
    /// Whether initial block download is complete
    std::atomic<bool> ibdComplete{false};
    
    /// Whether we're in reindex mode
    bool reindex{false};
    
    // ========================================================================
    // Paths
    // ========================================================================
    
    /// Data directory
    std::filesystem::path dataDir;
    
    /// Blocks directory
    std::filesystem::path blocksDir;
    
    /// Chainstate directory  
    std::filesystem::path chainstateDir;
    
    // ========================================================================
    // Constructor/Destructor
    // ========================================================================
    
    NodeContext() = default;
    ~NodeContext() = default;
    
    // Non-copyable and non-movable (due to atomic members)
    NodeContext(const NodeContext&) = delete;
    NodeContext& operator=(const NodeContext&) = delete;
    NodeContext(NodeContext&&) = delete;
    NodeContext& operator=(NodeContext&&) = delete;
    
    // ========================================================================
    // Convenience Accessors
    // ========================================================================
    
    /// Get the active chain tip
    BlockIndex* GetTip() const {
        return chainman ? chainman->GetActiveTip() : nullptr;
    }
    
    /// Get the active chain height
    int GetHeight() const {
        return chainman ? chainman->GetActiveHeight() : -1;
    }
    
    /// Check if node is ready for operations
    bool IsReady() const {
        return initialized.load() && chainman != nullptr;
    }
};

// ============================================================================
// Node Initialization Functions
// ============================================================================

/**
 * Initialize the node with all subsystems.
 * 
 * This function:
 * 1. Creates data directories if needed
 * 2. Opens BlockDB and CoinsViewDB
 * 3. Loads block index from database
 * 4. Creates ChainStateManager
 * 5. Initializes mempool
 * 6. Starts P2P network (if enabled)
 * 
 * @param node The node context to initialize
 * @param options Initialization options
 * @return true if initialization succeeded
 */
bool InitializeNode(NodeContext& node, const NodeInitOptions& options);

/**
 * Start the P2P network subsystem.
 * Should be called after InitializeNode.
 * 
 * @param node The node context
 * @param options Initialization options
 * @return true if network started successfully
 */
bool StartNetwork(NodeContext& node, const NodeInitOptions& options);

/**
 * Start block synchronization with peers.
 * Should be called after StartNetwork.
 * 
 * @param node The node context
 * @return true if sync started successfully
 */
bool StartSync(NodeContext& node);

/**
 * Shutdown the node, stopping all subsystems.
 * 
 * This function:
 * 1. Stops P2P network
 * 2. Stops mining/staking
 * 3. Flushes mempool
 * 4. Flushes chain state to disk
 * 5. Closes databases
 * 
 * @param node The node context to shutdown
 */
void ShutdownNode(NodeContext& node);

/**
 * Flush node state to disk without shutting down.
 * 
 * @param node The node context
 * @return true if flush succeeded
 */
bool FlushNodeState(NodeContext& node);

// ============================================================================
// Block Index Loading
// ============================================================================

/**
 * Load the block index from the database into the chain state manager.
 * 
 * @param node The node context
 * @return Number of block index entries loaded, or -1 on error
 */
int LoadBlockIndex(NodeContext& node);

/**
 * Verify block index integrity.
 * 
 * @param node The node context
 * @param checkLevel Level of checks (1-4, higher = more thorough)
 * @return true if verification passed
 */
bool VerifyBlockIndex(const NodeContext& node, int checkLevel);

/**
 * Find and activate the best chain.
 * 
 * @param node The node context
 * @return true if successful
 */
bool ActivateBestChain(NodeContext& node);

// ============================================================================
// Shutdown Control
// ============================================================================

/**
 * Request a graceful shutdown of the node.
 * This is thread-safe and can be called from any context.
 */
void RequestShutdown();

/**
 * Check if shutdown has been requested.
 * @return true if shutdown is in progress or requested
 */
bool ShutdownRequested();

} // namespace shurium

#endif // SHURIUM_NODE_CONTEXT_H
