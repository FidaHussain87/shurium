// SHURIUM - Staking Module
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements the SHURIUM Proof-of-Stake system with delegated staking.
//
// Key features:
// - Validator registration and management
// - Delegated staking pools
// - Slashing for misbehavior
// - Reward distribution
// - Unbonding periods
// - Validator rotation

#ifndef SHURIUM_STAKING_STAKING_H
#define SHURIUM_STAKING_STAKING_H

#include <shurium/core/types.h>
#include <shurium/crypto/keys.h>

#include <chrono>
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
namespace staking {

// Forward declarations
class ValidatorSet;
class StakingPool;
class SlashingManager;
class RewardDistributor;

// ============================================================================
// Staking Constants
// ============================================================================

/// Minimum stake to become a validator (100,000 NXS)
constexpr Amount MIN_VALIDATOR_STAKE = 100000 * COIN;

/// Minimum stake for delegation (100 NXS)
constexpr Amount MIN_DELEGATION_STAKE = 100 * COIN;

/// Maximum validators in active set
constexpr int MAX_ACTIVE_VALIDATORS = 100;

/// Unbonding period (blocks) - ~21 days
constexpr int UNBONDING_PERIOD = 60480;

/// Reward claim cooldown (blocks) - ~1 day
constexpr int REWARD_CLAIM_COOLDOWN = 2880;

/// Validator commission rate bounds (basis points)
constexpr int MIN_COMMISSION_RATE = 0;      // 0%
constexpr int MAX_COMMISSION_RATE = 5000;   // 50%
constexpr int DEFAULT_COMMISSION_RATE = 500; // 5%

/// Commission rate change cooldown (blocks) - ~7 days
constexpr int COMMISSION_CHANGE_COOLDOWN = 20160;

/// Maximum commission rate change per update (basis points)
constexpr int MAX_COMMISSION_CHANGE = 500;  // 5%

/// Slashing penalties (basis points)
constexpr int DOUBLE_SIGN_SLASH_RATE = 500;    // 5%
constexpr int DOWNTIME_SLASH_RATE = 10;        // 0.1%
constexpr int INVALID_BLOCK_SLASH_RATE = 100;  // 1%

/// Jail duration (blocks) - ~3 days
constexpr int JAIL_DURATION = 8640;

/// Missed blocks threshold for downtime slashing
constexpr int MISSED_BLOCKS_WINDOW = 10000;
constexpr int MAX_MISSED_BLOCKS = 500;  // 5% missed = slashing

/// Epoch length for reward distribution (blocks)
constexpr int EPOCH_LENGTH = 2880;  // ~1 day

/// Annual staking reward rate (basis points) - ~5%
constexpr int ANNUAL_REWARD_RATE = 500;

// ============================================================================
// Staking Types
// ============================================================================

/// Unique validator identifier
using ValidatorId = Hash160;

/// Unique delegation identifier
using DelegationId = Hash256;

/// Validator status
enum class ValidatorStatus {
    /// Registered but not yet active
    Pending,
    
    /// In the active validator set
    Active,
    
    /// Temporarily removed from active set (by choice)
    Inactive,
    
    /// Jailed due to misbehavior
    Jailed,
    
    /// Permanently removed (tombstoned)
    Tombstoned,
    
    /// Unbonding (exiting)
    Unbonding
};

/// Convert status to string
const char* ValidatorStatusToString(ValidatorStatus status);

/// Slashing reason
enum class SlashReason {
    /// Signed conflicting blocks
    DoubleSign,
    
    /// Extended downtime
    Downtime,
    
    /// Produced invalid block
    InvalidBlock,
    
    /// Other protocol violation
    ProtocolViolation
};

/// Convert reason to string
const char* SlashReasonToString(SlashReason reason);

/// Delegation status
enum class DelegationStatus {
    /// Active delegation
    Active,
    
    /// Unbonding (withdrawing)
    Unbonding,
    
    /// Completed unbonding
    Completed
};

/// Convert status to string
const char* DelegationStatusToString(DelegationStatus status);

// ============================================================================
// Validator
// ============================================================================

/**
 * A validator in the staking system.
 */
struct Validator {
    /// Unique identifier (derived from operator key)
    ValidatorId id;
    
    /// Operator public key (for signing blocks)
    PublicKey operatorKey;
    
    /// Reward withdrawal address
    Hash160 rewardAddress;
    
    /// Human-readable name
    std::string moniker;
    
    /// Description/website
    std::string description;
    
    /// Current status
    ValidatorStatus status{ValidatorStatus::Pending};
    
    /// Self-bonded stake
    Amount selfStake{0};
    
    /// Total delegated stake (excluding self)
    Amount delegatedStake{0};
    
    /// Commission rate (basis points)
    int commissionRate{DEFAULT_COMMISSION_RATE};
    
    /// Commission rate last changed height
    int commissionChangeHeight{0};
    
    /// Block height when registered
    int registrationHeight{0};
    
    /// Block height when jailed (0 if not jailed)
    int jailedHeight{0};
    
    /// Block height when unbonding started (0 if not unbonding)
    int unbondingHeight{0};
    
    /// Accumulated rewards (not yet distributed)
    Amount accumulatedRewards{0};
    
    /// Total rewards earned (historical)
    Amount totalRewardsEarned{0};
    
    /// Number of blocks produced
    uint64_t blocksProduced{0};
    
    /// Number of blocks missed in current window
    int missedBlocksCounter{0};
    
    /// Bitmap of missed blocks (sliding window)
    std::vector<bool> missedBlocksBitmap;
    
    /// Number of times slashed
    int slashCount{0};
    
    /// Total amount slashed
    Amount totalSlashed{0};
    
    /// Get total stake (self + delegated)
    Amount GetTotalStake() const { return selfStake + delegatedStake; }
    
    /// Get voting power (based on stake)
    uint64_t GetVotingPower() const;
    
    /// Check if validator can be activated
    bool CanActivate() const;
    
    /// Check if validator can produce blocks
    bool CanProduceBlocks() const;
    
    /// Check if jail period is over
    bool IsJailExpired(int currentHeight) const;
    
    /// Check if unbonding period is over
    bool IsUnbondingComplete(int currentHeight) const;
    
    /// Calculate commission for given reward
    Amount CalculateCommission(Amount reward) const;
    
    /// Update missed blocks tracking
    void RecordBlockProduced();
    void RecordBlockMissed();
    
    /// Get missed blocks percentage
    double GetMissedBlocksPercent() const;
    
    /// Calculate hash for signing
    Hash256 GetHash() const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<Validator> Deserialize(const Byte* data, size_t len);
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Delegation
// ============================================================================

/**
 * A stake delegation to a validator.
 */
struct Delegation {
    /// Unique delegation ID
    DelegationId id;
    
    /// Delegator address
    Hash160 delegator;
    
    /// Target validator
    ValidatorId validatorId;
    
    /// Delegated amount
    Amount amount{0};
    
    /// Current status
    DelegationStatus status{DelegationStatus::Active};
    
    /// Block height when created
    int creationHeight{0};
    
    /// Block height when unbonding started
    int unbondingHeight{0};
    
    /// Accumulated rewards (pending claim)
    Amount pendingRewards{0};
    
    /// Total rewards claimed
    Amount totalRewardsClaimed{0};
    
    /// Last reward claim height
    int lastClaimHeight{0};
    
    /// Shares in the validator's pool (for reward calculation)
    uint64_t shares{0};
    
    /// Check if unbonding is complete
    bool IsUnbondingComplete(int currentHeight) const;
    
    /// Check if can claim rewards
    bool CanClaimRewards(int currentHeight) const;
    
    /// Calculate delegation hash
    Hash256 GetHash() const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<Delegation> Deserialize(const Byte* data, size_t len);
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Unbonding Entry
// ============================================================================

/**
 * An entry in the unbonding queue.
 */
struct UnbondingEntry {
    /// Type of unbonding
    enum class Type {
        ValidatorSelfUnbond,
        DelegationUnbond,
        Redelegation
    };
    
    Type type;
    
    /// Source (validator or delegator)
    Hash160 source;
    
    /// Amount being unbonded
    Amount amount{0};
    
    /// Block height when unbonding started
    int startHeight{0};
    
    /// Block height when unbonding completes
    int completionHeight{0};
    
    /// Target validator (for redelegation)
    std::optional<ValidatorId> targetValidator;
    
    /// Check if complete
    bool IsComplete(int currentHeight) const {
        return currentHeight >= completionHeight;
    }
};

// ============================================================================
// Slashing Event
// ============================================================================

/**
 * Record of a slashing event.
 */
struct SlashEvent {
    /// Validator that was slashed
    ValidatorId validatorId;
    
    /// Reason for slashing
    SlashReason reason;
    
    /// Block height when slashed
    int height{0};
    
    /// Amount slashed from validator
    Amount validatorSlashed{0};
    
    /// Amount slashed from delegators
    Amount delegatorsSlashed{0};
    
    /// Evidence hash (if applicable)
    Hash256 evidenceHash;
    
    /// Whether validator was jailed
    bool jailed{false};
    
    /// Whether validator was tombstoned
    bool tombstoned{false};
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Validator Set
// ============================================================================

/**
 * Manages the set of validators.
 */
class ValidatorSet {
public:
    ValidatorSet();
    ~ValidatorSet();
    
    // === Validator Management ===
    
    /// Register a new validator
    bool RegisterValidator(const Validator& validator, const std::vector<Byte>& signature);
    
    /// Get validator by ID
    std::optional<Validator> GetValidator(const ValidatorId& id) const;
    
    /// Update validator info (moniker, description, commission)
    bool UpdateValidator(const ValidatorId& id, 
                        const std::string& moniker,
                        const std::string& description,
                        int newCommissionRate,
                        const std::vector<Byte>& signature);
    
    /// Activate a pending validator
    bool ActivateValidator(const ValidatorId& id);
    
    /// Deactivate a validator (voluntary exit from active set)
    bool DeactivateValidator(const ValidatorId& id, const std::vector<Byte>& signature);
    
    /// Start validator unbonding
    bool StartUnbonding(const ValidatorId& id, const std::vector<Byte>& signature);
    
    /// Jail a validator
    bool JailValidator(const ValidatorId& id, SlashReason reason);
    
    /// Unjail a validator
    bool UnjailValidator(const ValidatorId& id, const std::vector<Byte>& signature);
    
    /// Tombstone a validator (permanent removal)
    void TombstoneValidator(const ValidatorId& id);
    
    // === Queries ===
    
    /// Get all validators with given status
    std::vector<Validator> GetValidatorsByStatus(ValidatorStatus status) const;
    
    /// Get active validator set (sorted by stake)
    std::vector<Validator> GetActiveSet() const;
    
    /// Get validator count by status
    size_t GetValidatorCount(ValidatorStatus status) const;
    
    /// Get total staked amount
    Amount GetTotalStaked() const;
    
    /// Check if validator exists
    bool ValidatorExists(const ValidatorId& id) const;
    
    /// Check if validator is in active set
    bool IsActive(const ValidatorId& id) const;
    
    // === Block Production ===
    
    /// Record block production
    void RecordBlockProduced(const ValidatorId& id);
    
    /// Record missed block
    void RecordBlockMissed(const ValidatorId& id);
    
    /// Get next block proposer (round-robin weighted by stake)
    ValidatorId GetNextProposer(int height) const;
    
    // === Epoch Processing ===
    
    /// Process end of epoch (update active set)
    void ProcessEpochEnd(int height);
    
    /// Process pending unbondings
    void ProcessUnbondings(int height);
    
    // === Serialization ===
    
    std::vector<Byte> Serialize() const;
    bool Deserialize(const Byte* data, size_t len);
    
private:
    /// Update active set based on stake ranking
    void UpdateActiveSet();
    
    /// Verify validator signature
    bool VerifyValidatorSignature(const ValidatorId& id, 
                                   const Hash256& hash,
                                   const std::vector<Byte>& signature) const;
    
    mutable std::mutex mutex_;
    std::map<ValidatorId, Validator> validators_;
    std::set<ValidatorId> activeSet_;
    std::vector<UnbondingEntry> unbondingQueue_;
    int currentHeight_{0};
};

// ============================================================================
// Staking Pool
// ============================================================================

/**
 * Manages delegations to validators.
 */
class StakingPool {
public:
    StakingPool();
    explicit StakingPool(std::shared_ptr<ValidatorSet> validators);
    ~StakingPool();
    
    // === Delegation ===
    
    /// Create a new delegation
    std::optional<DelegationId> Delegate(
        const Hash160& delegator,
        const ValidatorId& validatorId,
        Amount amount,
        const std::vector<Byte>& signature);
    
    /// Add to existing delegation
    bool AddToDelegation(
        const DelegationId& delegationId,
        Amount amount,
        const std::vector<Byte>& signature);
    
    /// Start unbonding a delegation
    bool Undelegate(
        const DelegationId& delegationId,
        Amount amount,
        const std::vector<Byte>& signature);
    
    /// Redelegate to a different validator
    bool Redelegate(
        const DelegationId& delegationId,
        const ValidatorId& newValidatorId,
        Amount amount,
        const std::vector<Byte>& signature);
    
    // === Queries ===
    
    /// Get delegation by ID
    std::optional<Delegation> GetDelegation(const DelegationId& id) const;
    
    /// Get all delegations for a delegator
    std::vector<Delegation> GetDelegationsByDelegator(const Hash160& delegator) const;
    
    /// Get all delegations to a validator
    std::vector<Delegation> GetDelegationsToValidator(const ValidatorId& validatorId) const;
    
    /// Get total delegated to a validator
    Amount GetTotalDelegated(const ValidatorId& validatorId) const;
    
    /// Get delegation count
    size_t GetDelegationCount() const;
    
    // === Rewards ===
    
    /// Claim pending rewards
    Amount ClaimRewards(const DelegationId& delegationId, const std::vector<Byte>& signature);
    
    /// Get pending rewards for a delegation
    Amount GetPendingRewards(const DelegationId& delegationId) const;
    
    /// Distribute rewards to a validator's delegators
    void DistributeRewards(const ValidatorId& validatorId, Amount totalReward);
    
    // === Slashing ===
    
    /// Apply slashing to a validator's delegations
    Amount ApplySlashing(const ValidatorId& validatorId, int slashRateBps);
    
    // === Processing ===
    
    /// Process block (update heights, process unbondings)
    void ProcessBlock(int height);
    
    // === Serialization ===
    
    std::vector<Byte> Serialize() const;
    bool Deserialize(const Byte* data, size_t len);
    
private:
    /// Calculate shares for a delegation amount
    uint64_t CalculateShares(const ValidatorId& validatorId, Amount amount) const;
    
    /// Calculate amount for given shares
    Amount CalculateAmount(const ValidatorId& validatorId, uint64_t shares) const;
    
    /// Verify delegator signature
    bool VerifyDelegatorSignature(const Hash160& delegator,
                                   const Hash256& hash,
                                   const std::vector<Byte>& signature) const;
    
    mutable std::mutex mutex_;
    std::shared_ptr<ValidatorSet> validators_;
    std::map<DelegationId, Delegation> delegations_;
    std::map<Hash160, std::set<DelegationId>> delegatorIndex_;
    std::map<ValidatorId, std::set<DelegationId>> validatorIndex_;
    std::map<ValidatorId, uint64_t> totalShares_;  // Total shares per validator
    std::vector<UnbondingEntry> unbondingQueue_;
    int currentHeight_{0};
};

// ============================================================================
// Slashing Manager
// ============================================================================

/**
 * Handles slashing for validator misbehavior.
 */
class SlashingManager {
public:
    SlashingManager();
    SlashingManager(std::shared_ptr<ValidatorSet> validators,
                    std::shared_ptr<StakingPool> pool);
    ~SlashingManager();
    
    // === Evidence Submission ===
    
    /// Submit double signing evidence
    bool SubmitDoubleSignEvidence(
        const ValidatorId& validatorId,
        const Hash256& block1Hash,
        const Hash256& block2Hash,
        int height,
        const std::vector<Byte>& signature1,
        const std::vector<Byte>& signature2);
    
    /// Report extended downtime
    bool ReportDowntime(const ValidatorId& validatorId);
    
    /// Report invalid block
    bool ReportInvalidBlock(
        const ValidatorId& validatorId,
        const Hash256& blockHash,
        const std::string& reason);
    
    // === Queries ===
    
    /// Get slashing events for a validator
    std::vector<SlashEvent> GetSlashEvents(const ValidatorId& validatorId) const;
    
    /// Get all slashing events in height range
    std::vector<SlashEvent> GetSlashEventsByHeight(int startHeight, int endHeight) const;
    
    /// Check if evidence was already submitted
    bool IsEvidenceSubmitted(const Hash256& evidenceHash) const;
    
    /// Get total slashed amount
    Amount GetTotalSlashed() const;
    
    // === Processing ===
    
    /// Process block (check for downtime)
    void ProcessBlock(int height, const ValidatorId& proposer);
    
    // === Serialization ===
    
    std::vector<Byte> Serialize() const;
    bool Deserialize(const Byte* data, size_t len);
    
private:
    /// Execute slashing
    SlashEvent ExecuteSlash(const ValidatorId& validatorId, 
                            SlashReason reason,
                            int slashRateBps,
                            const Hash256& evidenceHash);
    
    /// Get slash rate for reason
    int GetSlashRate(SlashReason reason) const;
    
    /// Determine if should jail
    bool ShouldJail(SlashReason reason) const;
    
    /// Determine if should tombstone
    bool ShouldTombstone(SlashReason reason, int slashCount) const;
    
    mutable std::mutex mutex_;
    std::shared_ptr<ValidatorSet> validators_;
    std::shared_ptr<StakingPool> pool_;
    std::vector<SlashEvent> slashEvents_;
    std::set<Hash256> submittedEvidence_;
    Amount totalSlashed_{0};
    int currentHeight_{0};
};

// ============================================================================
// Reward Distributor
// ============================================================================

/**
 * Calculates and distributes staking rewards.
 */
class RewardDistributor {
public:
    /// Callback when rewards are minted
    using RewardMintCallback = std::function<void(Amount)>;
    
    RewardDistributor();
    RewardDistributor(std::shared_ptr<ValidatorSet> validators,
                      std::shared_ptr<StakingPool> pool);
    ~RewardDistributor();
    
    // === Reward Calculation ===
    
    /// Calculate block reward for current state
    Amount CalculateBlockReward() const;
    
    /// Calculate annual reward for given stake
    Amount CalculateAnnualReward(Amount stake) const;
    
    /// Calculate validator's share of block reward
    Amount CalculateValidatorReward(const ValidatorId& validatorId, Amount blockReward) const;
    
    // === Distribution ===
    
    /// Distribute block reward to proposer and delegators
    void DistributeBlockReward(const ValidatorId& proposer, Amount blockReward);
    
    /// Process end of epoch (distribute accumulated rewards)
    void ProcessEpochEnd(int height);
    
    // === Queries ===
    
    /// Get total rewards distributed
    Amount GetTotalRewardsDistributed() const;
    
    /// Get rewards distributed in current epoch
    Amount GetEpochRewards() const;
    
    /// Get current epoch number
    int GetCurrentEpoch() const;
    
    /// Get APY estimate (basis points)
    int GetEstimatedAPY() const;
    
    // === Configuration ===
    
    /// Set reward mint callback
    void SetRewardMintCallback(RewardMintCallback callback);
    
    /// Set annual reward rate (basis points)
    void SetAnnualRewardRate(int rateBps);
    
    // === Serialization ===
    
    std::vector<Byte> Serialize() const;
    bool Deserialize(const Byte* data, size_t len);
    
private:
    mutable std::mutex mutex_;
    std::shared_ptr<ValidatorSet> validators_;
    std::shared_ptr<StakingPool> pool_;
    
    int annualRewardRate_{ANNUAL_REWARD_RATE};
    Amount totalRewardsDistributed_{0};
    Amount epochRewards_{0};
    int currentEpoch_{0};
    int epochStartHeight_{0};
    
    RewardMintCallback mintCallback_;
};

// ============================================================================
// Staking Engine
// ============================================================================

/**
 * Main staking engine coordinating all components.
 */
class StakingEngine {
public:
    StakingEngine();
    ~StakingEngine();
    
    // === Component Access ===
    
    ValidatorSet& GetValidatorSet() { return *validators_; }
    const ValidatorSet& GetValidatorSet() const { return *validators_; }
    
    StakingPool& GetStakingPool() { return *pool_; }
    const StakingPool& GetStakingPool() const { return *pool_; }
    
    SlashingManager& GetSlashingManager() { return *slashing_; }
    const SlashingManager& GetSlashingManager() const { return *slashing_; }
    
    RewardDistributor& GetRewardDistributor() { return *rewards_; }
    const RewardDistributor& GetRewardDistributor() const { return *rewards_; }
    
    // === Block Processing ===
    
    /// Process a new block
    void ProcessBlock(int height, const ValidatorId& proposer, Amount blockReward);
    
    /// Get current block height
    int GetCurrentHeight() const { return currentHeight_; }
    
    // === Convenience Methods ===
    
    /// Register validator (convenience wrapper)
    bool RegisterValidator(const Validator& validator, const std::vector<Byte>& signature);
    
    /// Delegate stake (convenience wrapper)
    std::optional<DelegationId> Delegate(
        const Hash160& delegator,
        const ValidatorId& validatorId,
        Amount amount,
        const std::vector<Byte>& signature);
    
    /// Get total staked
    Amount GetTotalStaked() const;
    
    /// Get network staking APY
    int GetNetworkAPY() const;
    
    // === Serialization ===
    
    std::vector<Byte> Serialize() const;
    bool Deserialize(const Byte* data, size_t len);
    
private:
    std::shared_ptr<ValidatorSet> validators_;
    std::shared_ptr<StakingPool> pool_;
    std::shared_ptr<SlashingManager> slashing_;
    std::shared_ptr<RewardDistributor> rewards_;
    
    int currentHeight_{0};
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Calculate voting power from stake
uint64_t CalculateVotingPower(Amount stake);

/// Calculate validator ID from operator key
ValidatorId CalculateValidatorId(const PublicKey& operatorKey);

/// Format stake amount for display
std::string FormatStakeAmount(Amount amount);

/// Calculate annual reward
Amount CalculateAnnualReward(Amount stake, int rateBps);

/// Calculate epoch reward
Amount CalculateEpochReward(Amount stake, int rateBps, int epochLength);

} // namespace staking
} // namespace shurium

#endif // SHURIUM_STAKING_STAKING_H
