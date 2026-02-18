// SHURIUM - Treasury Management System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements the SHURIUM treasury management system for governance-controlled funds.
//
// Key features:
// - Multi-signature controlled treasury
// - Proposal-based spending
// - Budget allocation by category
// - Time-locked releases
// - Transparent fund tracking

#ifndef SHURIUM_ECONOMICS_TREASURY_H
#define SHURIUM_ECONOMICS_TREASURY_H

#include <shurium/core/types.h>
#include <shurium/economics/reward.h>
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
namespace economics {

// Forward declarations
class Treasury;
class TreasuryProposal;

// ============================================================================
// Treasury Constants
// ============================================================================

/// Minimum proposal amount (1000 NXS)
constexpr Amount MIN_PROPOSAL_AMOUNT = 1000 * COIN;

/// Maximum single proposal amount (10% of treasury)
constexpr int MAX_PROPOSAL_PERCENT = 10;

/// Proposal voting period (blocks) - ~7 days
constexpr int PROPOSAL_VOTING_PERIOD = 20160;

/// Proposal execution delay after approval (blocks) - ~2 days
constexpr int PROPOSAL_EXECUTION_DELAY = 5760;

/// Minimum approval percentage required
constexpr int MIN_APPROVAL_PERCENT = 60;

/// Quorum requirement (percentage of eligible voters)
constexpr int QUORUM_PERCENT = 20;

/// Treasury report interval (blocks) - ~30 days
constexpr int TREASURY_REPORT_INTERVAL = 86400;

// ============================================================================
// Treasury Categories
// ============================================================================

/// Categories for treasury spending
enum class TreasuryCategory {
    /// Ecosystem development (grants, bounties)
    EcosystemDevelopment,
    
    /// Core protocol development
    ProtocolDevelopment,
    
    /// Security audits and bug bounties
    Security,
    
    /// Marketing and community growth
    Marketing,
    
    /// Infrastructure and operations
    Infrastructure,
    
    /// Legal and compliance
    Legal,
    
    /// Education and documentation
    Education,
    
    /// Emergency reserve
    Emergency,
    
    /// Other approved spending
    Other
};

/// Convert category to string
const char* TreasuryCategoryToString(TreasuryCategory category);

/// Parse category from string
std::optional<TreasuryCategory> ParseTreasuryCategory(const std::string& str);

/// Budget allocation percentages per category
namespace BudgetAllocation {
    constexpr int ECOSYSTEM_DEVELOPMENT = 30;
    constexpr int PROTOCOL_DEVELOPMENT = 25;
    constexpr int SECURITY = 15;
    constexpr int MARKETING = 10;
    constexpr int INFRASTRUCTURE = 10;
    constexpr int LEGAL = 5;
    constexpr int EDUCATION = 3;
    constexpr int EMERGENCY = 2;
}

// ============================================================================
// Proposal Types
// ============================================================================

/// Unique proposal identifier
using ProposalId = Hash256;

/// Proposal status
enum class ProposalStatus {
    /// Proposal submitted, awaiting voting
    Pending,
    
    /// Currently in voting period
    Voting,
    
    /// Approved, awaiting execution
    Approved,
    
    /// Rejected by voters
    Rejected,
    
    /// Executed successfully
    Executed,
    
    /// Cancelled by proposer
    Cancelled,
    
    /// Expired without sufficient votes
    Expired,
    
    /// Failed during execution
    Failed
};

/// Convert status to string
const char* ProposalStatusToString(ProposalStatus status);

// ============================================================================
// Treasury Proposal
// ============================================================================

/**
 * A proposal for treasury spending.
 */
struct TreasuryProposal {
    /// Unique proposal ID
    ProposalId id;
    
    /// Proposal title
    std::string title;
    
    /// Detailed description
    std::string description;
    
    /// Spending category
    TreasuryCategory category{TreasuryCategory::Other};
    
    /// Requested amount
    Amount requestedAmount{0};
    
    /// Recipient address
    Hash160 recipient;
    
    /// Proposer's public key
    PublicKey proposer;
    
    /// Proposal deposit (refunded if approved)
    Amount deposit{0};
    
    /// Current status
    ProposalStatus status{ProposalStatus::Pending};
    
    /// Block height when submitted
    int submitHeight{0};
    
    /// Voting start height
    int votingStartHeight{0};
    
    /// Voting end height
    int votingEndHeight{0};
    
    /// Execution height (if approved)
    int executionHeight{0};
    
    /// Votes in favor
    uint64_t votesFor{0};
    
    /// Votes against
    uint64_t votesAgainst{0};
    
    /// Total voting power at snapshot
    uint64_t totalVotingPower{0};
    
    /// External URL (for detailed proposal)
    std::string url;
    
    /// Milestone-based release schedule (optional)
    struct Milestone {
        std::string description;
        Amount amount{0};
        int releaseHeight{0};
        bool released{false};
    };
    std::vector<Milestone> milestones;
    
    /// Calculate proposal hash
    Hash256 CalculateHash() const;
    
    /// Get approval percentage
    double GetApprovalPercent() const;
    
    /// Get participation percentage (quorum)
    double GetQuorumPercent() const;
    
    /// Check if proposal passed
    bool IsPassed() const;
    
    /// Check if quorum met
    bool HasQuorum() const;
    
    /// Check if voting period active
    bool IsVotingActive(int currentHeight) const;
    
    /// Check if ready for execution
    bool IsReadyForExecution(int currentHeight) const;
    
    /// Get total amount (including milestones)
    Amount GetTotalAmount() const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<TreasuryProposal> Deserialize(const Byte* data, size_t len);
    
    /// String representation
    std::string ToString() const;
};

// ============================================================================
// Vote
// ============================================================================

/**
 * A vote on a treasury proposal.
 */
struct TreasuryVote {
    /// Proposal being voted on
    ProposalId proposalId;
    
    /// Voter's public key
    PublicKey voter;
    
    /// Vote (true = for, false = against)
    bool inFavor{false};
    
    /// Voting power (based on stake/holdings)
    uint64_t votingPower{0};
    
    /// Block height of vote
    int voteHeight{0};
    
    /// Vote signature
    std::vector<Byte> signature;
    
    /// Calculate vote hash
    Hash256 GetHash() const;
    
    /// Create signature message
    std::vector<Byte> GetSignatureMessage() const;
    
    /// Verify signature
    bool VerifySignature() const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<TreasuryVote> Deserialize(const Byte* data, size_t len);
};

// ============================================================================
// Treasury Budget
// ============================================================================

/**
 * Budget tracking for a specific category.
 */
struct CategoryBudget {
    /// Category
    TreasuryCategory category;
    
    /// Allocated budget for current period
    Amount allocated{0};
    
    /// Amount spent this period
    Amount spent{0};
    
    /// Remaining budget
    Amount Remaining() const { return allocated > spent ? allocated - spent : 0; }
    
    /// Utilization percentage
    double Utilization() const { 
        return allocated > 0 ? static_cast<double>(spent) / allocated * 100.0 : 0.0;
    }
};

/**
 * Overall treasury budget allocation.
 */
struct TreasuryBudget {
    /// Budget period start height
    int periodStart{0};
    
    /// Budget period end height
    int periodEnd{0};
    
    /// Total treasury balance at period start
    Amount totalBalance{0};
    
    /// Budget by category
    std::map<TreasuryCategory, CategoryBudget> categories;
    
    /// Initialize budgets based on balance
    void Initialize(Amount balance, int startHeight, int periodBlocks);
    
    /// Get category budget
    const CategoryBudget* GetCategory(TreasuryCategory cat) const;
    
    /// Record spending
    bool RecordSpending(TreasuryCategory cat, Amount amount);
    
    /// Get total allocated
    Amount TotalAllocated() const;
    
    /// Get total spent
    Amount TotalSpent() const;
    
    /// String representation
    std::string ToString() const;
};

// ============================================================================
// Multi-Signature Configuration
// ============================================================================

/**
 * Multi-signature configuration for treasury operations.
 */
struct MultiSigConfig {
    /// Required signatures for standard spending
    int standardThreshold{3};
    
    /// Required signatures for large spending (>10% of balance)
    int largeThreshold{5};
    
    /// Required signatures for emergency operations
    int emergencyThreshold{2};
    
    /// Total signers
    int totalSigners{7};
    
    /// Authorized signers (public keys)
    std::vector<PublicKey> signers;
    
    /// Check if we have enough signatures
    bool HasEnoughSignatures(int count, Amount amount, Amount totalBalance) const;
    
    /// Validate configuration
    bool IsValid() const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<MultiSigConfig> Deserialize(const Byte* data, size_t len);
};

// ============================================================================
// Treasury
// ============================================================================

/**
 * Main treasury management class.
 * 
 * Manages the ecosystem development and other funds collected from
 * block rewards.
 */
class Treasury {
public:
    Treasury();
    ~Treasury();
    
    // ========================================================================
    // Fund Management
    // ========================================================================
    
    /// Add funds to treasury (from block rewards)
    void AddFunds(Amount amount, TreasuryCategory category);
    
    /// Get total balance
    Amount GetBalance() const { return balance_; }
    
    /// Get balance by category
    Amount GetCategoryBalance(TreasuryCategory category) const;
    
    /// Check if amount can be spent
    bool CanSpend(Amount amount, TreasuryCategory category) const;
    
    /// Execute a spending (after proposal approval)
    bool ExecuteSpending(const ProposalId& proposalId);
    
    // ========================================================================
    // Proposal Management
    // ========================================================================
    
    /**
     * Submit a new proposal.
     * 
     * @param proposal The proposal to submit
     * @param deposit Deposit amount (refunded if approved)
     * @param currentHeight Current block height
     * @return Proposal ID if successful
     */
    std::optional<ProposalId> SubmitProposal(
        TreasuryProposal proposal,
        Amount deposit,
        int currentHeight
    );
    
    /**
     * Vote on a proposal.
     * 
     * @param vote The vote
     * @param currentHeight Current block height
     * @return true if vote was recorded
     */
    bool SubmitVote(const TreasuryVote& vote, int currentHeight);
    
    /// Get proposal by ID
    const TreasuryProposal* GetProposal(const ProposalId& id) const;
    
    /// Get all proposals
    std::vector<const TreasuryProposal*> GetProposals(
        std::optional<ProposalStatus> status = std::nullopt
    ) const;
    
    /// Get active proposals (in voting period)
    std::vector<const TreasuryProposal*> GetActiveProposals(int currentHeight) const;
    
    /// Cancel a proposal (by proposer)
    bool CancelProposal(const ProposalId& id, const PublicKey& proposer);
    
    // ========================================================================
    // Voting
    // ========================================================================
    
    /// Get votes for a proposal
    std::vector<TreasuryVote> GetVotes(const ProposalId& proposalId) const;
    
    /// Check if voter has already voted
    bool HasVoted(const ProposalId& proposalId, const PublicKey& voter) const;
    
    /// Get voting power for a key
    uint64_t GetVotingPower(const PublicKey& key) const;
    
    /// Set voting power calculator
    using VotingPowerCalculator = std::function<uint64_t(const PublicKey&)>;
    void SetVotingPowerCalculator(VotingPowerCalculator calculator);
    
    // ========================================================================
    // Period Management
    // ========================================================================
    
    /// Process end of block (update statuses, execute approved proposals)
    void ProcessBlock(int height);
    
    /// Start a new budget period
    void StartNewPeriod(int height);
    
    /// Get current budget
    const TreasuryBudget& GetCurrentBudget() const { return currentBudget_; }
    
    /// Get budget period height
    int GetPeriodStartHeight() const { return currentBudget_.periodStart; }
    
    // ========================================================================
    // Multi-Sig Operations
    // ========================================================================
    
    /// Set multi-sig configuration
    void SetMultiSigConfig(const MultiSigConfig& config);
    
    /// Get multi-sig configuration
    const MultiSigConfig& GetMultiSigConfig() const { return multiSigConfig_; }
    
    /**
     * Emergency withdrawal (requires multi-sig).
     * 
     * @param amount Amount to withdraw
     * @param recipient Recipient address
     * @param signatures Signatures from authorized signers
     * @return true if successful
     */
    bool EmergencyWithdraw(
        Amount amount,
        const Hash160& recipient,
        const std::vector<std::pair<PublicKey, std::vector<Byte>>>& signatures
    );
    
    // ========================================================================
    // Reporting
    // ========================================================================
    
    /// Treasury report
    struct Report {
        /// Report timestamp
        std::chrono::system_clock::time_point timestamp;
        
        /// Block height
        int height{0};
        
        /// Total balance
        Amount totalBalance{0};
        
        /// Balance by category
        std::map<TreasuryCategory, Amount> categoryBalances;
        
        /// Total received this period
        Amount periodReceived{0};
        
        /// Total spent this period
        Amount periodSpent{0};
        
        /// Active proposals count
        size_t activeProposals{0};
        
        /// Executed proposals this period
        size_t executedProposals{0};
        
        /// String representation
        std::string ToString() const;
    };
    
    Report GenerateReport(int height) const;
    
    // ========================================================================
    // Serialization
    // ========================================================================
    
    std::vector<Byte> Serialize() const;
    bool Deserialize(const Byte* data, size_t len);

private:
    /// Total balance
    Amount balance_{0};
    
    /// Balance by category
    std::map<TreasuryCategory, Amount> categoryBalances_;
    
    /// All proposals
    std::map<ProposalId, TreasuryProposal> proposals_;
    
    /// Votes by proposal
    std::map<ProposalId, std::vector<TreasuryVote>> votes_;
    
    /// Current budget
    TreasuryBudget currentBudget_;
    
    /// Multi-sig configuration
    MultiSigConfig multiSigConfig_;
    
    /// Voting power calculator
    VotingPowerCalculator votingPowerCalculator_;
    
    /// Mutex for thread safety
    mutable std::mutex mutex_;
    
    /// Update proposal status based on voting results
    void UpdateProposalStatus(TreasuryProposal& proposal, int currentHeight);
    
    /// Execute approved proposals
    void ExecuteApprovedProposals(int currentHeight);
    
    /// Process milestone releases
    void ProcessMilestones(int currentHeight);
    
    /// Refund deposits for completed proposals
    void ProcessDeposits(int currentHeight);
};

// ============================================================================
// Treasury Output Builder
// ============================================================================

/**
 * Builds outputs for treasury-related transactions.
 */
class TreasuryOutputBuilder {
public:
    /// Build output for treasury deposit (from block reward)
    std::pair<std::vector<Byte>, Amount> BuildDepositOutput(
        const Hash160& treasuryAddress,
        Amount amount
    ) const;
    
    /// Build output for proposal spending
    std::vector<std::pair<std::vector<Byte>, Amount>> BuildSpendingOutputs(
        const TreasuryProposal& proposal
    ) const;
    
    /// Build output for deposit refund
    std::pair<std::vector<Byte>, Amount> BuildRefundOutput(
        const TreasuryProposal& proposal
    ) const;
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Calculate minimum deposit for proposal amount
Amount CalculateProposalDeposit(Amount proposalAmount);

/// Validate proposal parameters
bool ValidateProposal(const TreasuryProposal& proposal, Amount treasuryBalance);

/// Calculate voting power from stake
uint64_t CalculateVotingPower(Amount stake);

} // namespace economics
} // namespace shurium

#endif // SHURIUM_ECONOMICS_TREASURY_H
