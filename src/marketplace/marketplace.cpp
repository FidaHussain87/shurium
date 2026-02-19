// SHURIUM - Proof of Useful Work Marketplace Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/marketplace/marketplace.h>
#include <shurium/util/time.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

namespace shurium {
namespace marketplace {

// ============================================================================
// Event String Conversion
// ============================================================================

const char* MarketplaceEventToString(MarketplaceEvent event) {
    switch (event) {
        case MarketplaceEvent::PROBLEM_ADDED: return "problem_added";
        case MarketplaceEvent::PROBLEM_EXPIRED: return "problem_expired";
        case MarketplaceEvent::PROBLEM_SOLVED: return "problem_solved";
        case MarketplaceEvent::SOLUTION_SUBMITTED: return "solution_submitted";
        case MarketplaceEvent::SOLUTION_VERIFIED: return "solution_verified";
        case MarketplaceEvent::REWARD_DISTRIBUTED: return "reward_distributed";
        default: return "unknown";
    }
}

// ============================================================================
// MarketplaceStats Implementation
// ============================================================================

std::string MarketplaceStats::ToString() const {
    std::ostringstream oss;
    oss << "MarketplaceStats{"
        << "problems=" << totalProblems
        << ", solved=" << totalSolved
        << ", expired=" << totalExpired
        << ", solutions=" << totalSolutions
        << ", accepted=" << totalAccepted
        << ", rejected=" << totalRejected
        << ", rewards=" << totalRewards
        << ", pending_problems=" << pendingProblems
        << ", pending_solutions=" << pendingSolutions
        << "}";
    return oss.str();
}

// ============================================================================
// Marketplace Implementation
// ============================================================================

class Marketplace::Impl {
public:
    explicit Impl(MarketplaceConfig config) : config_(std::move(config)) {}
    
    MarketplaceConfig config_;
    
    std::atomic<bool> running_{false};
    
    // Problem storage
    mutable std::mutex problemsMutex_;
    std::map<Problem::Id, Problem> problems_;
    std::map<ProblemHash, Problem::Id> problemHashIndex_;
    uint64_t nextProblemId_{1};
    
    // Solution storage
    mutable std::mutex solutionsMutex_;
    std::map<Solution::Id, Solution> solutions_;
    std::map<Hash256, Solution::Id> solutionHashIndex_;
    std::map<Problem::Id, std::vector<Solution::Id>> problemSolutions_;
    uint64_t nextSolutionId_{1};
    
    // Allocations (problem -> set of miners working on it)
    mutable std::mutex allocationsMutex_;
    std::map<Problem::Id, std::set<std::string>> allocations_;
    
    // Verifier
    SolutionVerifier verifier_;
    
    // Listeners
    mutable std::mutex listenersMutex_;
    std::vector<IMarketplaceListener*> listeners_;
    
    // Statistics
    std::atomic<uint64_t> totalProblems_{0};
    std::atomic<uint64_t> totalSolved_{0};
    std::atomic<uint64_t> totalExpired_{0};
    std::atomic<uint64_t> totalSolutions_{0};
    std::atomic<uint64_t> totalAccepted_{0};
    std::atomic<uint64_t> totalRejected_{0};
    std::atomic<Amount> totalRewards_{0};
    
    // Solver rewards
    mutable std::mutex rewardsMutex_;
    std::map<std::string, Amount> solverRewards_;
    
    // Last tick time
    int64_t lastTickTime_{0};
};

Marketplace& Marketplace::Instance() {
    static Marketplace instance;
    return instance;
}

Marketplace::Marketplace(MarketplaceConfig config)
    : config_(config)
    , impl_(std::make_unique<Impl>(config)) {
}

Marketplace::~Marketplace() {
    Stop();
}

void Marketplace::Start() {
    impl_->running_ = true;
    impl_->lastTickTime_ = GetTime();
}

void Marketplace::Stop() {
    impl_->running_ = false;
}

bool Marketplace::IsRunning() const {
    return impl_->running_.load();
}

// ============================================================================
// Problem Management
// ============================================================================

Problem::Id Marketplace::SubmitProblem(Problem problem) {
    if (!IsRunning()) {
        return Problem::INVALID_ID;
    }
    
    // Validate problem
    if (!problem.IsValid()) {
        return Problem::INVALID_ID;
    }
    
    // Check reward limits
    if (problem.GetReward() < config_.minProblemReward ||
        problem.GetReward() > config_.maxProblemReward) {
        return Problem::INVALID_ID;
    }
    
    // Check deadline limits
    int64_t now = GetTime();
    int64_t deadlineOffset = problem.GetDeadline() - now;
    if (deadlineOffset < config_.minDeadline || 
        deadlineOffset > config_.maxDeadline) {
        return Problem::INVALID_ID;
    }
    
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    // Check pool size
    if (impl_->problems_.size() >= config_.maxPendingProblems) {
        return Problem::INVALID_ID;
    }
    
    // Assign ID
    Problem::Id id = impl_->nextProblemId_++;
    problem.SetId(id);
    problem.ComputeHash();
    
    // Store problem
    impl_->problemHashIndex_[problem.GetHash()] = id;
    impl_->problems_.emplace(id, problem);
    impl_->totalProblems_++;
    
    // Notify listeners
    NotifyListeners(MarketplaceEvent::PROBLEM_ADDED, &problem);
    
    // Call listener methods directly
    std::lock_guard<std::mutex> listenerLock(impl_->listenersMutex_);
    for (auto* listener : impl_->listeners_) {
        listener->OnProblemAdded(problem);
    }
    
    return id;
}

const Problem* Marketplace::GetProblem(Problem::Id id) const {
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    auto it = impl_->problems_.find(id);
    if (it == impl_->problems_.end()) {
        return nullptr;
    }
    return &it->second;
}

const Problem* Marketplace::GetProblemByHash(const ProblemHash& hash) const {
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    auto hashIt = impl_->problemHashIndex_.find(hash);
    if (hashIt == impl_->problemHashIndex_.end()) {
        return nullptr;
    }
    
    auto it = impl_->problems_.find(hashIt->second);
    if (it == impl_->problems_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<const Problem*> Marketplace::GetPendingProblems(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    std::vector<const Problem*> result;
    result.reserve(std::min(maxCount, impl_->problems_.size()));
    
    for (const auto& [id, problem] : impl_->problems_) {
        if (!problem.IsSolved() && !problem.IsExpired()) {
            result.push_back(&problem);
            if (result.size() >= maxCount) break;
        }
    }
    
    return result;
}

std::vector<const Problem*> Marketplace::GetProblemsByType(
    ProblemType type,
    size_t maxCount) const {
    
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    std::vector<const Problem*> result;
    
    for (const auto& [id, problem] : impl_->problems_) {
        if (problem.GetType() == type && !problem.IsSolved() && !problem.IsExpired()) {
            result.push_back(&problem);
            if (result.size() >= maxCount) break;
        }
    }
    
    return result;
}

std::vector<const Problem*> Marketplace::GetProblemsByCreator(
    const std::string& creator) const {
    
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    std::vector<const Problem*> result;
    
    for (const auto& [id, problem] : impl_->problems_) {
        if (problem.GetCreator() == creator) {
            result.push_back(&problem);
        }
    }
    
    return result;
}

bool Marketplace::CancelProblem(Problem::Id id, const std::string& requester) {
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    auto it = impl_->problems_.find(id);
    if (it == impl_->problems_.end()) {
        return false;
    }
    
    // Only creator can cancel
    if (it->second.GetCreator() != requester) {
        return false;
    }
    
    // Can't cancel solved problems
    if (it->second.IsSolved()) {
        return false;
    }
    
    impl_->problemHashIndex_.erase(it->second.GetHash());
    impl_->problems_.erase(it);
    
    return true;
}

// ============================================================================
// Solution Management
// ============================================================================

Solution::Id Marketplace::SubmitSolution(Solution solution) {
    if (!IsRunning()) {
        return Solution::INVALID_ID;
    }
    
    // Get the problem
    const Problem* problem = GetProblem(solution.GetProblemId());
    if (!problem) {
        return Solution::INVALID_ID;
    }
    
    // Check problem is still valid
    if (problem->IsSolved() || problem->IsExpired()) {
        return Solution::INVALID_ID;
    }
    
    // Check solution count for problem
    {
        std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
        auto it = impl_->problemSolutions_.find(solution.GetProblemId());
        if (it != impl_->problemSolutions_.end() && 
            it->second.size() >= config_.maxSolutionsPerProblem) {
            return Solution::INVALID_ID;
        }
    }
    
    // Validate solution structure
    if (!solution.IsValid()) {
        return Solution::INVALID_ID;
    }
    
    Solution::Id id;
    
    {
        std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
        
        // Assign ID
        id = impl_->nextSolutionId_++;
        solution.SetId(id);
        solution.SetSubmissionTime(GetTime());
        solution.SetProblemHash(problem->GetHash());
        solution.ComputeHash();
        
        // Store solution
        impl_->solutionHashIndex_[solution.GetHash()] = id;
        impl_->problemSolutions_[solution.GetProblemId()].push_back(id);
        impl_->solutions_.emplace(id, solution);
        impl_->totalSolutions_++;
        
        // Notify listeners
        {
            std::lock_guard<std::mutex> listenerLock(impl_->listenersMutex_);
            for (auto* listener : impl_->listeners_) {
                listener->OnSolutionSubmitted(solution);
            }
        }
    }  // Release solutionsMutex_ BEFORE calling TriggerVerification
    
    // Submit for verification (must be outside lock to avoid deadlock)
    TriggerVerification(id);
    
    return id;
}

const Solution* Marketplace::GetSolution(Solution::Id id) const {
    std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
    
    auto it = impl_->solutions_.find(id);
    if (it == impl_->solutions_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<const Solution*> Marketplace::GetSolutionsForProblem(
    Problem::Id problemId) const {
    
    std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
    
    std::vector<const Solution*> result;
    
    auto it = impl_->problemSolutions_.find(problemId);
    if (it == impl_->problemSolutions_.end()) {
        return result;
    }
    
    result.reserve(it->second.size());
    for (Solution::Id id : it->second) {
        auto solIt = impl_->solutions_.find(id);
        if (solIt != impl_->solutions_.end()) {
            result.push_back(&solIt->second);
        }
    }
    
    return result;
}

std::vector<const Solution*> Marketplace::GetSolutionsBySolver(
    const std::string& solver,
    size_t maxCount) const {
    
    std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
    
    std::vector<const Solution*> result;
    
    for (const auto& [id, solution] : impl_->solutions_) {
        if (solution.GetSolver() == solver) {
            result.push_back(&solution);
            if (result.size() >= maxCount) break;
        }
    }
    
    return result;
}

size_t Marketplace::GetPendingSolutionCount(Problem::Id problemId) const {
    std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
    
    auto it = impl_->problemSolutions_.find(problemId);
    if (it == impl_->problemSolutions_.end()) {
        return 0;
    }
    
    size_t count = 0;
    for (Solution::Id id : it->second) {
        auto solIt = impl_->solutions_.find(id);
        if (solIt != impl_->solutions_.end() && solIt->second.IsPending()) {
            ++count;
        }
    }
    
    return count;
}

// ============================================================================
// Verification
// ============================================================================

bool Marketplace::TriggerVerification(Solution::Id solutionId) {
    // Get solution
    Solution solution;
    {
        std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
        auto it = impl_->solutions_.find(solutionId);
        if (it == impl_->solutions_.end()) {
            return false;
        }
        solution = it->second;
        it->second.SetStatus(SolutionStatus::VERIFYING);
    }
    
    // Get problem
    const Problem* problem = GetProblem(solution.GetProblemId());
    if (!problem) {
        return false;
    }
    
    // Verify synchronously for now (async would use SubmitForVerification)
    VerificationDetails result = impl_->verifier_.Verify(*problem, solution);
    
    // Handle result
    OnVerificationComplete(solutionId, result);
    
    return true;
}

std::optional<VerificationDetails> Marketplace::GetVerificationResult(
    Solution::Id solutionId) const {
    
    std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
    
    auto it = impl_->solutions_.find(solutionId);
    if (it == impl_->solutions_.end()) {
        return std::nullopt;
    }
    
    // For now, return a basic result based on status
    VerificationDetails details;
    switch (it->second.GetStatus()) {
        case SolutionStatus::ACCEPTED:
            details.result = VerificationResult::VALID;
            break;
        case SolutionStatus::REJECTED:
            details.result = VerificationResult::INVALID;
            break;
        case SolutionStatus::PENDING:
        case SolutionStatus::VERIFYING:
            return std::nullopt;  // Not yet verified
        default:
            details.result = VerificationResult::ERROR;
    }
    
    return details;
}

void Marketplace::OnVerificationComplete(
    Solution::Id solutionId,
    const VerificationDetails& result) {
    
    Solution solution;
    Problem problem;
    
    {
        std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
        auto it = impl_->solutions_.find(solutionId);
        if (it == impl_->solutions_.end()) {
            return;
        }
        
        // Update solution status
        if (result.IsValid()) {
            it->second.SetStatus(SolutionStatus::ACCEPTED);
            impl_->totalAccepted_++;
        } else {
            it->second.SetStatus(SolutionStatus::REJECTED);
            impl_->totalRejected_++;
        }
        
        it->second.SetVerificationTime(GetTime());
        solution = it->second;
    }
    
    // Get problem
    {
        std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
        auto it = impl_->problems_.find(solution.GetProblemId());
        if (it == impl_->problems_.end()) {
            return;
        }
        problem = it->second;
    }
    
    // Notify listeners
    {
        std::lock_guard<std::mutex> listenerLock(impl_->listenersMutex_);
        for (auto* listener : impl_->listeners_) {
            listener->OnSolutionVerified(solutionId, result);
        }
    }
    
    // Distribute reward if valid
    if (result.IsValid()) {
        DistributeReward(problem, solution, result);
    }
}

void Marketplace::DistributeReward(
    const Problem& problem,
    const Solution& solution,
    const VerificationDetails& verification) {
    
    // Calculate reward
    Amount reward = CalculateReward(problem, solution, verification);
    
    // Update solution with reward
    {
        std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
        auto it = impl_->solutions_.find(solution.GetId());
        if (it != impl_->solutions_.end()) {
            it->second.SetReward(reward);
        }
    }
    
    // Mark problem as solved
    {
        std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
        auto it = impl_->problems_.find(problem.GetId());
        if (it != impl_->problems_.end()) {
            it->second.SetSolved(true);
            it->second.SetSolver(solution.GetSolver());
            impl_->totalSolved_++;
        }
    }
    
    // Record reward
    {
        std::lock_guard<std::mutex> lock(impl_->rewardsMutex_);
        impl_->solverRewards_[solution.GetSolver()] += reward;
        impl_->totalRewards_ += reward;
    }
    
    // Notify listeners
    {
        std::lock_guard<std::mutex> listenerLock(impl_->listenersMutex_);
        for (auto* listener : impl_->listeners_) {
            listener->OnProblemSolved(problem.GetId(), solution.GetId(), solution.GetSolver());
            listener->OnRewardDistributed(solution.GetSolver(), reward, problem.GetId());
        }
    }
}

// ============================================================================
// Mining Interface
// ============================================================================

std::vector<const Problem*> Marketplace::GetMiningProblems(
    size_t maxCount,
    Amount minReward) const {
    
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    // Collect eligible problems sorted by reward
    std::vector<std::pair<Amount, const Problem*>> eligible;
    
    for (const auto& [id, problem] : impl_->problems_) {
        if (!problem.IsSolved() && !problem.IsExpired() && 
            problem.GetReward() >= minReward) {
            eligible.emplace_back(problem.GetReward(), &problem);
        }
    }
    
    // Sort by reward (highest first)
    std::sort(eligible.begin(), eligible.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Return top problems
    std::vector<const Problem*> result;
    result.reserve(std::min(maxCount, eligible.size()));
    
    for (size_t i = 0; i < std::min(maxCount, eligible.size()); ++i) {
        result.push_back(eligible[i].second);
    }
    
    return result;
}

const Problem* Marketplace::GetHighestRewardProblem() const {
    auto problems = GetMiningProblems(1, 0);
    return problems.empty() ? nullptr : problems[0];
}

bool Marketplace::AllocateProblem(Problem::Id id, const std::string& miner) {
    std::lock_guard<std::mutex> lock(impl_->allocationsMutex_);
    impl_->allocations_[id].insert(miner);
    return true;
}

void Marketplace::ReleaseProblem(Problem::Id id, const std::string& miner) {
    std::lock_guard<std::mutex> lock(impl_->allocationsMutex_);
    
    auto it = impl_->allocations_.find(id);
    if (it != impl_->allocations_.end()) {
        it->second.erase(miner);
    }
}

// ============================================================================
// Rewards
// ============================================================================

Amount Marketplace::CalculateReward(
    const Problem& problem,
    const Solution& solution,
    const VerificationDetails& verification) const {
    
    Amount baseReward = problem.GetReward();
    
    // Apply bonus if solution was submitted early
    int64_t submissionTime = solution.GetSubmissionTime();
    int64_t deadline = problem.GetDeadline();
    int64_t creationTime = problem.GetCreationTime();
    
    if (deadline > creationTime) {
        double progress = static_cast<double>(submissionTime - creationTime) / 
                         (deadline - creationTime);
        
        // Give bonus for early submission
        if (progress < 0.5 && problem.GetBonusReward() > 0) {
            Amount bonus = static_cast<Amount>(
                problem.GetBonusReward() * (1.0 - progress * 2));
            baseReward += bonus;
        }
    }
    
    // Scale by verification score
    if (verification.score > 0) {
        baseReward = static_cast<Amount>(
            baseReward * (static_cast<double>(verification.score) / 1000000.0));
    }
    
    return baseReward;
}

Amount Marketplace::GetTotalPendingRewards() const {
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    Amount total = 0;
    for (const auto& [id, problem] : impl_->problems_) {
        if (!problem.IsSolved() && !problem.IsExpired()) {
            total += problem.GetReward();
        }
    }
    
    return total;
}

Amount Marketplace::GetRewardsForSolver(const std::string& solver) const {
    std::lock_guard<std::mutex> lock(impl_->rewardsMutex_);
    
    auto it = impl_->solverRewards_.find(solver);
    if (it == impl_->solverRewards_.end()) {
        return 0;
    }
    return it->second;
}

// ============================================================================
// Listeners
// ============================================================================

void Marketplace::AddListener(IMarketplaceListener* listener) {
    std::lock_guard<std::mutex> lock(impl_->listenersMutex_);
    impl_->listeners_.push_back(listener);
}

void Marketplace::RemoveListener(IMarketplaceListener* listener) {
    std::lock_guard<std::mutex> lock(impl_->listenersMutex_);
    impl_->listeners_.erase(
        std::remove(impl_->listeners_.begin(), impl_->listeners_.end(), listener),
        impl_->listeners_.end());
}

void Marketplace::NotifyListeners(MarketplaceEvent event, const void* /* data */) {
    // Event notifications are handled directly in specific methods
    (void)event;
}

// ============================================================================
// Statistics
// ============================================================================

MarketplaceStats Marketplace::GetStats() const {
    MarketplaceStats stats;
    
    stats.totalProblems = impl_->totalProblems_.load();
    stats.totalSolved = impl_->totalSolved_.load();
    stats.totalExpired = impl_->totalExpired_.load();
    stats.totalSolutions = impl_->totalSolutions_.load();
    stats.totalAccepted = impl_->totalAccepted_.load();
    stats.totalRejected = impl_->totalRejected_.load();
    stats.totalRewards = impl_->totalRewards_.load();
    
    {
        std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
        stats.pendingProblems = 0;
        for (const auto& [id, problem] : impl_->problems_) {
            if (!problem.IsSolved() && !problem.IsExpired()) {
                stats.pendingProblems++;
            }
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(impl_->solutionsMutex_);
        stats.pendingSolutions = 0;
        for (const auto& [id, solution] : impl_->solutions_) {
            if (solution.IsPending()) {
                stats.pendingSolutions++;
            }
        }
    }
    
    stats.avgVerificationTime = impl_->verifier_.GetAverageVerificationTime();
    
    return stats;
}

// ============================================================================
// Maintenance
// ============================================================================

size_t Marketplace::ProcessExpiredProblems() {
    std::lock_guard<std::mutex> lock(impl_->problemsMutex_);
    
    int64_t now = GetTime();
    size_t removed = 0;
    
    std::vector<Problem::Id> expired;
    
    for (auto& [id, problem] : impl_->problems_) {
        if (!problem.IsSolved() && problem.IsExpiredAt(now)) {
            expired.push_back(id);
        }
    }
    
    for (Problem::Id id : expired) {
        auto it = impl_->problems_.find(id);
        if (it != impl_->problems_.end()) {
            impl_->problemHashIndex_.erase(it->second.GetHash());
            impl_->problems_.erase(it);
            impl_->totalExpired_++;
            removed++;
            
            // Notify listeners
            std::lock_guard<std::mutex> listenerLock(impl_->listenersMutex_);
            for (auto* listener : impl_->listeners_) {
                listener->OnProblemExpired(id);
            }
        }
    }
    
    return removed;
}

void Marketplace::Cleanup() {
    ProcessExpiredProblems();
}

void Marketplace::Tick() {
    int64_t now = GetTime();
    
    // Process expired problems periodically
    if (config_.autoExpireProblems && 
        now - impl_->lastTickTime_ >= config_.expiryCheckInterval) {
        ProcessExpiredProblems();
        impl_->lastTickTime_ = now;
    }
}

// ============================================================================
// MiningHelper Implementation
// ============================================================================

MiningHelper::MiningHelper(Marketplace& marketplace)
    : marketplace_(marketplace) {
}

const Problem* MiningHelper::GetNextProblem() {
    return marketplace_.GetHighestRewardProblem();
}

Solution::Id MiningHelper::SubmitSolution(
    Problem::Id problemId,
    const std::string& solver,
    std::vector<uint8_t> result,
    std::vector<uint8_t> proof) {
    
    const Problem* problem = marketplace_.GetProblem(problemId);
    if (!problem) {
        return Solution::INVALID_ID;
    }
    
    Solution solution = SolutionBuilder(*problem)
        .SetSolver(solver)
        .SetResult(std::move(result))
        .SetProof(std::move(proof))
        .SetComputeTime(0)
        .BuildWithHash();
    
    return marketplace_.SubmitSolution(std::move(solution));
}

SolutionStatus MiningHelper::CheckStatus(Solution::Id solutionId) const {
    const Solution* solution = marketplace_.GetSolution(solutionId);
    if (!solution) {
        return SolutionStatus::PENDING;
    }
    return solution->GetStatus();
}

VerificationDetails MiningHelper::WaitForVerification(
    Solution::Id solutionId,
    uint64_t timeoutMs) {
    
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        auto result = marketplace_.GetVerificationResult(solutionId);
        if (result.has_value()) {
            return result.value();
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (static_cast<uint64_t>(elapsed) >= timeoutMs) {
            VerificationDetails details;
            details.result = VerificationResult::TIMEOUT;
            details.errorMessage = "Verification timed out";
            return details;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace marketplace
} // namespace shurium
