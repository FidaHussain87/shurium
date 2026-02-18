// SHURIUM - Validation Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/consensus/validation.h"
#include "shurium/core/merkle.h"
#include <set>
#include <sstream>

namespace shurium {
namespace consensus {

// ============================================================================
// ValidationState Implementation
// ============================================================================

std::string ValidationState::ToString() const {
    std::ostringstream ss;
    switch (mode_) {
        case Mode::VALID:
            ss << "Valid";
            break;
        case Mode::INVALID:
            ss << "Invalid: " << rejectReason_;
            if (!debugMessage_.empty()) {
                ss << " (" << debugMessage_ << ")";
            }
            break;
        case Mode::ERROR:
            ss << "Error: " << rejectReason_;
            break;
    }
    return ss.str();
}

// ============================================================================
// Block Header Validation
// ============================================================================

bool CheckBlockHeader(const Block& block, ValidationState& state, const Params& params) {
    // Check version
    if (block.nVersion < 1) {
        return state.Invalid("bad-version", "block version too low");
    }
    
    // Check that nBits is set (block is not null)
    if (block.nBits == 0) {
        return state.Invalid("bad-diffbits", "difficulty bits not set");
    }
    
    // Check proof-of-work matches claimed difficulty
    if (!CheckProofOfWork(block.GetHash(), block.nBits, params)) {
        return state.Invalid("high-hash", "proof of work failed");
    }
    
    return true;
}

// ============================================================================
// Block Validation
// ============================================================================

bool CheckBlock(const Block& block, ValidationState& state, const Params& params) {
    // First check the header
    if (!CheckBlockHeader(block, state, params)) {
        return false;
    }
    
    // A block must have at least one transaction (the coinbase)
    if (block.vtx.empty()) {
        return state.Invalid("bad-blk-length", "block has no transactions");
    }
    
    // Check block size limits
    size_t blockSize = block.GetTotalSize();
    if (blockSize > params.nMaxBlockSize) {
        return state.Invalid("bad-blk-length", "block too large");
    }
    
    // First transaction must be coinbase
    if (!block.vtx[0]->IsCoinBase()) {
        return state.Invalid("bad-cb-missing", "first transaction is not coinbase");
    }
    
    // Check that there's only one coinbase
    for (size_t i = 1; i < block.vtx.size(); ++i) {
        if (block.vtx[i]->IsCoinBase()) {
            return state.Invalid("bad-cb-multiple", "multiple coinbase transactions");
        }
    }
    
    // Check merkle root
    Hash256 computedMerkle = block.ComputeMerkleRoot();
    if (computedMerkle != block.hashMerkleRoot) {
        return state.Invalid("bad-txnmrklroot", "merkle root mismatch");
    }
    
    // Check all transactions
    unsigned int totalSigOps = 0;
    for (const auto& tx : block.vtx) {
        if (!CheckTransaction(*tx, state)) {
            return false;  // State already set by CheckTransaction
        }
        totalSigOps += GetTransactionSigOpCount(*tx);
    }
    
    // Check total signature operations
    if (totalSigOps > params.nMaxBlockSigOps) {
        return state.Invalid("bad-blk-sigops", "too many signature operations");
    }
    
    return true;
}

// ============================================================================
// Transaction Validation
// ============================================================================

bool CheckTransaction(const Transaction& tx, ValidationState& state) {
    // Basic checks that don't require context
    
    // Check for empty inputs
    if (tx.vin.empty()) {
        return state.Invalid("bad-txns-vin-empty", "transaction has no inputs");
    }
    
    // Check for empty outputs
    if (tx.vout.empty()) {
        return state.Invalid("bad-txns-vout-empty", "transaction has no outputs");
    }
    
    // Check output values
    Amount totalOut = 0;
    for (const auto& txout : tx.vout) {
        if (txout.nValue < 0) {
            return state.Invalid("bad-txns-vout-negative", "output value negative");
        }
        if (txout.nValue > MAX_MONEY) {
            return state.Invalid("bad-txns-vout-toolarge", "output value too large");
        }
        totalOut += txout.nValue;
        if (!MoneyRange(totalOut)) {
            return state.Invalid("bad-txns-txouttotal-toolarge", 
                               "total output value too large");
        }
    }
    
    // Check for duplicate inputs
    std::set<OutPoint> vInOutPoints;
    for (const auto& txin : tx.vin) {
        if (!vInOutPoints.insert(txin.prevout).second) {
            return state.Invalid("bad-txns-inputs-duplicate", "duplicate inputs");
        }
    }
    
    // Coinbase specific checks
    if (tx.IsCoinBase()) {
        // Coinbase script must be between 2 and 100 bytes
        size_t cbSize = tx.vin[0].scriptSig.size();
        if (cbSize < 2 || cbSize > 100) {
            return state.Invalid("bad-cb-length", 
                               "coinbase script wrong size");
        }
    } else {
        // Non-coinbase transactions must not have null inputs
        for (const auto& txin : tx.vin) {
            if (txin.prevout.IsNull()) {
                return state.Invalid("bad-txns-prevout-null", 
                                   "non-coinbase with null input");
            }
        }
    }
    
    return true;
}

// ============================================================================
// Contextual Validation
// ============================================================================

bool CheckBlockTime(const Block& block, int64_t nPrevBlockTime, ValidationState& state) {
    // Block timestamp must be greater than previous block
    if (static_cast<int64_t>(block.nTime) <= nPrevBlockTime) {
        return state.Invalid("time-too-old", "block timestamp too old");
    }
    
    // Block timestamp should not be too far in the future
    // (This would typically compare against adjusted network time)
    // For now, just ensure it's reasonable
    
    return true;
}

// ============================================================================
// Signature Operations Counting
// ============================================================================

unsigned int GetTransactionSigOpCount(const Transaction& tx) {
    unsigned int nSigOps = 0;
    
    for (const auto& txin : tx.vin) {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    
    for (const auto& txout : tx.vout) {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    
    return nSigOps;
}

unsigned int GetBlockSigOpCount(const Block& block) {
    unsigned int nSigOps = 0;
    
    for (const auto& tx : block.vtx) {
        nSigOps += GetTransactionSigOpCount(*tx);
    }
    
    return nSigOps;
}

} // namespace consensus
} // namespace shurium
