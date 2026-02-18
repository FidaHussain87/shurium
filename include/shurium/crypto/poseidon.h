// SHURIUM - Poseidon Hash Function
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// ZK-friendly algebraic hash function over the BN254 scalar field
// Based on: "Poseidon: A New Hash Function for Zero-Knowledge Proof Systems"
// https://eprint.iacr.org/2019/458

#ifndef SHURIUM_CRYPTO_POSEIDON_H
#define SHURIUM_CRYPTO_POSEIDON_H

#include <cstdint>
#include <vector>
#include <array>
#include "shurium/crypto/field.h"
#include "shurium/core/types.h"

namespace shurium {

// ============================================================================
// Poseidon Configuration
// ============================================================================

/// Poseidon hash configuration parameters
struct PoseidonConfig {
    /// State width (t)
    size_t width;
    
    /// Number of full rounds (R_F)
    size_t fullRounds;
    
    /// Number of partial rounds (R_P)
    size_t partialRounds;
    
    /// Capacity (for security)
    size_t capacity;
    
    /// Rate (for input/output)
    size_t rate() const { return width - capacity; }
    
    /// Total rounds
    size_t totalRounds() const { return fullRounds + partialRounds; }
};

// ============================================================================
// Poseidon Parameters (Pre-computed)
// ============================================================================

/// Standard Poseidon configurations for different input sizes
namespace PoseidonParams {
    /// 2-to-1 compression (width=3, capacity=1, rate=2)
    extern const PoseidonConfig CONFIG_2_1;
    
    /// 4-to-1 compression (width=5, capacity=1, rate=4)
    extern const PoseidonConfig CONFIG_4_1;
    
    /// Standard hash (width=5, capacity=1, rate=4)
    extern const PoseidonConfig CONFIG_STANDARD;
}

// ============================================================================
// Poseidon Hash Class
// ============================================================================

/// Poseidon hash function over BN254 scalar field
/// 
/// This is an algebraic sponge-based hash optimized for ZK proofs.
/// It uses the Hades design strategy with partial rounds for efficiency.
class Poseidon {
public:
    /// Output size in bytes (32 bytes = 256 bits = 1 field element)
    static constexpr size_t OUTPUT_SIZE = 32;
    
    /// Create Poseidon hasher with default configuration
    Poseidon();
    
    /// Create Poseidon hasher with specific configuration
    explicit Poseidon(const PoseidonConfig& config);
    
    /// Reset the hasher to initial state
    Poseidon& Reset();
    
    /// Absorb a field element into the sponge
    Poseidon& Absorb(const FieldElement& element);
    
    /// Absorb multiple field elements
    Poseidon& Absorb(const std::vector<FieldElement>& elements);
    
    /// Absorb raw bytes (converts to field elements)
    Poseidon& AbsorbBytes(const Byte* data, size_t len);
    
    /// Squeeze one field element from the sponge
    FieldElement Squeeze();
    
    /// Squeeze multiple field elements
    std::vector<FieldElement> Squeeze(size_t count);
    
    /// Hash a vector of field elements to a single field element
    static FieldElement Hash(const std::vector<FieldElement>& inputs);
    
    /// Hash two field elements (2-to-1 compression, commonly used in Merkle trees)
    static FieldElement Hash2(const FieldElement& left, const FieldElement& right);
    
    /// Hash raw bytes to a field element
    static FieldElement HashBytes(const Byte* data, size_t len);
    
    /// Hash raw bytes to a 32-byte output
    static std::array<Byte, 32> HashToBytes(const Byte* data, size_t len);

private:
    /// Configuration
    PoseidonConfig config_;
    
    /// Sponge state
    std::vector<FieldElement> state_;
    
    /// Current position in rate portion
    size_t absorbPos_;
    
    /// Whether squeeze mode has started
    bool squeezing_;
    
    /// Round constants (one per state element per round)
    std::vector<std::vector<FieldElement>> roundConstants_;
    
    /// MDS matrix (width x width)
    std::vector<std::vector<FieldElement>> mdsMatrix_;
    
    /// Initialize constants for the given configuration
    void InitConstants();
    
    /// Apply the Poseidon permutation to the state
    void Permute();
    
    /// Apply full round (S-box on all elements)
    void FullRound(size_t roundIdx);
    
    /// Apply partial round (S-box on first element only)
    void PartialRound(size_t roundIdx);
    
    /// Add round constants to state
    void AddRoundConstants(size_t roundIdx);
    
    /// Apply MDS matrix multiplication
    void MixColumns();
    
    /// Apply S-box (x^5) to a single element
    static FieldElement Sbox(const FieldElement& x);
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Hash field elements using Poseidon
inline FieldElement PoseidonHash(const std::vector<FieldElement>& inputs) {
    return Poseidon::Hash(inputs);
}

/// Hash two field elements (for Merkle trees)
inline FieldElement PoseidonHash2(const FieldElement& left, const FieldElement& right) {
    return Poseidon::Hash2(left, right);
}

/// Hash bytes to field element
inline FieldElement PoseidonHashBytes(const Byte* data, size_t len) {
    return Poseidon::HashBytes(data, len);
}

/// Hash bytes to 32-byte output
inline std::array<Byte, 32> PoseidonHashToBytes(const Byte* data, size_t len) {
    return Poseidon::HashToBytes(data, len);
}

} // namespace shurium

#endif // SHURIUM_CRYPTO_POSEIDON_H
