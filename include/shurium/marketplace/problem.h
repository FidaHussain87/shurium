// SHURIUM - Proof of Useful Work Problem Definition
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Defines computational problems for the Proof of Useful Work system.
// Problems can be ML training tasks, scientific computations, or other
// verifiable computational work that provides real-world value.

#ifndef SHURIUM_MARKETPLACE_PROBLEM_H
#define SHURIUM_MARKETPLACE_PROBLEM_H

#include <shurium/core/types.h>
#include <shurium/core/serialize.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace marketplace {

// ============================================================================
// Problem Types
// ============================================================================

/// Types of computational problems supported
enum class ProblemType : uint32_t {
    /// Unknown/invalid type
    UNKNOWN = 0,
    
    /// Machine learning model training
    ML_TRAINING = 1,
    
    /// Neural network inference benchmark
    ML_INFERENCE = 2,
    
    /// Matrix multiplication/linear algebra
    LINEAR_ALGEBRA = 3,
    
    /// Hash-based proof of work (fallback)
    HASH_POW = 4,
    
    /// Scientific simulation
    SIMULATION = 5,
    
    /// Data processing/ETL
    DATA_PROCESSING = 6,
    
    /// Optimization problem
    OPTIMIZATION = 7,
    
    /// Cryptographic computation
    CRYPTOGRAPHIC = 8,
    
    /// Custom verifiable computation
    CUSTOM = 255
};

/// Get string representation of problem type
const char* ProblemTypeToString(ProblemType type);

/// Parse problem type from string
std::optional<ProblemType> ProblemTypeFromString(const std::string& str);

// ============================================================================
// Problem Difficulty
// ============================================================================

/// Difficulty parameters for a problem
struct ProblemDifficulty {
    /// Target difficulty (lower = harder)
    uint64_t target{0};
    
    /// Estimated compute time (seconds)
    uint32_t estimatedTime{0};
    
    /// Minimum required memory (bytes)
    uint64_t minMemory{0};
    
    /// Number of operations required
    uint64_t operations{0};
    
    /// Difficulty adjustment factor (scaled by 1e6)
    uint32_t adjustmentFactor{1000000};
    
    /// Default constructor
    ProblemDifficulty() = default;
    
    /// Create with target difficulty
    explicit ProblemDifficulty(uint64_t t) : target(t) {}
    
    /// Check if valid
    bool IsValid() const { return target > 0; }
    
    /// Compare difficulty
    bool operator<(const ProblemDifficulty& other) const {
        return target > other.target; // Lower target = harder
    }
};

/// Serialization for ProblemDifficulty
template<typename Stream>
void Serialize(Stream& s, const ProblemDifficulty& diff) {
    ::shurium::Serialize(s, diff.target);
    ::shurium::Serialize(s, diff.estimatedTime);
    ::shurium::Serialize(s, diff.minMemory);
    ::shurium::Serialize(s, diff.operations);
    ::shurium::Serialize(s, diff.adjustmentFactor);
}

template<typename Stream>
void Unserialize(Stream& s, ProblemDifficulty& diff) {
    ::shurium::Unserialize(s, diff.target);
    ::shurium::Unserialize(s, diff.estimatedTime);
    ::shurium::Unserialize(s, diff.minMemory);
    ::shurium::Unserialize(s, diff.operations);
    ::shurium::Unserialize(s, diff.adjustmentFactor);
}

// ============================================================================
// Problem Specification
// ============================================================================

/**
 * Specification for a computational problem.
 * 
 * Contains all the parameters needed to define and verify a problem.
 */
class ProblemSpec {
public:
    /// Maximum data size (1 MB)
    static constexpr size_t MAX_DATA_SIZE = 1024 * 1024;
    
    /// Maximum description length
    static constexpr size_t MAX_DESCRIPTION_LENGTH = 4096;
    
    /// Default constructor
    ProblemSpec() = default;
    
    /// Create with type
    explicit ProblemSpec(ProblemType type) : type_(type) {}
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    /// Get problem type
    ProblemType GetType() const { return type_; }
    void SetType(ProblemType type) { type_ = type; }
    
    /// Get problem version
    uint32_t GetVersion() const { return version_; }
    void SetVersion(uint32_t v) { version_ = v; }
    
    /// Get problem description
    const std::string& GetDescription() const { return description_; }
    void SetDescription(const std::string& desc);
    
    /// Get input data
    const std::vector<uint8_t>& GetInputData() const { return inputData_; }
    void SetInputData(std::vector<uint8_t> data);
    
    /// Get verification data (for checking solutions)
    const std::vector<uint8_t>& GetVerificationData() const { return verificationData_; }
    void SetVerificationData(std::vector<uint8_t> data);
    
    /// Get parameters as JSON string
    const std::string& GetParameters() const { return parameters_; }
    void SetParameters(const std::string& params) { parameters_ = params; }
    
    // ========================================================================
    // Validation
    // ========================================================================
    
    /// Check if specification is valid
    bool IsValid() const;
    
    /// Compute hash of the problem spec
    Hash256 GetHash() const;
    
    // ========================================================================
    // Serialization
    // ========================================================================
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, static_cast<uint32_t>(type_));
        ::shurium::Serialize(s, version_);
        ::shurium::Serialize(s, description_);
        ::shurium::Serialize(s, inputData_);
        ::shurium::Serialize(s, verificationData_);
        ::shurium::Serialize(s, parameters_);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        uint32_t type;
        ::shurium::Unserialize(s, type);
        type_ = static_cast<ProblemType>(type);
        ::shurium::Unserialize(s, version_);
        ::shurium::Unserialize(s, description_);
        ::shurium::Unserialize(s, inputData_);
        ::shurium::Unserialize(s, verificationData_);
        ::shurium::Unserialize(s, parameters_);
    }

private:
    ProblemType type_{ProblemType::UNKNOWN};
    uint32_t version_{1};
    std::string description_;
    std::vector<uint8_t> inputData_;
    std::vector<uint8_t> verificationData_;
    std::string parameters_;  // JSON parameters
};

/// Free function serializers for ProblemSpec
template<typename Stream>
void Serialize(Stream& s, const ProblemSpec& spec) {
    spec.Serialize(s);
}

template<typename Stream>
void Unserialize(Stream& s, ProblemSpec& spec) {
    spec.Unserialize(s);
}

// ============================================================================
// Problem
// ============================================================================

/**
 * A computational problem in the PoUW marketplace.
 * 
 * Problems are created by users who need computation done, and are
 * solved by miners who provide the computational work.
 */
class Problem {
public:
    /// Problem ID type
    using Id = uint64_t;
    
    /// Invalid problem ID
    static constexpr Id INVALID_ID = 0;
    
    /// Default constructor
    Problem();
    
    /// Create with specification
    explicit Problem(ProblemSpec spec);
    
    /// Destructor
    ~Problem();
    
    // ========================================================================
    // Identification
    // ========================================================================
    
    /// Get unique problem ID
    Id GetId() const { return id_; }
    void SetId(Id id) { id_ = id; }
    
    /// Get problem hash (immutable identifier)
    const ProblemHash& GetHash() const { return hash_; }
    
    /// Compute and set hash from specification
    void ComputeHash();
    
    // ========================================================================
    // Specification
    // ========================================================================
    
    /// Get problem specification
    const ProblemSpec& GetSpec() const { return spec_; }
    ProblemSpec& GetSpec() { return spec_; }
    void SetSpec(ProblemSpec spec);
    
    /// Get problem type
    ProblemType GetType() const { return spec_.GetType(); }
    
    // ========================================================================
    // Difficulty
    // ========================================================================
    
    /// Get difficulty parameters
    const ProblemDifficulty& GetDifficulty() const { return difficulty_; }
    void SetDifficulty(ProblemDifficulty diff) { difficulty_ = diff; }
    
    // ========================================================================
    // Rewards
    // ========================================================================
    
    /// Get reward for solving (in base units)
    Amount GetReward() const { return reward_; }
    void SetReward(Amount reward) { reward_ = reward; }
    
    /// Get bonus reward (for early submission)
    Amount GetBonusReward() const { return bonusReward_; }
    void SetBonusReward(Amount bonus) { bonusReward_ = bonus; }
    
    // ========================================================================
    // Timing
    // ========================================================================
    
    /// Get creation time
    int64_t GetCreationTime() const { return creationTime_; }
    void SetCreationTime(int64_t time) { creationTime_ = time; }
    
    /// Get deadline (Unix timestamp)
    int64_t GetDeadline() const { return deadline_; }
    void SetDeadline(int64_t time) { deadline_ = time; }
    
    /// Check if problem has expired
    bool IsExpired() const;
    
    /// Check if problem has expired at given time
    bool IsExpiredAt(int64_t time) const;
    
    // ========================================================================
    // Status
    // ========================================================================
    
    /// Check if problem is solved
    bool IsSolved() const { return solved_; }
    void SetSolved(bool solved) { solved_ = solved; }
    
    /// Get solver address (if solved)
    const std::string& GetSolver() const { return solver_; }
    void SetSolver(const std::string& addr) { solver_ = addr; }
    
    // ========================================================================
    // Creator
    // ========================================================================
    
    /// Get creator address
    const std::string& GetCreator() const { return creator_; }
    void SetCreator(const std::string& addr) { creator_ = addr; }
    
    // ========================================================================
    // Validation
    // ========================================================================
    
    /// Check if problem is valid
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
        ::shurium::Serialize(s, spec_);
        ::shurium::Serialize(s, difficulty_);
        ::shurium::Serialize(s, reward_);
        ::shurium::Serialize(s, bonusReward_);
        ::shurium::Serialize(s, creationTime_);
        ::shurium::Serialize(s, deadline_);
        ::shurium::Serialize(s, solved_);
        ::shurium::Serialize(s, solver_);
        ::shurium::Serialize(s, creator_);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, id_);
        ::shurium::Unserialize(s, hash_);
        ::shurium::Unserialize(s, spec_);
        ::shurium::Unserialize(s, difficulty_);
        ::shurium::Unserialize(s, reward_);
        ::shurium::Unserialize(s, bonusReward_);
        ::shurium::Unserialize(s, creationTime_);
        ::shurium::Unserialize(s, deadline_);
        ::shurium::Unserialize(s, solved_);
        ::shurium::Unserialize(s, solver_);
        ::shurium::Unserialize(s, creator_);
    }

private:
    Id id_{INVALID_ID};
    ProblemHash hash_;
    ProblemSpec spec_;
    ProblemDifficulty difficulty_;
    Amount reward_{0};
    Amount bonusReward_{0};
    int64_t creationTime_{0};
    int64_t deadline_{0};
    bool solved_{false};
    std::string solver_;
    std::string creator_;
};

/// Free function serializers for Problem
template<typename Stream>
void Serialize(Stream& s, const Problem& problem) {
    problem.Serialize(s);
}

template<typename Stream>
void Unserialize(Stream& s, Problem& problem) {
    problem.Unserialize(s);
}

// ============================================================================
// Problem Factory
// ============================================================================

/**
 * Factory for creating different types of problems.
 */
class ProblemFactory {
public:
    /// Create a factory instance
    static ProblemFactory& Instance();
    
    /// Create an ML training problem
    Problem CreateMLTrainingProblem(
        const std::vector<uint8_t>& modelData,
        const std::vector<uint8_t>& trainingData,
        const std::string& hyperparameters,
        Amount reward,
        int64_t deadline);
    
    /// Create a hash-based PoW problem (fallback)
    Problem CreateHashProblem(
        const Hash256& target,
        uint32_t nonce,
        Amount reward,
        int64_t deadline);
    
    /// Create a linear algebra problem
    Problem CreateLinearAlgebraProblem(
        const std::vector<uint8_t>& matrixData,
        const std::string& operation,
        Amount reward,
        int64_t deadline);
    
    /// Create a custom problem
    Problem CreateCustomProblem(
        ProblemSpec spec,
        ProblemDifficulty difficulty,
        Amount reward,
        int64_t deadline);

private:
    ProblemFactory() = default;
    uint64_t nextId_{1};
};

// ============================================================================
// Problem Pool
// ============================================================================

/**
 * Pool of pending problems waiting to be solved.
 */
class ProblemPool {
public:
    /// Maximum pool size
    static constexpr size_t MAX_POOL_SIZE = 10000;
    
    /// Create a problem pool
    ProblemPool();
    
    /// Destructor
    ~ProblemPool();
    
    // ========================================================================
    // Problem Management
    // ========================================================================
    
    /// Add a problem to the pool
    /// @return true if added successfully
    bool AddProblem(Problem problem);
    
    /// Remove a problem from the pool
    bool RemoveProblem(Problem::Id id);
    
    /// Get a problem by ID
    const Problem* GetProblem(Problem::Id id) const;
    
    /// Get a problem by hash
    const Problem* GetProblemByHash(const ProblemHash& hash) const;
    
    /// Check if problem exists
    bool HasProblem(Problem::Id id) const;
    
    // ========================================================================
    // Selection
    // ========================================================================
    
    /// Get problems for a miner to work on
    /// @param maxCount Maximum number to return
    /// @param minReward Minimum reward filter
    /// @return Vector of problems
    std::vector<const Problem*> GetProblemsForMining(
        size_t maxCount = 10,
        Amount minReward = 0) const;
    
    /// Get highest reward problem
    const Problem* GetHighestRewardProblem() const;
    
    /// Get problems by type
    std::vector<const Problem*> GetProblemsByType(ProblemType type) const;
    
    // ========================================================================
    // Maintenance
    // ========================================================================
    
    /// Remove expired problems
    /// @return Number of problems removed
    size_t RemoveExpired();
    
    /// Mark problem as solved
    void MarkSolved(Problem::Id id, const std::string& solver);
    
    /// Clear the pool
    void Clear();
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get pool size
    size_t Size() const;
    
    /// Check if pool is empty
    bool IsEmpty() const;
    
    /// Get total rewards in pool
    Amount GetTotalRewards() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace marketplace
} // namespace shurium

#endif // SHURIUM_MARKETPLACE_PROBLEM_H
