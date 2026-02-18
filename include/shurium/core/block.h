// SHURIUM - Block Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the block primitives for SHURIUM.
// Based on Bitcoin's block structure with modifications for PoUW.

#ifndef SHURIUM_CORE_BLOCK_H
#define SHURIUM_CORE_BLOCK_H

#include "shurium/core/types.h"
#include "shurium/core/transaction.h"
#include "shurium/core/serialize.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace shurium {

// ============================================================================
// BlockHeader - Block metadata for hashing and chain validation
// ============================================================================

/// Block header containing all metadata needed for proof-of-work validation.
/// The block hash is computed from the serialized header (80 bytes).
class BlockHeader {
public:
    /// Block version (for consensus upgrades)
    int32_t nVersion;
    
    /// Hash of the previous block header
    BlockHash hashPrevBlock;
    
    /// Merkle root of all transactions in the block
    Hash256 hashMerkleRoot;
    
    /// Block creation time (Unix timestamp)
    uint32_t nTime;
    
    /// Difficulty target in compact format
    uint32_t nBits;
    
    /// Nonce used to satisfy proof-of-work
    uint32_t nNonce;

    /// Default constructor - creates a null block header
    BlockHeader() {
        SetNull();
    }

    /// Reset all fields to null/zero state
    void SetNull() {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
    }

    /// Check if header is in null state (nBits == 0 indicates uninitialized)
    bool IsNull() const {
        return nBits == 0;
    }

    /// Compute the block hash (double SHA256 of serialized header)
    BlockHash GetHash() const;

    /// Get block timestamp as Unix time
    int64_t GetBlockTime() const {
        return static_cast<int64_t>(nTime);
    }

    /// Convert to human-readable string
    std::string ToString() const;
};

/// Serialization for BlockHeader
template<typename Stream>
void Serialize(Stream& s, const BlockHeader& header) {
    Serialize(s, header.nVersion);
    Serialize(s, header.hashPrevBlock);
    Serialize(s, header.hashMerkleRoot);
    Serialize(s, header.nTime);
    Serialize(s, header.nBits);
    Serialize(s, header.nNonce);
}

template<typename Stream>
void Unserialize(Stream& s, BlockHeader& header) {
    Unserialize(s, header.nVersion);
    Unserialize(s, header.hashPrevBlock);
    Unserialize(s, header.hashMerkleRoot);
    Unserialize(s, header.nTime);
    Unserialize(s, header.nBits);
    Unserialize(s, header.nNonce);
}

// ============================================================================
// Block - Complete block with header and transactions
// ============================================================================

/// A complete block containing header and transactions.
/// Inherits from BlockHeader for convenient access to header fields.
class Block : public BlockHeader {
public:
    /// Transactions in this block (first must be coinbase)
    std::vector<TransactionRef> vtx;

    /// Default constructor
    Block() {
        SetNull();
    }

    /// Construct from a header (copies header, leaves transactions empty)
    explicit Block(const BlockHeader& header) {
        SetNull();
        // Copy header fields
        nVersion = header.nVersion;
        hashPrevBlock = header.hashPrevBlock;
        hashMerkleRoot = header.hashMerkleRoot;
        nTime = header.nTime;
        nBits = header.nBits;
        nNonce = header.nNonce;
    }

    /// Reset block to null state
    void SetNull() {
        BlockHeader::SetNull();
        vtx.clear();
    }

    /// Extract just the header portion
    BlockHeader GetBlockHeader() const {
        BlockHeader header;
        header.nVersion = nVersion;
        header.hashPrevBlock = hashPrevBlock;
        header.hashMerkleRoot = hashMerkleRoot;
        header.nTime = nTime;
        header.nBits = nBits;
        header.nNonce = nNonce;
        return header;
    }

    /// Compute the merkle root from the transactions
    Hash256 ComputeMerkleRoot() const;

    /// Get total serialized size of the block
    size_t GetTotalSize() const;

    /// Convert to human-readable string
    std::string ToString() const;
};

/// Serialization for Block
template<typename Stream>
void Serialize(Stream& s, const Block& block) {
    // Serialize header fields
    Serialize(s, static_cast<const BlockHeader&>(block));
    
    // Serialize transaction count
    WriteCompactSize(s, block.vtx.size());
    
    // Serialize each transaction
    for (const auto& tx : block.vtx) {
        Serialize(s, *tx);
    }
}

template<typename Stream>
void Unserialize(Stream& s, Block& block) {
    // Deserialize header fields
    Unserialize(s, static_cast<BlockHeader&>(block));
    
    // Deserialize transaction count
    uint64_t txCount = ReadCompactSize(s);
    
    // Deserialize each transaction
    block.vtx.clear();
    block.vtx.reserve(txCount);
    for (uint64_t i = 0; i < txCount; ++i) {
        MutableTransaction mtx;
        Unserialize(s, mtx);
        block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    }
}

// ============================================================================
// BlockLocator - For finding common ancestors in chain synchronization
// ============================================================================

/// Block locator for finding a common ancestor between two chains.
/// Contains a sparse set of block hashes starting from the tip.
class BlockLocator {
public:
    /// List of block hashes (sparse, decreasing height)
    std::vector<BlockHash> vHave;

    /// Default constructor
    BlockLocator() = default;

    /// Construct with a list of hashes
    explicit BlockLocator(std::vector<BlockHash>&& have) 
        : vHave(std::move(have)) {}

    /// Reset to empty state
    void SetNull() {
        vHave.clear();
    }

    /// Check if locator is empty
    bool IsNull() const {
        return vHave.empty();
    }
};

/// Serialization for BlockLocator
template<typename Stream>
void Serialize(Stream& s, const BlockLocator& locator) {
    // Write dummy version for compatibility
    Serialize(s, static_cast<int32_t>(70016));
    Serialize(s, locator.vHave);
}

template<typename Stream>
void Unserialize(Stream& s, BlockLocator& locator) {
    // Read and discard dummy version
    int32_t nVersion;
    Unserialize(s, nVersion);
    Unserialize(s, locator.vHave);
}

// ============================================================================
// Genesis Block Creation
// ============================================================================

/// Create the genesis block for SHURIUM
/// @param nTime Timestamp for the block
/// @param nNonce Nonce that satisfies proof-of-work
/// @param nBits Difficulty target in compact format
/// @param nVersion Block version
/// @param genesisReward Block reward amount
/// @return The genesis block
Block CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, 
                         int32_t nVersion, Amount genesisReward);

/// Create the genesis block with custom message
/// @param pszTimestamp Message embedded in coinbase (for timestamping)
/// @param genesisOutputScript Script for the genesis output
/// @param nTime Timestamp
/// @param nNonce Nonce
/// @param nBits Difficulty target
/// @param nVersion Version
/// @param genesisReward Reward amount
/// @return The genesis block
Block CreateGenesisBlock(const char* pszTimestamp, const Script& genesisOutputScript,
                         uint32_t nTime, uint32_t nNonce, uint32_t nBits,
                         int32_t nVersion, Amount genesisReward);

} // namespace shurium

#endif // SHURIUM_CORE_BLOCK_H
