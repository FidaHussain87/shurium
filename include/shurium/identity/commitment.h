// SHURIUM - Cryptographic Commitment Schemes
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements Pedersen-style commitments using Poseidon hash for ZK-friendly
// identity commitments in the SHURIUM UBI system.
//
// A commitment allows one to commit to a value while keeping it hidden,
// with the ability to reveal it later. Properties:
// - Hiding: Commitment reveals nothing about the committed value
// - Binding: Cannot open commitment to a different value

#ifndef SHURIUM_IDENTITY_COMMITMENT_H
#define SHURIUM_IDENTITY_COMMITMENT_H

#include <shurium/core/types.h>
#include <shurium/crypto/field.h>
#include <shurium/crypto/poseidon.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace identity {

// ============================================================================
// Commitment Types
// ============================================================================

/// A 32-byte commitment value
using CommitmentHash = std::array<Byte, 32>;

/// Opening information for a commitment (value + randomness)
struct CommitmentOpening {
    /// The committed value
    FieldElement value;
    
    /// The randomness (blinding factor)
    FieldElement randomness;
    
    /// Optional auxiliary data (for extended commitments)
    std::vector<FieldElement> auxData;
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<CommitmentOpening> FromBytes(const Byte* data, size_t len);
};

// ============================================================================
// Pedersen Commitment (Hash-based using Poseidon)
// ============================================================================

/**
 * Pedersen-style commitment using Poseidon hash.
 * 
 * C = Poseidon(value, randomness)
 * 
 * This is a ZK-friendly commitment scheme suitable for use in SNARKs.
 * The Poseidon hash provides:
 * - Computational hiding (given C, hard to find value without randomness)
 * - Computational binding (hard to find two openings for same C)
 */
class PedersenCommitment {
public:
    /// Size of commitment in bytes
    static constexpr size_t SIZE = 32;
    
    /// Default constructor - empty commitment
    PedersenCommitment() : hash_{} {}
    
    /// Construct from hash bytes
    explicit PedersenCommitment(const CommitmentHash& hash) : hash_(hash) {}
    
    /// Construct from field element
    explicit PedersenCommitment(const FieldElement& element);
    
    /// Create a new commitment to a value
    /// @param value The value to commit to
    /// @param randomness The blinding factor (should be cryptographically random)
    /// @return The commitment
    static PedersenCommitment Commit(const FieldElement& value,
                                      const FieldElement& randomness);
    
    /// Create a commitment with automatically generated randomness
    /// @param value The value to commit to
    /// @param[out] randomness The generated randomness (save this to open later!)
    /// @return The commitment
    static PedersenCommitment CommitWithRandomness(const FieldElement& value,
                                                    FieldElement& randomness);
    
    /// Verify that an opening is valid for this commitment
    /// @param opening The opening information
    /// @return true if opening matches this commitment
    bool Verify(const CommitmentOpening& opening) const;
    
    /// Verify with explicit value and randomness
    bool Verify(const FieldElement& value, const FieldElement& randomness) const;
    
    /// Get the commitment hash
    const CommitmentHash& GetHash() const { return hash_; }
    
    /// Get as field element
    FieldElement ToFieldElement() const;
    
    /// Get raw bytes
    const Byte* data() const { return hash_.data(); }
    size_t size() const { return SIZE; }
    
    /// Convert to hex string
    std::string ToHex() const;
    
    /// Parse from hex string
    static std::optional<PedersenCommitment> FromHex(const std::string& hex);
    
    /// Check if commitment is empty/zero
    bool IsEmpty() const;
    
    /// Comparison operators
    bool operator==(const PedersenCommitment& other) const;
    bool operator!=(const PedersenCommitment& other) const;
    bool operator<(const PedersenCommitment& other) const;
    
    /// Serialization
    template<typename Stream>
    void Serialize(Stream& s) const {
        s.Write(reinterpret_cast<const char*>(hash_.data()), SIZE);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        s.Read(reinterpret_cast<char*>(hash_.data()), SIZE);
    }

private:
    CommitmentHash hash_;
};

// ============================================================================
// Identity Commitment
// ============================================================================

/**
 * Identity commitment for the SHURIUM UBI system.
 * 
 * An identity commitment proves ownership of a secret identity without
 * revealing it. The commitment structure is:
 * 
 * IdentityCommitment = Poseidon(secretKey, nullifierKey, trapdoor)
 * 
 * Where:
 * - secretKey: The user's main secret (derived from their master key)
 * - nullifierKey: Used to generate nullifiers for double-spend prevention
 * - trapdoor: Additional randomness for hiding
 * 
 * This allows:
 * - Anonymous UBI claims (prove you're registered without revealing identity)
 * - Double-claim prevention (nullifiers are deterministic per epoch)
 * - Revocability (if needed, via trapdoor revelation)
 */
class IdentityCommitment {
public:
    /// Size of identity commitment
    static constexpr size_t SIZE = 32;
    
    /// Default constructor
    IdentityCommitment() : commitment_{} {}
    
    /// Construct from raw bytes
    explicit IdentityCommitment(const CommitmentHash& hash) : commitment_(hash) {}
    
    /// Construct from Pedersen commitment
    explicit IdentityCommitment(const PedersenCommitment& commitment)
        : commitment_(commitment) {}
    
    /// Create a new identity commitment
    /// @param secretKey The user's secret key (keep this private!)
    /// @param nullifierKey Key for generating nullifiers
    /// @param trapdoor Random trapdoor value
    /// @return The identity commitment
    static IdentityCommitment Create(const FieldElement& secretKey,
                                      const FieldElement& nullifierKey,
                                      const FieldElement& trapdoor);
    
    /// Generate keys and create identity commitment
    /// @param[out] secretKey Generated secret key
    /// @param[out] nullifierKey Generated nullifier key
    /// @param[out] trapdoor Generated trapdoor
    /// @return The identity commitment
    static IdentityCommitment Generate(FieldElement& secretKey,
                                        FieldElement& nullifierKey,
                                        FieldElement& trapdoor);
    
    /// Verify identity commitment matches the given secrets
    bool Verify(const FieldElement& secretKey,
                const FieldElement& nullifierKey,
                const FieldElement& trapdoor) const;
    
    /// Get underlying commitment
    const PedersenCommitment& GetCommitment() const { return commitment_; }
    
    /// Get hash
    const CommitmentHash& GetHash() const { return commitment_.GetHash(); }
    
    /// Get as field element
    FieldElement ToFieldElement() const { return commitment_.ToFieldElement(); }
    
    /// Convert to hex
    std::string ToHex() const { return commitment_.ToHex(); }
    
    /// Parse from hex
    static std::optional<IdentityCommitment> FromHex(const std::string& hex);
    
    /// Check if empty
    bool IsEmpty() const { return commitment_.IsEmpty(); }
    
    /// Comparison
    bool operator==(const IdentityCommitment& other) const;
    bool operator!=(const IdentityCommitment& other) const;
    bool operator<(const IdentityCommitment& other) const;
    
    /// Serialization
    template<typename Stream>
    void Serialize(Stream& s) const {
        commitment_.Serialize(s);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        commitment_.Unserialize(s);
    }

private:
    PedersenCommitment commitment_;
};

// ============================================================================
// Vector Commitment (Merkle Tree based)
// ============================================================================

/**
 * Vector commitment using a Poseidon Merkle tree.
 * 
 * This allows committing to a vector of values and later proving
 * membership of individual elements. Used for:
 * - Identity set commitment (all registered identities)
 * - Efficient membership proofs in ZK circuits
 */
class VectorCommitment {
public:
    /// A Merkle proof for a single element
    struct MerkleProof {
        /// Index of the element in the vector
        uint64_t index;
        
        /// Sibling hashes along the path to the root
        std::vector<FieldElement> siblings;
        
        /// Direction bits (0 = left, 1 = right) for each level
        std::vector<bool> pathBits;
        
        /// Serialize
        std::vector<Byte> ToBytes() const;
        
        /// Deserialize
        static std::optional<MerkleProof> FromBytes(const Byte* data, size_t len);
    };
    
    /// Default constructor - empty tree
    VectorCommitment();
    
    /// Construct from a vector of elements
    explicit VectorCommitment(const std::vector<FieldElement>& elements);
    
    /// Construct from existing root
    explicit VectorCommitment(const FieldElement& root, uint64_t size);
    
    /// Add an element to the tree
    /// @return Index of the added element
    uint64_t Add(const FieldElement& element);
    
    /// Add multiple elements
    void AddBatch(const std::vector<FieldElement>& elements);
    
    /// Get the root hash (commitment)
    FieldElement GetRoot() const { return root_; }
    
    /// Get the number of elements
    uint64_t Size() const { return size_; }
    
    /// Get tree depth
    uint32_t Depth() const { return depth_; }
    
    /// Generate a membership proof for an element
    /// @param index Index of the element
    /// @return Merkle proof or nullopt if index out of range
    std::optional<MerkleProof> Prove(uint64_t index) const;
    
    /// Verify a membership proof
    /// @param element The element
    /// @param proof The Merkle proof
    /// @return true if proof is valid
    bool Verify(const FieldElement& element, const MerkleProof& proof) const;
    
    /// Static verification (without tree, just root)
    static bool VerifyProof(const FieldElement& root,
                            const FieldElement& element,
                            const MerkleProof& proof);
    
    /// Get element at index (if stored)
    std::optional<FieldElement> GetElement(uint64_t index) const;
    
    /// Check if tree is empty
    bool IsEmpty() const { return size_ == 0; }

private:
    /// Tree root
    FieldElement root_;
    
    /// Number of elements
    uint64_t size_{0};
    
    /// Tree depth (log2 of capacity)
    uint32_t depth_{0};
    
    /// Leaf nodes (stored for proof generation)
    std::vector<FieldElement> leaves_;
    
    /// Internal nodes (level -> nodes at that level)
    std::vector<std::vector<FieldElement>> levels_;
    
    /// Rebuild tree from leaves
    void RebuildTree();
    
    /// Get default (empty) leaf value
    static FieldElement DefaultLeaf();
    
    /// Compute parent hash from two children
    static FieldElement HashPair(const FieldElement& left, const FieldElement& right);
};

// ============================================================================
// Commitment Utilities
// ============================================================================

/// Generate cryptographically secure randomness as a field element
FieldElement GenerateRandomFieldElement();

/// Hash arbitrary data to a field element (for commitment inputs)
FieldElement HashToFieldElement(const Byte* data, size_t len);

/// Hash a string to a field element
inline FieldElement HashToFieldElement(const std::string& str) {
    return HashToFieldElement(reinterpret_cast<const Byte*>(str.data()), str.size());
}

} // namespace identity
} // namespace shurium

#endif // SHURIUM_IDENTITY_COMMITMENT_H
