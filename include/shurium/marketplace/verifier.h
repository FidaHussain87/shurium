// SHURIUM - Proof of Useful Work Verifier
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Verifies solutions to computational problems.
// Different problem types have different verification strategies.

#ifndef SHURIUM_MARKETPLACE_VERIFIER_H
#define SHURIUM_MARKETPLACE_VERIFIER_H

#include <shurium/core/types.h>
#include <shurium/marketplace/problem.h>
#include <shurium/marketplace/solution.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace marketplace {

// ============================================================================
// Verification Result
// ============================================================================

/// Result of solution verification
enum class VerificationResult {
    /// Solution is valid
    VALID,
    
    /// Solution is invalid
    INVALID,
    
    /// Problem not found
    PROBLEM_NOT_FOUND,
    
    /// Solution has wrong format
    MALFORMED,
    
    /// Problem/solution type mismatch
    TYPE_MISMATCH,
    
    /// Verification timed out
    TIMEOUT,
    
    /// Internal verifier error
    ERROR
};

/// Get string representation of verification result
const char* VerificationResultToString(VerificationResult result);

// ============================================================================
// Verification Details
// ============================================================================

/**
 * Detailed information about a verification result.
 */
struct VerificationDetails {
    /// Overall result
    VerificationResult result{VerificationResult::ERROR};
    
    /// Error message (if any)
    std::string errorMessage;
    
    /// Verification time (milliseconds)
    uint64_t verificationTimeMs{0};
    
    /// Score/quality metric (0-1000000)
    uint32_t score{0};
    
    /// Whether solution meets minimum requirements
    bool meetsRequirements{false};
    
    /// Specific checks performed
    std::vector<std::pair<std::string, bool>> checks;
    
    /// Check if verification was successful
    bool IsValid() const { return result == VerificationResult::VALID; }
    
    /// Add a check result
    void AddCheck(const std::string& name, bool passed) {
        checks.emplace_back(name, passed);
    }
    
    /// Convert to string
    std::string ToString() const;
};

// ============================================================================
// Verifier Interface
// ============================================================================

/**
 * Interface for problem-specific verifiers.
 * 
 * Each problem type has its own verifier implementation.
 */
class IVerifier {
public:
    virtual ~IVerifier() = default;
    
    /// Get the problem type this verifier handles
    virtual ProblemType GetType() const = 0;
    
    /// Verify a solution against a problem
    virtual VerificationDetails Verify(
        const Problem& problem,
        const Solution& solution) = 0;
    
    /// Quick validation (structural checks only)
    virtual bool QuickValidate(
        const Problem& problem,
        const Solution& solution) = 0;
    
    /// Get estimated verification time (milliseconds)
    virtual uint64_t EstimateVerificationTime(
        const Problem& problem) const = 0;
};

// ============================================================================
// Hash PoW Verifier
// ============================================================================

/**
 * Verifier for hash-based proof of work problems.
 */
class HashPowVerifier : public IVerifier {
public:
    HashPowVerifier();
    ~HashPowVerifier() override;
    
    ProblemType GetType() const override { return ProblemType::HASH_POW; }
    
    VerificationDetails Verify(
        const Problem& problem,
        const Solution& solution) override;
    
    bool QuickValidate(
        const Problem& problem,
        const Solution& solution) override;
    
    uint64_t EstimateVerificationTime(
        const Problem& problem) const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// ML Training Verifier
// ============================================================================

/**
 * Verifier for machine learning training problems.
 * 
 * Verifies that:
 * 1. Model weights are valid
 * 2. Training improved the model
 * 3. Validation accuracy meets threshold
 */
class MLTrainingVerifier : public IVerifier {
public:
    MLTrainingVerifier();
    ~MLTrainingVerifier() override;
    
    ProblemType GetType() const override { return ProblemType::ML_TRAINING; }
    
    VerificationDetails Verify(
        const Problem& problem,
        const Solution& solution) override;
    
    bool QuickValidate(
        const Problem& problem,
        const Solution& solution) override;
    
    uint64_t EstimateVerificationTime(
        const Problem& problem) const override;
    
    /// Set minimum accuracy threshold (0-1000000)
    void SetMinAccuracy(uint32_t accuracy) { minAccuracy_ = accuracy; }
    
    /// Set maximum verification time
    void SetMaxVerificationTime(uint64_t ms) { maxVerificationTime_ = ms; }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    uint32_t minAccuracy_{800000};  // 80% default
    uint64_t maxVerificationTime_{60000};  // 60s default
};

// ============================================================================
// Linear Algebra Verifier
// ============================================================================

/**
 * Verifier for linear algebra problems.
 * 
 * Verifies matrix operations by checking results against
 * known properties and spot-checking computations.
 */
class LinearAlgebraVerifier : public IVerifier {
public:
    LinearAlgebraVerifier();
    ~LinearAlgebraVerifier() override;
    
    ProblemType GetType() const override { return ProblemType::LINEAR_ALGEBRA; }
    
    VerificationDetails Verify(
        const Problem& problem,
        const Solution& solution) override;
    
    bool QuickValidate(
        const Problem& problem,
        const Solution& solution) override;
    
    uint64_t EstimateVerificationTime(
        const Problem& problem) const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Verifier Registry
// ============================================================================

/**
 * Registry of verifiers for different problem types.
 */
class VerifierRegistry {
public:
    /// Get singleton instance
    static VerifierRegistry& Instance();
    
    /// Register a verifier
    void Register(std::unique_ptr<IVerifier> verifier);
    
    /// Get verifier for a problem type
    IVerifier* GetVerifier(ProblemType type);
    
    /// Check if a verifier is registered
    bool HasVerifier(ProblemType type) const;
    
    /// Get all registered types
    std::vector<ProblemType> GetRegisteredTypes() const;

private:
    VerifierRegistry();
    ~VerifierRegistry();
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Solution Verifier (Main Interface)
// ============================================================================

/**
 * Main verifier interface for the PoUW system.
 * 
 * Routes verification to the appropriate type-specific verifier
 * and handles common validation logic.
 */
class SolutionVerifier {
public:
    /// Verification callback type
    using VerificationCallback = std::function<void(
        Solution::Id solutionId,
        const VerificationDetails& result)>;
    
    /// Create a solution verifier
    SolutionVerifier();
    
    /// Destructor
    ~SolutionVerifier();
    
    // ========================================================================
    // Synchronous Verification
    // ========================================================================
    
    /// Verify a solution synchronously
    VerificationDetails Verify(
        const Problem& problem,
        const Solution& solution);
    
    /// Quick validation (structural checks only)
    bool QuickValidate(
        const Problem& problem,
        const Solution& solution);
    
    // ========================================================================
    // Asynchronous Verification
    // ========================================================================
    
    /// Submit solution for asynchronous verification
    /// @return true if submitted successfully
    bool SubmitForVerification(
        const Problem& problem,
        Solution solution,
        VerificationCallback callback);
    
    /// Get pending verification count
    size_t GetPendingCount() const;
    
    /// Cancel pending verification
    bool CancelVerification(Solution::Id solutionId);
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /// Set maximum concurrent verifications
    void SetMaxConcurrent(size_t max) { maxConcurrent_ = max; }
    
    /// Set verification timeout (milliseconds)
    void SetTimeout(uint64_t ms) { timeout_ = ms; }
    
    /// Set strict mode (fail on any check failure)
    void SetStrictMode(bool strict) { strictMode_ = strict; }
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get total verifications performed
    uint64_t GetTotalVerifications() const;
    
    /// Get successful verification count
    uint64_t GetSuccessfulCount() const;
    
    /// Get failed verification count
    uint64_t GetFailedCount() const;
    
    /// Get average verification time (ms)
    uint64_t GetAverageVerificationTime() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    size_t maxConcurrent_{4};
    uint64_t timeout_{120000};  // 2 minutes
    bool strictMode_{false};
};

// ============================================================================
// Verification Utility Functions
// ============================================================================

/// Verify that solution hash is below target
bool VerifyHashTarget(const Hash256& hash, uint64_t target);

/// Verify solution data integrity
bool VerifyDataIntegrity(const SolutionData& data);

/// Compute expected hash for verification
Hash256 ComputeVerificationHash(
    const Problem& problem,
    const SolutionData& data);

} // namespace marketplace
} // namespace shurium

#endif // SHURIUM_MARKETPLACE_VERIFIER_H
