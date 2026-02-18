// SHURIUM - Block Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/core/block.h"
#include "shurium/core/merkle.h"
#include "shurium/crypto/sha256.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace shurium {

// ============================================================================
// BlockHeader Implementation
// ============================================================================

BlockHash BlockHeader::GetHash() const {
    // Serialize the header
    DataStream ss;
    Serialize(ss, *this);
    
    // Double SHA256 (Bitcoin-compatible block hashing)
    Hash256 hash = DoubleSHA256(ss.data(), ss.TotalSize());
    
    return BlockHash(hash);
}

std::string BlockHeader::ToString() const {
    std::ostringstream ss;
    ss << "BlockHeader(\n";
    ss << "  version=" << nVersion << "\n";
    ss << "  hashPrevBlock=" << hashPrevBlock.ToHex().substr(0, 16) << "...\n";
    ss << "  hashMerkleRoot=" << hashMerkleRoot.ToHex().substr(0, 16) << "...\n";
    ss << "  nTime=" << nTime << "\n";
    ss << "  nBits=0x" << std::hex << nBits << std::dec << "\n";
    ss << "  nNonce=" << nNonce << "\n";
    ss << "  hash=" << GetHash().ToHex().substr(0, 16) << "...\n";
    ss << ")";
    return ss.str();
}

// ============================================================================
// Block Implementation
// ============================================================================

Hash256 Block::ComputeMerkleRoot() const {
    if (vtx.empty()) {
        return Hash256();  // Null hash for empty block
    }
    
    // Extract transaction hashes
    std::vector<Hash256> txHashes;
    txHashes.reserve(vtx.size());
    for (const auto& tx : vtx) {
        txHashes.push_back(Hash256(tx->GetHash().data(), 32));
    }
    
    // Compute merkle root using our merkle module
    // Use fully qualified name to avoid ambiguity with the member function
    return shurium::ComputeMerkleRoot(std::move(txHashes));
}

size_t Block::GetTotalSize() const {
    return GetSerializeSize(*this);
}

std::string Block::ToString() const {
    std::ostringstream ss;
    ss << "Block(\n";
    ss << "  hash=" << GetHash().ToHex().substr(0, 16) << "...\n";
    ss << "  version=" << nVersion << "\n";
    ss << "  hashPrevBlock=" << hashPrevBlock.ToHex().substr(0, 16) << "...\n";
    ss << "  hashMerkleRoot=" << hashMerkleRoot.ToHex().substr(0, 16) << "...\n";
    ss << "  nTime=" << nTime << "\n";
    ss << "  nBits=0x" << std::hex << nBits << std::dec << "\n";
    ss << "  nNonce=" << nNonce << "\n";
    ss << "  vtx.size=" << vtx.size() << "\n";
    
    // Show first few transactions
    for (size_t i = 0; i < std::min(vtx.size(), size_t(3)); ++i) {
        ss << "    tx[" << i << "]=" << vtx[i]->GetHash().ToHex().substr(0, 16) << "...\n";
    }
    if (vtx.size() > 3) {
        ss << "    ... and " << (vtx.size() - 3) << " more transactions\n";
    }
    
    ss << ")";
    return ss.str();
}

// ============================================================================
// Genesis Block Creation
// ============================================================================

Block CreateGenesisBlock(const char* pszTimestamp, const Script& genesisOutputScript,
                         uint32_t nTime, uint32_t nNonce, uint32_t nBits,
                         int32_t nVersion, Amount genesisReward) {
    // Create the coinbase transaction
    MutableTransaction txCoinbase;
    txCoinbase.version = 1;
    txCoinbase.nLockTime = 0;
    
    // Coinbase input with null outpoint
    OutPoint nullOutpoint;
    
    // Create coinbase script with timestamp message
    Script coinbaseScript;
    // Push block height (0 for genesis)
    coinbaseScript << std::vector<uint8_t>{0x00};
    // Push timestamp message
    std::vector<uint8_t> timestampBytes(
        reinterpret_cast<const uint8_t*>(pszTimestamp),
        reinterpret_cast<const uint8_t*>(pszTimestamp) + std::strlen(pszTimestamp)
    );
    coinbaseScript << timestampBytes;
    
    txCoinbase.vin.push_back(TxIn(nullOutpoint, coinbaseScript, 0xFFFFFFFF));
    
    // Genesis output
    txCoinbase.vout.push_back(TxOut(genesisReward, genesisOutputScript));
    
    // Create the block
    Block genesis;
    genesis.nVersion = nVersion;
    genesis.nTime = nTime;
    genesis.nBits = nBits;
    genesis.nNonce = nNonce;
    genesis.hashPrevBlock.SetNull();  // Genesis has no parent
    
    // Add the coinbase transaction
    genesis.vtx.push_back(MakeTransactionRef(std::move(txCoinbase)));
    
    // Compute and set the merkle root
    genesis.hashMerkleRoot = genesis.ComputeMerkleRoot();
    
    return genesis;
}

Block CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits,
                         int32_t nVersion, Amount genesisReward) {
    // Default timestamp message for SHURIUM
    const char* pszTimestamp = "SHURIUM: Next Evolution Universal Exchange System - Genesis";
    
    // Default output script (OP_RETURN with message - unspendable)
    // In production, this would be a P2PKH to a well-known address
    Script genesisOutputScript;
    
    // For now, create a P2PKH script with a placeholder pubkey hash
    // The genesis reward is symbolic and typically unspendable
    Hash160 genesisPubKeyHash;
    // Fill with identifiable pattern
    for (size_t i = 0; i < 20; ++i) {
        genesisPubKeyHash[i] = static_cast<uint8_t>(i);
    }
    genesisOutputScript = Script::CreateP2PKH(genesisPubKeyHash);
    
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript,
                              nTime, nNonce, nBits, nVersion, genesisReward);
}

} // namespace shurium
