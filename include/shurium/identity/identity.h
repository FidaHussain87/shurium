// SHURIUM - Identity Management System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Central identity management for the SHURIUM UBI system.
// Provides:
// - Identity registration and verification
// - Identity tree management (Merkle tree of all identities)
// - Integration with ZK proofs for anonymous claims
// - UBI claim processing

#ifndef SHURIUM_IDENTITY_IDENTITY_H
#define SHURIUM_IDENTITY_IDENTITY_H

#include <shurium/core/types.h>
#include <shurium/crypto/field.h>
#include <shurium/crypto/keys.h>
#include <shurium/identity/commitment.h>
#include <shurium/identity/nullifier.h>
#include <shurium/identity/zkproof.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace shurium {
namespace identity {

// ============================================================================
// Identity Status
// ============================================================================

/// Status of a registered identity
enum class IdentityStatus {
    /// Identity is pending verification
    Pending,
    
    /// Identity is active and can claim UBI
    Active,
    
    /// Identity is suspended (temporary)
    Suspended,
    
    /// Identity is revoked (permanent)
    Revoked,
    
    /// Identity has expired
    Expired
};

// ============================================================================
// Identity Secrets (User's private data)
// ============================================================================

/**
 * Secret data for an identity.
 * 
 * SECURITY: This should only exist on the user's device and never
 * be transmitted or stored by the network. It contains all the
 * information needed to prove ownership of an identity.
 */
struct IdentitySecrets {
    /// Master seed (from which all other secrets are derived)
    std::array<Byte, 32> masterSeed;
    
    /// Secret key (for identity commitment)
    FieldElement secretKey;
    
    /// Nullifier key (for generating nullifiers)
    FieldElement nullifierKey;
    
    /// Trapdoor (additional randomness)
    FieldElement trapdoor;
    
    /// Index in the identity tree
    uint64_t treeIndex;
    
    /// Generate new identity secrets
    static IdentitySecrets Generate();
    
    /// Derive secrets from master seed
    static IdentitySecrets FromMasterSeed(const std::array<Byte, 32>& seed);
    
    /// Compute the identity commitment
    IdentityCommitment GetCommitment() const;
    
    /// Derive nullifier for an epoch
    Nullifier DeriveNullifier(EpochId epoch,
                              const FieldElement& domain = Nullifier::DOMAIN_UBI) const;
    
    /// Encrypt secrets for storage
    std::vector<Byte> Encrypt(const std::array<Byte, 32>& key) const;
    
    /// Decrypt secrets
    static std::optional<IdentitySecrets> Decrypt(const Byte* data, size_t len,
                                                   const std::array<Byte, 32>& key);
    
    /// Securely clear secrets from memory
    void Clear();
    
    /// Destructor - securely clears memory
    ~IdentitySecrets();
};

// ============================================================================
// Identity Record (Public on-chain data)
// ============================================================================

/**
 * Public record of a registered identity.
 * 
 * This is stored on-chain and contains only the commitment and metadata.
 * The actual identity secrets are never revealed.
 */
struct IdentityRecord {
    /// Unique identity ID (hash of commitment)
    Hash256 id;
    
    /// The identity commitment
    IdentityCommitment commitment;
    
    /// Current status
    IdentityStatus status;
    
    /// Block height when registered
    uint32_t registrationHeight;
    
    /// Block height when status last changed
    uint32_t lastUpdateHeight;
    
    /// Expiration height (0 = never)
    uint32_t expirationHeight;
    
    /// Index in the identity tree
    uint64_t treeIndex;
    
    /// Registration timestamp
    int64_t registrationTime;
    
    /// Create a new identity record
    static IdentityRecord Create(const IdentityCommitment& commitment,
                                  uint32_t height,
                                  int64_t timestamp);
    
    /// Check if identity is currently active
    bool IsActive() const { return status == IdentityStatus::Active; }
    
    /// Check if identity can claim UBI
    bool CanClaimUBI(uint32_t currentHeight) const;
    
    /// Serialize
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize
    static std::optional<IdentityRecord> FromBytes(const Byte* data, size_t len);
    
    /// Comparison by ID
    bool operator==(const IdentityRecord& other) const;
    bool operator<(const IdentityRecord& other) const;
};

// ============================================================================
// Identity Registration Request
// ============================================================================

/**
 * A request to register a new identity.
 * 
 * Registration requires proving uniqueness (not already registered) and
 * potentially additional verification (e.g., biometric, social vouching).
 */
struct RegistrationRequest {
    /// The identity commitment to register
    IdentityCommitment commitment;
    
    /// Registration proof (if required)
    std::optional<ZKProof> registrationProof;
    
    /// External verification data (e.g., verifier signatures)
    std::vector<Byte> verificationData;
    
    /// Timestamp of request
    int64_t timestamp;
    
    /// Validate the request structure
    bool IsValid() const;
    
    /// Serialize
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize
    static std::optional<RegistrationRequest> FromBytes(const Byte* data, size_t len);
};

// ============================================================================
// UBI Claim
// ============================================================================

/**
 * A UBI claim from a registered identity.
 * 
 * Contains the ZK proof that the claimer:
 * 1. Owns a registered identity
 * 2. Has not claimed in this epoch
 */
struct UBIClaim {
    /// The nullifier (proves this identity hasn't claimed)
    Nullifier nullifier;
    
    /// Epoch being claimed
    EpochId epoch;
    
    /// Recipient address (where to send UBI)
    std::vector<Byte> recipientScript;
    
    /// ZK proof of valid claim
    IdentityProof proof;
    
    /// Claim timestamp
    int64_t timestamp;
    
    /// Validate claim structure
    bool IsValid() const;
    
    /// Verify the claim (including ZK proof)
    bool Verify(const FieldElement& identityRoot,
                const NullifierSet& usedNullifiers) const;
    
    /// Serialize
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize
    static std::optional<UBIClaim> FromBytes(const Byte* data, size_t len);
};

// ============================================================================
// Identity Manager
// ============================================================================

/**
 * Manages the identity system state.
 * 
 * Maintains:
 * - Set of registered identities
 * - Identity Merkle tree
 * - Used nullifiers per epoch
 * - Claim processing
 */
class IdentityManager {
public:
    /// Configuration
    struct Config {
        /// Maximum identities (0 = unlimited)
        uint64_t maxIdentities = 0;
        
        /// Identity expiration (in blocks, 0 = never)
        uint32_t identityLifetime = 0;
        
        /// Minimum blocks before identity becomes active
        uint32_t activationDelay = 100;
        
        /// Epoch duration in seconds
        int64_t epochDuration = 604800;  // 1 week
        
        /// Genesis timestamp
        int64_t genesisTime = 0;
        
        /// Require registration proof?
        bool requireRegistrationProof = false;
        
        Config() = default;
    };
    
    /// Statistics
    struct Stats {
        uint64_t totalIdentities;
        uint64_t activeIdentities;
        uint64_t pendingIdentities;
        uint64_t revokedIdentities;
        uint64_t claimsThisEpoch;
        EpochId currentEpoch;
    };
    
    /// Default constructor
    IdentityManager();
    
    /// Constructor with config
    explicit IdentityManager(const Config& config);
    
    /// Destructor
    ~IdentityManager();
    
    /// Initialize from serialized state
    bool Initialize(const Byte* data, size_t len);
    
    /// Get configuration
    const Config& GetConfig() const { return config_; }
    
    // --- Epoch Management ---
    
    /// Set current block height and time
    void SetBlockContext(uint32_t height, int64_t timestamp);
    
    /// Get current epoch
    EpochId GetCurrentEpoch() const;
    
    /// Advance to next epoch (called when epoch boundary crossed)
    void AdvanceEpoch(EpochId newEpoch);
    
    // --- Identity Registration ---
    
    /// Register a new identity
    /// @param request Registration request
    /// @return Identity record on success, nullopt on failure
    std::optional<IdentityRecord> RegisterIdentity(const RegistrationRequest& request);
    
    /// Check if a commitment is already registered
    bool IsCommitmentRegistered(const IdentityCommitment& commitment) const;
    
    /// Get identity record by commitment
    std::optional<IdentityRecord> GetIdentity(const IdentityCommitment& commitment) const;
    
    /// Get identity record by ID
    std::optional<IdentityRecord> GetIdentityById(const Hash256& id) const;
    
    /// Get identity by tree index
    std::optional<IdentityRecord> GetIdentityByIndex(uint64_t index) const;
    
    /// Update identity status
    bool UpdateIdentityStatus(const Hash256& id, IdentityStatus newStatus);
    
    /// Get all identities with a status
    std::vector<IdentityRecord> GetIdentitiesByStatus(IdentityStatus status) const;
    
    // --- Identity Tree ---
    
    /// Get the identity tree root
    FieldElement GetIdentityRoot() const;
    
    /// Generate membership proof for an identity
    std::optional<VectorCommitment::MerkleProof> 
    GetMembershipProof(const IdentityCommitment& commitment) const;
    
    /// Verify membership proof
    bool VerifyMembershipProof(const IdentityCommitment& commitment,
                               const VectorCommitment::MerkleProof& proof) const;
    
    // --- UBI Claims ---
    
    /// Process a UBI claim
    /// @param claim The claim to process
    /// @return true if claim is valid and processed
    bool ProcessUBIClaim(const UBIClaim& claim);
    
    /// Check if a nullifier has been used
    bool IsNullifierUsed(const Nullifier& nullifier) const;
    
    /// Get number of claims in current epoch
    uint64_t GetClaimsThisEpoch() const;
    
    /// Get used nullifier set (for epoch)
    const NullifierSet& GetNullifierSet() const { return nullifierSet_; }
    
    // --- Statistics ---
    
    /// Get current statistics
    Stats GetStats() const;
    
    /// Get total number of registered identities
    uint64_t GetIdentityCount() const;
    
    // --- Serialization ---
    
    /// Serialize entire state
    std::vector<Byte> Serialize() const;
    
    /// Get state hash (for consensus)
    Hash256 GetStateHash() const;

private:
    Config config_;
    
    /// Current block height
    uint32_t currentHeight_{0};
    
    /// Current timestamp
    int64_t currentTime_{0};
    
    /// Current epoch
    EpochId currentEpoch_{0};
    
    /// Identity tree (Merkle tree of commitments)
    VectorCommitment identityTree_;
    
    /// Map from commitment hash to identity record
    std::map<CommitmentHash, IdentityRecord> identities_;
    
    /// Map from identity ID to commitment hash
    std::map<Hash256, CommitmentHash> idToCommitment_;
    
    /// Used nullifiers
    NullifierSet nullifierSet_;
    
    /// Proof verifier
    std::unique_ptr<ProofVerifier> verifier_;
    
    /// Mutex for thread safety
    mutable std::mutex mutex_;
    
    /// Add identity to tree
    uint64_t AddToTree(const IdentityCommitment& commitment);
    
    /// Validate registration request
    bool ValidateRegistration(const RegistrationRequest& request) const;
};

// ============================================================================
// Identity Utilities
// ============================================================================

/// Get string name for identity status
std::string IdentityStatusToString(IdentityStatus status);

/// Parse identity status from string
std::optional<IdentityStatus> IdentityStatusFromString(const std::string& str);

/// Compute identity ID from commitment
Hash256 ComputeIdentityId(const IdentityCommitment& commitment);

/// Generate a random master seed
std::array<Byte, 32> GenerateMasterSeed();

} // namespace identity
} // namespace shurium

#endif // SHURIUM_IDENTITY_IDENTITY_H
