// SHURIUM - Governance Module
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements the SHURIUM on-chain governance system for decentralized decision-making.
//
// Key features:
// - Multiple proposal types (Parameter, Protocol, Constitutional)
// - Weighted voting based on stake
// - Vote delegation
// - Timelocked execution
// - Protocol upgrade coordination
// - Emergency governance actions

#ifndef SHURIUM_GOVERNANCE_GOVERNANCE_H
#define SHURIUM_GOVERNANCE_GOVERNANCE_H

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
#include <variant>
#include <vector>

namespace shurium {
namespace governance {

// Forward declarations
class GovernanceEngine;
class VotingPowerTracker;
class DelegationRegistry;

// ============================================================================
// Governance Constants
// ============================================================================

/// Minimum stake required to create proposals (10,000 NXS)
constexpr Amount MIN_PROPOSAL_STAKE = 10000 * COIN;

/// Minimum stake required to vote (100 NXS)
constexpr Amount MIN_VOTING_STAKE = 100 * COIN;

/// Parameter change voting period (blocks) - ~3 days
constexpr int PARAMETER_VOTING_PERIOD = 8640;

/// Protocol upgrade voting period (blocks) - ~14 days
constexpr int PROTOCOL_VOTING_PERIOD = 40320;

/// Constitutional change voting period (blocks) - ~30 days
constexpr int CONSTITUTIONAL_VOTING_PERIOD = 86400;

/// Execution delay for parameter changes (blocks) - ~1 day
constexpr int PARAMETER_EXECUTION_DELAY = 2880;

/// Execution delay for protocol upgrades (blocks) - ~7 days
constexpr int PROTOCOL_EXECUTION_DELAY = 20160;

/// Execution delay for constitutional changes (blocks) - ~14 days
constexpr int CONSTITUTIONAL_EXECUTION_DELAY = 40320;

/// Minimum approval for parameter changes (%)
constexpr int PARAMETER_APPROVAL_THRESHOLD = 50;

/// Minimum approval for protocol upgrades (%)
constexpr int PROTOCOL_APPROVAL_THRESHOLD = 66;

/// Minimum approval for constitutional changes (%)
constexpr int CONSTITUTIONAL_APPROVAL_THRESHOLD = 75;

/// Quorum for parameter changes (%)
constexpr int PARAMETER_QUORUM = 10;

/// Quorum for protocol upgrades (%)
constexpr int PROTOCOL_QUORUM = 20;

/// Quorum for constitutional changes (%)
constexpr int CONSTITUTIONAL_QUORUM = 33;

/// Maximum active proposals per proposer
constexpr int MAX_ACTIVE_PROPOSALS_PER_USER = 3;

/// Vote change cooldown (blocks) - ~6 hours
constexpr int VOTE_CHANGE_COOLDOWN = 720;

/// Delegation update cooldown (blocks) - ~1 day
constexpr int DELEGATION_COOLDOWN = 2880;

/// Maximum delegation chain depth
constexpr int MAX_DELEGATION_DEPTH = 5;

/// Protocol version format: major.minor.patch
constexpr uint32_t PROTOCOL_VERSION_CURRENT = 0x00010000; // 1.0.0

// ============================================================================
// Governance Types
// ============================================================================

/// Unique governance proposal identifier
using GovernanceProposalId = Hash256;

/// Voter identifier (public key hash)
using VoterId = Hash160;

/// Proposal types with different requirements
enum class ProposalType {
    /// Parameter change (fee rates, limits, etc.)
    Parameter,
    
    /// Protocol upgrade (consensus rules, features)
    Protocol,
    
    /// Constitutional change (fundamental governance rules)
    Constitutional,
    
    /// Emergency action (requires supermajority + guardians)
    Emergency,
    
    /// Text proposal (non-binding signaling)
    Signal
};

/// Convert proposal type to string
const char* ProposalTypeToString(ProposalType type);

/// Parse proposal type from string
std::optional<ProposalType> ParseProposalType(const std::string& str);

/// Governance proposal status
enum class GovernanceStatus {
    /// Proposal created, deposit pending
    Draft,
    
    /// Deposit received, waiting for voting start
    Pending,
    
    /// Currently in voting period
    Active,
    
    /// Voting ended, approval reached, waiting execution
    Approved,
    
    /// Voting ended, approval not reached
    Rejected,
    
    /// Quorum not met
    QuorumFailed,
    
    /// Successfully executed
    Executed,
    
    /// Execution failed
    ExecutionFailed,
    
    /// Cancelled by proposer (before voting)
    Cancelled,
    
    /// Vetoed by guardians
    Vetoed,
    
    /// Expired (voting period ended without quorum)
    Expired
};

/// Convert status to string
const char* GovernanceStatusToString(GovernanceStatus status);

/// Vote choice
enum class VoteChoice {
    /// Support the proposal
    Yes,
    
    /// Oppose the proposal
    No,
    
    /// Abstain (counts toward quorum but not approval)
    Abstain,
    
    /// Strong opposition (can trigger additional review)
    NoWithVeto
};

/// Convert vote choice to string
const char* VoteChoiceToString(VoteChoice choice);

// ============================================================================
// Configurable Parameters
// ============================================================================

/// Parameters that can be changed through governance
enum class GovernableParameter {
    /// Transaction fee multiplier (basis points)
    TransactionFeeMultiplier,
    
    /// Block size limit (bytes)
    BlockSizeLimit,
    
    /// Minimum transaction fee (base units)
    MinTransactionFee,
    
    /// Block reward adjustment rate (basis points)
    BlockRewardAdjustment,
    
    /// UBI distribution rate (basis points of block reward)
    UBIDistributionRate,
    
    /// Oracle minimum stake
    OracleMinStake,
    
    /// Oracle slashing rate (basis points)
    OracleSlashingRate,
    
    /// Treasury allocation percentages
    TreasuryAllocationDev,
    TreasuryAllocationSecurity,
    TreasuryAllocationMarketing,
    
    /// Stability fee rate (basis points)
    StabilityFeeRate,
    
    /// Price deviation threshold (basis points)
    PriceDeviationThreshold,
    
    /// Proposal deposit amount
    ProposalDepositAmount,
    
    /// Voting period duration (blocks)
    VotingPeriodBlocks,
    
    /// Maximum parameter count (for iteration)
    MaxParameterCount
};

/// Convert parameter to string
const char* GovernableParameterToString(GovernableParameter param);

/// Parse parameter from string
std::optional<GovernableParameter> ParseGovernableParameter(const std::string& str);

/// Parameter value type (int64 or string)
using ParameterValue = std::variant<int64_t, std::string>;

/// Parameter change specification
struct ParameterChange {
    GovernableParameter parameter;
    ParameterValue currentValue;
    ParameterValue newValue;
    
    /// Validate the change is within allowed bounds
    bool IsValid() const;
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Protocol Upgrade
// ============================================================================

/// Protocol feature flags
enum class ProtocolFeature : uint32_t {
    /// Zero-knowledge identity proofs
    ZKIdentity = 1 << 0,
    
    /// Enhanced privacy transactions
    PrivacyTx = 1 << 1,
    
    /// Instant finality
    InstantFinality = 1 << 2,
    
    /// Cross-chain bridges
    CrossChain = 1 << 3,
    
    /// Smart contract support
    SmartContracts = 1 << 4,
    
    /// Sharding support
    Sharding = 1 << 5,
    
    /// Post-quantum cryptography
    PostQuantum = 1 << 6,
    
    /// Enhanced oracle system
    OracleV2 = 1 << 7,
    
    /// Governance V2
    GovernanceV2 = 1 << 8,
    
    /// Layer 2 support
    Layer2 = 1 << 9
};

/// Protocol upgrade specification
struct ProtocolUpgrade {
    /// New protocol version
    uint32_t newVersion{0};
    
    /// Minimum client version required
    uint32_t minClientVersion{0};
    
    /// Features being activated
    uint32_t activatedFeatures{0};
    
    /// Features being deprecated
    uint32_t deprecatedFeatures{0};
    
    /// Activation height (after execution delay)
    int activationHeight{0};
    
    /// Mandatory upgrade deadline height
    int deadlineHeight{0};
    
    /// Code reference (git commit, IPFS hash, etc.)
    std::string codeReference;
    
    /// Changelog URL
    std::string changelogUrl;
    
    /// Check if upgrade is backward compatible
    bool IsBackwardCompatible() const;
    
    /// Format version string
    static std::string FormatVersion(uint32_t version);
    
    /// Parse version string
    static std::optional<uint32_t> ParseVersion(const std::string& str);
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Constitutional Rules
// ============================================================================

/// Constitutional article categories
enum class ConstitutionalArticle {
    /// Governance process rules
    GovernanceProcess,
    
    /// Economic policy fundamentals
    EconomicPolicy,
    
    /// Privacy rights
    PrivacyRights,
    
    /// Security requirements
    SecurityRequirements,
    
    /// Upgrade procedures
    UpgradeProcedures,
    
    /// Emergency powers
    EmergencyPowers,
    
    /// Fundamental limits (max supply, etc.)
    FundamentalLimits
};

/// Convert article to string
const char* ConstitutionalArticleToString(ConstitutionalArticle article);

/// Constitutional change specification
struct ConstitutionalChange {
    ConstitutionalArticle article;
    std::string currentText;
    std::string newText;
    std::string rationale;
    
    /// Calculate hash of the change
    Hash256 GetHash() const;
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Vote Record
// ============================================================================

/**
 * Individual vote record.
 */
struct Vote {
    /// Proposal being voted on
    GovernanceProposalId proposalId;
    
    /// Voter identity
    VoterId voter;
    
    /// Vote choice
    VoteChoice choice{VoteChoice::Abstain};
    
    /// Voting power at time of vote
    uint64_t votingPower{0};
    
    /// Block height when vote cast
    int voteHeight{0};
    
    /// Optional reason/comment
    std::string reason;
    
    /// Signature proving vote authenticity
    std::vector<Byte> signature;
    
    /// Calculate vote hash
    Hash256 GetHash() const;
    
    /// Verify vote signature
    bool VerifySignature(const PublicKey& pubKey) const;
    
    /// Sign the vote
    bool Sign(const std::vector<Byte>& privateKey);
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Vote Delegation
// ============================================================================

/**
 * Vote delegation record.
 */
struct Delegation {
    /// Delegator (who is giving their vote)
    VoterId delegator;
    
    /// Delegate (who receives the voting power)
    VoterId delegate;
    
    /// Proposal type scope (nullopt = all types)
    std::optional<ProposalType> scope;
    
    /// Expiration height (0 = no expiration)
    int expirationHeight{0};
    
    /// Block height when delegation created
    int creationHeight{0};
    
    /// Whether delegation is currently active
    bool isActive{true};
    
    /// Calculate delegation hash
    Hash256 GetHash() const;
    
    /// Check if delegation is valid at given height
    bool IsValidAt(int height) const;
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Governance Proposal
// ============================================================================

/**
 * A governance proposal for on-chain decision making.
 */
struct GovernanceProposal {
    /// Unique proposal ID
    GovernanceProposalId id;
    
    /// Proposal type
    ProposalType type{ProposalType::Signal};
    
    /// Proposal title
    std::string title;
    
    /// Detailed description
    std::string description;
    
    /// Proposer's public key
    PublicKey proposer;
    
    /// Deposit amount (returned if not rejected)
    Amount deposit{0};
    
    /// Current status
    GovernanceStatus status{GovernanceStatus::Draft};
    
    /// Block height when submitted
    int submitHeight{0};
    
    /// Voting start height
    int votingStartHeight{0};
    
    /// Voting end height
    int votingEndHeight{0};
    
    /// Execution height (if approved)
    int executionHeight{0};
    
    /// Votes breakdown
    uint64_t votesYes{0};
    uint64_t votesNo{0};
    uint64_t votesAbstain{0};
    uint64_t votesNoWithVeto{0};
    
    /// Total voting power snapshot at voting start
    uint64_t totalVotingPower{0};
    
    /// External discussion URL
    std::string discussionUrl;
    
    /// Type-specific payload
    std::variant<
        std::vector<ParameterChange>,  // For Parameter proposals
        ProtocolUpgrade,               // For Protocol proposals
        ConstitutionalChange,          // For Constitutional proposals
        std::string                    // For Signal/Emergency (text)
    > payload;
    
    /// Calculate proposal hash
    Hash256 CalculateHash() const;
    
    /// Get voting period for this proposal type
    int GetVotingPeriod() const;
    
    /// Get execution delay for this proposal type
    int GetExecutionDelay() const;
    
    /// Get approval threshold for this proposal type
    int GetApprovalThreshold() const;
    
    /// Get quorum requirement for this proposal type
    int GetQuorumRequirement() const;
    
    /// Calculate approval percentage
    double GetApprovalPercent() const;
    
    /// Calculate participation percentage
    double GetParticipationPercent() const;
    
    /// Check if quorum is met
    bool HasQuorum() const;
    
    /// Check if approval threshold is met
    bool HasApproval() const;
    
    /// Check if veto threshold is reached (>33% NoWithVeto)
    bool IsVetoed() const;
    
    /// Check if voting is currently active
    bool IsVotingActive(int currentHeight) const;
    
    /// Check if ready for execution
    bool IsReadyForExecution(int currentHeight) const;
    
    /// Get total votes cast
    uint64_t GetTotalVotes() const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<GovernanceProposal> Deserialize(const Byte* data, size_t len);
    
    /// Get human-readable description
    std::string ToString() const;
};

// ============================================================================
// Voting Power Tracker
// ============================================================================

/**
 * Tracks voting power for all participants.
 */
class VotingPowerTracker {
public:
    VotingPowerTracker();
    ~VotingPowerTracker();
    
    /// Update voting power for a voter
    void UpdateVotingPower(const VoterId& voter, uint64_t power);
    
    /// Get voting power for a voter
    uint64_t GetVotingPower(const VoterId& voter) const;
    
    /// Get total voting power in the system
    uint64_t GetTotalVotingPower() const;
    
    /// Take snapshot of voting power at current state
    std::map<VoterId, uint64_t> TakeSnapshot() const;
    
    /// Get number of voters with non-zero power
    size_t GetVoterCount() const;
    
    /// Remove voter (e.g., stake withdrawn)
    void RemoveVoter(const VoterId& voter);
    
    /// Clear all voting power
    void Clear();
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    bool Deserialize(const Byte* data, size_t len);
    
private:
    mutable std::mutex mutex_;
    std::map<VoterId, uint64_t> votingPower_;
    uint64_t totalPower_{0};
};

// ============================================================================
// Delegation Registry
// ============================================================================

/**
 * Manages vote delegations.
 */
class DelegationRegistry {
public:
    DelegationRegistry();
    ~DelegationRegistry();
    
    /// Add a delegation
    bool AddDelegation(const Delegation& delegation);
    
    /// Remove a delegation
    bool RemoveDelegation(const VoterId& delegator);
    
    /// Get delegation for a voter
    std::optional<Delegation> GetDelegation(const VoterId& delegator) const;
    
    /// Get all delegators to a delegate
    std::vector<VoterId> GetDelegators(const VoterId& delegate) const;
    
    /// Get effective voting power including delegations
    uint64_t GetEffectiveVotingPower(
        const VoterId& voter,
        const VotingPowerTracker& tracker,
        ProposalType proposalType,
        int currentHeight) const;
    
    /// Check for delegation cycles
    bool HasCycle(const VoterId& delegator, const VoterId& delegate) const;
    
    /// Get delegation chain depth
    int GetDelegationDepth(const VoterId& voter) const;
    
    /// Expire old delegations at given height
    void ExpireDelegations(int height);
    
    /// Get number of active delegations
    size_t GetActiveDelegationCount() const;
    
    /// Clear all delegations
    void Clear();
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    bool Deserialize(const Byte* data, size_t len);
    
private:
    mutable std::mutex mutex_;
    std::map<VoterId, Delegation> delegations_;  // delegator -> delegation
    std::map<VoterId, std::set<VoterId>> reverseLookup_;  // delegate -> delegators
};

// ============================================================================
// Parameter Registry
// ============================================================================

/**
 * Stores current governable parameter values.
 */
class ParameterRegistry {
public:
    ParameterRegistry();
    ~ParameterRegistry();
    
    /// Initialize with default values
    void InitializeDefaults();
    
    /// Get parameter value
    ParameterValue GetParameter(GovernableParameter param) const;
    
    /// Get parameter as int64
    int64_t GetParameterInt(GovernableParameter param) const;
    
    /// Get parameter as string
    std::string GetParameterString(GovernableParameter param) const;
    
    /// Set parameter value
    bool SetParameter(GovernableParameter param, const ParameterValue& value);
    
    /// Apply parameter changes from a proposal
    bool ApplyChanges(const std::vector<ParameterChange>& changes);
    
    /// Validate a proposed change
    bool ValidateChange(const ParameterChange& change) const;
    
    /// Get all parameters
    std::map<GovernableParameter, ParameterValue> GetAllParameters() const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    bool Deserialize(const Byte* data, size_t len);
    
private:
    mutable std::mutex mutex_;
    std::map<GovernableParameter, ParameterValue> parameters_;
};

// ============================================================================
// Guardian System
// ============================================================================

/// Guardian role for emergency governance
struct Guardian {
    /// Guardian identifier
    VoterId id;
    
    /// Guardian's public key
    PublicKey publicKey;
    
    /// Appointment height
    int appointmentHeight{0};
    
    /// Whether guardian is active
    bool isActive{true};
    
    /// Veto count used
    int vetosUsed{0};
    
    /// Maximum vetos allowed per period
    static constexpr int MAX_VETOS_PER_PERIOD = 3;
};

/**
 * Manages the guardian system for emergency actions.
 */
class GuardianRegistry {
public:
    GuardianRegistry();
    ~GuardianRegistry();
    
    /// Add a guardian (through constitutional process)
    bool AddGuardian(const Guardian& guardian);
    
    /// Remove a guardian
    bool RemoveGuardian(const VoterId& id);
    
    /// Get guardian info
    std::optional<Guardian> GetGuardian(const VoterId& id) const;
    
    /// Check if an action is vetoed by guardians
    bool IsVetoed(const GovernanceProposalId& proposalId) const;
    
    /// Record a guardian veto
    bool RecordVeto(const VoterId& guardianId, const GovernanceProposalId& proposalId);
    
    /// Get number of vetoes for a proposal
    int GetVetoCount(const GovernanceProposalId& proposalId) const;
    
    /// Get required veto count to block
    int GetRequiredVetoCount() const;
    
    /// Get active guardian count
    size_t GetActiveGuardianCount() const;
    
    /// Reset veto counts (at period boundary)
    void ResetVetoCounts();
    
private:
    mutable std::mutex mutex_;
    std::map<VoterId, Guardian> guardians_;
    std::map<GovernanceProposalId, std::set<VoterId>> proposalVetoes_;
};

// ============================================================================
// Governance Engine
// ============================================================================

/**
 * Main governance engine managing the entire governance lifecycle.
 */
class GovernanceEngine {
public:
    /// Callback for parameter changes
    using ParameterChangeCallback = std::function<void(GovernableParameter, const ParameterValue&)>;
    
    /// Callback for protocol upgrades
    using ProtocolUpgradeCallback = std::function<void(const ProtocolUpgrade&)>;
    
    GovernanceEngine();
    explicit GovernanceEngine(std::shared_ptr<ParameterRegistry> params);
    ~GovernanceEngine();
    
    // === Proposal Management ===
    
    /// Submit a new proposal
    std::optional<GovernanceProposalId> SubmitProposal(
        const GovernanceProposal& proposal,
        const std::vector<Byte>& signature);
    
    /// Get proposal by ID
    std::optional<GovernanceProposal> GetProposal(const GovernanceProposalId& id) const;
    
    /// Get all proposals with given status
    std::vector<GovernanceProposal> GetProposalsByStatus(GovernanceStatus status) const;
    
    /// Get all proposals by a proposer
    std::vector<GovernanceProposal> GetProposalsByProposer(const PublicKey& proposer) const;
    
    /// Get active proposal count
    size_t GetActiveProposalCount() const;
    
    /// Get total proposal count
    size_t GetTotalProposalCount() const;
    
    /// Cancel a proposal (by proposer, before voting)
    bool CancelProposal(const GovernanceProposalId& id, const std::vector<Byte>& signature);
    
    // === Voting ===
    
    /// Cast a vote on a proposal
    bool CastVote(const Vote& vote);
    
    /// Change a vote (within cooldown rules)
    bool ChangeVote(const Vote& newVote);
    
    /// Get vote for a voter on a proposal
    std::optional<Vote> GetVote(const GovernanceProposalId& proposalId, const VoterId& voter) const;
    
    /// Get all votes for a proposal
    std::vector<Vote> GetVotes(const GovernanceProposalId& proposalId) const;
    
    /// Check if voter has voted on proposal
    bool HasVoted(const GovernanceProposalId& proposalId, const VoterId& voter) const;
    
    // === Delegation ===
    
    /// Delegate voting power
    bool Delegate(const Delegation& delegation, const std::vector<Byte>& signature);
    
    /// Revoke delegation
    bool RevokeDelegation(const VoterId& delegator, const std::vector<Byte>& signature);
    
    /// Get delegation registry
    const DelegationRegistry& GetDelegations() const { return delegations_; }
    
    // === Voting Power ===
    
    /// Update voting power for a participant
    void UpdateVotingPower(const VoterId& voter, uint64_t power);
    
    /// Get voting power for a participant
    uint64_t GetVotingPower(const VoterId& voter) const;
    
    /// Get effective voting power (including delegations)
    uint64_t GetEffectiveVotingPower(const VoterId& voter, ProposalType type) const;
    
    /// Get voting power tracker
    const VotingPowerTracker& GetVotingPowerTracker() const { return votingPower_; }
    
    // === Parameters ===
    
    /// Get parameter registry
    const ParameterRegistry& GetParameters() const { return *params_; }
    
    /// Get a specific parameter value
    ParameterValue GetParameter(GovernableParameter param) const;
    
    // === Guardians ===
    
    /// Veto a proposal (guardian action)
    bool VetoProposal(const GovernanceProposalId& proposalId, 
                      const VoterId& guardianId,
                      const std::vector<Byte>& signature);
    
    /// Get guardian registry
    const GuardianRegistry& GetGuardians() const { return guardians_; }
    
    // === Lifecycle ===
    
    /// Process block - update proposal states, execute ready proposals
    void ProcessBlock(int height);
    
    /// Get current block height
    int GetCurrentHeight() const { return currentHeight_; }
    
    // === Callbacks ===
    
    /// Set callback for parameter changes
    void SetParameterChangeCallback(ParameterChangeCallback callback);
    
    /// Set callback for protocol upgrades
    void SetProtocolUpgradeCallback(ProtocolUpgradeCallback callback);
    
    // === Serialization ===
    
    /// Serialize engine state
    std::vector<Byte> Serialize() const;
    
    /// Deserialize engine state
    bool Deserialize(const Byte* data, size_t len);
    
private:
    /// Start voting period for a proposal
    void StartVoting(GovernanceProposalId& id, int height);
    
    /// End voting period for a proposal
    void EndVoting(GovernanceProposalId& id, int height);
    
    /// Execute an approved proposal
    bool ExecuteProposal(GovernanceProposal& proposal);
    
    /// Execute parameter changes
    bool ExecuteParameterChanges(const std::vector<ParameterChange>& changes);
    
    /// Execute protocol upgrade
    bool ExecuteProtocolUpgrade(const ProtocolUpgrade& upgrade);
    
    /// Execute constitutional change
    bool ExecuteConstitutionalChange(const ConstitutionalChange& change);
    
    /// Verify proposal signature
    bool VerifyProposalSignature(const GovernanceProposal& proposal,
                                  const std::vector<Byte>& signature) const;
    
    /// Validate proposal content
    bool ValidateProposal(const GovernanceProposal& proposal) const;
    
    /// Calculate voting power snapshot for a proposal
    void SnapshotVotingPower(GovernanceProposal& proposal);
    
    mutable std::mutex mutex_;
    
    // Storage
    std::map<GovernanceProposalId, GovernanceProposal> proposals_;
    std::map<GovernanceProposalId, std::map<VoterId, Vote>> votes_;
    
    // Subsystems
    VotingPowerTracker votingPower_;
    DelegationRegistry delegations_;
    std::shared_ptr<ParameterRegistry> params_;
    GuardianRegistry guardians_;
    
    // State
    int currentHeight_{0};
    
    // Callbacks
    ParameterChangeCallback paramChangeCallback_;
    ProtocolUpgradeCallback upgradeCallback_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Calculate voting power from stake amount
uint64_t CalculateVotingPower(Amount stake);

/// Validate a parameter change is within bounds
bool ValidateParameterBounds(GovernableParameter param, const ParameterValue& value);

/// Get default value for a parameter
ParameterValue GetParameterDefault(GovernableParameter param);

/// Get minimum value for a parameter (if numeric)
std::optional<int64_t> GetParameterMin(GovernableParameter param);

/// Get maximum value for a parameter (if numeric)
std::optional<int64_t> GetParameterMax(GovernableParameter param);

/// Format amount for display
std::string FormatGovernanceAmount(Amount amount);

} // namespace governance
} // namespace shurium

#endif // SHURIUM_GOVERNANCE_GOVERNANCE_H
