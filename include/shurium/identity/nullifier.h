// SHURIUM - Nullifier System for Double-Claim Prevention
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Nullifiers are deterministic values derived from identity secrets that
// prevent double-spending/double-claiming in the UBI system.
//
// Key properties:
// - Deterministic: Same identity + epoch = same nullifier
// - Unlinkable: Cannot link nullifiers to identities without secrets
// - Non-reusable: Each nullifier can only be used once per epoch

#ifndef SHURIUM_IDENTITY_NULLIFIER_H
#define SHURIUM_IDENTITY_NULLIFIER_H

#include <shurium/core/types.h>
#include <shurium/crypto/field.h>
#include <shurium/crypto/poseidon.h>

#include <array>
#include <cstdint>
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
// Nullifier Types
// ============================================================================

/// A 32-byte nullifier value
using NullifierHash = std::array<Byte, 32>;

/// Epoch identifier (e.g., week number since genesis)
using EpochId = uint64_t;

// ============================================================================
// Nullifier
// ============================================================================

/**
 * A nullifier that prevents double-claims in the UBI system.
 * 
 * Nullifier = Poseidon(nullifierKey, epochId, domain)
 * 
 * Where:
 * - nullifierKey: From the identity commitment (kept secret)
 * - epochId: Current epoch (e.g., week number)
 * - domain: Domain separator to prevent cross-protocol attacks
 * 
 * The nullifier is unique per identity per epoch, allowing anonymous
 * claims while preventing the same identity from claiming twice.
 */
class Nullifier {
public:
    /// Size in bytes
    static constexpr size_t SIZE = 32;
    
    /// Domain separator for UBI claims
    static const FieldElement DOMAIN_UBI;
    
    /// Domain separator for voting
    static const FieldElement DOMAIN_VOTE;
    
    /// Domain separator for identity refresh
    static const FieldElement DOMAIN_REFRESH;
    
    /// Default constructor - empty nullifier
    Nullifier() : hash_{}, epochId_(0) {}
    
    /// Construct from hash
    explicit Nullifier(const NullifierHash& hash, EpochId epoch = 0)
        : hash_(hash), epochId_(epoch) {}
    
    /// Construct from field element
    explicit Nullifier(const FieldElement& element, EpochId epoch = 0);
    
    /// Derive a nullifier from secrets
    /// @param nullifierKey The nullifier key from identity
    /// @param epochId The epoch identifier
    /// @param domain Domain separator (default: UBI)
    /// @return The derived nullifier
    static Nullifier Derive(const FieldElement& nullifierKey,
                            EpochId epochId,
                            const FieldElement& domain = DOMAIN_UBI);
    
    /// Get the nullifier hash
    const NullifierHash& GetHash() const { return hash_; }
    
    /// Get as field element
    FieldElement ToFieldElement() const;
    
    /// Get the epoch
    EpochId GetEpoch() const { return epochId_; }
    
    /// Get raw bytes
    const Byte* data() const { return hash_.data(); }
    size_t size() const { return SIZE; }
    
    /// Convert to hex string
    std::string ToHex() const;
    
    /// Parse from hex
    static std::optional<Nullifier> FromHex(const std::string& hex, EpochId epoch = 0);
    
    /// Check if empty
    bool IsEmpty() const;
    
    /// Comparison operators
    bool operator==(const Nullifier& other) const;
    bool operator!=(const Nullifier& other) const;
    bool operator<(const Nullifier& other) const;
    
    /// Serialization
    template<typename Stream>
    void Serialize(Stream& s) const {
        s.Write(reinterpret_cast<const char*>(hash_.data()), SIZE);
        uint64_t epoch = epochId_;
        s.Write(reinterpret_cast<const char*>(&epoch), sizeof(epoch));
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        s.Read(reinterpret_cast<char*>(hash_.data()), SIZE);
        uint64_t epoch;
        s.Read(reinterpret_cast<char*>(&epoch), sizeof(epoch));
        epochId_ = epoch;
    }

private:
    NullifierHash hash_;
    EpochId epochId_;
};

// ============================================================================
// Nullifier Set (Database for tracking used nullifiers)
// ============================================================================

/**
 * A set of used nullifiers for double-spend prevention.
 * 
 * This tracks all nullifiers that have been used, organized by epoch.
 * Old epochs can be pruned to save space.
 */
class NullifierSet {
public:
    /// Result of attempting to add a nullifier
    enum class AddResult {
        Success,        ///< Nullifier was added successfully
        AlreadyExists,  ///< Nullifier already in set (double-spend attempt)
        InvalidEpoch,   ///< Epoch too old or too far in future
        SetFull         ///< Set has reached capacity
    };
    
    /// Configuration for nullifier set
    struct Config {
        /// Maximum epochs to keep (older are pruned)
        uint32_t maxEpochHistory = 52;  // ~1 year of weeks
        
        /// Maximum nullifiers per epoch (0 = unlimited)
        uint64_t maxPerEpoch = 0;
        
        /// Allow nullifiers from future epochs?
        bool allowFutureEpochs = false;
        
        /// Maximum future epoch offset allowed
        uint32_t maxFutureOffset = 1;
        
        Config() = default;
    };
    
    /// Default constructor
    NullifierSet();
    
    /// Constructor with config
    explicit NullifierSet(const Config& config);
    
    /// Destructor
    ~NullifierSet();
    
    /// Set the current epoch
    void SetCurrentEpoch(EpochId epoch);
    
    /// Get the current epoch
    EpochId GetCurrentEpoch() const { return currentEpoch_; }
    
    /// Check if a nullifier exists
    bool Contains(const Nullifier& nullifier) const;
    
    /// Check if a nullifier exists for a specific epoch
    bool Contains(const NullifierHash& hash, EpochId epoch) const;
    
    /// Add a nullifier to the set
    /// @param nullifier The nullifier to add
    /// @return Result indicating success or reason for failure
    AddResult Add(const Nullifier& nullifier);
    
    /// Add multiple nullifiers atomically
    /// @param nullifiers Nullifiers to add
    /// @return True if all were added, false if any duplicate found (none added)
    bool AddBatch(const std::vector<Nullifier>& nullifiers);
    
    /// Remove a nullifier (for rollback)
    bool Remove(const Nullifier& nullifier);
    
    /// Get count of nullifiers in an epoch
    uint64_t CountForEpoch(EpochId epoch) const;
    
    /// Get total count of nullifiers
    uint64_t TotalCount() const;
    
    /// Get all epochs with nullifiers
    std::vector<EpochId> GetEpochs() const;
    
    /// Prune old epochs
    /// @param keepEpochs Number of recent epochs to keep
    /// @return Number of nullifiers pruned
    uint64_t Prune(uint32_t keepEpochs);
    
    /// Prune epochs older than specified epoch
    uint64_t PruneOlderThan(EpochId epoch);
    
    /// Clear all nullifiers
    void Clear();
    
    /// Serialize the entire set
    std::vector<Byte> Serialize() const;
    
    /// Deserialize from bytes
    static std::unique_ptr<NullifierSet> Deserialize(const Byte* data, size_t len,
                                                      const Config& config);
    
    /// Get configuration
    const Config& GetConfig() const { return config_; }

private:
    Config config_;
    EpochId currentEpoch_{0};
    
    /// Nullifiers organized by epoch
    std::map<EpochId, std::set<NullifierHash>> epochNullifiers_;
    
    /// Mutex for thread safety
    mutable std::unique_ptr<std::mutex> mutex_;
    
    /// Validate epoch is acceptable
    bool IsValidEpoch(EpochId epoch) const;
};

// ============================================================================
// Epoch Utilities
// ============================================================================

/// Calculate epoch ID from Unix timestamp
/// @param timestamp Unix timestamp (seconds since epoch)
/// @param epochDuration Duration of each epoch in seconds (default: 1 week)
/// @param genesisTime Genesis timestamp (default: 0)
EpochId CalculateEpoch(int64_t timestamp,
                       int64_t epochDuration = 604800,  // 1 week
                       int64_t genesisTime = 0);

/// Get the start timestamp of an epoch
int64_t GetEpochStartTime(EpochId epoch,
                          int64_t epochDuration = 604800,
                          int64_t genesisTime = 0);

/// Get the end timestamp of an epoch
int64_t GetEpochEndTime(EpochId epoch,
                        int64_t epochDuration = 604800,
                        int64_t genesisTime = 0);

/// Check if a timestamp is within an epoch
bool IsInEpoch(int64_t timestamp, EpochId epoch,
               int64_t epochDuration = 604800,
               int64_t genesisTime = 0);

// ============================================================================
// Nullifier Proof
// ============================================================================

/**
 * A proof that a nullifier was derived correctly from an identity.
 * 
 * This is used in ZK proofs to show that:
 * 1. The nullifier corresponds to a registered identity
 * 2. The identity has not claimed in this epoch (nullifier is fresh)
 * 
 * Without revealing which identity is making the claim.
 */
struct NullifierProof {
    /// The nullifier being proven
    Nullifier nullifier;
    
    /// The epoch for this claim
    EpochId epoch;
    
    /// Merkle proof of identity membership (root of identity tree)
    std::vector<FieldElement> merkleProof;
    
    /// Path bits for Merkle proof
    std::vector<bool> pathBits;
    
    /// ZK proof data (opaque bytes for the ZK circuit)
    std::vector<Byte> zkProofData;
    
    /// Serialize to bytes
    std::vector<Byte> ToBytes() const;
    
    /// Deserialize from bytes
    static std::optional<NullifierProof> FromBytes(const Byte* data, size_t len);
    
    /// Check if proof is well-formed (not verified, just structural check)
    bool IsWellFormed() const;
};

} // namespace identity
} // namespace shurium

#endif // SHURIUM_IDENTITY_NULLIFIER_H
