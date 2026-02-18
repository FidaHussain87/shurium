// SHURIUM - Proof of Useful Work Marketplace
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Coordinates the PoUW marketplace - matching problems with solvers,
// distributing rewards, and managing the overall workflow.

#ifndef SHURIUM_MARKETPLACE_MARKETPLACE_H
#define SHURIUM_MARKETPLACE_MARKETPLACE_H

#include <shurium/core/types.h>
#include <shurium/marketplace/problem.h>
#include <shurium/marketplace/solution.h>
#include <shurium/marketplace/verifier.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace marketplace {

// ============================================================================
// Marketplace Events
// ============================================================================

/// Event types in the marketplace
enum class MarketplaceEvent {
    /// New problem added
    PROBLEM_ADDED,
    
    /// Problem expired
    PROBLEM_EXPIRED,
    
    /// Problem solved
    PROBLEM_SOLVED,
    
    /// Solution submitted
    SOLUTION_SUBMITTED,
    
    /// Solution verified (accepted or rejected)
    SOLUTION_VERIFIED,
    
    /// Reward distributed
    REWARD_DISTRIBUTED
};

/// Get string representation of event
const char* MarketplaceEventToString(MarketplaceEvent event);

// ============================================================================
// Marketplace Statistics
// ============================================================================

/**
 * Statistics about marketplace activity.
 */
struct MarketplaceStats {
    /// Total problems created
    uint64_t totalProblems{0};
    
    /// Total problems solved
    uint64_t totalSolved{0};
    
    /// Total problems expired
    uint64_t totalExpired{0};
    
    /// Total solutions submitted
    uint64_t totalSolutions{0};
    
    /// Total solutions accepted
    uint64_t totalAccepted{0};
    
    /// Total solutions rejected
    uint64_t totalRejected{0};
    
    /// Total rewards distributed
    Amount totalRewards{0};
    
    /// Current pending problems
    uint64_t pendingProblems{0};
    
    /// Current pending solutions
    uint64_t pendingSolutions{0};
    
    /// Average solution time (ms)
    uint64_t avgSolutionTime{0};
    
    /// Average verification time (ms)
    uint64_t avgVerificationTime{0};
    
    /// Problems by type
    std::vector<std::pair<ProblemType, uint64_t>> problemsByType;
    
    /// Convert to string
    std::string ToString() const;
};

// ============================================================================
// Marketplace Configuration
// ============================================================================

/**
 * Configuration for the marketplace.
 */
struct MarketplaceConfig {
    /// Maximum pending problems
    size_t maxPendingProblems{10000};
    
    /// Maximum pending solutions per problem
    size_t maxSolutionsPerProblem{100};
    
    /// Minimum problem reward
    Amount minProblemReward{1000};
    
    /// Maximum problem reward
    Amount maxProblemReward{1000000000};
    
    /// Minimum deadline (seconds from now)
    int64_t minDeadline{60};
    
    /// Maximum deadline (seconds from now)
    int64_t maxDeadline{86400 * 30};  // 30 days
    
    /// Verification timeout (ms)
    uint64_t verificationTimeout{120000};
    
    /// Max concurrent verifications
    size_t maxConcurrentVerifications{4};
    
    /// Enable automatic problem expiry
    bool autoExpireProblems{true};
    
    /// Problem expiry check interval (seconds)
    int64_t expiryCheckInterval{60};
};

// ============================================================================
// Marketplace Listener
// ============================================================================

/**
 * Listener for marketplace events.
 */
class IMarketplaceListener {
public:
    virtual ~IMarketplaceListener() = default;
    
    /// Called when a new problem is added
    virtual void OnProblemAdded(const Problem& problem) {}
    
    /// Called when a problem expires
    virtual void OnProblemExpired(Problem::Id problemId) {}
    
    /// Called when a problem is solved
    virtual void OnProblemSolved(
        Problem::Id problemId,
        Solution::Id solutionId,
        const std::string& solver) {}
    
    /// Called when a solution is submitted
    virtual void OnSolutionSubmitted(const Solution& solution) {}
    
    /// Called when a solution is verified
    virtual void OnSolutionVerified(
        Solution::Id solutionId,
        const VerificationDetails& result) {}
    
    /// Called when a reward is distributed
    virtual void OnRewardDistributed(
        const std::string& solver,
        Amount amount,
        Problem::Id problemId) {}
};

// ============================================================================
// Marketplace
// ============================================================================

/**
 * Main marketplace for the PoUW system.
 * 
 * Coordinates:
 * - Problem submission and distribution
 * - Solution submission and verification
 * - Reward calculation and distribution
 */
class Marketplace {
public:
    /// Get singleton instance
    static Marketplace& Instance();
    
    /// Create a marketplace with configuration
    explicit Marketplace(MarketplaceConfig config = {});
    
    /// Destructor
    ~Marketplace();
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    /// Start the marketplace
    void Start();
    
    /// Stop the marketplace
    void Stop();
    
    /// Check if marketplace is running
    bool IsRunning() const;
    
    // ========================================================================
    // Problem Management
    // ========================================================================
    
    /// Submit a new problem
    /// @return Problem ID or INVALID_ID on failure
    Problem::Id SubmitProblem(Problem problem);
    
    /// Get a problem by ID
    const Problem* GetProblem(Problem::Id id) const;
    
    /// Get a problem by hash
    const Problem* GetProblemByHash(const ProblemHash& hash) const;
    
    /// Get pending problems
    std::vector<const Problem*> GetPendingProblems(
        size_t maxCount = 100) const;
    
    /// Get problems by type
    std::vector<const Problem*> GetProblemsByType(
        ProblemType type,
        size_t maxCount = 100) const;
    
    /// Get problems for a creator
    std::vector<const Problem*> GetProblemsByCreator(
        const std::string& creator) const;
    
    /// Cancel a problem (creator only)
    bool CancelProblem(Problem::Id id, const std::string& requester);
    
    // ========================================================================
    // Solution Management
    // ========================================================================
    
    /// Submit a solution
    /// @return Solution ID or INVALID_ID on failure
    Solution::Id SubmitSolution(Solution solution);
    
    /// Get a solution by ID
    const Solution* GetSolution(Solution::Id id) const;
    
    /// Get solutions for a problem
    std::vector<const Solution*> GetSolutionsForProblem(
        Problem::Id problemId) const;
    
    /// Get solutions by solver
    std::vector<const Solution*> GetSolutionsBySolver(
        const std::string& solver,
        size_t maxCount = 100) const;
    
    /// Get pending solutions count for a problem
    size_t GetPendingSolutionCount(Problem::Id problemId) const;
    
    // ========================================================================
    // Verification
    // ========================================================================
    
    /// Manually trigger verification of a solution
    bool TriggerVerification(Solution::Id solutionId);
    
    /// Get verification status
    std::optional<VerificationDetails> GetVerificationResult(
        Solution::Id solutionId) const;
    
    // ========================================================================
    // Mining Interface
    // ========================================================================
    
    /// Get problems available for mining
    std::vector<const Problem*> GetMiningProblems(
        size_t maxCount = 10,
        Amount minReward = 0) const;
    
    /// Get the highest reward problem
    const Problem* GetHighestRewardProblem() const;
    
    /// Allocate a problem to a miner (prevents duplicate work)
    bool AllocateProblem(Problem::Id id, const std::string& miner);
    
    /// Release a problem allocation
    void ReleaseProblem(Problem::Id id, const std::string& miner);
    
    // ========================================================================
    // Rewards
    // ========================================================================
    
    /// Calculate reward for a solution
    Amount CalculateReward(
        const Problem& problem,
        const Solution& solution,
        const VerificationDetails& verification) const;
    
    /// Get total pending rewards
    Amount GetTotalPendingRewards() const;
    
    /// Get rewards earned by a solver
    Amount GetRewardsForSolver(const std::string& solver) const;
    
    // ========================================================================
    // Listeners
    // ========================================================================
    
    /// Add a marketplace listener
    void AddListener(IMarketplaceListener* listener);
    
    /// Remove a marketplace listener
    void RemoveListener(IMarketplaceListener* listener);
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get marketplace statistics
    MarketplaceStats GetStats() const;
    
    /// Get configuration
    const MarketplaceConfig& GetConfig() const { return config_; }
    
    // ========================================================================
    // Maintenance
    // ========================================================================
    
    /// Process expired problems
    size_t ProcessExpiredProblems();
    
    /// Clean up old data
    void Cleanup();
    
    /// Periodic tick (call from main loop)
    void Tick();

private:
    /// Handle verification complete
    void OnVerificationComplete(
        Solution::Id solutionId,
        const VerificationDetails& result);
    
    /// Distribute reward for a solution
    void DistributeReward(
        const Problem& problem,
        const Solution& solution,
        const VerificationDetails& verification);
    
    /// Notify listeners of an event
    void NotifyListeners(MarketplaceEvent event, const void* data);
    
    MarketplaceConfig config_;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Marketplace Mining Helper
// ============================================================================

/**
 * Helper class for miners interacting with the marketplace.
 */
class MiningHelper {
public:
    /// Create a mining helper
    explicit MiningHelper(Marketplace& marketplace);
    
    /// Get next problem to work on
    const Problem* GetNextProblem();
    
    /// Submit a solution
    Solution::Id SubmitSolution(
        Problem::Id problemId,
        const std::string& solver,
        std::vector<uint8_t> result,
        std::vector<uint8_t> proof);
    
    /// Check solution status
    SolutionStatus CheckStatus(Solution::Id solutionId) const;
    
    /// Wait for verification result
    VerificationDetails WaitForVerification(
        Solution::Id solutionId,
        uint64_t timeoutMs = 120000);

private:
    Marketplace& marketplace_;
    std::string currentMiner_;
};

} // namespace marketplace
} // namespace shurium

#endif // SHURIUM_MARKETPLACE_MARKETPLACE_H
