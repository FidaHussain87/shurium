// SHURIUM - Zero-Knowledge Proof System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements a simplified ZK proof system for identity verification.
// In production, this would integrate with a full ZK library (e.g., libsnark,
// bellman, or circom/snarkjs). This implementation provides:
//
// - Proof generation and verification interfaces
// - Circuit definitions for identity proofs
// - Groth16-style proof structure (placeholder for real proofs)

#ifndef SHURIUM_IDENTITY_ZKPROOF_H
#define SHURIUM_IDENTITY_ZKPROOF_H

#include <shurium/core/types.h>
#include <shurium/crypto/field.h>
#include <shurium/identity/commitment.h>
#include <shurium/identity/nullifier.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace identity {

// ============================================================================
// Proof Types
// ============================================================================

/// Type of zero-knowledge proof
enum class ProofType {
    /// Identity registration proof
    Registration,
    
    /// UBI claim proof (prove membership + generate nullifier)
    UBIClaim,
    
    /// Identity update proof (prove old identity, commit to new)
    Update,
    
    /// Membership proof (prove identity is in set without revealing which)
    Membership,
    
    /// Range proof (prove value is in range without revealing it)
    Range,
    
    /// Custom/generic proof
    Custom
};

/// Proof system used
enum class ProofSystem {
    /// Groth16 (trusted setup, small proofs)
    Groth16,
    
    /// PLONK (universal setup)
    PLONK,
    
    /// Bulletproofs (no trusted setup, larger proofs)
    Bulletproofs,
    
    /// STARK (no trusted setup, larger proofs, post-quantum)
    STARK,
    
    /// Placeholder for development/testing
    Placeholder
};

// ============================================================================
// Groth16-style Proof Structure
// ============================================================================

/**
 * A Groth16-style zero-knowledge proof.
 * 
 * A Groth16 proof consists of three group elements (A, B, C) that can
 * be verified against public inputs using a verification key.
 * 
 * This is a simplified representation - real proofs would use actual
 * elliptic curve points.
 */
struct Groth16Proof {
    /// Size of each proof element (compressed G1/G2 points would be 32-64 bytes)
    static constexpr size_t ELEMENT_SIZE = 32;
    
    /// Proof element A (G1 point)
    std::array<Byte, 64> proofA;
    
    /// Proof element B (G2 point - larger)
    std::array<Byte, 128> proofB;
    
    /// Proof element C (G1 point)
    std::array<Byte, 64> proofC;
    
    /// Create an empty proof
    Groth16Proof();
    
    /// Create from raw bytes
    static std::optional<Groth16Proof> FromBytes(const Byte* data, size_t len);
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Check if proof appears well-formed (not verified)
    bool IsWellFormed() const;
    
    /// Convert to hex string
    std::string ToHex() const;
    
    /// Parse from hex
    static std::optional<Groth16Proof> FromHex(const std::string& hex);
};

// ============================================================================
// Verification Key
// ============================================================================

/**
 * Verification key for a ZK circuit.
 * 
 * Generated during trusted setup, used to verify proofs.
 */
struct VerificationKey {
    /// Circuit identifier
    std::string circuitId;
    
    /// Proof system
    ProofSystem system;
    
    /// Key data (format depends on proof system)
    std::vector<Byte> keyData;
    
    /// Number of public inputs expected
    uint32_t numPublicInputs{0};
    
    /// Serialize
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize
    static std::optional<VerificationKey> FromBytes(const Byte* data, size_t len);
    
    /// Check if key is valid
    bool IsValid() const;
};

// ============================================================================
// Public Inputs
// ============================================================================

/**
 * Public inputs for a ZK proof.
 * 
 * These are the values that are publicly visible and constrained by the proof.
 * The proof shows that the prover knows private inputs (witness) such that
 * the circuit constraints are satisfied given these public inputs.
 */
struct PublicInputs {
    /// Input values as field elements
    std::vector<FieldElement> values;
    
    /// Create empty inputs
    PublicInputs() = default;
    
    /// Create with values
    explicit PublicInputs(std::vector<FieldElement> vals) : values(std::move(vals)) {}
    
    /// Add an input
    void Add(const FieldElement& value) { values.push_back(value); }
    
    /// Get number of inputs
    size_t Count() const { return values.size(); }
    
    /// Check if empty
    bool IsEmpty() const { return values.empty(); }
    
    /// Serialize
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize
    static std::optional<PublicInputs> FromBytes(const Byte* data, size_t len);
};

// ============================================================================
// ZK Proof (Generic wrapper)
// ============================================================================

/**
 * A generic zero-knowledge proof with its public inputs.
 * 
 * This wraps the proof data along with metadata needed for verification.
 */
class ZKProof {
public:
    /// Maximum proof size (for validation)
    static constexpr size_t MAX_PROOF_SIZE = 16384;  // 16KB
    
    /// Default constructor - invalid proof
    ZKProof() : type_(ProofType::Custom), system_(ProofSystem::Placeholder) {}
    
    /// Construct with type and system
    ZKProof(ProofType type, ProofSystem system)
        : type_(type), system_(system) {}
    
    /// Get proof type
    ProofType GetType() const { return type_; }
    
    /// Get proof system
    ProofSystem GetSystem() const { return system_; }
    
    /// Get public inputs
    const PublicInputs& GetPublicInputs() const { return publicInputs_; }
    
    /// Set public inputs
    void SetPublicInputs(const PublicInputs& inputs) { publicInputs_ = inputs; }
    
    /// Get raw proof data
    const std::vector<Byte>& GetProofData() const { return proofData_; }
    
    /// Set raw proof data
    void SetProofData(const std::vector<Byte>& data) { proofData_ = data; }
    
    /// Get Groth16 proof (if applicable)
    std::optional<Groth16Proof> GetGroth16Proof() const;
    
    /// Set Groth16 proof
    void SetGroth16Proof(const Groth16Proof& proof);
    
    /// Check if proof appears valid (structural check only)
    bool IsValid() const;
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<ZKProof> FromBytes(const Byte* data, size_t len);
    
    /// Convert to hex
    std::string ToHex() const;
    
    /// Parse from hex
    static std::optional<ZKProof> FromHex(const std::string& hex);

private:
    ProofType type_;
    ProofSystem system_;
    PublicInputs publicInputs_;
    std::vector<Byte> proofData_;
};

// ============================================================================
// Identity Proof (Specific proof for SHURIUM identity system)
// ============================================================================

/**
 * An identity proof for the SHURIUM UBI system.
 * 
 * This proves:
 * 1. The prover knows the secrets for a registered identity
 * 2. The identity commitment is in the identity Merkle tree
 * 3. The nullifier is correctly derived from the identity
 * 4. The nullifier has not been used before (checked externally)
 * 
 * Without revealing which identity is making the claim.
 */
class IdentityProof {
public:
    /// Create an empty/invalid proof
    IdentityProof() = default;
    
    /// Create a UBI claim proof
    /// @param identityRoot Root of the identity Merkle tree
    /// @param nullifier The derived nullifier for this epoch
    /// @param epoch The epoch for this claim
    /// @param secretKey Prover's secret key (PRIVATE - not stored)
    /// @param nullifierKey Prover's nullifier key (PRIVATE - not stored)
    /// @param trapdoor Prover's trapdoor (PRIVATE - not stored)
    /// @param merkleProof Proof of identity membership
    /// @return The identity proof
    static IdentityProof CreateUBIClaimProof(
        const FieldElement& identityRoot,
        const Nullifier& nullifier,
        EpochId epoch,
        const FieldElement& secretKey,
        const FieldElement& nullifierKey,
        const FieldElement& trapdoor,
        const VectorCommitment::MerkleProof& merkleProof);
    
    /// Verify the proof
    /// @param identityRoot Expected root of identity tree
    /// @param nullifierSet Set of used nullifiers (for freshness check)
    /// @return true if proof is valid
    bool Verify(const FieldElement& identityRoot,
                const NullifierSet& nullifierSet) const;
    
    /// Verify without nullifier set (just ZK verification)
    bool VerifyProof(const FieldElement& identityRoot) const;
    
    /// Get the nullifier
    const Nullifier& GetNullifier() const { return nullifier_; }
    
    /// Get the epoch
    EpochId GetEpoch() const { return epoch_; }
    
    /// Get the underlying ZK proof
    const ZKProof& GetZKProof() const { return zkProof_; }
    
    /// Check if proof is valid (structural)
    bool IsValid() const;
    
    /// Serialize
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize
    static std::optional<IdentityProof> FromBytes(const Byte* data, size_t len);

private:
    Nullifier nullifier_;
    EpochId epoch_{0};
    ZKProof zkProof_;
};

// ============================================================================
// Proof Verifier
// ============================================================================

/**
 * Verifier for ZK proofs.
 * 
 * This class handles verification of different proof types using
 * the appropriate verification keys.
 */
class ProofVerifier {
public:
    /// Create a verifier
    ProofVerifier();
    
    /// Destructor
    ~ProofVerifier();
    
    /// Register a verification key
    void RegisterKey(const std::string& circuitId, const VerificationKey& key);
    
    /// Check if a key is registered
    bool HasKey(const std::string& circuitId) const;
    
    /// Get a registered key
    std::optional<VerificationKey> GetKey(const std::string& circuitId) const;
    
    /// Verify a proof
    /// @param proof The proof to verify
    /// @param circuitId Which circuit this proof is for
    /// @return true if proof is valid
    bool Verify(const ZKProof& proof, const std::string& circuitId) const;
    
    /// Verify a Groth16 proof directly
    bool VerifyGroth16(const Groth16Proof& proof,
                       const PublicInputs& inputs,
                       const VerificationKey& key) const;
    
    /// Verify an identity proof
    bool VerifyIdentityProof(const IdentityProof& proof,
                             const FieldElement& identityRoot) const;
    
    /// Get singleton instance (for convenience)
    static ProofVerifier& Instance();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Proof Generator (for testing/development)
// ============================================================================

/**
 * Generator for ZK proofs.
 * 
 * NOTE: In production, proof generation would typically happen client-side
 * using specialized proving software. This generator is primarily for
 * testing and development purposes.
 */
class ProofGenerator {
public:
    /// Create a generator
    ProofGenerator();
    
    /// Destructor
    ~ProofGenerator();
    
    /// Generate a UBI claim proof
    /// @param secretKey User's secret key
    /// @param nullifierKey User's nullifier key
    /// @param trapdoor User's trapdoor
    /// @param identityRoot Current identity tree root
    /// @param merkleProof Proof of membership in identity tree
    /// @param epoch Current epoch
    /// @return Generated proof or nullopt on error
    std::optional<IdentityProof> GenerateUBIClaimProof(
        const FieldElement& secretKey,
        const FieldElement& nullifierKey,
        const FieldElement& trapdoor,
        const FieldElement& identityRoot,
        const VectorCommitment::MerkleProof& merkleProof,
        EpochId epoch) const;
    
    /// Generate a placeholder proof (for testing)
    ZKProof GeneratePlaceholderProof(ProofType type,
                                      const PublicInputs& publicInputs) const;
    
    /// Get singleton instance
    static ProofGenerator& Instance();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Get string name for proof type
std::string ProofTypeToString(ProofType type);

/// Parse proof type from string
std::optional<ProofType> ProofTypeFromString(const std::string& str);

/// Get string name for proof system
std::string ProofSystemToString(ProofSystem system);

/// Parse proof system from string
std::optional<ProofSystem> ProofSystemFromString(const std::string& str);

} // namespace identity
} // namespace shurium

#endif // SHURIUM_IDENTITY_ZKPROOF_H
