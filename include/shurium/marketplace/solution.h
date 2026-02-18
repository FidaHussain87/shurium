// SHURIUM - Proof of Useful Work Solution
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Defines solutions to computational problems for the PoUW system.
// Solutions contain the computed result along with proof of work.

#ifndef SHURIUM_MARKETPLACE_SOLUTION_H
#define SHURIUM_MARKETPLACE_SOLUTION_H

#include <shurium/core/types.h>
#include <shurium/core/serialize.h>
#include <shurium/marketplace/problem.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace marketplace {

// ============================================================================
// Solution Status
// ============================================================================

/// Status of a solution
enum class SolutionStatus : uint8_t {
    /// Solution is pending verification
    PENDING = 0,
    
    /// Solution is being verified
    VERIFYING = 1,
    
    /// Solution is valid and accepted
    ACCEPTED = 2,
    
    /// Solution is invalid
    REJECTED = 3,
    
    /// Solution expired before verification
    EXPIRED = 4
};

/// Get string representation of solution status
const char* SolutionStatusToString(SolutionStatus status);

// ============================================================================
// Solution Data
// ============================================================================

/**
 * Data produced by solving a problem.
 */
class SolutionData {
public:
    /// Maximum result size (10 MB)
    static constexpr size_t MAX_RESULT_SIZE = 10 * 1024 * 1024;
    
    /// Default constructor
    SolutionData() = default;
    
    // ========================================================================
    // Result Data
    // ========================================================================
    
    /// Get result data
    const std::vector<uint8_t>& GetResult() const { return result_; }
    void SetResult(std::vector<uint8_t> result);
    
    /// Get result hash
    const Hash256& GetResultHash() const { return resultHash_; }
    
    /// Compute result hash
    void ComputeResultHash();
    
    // ========================================================================
    // Proof Data
    // ========================================================================
    
    /// Get proof of work data
    const std::vector<uint8_t>& GetProof() const { return proof_; }
    void SetProof(std::vector<uint8_t> proof) { proof_ = std::move(proof); }
    
    /// Get intermediate values (for verification)
    const std::vector<Hash256>& GetIntermediates() const { return intermediates_; }
    void AddIntermediate(const Hash256& hash) { intermediates_.push_back(hash); }
    void ClearIntermediates() { intermediates_.clear(); }
    
    // ========================================================================
    // Metadata
    // ========================================================================
    
    /// Get computation time (milliseconds)
    uint64_t GetComputeTime() const { return computeTime_; }
    void SetComputeTime(uint64_t ms) { computeTime_ = ms; }
    
    /// Get iterations performed
    uint64_t GetIterations() const { return iterations_; }
    void SetIterations(uint64_t iters) { iterations_ = iters; }
    
    /// Get accuracy metric (problem-specific, 0-1000000)
    uint32_t GetAccuracy() const { return accuracy_; }
    void SetAccuracy(uint32_t acc) { accuracy_ = acc; }
    
    // ========================================================================
    // Validation
    // ========================================================================
    
    /// Check if solution data is valid
    bool IsValid() const;
    
    // ========================================================================
    // Serialization
    // ========================================================================
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, result_);
        ::shurium::Serialize(s, resultHash_);
        ::shurium::Serialize(s, proof_);
        ::shurium::Serialize(s, intermediates_);
        ::shurium::Serialize(s, computeTime_);
        ::shurium::Serialize(s, iterations_);
        ::shurium::Serialize(s, accuracy_);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, result_);
        ::shurium::Unserialize(s, resultHash_);
        ::shurium::Unserialize(s, proof_);
        ::shurium::Unserialize(s, intermediates_);
        ::shurium::Unserialize(s, computeTime_);
        ::shurium::Unserialize(s, iterations_);
        ::shurium::Unserialize(s, accuracy_);
    }

private:
    std::vector<uint8_t> result_;
    Hash256 resultHash_;
    std::vector<uint8_t> proof_;
    std::vector<Hash256> intermediates_;
    uint64_t computeTime_{0};
    uint64_t iterations_{0};
    uint32_t accuracy_{0};
};

/// Free function serializers for SolutionData
template<typename Stream>
void Serialize(Stream& s, const SolutionData& data) {
    data.Serialize(s);
}

template<typename Stream>
void Unserialize(Stream& s, SolutionData& data) {
    data.Unserialize(s);
}

// ============================================================================
// Solution
// ============================================================================

/**
 * A solution to a computational problem.
 * 
 * Contains the result data, proof of work, and metadata about
 * who solved it and when.
 */
class Solution {
public:
    /// Solution ID type
    using Id = uint64_t;
    
    /// Invalid solution ID
    static constexpr Id INVALID_ID = 0;
    
    /// Default constructor
    Solution();
    
    /// Create for a specific problem
    explicit Solution(Problem::Id problemId);
    
    /// Destructor
    ~Solution();
    
    // ========================================================================
    // Identification
    // ========================================================================
    
    /// Get unique solution ID
    Id GetId() const { return id_; }
    void SetId(Id id) { id_ = id; }
    
    /// Get solution hash
    const Hash256& GetHash() const { return hash_; }
    
    /// Compute solution hash
    void ComputeHash();
    
    // ========================================================================
    // Problem Reference
    // ========================================================================
    
    /// Get problem ID this solves
    Problem::Id GetProblemId() const { return problemId_; }
    void SetProblemId(Problem::Id id) { problemId_ = id; }
    
    /// Get problem hash
    const ProblemHash& GetProblemHash() const { return problemHash_; }
    void SetProblemHash(const ProblemHash& hash) { problemHash_ = hash; }
    
    // ========================================================================
    // Solution Data
    // ========================================================================
    
    /// Get solution data
    const SolutionData& GetData() const { return data_; }
    SolutionData& GetData() { return data_; }
    void SetData(SolutionData data) { data_ = std::move(data); }
    
    // ========================================================================
    // Solver Information
    // ========================================================================
    
    /// Get solver address
    const std::string& GetSolver() const { return solver_; }
    void SetSolver(const std::string& addr) { solver_ = addr; }
    
    /// Get solver's nonce
    uint64_t GetNonce() const { return nonce_; }
    void SetNonce(uint64_t n) { nonce_ = n; }
    
    // ========================================================================
    // Timing
    // ========================================================================
    
    /// Get submission time
    int64_t GetSubmissionTime() const { return submissionTime_; }
    void SetSubmissionTime(int64_t time) { submissionTime_ = time; }
    
    /// Get verification time
    int64_t GetVerificationTime() const { return verificationTime_; }
    void SetVerificationTime(int64_t time) { verificationTime_ = time; }
    
    // ========================================================================
    // Status
    // ========================================================================
    
    /// Get solution status
    SolutionStatus GetStatus() const { return status_; }
    void SetStatus(SolutionStatus status) { status_ = status; }
    
    /// Check if solution is accepted
    bool IsAccepted() const { return status_ == SolutionStatus::ACCEPTED; }
    
    /// Check if solution is rejected
    bool IsRejected() const { return status_ == SolutionStatus::REJECTED; }
    
    /// Check if solution is pending
    bool IsPending() const { return status_ == SolutionStatus::PENDING; }
    
    // ========================================================================
    // Rewards
    // ========================================================================
    
    /// Get reward received
    Amount GetReward() const { return reward_; }
    void SetReward(Amount amount) { reward_ = amount; }
    
    // ========================================================================
    // Validation
    // ========================================================================
    
    /// Check if solution is structurally valid
    bool IsValid() const;
    
    /// Convert to string representation
    std::string ToString() const;
    
    // ========================================================================
    // Serialization
    // ========================================================================
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, id_);
        ::shurium::Serialize(s, hash_);
        ::shurium::Serialize(s, problemId_);
        ::shurium::Serialize(s, problemHash_);
        ::shurium::Serialize(s, data_);
        ::shurium::Serialize(s, solver_);
        ::shurium::Serialize(s, nonce_);
        ::shurium::Serialize(s, submissionTime_);
        ::shurium::Serialize(s, verificationTime_);
        ::shurium::Serialize(s, static_cast<uint8_t>(status_));
        ::shurium::Serialize(s, reward_);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, id_);
        ::shurium::Unserialize(s, hash_);
        ::shurium::Unserialize(s, problemId_);
        ::shurium::Unserialize(s, problemHash_);
        ::shurium::Unserialize(s, data_);
        ::shurium::Unserialize(s, solver_);
        ::shurium::Unserialize(s, nonce_);
        ::shurium::Unserialize(s, submissionTime_);
        ::shurium::Unserialize(s, verificationTime_);
        uint8_t status;
        ::shurium::Unserialize(s, status);
        status_ = static_cast<SolutionStatus>(status);
        ::shurium::Unserialize(s, reward_);
    }

private:
    Id id_{INVALID_ID};
    Hash256 hash_;
    Problem::Id problemId_{Problem::INVALID_ID};
    ProblemHash problemHash_;
    SolutionData data_;
    std::string solver_;
    uint64_t nonce_{0};
    int64_t submissionTime_{0};
    int64_t verificationTime_{0};
    SolutionStatus status_{SolutionStatus::PENDING};
    Amount reward_{0};
};

/// Free function serializers for Solution
template<typename Stream>
void Serialize(Stream& s, const Solution& sol) {
    sol.Serialize(s);
}

template<typename Stream>
void Unserialize(Stream& s, Solution& sol) {
    sol.Unserialize(s);
}

// ============================================================================
// Solution Builder
// ============================================================================

/**
 * Builder for constructing solutions.
 */
class SolutionBuilder {
public:
    /// Create a builder for a problem
    explicit SolutionBuilder(const Problem& problem);
    
    /// Set solver address
    SolutionBuilder& SetSolver(const std::string& addr);
    
    /// Set nonce
    SolutionBuilder& SetNonce(uint64_t nonce);
    
    /// Set result data
    SolutionBuilder& SetResult(std::vector<uint8_t> result);
    
    /// Set proof data
    SolutionBuilder& SetProof(std::vector<uint8_t> proof);
    
    /// Add intermediate hash
    SolutionBuilder& AddIntermediate(const Hash256& hash);
    
    /// Set computation time
    SolutionBuilder& SetComputeTime(uint64_t ms);
    
    /// Set iterations
    SolutionBuilder& SetIterations(uint64_t iters);
    
    /// Set accuracy metric
    SolutionBuilder& SetAccuracy(uint32_t accuracy);
    
    /// Build the solution
    Solution Build();
    
    /// Build and compute hash
    Solution BuildWithHash();

private:
    Problem::Id problemId_;
    ProblemHash problemHash_;
    std::string solver_;
    uint64_t nonce_{0};
    SolutionData data_;
};

// ============================================================================
// Solution Cache
// ============================================================================

/**
 * Cache for recent solutions.
 */
class SolutionCache {
public:
    /// Maximum cache size
    static constexpr size_t MAX_CACHE_SIZE = 1000;
    
    /// Create a solution cache
    explicit SolutionCache(size_t maxSize = MAX_CACHE_SIZE);
    
    /// Destructor
    ~SolutionCache();
    
    /// Add a solution to the cache
    void Add(Solution solution);
    
    /// Get a solution by ID
    const Solution* Get(Solution::Id id) const;
    
    /// Get a solution by hash
    const Solution* GetByHash(const Hash256& hash) const;
    
    /// Check if solution exists
    bool Has(Solution::Id id) const;
    
    /// Remove a solution
    void Remove(Solution::Id id);
    
    /// Get solutions for a problem
    std::vector<const Solution*> GetForProblem(Problem::Id problemId) const;
    
    /// Clear the cache
    void Clear();
    
    /// Get cache size
    size_t Size() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace marketplace
} // namespace shurium

#endif // SHURIUM_MARKETPLACE_SOLUTION_H
