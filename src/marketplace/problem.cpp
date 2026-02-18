// SHURIUM - Proof of Useful Work Problem Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/marketplace/problem.h>
#include <shurium/crypto/sha256.h>
#include <shurium/util/time.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <sstream>

namespace shurium {
namespace marketplace {

// ============================================================================
// Problem Type String Conversion
// ============================================================================

const char* ProblemTypeToString(ProblemType type) {
    switch (type) {
        case ProblemType::UNKNOWN: return "unknown";
        case ProblemType::ML_TRAINING: return "ml_training";
        case ProblemType::ML_INFERENCE: return "ml_inference";
        case ProblemType::LINEAR_ALGEBRA: return "linear_algebra";
        case ProblemType::HASH_POW: return "hash_pow";
        case ProblemType::SIMULATION: return "simulation";
        case ProblemType::DATA_PROCESSING: return "data_processing";
        case ProblemType::OPTIMIZATION: return "optimization";
        case ProblemType::CRYPTOGRAPHIC: return "cryptographic";
        case ProblemType::CUSTOM: return "custom";
        default: return "unknown";
    }
}

std::optional<ProblemType> ProblemTypeFromString(const std::string& str) {
    if (str == "unknown") return ProblemType::UNKNOWN;
    if (str == "ml_training") return ProblemType::ML_TRAINING;
    if (str == "ml_inference") return ProblemType::ML_INFERENCE;
    if (str == "linear_algebra") return ProblemType::LINEAR_ALGEBRA;
    if (str == "hash_pow") return ProblemType::HASH_POW;
    if (str == "simulation") return ProblemType::SIMULATION;
    if (str == "data_processing") return ProblemType::DATA_PROCESSING;
    if (str == "optimization") return ProblemType::OPTIMIZATION;
    if (str == "cryptographic") return ProblemType::CRYPTOGRAPHIC;
    if (str == "custom") return ProblemType::CUSTOM;
    return std::nullopt;
}

// ============================================================================
// ProblemSpec Implementation
// ============================================================================

void ProblemSpec::SetDescription(const std::string& desc) {
    description_ = desc;
    if (description_.size() > MAX_DESCRIPTION_LENGTH) {
        description_.resize(MAX_DESCRIPTION_LENGTH);
    }
}

void ProblemSpec::SetInputData(std::vector<uint8_t> data) {
    if (data.size() <= MAX_DATA_SIZE) {
        inputData_ = std::move(data);
    }
}

void ProblemSpec::SetVerificationData(std::vector<uint8_t> data) {
    if (data.size() <= MAX_DATA_SIZE) {
        verificationData_ = std::move(data);
    }
}

bool ProblemSpec::IsValid() const {
    // Must have a valid type
    if (type_ == ProblemType::UNKNOWN) {
        return false;
    }
    
    // Must have some input data for most types
    if (type_ != ProblemType::HASH_POW && inputData_.empty()) {
        return false;
    }
    
    return true;
}

Hash256 ProblemSpec::GetHash() const {
    DataStream stream;
    Serialize(stream);
    
    SHA256 hasher;
    hasher.Write(stream.data(), stream.size());
    
    Hash256 hash;
    hasher.Finalize(hash.data());
    return hash;
}

// ============================================================================
// Problem Implementation
// ============================================================================

Problem::Problem() = default;

Problem::Problem(ProblemSpec spec) : spec_(std::move(spec)) {
    ComputeHash();
}

Problem::~Problem() = default;

void Problem::ComputeHash() {
    hash_ = ProblemHash(spec_.GetHash());
}

void Problem::SetSpec(ProblemSpec spec) {
    spec_ = std::move(spec);
    ComputeHash();
}

bool Problem::IsExpired() const {
    return IsExpiredAt(GetTime());
}

bool Problem::IsExpiredAt(int64_t time) const {
    return deadline_ > 0 && time > deadline_;
}

bool Problem::IsValid() const {
    // Check specification
    if (!spec_.IsValid()) {
        return false;
    }
    
    // Check reward
    if (reward_ <= 0) {
        return false;
    }
    
    // Check deadline
    if (deadline_ <= 0) {
        return false;
    }
    
    // Check creator
    if (creator_.empty()) {
        return false;
    }
    
    return true;
}

std::string Problem::ToString() const {
    std::ostringstream oss;
    oss << "Problem{id=" << id_
        << ", type=" << ProblemTypeToString(spec_.GetType())
        << ", reward=" << reward_
        << ", deadline=" << deadline_
        << ", solved=" << (solved_ ? "true" : "false")
        << "}";
    return oss.str();
}

// ============================================================================
// ProblemFactory Implementation
// ============================================================================

ProblemFactory& ProblemFactory::Instance() {
    static ProblemFactory instance;
    return instance;
}

Problem ProblemFactory::CreateMLTrainingProblem(
    const std::vector<uint8_t>& modelData,
    const std::vector<uint8_t>& trainingData,
    const std::string& hyperparameters,
    Amount reward,
    int64_t deadline) {
    
    ProblemSpec spec(ProblemType::ML_TRAINING);
    spec.SetVersion(1);
    spec.SetDescription("ML model training task");
    spec.SetInputData(std::vector<uint8_t>(trainingData.begin(), trainingData.end()));
    spec.SetVerificationData(std::vector<uint8_t>(modelData.begin(), modelData.end()));
    spec.SetParameters(hyperparameters);
    
    Problem problem(std::move(spec));
    problem.SetId(nextId_++);
    problem.SetReward(reward);
    problem.SetDeadline(deadline);
    problem.SetCreationTime(GetTime());
    
    // Set difficulty based on data size
    ProblemDifficulty diff;
    diff.target = 0xFFFFFFFFFFFFFFFF / (trainingData.size() + 1);
    diff.estimatedTime = static_cast<uint32_t>(trainingData.size() / 1000);  // Rough estimate
    diff.minMemory = modelData.size() + trainingData.size();
    diff.operations = trainingData.size() * 100;  // Assume 100 ops per byte
    problem.SetDifficulty(diff);
    
    return problem;
}

Problem ProblemFactory::CreateHashProblem(
    const Hash256& target,
    uint32_t nonce,
    Amount reward,
    int64_t deadline) {
    
    ProblemSpec spec(ProblemType::HASH_POW);
    spec.SetVersion(1);
    spec.SetDescription("Hash-based proof of work");
    
    // Store target in input data
    std::vector<uint8_t> inputData(target.begin(), target.end());
    inputData.resize(36);  // Add nonce space
    std::memcpy(inputData.data() + 32, &nonce, sizeof(nonce));
    spec.SetInputData(std::move(inputData));
    
    Problem problem(std::move(spec));
    problem.SetId(nextId_++);
    problem.SetReward(reward);
    problem.SetDeadline(deadline);
    problem.SetCreationTime(GetTime());
    
    // Set difficulty from target
    ProblemDifficulty diff;
    // Use first 8 bytes of target as difficulty
    std::memcpy(&diff.target, target.data(), sizeof(diff.target));
    diff.estimatedTime = 60;  // 1 minute estimate
    diff.minMemory = 1024;    // 1 KB
    diff.operations = 1000000;  // 1M hashes estimate
    problem.SetDifficulty(diff);
    
    return problem;
}

Problem ProblemFactory::CreateLinearAlgebraProblem(
    const std::vector<uint8_t>& matrixData,
    const std::string& operation,
    Amount reward,
    int64_t deadline) {
    
    ProblemSpec spec(ProblemType::LINEAR_ALGEBRA);
    spec.SetVersion(1);
    spec.SetDescription("Linear algebra computation: " + operation);
    spec.SetInputData(std::vector<uint8_t>(matrixData.begin(), matrixData.end()));
    spec.SetParameters("{\"operation\": \"" + operation + "\"}");
    
    Problem problem(std::move(spec));
    problem.SetId(nextId_++);
    problem.SetReward(reward);
    problem.SetDeadline(deadline);
    problem.SetCreationTime(GetTime());
    
    // Set difficulty based on matrix size
    ProblemDifficulty diff;
    size_t n = static_cast<size_t>(std::sqrt(matrixData.size() / sizeof(double)));
    diff.target = 0xFFFFFFFFFFFFFFFF / (n * n * n + 1);  // O(n^3) for matrix mult
    diff.estimatedTime = static_cast<uint32_t>(n * n * n / 1000000);
    diff.minMemory = matrixData.size() * 3;  // Input + output + workspace
    diff.operations = n * n * n;
    problem.SetDifficulty(diff);
    
    return problem;
}

Problem ProblemFactory::CreateCustomProblem(
    ProblemSpec spec,
    ProblemDifficulty difficulty,
    Amount reward,
    int64_t deadline) {
    
    Problem problem(std::move(spec));
    problem.SetId(nextId_++);
    problem.SetDifficulty(difficulty);
    problem.SetReward(reward);
    problem.SetDeadline(deadline);
    problem.SetCreationTime(GetTime());
    
    return problem;
}

// ============================================================================
// ProblemPool Implementation
// ============================================================================

class ProblemPool::Impl {
public:
    mutable std::mutex mutex_;
    std::map<Problem::Id, Problem> problems_;
    std::map<ProblemHash, Problem::Id> hashIndex_;
    Amount totalRewards_{0};
};

ProblemPool::ProblemPool() : impl_(std::make_unique<Impl>()) {}

ProblemPool::~ProblemPool() = default;

bool ProblemPool::AddProblem(Problem problem) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    if (impl_->problems_.size() >= MAX_POOL_SIZE) {
        return false;
    }
    
    if (!problem.IsValid()) {
        return false;
    }
    
    Problem::Id id = problem.GetId();
    if (impl_->problems_.find(id) != impl_->problems_.end()) {
        return false;
    }
    
    impl_->totalRewards_ += problem.GetReward();
    impl_->hashIndex_[problem.GetHash()] = id;
    impl_->problems_.emplace(id, std::move(problem));
    
    return true;
}

bool ProblemPool::RemoveProblem(Problem::Id id) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto it = impl_->problems_.find(id);
    if (it == impl_->problems_.end()) {
        return false;
    }
    
    impl_->totalRewards_ -= it->second.GetReward();
    impl_->hashIndex_.erase(it->second.GetHash());
    impl_->problems_.erase(it);
    
    return true;
}

const Problem* ProblemPool::GetProblem(Problem::Id id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto it = impl_->problems_.find(id);
    if (it == impl_->problems_.end()) {
        return nullptr;
    }
    return &it->second;
}

const Problem* ProblemPool::GetProblemByHash(const ProblemHash& hash) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto hashIt = impl_->hashIndex_.find(hash);
    if (hashIt == impl_->hashIndex_.end()) {
        return nullptr;
    }
    
    auto it = impl_->problems_.find(hashIt->second);
    if (it == impl_->problems_.end()) {
        return nullptr;
    }
    return &it->second;
}

bool ProblemPool::HasProblem(Problem::Id id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->problems_.find(id) != impl_->problems_.end();
}

std::vector<const Problem*> ProblemPool::GetProblemsForMining(
    size_t maxCount,
    Amount minReward) const {
    
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    std::vector<const Problem*> result;
    result.reserve(std::min(maxCount, impl_->problems_.size()));
    
    // Collect eligible problems
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
    for (size_t i = 0; i < std::min(maxCount, eligible.size()); ++i) {
        result.push_back(eligible[i].second);
    }
    
    return result;
}

const Problem* ProblemPool::GetHighestRewardProblem() const {
    auto problems = GetProblemsForMining(1, 0);
    return problems.empty() ? nullptr : problems[0];
}

std::vector<const Problem*> ProblemPool::GetProblemsByType(ProblemType type) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    std::vector<const Problem*> result;
    for (const auto& [id, problem] : impl_->problems_) {
        if (problem.GetType() == type && !problem.IsSolved() && !problem.IsExpired()) {
            result.push_back(&problem);
        }
    }
    
    return result;
}

size_t ProblemPool::RemoveExpired() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    int64_t now = GetTime();
    size_t removed = 0;
    
    auto it = impl_->problems_.begin();
    while (it != impl_->problems_.end()) {
        if (it->second.IsExpiredAt(now)) {
            impl_->totalRewards_ -= it->second.GetReward();
            impl_->hashIndex_.erase(it->second.GetHash());
            it = impl_->problems_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    
    return removed;
}

void ProblemPool::MarkSolved(Problem::Id id, const std::string& solver) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto it = impl_->problems_.find(id);
    if (it != impl_->problems_.end()) {
        it->second.SetSolved(true);
        it->second.SetSolver(solver);
    }
}

void ProblemPool::Clear() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->problems_.clear();
    impl_->hashIndex_.clear();
    impl_->totalRewards_ = 0;
}

size_t ProblemPool::Size() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->problems_.size();
}

bool ProblemPool::IsEmpty() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->problems_.empty();
}

Amount ProblemPool::GetTotalRewards() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->totalRewards_;
}

} // namespace marketplace
} // namespace shurium
