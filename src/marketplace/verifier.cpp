// SHURIUM - Proof of Useful Work Verifier Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/marketplace/verifier.h>
#include <shurium/crypto/sha256.h>
#include <shurium/util/time.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

namespace shurium {
namespace marketplace {

// ============================================================================
// Verification Result String Conversion
// ============================================================================

const char* VerificationResultToString(VerificationResult result) {
    switch (result) {
        case VerificationResult::VALID: return "valid";
        case VerificationResult::INVALID: return "invalid";
        case VerificationResult::PROBLEM_NOT_FOUND: return "problem_not_found";
        case VerificationResult::MALFORMED: return "malformed";
        case VerificationResult::TYPE_MISMATCH: return "type_mismatch";
        case VerificationResult::TIMEOUT: return "timeout";
        case VerificationResult::ERROR: return "error";
        default: return "unknown";
    }
}

// ============================================================================
// VerificationDetails Implementation
// ============================================================================

std::string VerificationDetails::ToString() const {
    std::ostringstream oss;
    oss << "VerificationDetails{result=" << VerificationResultToString(result)
        << ", score=" << score
        << ", time=" << verificationTimeMs << "ms";
    
    if (!errorMessage.empty()) {
        oss << ", error=\"" << errorMessage << "\"";
    }
    
    if (!checks.empty()) {
        oss << ", checks=[";
        for (size_t i = 0; i < checks.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << checks[i].first << ":" << (checks[i].second ? "pass" : "fail");
        }
        oss << "]";
    }
    
    oss << "}";
    return oss.str();
}

// ============================================================================
// HashPowVerifier Implementation
// ============================================================================

class HashPowVerifier::Impl {
public:
    // No state needed for hash verification
};

HashPowVerifier::HashPowVerifier() : impl_(std::make_unique<Impl>()) {}

HashPowVerifier::~HashPowVerifier() = default;

VerificationDetails HashPowVerifier::Verify(
    const Problem& problem,
    const Solution& solution) {
    
    VerificationDetails details;
    auto startTime = std::chrono::steady_clock::now();
    
    // Quick validation first
    if (!QuickValidate(problem, solution)) {
        details.result = VerificationResult::MALFORMED;
        details.errorMessage = "Quick validation failed";
        return details;
    }
    
    // Get target from problem
    const auto& inputData = problem.GetSpec().GetInputData();
    if (inputData.size() < 32) {
        details.result = VerificationResult::MALFORMED;
        details.errorMessage = "Problem input data too small";
        return details;
    }
    
    Hash256 target;
    std::copy(inputData.begin(), inputData.begin() + 32, target.begin());
    
    // Get solution result hash
    const Hash256& resultHash = solution.GetData().GetResultHash();
    
    // Check if result hash is below target
    details.AddCheck("hash_below_target", resultHash < target);
    
    // Verify the nonce produces the claimed hash
    const auto& result = solution.GetData().GetResult();
    Hash256 computedHash;
    SHA256 hasher;
    hasher.Write(result.data(), result.size());
    hasher.Finalize(computedHash.data());
    
    details.AddCheck("hash_valid", computedHash == resultHash);
    
    // Calculate score (inverse of hash value - lower hash = higher score)
    uint64_t hashValue = 0;
    std::memcpy(&hashValue, resultHash.data(), sizeof(hashValue));
    uint64_t targetValue = 0;
    std::memcpy(&targetValue, target.data(), sizeof(targetValue));
    
    if (targetValue > 0) {
        details.score = static_cast<uint32_t>(
            (static_cast<double>(targetValue - hashValue) / targetValue) * 1000000);
    }
    
    // Check all checks passed
    bool allPassed = true;
    for (const auto& check : details.checks) {
        if (!check.second) {
            allPassed = false;
            break;
        }
    }
    
    details.result = allPassed ? VerificationResult::VALID : VerificationResult::INVALID;
    details.meetsRequirements = allPassed;
    
    auto endTime = std::chrono::steady_clock::now();
    details.verificationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    return details;
}

bool HashPowVerifier::QuickValidate(
    const Problem& problem,
    const Solution& solution) {
    
    // Check problem type
    if (problem.GetType() != ProblemType::HASH_POW) {
        return false;
    }
    
    // Check solution has result
    if (solution.GetData().GetResult().empty()) {
        return false;
    }
    
    // Check problem reference matches
    if (solution.GetProblemId() != problem.GetId()) {
        return false;
    }
    
    return true;
}

uint64_t HashPowVerifier::EstimateVerificationTime(
    const Problem& /* problem */) const {
    // Hash verification is very fast
    return 10;  // 10ms
}

// ============================================================================
// MLTrainingVerifier Implementation
// ============================================================================

class MLTrainingVerifier::Impl {
public:
    // Placeholder for ML verification infrastructure
};

MLTrainingVerifier::MLTrainingVerifier() : impl_(std::make_unique<Impl>()) {}

MLTrainingVerifier::~MLTrainingVerifier() = default;

VerificationDetails MLTrainingVerifier::Verify(
    const Problem& problem,
    const Solution& solution) {
    
    VerificationDetails details;
    auto startTime = std::chrono::steady_clock::now();
    
    // Quick validation first
    if (!QuickValidate(problem, solution)) {
        details.result = VerificationResult::MALFORMED;
        details.errorMessage = "Quick validation failed";
        return details;
    }
    
    // For now, implement a simplified verification
    // In production, this would run actual ML inference
    
    // Check 1: Solution has valid structure
    details.AddCheck("valid_structure", solution.IsValid());
    
    // Check 2: Accuracy meets threshold
    uint32_t accuracy = solution.GetData().GetAccuracy();
    details.AddCheck("accuracy_threshold", accuracy >= minAccuracy_);
    
    // Check 3: Iterations are reasonable
    uint64_t iters = solution.GetData().GetIterations();
    details.AddCheck("iterations_valid", iters > 0 && iters < 1000000000);
    
    // Check 4: Result size is reasonable
    size_t resultSize = solution.GetData().GetResult().size();
    const auto& inputSize = problem.GetSpec().GetInputData().size();
    details.AddCheck("result_size_valid", resultSize > 0 && resultSize <= inputSize * 10);
    
    // Score is the accuracy
    details.score = accuracy;
    
    // Check all checks passed
    bool allPassed = true;
    for (const auto& check : details.checks) {
        if (!check.second) {
            allPassed = false;
            break;
        }
    }
    
    details.result = allPassed ? VerificationResult::VALID : VerificationResult::INVALID;
    details.meetsRequirements = allPassed;
    
    auto endTime = std::chrono::steady_clock::now();
    details.verificationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    return details;
}

bool MLTrainingVerifier::QuickValidate(
    const Problem& problem,
    const Solution& solution) {
    
    // Check problem type
    if (problem.GetType() != ProblemType::ML_TRAINING) {
        return false;
    }
    
    // Check solution has result
    if (solution.GetData().GetResult().empty()) {
        return false;
    }
    
    // Check problem reference matches
    if (solution.GetProblemId() != problem.GetId()) {
        return false;
    }
    
    return true;
}

uint64_t MLTrainingVerifier::EstimateVerificationTime(
    const Problem& problem) const {
    // Estimate based on data size
    size_t dataSize = problem.GetSpec().GetInputData().size();
    return std::min(maxVerificationTime_, static_cast<uint64_t>(dataSize / 100));
}

// ============================================================================
// LinearAlgebraVerifier Implementation
// ============================================================================

class LinearAlgebraVerifier::Impl {
public:
    // Placeholder for linear algebra verification
};

LinearAlgebraVerifier::LinearAlgebraVerifier() : impl_(std::make_unique<Impl>()) {}

LinearAlgebraVerifier::~LinearAlgebraVerifier() = default;

VerificationDetails LinearAlgebraVerifier::Verify(
    const Problem& problem,
    const Solution& solution) {
    
    VerificationDetails details;
    auto startTime = std::chrono::steady_clock::now();
    
    // Quick validation first
    if (!QuickValidate(problem, solution)) {
        details.result = VerificationResult::MALFORMED;
        details.errorMessage = "Quick validation failed";
        return details;
    }
    
    // Simplified verification
    // In production, this would verify matrix operations
    
    // Check 1: Valid structure
    details.AddCheck("valid_structure", solution.IsValid());
    
    // Check 2: Result has correct size
    const auto& result = solution.GetData().GetResult();
    const auto& input = problem.GetSpec().GetInputData();
    
    // For matrix multiplication, output size should match input size
    details.AddCheck("result_size_valid", 
        result.size() > 0 && result.size() <= input.size() * 2);
    
    // Check 3: Intermediate values provided
    const auto& intermediates = solution.GetData().GetIntermediates();
    details.AddCheck("has_intermediates", !intermediates.empty());
    
    // Score based on result size match
    if (result.size() > 0 && input.size() > 0) {
        double ratio = static_cast<double>(result.size()) / input.size();
        if (ratio >= 0.5 && ratio <= 2.0) {
            details.score = 900000;  // 90% score for reasonable size
        } else {
            details.score = 500000;  // 50% score otherwise
        }
    }
    
    // Check all checks passed
    bool allPassed = true;
    for (const auto& check : details.checks) {
        if (!check.second) {
            allPassed = false;
            break;
        }
    }
    
    details.result = allPassed ? VerificationResult::VALID : VerificationResult::INVALID;
    details.meetsRequirements = allPassed;
    
    auto endTime = std::chrono::steady_clock::now();
    details.verificationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    return details;
}

bool LinearAlgebraVerifier::QuickValidate(
    const Problem& problem,
    const Solution& solution) {
    
    // Check problem type
    if (problem.GetType() != ProblemType::LINEAR_ALGEBRA) {
        return false;
    }
    
    // Check solution has result
    if (solution.GetData().GetResult().empty()) {
        return false;
    }
    
    // Check problem reference matches
    if (solution.GetProblemId() != problem.GetId()) {
        return false;
    }
    
    return true;
}

uint64_t LinearAlgebraVerifier::EstimateVerificationTime(
    const Problem& problem) const {
    // O(n^2) for verification vs O(n^3) for computation
    size_t dataSize = problem.GetSpec().GetInputData().size();
    size_t n = static_cast<size_t>(std::sqrt(dataSize / sizeof(double)));
    return n * n / 1000;  // Rough estimate in ms
}

// ============================================================================
// VerifierRegistry Implementation
// ============================================================================

class VerifierRegistry::Impl {
public:
    std::mutex mutex_;
    std::map<ProblemType, std::unique_ptr<IVerifier>> verifiers_;
};

VerifierRegistry::VerifierRegistry() : impl_(std::make_unique<Impl>()) {
    // Register default verifiers
    Register(std::make_unique<HashPowVerifier>());
    Register(std::make_unique<MLTrainingVerifier>());
    Register(std::make_unique<LinearAlgebraVerifier>());
}

VerifierRegistry::~VerifierRegistry() = default;

VerifierRegistry& VerifierRegistry::Instance() {
    static VerifierRegistry instance;
    return instance;
}

void VerifierRegistry::Register(std::unique_ptr<IVerifier> verifier) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->verifiers_[verifier->GetType()] = std::move(verifier);
}

IVerifier* VerifierRegistry::GetVerifier(ProblemType type) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->verifiers_.find(type);
    if (it == impl_->verifiers_.end()) {
        return nullptr;
    }
    return it->second.get();
}

bool VerifierRegistry::HasVerifier(ProblemType type) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->verifiers_.find(type) != impl_->verifiers_.end();
}

std::vector<ProblemType> VerifierRegistry::GetRegisteredTypes() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    std::vector<ProblemType> types;
    types.reserve(impl_->verifiers_.size());
    for (const auto& [type, _] : impl_->verifiers_) {
        types.push_back(type);
    }
    return types;
}

// ============================================================================
// SolutionVerifier Implementation
// ============================================================================

class SolutionVerifier::Impl {
public:
    std::atomic<uint64_t> totalVerifications_{0};
    std::atomic<uint64_t> successfulCount_{0};
    std::atomic<uint64_t> failedCount_{0};
    std::atomic<uint64_t> totalVerificationTime_{0};
    
    mutable std::mutex mutex_;
    std::queue<std::tuple<Problem, Solution, SolutionVerifier::VerificationCallback>> pendingQueue_;
    std::map<Solution::Id, VerificationDetails> results_;
};

SolutionVerifier::SolutionVerifier() : impl_(std::make_unique<Impl>()) {}

SolutionVerifier::~SolutionVerifier() = default;

VerificationDetails SolutionVerifier::Verify(
    const Problem& problem,
    const Solution& solution) {
    
    VerificationDetails details;
    
    // Get appropriate verifier
    IVerifier* verifier = VerifierRegistry::Instance().GetVerifier(problem.GetType());
    if (!verifier) {
        details.result = VerificationResult::TYPE_MISMATCH;
        details.errorMessage = "No verifier for problem type: " + 
            std::string(ProblemTypeToString(problem.GetType()));
        return details;
    }
    
    // Verify
    auto startTime = std::chrono::steady_clock::now();
    details = verifier->Verify(problem, solution);
    auto endTime = std::chrono::steady_clock::now();
    
    // Update statistics
    impl_->totalVerifications_++;
    impl_->totalVerificationTime_ += details.verificationTimeMs;
    
    if (details.result == VerificationResult::VALID) {
        impl_->successfulCount_++;
    } else {
        impl_->failedCount_++;
    }
    
    return details;
}

bool SolutionVerifier::QuickValidate(
    const Problem& problem,
    const Solution& solution) {
    
    IVerifier* verifier = VerifierRegistry::Instance().GetVerifier(problem.GetType());
    if (!verifier) {
        return false;
    }
    
    return verifier->QuickValidate(problem, solution);
}

bool SolutionVerifier::SubmitForVerification(
    const Problem& problem,
    Solution solution,
    VerificationCallback callback) {
    
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    if (impl_->pendingQueue_.size() >= maxConcurrent_) {
        return false;
    }
    
    impl_->pendingQueue_.emplace(problem, std::move(solution), std::move(callback));
    return true;
}

size_t SolutionVerifier::GetPendingCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->pendingQueue_.size();
}

bool SolutionVerifier::CancelVerification(Solution::Id solutionId) {
    // For simplicity, we don't support cancellation in this implementation
    (void)solutionId;
    return false;
}

uint64_t SolutionVerifier::GetTotalVerifications() const {
    return impl_->totalVerifications_.load();
}

uint64_t SolutionVerifier::GetSuccessfulCount() const {
    return impl_->successfulCount_.load();
}

uint64_t SolutionVerifier::GetFailedCount() const {
    return impl_->failedCount_.load();
}

uint64_t SolutionVerifier::GetAverageVerificationTime() const {
    uint64_t total = impl_->totalVerifications_.load();
    if (total == 0) return 0;
    return impl_->totalVerificationTime_.load() / total;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool VerifyHashTarget(const Hash256& hash, uint64_t target) {
    // Compare first 8 bytes of hash against target
    uint64_t hashValue = 0;
    std::memcpy(&hashValue, hash.data(), sizeof(hashValue));
    return hashValue < target;
}

bool VerifyDataIntegrity(const SolutionData& data) {
    return data.IsValid();
}

Hash256 ComputeVerificationHash(
    const Problem& problem,
    const SolutionData& data) {
    
    DataStream stream;
    ::shurium::Serialize(stream, problem.GetHash());
    ::shurium::Serialize(stream, data.GetResultHash());
    
    Hash256 hash;
    SHA256 hasher;
    hasher.Write(stream.data(), stream.size());
    hasher.Finalize(hash.data());
    
    return hash;
}

} // namespace marketplace
} // namespace shurium
