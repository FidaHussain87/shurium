// SHURIUM - Chain State Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements chain state management including UTXO handling,
// block connection/disconnection, and chain reorganization.

#include <shurium/chain/chainstate.h>
#include <shurium/chain/blockindex.h>
#include <shurium/consensus/validation.h>
#include <shurium/core/block.h>
#include <shurium/util/time.h>

#include <algorithm>

namespace shurium {

// ============================================================================
// ChainState Implementation
// ============================================================================

ChainState::ChainState(BlockMap& blockIndex,
                       const consensus::Params& params,
                       CoinsView* coinsDB)
    : m_blockIndex(blockIndex)
    , m_params(params)
    , m_coinsDB(coinsDB) {
    
    // Create coins cache on top of backing storage
    if (coinsDB) {
        m_coins = std::make_unique<CoinsViewCache>(coinsDB);
    }
}

bool ChainState::Initialize() {
    if (m_initialized) {
        return true;
    }
    
    // Load the best block from the coins database
    if (m_coins) {
        BlockHash bestBlock = m_coins->GetBestBlock();
        
        if (!bestBlock.IsNull()) {
            // Find the block in our index
            auto it = m_blockIndex.find(bestBlock);
            if (it != m_blockIndex.end()) {
                // Build the chain from genesis to this block
                BlockIndex* pindex = it->second.get();
                while (pindex != nullptr) {
                    m_chain.SetTip(pindex);
                    pindex = pindex->pprev;
                }
            }
        }
    }
    
    m_initialized = true;
    return true;
}

// ============================================================================
// Block Connection (Internal)
// ============================================================================

bool ChainState::ConnectBlock(const Block& block, BlockIndex* pindex,
                              CoinsViewCache& view, BlockUndo& blockundo) {
    // Verify the block first
    consensus::ValidationState state;
    if (!consensus::CheckBlock(block, state, m_params)) {
        return false;
    }
    
    // Check that all transactions' inputs are available
    for (size_t i = 1; i < block.vtx.size(); ++i) {  // Skip coinbase
        const Transaction& tx = *block.vtx[i];
        
        TxUndo txundo;
        for (const auto& txin : tx.vin) {
            auto coin = view.GetCoin(txin.prevout);
            if (!coin.has_value()) {
                return false;  // Input not found
            }
            
            // Check coinbase maturity
            if (coin->IsCoinBase() && !coin->IsMature(pindex->nHeight)) {
                return false;  // Trying to spend immature coinbase
            }
            
            // Save for undo
            txundo.vprevout.push_back(*coin);
            
            // Spend the coin
            view.SpendCoin(txin.prevout);
        }
        
        blockundo.vtxundo.push_back(std::move(txundo));
    }
    
    // Add outputs from all transactions (including coinbase)
    for (size_t i = 0; i < block.vtx.size(); ++i) {
        const Transaction& tx = *block.vtx[i];
        TxHash txhash = tx.GetHash();
        
        for (size_t j = 0; j < tx.vout.size(); ++j) {
            OutPoint outpoint(txhash, static_cast<uint32_t>(j));
            Coin coin(tx.vout[j], pindex->nHeight, i == 0);
            view.AddCoin(outpoint, std::move(coin), false);
        }
    }
    
    // Update best block
    view.SetBestBlock(pindex->GetBlockHash());
    
    return true;
}

bool ChainState::DisconnectBlock(const Block& block, const BlockIndex* pindex,
                                 CoinsViewCache& view, const BlockUndo& blockundo) {
    // Process transactions in reverse order
    for (size_t i = block.vtx.size(); i-- > 0; ) {
        const Transaction& tx = *block.vtx[i];
        TxHash txhash = tx.GetHash();
        
        // Remove outputs
        for (size_t j = 0; j < tx.vout.size(); ++j) {
            OutPoint outpoint(txhash, static_cast<uint32_t>(j));
            view.SpendCoin(outpoint);
        }
        
        // Restore inputs (skip coinbase)
        if (i > 0) {
            const TxUndo& txundo = blockundo.vtxundo[i - 1];
            for (size_t k = 0; k < tx.vin.size(); ++k) {
                Coin restoredCoin(txundo.vprevout[k]);
                view.AddCoin(tx.vin[k].prevout, std::move(restoredCoin), true);
            }
        }
    }
    
    // Update best block to previous
    if (pindex->pprev) {
        view.SetBestBlock(pindex->pprev->GetBlockHash());
    } else {
        view.SetBestBlock(BlockHash());
    }
    
    return true;
}

// ============================================================================
// Block Connection (Public)
// ============================================================================

ConnectResult ChainState::ConnectBlock(const Block& block, BlockIndex* pindex,
                                       BlockUndo& blockundo) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    if (!m_coins) {
        return ConnectResult::FAILED;
    }
    
    // Create a cache for this operation
    CoinsViewCache viewCache(m_coins.get());
    
    if (!ConnectBlock(block, pindex, viewCache, blockundo)) {
        return ConnectResult::INVALID;
    }
    
    // Flush changes to the underlying cache
    viewCache.Flush();
    
    // Update chain
    m_chain.SetTip(pindex);
    
    return ConnectResult::OK;
}

ConnectResult ChainState::DisconnectTip(const Block& block, const BlockUndo& blockundo) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    if (!m_coins) {
        return ConnectResult::FAILED;
    }
    
    BlockIndex* pindex = m_chain.Tip();
    if (!pindex) {
        return ConnectResult::FAILED;
    }
    
    // Verify block matches tip
    if (block.GetHash() != pindex->GetBlockHash()) {
        return ConnectResult::FAILED;
    }
    
    // Create a cache for this operation
    CoinsViewCache viewCache(m_coins.get());
    
    if (!DisconnectBlock(block, pindex, viewCache, blockundo)) {
        return ConnectResult::INVALID;
    }
    
    // Flush changes
    viewCache.Flush();
    
    // Update chain
    m_chain.SetTip(pindex->pprev);
    
    return ConnectResult::OK;
}

bool ChainState::ActivateBestChain(BlockIndex* pindexMostWork) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    // If no specific target, use current best
    if (!pindexMostWork) {
        // Find the block with most work
        // For now, just keep current tip
        return true;
    }
    
    // Check if we're already at the best chain
    if (m_chain.Tip() == pindexMostWork) {
        return true;
    }
    
    // Find common ancestor
    const BlockIndex* pindexFork = m_chain.FindFork(pindexMostWork);
    if (!pindexFork) {
        return false;
    }
    
    // Disconnect blocks to get to the fork point
    while (m_chain.Tip() != pindexFork) {
        // Would need block data and undo data to disconnect
        // For now, just report failure if reorganization needed
        return false;
    }
    
    // Connect blocks from fork to new tip
    std::vector<BlockIndex*> toConnect;
    BlockIndex* pindex = pindexMostWork;
    while (pindex != pindexFork) {
        toConnect.push_back(pindex);
        pindex = pindex->pprev;
    }
    
    // Connect in forward order
    for (auto it = toConnect.rbegin(); it != toConnect.rend(); ++it) {
        // Would need block data to connect
        // For now, just update chain tip
        m_chain.SetTip(*it);
    }
    
    return true;
}

bool ChainState::FlushStateToDisk() {
    std::lock_guard<std::mutex> lock(m_cs);
    
    if (m_coins) {
        return m_coins->Flush();
    }
    
    return true;
}

// ============================================================================
// ChainStateManager Implementation
// ============================================================================

ChainStateManager::ChainStateManager()
    : m_params(consensus::Params::Main()) {
}

ChainStateManager::ChainStateManager(const consensus::Params& params)
    : m_params(params) {
}

bool ChainStateManager::Initialize(CoinsView* coinsDB) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    // Create the active chainstate
    m_activeChainState = std::make_unique<ChainState>(
        m_blockIndex, m_params, coinsDB);
    
    return m_activeChainState->Initialize();
}

BlockIndex* ChainStateManager::LookupBlockIndex(const BlockHash& hash) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    auto it = m_blockIndex.find(hash);
    if (it == m_blockIndex.end()) {
        return nullptr;
    }
    return it->second.get();
}

const BlockIndex* ChainStateManager::LookupBlockIndex(const BlockHash& hash) const {
    std::lock_guard<std::mutex> lock(m_cs);
    
    auto it = m_blockIndex.find(hash);
    if (it == m_blockIndex.end()) {
        return nullptr;
    }
    return it->second.get();
}

BlockIndex* ChainStateManager::AddBlockIndex(const BlockHash& hash, 
                                              const BlockHeader& header) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    // Check if already exists
    auto it = m_blockIndex.find(hash);
    if (it != m_blockIndex.end()) {
        return it->second.get();
    }
    
    // Create new index entry
    auto pindex = std::make_unique<BlockIndex>(header);
    
    // Set the block hash pointer
    auto [insertIt, inserted] = m_blockIndex.emplace(hash, std::move(pindex));
    if (!inserted) {
        return nullptr;
    }
    
    BlockIndex* pnewIndex = insertIt->second.get();
    
    // Link to previous block
    if (!header.hashPrevBlock.IsNull()) {
        auto prevIt = m_blockIndex.find(header.hashPrevBlock);
        if (prevIt != m_blockIndex.end()) {
            pnewIndex->pprev = prevIt->second.get();
            pnewIndex->nHeight = pnewIndex->pprev->nHeight + 1;
        }
    } else {
        pnewIndex->nHeight = 0;  // Genesis block
    }
    
    // Update best header if this has more work
    if (!m_bestHeader || pnewIndex->nChainWork > m_bestHeader->nChainWork) {
        m_bestHeader = pnewIndex;
    }
    
    return pnewIndex;
}

BlockIndex* ChainStateManager::ProcessBlockHeader(const BlockHeader& header) {
    // Compute hash
    BlockHash hash = header.GetHash();
    
    // Validate header
    consensus::ValidationState state;
    Block tempBlock;
    static_cast<BlockHeader&>(tempBlock) = header;
    tempBlock.vtx.clear();
    
    // Basic header checks (skip full block validation)
    if (header.nBits == 0) {
        return nullptr;  // Invalid difficulty
    }
    
    // Check proof of work
    if (!consensus::CheckProofOfWork(hash, header.nBits, m_params)) {
        return nullptr;
    }
    
    // Add to index
    return AddBlockIndex(hash, header);
}

bool ChainStateManager::ProcessNewBlock(const Block& block, bool /* fForceProcessing */) {
    // Process header first
    BlockIndex* pindex = ProcessBlockHeader(block.GetBlockHeader());
    if (!pindex) {
        return false;
    }
    
    // Validate the full block
    consensus::ValidationState state;
    if (!consensus::CheckBlock(block, state, m_params)) {
        return false;
    }
    
    // Check if this extends the best chain
    if (!m_activeChainState) {
        return false;
    }
    
    BlockIndex* tip = m_activeChainState->GetTip();
    
    // If this block's parent is the current tip, connect it
    if (tip && pindex->pprev == tip) {
        BlockUndo undo;
        ConnectResult result = m_activeChainState->ConnectBlock(block, pindex, undo);
        return IsSuccess(result);
    }
    
    // Otherwise, this might cause a reorganization
    // For now, just accept if it extends from genesis or current chain
    return true;
}

} // namespace shurium
