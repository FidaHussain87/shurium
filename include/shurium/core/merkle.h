// SHURIUM - Merkle Tree Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file provides Merkle tree computation and verification functions.
// Used for transaction inclusion proofs in blocks.

#ifndef SHURIUM_CORE_MERKLE_H
#define SHURIUM_CORE_MERKLE_H

#include "shurium/core/types.h"
#include <vector>
#include <cstdint>

namespace shurium {

// ============================================================================
// Merkle Root Computation
// ============================================================================

/// Compute the Merkle root of a vector of hashes.
/// 
/// @param leaves Vector of leaf hashes (will be modified)
/// @param mutated If not null, set to true if duplicate adjacent pairs detected
///                (CVE-2012-2459 defense)
/// @return The Merkle root, or null hash if leaves is empty
///
/// Note: Uses Bitcoin's Merkle tree construction where odd-length levels
/// duplicate the last element. This has a known vulnerability where
/// [A, B, C] and [A, B, C, C] produce the same root.
Hash256 ComputeMerkleRoot(std::vector<Hash256> leaves, bool* mutated = nullptr);

// ============================================================================
// Merkle Proof (Path) Functions
// ============================================================================

/// Compute the Merkle path (proof) for a leaf at the given position.
///
/// @param leaves Vector of all leaf hashes
/// @param position Index of the leaf to prove (0-based)
/// @return Vector of sibling hashes from leaf to root
///
/// The proof can be used with VerifyMerkleProof() to verify a leaf's
/// inclusion without knowing all other leaves.
std::vector<Hash256> ComputeMerklePath(const std::vector<Hash256>& leaves, uint32_t position);

/// Verify a Merkle proof.
///
/// @param leaf The leaf hash to verify
/// @param position The claimed position of the leaf
/// @param root The expected Merkle root
/// @param proof The Merkle path (from ComputeMerklePath)
/// @return true if the proof is valid
bool VerifyMerkleProof(const Hash256& leaf, uint32_t position, 
                       const Hash256& root, const std::vector<Hash256>& proof);

// ============================================================================
// Helper Functions
// ============================================================================

/// Hash two 32-byte hashes together using double-SHA256
/// @param left First hash
/// @param right Second hash
/// @return DoubleSHA256(left || right)
Hash256 HashPair(const Hash256& left, const Hash256& right);

} // namespace shurium

#endif // SHURIUM_CORE_MERKLE_H
