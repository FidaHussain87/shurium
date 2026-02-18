// SHURIUM - Proof of Useful Work Solution Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/marketplace/solution.h>
#include <shurium/crypto/sha256.h>
#include <shurium/util/time.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <sstream>

namespace shurium {
namespace marketplace {

// ============================================================================
// Solution Status String Conversion
// ============================================================================

const char* SolutionStatusToString(SolutionStatus status) {
    switch (status) {
        case SolutionStatus::PENDING: return "pending";
        case SolutionStatus::VERIFYING: return "verifying";
        case SolutionStatus::ACCEPTED: return "accepted";
        case SolutionStatus::REJECTED: return "rejected";
        case SolutionStatus::EXPIRED: return "expired";
        default: return "unknown";
    }
}

// ============================================================================
// SolutionData Implementation
// ============================================================================

void SolutionData::SetResult(std::vector<uint8_t> result) {
    if (result.size() <= MAX_RESULT_SIZE) {
        result_ = std::move(result);
        ComputeResultHash();
    }
}

void SolutionData::ComputeResultHash() {
    if (result_.empty()) {
        resultHash_.SetNull();
        return;
    }
    
    SHA256 hasher;
    hasher.Write(result_.data(), result_.size());
    hasher.Finalize(resultHash_.data());
}

bool SolutionData::IsValid() const {
    // Must have some result data
    if (result_.empty()) {
        return false;
    }
    
    // Verify result hash
    Hash256 computedHash;
    SHA256 hasher;
    hasher.Write(result_.data(), result_.size());
    hasher.Finalize(computedHash.data());
    
    if (computedHash != resultHash_) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Solution Implementation
// ============================================================================

Solution::Solution() = default;

Solution::Solution(Problem::Id problemId) : problemId_(problemId) {}

Solution::~Solution() = default;

void Solution::ComputeHash() {
    DataStream stream;
    
    // Hash the key fields (not including status/verification results)
    ::shurium::Serialize(stream, problemId_);
    ::shurium::Serialize(stream, problemHash_);
    ::shurium::Serialize(stream, data_.GetResultHash());
    ::shurium::Serialize(stream, solver_);
    ::shurium::Serialize(stream, nonce_);
    
    SHA256 hasher;
    hasher.Write(stream.data(), stream.size());
    hasher.Finalize(hash_.data());
}

bool Solution::IsValid() const {
    // Must have a problem reference
    if (problemId_ == Problem::INVALID_ID) {
        return false;
    }
    
    // Must have valid solution data
    if (!data_.IsValid()) {
        return false;
    }
    
    // Must have a solver
    if (solver_.empty()) {
        return false;
    }
    
    return true;
}

std::string Solution::ToString() const {
    std::ostringstream oss;
    oss << "Solution{id=" << id_
        << ", problemId=" << problemId_
        << ", solver=" << solver_
        << ", status=" << SolutionStatusToString(status_)
        << ", reward=" << reward_
        << "}";
    return oss.str();
}

// ============================================================================
// SolutionBuilder Implementation
// ============================================================================

SolutionBuilder::SolutionBuilder(const Problem& problem)
    : problemId_(problem.GetId())
    , problemHash_(problem.GetHash()) {
}

SolutionBuilder& SolutionBuilder::SetSolver(const std::string& addr) {
    solver_ = addr;
    return *this;
}

SolutionBuilder& SolutionBuilder::SetNonce(uint64_t nonce) {
    nonce_ = nonce;
    return *this;
}

SolutionBuilder& SolutionBuilder::SetResult(std::vector<uint8_t> result) {
    data_.SetResult(std::move(result));
    return *this;
}

SolutionBuilder& SolutionBuilder::SetProof(std::vector<uint8_t> proof) {
    data_.SetProof(std::move(proof));
    return *this;
}

SolutionBuilder& SolutionBuilder::AddIntermediate(const Hash256& hash) {
    data_.AddIntermediate(hash);
    return *this;
}

SolutionBuilder& SolutionBuilder::SetComputeTime(uint64_t ms) {
    data_.SetComputeTime(ms);
    return *this;
}

SolutionBuilder& SolutionBuilder::SetIterations(uint64_t iters) {
    data_.SetIterations(iters);
    return *this;
}

SolutionBuilder& SolutionBuilder::SetAccuracy(uint32_t accuracy) {
    data_.SetAccuracy(accuracy);
    return *this;
}

Solution SolutionBuilder::Build() {
    Solution solution(problemId_);
    solution.SetProblemHash(problemHash_);
    solution.SetSolver(solver_);
    solution.SetNonce(nonce_);
    solution.SetData(std::move(data_));
    solution.SetSubmissionTime(GetTime());
    return solution;
}

Solution SolutionBuilder::BuildWithHash() {
    Solution solution = Build();
    solution.ComputeHash();
    return solution;
}

// ============================================================================
// SolutionCache Implementation
// ============================================================================

class SolutionCache::Impl {
public:
    explicit Impl(size_t maxSize) : maxSize_(maxSize) {}
    
    mutable std::mutex mutex_;
    size_t maxSize_;
    std::map<Solution::Id, Solution> solutions_;
    std::map<Hash256, Solution::Id> hashIndex_;
    std::map<Problem::Id, std::vector<Solution::Id>> problemIndex_;
};

SolutionCache::SolutionCache(size_t maxSize)
    : impl_(std::make_unique<Impl>(maxSize)) {
}

SolutionCache::~SolutionCache() = default;

void SolutionCache::Add(Solution solution) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    // Remove oldest if at capacity
    while (impl_->solutions_.size() >= impl_->maxSize_ && !impl_->solutions_.empty()) {
        auto oldest = impl_->solutions_.begin();
        impl_->hashIndex_.erase(oldest->second.GetHash());
        
        // Remove from problem index
        auto& vec = impl_->problemIndex_[oldest->second.GetProblemId()];
        vec.erase(std::remove(vec.begin(), vec.end(), oldest->first), vec.end());
        
        impl_->solutions_.erase(oldest);
    }
    
    Solution::Id id = solution.GetId();
    Problem::Id problemId = solution.GetProblemId();
    Hash256 hash = solution.GetHash();
    
    impl_->hashIndex_[hash] = id;
    impl_->problemIndex_[problemId].push_back(id);
    impl_->solutions_.emplace(id, std::move(solution));
}

const Solution* SolutionCache::Get(Solution::Id id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto it = impl_->solutions_.find(id);
    if (it == impl_->solutions_.end()) {
        return nullptr;
    }
    return &it->second;
}

const Solution* SolutionCache::GetByHash(const Hash256& hash) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto hashIt = impl_->hashIndex_.find(hash);
    if (hashIt == impl_->hashIndex_.end()) {
        return nullptr;
    }
    
    auto it = impl_->solutions_.find(hashIt->second);
    if (it == impl_->solutions_.end()) {
        return nullptr;
    }
    return &it->second;
}

bool SolutionCache::Has(Solution::Id id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->solutions_.find(id) != impl_->solutions_.end();
}

void SolutionCache::Remove(Solution::Id id) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto it = impl_->solutions_.find(id);
    if (it == impl_->solutions_.end()) {
        return;
    }
    
    impl_->hashIndex_.erase(it->second.GetHash());
    
    // Remove from problem index
    auto& vec = impl_->problemIndex_[it->second.GetProblemId()];
    vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    
    impl_->solutions_.erase(it);
}

std::vector<const Solution*> SolutionCache::GetForProblem(Problem::Id problemId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    std::vector<const Solution*> result;
    
    auto it = impl_->problemIndex_.find(problemId);
    if (it == impl_->problemIndex_.end()) {
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

void SolutionCache::Clear() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->solutions_.clear();
    impl_->hashIndex_.clear();
    impl_->problemIndex_.clear();
}

size_t SolutionCache::Size() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->solutions_.size();
}

} // namespace marketplace
} // namespace shurium
