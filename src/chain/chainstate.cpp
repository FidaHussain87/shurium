// SHURIUM - Chain State Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/chain/chainstate.h"
#include "shurium/consensus/validation.h"
#include "shurium/script/interpreter.h"
#include <cassert>
#include <algorithm>

namespace shurium {

// ============================================================================
// Script Verification Helper
// ============================================================================

/**
 * Verify all input scripts for a transaction.
 * 
 * @param tx The transaction to verify
 * @param coinsView The UTXO set to look up previous outputs
 * @param flags Script verification flags
 * @return true if all scripts verify successfully
 */
static bool VerifyTransactionScripts(const Transaction& tx,
                                      const CoinsViewCache& coinsView,
                                      ScriptFlags flags) {
    // Coinbase transactions have no inputs to verify
    if (tx.IsCoinBase()) {
        return true;
    }
    
    // Verify each input
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        const TxIn& txin = tx.vin[i];
        
        // Get the previous output being spent
        const Coin& coin = coinsView.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            return false;  // Should have been caught earlier
        }
        
        // Get the amount for signature checking
        Amount amount = coin.GetAmount();
        
        // Create signature checker for this input
        TransactionSignatureChecker checker(&tx, static_cast<unsigned int>(i), amount);
        
        // Verify the script
        ScriptError error;
        if (!VerifyScript(txin.scriptSig, 
                          coin.GetScriptPubKey(), 
                          flags, 
                          checker, 
                          &error)) {
            // Script verification failed
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// ChainState Implementation
// ============================================================================

ChainState::ChainState(BlockMap& blockIndex,
                       const consensus::Params& params,
                       CoinsView* coinsDB)
    : m_blockIndex(blockIndex)
    , m_coinsDB(coinsDB)
    , m_params(params) {
    m_coins = std::make_unique<CoinsViewCache>(coinsDB);
}

bool ChainState::Initialize() {
    if (m_initialized) return true;
    
    // Find the best block from the UTXO database
    BlockHash bestBlock = m_coins->GetBestBlock();
    
    if (bestBlock.IsNull()) {
        // No best block - this is a fresh database
        // Initialize with genesis block if available
        m_initialized = true;
        return true;
    }
    
    // Find the block index for the best block
    auto it = m_blockIndex.find(bestBlock);
    if (it == m_blockIndex.end()) {
        // Best block not in index - database may be corrupted
        return false;
    }
    
    // Set the chain to this tip
    m_chain.SetTip(it->second.get());
    
    m_initialized = true;
    return true;
}

ConnectResult ChainState::ConnectBlock(const Block& block, BlockIndex* pindex,
                                        BlockUndo& blockundo) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    // Verify this block connects to our current tip
    if (m_chain.Tip() && m_chain.Tip() != pindex->pprev) {
        return ConnectResult::FAILED;
    }
    
    // Determine script verification flags based on block height
    // Use mandatory flags for block validation (less strict than mempool)
    ScriptFlags scriptFlags = ScriptFlags::MANDATORY_VERIFY_FLAGS;
    
    // First pass: Verify all transaction scripts BEFORE spending coins
    // This ensures we have access to the original outputs for signature verification
    for (size_t i = 0; i < block.vtx.size(); ++i) {
        const Transaction& tx = *block.vtx[i];
        
        if (!tx.IsCoinBase()) {
            // Verify all input scripts for this transaction
            if (!VerifyTransactionScripts(tx, *m_coins, scriptFlags)) {
                return ConnectResult::CONSENSUS_ERROR;
            }
        }
    }
    
    // Second pass: Update UTXO set
    // Prepare undo information
    blockundo.vtxundo.resize(block.vtx.size() - 1);  // All but coinbase
    
    // Process each transaction
    for (size_t i = 0; i < block.vtx.size(); ++i) {
        const Transaction& tx = *block.vtx[i];
        
        if (!tx.IsCoinBase()) {
            // Check all inputs exist and collect undo info
            TxUndo& txundo = blockundo.vtxundo[i - 1];
            txundo.vprevout.resize(tx.vin.size());
            
            for (size_t j = 0; j < tx.vin.size(); ++j) {
                const OutPoint& prevout = tx.vin[j].prevout;
                
                // Get the coin being spent
                const Coin& coin = m_coins->AccessCoin(prevout);
                if (coin.IsSpent()) {
                    return ConnectResult::MISSING_INPUTS;
                }
                
                // Check coinbase maturity
                if (coin.IsCoinBase()) {
                    if (!coin.IsMature(pindex->nHeight)) {
                        return ConnectResult::PREMATURE_SPEND;
                    }
                }
                
                // Save for undo
                txundo.vprevout[j] = coin;
                
                // Spend the coin
                if (!m_coins->SpendCoin(prevout)) {
                    return ConnectResult::DOUBLE_SPEND;
                }
            }
        }
        
        // Add outputs
        m_coins->AddTransaction(tx, pindex->nHeight);
    }
    
    // Update best block
    m_coins->SetBestBlock(pindex->GetBlockHash());
    
    // Update the chain
    m_chain.SetTip(pindex);
    
    // Update block index status
    pindex->RaiseValidity(BlockStatus::VALID_SCRIPTS);
    pindex->nStatus = pindex->nStatus | BlockStatus::HAVE_DATA;
    
    return ConnectResult::OK;
}

bool ChainState::ConnectBlock(const Block& block, BlockIndex* pindex,
                              CoinsViewCache& view, BlockUndo& blockundo) {
    // Internal helper - called with lock held
    
    // Determine script verification flags
    ScriptFlags scriptFlags = ScriptFlags::MANDATORY_VERIFY_FLAGS;
    
    // First pass: Verify all transaction scripts
    for (size_t i = 0; i < block.vtx.size(); ++i) {
        const Transaction& tx = *block.vtx[i];
        
        if (!tx.IsCoinBase()) {
            if (!VerifyTransactionScripts(tx, view, scriptFlags)) {
                return false;
            }
        }
    }
    
    // Second pass: Update UTXO set
    blockundo.vtxundo.resize(block.vtx.size() - 1);
    
    for (size_t i = 0; i < block.vtx.size(); ++i) {
        const Transaction& tx = *block.vtx[i];
        
        if (!tx.IsCoinBase()) {
            TxUndo& txundo = blockundo.vtxundo[i - 1];
            txundo.vprevout.resize(tx.vin.size());
            
            for (size_t j = 0; j < tx.vin.size(); ++j) {
                const OutPoint& prevout = tx.vin[j].prevout;
                const Coin& coin = view.AccessCoin(prevout);
                
                if (coin.IsSpent()) return false;
                if (coin.IsCoinBase() && !coin.IsMature(pindex->nHeight)) {
                    return false;
                }
                
                txundo.vprevout[j] = coin;
                if (!view.SpendCoin(prevout)) return false;
            }
        }
        
        view.AddTransaction(tx, pindex->nHeight);
    }
    
    view.SetBestBlock(pindex->GetBlockHash());
    return true;
}

ConnectResult ChainState::DisconnectTip(const Block& block, const BlockUndo& blockundo) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    BlockIndex* pindex = m_chain.Tip();
    if (!pindex) {
        return ConnectResult::FAILED;
    }
    
    // Verify block matches tip
    if (pindex->GetBlockHash() != block.GetHash()) {
        return ConnectResult::FAILED;
    }
    
    // Process transactions in reverse order
    for (int i = static_cast<int>(block.vtx.size()) - 1; i >= 0; --i) {
        const Transaction& tx = *block.vtx[i];
        TxHash txhash = tx.GetHash();
        
        // Remove outputs
        for (size_t j = 0; j < tx.vout.size(); ++j) {
            if (!tx.vout[j].IsNull()) {
                m_coins->SpendCoin(OutPoint(txhash, static_cast<uint32_t>(j)));
            }
        }
        
        // Restore inputs (except for coinbase)
        if (!tx.IsCoinBase()) {
            const TxUndo& txundo = blockundo.vtxundo[i - 1];
            for (size_t j = 0; j < tx.vin.size(); ++j) {
                m_coins->AddCoin(tx.vin[j].prevout, 
                                 Coin(txundo.vprevout[j]), 
                                 true);
            }
        }
    }
    
    // Update best block to previous
    if (pindex->pprev) {
        m_coins->SetBestBlock(pindex->pprev->GetBlockHash());
        m_chain.SetTip(pindex->pprev);
    } else {
        m_coins->SetBestBlock(BlockHash());
        m_chain.Clear();
    }
    
    return ConnectResult::OK;
}

bool ChainState::DisconnectBlock(const Block& block, const BlockIndex* pindex,
                                  CoinsViewCache& view, const BlockUndo& blockundo) {
    // Internal helper
    for (int i = static_cast<int>(block.vtx.size()) - 1; i >= 0; --i) {
        const Transaction& tx = *block.vtx[i];
        TxHash txhash = tx.GetHash();
        
        for (size_t j = 0; j < tx.vout.size(); ++j) {
            if (!tx.vout[j].IsNull()) {
                view.SpendCoin(OutPoint(txhash, static_cast<uint32_t>(j)));
            }
        }
        
        if (!tx.IsCoinBase()) {
            const TxUndo& txundo = blockundo.vtxundo[i - 1];
            for (size_t j = 0; j < tx.vin.size(); ++j) {
                view.AddCoin(tx.vin[j].prevout, Coin(txundo.vprevout[j]), true);
            }
        }
    }
    
    return true;
}

bool ChainState::ActivateBestChain(BlockIndex* pindexMostWork) {
    std::lock_guard<std::mutex> lock(m_cs);
    
    // If no target specified, find the best chain
    if (!pindexMostWork) {
        // Find the block with most work
        uint64_t bestWork = 0;
        for (const auto& [hash, pindex] : m_blockIndex) {
            if (pindex->nChainWork > bestWork && 
                pindex->IsValid(BlockStatus::VALID_TRANSACTIONS)) {
                bestWork = pindex->nChainWork;
                pindexMostWork = pindex.get();
            }
        }
    }
    
    if (!pindexMostWork) return true;  // Nothing to do
    
    // Check if we're already at the best chain
    if (m_chain.Tip() == pindexMostWork) return true;
    
    // Find fork point with current chain
    const BlockIndex* pfork = m_chain.FindFork(pindexMostWork);
    
    // Disconnect blocks back to fork point
    while (m_chain.Tip() && m_chain.Tip() != pfork) {
        // In production, we'd load the block and undo data here
        // For now, just update the chain pointer
        m_chain.SetTip(m_chain.Tip()->pprev);
    }
    
    // Connect blocks from fork to new tip
    std::vector<BlockIndex*> toConnect;
    BlockIndex* pindex = pindexMostWork;
    while (pindex && pindex != pfork) {
        toConnect.push_back(pindex);
        pindex = pindex->pprev;
    }
    
    // Connect in forward order
    for (auto it = toConnect.rbegin(); it != toConnect.rend(); ++it) {
        // In production, we'd load the block and call ConnectBlock
        // For now, just update the chain
        m_chain.SetTip(*it);
    }
    
    return true;
}

bool ChainState::FlushStateToDisk() {
    std::lock_guard<std::mutex> lock(m_cs);
    return m_coins->Flush();
}

// ============================================================================
// ChainStateManager Implementation
// ============================================================================

ChainStateManager::ChainStateManager()
    : m_params(consensus::Params::Main()) {}

ChainStateManager::ChainStateManager(const consensus::Params& params)
    : m_params(params) {}

bool ChainStateManager::Initialize(CoinsView* coinsDB) {
    m_activeChainState = std::make_unique<ChainState>(
        m_blockIndex, m_params, coinsDB);
    
    return m_activeChainState->Initialize();
}

BlockIndex* ChainStateManager::LookupBlockIndex(const BlockHash& hash) {
    auto it = m_blockIndex.find(hash);
    return (it != m_blockIndex.end()) ? it->second.get() : nullptr;
}

const BlockIndex* ChainStateManager::LookupBlockIndex(const BlockHash& hash) const {
    auto it = m_blockIndex.find(hash);
    return (it != m_blockIndex.end()) ? it->second.get() : nullptr;
}

BlockIndex* ChainStateManager::AddBlockIndex(const BlockHash& hash, 
                                              const BlockHeader& header) {
    // Check if already exists
    auto it = m_blockIndex.find(hash);
    if (it != m_blockIndex.end()) {
        return it->second.get();
    }
    
    // Create new index entry
    auto pindex = std::make_unique<BlockIndex>(header);
    BlockIndex* pindexNew = pindex.get();
    
    // Set up parent pointer
    if (!header.hashPrevBlock.IsNull()) {
        auto pit = m_blockIndex.find(BlockHash(header.hashPrevBlock));
        if (pit != m_blockIndex.end()) {
            pindexNew->pprev = pit->second.get();
            pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        }
    }
    
    // Calculate chain work
    pindexNew->nChainWork = (pindexNew->pprev ? pindexNew->pprev->nChainWork : 0) +
                            GetBlockProof(header.nBits);
    
    // Calculate max time
    pindexNew->nTimeMax = pindexNew->pprev ? 
        std::max(pindexNew->pprev->nTimeMax, header.nTime) : header.nTime;
    
    // Insert into map
    auto [insertIt, inserted] = m_blockIndex.emplace(hash, std::move(pindex));
    
    // Set the hash pointer
    insertIt->second->phashBlock = &insertIt->first;
    
    // Build skip pointer
    insertIt->second->BuildSkip();
    
    return insertIt->second.get();
}

BlockIndex* ChainStateManager::ProcessBlockHeader(const BlockHeader& header) {
    BlockHash hash = header.GetHash();
    
    // Check if we already have this block
    BlockIndex* pindex = LookupBlockIndex(hash);
    if (pindex) {
        return pindex;
    }
    
    // Basic header validation (version check, etc.)
    // Note: Full validation requires CheckBlockHeader with Block, but for
    // headers-first sync we do minimal validation here
    if (header.nVersion < 1) {
        return nullptr;  // Invalid version
    }
    
    // Check we have the parent
    if (!header.hashPrevBlock.IsNull()) {
        BlockIndex* pprev = LookupBlockIndex(BlockHash(header.hashPrevBlock));
        if (!pprev) {
            // Missing parent - could request it
            return nullptr;
        }
    }
    
    // Add to index
    pindex = AddBlockIndex(hash, header);
    
    // Update best header if this has more work
    if (!m_bestHeader || pindex->nChainWork > m_bestHeader->nChainWork) {
        m_bestHeader = pindex;
    }
    
    return pindex;
}

bool ChainStateManager::ProcessNewBlock(const Block& block, bool fForceProcessing) {
    // Process the header first
    BlockIndex* pindex = ProcessBlockHeader(block.GetBlockHeader());
    if (!pindex) {
        return false;
    }
    
    // Check if we already have this block data
    if (pindex->HaveData() && !fForceProcessing) {
        return true;
    }
    
    // Validate the full block
    consensus::ValidationState state;
    if (!consensus::CheckBlock(block, state, m_params)) {
        pindex->nStatus = pindex->nStatus | BlockStatus::FAILED_VALID;
        return false;
    }
    
    // Mark as having valid transactions
    pindex->RaiseValidity(BlockStatus::VALID_TRANSACTIONS);
    pindex->nTx = static_cast<uint32_t>(block.vtx.size());
    
    // Try to activate best chain (this may connect this block)
    return ActivateBestChain();
}

} // namespace shurium
