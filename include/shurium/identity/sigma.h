// SHURIUM - Sigma Protocols for Zero-Knowledge Proofs
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements Sigma protocols - interactive proofs of knowledge that can
// be made non-interactive using the Fiat-Shamir transform.
//
// Key protocols:
// - Schnorr proof: prove knowledge of discrete log
// - Pedersen opening: prove knowledge of commitment opening
// - Equality proof: prove two commitments contain same value

#ifndef SHURIUM_IDENTITY_SIGMA_H
#define SHURIUM_IDENTITY_SIGMA_H

#include <shurium/core/types.h>
#include <shurium/crypto/field.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace identity {

// ============================================================================
// Schnorr Proof (Knowledge of Discrete Log)
// ============================================================================

/**
 * Schnorr proof of knowledge of discrete log.
 * 
 * Given public value P = g^x (where g is a generator), proves knowledge
 * of x without revealing it.
 * 
 * Protocol (made non-interactive via Fiat-Shamir):
 * 1. Prover picks random r, computes R = g^r
 * 2. Challenge c = Hash(P, R, context)
 * 3. Response s = r + c*x
 * 4. Verifier checks: g^s = R * P^c
 */
struct SchnorrProof {
    /// Commitment R = g^r
    FieldElement commitment;
    
    /// Response s = r + c*x
    FieldElement response;
    
    /// Create empty proof
    SchnorrProof() = default;
    
    /// Create proof
    SchnorrProof(const FieldElement& comm, const FieldElement& resp)
        : commitment(comm), response(resp) {}
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<SchnorrProof> FromBytes(const Byte* data, size_t len);
    
    /// Check if proof is well-formed
    bool IsWellFormed() const;
};

/**
 * Schnorr prover for discrete log proofs.
 */
class SchnorrProver {
public:
    /// Create a proof of knowledge of discrete log
    /// @param secretKey The secret value x
    /// @param generator The generator g (base point)
    /// @param publicKey The public value P = g^x (optional, computed if not provided)
    /// @param context Additional context for Fiat-Shamir transform
    /// @return The Schnorr proof
    static SchnorrProof Prove(
        const FieldElement& secretKey,
        const FieldElement& generator,
        const FieldElement& publicKey,
        const std::vector<Byte>& context = {});
    
    /// Create a proof with automatic public key computation
    static SchnorrProof Prove(
        const FieldElement& secretKey,
        const FieldElement& generator,
        const std::vector<Byte>& context = {});
};

/**
 * Schnorr verifier for discrete log proofs.
 */
class SchnorrVerifier {
public:
    /// Verify a Schnorr proof
    /// @param proof The proof to verify
    /// @param generator The generator g
    /// @param publicKey The public value P = g^x
    /// @param context The context used in proof generation
    /// @return true if proof is valid
    static bool Verify(
        const SchnorrProof& proof,
        const FieldElement& generator,
        const FieldElement& publicKey,
        const std::vector<Byte>& context = {});
    
    /// Compute the Fiat-Shamir challenge
    static FieldElement ComputeChallenge(
        const FieldElement& publicKey,
        const FieldElement& commitment,
        const std::vector<Byte>& context);
};

// ============================================================================
// Pedersen Commitment Proof (Knowledge of Opening)
// ============================================================================

/**
 * Proof of knowledge of a Pedersen commitment opening.
 * 
 * Given C = g^v * h^r, proves knowledge of (v, r) without revealing them.
 * 
 * This uses a variant of Schnorr proof for commitments.
 */
struct PedersenOpeningProof {
    /// Commitment to randomness: A = g^s_v * h^s_r
    FieldElement commitment;
    
    /// Response for value: z_v = s_v + c*v
    FieldElement responseValue;
    
    /// Response for randomness: z_r = s_r + c*r
    FieldElement responseRandomness;
    
    /// Create empty proof
    PedersenOpeningProof() = default;
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<PedersenOpeningProof> FromBytes(const Byte* data, size_t len);
    
    /// Check if well-formed
    bool IsWellFormed() const;
};

/**
 * Prover for Pedersen commitment opening proofs.
 */
class PedersenOpeningProver {
public:
    /// Create a proof of knowledge of commitment opening
    /// @param value The committed value v
    /// @param randomness The blinding factor r
    /// @param generatorG Generator g for value
    /// @param generatorH Generator h for randomness
    /// @param commitment The commitment C = g^v * h^r
    /// @param context Additional context for Fiat-Shamir
    static PedersenOpeningProof Prove(
        const FieldElement& value,
        const FieldElement& randomness,
        const FieldElement& generatorG,
        const FieldElement& generatorH,
        const FieldElement& commitment,
        const std::vector<Byte>& context = {});
};

/**
 * Verifier for Pedersen commitment opening proofs.
 */
class PedersenOpeningVerifier {
public:
    /// Verify a Pedersen opening proof
    /// @param proof The proof to verify
    /// @param generatorG Generator g for value
    /// @param generatorH Generator h for randomness
    /// @param commitment The commitment C
    /// @param context The context used in proof generation
    /// @return true if proof is valid
    static bool Verify(
        const PedersenOpeningProof& proof,
        const FieldElement& generatorG,
        const FieldElement& generatorH,
        const FieldElement& commitment,
        const std::vector<Byte>& context = {});
};

// ============================================================================
// Equality Proof (Two Commitments Have Same Value)
// ============================================================================

/**
 * Proof that two Pedersen commitments contain the same value.
 * 
 * Given C1 = g^v * h1^r1 and C2 = g^v * h2^r2, proves they commit to
 * the same value v without revealing it.
 * 
 * Uses parallel Schnorr-like proofs with a shared value component.
 */
struct EqualityProof {
    /// First commitment: A1 = g^s_v * h1^s_r1
    FieldElement commitment1;
    
    /// Second commitment: A2 = g^s_v * h2^s_r2 (same s_v binds the proofs)
    FieldElement commitment2;
    
    /// Response for value (shared between both)
    FieldElement responseValue;
    
    /// Response for first randomness
    FieldElement responseRandom1;
    
    /// Response for second randomness
    FieldElement responseRandom2;
    
    /// Create empty proof
    EqualityProof() = default;
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<EqualityProof> FromBytes(const Byte* data, size_t len);
    
    /// Check if well-formed
    bool IsWellFormed() const;
};

/**
 * Prover for equality proofs.
 */
class EqualityProver {
public:
    /// Create a proof that two commitments have the same value
    static EqualityProof Prove(
        const FieldElement& value,
        const FieldElement& randomness1,
        const FieldElement& randomness2,
        const FieldElement& generatorG,
        const FieldElement& generatorH1,
        const FieldElement& generatorH2,
        const FieldElement& commitment1,
        const FieldElement& commitment2,
        const std::vector<Byte>& context = {});
};

/**
 * Verifier for equality proofs.
 */
class EqualityVerifier {
public:
    /// Verify an equality proof
    static bool Verify(
        const EqualityProof& proof,
        const FieldElement& generatorG,
        const FieldElement& generatorH1,
        const FieldElement& generatorH2,
        const FieldElement& commitment1,
        const FieldElement& commitment2,
        const std::vector<Byte>& context = {});
};

// ============================================================================
// OR Proof (Disjunction)
// ============================================================================

/**
 * Proof that one of two statements is true (disjunction).
 * 
 * Used for membership proofs where you prove you know the opening
 * of one of several commitments without revealing which one.
 */
struct OrProof {
    /// Commitments for each branch
    std::vector<FieldElement> commitments;
    
    /// Challenges for each branch (sum to main challenge)
    std::vector<FieldElement> challenges;
    
    /// Responses for each branch
    std::vector<FieldElement> responses;
    
    /// Number of branches
    size_t NumBranches() const { return commitments.size(); }
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<OrProof> FromBytes(const Byte* data, size_t len);
    
    /// Check if well-formed
    bool IsWellFormed() const;
};

/**
 * Prover for OR proofs (1-of-N).
 */
class OrProver {
public:
    /// Create a 1-of-N OR proof
    /// @param witnessIndex Index of the branch where prover knows the witness
    /// @param witness The secret witness value
    /// @param generators Generators for each branch
    /// @param publicValues Public values for each branch
    /// @param context Additional context
    static OrProof Prove(
        size_t witnessIndex,
        const FieldElement& witness,
        const std::vector<FieldElement>& generators,
        const std::vector<FieldElement>& publicValues,
        const std::vector<Byte>& context = {});
};

/**
 * Verifier for OR proofs.
 */
class OrVerifier {
public:
    /// Verify a 1-of-N OR proof
    static bool Verify(
        const OrProof& proof,
        const std::vector<FieldElement>& generators,
        const std::vector<FieldElement>& publicValues,
        const std::vector<Byte>& context = {});
};

// ============================================================================
// Generator Functions
// ============================================================================

/// Get the standard generator G for Pedersen commitments
FieldElement GetGeneratorG();

/// Get the standard generator H for Pedersen commitments
FieldElement GetGeneratorH();

/// Get additional generators for vector commitments
std::vector<FieldElement> GetGenerators(size_t count);

/// Derive a generator from a seed (nothing-up-my-sleeve)
FieldElement DeriveGenerator(const std::string& seed);

} // namespace identity
} // namespace shurium

#endif // SHURIUM_IDENTITY_SIGMA_H
