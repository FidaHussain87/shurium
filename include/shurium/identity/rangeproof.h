// SHURIUM - Range Proofs
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements range proofs that prove a committed value lies within a range
// without revealing the value itself.
//
// Uses a simplified Bulletproofs-inspired approach for efficiency.

#ifndef SHURIUM_IDENTITY_RANGEPROOF_H
#define SHURIUM_IDENTITY_RANGEPROOF_H

#include <shurium/core/types.h>
#include <shurium/crypto/field.h>
#include <shurium/identity/sigma.h>

#include <array>
#include <optional>
#include <vector>

namespace shurium {
namespace identity {

// ============================================================================
// Range Proof Configuration
// ============================================================================

/// Maximum number of bits for range proofs
constexpr size_t MAX_RANGE_BITS = 64;

/// Default number of bits (for amounts up to 2^64 - 1)
constexpr size_t DEFAULT_RANGE_BITS = 64;

// ============================================================================
// Range Proof Structure
// ============================================================================

/**
 * A zero-knowledge range proof.
 * 
 * Proves that a Pedersen commitment C = g^v * h^r contains a value v
 * such that 0 <= v < 2^n, without revealing v or r.
 * 
 * Structure (Bulletproofs-inspired):
 * - A, S: Initial commitments to bit vectors
 * - T1, T2: Commitments to polynomial coefficients
 * - tau_x, mu, t_hat: Final opening values
 * - L, R: Inner product argument vectors
 */
struct RangeProof {
    /// Number of bits in the range (value must be in [0, 2^numBits))
    uint8_t numBits{DEFAULT_RANGE_BITS};
    
    /// Initial commitment A (bit commitments)
    FieldElement A;
    
    /// Initial commitment S (blinding factors)
    FieldElement S;
    
    /// Polynomial commitment T1
    FieldElement T1;
    
    /// Polynomial commitment T2
    FieldElement T2;
    
    /// Opening value tau_x
    FieldElement tau_x;
    
    /// Opening value mu
    FieldElement mu;
    
    /// Polynomial evaluation t_hat
    FieldElement t_hat;
    
    /// Inner product argument - L values
    std::vector<FieldElement> L;
    
    /// Inner product argument - R values
    std::vector<FieldElement> R;
    
    /// Final scalar a
    FieldElement a;
    
    /// Final scalar b
    FieldElement b;
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<RangeProof> FromBytes(const Byte* data, size_t len);
    
    /// Check if proof is well-formed
    bool IsWellFormed() const;
    
    /// Get approximate size in bytes
    size_t Size() const;
};

// ============================================================================
// Range Proof Generators
// ============================================================================

/**
 * Pre-computed generators for range proofs.
 * 
 * These are computed deterministically from nothing-up-my-sleeve seeds.
 */
class RangeProofGenerators {
public:
    /// Get generators for a specific bit count
    static const RangeProofGenerators& Get(size_t numBits = DEFAULT_RANGE_BITS);
    
    /// Generator g (for values)
    const FieldElement& G() const { return g_; }
    
    /// Generator h (for blinding)
    const FieldElement& H() const { return h_; }
    
    /// Vector of generators G_i
    const std::vector<FieldElement>& Gi() const { return gi_; }
    
    /// Vector of generators H_i
    const std::vector<FieldElement>& Hi() const { return hi_; }
    
    /// Generator u (for inner product)
    const FieldElement& U() const { return u_; }
    
private:
    RangeProofGenerators(size_t numBits);
    
    size_t numBits_;
    FieldElement g_;
    FieldElement h_;
    std::vector<FieldElement> gi_;
    std::vector<FieldElement> hi_;
    FieldElement u_;
};

// ============================================================================
// Range Proof Prover
// ============================================================================

/**
 * Prover for range proofs.
 */
class RangeProofProver {
public:
    /// Create a range proof
    /// @param value The value to prove is in range (must be in [0, 2^numBits))
    /// @param randomness The blinding factor used in the commitment
    /// @param numBits Number of bits in the range
    /// @return The range proof, or nullopt if value is out of range
    static std::optional<RangeProof> Prove(
        uint64_t value,
        const FieldElement& randomness,
        size_t numBits = DEFAULT_RANGE_BITS);
    
    /// Create a range proof with a custom commitment
    /// @param value The value
    /// @param randomness The blinding factor
    /// @param commitment The pre-computed commitment (for verification)
    /// @param numBits Number of bits
    static std::optional<RangeProof> Prove(
        uint64_t value,
        const FieldElement& randomness,
        const FieldElement& commitment,
        size_t numBits = DEFAULT_RANGE_BITS);
};

// ============================================================================
// Range Proof Verifier
// ============================================================================

/**
 * Verifier for range proofs.
 */
class RangeProofVerifier {
public:
    /// Verify a range proof
    /// @param proof The proof to verify
    /// @param commitment The Pedersen commitment being proven
    /// @return true if the proof is valid
    static bool Verify(
        const RangeProof& proof,
        const FieldElement& commitment);
    
    /// Batch verify multiple range proofs (more efficient)
    /// @param proofs Vector of proofs
    /// @param commitments Vector of corresponding commitments
    /// @return true if all proofs are valid
    static bool BatchVerify(
        const std::vector<RangeProof>& proofs,
        const std::vector<FieldElement>& commitments);
};

// ============================================================================
// Simplified Range Proof (for smaller ranges)
// ============================================================================

/**
 * A simplified range proof for small ranges.
 * 
 * For proving values in small ranges (e.g., 0-100), we can use a more
 * efficient approach based on discrete log equality proofs.
 */
struct SimpleRangeProof {
    /// The range [min, max]
    uint64_t minValue{0};
    uint64_t maxValue{0};
    
    /// Proof components
    std::vector<SchnorrProof> bitProofs;
    
    /// Aggregate proof
    FieldElement aggregateCommitment;
    FieldElement aggregateResponse;
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<SimpleRangeProof> FromBytes(const Byte* data, size_t len);
    
    /// Check if well-formed
    bool IsWellFormed() const;
};

/**
 * Prover for simple range proofs.
 */
class SimpleRangeProofProver {
public:
    /// Create a simple range proof
    /// @param value The value to prove is in range
    /// @param randomness The blinding factor
    /// @param minValue Minimum of range (inclusive)
    /// @param maxValue Maximum of range (inclusive)
    static std::optional<SimpleRangeProof> Prove(
        uint64_t value,
        const FieldElement& randomness,
        uint64_t minValue,
        uint64_t maxValue);
};

/**
 * Verifier for simple range proofs.
 */
class SimpleRangeProofVerifier {
public:
    /// Verify a simple range proof
    static bool Verify(
        const SimpleRangeProof& proof,
        const FieldElement& commitment);
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Create a Pedersen commitment to a value
/// C = g^v * h^r
FieldElement PedersenCommit(uint64_t value, const FieldElement& randomness);

/// Create a Pedersen commitment using field elements
FieldElement PedersenCommit(const FieldElement& value, const FieldElement& randomness);

/// Generate random blinding factor
FieldElement GenerateBlinding();

/// Compute inner product of two vectors
FieldElement InnerProduct(const std::vector<FieldElement>& a,
                          const std::vector<FieldElement>& b);

/// Hadamard (element-wise) product of two vectors
std::vector<FieldElement> HadamardProduct(const std::vector<FieldElement>& a,
                                           const std::vector<FieldElement>& b);

} // namespace identity
} // namespace shurium

#endif // SHURIUM_IDENTITY_RANGEPROOF_H
