// SHURIUM - Coin Selection Algorithms
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements coin selection algorithms for transaction building:
// - Knapsack: Bitcoin Core's classic algorithm
// - Branch and Bound: Optimal selection avoiding change
// - Single Random Draw: Simple random selection
// - FIFO: First-in-first-out for privacy

#ifndef SHURIUM_WALLET_COINSELECTION_H
#define SHURIUM_WALLET_COINSELECTION_H

#include <shurium/core/types.h>
#include <shurium/core/transaction.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <vector>

namespace shurium {
namespace wallet {

// ============================================================================
// Types
// ============================================================================

/// Fee rate in satoshis per virtual byte
using FeeRate = int64_t;

/// Represents an available output for spending
struct OutputGroup {
    /// The outpoint
    OutPoint outpoint;
    
    /// The output
    TxOut output;
    
    /// Effective value (value minus fee to spend)
    Amount effectiveValue;
    
    /// Fee to spend this output
    Amount fee;
    
    /// Input size in virtual bytes
    size_t inputSize;
    
    /// Depth (confirmations)
    int32_t depth;
    
    /// Is from coinbase?
    bool coinbase;
    
    /// Time received
    int64_t time;
    
    /// Constructor
    OutputGroup(const OutPoint& op, const TxOut& out, FeeRate feeRate,
                int32_t confirmations = 0, bool isCoinbase = false, int64_t timestamp = 0);
    
    /// Default constructor
    OutputGroup() : effectiveValue(0), fee(0), inputSize(0), depth(0),
                    coinbase(false), time(0) {}
    
    /// Calculate effective value given fee rate
    void CalculateEffectiveValue(FeeRate feeRate);
    
    /// Get the value
    Amount GetValue() const { return output.nValue; }
    
    /// Comparison by value (for sorting)
    bool operator<(const OutputGroup& other) const {
        return effectiveValue < other.effectiveValue;
    }
};

/// Result of coin selection
struct SelectionResult {
    /// Selected outputs
    std::vector<OutputGroup> selected;
    
    /// Total value of selected outputs
    Amount totalValue;
    
    /// Total effective value (after input fees)
    Amount totalEffectiveValue;
    
    /// Total fee for inputs
    Amount inputFee;
    
    /// Change amount (may be 0 if exact match)
    Amount change;
    
    /// Was selection successful?
    bool success;
    
    /// Algorithm used
    std::string algorithm;
    
    /// Constructor
    SelectionResult() : totalValue(0), totalEffectiveValue(0), inputFee(0),
                        change(0), success(false) {}
    
    /// Get number of selected outputs
    size_t Size() const { return selected.size(); }
    
    /// Add an output to selection
    void Add(const OutputGroup& output);
    
    /// Calculate totals
    void CalculateTotals(Amount targetValue, FeeRate feeRate, size_t outputSize);
};

// ============================================================================
// Selection Parameters
// ============================================================================

/// Parameters for coin selection
struct SelectionParams {
    /// Target value to send
    Amount targetValue;
    
    /// Fee rate (satoshis per vbyte)
    FeeRate feeRate;
    
    /// Fixed fee (if any)
    Amount fixedFee;
    
    /// Size of each output in transaction
    size_t outputSize;
    
    /// Number of outputs in transaction
    size_t outputCount;
    
    /// Minimum change to create
    Amount minChange;
    
    /// Maximum change (above this, prefer creating change output)
    Amount maxChange;
    
    /// Change output size
    size_t changeOutputSize;
    
    /// Minimum confirmations required
    int32_t minConfirmations;
    
    /// Maximum number of inputs
    size_t maxInputs;
    
    /// Include unconfirmed?
    bool includeUnconfirmed;
    
    /// Prefer confirmed outputs?
    bool preferConfirmed;
    
    /// Constructor with defaults
    SelectionParams()
        : targetValue(0), feeRate(1), fixedFee(0)
        , outputSize(34), outputCount(1), minChange(546)
        , maxChange(1000000), changeOutputSize(32)
        , minConfirmations(0), maxInputs(500)
        , includeUnconfirmed(true), preferConfirmed(true) {}
    
    /// Get cost of change output
    Amount GetChangeCost() const {
        return static_cast<Amount>(changeOutputSize * feeRate);
    }
    
    /// Get minimum target including fees
    Amount GetMinTarget() const {
        return targetValue + GetChangeCost();
    }
};

// ============================================================================
// Coin Selection Algorithms
// ============================================================================

/**
 * Coin selection using the Branch and Bound algorithm.
 * 
 * Attempts to find an exact match (no change) within a tolerance.
 * This is optimal for avoiding change outputs and thus improving privacy.
 * 
 * Based on: "An Efficient Algorithm for Finding Multiple Solutions to
 * Bounded Knapsack Problems"
 */
class BranchAndBound {
public:
    /// Maximum iterations to try
    static constexpr size_t MAX_ITERATIONS = 100000;
    
    /// Select coins using branch and bound
    /// @param outputs Available outputs
    /// @param params Selection parameters
    /// @return Selection result
    static SelectionResult Select(std::vector<OutputGroup> outputs,
                                   const SelectionParams& params);

private:
    /// Recursive search
    static bool Search(const std::vector<OutputGroup>& outputs,
                       Amount target,
                       Amount costOfChange,
                       std::vector<bool>& selection,
                       Amount currentValue,
                       size_t depth,
                       size_t& iterations);
};

/**
 * Knapsack coin selection algorithm.
 * 
 * Classic algorithm used by Bitcoin Core. Tries to find a good
 * selection by repeatedly selecting random subsets.
 */
class Knapsack {
public:
    /// Number of random passes
    static constexpr size_t PASSES = 50;
    
    /// Select coins using knapsack algorithm
    static SelectionResult Select(std::vector<OutputGroup> outputs,
                                   const SelectionParams& params);

private:
    /// Approximate best subset selection
    static void ApproximateBestSubset(const std::vector<OutputGroup>& outputs,
                                       Amount totalLower,
                                       Amount targetValue,
                                       std::vector<bool>& selection,
                                       Amount& bestValue);
};

/**
 * Single Random Draw selection.
 * 
 * Simple algorithm that randomly shuffles outputs and selects
 * until target is met. Good for privacy as selection is unpredictable.
 */
class SingleRandomDraw {
public:
    static SelectionResult Select(std::vector<OutputGroup> outputs,
                                   const SelectionParams& params);
};

/**
 * FIFO (First-In-First-Out) selection.
 * 
 * Selects oldest outputs first. Good for UTXO consolidation
 * and avoiding dust accumulation.
 */
class FIFOSelection {
public:
    static SelectionResult Select(std::vector<OutputGroup> outputs,
                                   const SelectionParams& params);
};

/**
 * Largest-First selection.
 * 
 * Selects largest outputs first. Minimizes number of inputs
 * but may leave small UTXOs unspent.
 */
class LargestFirst {
public:
    static SelectionResult Select(std::vector<OutputGroup> outputs,
                                   const SelectionParams& params);
};

// ============================================================================
// Coin Selector
// ============================================================================

/// Selection strategy
enum class SelectionStrategy {
    /// Try all algorithms, pick best result
    Auto,
    
    /// Branch and bound only
    BranchAndBound,
    
    /// Knapsack only
    Knapsack,
    
    /// Single random draw
    Random,
    
    /// FIFO selection
    FIFO,
    
    /// Largest first
    LargestFirst
};

/**
 * Main coin selector that orchestrates different algorithms.
 */
class CoinSelector {
public:
    /// Create selector with default parameters
    CoinSelector();
    
    /// Create with specific parameters
    explicit CoinSelector(const SelectionParams& params);
    
    /// Set parameters
    void SetParams(const SelectionParams& params) { params_ = params; }
    
    /// Get parameters
    const SelectionParams& GetParams() const { return params_; }
    
    /// Select coins using specified strategy
    SelectionResult Select(const std::vector<OutputGroup>& outputs,
                           SelectionStrategy strategy = SelectionStrategy::Auto) const;
    
    /// Select coins for a specific amount
    SelectionResult SelectForAmount(const std::vector<OutputGroup>& outputs,
                                     Amount amount,
                                     SelectionStrategy strategy = SelectionStrategy::Auto) const;
    
    /// Filter outputs based on parameters
    std::vector<OutputGroup> FilterOutputs(const std::vector<OutputGroup>& outputs) const;
    
    /// Calculate required fee for a selection
    Amount CalculateFee(const SelectionResult& result, size_t outputCount) const;
    
    /// Calculate change amount
    Amount CalculateChange(const SelectionResult& result, Amount targetWithFee) const;

private:
    SelectionParams params_;
    
    /// Select using auto strategy
    SelectionResult SelectAuto(const std::vector<OutputGroup>& outputs) const;
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Estimate input size for a script type
size_t EstimateInputSize(const Script& scriptPubKey);

/// Estimate output size for a script type  
size_t EstimateOutputSize(const Script& scriptPubKey);

/// Calculate fee for transaction with given inputs and outputs
Amount CalculateTransactionFee(size_t numInputs, size_t inputSize,
                                size_t numOutputs, size_t outputSize,
                                FeeRate feeRate);

/// Get minimum relay fee
FeeRate GetMinRelayFee();

/// Get dust threshold for an output
Amount GetDustThreshold(const TxOut& output, FeeRate dustRelayFee);

/// Check if output is dust
bool IsDust(const TxOut& output, FeeRate dustRelayFee);

/// Sort outputs by various criteria
void SortByValue(std::vector<OutputGroup>& outputs, bool ascending = true);
void SortByDepth(std::vector<OutputGroup>& outputs, bool ascending = false);
void SortByTime(std::vector<OutputGroup>& outputs, bool ascending = true);
void ShuffleOutputs(std::vector<OutputGroup>& outputs);

} // namespace wallet
} // namespace shurium

#endif // SHURIUM_WALLET_COINSELECTION_H
