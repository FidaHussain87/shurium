// SHURIUM - Universal Basic Income (UBI) System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements the SHURIUM Universal Basic Income distribution system.
// 
// Key features:
// - 30% of each block reward goes to the UBI pool
// - Distributed equally among all verified unique humans
// - Claims are anonymous using zero-knowledge proofs
// - One claim per identity per epoch (daily)
// - Nullifiers prevent double-claiming

#ifndef SHURIUM_ECONOMICS_UBI_H
#define SHURIUM_ECONOMICS_UBI_H

#include <shurium/core/types.h>
#include <shurium/economics/reward.h>
#include <shurium/identity/commitment.h>
#include <shurium/identity/identity.h>
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
namespace economics {

// Forward declarations
class UBIPool;
class UBIClaim;

// ============================================================================
// UBI Constants
// ============================================================================

/// Minimum number of verified identities for UBI distribution
constexpr uint32_t MIN_IDENTITIES_FOR_UBI = 100;

/// Maximum UBI amount per person per epoch (safety cap)
constexpr Amount MAX_UBI_PER_PERSON = 10000 * COIN;

/// Claim window in blocks after epoch end
constexpr int UBI_CLAIM_WINDOW = 2880; // ~24 hours

/// Grace period for late claims (additional epochs)
constexpr int UBI_GRACE_EPOCHS = 7; // ~1 week

// ============================================================================
// UBI Claim Status
// ============================================================================

/// Status of a UBI claim
enum class ClaimStatus {
    /// Claim is pending verification
    Pending,
    
    /// Claim is valid and processed
    Valid,
    
    /// Claim was rejected (invalid proof)
    InvalidProof,
    
    /// Claim was rejected (nullifier already used)
    DoubleClaim,
    
    /// Claim was rejected (identity not in tree)
    IdentityNotFound,
    
    /// Claim was rejected (epoch not claimable)
    EpochExpired,
    
    /// Claim was rejected (epoch not yet complete)
    EpochNotComplete,
    
    /// Claim was rejected (pool empty)
    PoolEmpty
};

/// Convert claim status to string
const char* ClaimStatusToString(ClaimStatus status);

// ============================================================================
// UBI Claim
// ============================================================================

/**
 * A claim for UBI distribution.
 * 
 * Claims are submitted anonymously using zero-knowledge proofs.
 * The claim proves:
 * 1. The claimant has a valid identity in the identity tree
 * 2. The claimant knows the secrets for that identity
 * 3. The nullifier is correctly derived for this epoch
 */
struct UBIClaim {
    /// Epoch being claimed
    EpochId epoch{0};
    
    /// Nullifier (prevents double-claiming)
    identity::Nullifier nullifier;
    
    /// Zero-knowledge proof of valid identity
    identity::ZKProof proof;
    
    /// Recipient address (where to send UBI)
    Hash160 recipient;
    
    /// Block height when claim was submitted
    int submitHeight{0};
    
    /// Claim status
    ClaimStatus status{ClaimStatus::Pending};
    
    /// Amount received (set after processing)
    Amount amount{0};
    
    /// Create a new claim
    static UBIClaim Create(
        EpochId epoch,
        const identity::IdentitySecrets& secrets,
        const Hash160& recipient,
        const identity::VectorCommitment::MerkleProof& membershipProof
    );
    
    /// Serialize for transmission
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<UBIClaim> Deserialize(const Byte* data, size_t len);
    
    /// Get hash of this claim
    Hash256 GetHash() const;
    
    /// Get string representation
    std::string ToString() const;
};

// ============================================================================
// Epoch UBI Pool
// ============================================================================

/**
 * UBI pool for a single epoch.
 * 
 * Tracks the accumulated UBI funds and claim status for one epoch.
 */
struct EpochUBIPool {
    /// Epoch identifier
    EpochId epoch{0};
    
    /// Total UBI pool for this epoch
    Amount totalPool{0};
    
    /// Number of eligible identities at epoch end
    uint32_t eligibleCount{0};
    
    /// Amount per person (calculated at epoch end)
    Amount amountPerPerson{0};
    
    /// Amount claimed so far
    Amount amountClaimed{0};
    
    /// Number of successful claims
    uint32_t claimCount{0};
    
    /// Set of used nullifiers (to prevent double-claiming)
    std::set<identity::Nullifier> usedNullifiers;
    
    /// Whether pool is finalized (epoch complete)
    bool isFinalized{false};
    
    /// Block height when epoch ended
    int endHeight{0};
    
    /// Claim deadline (block height)
    int claimDeadline{0};
    
    /// Calculate per-person amount
    void Finalize(uint32_t identityCount);
    
    /// Check if nullifier is already used
    bool IsNullifierUsed(const identity::Nullifier& nullifier) const;
    
    /// Record a claim
    void RecordClaim(const identity::Nullifier& nullifier, Amount amount);
    
    /// Get unclaimed amount
    Amount UnclaimedAmount() const;
    
    /// Get claim rate (percentage)
    double ClaimRate() const;
    
    /// Check if claims are still accepted
    bool AcceptingClaims(int currentHeight) const;
    
    /// Get string representation
    std::string ToString() const;
};

// ============================================================================
// UBI Distributor
// ============================================================================

/**
 * Main class for UBI distribution.
 * 
 * Manages epoch pools, processes claims, and tracks distribution statistics.
 */
class UBIDistributor {
public:
    /// Create distributor with reward calculator
    explicit UBIDistributor(const RewardCalculator& calculator);
    
    ~UBIDistributor();
    
    // ========================================================================
    // Pool Management
    // ========================================================================
    
    /// Add UBI funds from a block reward
    /// @param height Block height
    /// @param amount UBI pool amount from this block
    void AddBlockReward(int height, Amount amount);
    
    /// Finalize an epoch's pool
    /// @param epoch Epoch to finalize
    /// @param identityCount Number of eligible identities
    void FinalizeEpoch(EpochId epoch, uint32_t identityCount);
    
    /// Get pool for an epoch
    const EpochUBIPool* GetPool(EpochId epoch) const;
    
    /// Get current epoch
    EpochId GetCurrentEpoch() const { return currentEpoch_; }
    
    /// Get per-person amount for an epoch
    Amount GetAmountPerPerson(EpochId epoch) const;
    
    // ========================================================================
    // Claim Processing
    // ========================================================================
    
    /**
     * Process a UBI claim.
     * 
     * @param claim The claim to process
     * @param identityTreeRoot Current identity tree root
     * @param currentHeight Current block height
     * @return Claim status after processing
     */
    ClaimStatus ProcessClaim(
        UBIClaim& claim,
        const Hash256& identityTreeRoot,
        int currentHeight
    );
    
    /**
     * Verify a claim without processing.
     * 
     * @param claim The claim to verify
     * @param identityTreeRoot Current identity tree root
     * @param currentHeight Current block height
     * @return Whether the claim is valid
     */
    bool VerifyClaim(
        const UBIClaim& claim,
        const Hash256& identityTreeRoot,
        int currentHeight
    ) const;
    
    /// Check if epoch is claimable
    bool IsEpochClaimable(EpochId epoch, int currentHeight) const;
    
    /// Get claim deadline for an epoch
    int GetClaimDeadline(EpochId epoch) const;
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get total UBI distributed all-time
    Amount GetTotalDistributed() const { return totalDistributed_; }
    
    /// Get total claims processed all-time
    uint64_t GetTotalClaims() const { return totalClaims_; }
    
    /// Get average claim rate across epochs
    double GetAverageClaimRate() const;
    
    /// Get distribution statistics for an epoch
    struct EpochStats {
        EpochId epoch;
        Amount poolSize;
        Amount distributed;
        Amount unclaimed;
        uint32_t eligibleCount;
        uint32_t claimCount;
        double claimRate;
    };
    EpochStats GetEpochStats(EpochId epoch) const;
    
    // ========================================================================
    // Serialization
    // ========================================================================
    
    /// Serialize state for persistence
    std::vector<Byte> Serialize() const;
    
    /// Deserialize state
    bool Deserialize(const Byte* data, size_t len);

private:
    const RewardCalculator& calculator_;
    
    /// Current epoch
    EpochId currentEpoch_{0};
    
    /// Pool for each epoch
    std::map<EpochId, EpochUBIPool> pools_;
    
    /// Total distributed all-time
    Amount totalDistributed_{0};
    
    /// Total claims processed
    uint64_t totalClaims_{0};
    
    /// Mutex for thread safety
    mutable std::mutex mutex_;
    
    /// Get or create pool for epoch
    EpochUBIPool& GetOrCreatePool(EpochId epoch);
    
    /// Clean up old pools
    void PruneOldPools(EpochId currentEpoch);
};

// ============================================================================
// UBI Transaction Builder
// ============================================================================

/**
 * Builds transactions for UBI claims.
 */
class UBITransactionBuilder {
public:
    /// Build a claim transaction
    /// @param claim The claim
    /// @param amount Amount being claimed
    /// @return Transaction outputs
    std::vector<std::pair<std::vector<Byte>, Amount>> BuildClaimOutputs(
        const UBIClaim& claim,
        Amount amount
    ) const;
    
    /// Verify claim transaction outputs
    bool VerifyClaimOutputs(
        const UBIClaim& claim,
        const std::vector<std::pair<std::vector<Byte>, Amount>>& outputs
    ) const;
};

// ============================================================================
// UBI Claim Generator
// ============================================================================

/**
 * Helper class for generating UBI claims.
 * 
 * Used by wallets to create valid claims with ZK proofs.
 */
class UBIClaimGenerator {
public:
    /**
     * Generate a claim for an epoch.
     * 
     * @param epoch Epoch to claim
     * @param secrets User's identity secrets
     * @param recipient Address to receive UBI
     * @param membershipProof Proof of identity membership
     * @return The generated claim
     */
    static UBIClaim GenerateClaim(
        EpochId epoch,
        const identity::IdentitySecrets& secrets,
        const Hash160& recipient,
        const identity::VectorCommitment::MerkleProof& membershipProof
    );
    
    /**
     * Check if user can claim for an epoch.
     * 
     * @param epoch Epoch to check
     * @param secrets User's identity secrets
     * @param distributor UBI distributor instance
     * @return Whether claim is possible
     */
    static bool CanClaim(
        EpochId epoch,
        const identity::IdentitySecrets& secrets,
        const UBIDistributor& distributor
    );
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Calculate expected UBI per person for current parameters
Amount CalculateExpectedUBI(uint32_t identityCount, const RewardCalculator& calculator);

/// Estimate annual UBI income per person
Amount EstimateAnnualUBI(uint32_t identityCount, const RewardCalculator& calculator);

} // namespace economics
} // namespace shurium

#endif // SHURIUM_ECONOMICS_UBI_H
