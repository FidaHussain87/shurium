// SHURIUM - Merkle Tree Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/core/merkle.h"
#include "shurium/crypto/sha256.h"
#include <cstring>

namespace shurium {

// ============================================================================
// Helper Functions
// ============================================================================

Hash256 HashPair(const Hash256& left, const Hash256& right) {
    uint8_t combined[64];
    std::memcpy(combined, left.data(), 32);
    std::memcpy(combined + 32, right.data(), 32);
    return DoubleSHA256(combined, 64);
}

// ============================================================================
// Merkle Root Computation
// ============================================================================

Hash256 ComputeMerkleRoot(std::vector<Hash256> hashes, bool* mutated) {
    bool mutation = false;
    
    // Empty tree returns null hash
    if (hashes.empty()) {
        if (mutated) *mutated = false;
        return Hash256();
    }
    
    // Single leaf is its own root
    if (hashes.size() == 1) {
        if (mutated) *mutated = false;
        return hashes[0];
    }
    
    // Build tree level by level
    while (hashes.size() > 1) {
        // Check for mutation (duplicate adjacent hashes)
        if (mutated) {
            for (size_t pos = 0; pos + 1 < hashes.size(); pos += 2) {
                if (hashes[pos] == hashes[pos + 1]) {
                    mutation = true;
                }
            }
        }
        
        // If odd number of hashes, duplicate the last one
        if (hashes.size() & 1) {
            hashes.push_back(hashes.back());
        }
        
        // Compute next level
        size_t newSize = hashes.size() / 2;
        for (size_t i = 0; i < newSize; ++i) {
            hashes[i] = HashPair(hashes[i * 2], hashes[i * 2 + 1]);
        }
        hashes.resize(newSize);
    }
    
    if (mutated) *mutated = mutation;
    return hashes[0];
}

// ============================================================================
// Merkle Proof Functions
// ============================================================================

std::vector<Hash256> ComputeMerklePath(const std::vector<Hash256>& leaves, uint32_t position) {
    std::vector<Hash256> proof;
    
    if (leaves.empty() || position >= leaves.size()) {
        return proof;
    }
    
    // Single leaf needs no proof
    if (leaves.size() == 1) {
        return proof;
    }
    
    // Make a copy to work with
    std::vector<Hash256> hashes = leaves;
    uint32_t pos = position;
    
    // Build proof from bottom to top
    while (hashes.size() > 1) {
        // Handle odd-length level
        if (hashes.size() & 1) {
            hashes.push_back(hashes.back());
        }
        
        // Get sibling hash
        uint32_t siblingPos = (pos & 1) ? (pos - 1) : (pos + 1);
        if (siblingPos < hashes.size()) {
            proof.push_back(hashes[siblingPos]);
        }
        
        // Compute next level
        size_t newSize = hashes.size() / 2;
        for (size_t i = 0; i < newSize; ++i) {
            hashes[i] = HashPair(hashes[i * 2], hashes[i * 2 + 1]);
        }
        hashes.resize(newSize);
        
        // Update position for next level
        pos /= 2;
    }
    
    return proof;
}

bool VerifyMerkleProof(const Hash256& leaf, uint32_t position,
                       const Hash256& root, const std::vector<Hash256>& proof) {
    Hash256 current = leaf;
    uint32_t pos = position;
    
    for (const Hash256& sibling : proof) {
        // Determine order based on position
        if (pos & 1) {
            // Current is on the right
            current = HashPair(sibling, current);
        } else {
            // Current is on the left
            current = HashPair(current, sibling);
        }
        pos /= 2;
    }
    
    return current == root;
}

} // namespace shurium
