// SHURIUM - Block Assembler Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the BlockAssembler class for creating block templates
// used in mining operations.

#ifndef SHURIUM_MINER_BLOCKASSEMBLER_H
#define SHURIUM_MINER_BLOCKASSEMBLER_H

#include "shurium/core/block.h"
#include "shurium/core/transaction.h"
#include "shurium/chain/chainstate.h"
#include "shurium/mempool/mempool.h"
#include "shurium/consensus/params.h"
#include <memory>
#include <vector>
#include <string>

namespace shurium {
namespace miner {

// ============================================================================
// Block Template
// ============================================================================

/**
 * Represents a block template for mining.
 * Contains all information needed by a miner to produce a valid block.
 */
struct BlockTemplate {
    /// The block being assembled
    Block block;
    
    /// Transactions included (with fees)
    struct TxInfo {
        TransactionRef tx;
        Amount fee;
        size_t sigops;
    };
    std::vector<TxInfo> txInfo;
    
    /// Total fees collected
    Amount totalFees{0};
    
    /// Block height
    int32_t height{0};
    
    /// Target in compact form
    uint32_t nBits{0};
    
    /// Target as hash (for comparison)
    Hash256 target;
    
    /// Minimum timestamp allowed
    int64_t minTime{0};
    
    /// Current time when template was created
    int64_t curTime{0};
    
    /// Coinbase value (subsidy + fees)
    Amount coinbaseValue{0};
    
    /// Is the template valid for mining?
    bool isValid{false};
    
    /// Error message if not valid
    std::string error;
};

// ============================================================================
// Block Assembler Options
// ============================================================================

/**
 * Options for block assembly.
 */
struct BlockAssemblerOptions {
    /// Maximum block weight
    uint32_t maxBlockWeight{4000000};
    
    /// Maximum block sigops
    uint32_t maxBlockSigops{80000};
    
    /// Block minimum time (MTP + 1)
    bool enforceMinTime{true};
    
    /// Include witness data
    bool includeWitness{true};
    
    /// Minimum fee rate for inclusion
    FeeRate minFeeRate{1};
    
    /// Default constructor with reasonable defaults
    BlockAssemblerOptions() = default;
};

// ============================================================================
// Block Assembler
// ============================================================================

/**
 * Assembles block templates for mining.
 * 
 * The BlockAssembler selects transactions from the mempool,
 * creates the coinbase transaction, and builds a complete
 * block template ready for mining.
 */
class BlockAssembler {
public:
    /**
     * Construct a block assembler.
     * 
     * @param chainState Reference to the chain state
     * @param mempool Reference to the mempool
     * @param params Consensus parameters
     * @param options Assembly options
     */
    BlockAssembler(ChainState& chainState, 
                   Mempool& mempool,
                   const consensus::Params& params,
                   const BlockAssemblerOptions& options = {});
    
    /**
     * Create a block template for mining.
     * 
     * @param coinbaseScript Script for the coinbase output
     * @return BlockTemplate ready for mining
     */
    BlockTemplate CreateNewBlock(const Script& coinbaseScript);
    
    /**
     * Create a block template with default coinbase.
     * 
     * @param coinbaseDest Destination for coinbase (public key hash)
     * @return BlockTemplate ready for mining
     */
    BlockTemplate CreateNewBlock(const Hash160& coinbaseDest);
    
private:
    /// Add transactions from mempool
    void AddTransactionsFromMempool();
    
    /// Create the coinbase transaction
    TransactionRef CreateCoinbase(const Script& coinbaseScript);
    
    /// Calculate the coinbase value (subsidy + fees)
    Amount CalculateCoinbaseValue() const;
    
    /// Check if we can add more transactions
    bool CanAddTransaction(const Transaction& tx, Amount fee) const;
    
    /// Add a transaction to the block
    void AddTransaction(const TransactionRef& tx, Amount fee, size_t sigops);
    
    /// Finalize the block template
    void FinalizeBlock();
    
    // References
    ChainState& m_chainState;
    Mempool& m_mempool;
    const consensus::Params& m_params;
    BlockAssemblerOptions m_options;
    
    // Current block being built
    std::unique_ptr<BlockTemplate> m_template;
    
    // Running totals
    uint32_t m_blockWeight{0};
    uint32_t m_blockSigops{0};
    size_t m_txCount{0};
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Create a coinbase transaction for a given height and reward.
 * 
 * @param height Block height
 * @param coinbaseScript Output script for the coinbase
 * @param reward Total coinbase value (subsidy + fees)
 * @param params Consensus parameters (for SHURIUM reward distribution)
 * @return The coinbase transaction
 */
TransactionRef CreateCoinbaseTransaction(int32_t height,
                                          const Script& coinbaseScript,
                                          Amount reward,
                                          const consensus::Params& params);

/**
 * Serialize a block to hex string.
 * @param block The block to serialize
 * @return Hex-encoded block data
 */
std::string BlockToHex(const Block& block);

/**
 * Deserialize a block from hex string.
 * @param hex Hex-encoded block data
 * @return The deserialized block, or nullopt if invalid
 */
std::optional<Block> HexToBlock(const std::string& hex);

/**
 * Calculate target hash from compact nBits.
 * @param nBits Compact difficulty target
 * @return 256-bit target hash
 */
Hash256 GetTargetFromBits(uint32_t nBits);

/**
 * Format target as hex string for mining.
 * @param target Target hash
 * @return Hex string (64 chars, big-endian display)
 */
std::string TargetToHex(const Hash256& target);

} // namespace miner
} // namespace shurium

#endif // SHURIUM_MINER_BLOCKASSEMBLER_H
