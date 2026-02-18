// SHURIUM - Block Index Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the block index and chain structures for tracking
// the blockchain state.

#ifndef SHURIUM_CHAIN_BLOCKINDEX_H
#define SHURIUM_CHAIN_BLOCKINDEX_H

#include "shurium/core/types.h"
#include "shurium/core/block.h"
#include "shurium/core/serialize.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

namespace shurium {

// ============================================================================
// Block Status Flags
// ============================================================================

/**
 * Block validation status flags.
 * These track how far validation has progressed for a block.
 */
enum class BlockStatus : uint32_t {
    UNKNOWN = 0,
    
    // Validity levels (mutually exclusive, stored in low bits)
    VALID_HEADER = 1,      // Header is valid (PoW, timestamp, etc.)
    VALID_TREE = 2,        // All parents found, difficulty matches
    VALID_TRANSACTIONS = 3, // Transactions are valid (merkle, no duplicates)
    VALID_CHAIN = 4,       // No double spends, coinbase maturity ok
    VALID_SCRIPTS = 5,     // All scripts verified
    
    VALID_MASK = 0x07,     // Mask for validity level
    
    // Data availability flags
    HAVE_DATA = 0x08,      // Full block data available
    HAVE_UNDO = 0x10,      // Undo data available for disconnecting
    HAVE_MASK = 0x18,      // Mask for data flags
    
    // Failure flags
    FAILED_VALID = 0x20,   // Block failed validation
    FAILED_CHILD = 0x40,   // Descends from a failed block
    FAILED_MASK = 0x60,    // Mask for failure flags
};

inline BlockStatus operator|(BlockStatus a, BlockStatus b) {
    return static_cast<BlockStatus>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline BlockStatus operator&(BlockStatus a, BlockStatus b) {
    return static_cast<BlockStatus>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline BlockStatus operator~(BlockStatus a) {
    return static_cast<BlockStatus>(~static_cast<uint32_t>(a));
}

inline bool HasStatus(BlockStatus status, BlockStatus flag) {
    return (static_cast<uint32_t>(status) & static_cast<uint32_t>(flag)) != 0;
}

inline uint32_t GetValidityLevel(BlockStatus status) {
    return static_cast<uint32_t>(status & BlockStatus::VALID_MASK);
}

// ============================================================================
// BlockIndex - Index entry for a block in the chain
// ============================================================================

/**
 * Block index entry.
 * 
 * This structure contains metadata about a block and its position in the
 * blockchain. The actual block data is stored separately; this just
 * contains the index information needed for chain selection and validation.
 */
class BlockIndex {
public:
    /// Pointer to the hash of this block (owned by BlockMap)
    const BlockHash* phashBlock{nullptr};
    
    /// Pointer to previous block's index, or nullptr for genesis
    BlockIndex* pprev{nullptr};
    
    /// Skip pointer for efficient ancestor lookup
    BlockIndex* pskip{nullptr};
    
    /// Height of this block (genesis = 0)
    int32_t nHeight{0};
    
    /// File number where block data is stored
    int32_t nFile{0};
    
    /// Byte offset in the block file
    uint32_t nDataPos{0};
    
    /// Byte offset in the undo file
    uint32_t nUndoPos{0};
    
    /// Total chain work up to and including this block
    /// Using a simple uint64_t for now (may need arith_uint256 for production)
    uint64_t nChainWork{0};
    
    /// Number of transactions in this block (0 if unknown)
    uint32_t nTx{0};
    
    /// Total transactions in chain up to this block
    uint64_t nChainTx{0};
    
    /// Validation status
    BlockStatus nStatus{BlockStatus::UNKNOWN};
    
    // Block header fields (cached for quick access)
    int32_t nVersion{0};
    Hash256 hashMerkleRoot;
    uint32_t nTime{0};
    uint32_t nBits{0};
    uint32_t nNonce{0};
    
    /// Sequential ID for ordering blocks received at same height
    int32_t nSequenceId{0};
    
    /// Maximum timestamp in the chain up to this block
    uint32_t nTimeMax{0};
    
    // ========================================================================
    // Constructors
    // ========================================================================
    
    BlockIndex() = default;
    
    /// Construct from a block header
    explicit BlockIndex(const BlockHeader& header)
        : nVersion(header.nVersion),
          hashMerkleRoot(header.hashMerkleRoot),
          nTime(header.nTime),
          nBits(header.nBits),
          nNonce(header.nNonce) {}
    
    // Prevent copies to avoid dangling pointers
    BlockIndex(const BlockIndex&) = delete;
    BlockIndex& operator=(const BlockIndex&) = delete;
    
    // Allow moves
    BlockIndex(BlockIndex&&) = default;
    BlockIndex& operator=(BlockIndex&&) = default;
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    /// Get the block hash
    BlockHash GetBlockHash() const {
        if (phashBlock) return *phashBlock;
        return BlockHash();
    }
    
    /// Reconstruct the block header
    BlockHeader GetBlockHeader() const {
        BlockHeader header;
        header.nVersion = nVersion;
        header.hashPrevBlock = pprev ? pprev->GetBlockHash() : BlockHash();
        header.hashMerkleRoot = hashMerkleRoot;
        header.nTime = nTime;
        header.nBits = nBits;
        header.nNonce = nNonce;
        return header;
    }
    
    /// Get the block timestamp
    int64_t GetBlockTime() const { return static_cast<int64_t>(nTime); }
    
    /// Get maximum timestamp in chain
    int64_t GetBlockTimeMax() const { return static_cast<int64_t>(nTimeMax); }
    
    /// Get median time of past N blocks (for time validation)
    int64_t GetMedianTimePast() const {
        static constexpr int MEDIAN_TIME_SPAN = 11;
        
        std::vector<int64_t> times;
        times.reserve(MEDIAN_TIME_SPAN);
        
        const BlockIndex* pindex = this;
        for (int i = 0; i < MEDIAN_TIME_SPAN && pindex; ++i) {
            times.push_back(pindex->GetBlockTime());
            pindex = pindex->pprev;
        }
        
        std::sort(times.begin(), times.end());
        return times[times.size() / 2];
    }
    
    /// Check if we have transaction count information
    bool HaveNumChainTxs() const { return nChainTx != 0; }
    
    // ========================================================================
    // Validation Status
    // ========================================================================
    
    /// Check if block is valid up to the given level
    bool IsValid(BlockStatus upTo) const {
        if (HasStatus(nStatus, BlockStatus::FAILED_MASK)) {
            return false;
        }
        return GetValidityLevel(nStatus) >= static_cast<uint32_t>(upTo);
    }
    
    /// Raise validity level (returns true if changed)
    bool RaiseValidity(BlockStatus upTo) {
        uint32_t level = static_cast<uint32_t>(upTo & BlockStatus::VALID_MASK);
        if (HasStatus(nStatus, BlockStatus::FAILED_MASK)) {
            return false;
        }
        if (GetValidityLevel(nStatus) < level) {
            nStatus = (nStatus & ~BlockStatus::VALID_MASK) | 
                      static_cast<BlockStatus>(level);
            return true;
        }
        return false;
    }
    
    /// Check if we have block data
    bool HaveData() const { return HasStatus(nStatus, BlockStatus::HAVE_DATA); }
    
    /// Check if we have undo data
    bool HaveUndo() const { return HasStatus(nStatus, BlockStatus::HAVE_UNDO); }
    
    /// Check if block failed validation
    bool IsFailed() const { return HasStatus(nStatus, BlockStatus::FAILED_MASK); }
    
    // ========================================================================
    // Skip List
    // ========================================================================
    
    /// Build the skip pointer for efficient ancestor lookups
    void BuildSkip();
    
    /// Find an ancestor at a specific height
    BlockIndex* GetAncestor(int height);
    const BlockIndex* GetAncestor(int height) const;
    
    // ========================================================================
    // Utility
    // ========================================================================
    
    /// Convert to string for debugging
    std::string ToString() const;
};

// ============================================================================
// Chain - An in-memory indexed chain of blocks
// ============================================================================

/**
 * An in-memory indexed chain of blocks.
 * 
 * This represents the active chain from genesis to the current tip.
 * Provides efficient access by height and various chain queries.
 */
class Chain {
private:
    std::vector<BlockIndex*> vChain;
    
public:
    Chain() = default;
    
    // Prevent copies
    Chain(const Chain&) = delete;
    Chain& operator=(const Chain&) = delete;
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    /// Get the genesis block index
    BlockIndex* Genesis() const {
        return vChain.empty() ? nullptr : vChain[0];
    }
    
    /// Get the tip (most recent block) index
    BlockIndex* Tip() const {
        return vChain.empty() ? nullptr : vChain.back();
    }
    
    /// Get block at a specific height
    BlockIndex* operator[](int height) const {
        if (height < 0 || height >= static_cast<int>(vChain.size())) {
            return nullptr;
        }
        return vChain[height];
    }
    
    /// Get the chain height (-1 if empty)
    int Height() const {
        return static_cast<int>(vChain.size()) - 1;
    }
    
    /// Check if chain is empty
    bool empty() const { return vChain.empty(); }
    
    /// Get the number of blocks in the chain
    size_t size() const { return vChain.size(); }
    
    // ========================================================================
    // Queries
    // ========================================================================
    
    /// Check if a block is in this chain
    bool Contains(const BlockIndex* pindex) const {
        if (!pindex || pindex->nHeight < 0 || 
            pindex->nHeight >= static_cast<int>(vChain.size())) {
            return false;
        }
        return vChain[pindex->nHeight] == pindex;
    }
    
    /// Get the next block after pindex in this chain
    BlockIndex* Next(const BlockIndex* pindex) const {
        if (!Contains(pindex)) return nullptr;
        int nextHeight = pindex->nHeight + 1;
        if (nextHeight >= static_cast<int>(vChain.size())) return nullptr;
        return vChain[nextHeight];
    }
    
    /// Find the last common ancestor with another block
    const BlockIndex* FindFork(const BlockIndex* pindex) const;
    
    /// Find the first block at or after the given time and height
    BlockIndex* FindEarliestAtLeast(int64_t nTime, int height) const;
    
    // ========================================================================
    // Modification
    // ========================================================================
    
    /// Set the chain tip to a specific block (rebuilds chain)
    void SetTip(BlockIndex* pindex);
    
    /// Clear the chain
    void Clear() { vChain.clear(); }
    
    // ========================================================================
    // Iteration
    // ========================================================================
    
    using iterator = std::vector<BlockIndex*>::iterator;
    using const_iterator = std::vector<BlockIndex*>::const_iterator;
    
    iterator begin() { return vChain.begin(); }
    iterator end() { return vChain.end(); }
    const_iterator begin() const { return vChain.begin(); }
    const_iterator end() const { return vChain.end(); }
};

// ============================================================================
// BlockMap - Hash map of all known blocks
// ============================================================================

/// Hash function for BlockHash
struct BlockHashHasher {
    size_t operator()(const BlockHash& hash) const {
        size_t seed = 0;
        for (size_t i = 0; i < 8; ++i) {
            seed ^= static_cast<size_t>(hash[i]) << (i * 8);
        }
        return seed;
    }
};

/// Map from block hash to block index
using BlockMap = std::unordered_map<BlockHash, std::unique_ptr<BlockIndex>, BlockHashHasher>;

// ============================================================================
// Utility Functions
// ============================================================================

/// Find the last common ancestor of two blocks
const BlockIndex* LastCommonAncestor(const BlockIndex* pa, const BlockIndex* pb);

/// Calculate the work for a given nBits value
uint64_t GetBlockProof(uint32_t nBits);

/// Get block locator for a block index
BlockLocator GetLocator(const BlockIndex* pindex);

} // namespace shurium

#endif // SHURIUM_CHAIN_BLOCKINDEX_H
