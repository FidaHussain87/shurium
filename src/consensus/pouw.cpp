// SHURIUM - Proof of Useful Work Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements the Proof of Useful Work consensus mechanism for SHURIUM.
// PoUW combines traditional hash-based mining with verifiable useful computation.

#include <shurium/consensus/params.h>
#include <shurium/consensus/validation.h>
#include <shurium/core/block.h>
#include <shurium/crypto/sha256.h>
#include <shurium/chain/blockindex.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace shurium {
namespace consensus {

// Note: Params::Main(), TestNet(), and RegTest() are defined in params.cpp
// This file contains PoUW-specific consensus functions.

// ============================================================================
// Block Subsidy Calculation
// ============================================================================

Amount GetBlockSubsidy(int nHeight, const Params& params) {
    // Genesis block
    if (nHeight == 0) {
        return params.nInitialBlockReward;
    }
    
    // Calculate number of halvings
    int halvings = nHeight / params.nSubsidyHalvingInterval;
    
    // After 64 halvings, subsidy is essentially zero
    if (halvings >= 64) {
        return 0;
    }
    
    Amount subsidy = params.nInitialBlockReward;
    subsidy >>= halvings;
    
    return subsidy;
}

// ============================================================================
// Reward Distribution
// ============================================================================

Amount CalculateUBIReward(Amount blockReward, const Params& params) {
    return (blockReward * params.nUBIPercentage) / 100;
}

Amount CalculateWorkReward(Amount blockReward, const Params& params) {
    return (blockReward * params.nWorkRewardPercentage) / 100;
}

Amount CalculateContributionReward(Amount blockReward, const Params& params) {
    return (blockReward * params.nContributionRewardPercentage) / 100;
}

Amount CalculateEcosystemReward(Amount blockReward, const Params& params) {
    return (blockReward * params.nEcosystemPercentage) / 100;
}

Amount CalculateStabilityReserve(Amount blockReward, const Params& params) {
    return (blockReward * params.nStabilityReservePercentage) / 100;
}

bool IsUBIDistributionBlock(int nHeight, const Params& params) {
    if (nHeight == 0) return false;
    return (nHeight % params.nUBIDistributionInterval) == 0;
}

// ============================================================================
// Difficulty Functions
// ============================================================================

Hash256 CompactToBig(uint32_t nCompact) {
    Hash256 target;
    target.SetNull();
    
    // Extract size and word
    // Size indicates the number of significant bytes
    // Word contains the 3 most significant bytes of the target
    int size = (nCompact >> 24) & 0xFF;
    uint32_t word = nCompact & 0x007FFFFF;
    
    // Check for negative (sign bit set in word)
    if (nCompact & 0x00800000) {
        // Negative targets are invalid
        return target;
    }
    
    // Handle overflow - size can't exceed 32 for a 256-bit number
    if (size > 34) {
        return target;  // Return zero target (invalid)
    }
    
    // The compact format represents: word * 2^(8*(size-3))
    // In little-endian storage (LSB at byte[0], MSB at byte[31]):
    // - Position byte[0] is the least significant
    // - Position byte[31] is the most significant
    //
    // For size=32, word should go at bytes 29, 30, 31 (the most significant)
    // For size=3, word should go at bytes 0, 1, 2 (the least significant)
    
    if (size <= 3) {
        // Word fits in first 3 bytes
        word >>= 8 * (3 - size);
        target[0] = word & 0xFF;
        target[1] = (word >> 8) & 0xFF;
        target[2] = (word >> 16) & 0xFF;
    } else {
        // Position in little-endian array: bytes (size-3) to (size-1)
        int pos = size - 3;
        if (pos <= 29) {
            // Put the 3 bytes of word at the appropriate position
            target[pos] = word & 0xFF;
            if (pos + 1 < 32) target[pos + 1] = (word >> 8) & 0xFF;
            if (pos + 2 < 32) target[pos + 2] = (word >> 16) & 0xFF;
        }
    }
    
    return target;
}

uint32_t BigToCompact(const Hash256& target) {
    // In little-endian storage, byte[31] is the most significant byte
    // Find the most significant non-zero byte (scan from high to low)
    int msb_pos = -1;
    for (int i = 31; i >= 0; --i) {
        if (target[i] != 0) {
            msb_pos = i;
            break;
        }
    }
    
    if (msb_pos < 0) {
        return 0;  // All zeros
    }
    
    // Size is the number of bytes needed to represent the number
    // msb_pos is the index of the most significant non-zero byte
    int size = msb_pos + 1;
    
    uint32_t compact;
    
    if (size <= 3) {
        // Number fits in 3 bytes or less
        uint32_t word = 0;
        for (int i = size - 1; i >= 0; --i) {
            word = (word << 8) | target[i];
        }
        word <<= 8 * (3 - size);
        compact = (size << 24) | word;
    } else {
        // Extract the 3 most significant bytes
        // They are at positions msb_pos, msb_pos-1, msb_pos-2
        uint32_t word = (static_cast<uint32_t>(target[msb_pos]) << 16) |
                       (static_cast<uint32_t>(target[msb_pos - 1]) << 8) |
                       static_cast<uint32_t>(target[msb_pos - 2]);
        
        // Handle negative flag (if high bit of word is set)
        if (word & 0x00800000) {
            word >>= 8;
            size++;
        }
        
        compact = (size << 24) | word;
    }
    
    return compact;
}

bool CheckProofOfWork(const BlockHash& hash, uint32_t nBits, const Params& params) {
    // Check that nBits is not zero
    if (nBits == 0) {
        return false;
    }
    
    // Convert compact target to full target
    Hash256 target = CompactToBig(nBits);
    
    // Check target is below limit (powLimit is the maximum allowed target)
    // In little-endian storage, MSB is at byte[31], LSB at byte[0]
    // Compare from MSB to LSB
    bool targetExceedsLimit = false;
    for (int i = 31; i >= 0; --i) {
        if (target[i] > params.powLimit[i]) {
            targetExceedsLimit = true;
            break;
        } else if (target[i] < params.powLimit[i]) {
            break;
        }
    }
    
    if (targetExceedsLimit) {
        return false;
    }
    
    // Check that hash is below target
    // The < operator already handles little-endian comparison correctly
    return static_cast<const Hash256&>(hash) < target;
}

// ============================================================================
// PoUW Difficulty Adjustment
// ============================================================================

/// Get the next work required (main entry point for difficulty adjustment)
/// This determines the nBits value for the next block to be mined.
uint32_t GetNextWorkRequired(const BlockIndex* pindexLast, const Params& params) {
    return GetNextWorkRequired(pindexLast, nullptr, params);
}

/// Get the next work required with optional block header
/// The pblock parameter allows special testnet rules (minimum difficulty blocks)
uint32_t GetNextWorkRequired(const BlockIndex* pindexLast, const BlockHeader* pblock,
                             const Params& params) {
    // Genesis block or empty chain - use genesis difficulty
    if (pindexLast == nullptr) {
        return BigToCompact(params.powLimit);
    }
    
    // Regtest: no retargeting
    if (params.fPowNoRetargeting) {
        return pindexLast->nBits;
    }
    
    // Check if we're at a difficulty adjustment interval
    int64_t difficultyAdjustmentInterval = params.DifficultyAdjustmentInterval();
    int nextHeight = pindexLast->nHeight + 1;
    
    // Special rule for testnet: allow minimum difficulty blocks
    // if the block's timestamp is more than 2x target spacing after previous block
    if (params.fAllowMinDifficultyBlocks && pblock != nullptr) {
        int64_t blockTime = pblock->nTime;
        int64_t prevTime = pindexLast->GetBlockTime();
        
        // If more than 2x target spacing has passed, allow min difficulty
        if (blockTime > prevTime + params.nPowTargetSpacing * 2) {
            return BigToCompact(params.powLimit);
        }
        
        // Otherwise, return the last non-special-min-difficulty block's nBits
        // Walk back through the chain to find a block that wasn't min difficulty
        const BlockIndex* pindex = pindexLast;
        while (pindex->pprev != nullptr && 
               pindex->nHeight % difficultyAdjustmentInterval != 0 &&
               pindex->nBits == BigToCompact(params.powLimit)) {
            pindex = pindex->pprev;
        }
        return pindex->nBits;
    }
    
    // Not at retarget interval - keep the same difficulty
    if (nextHeight % difficultyAdjustmentInterval != 0) {
        return pindexLast->nBits;
    }
    
    // Find the first block of this retarget period
    // We need to go back DifficultyAdjustmentInterval blocks
    int heightFirst = nextHeight - static_cast<int>(difficultyAdjustmentInterval);
    if (heightFirst < 0) {
        heightFirst = 0;
    }
    
    const BlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < static_cast<int>(difficultyAdjustmentInterval) - 1; ++i) {
        pindexFirst = pindexFirst->pprev;
    }
    
    if (pindexFirst == nullptr) {
        // Not enough blocks yet, keep current difficulty
        return pindexLast->nBits;
    }
    
    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

/// Calculate new difficulty target based on past block times
/// Uses a modified DAA (Difficulty Adjustment Algorithm) based on Bitcoin's approach.
uint32_t CalculateNextWorkRequired(const BlockIndex* pindexLast,
                                   int64_t nFirstBlockTime,
                                   const Params& params) {
    if (params.fPowNoRetargeting) {
        return pindexLast->nBits;
    }
    
    // Calculate actual timespan
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    
    // Limit adjustment to 4x in either direction
    int64_t nTargetTimespan = params.nPowTargetTimespan;
    if (nActualTimespan < nTargetTimespan / 4) {
        nActualTimespan = nTargetTimespan / 4;
    }
    if (nActualTimespan > nTargetTimespan * 4) {
        nActualTimespan = nTargetTimespan * 4;
    }
    
    // Get the current target
    Hash256 bnNew = CompactToBig(pindexLast->nBits);
    
    // newTarget = oldTarget * actualTimespan / targetTimespan
    // We need to do big integer multiplication and division
    // For simplicity, we'll use a scaling approach on the compact representation
    
    // Extract mantissa and exponent from current compact format
    uint32_t nOldBits = pindexLast->nBits;
    int nExponent = (nOldBits >> 24) & 0xFF;
    uint32_t nMantissa = nOldBits & 0x007FFFFF;
    
    // Scale the mantissa: newMantissa = mantissa * actualTimespan / targetTimespan
    // To avoid overflow, we may need to adjust the exponent
    uint64_t nScaledMantissa = static_cast<uint64_t>(nMantissa) * static_cast<uint64_t>(nActualTimespan);
    nScaledMantissa /= static_cast<uint64_t>(nTargetTimespan);
    
    // Normalize: ensure mantissa fits in 23 bits (0x007FFFFF max)
    // If mantissa overflows, shift right and increase exponent
    while (nScaledMantissa > 0x007FFFFF) {
        nScaledMantissa >>= 8;
        nExponent++;
    }
    
    // If mantissa underflows (too small), shift left and decrease exponent
    while (nScaledMantissa < 0x008000 && nExponent > 1) {
        nScaledMantissa <<= 8;
        nExponent--;
    }
    
    // Clamp exponent to valid range
    if (nExponent > 32) nExponent = 32;
    if (nExponent < 1) nExponent = 1;
    
    // Reassemble compact format
    uint32_t nNewBits = (static_cast<uint32_t>(nExponent) << 24) | 
                        (static_cast<uint32_t>(nScaledMantissa) & 0x007FFFFF);
    
    // Handle sign bit: if high bit of mantissa is set, we need to pad
    if (nScaledMantissa & 0x00800000) {
        nNewBits = ((nExponent + 1) << 24) | ((nScaledMantissa >> 8) & 0x007FFFFF);
    }
    
    // Check against pow limit (maximum target / minimum difficulty)
    Hash256 newTarget = CompactToBig(nNewBits);
    
    // Compare newTarget with powLimit (little-endian: compare from MSB byte 31 down)
    bool exceedsLimit = false;
    for (int i = 31; i >= 0; --i) {
        if (newTarget[i] > params.powLimit[i]) {
            exceedsLimit = true;
            break;
        } else if (newTarget[i] < params.powLimit[i]) {
            break;
        }
    }
    
    if (exceedsLimit) {
        return BigToCompact(params.powLimit);
    }
    
    return nNewBits;
}

// ============================================================================
// PoUW Verification
// ============================================================================

/// Verify that a block's useful work proof is valid
/// This validates the PoUW commitment in the block header
bool VerifyUsefulWork(const Block& block, const Params& /* params */) {
    // For MVP, we accept all blocks that meet the hash target
    // Full implementation would verify:
    // 1. The work computation is correct
    // 2. The work result is committed in the block
    // 3. The work meets minimum quality requirements
    
    // Check that the block has the expected structure
    if (block.vtx.empty()) {
        return false;
    }
    
    // The coinbase transaction should commit to useful work results
    // For now, we just check basic validity
    const Transaction& coinbase = *block.vtx[0];
    if (!coinbase.IsCoinBase()) {
        return false;
    }
    
    // Placeholder: accept all blocks
    // Real implementation would verify PoUW proofs
    return true;
}

/// Check if a solution to a computational problem is valid
bool VerifyPoUWSolution(const Hash256& problemHash,
                        const std::vector<uint8_t>& solution,
                        uint32_t /* difficulty */) {
    // Verify that the solution hash meets the difficulty target
    if (solution.empty()) {
        return false;
    }
    
    // Hash the solution with the problem hash
    SHA256 hasher;
    hasher.Write(problemHash.data(), problemHash.size());
    hasher.Write(solution.data(), solution.size());
    
    Hash256 solutionHash;
    hasher.Finalize(solutionHash.data());
    
    // For MVP, any non-zero solution is valid
    // Real implementation would check against difficulty
    for (size_t i = 0; i < solutionHash.size(); ++i) {
        if (solutionHash[i] != 0) {
            return true;
        }
    }
    
    return false;
}

} // namespace consensus
} // namespace shurium
