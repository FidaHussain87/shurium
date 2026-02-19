// SHURIUM - Chain State Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the chain state manager that coordinates
// the UTXO set, block index, and active chain.

#ifndef SHURIUM_CHAIN_CHAINSTATE_H
#define SHURIUM_CHAIN_CHAINSTATE_H

#include "shurium/chain/coins.h"
#include "shurium/chain/blockindex.h"
#include "shurium/consensus/params.h"
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>

namespace shurium {

// Forward declarations
namespace db { class BlockDB; }

// ============================================================================
// BlockUndo - Undo information for disconnecting a block
// ============================================================================

/**
 * Undo information for a single transaction.
 * Stores the coins that were spent by the transaction's inputs.
 */
struct TxUndo {
    std::vector<Coin> vprevout;
    
    void Clear() { vprevout.clear(); }
    bool empty() const { return vprevout.empty(); }
    size_t size() const { return vprevout.size(); }
};

/// Serialization for TxUndo
template<typename Stream>
void Serialize(Stream& s, const TxUndo& txundo) {
    WriteCompactSize(s, txundo.vprevout.size());
    for (const auto& coin : txundo.vprevout) {
        Serialize(s, coin);
    }
}

template<typename Stream>
void Unserialize(Stream& s, TxUndo& txundo) {
    size_t count = ReadCompactSize(s);
    txundo.vprevout.resize(count);
    for (auto& coin : txundo.vprevout) {
        Unserialize(s, coin);
    }
}

/**
 * Undo information for a complete block.
 * Used to disconnect blocks during chain reorganization.
 */
struct BlockUndo {
    std::vector<TxUndo> vtxundo;  // Undo info for each transaction (except coinbase)
    
    void Clear() { vtxundo.clear(); }
    bool empty() const { return vtxundo.empty(); }
    size_t size() const { return vtxundo.size(); }
};

/// Serialization for BlockUndo
template<typename Stream>
void Serialize(Stream& s, const BlockUndo& blockundo) {
    WriteCompactSize(s, blockundo.vtxundo.size());
    for (const auto& txundo : blockundo.vtxundo) {
        Serialize(s, txundo);
    }
}

template<typename Stream>
void Unserialize(Stream& s, BlockUndo& blockundo) {
    size_t count = ReadCompactSize(s);
    blockundo.vtxundo.resize(count);
    for (auto& txundo : blockundo.vtxundo) {
        Unserialize(s, txundo);
    }
}

// ============================================================================
// ConnectResult - Result of connecting a block
// ============================================================================

/**
 * Result codes for block connection/disconnection operations.
 */
enum class ConnectResult {
    OK,                    // Success
    INVALID,              // Block is invalid
    FAILED,               // Operation failed (recoverable)
    CONSENSUS_ERROR,      // Consensus rule violation
    MISSING_INPUTS,       // Transaction inputs not found
    PREMATURE_SPEND,      // Trying to spend immature coinbase
    DOUBLE_SPEND,         // Output already spent
};

inline bool IsSuccess(ConnectResult result) {
    return result == ConnectResult::OK;
}

// ============================================================================
// ChainState - Manages the state of a single blockchain
// ============================================================================

/**
 * ChainState manages a single blockchain's state.
 * 
 * This includes:
 * - The active chain (from genesis to tip)
 * - The UTXO set
 * - Block connection/disconnection
 * - Validation state
 */
class ChainState {
private:
    /// The active chain
    Chain m_chain;
    
    /// The UTXO view (cached)
    std::unique_ptr<CoinsViewCache> m_coins;
    
    /// The backing UTXO storage
    CoinsView* m_coinsDB;
    
    /// Block index map (all known blocks)
    BlockMap& m_blockIndex;
    
    /// Consensus parameters
    const consensus::Params& m_params;
    
    /// Mutex for thread-safe access
    mutable std::mutex m_cs;
    
    /// Whether this chainstate has been initialized
    std::atomic<bool> m_initialized{false};
    
    // Internal helpers
    bool ConnectBlock(const Block& block, BlockIndex* pindex, 
                      CoinsViewCache& view, BlockUndo& blockundo);
    bool DisconnectBlock(const Block& block, const BlockIndex* pindex,
                         CoinsViewCache& view, const BlockUndo& blockundo);
    
public:
    /**
     * Construct a new ChainState.
     * 
     * @param blockIndex Reference to the shared block index map
     * @param params Consensus parameters
     * @param coinsDB Backing UTXO storage (takes ownership via cache)
     */
    ChainState(BlockMap& blockIndex, 
               const consensus::Params& params,
               CoinsView* coinsDB);
    
    ~ChainState() = default;
    
    // Prevent copies
    ChainState(const ChainState&) = delete;
    ChainState& operator=(const ChainState&) = delete;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /// Initialize the chainstate, loading the active chain
    bool Initialize();
    
    /// Check if initialized
    bool IsInitialized() const { return m_initialized; }
    
    // ========================================================================
    // Chain Access
    // ========================================================================
    
    /// Get the active chain
    const Chain& GetChain() const { return m_chain; }
    Chain& GetChain() { return m_chain; }
    
    /// Get the chain tip
    BlockIndex* GetTip() const {
        std::lock_guard<std::mutex> lock(m_cs);
        return m_chain.Tip();
    }
    
    /// Get chain height
    int GetHeight() const {
        std::lock_guard<std::mutex> lock(m_cs);
        return m_chain.Height();
    }
    
    /// Check if a block is in the active chain
    bool IsInActiveChain(const BlockIndex* pindex) const {
        std::lock_guard<std::mutex> lock(m_cs);
        return m_chain.Contains(pindex);
    }
    
    // ========================================================================
    // UTXO Access
    // ========================================================================
    
    /// Get the UTXO view
    CoinsViewCache& GetCoins() { return *m_coins; }
    const CoinsViewCache& GetCoins() const { return *m_coins; }
    
    /// Check if a UTXO exists
    bool HaveCoins(const OutPoint& outpoint) const {
        std::lock_guard<std::mutex> lock(m_cs);
        return m_coins->HaveCoin(outpoint);
    }
    
    /// Get a UTXO
    std::optional<Coin> GetCoin(const OutPoint& outpoint) const {
        std::lock_guard<std::mutex> lock(m_cs);
        return m_coins->GetCoin(outpoint);
    }
    
    // ========================================================================
    // Block Operations
    // ========================================================================
    
    /**
     * Connect a block to the chain.
     * 
     * @param block The block to connect
     * @param pindex The block's index entry
     * @param blockundo Output: undo information for disconnecting
     * @return Result of the connection attempt
     */
    ConnectResult ConnectBlock(const Block& block, BlockIndex* pindex,
                               BlockUndo& blockundo);
    
    /**
     * Disconnect the tip block from the chain.
     * 
     * @param block The block to disconnect
     * @param blockundo The undo information for this block
     * @return Result of the disconnection attempt
     */
    ConnectResult DisconnectTip(const Block& block, const BlockUndo& blockundo);
    
    /**
     * Activate the best chain, connecting/disconnecting as needed.
     * 
     * @param pindexMostWork The block with the most work to activate
     * @return True if successful
     */
    bool ActivateBestChain(BlockIndex* pindexMostWork = nullptr);
    
    // ========================================================================
    // Validation
    // ========================================================================
    
    /**
     * Check if a transaction's inputs are available.
     */
    bool HaveInputs(const Transaction& tx) const {
        std::lock_guard<std::mutex> lock(m_cs);
        return m_coins->HaveInputs(tx);
    }
    
    /**
     * Get the total value of a transaction's inputs.
     */
    Amount GetValueIn(const Transaction& tx) const {
        std::lock_guard<std::mutex> lock(m_cs);
        return m_coins->GetValueIn(tx);
    }
    
    /**
     * Check if a coinbase output is mature enough to spend.
     */
    bool IsCoinbaseMature(const Coin& coin) const {
        int currentHeight = GetHeight();
        return coin.IsMature(currentHeight);
    }
    
    // ========================================================================
    // Flush & Persistence
    // ========================================================================
    
    /// Flush UTXO cache to backing storage
    bool FlushStateToDisk();
    
    /// Get memory usage statistics
    size_t GetCoinsCacheSize() const { return m_coins->GetCacheSize(); }
    size_t GetCoinsCacheUsage() const { return m_coins->GetCacheUsage(); }
};

// ============================================================================
// ChainStateManager - Manages multiple chain states (for AssumeUTXO)
// ============================================================================

/**
 * Manages one or more ChainState objects.
 * 
 * In the simple case, there's just one chainstate for the active chain.
 * With AssumeUTXO, there can be a background validation chainstate.
 */
class ChainStateManager {
private:
    /// Block index map (shared by all chainstates)
    BlockMap m_blockIndex;
    
    /// The active chainstate
    std::unique_ptr<ChainState> m_activeChainState;
    
    /// Consensus parameters
    consensus::Params m_params;
    
    /// Best known block (may be ahead of active tip during sync)
    BlockIndex* m_bestHeader{nullptr};
    
    /// Block database for storing blocks (optional, not owned)
    db::BlockDB* m_blockdb{nullptr};
    
    /// Mutex for thread-safe access
    mutable std::mutex m_cs;
    
public:
    ChainStateManager();
    explicit ChainStateManager(const consensus::Params& params);
    ~ChainStateManager() = default;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /// Initialize with backing UTXO storage
    bool Initialize(CoinsView* coinsDB);
    
    /// Set the block database for storing blocks
    void SetBlockDB(db::BlockDB* blockdb) { m_blockdb = blockdb; }
    
    /// Get the block database
    db::BlockDB* GetBlockDB() const { return m_blockdb; }
    
    // ========================================================================
    // Block Index
    // ========================================================================
    
    /// Get the block index map
    BlockMap& GetBlockIndex() { return m_blockIndex; }
    const BlockMap& GetBlockIndex() const { return m_blockIndex; }
    
    /// Look up a block by hash
    BlockIndex* LookupBlockIndex(const BlockHash& hash);
    const BlockIndex* LookupBlockIndex(const BlockHash& hash) const;
    
    /// Add a new block index entry
    BlockIndex* AddBlockIndex(const BlockHash& hash, const BlockHeader& header);
    
    /// Get the best header
    BlockIndex* GetBestHeader() const {
        std::lock_guard<std::mutex> lock(m_cs);
        return m_bestHeader;
    }
    
    /// Update the best header
    void SetBestHeader(BlockIndex* pindex) {
        std::lock_guard<std::mutex> lock(m_cs);
        m_bestHeader = pindex;
    }
    
    // ========================================================================
    // Chain State Access
    // ========================================================================
    
    /// Get the active chainstate
    ChainState& GetActiveChainState() { return *m_activeChainState; }
    const ChainState& GetActiveChainState() const { return *m_activeChainState; }
    
    /// Get the active chain
    Chain& GetActiveChain() { return m_activeChainState->GetChain(); }
    const Chain& GetActiveChain() const { return m_activeChainState->GetChain(); }
    
    /// Get the active tip
    BlockIndex* GetActiveTip() const {
        return m_activeChainState ? m_activeChainState->GetTip() : nullptr;
    }
    
    /// Get the active chain height
    int GetActiveHeight() const {
        return m_activeChainState ? m_activeChainState->GetHeight() : -1;
    }
    
    // ========================================================================
    // Consensus
    // ========================================================================
    
    /// Get consensus parameters
    const consensus::Params& GetParams() const { return m_params; }
    
    // ========================================================================
    // Block Processing
    // ========================================================================
    
    /**
     * Process a new block header.
     * Adds to block index if valid.
     */
    BlockIndex* ProcessBlockHeader(const BlockHeader& header);
    
    /**
     * Process a new block.
     * Validates and potentially connects to the active chain.
     */
    bool ProcessNewBlock(const Block& block, bool fForceProcessing = false);
    
    /**
     * Activate the best chain.
     */
    bool ActivateBestChain() {
        return m_activeChainState && m_activeChainState->ActivateBestChain();
    }
};

} // namespace shurium

#endif // SHURIUM_CHAIN_CHAINSTATE_H
