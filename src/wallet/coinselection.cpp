// SHURIUM - Coin Selection Algorithms Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/wallet/coinselection.h>
#include <shurium/core/random.h>

#include <algorithm>
#include <limits>
#include <random>

namespace shurium {
namespace wallet {

// ============================================================================
// OutputGroup Implementation
// ============================================================================

OutputGroup::OutputGroup(const OutPoint& op, const TxOut& out, FeeRate feeRate,
                         int32_t confirmations, bool isCoinbase, int64_t timestamp)
    : outpoint(op)
    , output(out)
    , depth(confirmations)
    , coinbase(isCoinbase)
    , time(timestamp) {
    CalculateEffectiveValue(feeRate);
}

void OutputGroup::CalculateEffectiveValue(FeeRate feeRate) {
    inputSize = EstimateInputSize(output.scriptPubKey);
    fee = static_cast<Amount>(inputSize * feeRate);
    effectiveValue = output.nValue - fee;
}

// ============================================================================
// SelectionResult Implementation
// ============================================================================

void SelectionResult::Add(const OutputGroup& output) {
    selected.push_back(output);
    totalValue += output.GetValue();
    totalEffectiveValue += output.effectiveValue;
    inputFee += output.fee;
}

void SelectionResult::CalculateTotals(Amount targetValue, FeeRate feeRate, 
                                       size_t outputSize) {
    totalValue = 0;
    totalEffectiveValue = 0;
    inputFee = 0;
    
    for (const auto& out : selected) {
        totalValue += out.GetValue();
        totalEffectiveValue += out.effectiveValue;
        inputFee += out.fee;
    }
    
    // Calculate change
    if (totalEffectiveValue > targetValue) {
        change = totalEffectiveValue - targetValue;
    } else {
        change = 0;
    }
}

// ============================================================================
// Branch and Bound Algorithm
// ============================================================================

SelectionResult BranchAndBound::Select(std::vector<OutputGroup> outputs,
                                        const SelectionParams& params) {
    SelectionResult result;
    result.algorithm = "BranchAndBound";
    
    if (outputs.empty() || params.targetValue <= 0) {
        return result;
    }
    
    // Sort by effective value (descending)
    std::sort(outputs.begin(), outputs.end(),
              [](const OutputGroup& a, const OutputGroup& b) {
                  return a.effectiveValue > b.effectiveValue;
              });
    
    // Remove outputs with negative effective value
    outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
                                  [](const OutputGroup& o) { 
                                      return o.effectiveValue <= 0; 
                                  }),
                  outputs.end());
    
    if (outputs.empty()) {
        return result;
    }
    
    // Calculate total available
    Amount totalAvailable = 0;
    for (const auto& out : outputs) {
        totalAvailable += out.effectiveValue;
    }
    
    // Check if we have enough
    if (totalAvailable < params.targetValue) {
        return result;
    }
    
    Amount target = params.targetValue;
    Amount costOfChange = params.GetChangeCost();
    
    std::vector<bool> selection(outputs.size(), false);
    std::vector<bool> bestSelection(outputs.size(), false);
    Amount bestValue = std::numeric_limits<Amount>::max();
    size_t iterations = 0;
    
    // Start search
    bool found = Search(outputs, target, costOfChange, selection, 0, 0, iterations);
    
    if (!found && iterations >= MAX_ITERATIONS) {
        // Timeout - fall back to simple selection
        return result;
    }
    
    if (found) {
        result.success = true;
        for (size_t i = 0; i < outputs.size(); ++i) {
            if (selection[i]) {
                result.Add(outputs[i]);
            }
        }
        result.CalculateTotals(params.targetValue, params.feeRate, params.outputSize);
    }
    
    return result;
}

bool BranchAndBound::Search(const std::vector<OutputGroup>& outputs,
                             Amount target,
                             Amount costOfChange,
                             std::vector<bool>& selection,
                             Amount currentValue,
                             size_t depth,
                             size_t& iterations) {
    if (++iterations > MAX_ITERATIONS) {
        return false;
    }
    
    // Check if current selection is exact match (within cost of change)
    if (currentValue >= target && currentValue <= target + costOfChange) {
        return true;
    }
    
    // If we've processed all outputs
    if (depth >= outputs.size()) {
        return false;
    }
    
    // Prune: if remaining outputs can't reach target
    Amount remaining = 0;
    for (size_t i = depth; i < outputs.size(); ++i) {
        remaining += outputs[i].effectiveValue;
    }
    
    if (currentValue + remaining < target) {
        return false;  // Can't reach target
    }
    
    // Try including current output
    selection[depth] = true;
    if (Search(outputs, target, costOfChange, selection,
               currentValue + outputs[depth].effectiveValue, depth + 1, iterations)) {
        return true;
    }
    
    // Try excluding current output
    selection[depth] = false;
    if (Search(outputs, target, costOfChange, selection,
               currentValue, depth + 1, iterations)) {
        return true;
    }
    
    return false;
}

// ============================================================================
// Knapsack Algorithm
// ============================================================================

SelectionResult Knapsack::Select(std::vector<OutputGroup> outputs,
                                  const SelectionParams& params) {
    SelectionResult result;
    result.algorithm = "Knapsack";
    
    if (outputs.empty() || params.targetValue <= 0) {
        return result;
    }
    
    // Remove outputs with negative effective value
    outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
                                  [](const OutputGroup& o) { 
                                      return o.effectiveValue <= 0; 
                                  }),
                  outputs.end());
    
    if (outputs.empty()) {
        return result;
    }
    
    Amount target = params.targetValue;
    
    // First, look for an exact match
    std::sort(outputs.begin(), outputs.end(),
              [](const OutputGroup& a, const OutputGroup& b) {
                  return a.effectiveValue > b.effectiveValue;
              });
    
    // Check for single output that matches exactly
    for (const auto& out : outputs) {
        if (out.effectiveValue == target) {
            result.success = true;
            result.Add(out);
            result.CalculateTotals(params.targetValue, params.feeRate, params.outputSize);
            return result;
        }
    }
    
    // Find smallest output larger than target
    OutputGroup* smallestLarger = nullptr;
    for (auto& out : outputs) {
        if (out.effectiveValue >= target) {
            if (!smallestLarger || out.effectiveValue < smallestLarger->effectiveValue) {
                smallestLarger = &out;
            }
        }
    }
    
    // Separate outputs into those smaller and larger than target
    std::vector<OutputGroup> smallerOutputs;
    Amount totalSmaller = 0;
    
    for (const auto& out : outputs) {
        if (out.effectiveValue < target) {
            smallerOutputs.push_back(out);
            totalSmaller += out.effectiveValue;
        }
    }
    
    // If sum of smaller outputs is less than target and we have no larger output
    if (totalSmaller < target && !smallestLarger) {
        return result;  // Not enough funds
    }
    
    // If sum of smaller is enough, try to find good subset
    if (totalSmaller >= target) {
        std::vector<bool> bestSelection(smallerOutputs.size(), false);
        Amount bestValue = 0;
        
        ApproximateBestSubset(smallerOutputs, totalSmaller, target, 
                              bestSelection, bestValue);
        
        // If found selection that reaches target
        if (bestValue >= target) {
            result.success = true;
            for (size_t i = 0; i < smallerOutputs.size(); ++i) {
                if (bestSelection[i]) {
                    result.Add(smallerOutputs[i]);
                }
            }
            result.CalculateTotals(params.targetValue, params.feeRate, params.outputSize);
            
            // Compare with smallest larger output
            if (smallestLarger) {
                Amount waste = result.totalEffectiveValue - target;
                Amount wasteWithLarger = smallestLarger->effectiveValue - target;
                
                // Use larger output if it produces less waste
                if (wasteWithLarger < waste) {
                    result.selected.clear();
                    result.Add(*smallestLarger);
                    result.CalculateTotals(params.targetValue, params.feeRate, 
                                           params.outputSize);
                }
            }
            return result;
        }
    }
    
    // Use smallest larger output
    if (smallestLarger) {
        result.success = true;
        result.Add(*smallestLarger);
        result.CalculateTotals(params.targetValue, params.feeRate, params.outputSize);
        return result;
    }
    
    return result;
}

void Knapsack::ApproximateBestSubset(const std::vector<OutputGroup>& outputs,
                                      Amount totalLower,
                                      Amount targetValue,
                                      std::vector<bool>& selection,
                                      Amount& bestValue) {
    std::fill(selection.begin(), selection.end(), true);
    bestValue = totalLower;
    
    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 1);
    
    for (size_t pass = 0; pass < PASSES; ++pass) {
        std::vector<bool> currentSelection(outputs.size(), false);
        Amount currentValue = 0;
        
        // Random selection
        bool reachedTarget = false;
        for (size_t i = 0; i < outputs.size(); ++i) {
            if (dist(gen) || (pass == 0 && !reachedTarget)) {
                currentSelection[i] = true;
                currentValue += outputs[i].effectiveValue;
                
                if (currentValue >= targetValue) {
                    reachedTarget = true;
                    
                    // Try to remove some outputs
                    for (size_t j = i; j > 0; --j) {
                        if (currentSelection[j - 1] && 
                            currentValue - outputs[j - 1].effectiveValue >= targetValue) {
                            currentSelection[j - 1] = false;
                            currentValue -= outputs[j - 1].effectiveValue;
                        }
                    }
                }
            }
        }
        
        // Update best if current is better
        if (currentValue >= targetValue && currentValue < bestValue) {
            selection = currentSelection;
            bestValue = currentValue;
            
            // If we found exact match, we're done
            if (currentValue == targetValue) {
                return;
            }
        }
    }
}

// ============================================================================
// Single Random Draw Algorithm
// ============================================================================

SelectionResult SingleRandomDraw::Select(std::vector<OutputGroup> outputs,
                                          const SelectionParams& params) {
    SelectionResult result;
    result.algorithm = "SingleRandomDraw";
    
    if (outputs.empty() || params.targetValue <= 0) {
        return result;
    }
    
    // Shuffle outputs
    ShuffleOutputs(outputs);
    
    // Remove outputs with negative effective value
    outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
                                  [](const OutputGroup& o) { 
                                      return o.effectiveValue <= 0; 
                                  }),
                  outputs.end());
    
    // Select outputs until we reach target
    Amount accumulated = 0;
    
    for (const auto& out : outputs) {
        result.Add(out);
        accumulated += out.effectiveValue;
        
        if (accumulated >= params.targetValue) {
            result.success = true;
            result.CalculateTotals(params.targetValue, params.feeRate, params.outputSize);
            return result;
        }
        
        // Check max inputs limit
        if (result.selected.size() >= params.maxInputs) {
            break;
        }
    }
    
    // Failed to reach target
    result.selected.clear();
    return result;
}

// ============================================================================
// FIFO Selection Algorithm
// ============================================================================

SelectionResult FIFOSelection::Select(std::vector<OutputGroup> outputs,
                                       const SelectionParams& params) {
    SelectionResult result;
    result.algorithm = "FIFO";
    
    if (outputs.empty() || params.targetValue <= 0) {
        return result;
    }
    
    // Sort by time (oldest first)
    SortByTime(outputs, true);
    
    // Remove outputs with negative effective value
    outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
                                  [](const OutputGroup& o) { 
                                      return o.effectiveValue <= 0; 
                                  }),
                  outputs.end());
    
    // Select oldest outputs until we reach target
    Amount accumulated = 0;
    
    for (const auto& out : outputs) {
        result.Add(out);
        accumulated += out.effectiveValue;
        
        if (accumulated >= params.targetValue) {
            result.success = true;
            result.CalculateTotals(params.targetValue, params.feeRate, params.outputSize);
            return result;
        }
        
        if (result.selected.size() >= params.maxInputs) {
            break;
        }
    }
    
    result.selected.clear();
    return result;
}

// ============================================================================
// Largest First Selection Algorithm
// ============================================================================

SelectionResult LargestFirst::Select(std::vector<OutputGroup> outputs,
                                      const SelectionParams& params) {
    SelectionResult result;
    result.algorithm = "LargestFirst";
    
    if (outputs.empty() || params.targetValue <= 0) {
        return result;
    }
    
    // Sort by value (largest first)
    SortByValue(outputs, false);
    
    // Remove outputs with negative effective value
    outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
                                  [](const OutputGroup& o) { 
                                      return o.effectiveValue <= 0; 
                                  }),
                  outputs.end());
    
    // Select largest outputs until we reach target
    Amount accumulated = 0;
    
    for (const auto& out : outputs) {
        result.Add(out);
        accumulated += out.effectiveValue;
        
        if (accumulated >= params.targetValue) {
            result.success = true;
            result.CalculateTotals(params.targetValue, params.feeRate, params.outputSize);
            return result;
        }
        
        if (result.selected.size() >= params.maxInputs) {
            break;
        }
    }
    
    result.selected.clear();
    return result;
}

// ============================================================================
// CoinSelector Implementation
// ============================================================================

CoinSelector::CoinSelector() = default;

CoinSelector::CoinSelector(const SelectionParams& params) : params_(params) {}

SelectionResult CoinSelector::Select(const std::vector<OutputGroup>& outputs,
                                       SelectionStrategy strategy) const {
    auto filtered = FilterOutputs(outputs);
    
    switch (strategy) {
        case SelectionStrategy::Auto:
            return SelectAuto(filtered);
            
        case SelectionStrategy::BranchAndBound:
            return BranchAndBound::Select(std::move(filtered), params_);
            
        case SelectionStrategy::Knapsack:
            return Knapsack::Select(std::move(filtered), params_);
            
        case SelectionStrategy::Random:
            return SingleRandomDraw::Select(std::move(filtered), params_);
            
        case SelectionStrategy::FIFO:
            return FIFOSelection::Select(std::move(filtered), params_);
            
        case SelectionStrategy::LargestFirst:
            return LargestFirst::Select(std::move(filtered), params_);
            
        default:
            return SelectionResult();
    }
}

SelectionResult CoinSelector::SelectForAmount(const std::vector<OutputGroup>& outputs,
                                               Amount amount,
                                               SelectionStrategy strategy) const {
    SelectionParams params = params_;
    params.targetValue = amount;
    CoinSelector selector(params);
    return selector.Select(outputs, strategy);
}

std::vector<OutputGroup> CoinSelector::FilterOutputs(
    const std::vector<OutputGroup>& outputs) const {
    
    std::vector<OutputGroup> filtered;
    filtered.reserve(outputs.size());
    
    for (const auto& out : outputs) {
        // Check confirmations
        if (out.depth < params_.minConfirmations) {
            continue;
        }
        
        // Check unconfirmed
        if (!params_.includeUnconfirmed && out.depth == 0) {
            continue;
        }
        
        // Check positive effective value
        if (out.effectiveValue <= 0) {
            continue;
        }
        
        filtered.push_back(out);
    }
    
    // If prefer confirmed, sort confirmed first
    if (params_.preferConfirmed) {
        std::stable_sort(filtered.begin(), filtered.end(),
                         [](const OutputGroup& a, const OutputGroup& b) {
                             return a.depth > b.depth;
                         });
    }
    
    return filtered;
}

Amount CoinSelector::CalculateFee(const SelectionResult& result, 
                                   size_t outputCount) const {
    // Base transaction size
    size_t txSize = 10;  // Version (4) + locktime (4) + input/output counts (~2)
    
    // Input size
    for (const auto& out : result.selected) {
        txSize += out.inputSize;
    }
    
    // Output size
    txSize += outputCount * params_.outputSize;
    
    // Change output
    if (result.change > 0) {
        txSize += params_.changeOutputSize;
    }
    
    return static_cast<Amount>(txSize * params_.feeRate);
}

Amount CoinSelector::CalculateChange(const SelectionResult& result, 
                                      Amount targetWithFee) const {
    if (result.totalEffectiveValue > targetWithFee) {
        return result.totalEffectiveValue - targetWithFee;
    }
    return 0;
}

SelectionResult CoinSelector::SelectAuto(const std::vector<OutputGroup>& outputs) const {
    // Try Branch and Bound first (best for avoiding change)
    auto bnbResult = BranchAndBound::Select(
        std::vector<OutputGroup>(outputs), params_);
    
    if (bnbResult.success && bnbResult.change == 0) {
        return bnbResult;  // Perfect match!
    }
    
    // Try Knapsack
    auto knapsackResult = Knapsack::Select(
        std::vector<OutputGroup>(outputs), params_);
    
    // Compare results and pick the best
    if (bnbResult.success && knapsackResult.success) {
        // Prefer result with less waste (smaller change)
        if (bnbResult.change <= knapsackResult.change) {
            return bnbResult;
        }
        return knapsackResult;
    }
    
    if (bnbResult.success) {
        return bnbResult;
    }
    
    if (knapsackResult.success) {
        return knapsackResult;
    }
    
    // Fall back to largest first
    return LargestFirst::Select(std::vector<OutputGroup>(outputs), params_);
}

// ============================================================================
// Utility Functions
// ============================================================================

size_t EstimateInputSize(const Script& scriptPubKey) {
    // Estimate based on script type
    // P2PKH: ~148 bytes
    // P2SH: ~297 bytes (2-of-3 multisig)
    // P2WPKH: ~68 vbytes
    // P2WSH: ~104 vbytes
    
    if (scriptPubKey.empty()) {
        return 148;  // Default to P2PKH
    }
    
    // Check for witness programs (P2WPKH/P2WSH)
    if (scriptPubKey.size() >= 2 && scriptPubKey[0] == 0x00) {
        if (scriptPubKey.size() == 22) {  // OP_0 PUSH20
            return 68;  // P2WPKH
        } else if (scriptPubKey.size() == 34) {  // OP_0 PUSH32
            return 104;  // P2WSH
        }
    }
    
    // Check for P2PKH (OP_DUP OP_HASH160 PUSH20 ... OP_EQUALVERIFY OP_CHECKSIG)
    if (scriptPubKey.size() == 25 && 
        scriptPubKey[0] == 0x76 && scriptPubKey[1] == 0xa9) {
        return 148;
    }
    
    // Check for P2SH (OP_HASH160 PUSH20 ... OP_EQUAL)
    if (scriptPubKey.size() == 23 && scriptPubKey[0] == 0xa9) {
        return 297;  // Assume 2-of-3 multisig
    }
    
    return 148;  // Default
}

size_t EstimateOutputSize(const Script& scriptPubKey) {
    // P2PKH: 34 bytes
    // P2SH: 32 bytes
    // P2WPKH: 31 bytes
    // P2WSH: 43 bytes
    
    if (scriptPubKey.empty()) {
        return 34;
    }
    
    if (scriptPubKey.size() >= 2 && scriptPubKey[0] == 0x00) {
        if (scriptPubKey.size() == 22) {
            return 31;  // P2WPKH
        } else if (scriptPubKey.size() == 34) {
            return 43;  // P2WSH
        }
    }
    
    if (scriptPubKey.size() == 25) {
        return 34;  // P2PKH
    }
    
    if (scriptPubKey.size() == 23) {
        return 32;  // P2SH
    }
    
    return 34;
}

Amount CalculateTransactionFee(size_t numInputs, size_t inputSize,
                                size_t numOutputs, size_t outputSize,
                                FeeRate feeRate) {
    size_t txSize = 10;  // Base size
    txSize += numInputs * inputSize;
    txSize += numOutputs * outputSize;
    return static_cast<Amount>(txSize * feeRate);
}

FeeRate GetMinRelayFee() {
    return 1;  // 1 sat/vbyte minimum
}

Amount GetDustThreshold(const TxOut& output, FeeRate dustRelayFee) {
    // Dust threshold = 3 * (input_size + output_size) * dust_relay_fee
    size_t inputSize = EstimateInputSize(output.scriptPubKey);
    size_t outputSize = EstimateOutputSize(output.scriptPubKey);
    return 3 * (inputSize + outputSize) * dustRelayFee;
}

bool IsDust(const TxOut& output, FeeRate dustRelayFee) {
    return output.nValue < GetDustThreshold(output, dustRelayFee);
}

void SortByValue(std::vector<OutputGroup>& outputs, bool ascending) {
    std::sort(outputs.begin(), outputs.end(),
              [ascending](const OutputGroup& a, const OutputGroup& b) {
                  if (ascending) {
                      return a.effectiveValue < b.effectiveValue;
                  } else {
                      return a.effectiveValue > b.effectiveValue;
                  }
              });
}

void SortByDepth(std::vector<OutputGroup>& outputs, bool ascending) {
    std::sort(outputs.begin(), outputs.end(),
              [ascending](const OutputGroup& a, const OutputGroup& b) {
                  if (ascending) {
                      return a.depth < b.depth;
                  } else {
                      return a.depth > b.depth;
                  }
              });
}

void SortByTime(std::vector<OutputGroup>& outputs, bool ascending) {
    std::sort(outputs.begin(), outputs.end(),
              [ascending](const OutputGroup& a, const OutputGroup& b) {
                  if (ascending) {
                      return a.time < b.time;
                  } else {
                      return a.time > b.time;
                  }
              });
}

void ShuffleOutputs(std::vector<OutputGroup>& outputs) {
    if (outputs.size() <= 1) {
        return;
    }
    
    // Fisher-Yates shuffle using cryptographic randomness
    for (size_t i = outputs.size() - 1; i > 0; --i) {
        uint64_t rand;
        GetRandBytes(reinterpret_cast<Byte*>(&rand), sizeof(rand));
        size_t j = rand % (i + 1);
        std::swap(outputs[i], outputs[j]);
    }
}

} // namespace wallet
} // namespace shurium
