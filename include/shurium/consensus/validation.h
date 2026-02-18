// SHURIUM - Validation Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines validation functions for blocks and transactions.

#ifndef SHURIUM_CONSENSUS_VALIDATION_H
#define SHURIUM_CONSENSUS_VALIDATION_H

#include "shurium/core/types.h"
#include "shurium/core/block.h"
#include "shurium/core/transaction.h"
#include "shurium/consensus/params.h"
#include <string>

namespace shurium {
namespace consensus {

// ============================================================================
// Validation State
// ============================================================================

/// Result state from validation operations
class ValidationState {
public:
    enum class Mode {
        VALID,      ///< Everything ok
        INVALID,    ///< Network rule violation
        ERROR       ///< Runtime error (disk space, database problems, etc.)
    };

private:
    Mode mode_ = Mode::VALID;
    std::string rejectReason_;
    std::string debugMessage_;

public:
    /// Check if state is valid
    bool IsValid() const { return mode_ == Mode::VALID; }
    
    /// Check if validation failed due to consensus rules
    bool IsInvalid() const { return mode_ == Mode::INVALID; }
    
    /// Check if validation failed due to runtime error
    bool IsError() const { return mode_ == Mode::ERROR; }
    
    /// Get the rejection reason code
    const std::string& GetRejectReason() const { return rejectReason_; }
    
    /// Get detailed debug message
    const std::string& GetDebugMessage() const { return debugMessage_; }
    
    /// Mark as invalid with reason
    bool Invalid(const std::string& rejectReason, 
                 const std::string& debugMessage = "") {
        mode_ = Mode::INVALID;
        rejectReason_ = rejectReason;
        debugMessage_ = debugMessage;
        return false;
    }
    
    /// Mark as error
    bool Error(const std::string& message) {
        mode_ = Mode::ERROR;
        rejectReason_ = message;
        return false;
    }
    
    /// Convert to string for logging
    std::string ToString() const;
};

// ============================================================================
// Block Validation
// ============================================================================

/// Check block header for validity (no context needed)
/// @param block Block to check
/// @param state Validation state (output)
/// @param params Consensus parameters
/// @return True if valid
bool CheckBlockHeader(const Block& block, ValidationState& state, const Params& params);

/// Check block for validity (no context needed)
/// This checks:
/// - Block has transactions
/// - First transaction is coinbase
/// - No other coinbase transactions
/// - Merkle root matches
/// - Block size within limits
/// - All transactions are valid
/// @param block Block to check
/// @param state Validation state (output)
/// @param params Consensus parameters
/// @return True if valid
bool CheckBlock(const Block& block, ValidationState& state, const Params& params);

// ============================================================================
// Transaction Validation
// ============================================================================

/// Check transaction for validity (no context needed)
/// This checks:
/// - Non-empty inputs and outputs
/// - Output values in valid range
/// - No duplicate inputs
/// - Coinbase script size limits
/// @param tx Transaction to check
/// @param state Validation state (output)
/// @return True if valid
bool CheckTransaction(const Transaction& tx, ValidationState& state);

// ============================================================================
// Contextual Validation (requires chain context)
// ============================================================================

/// Check if a transaction's inputs are available
/// @param tx Transaction to check
/// @param state Validation state (output)
/// @return True if all inputs are available
// bool CheckTxInputsAvailable(const Transaction& tx, ValidationState& state);

/// Check if block timestamp is valid in context
/// @param block Block to check
/// @param nPrevBlockTime Previous block timestamp
/// @param state Validation state (output)
/// @return True if timestamp is valid
bool CheckBlockTime(const Block& block, int64_t nPrevBlockTime, ValidationState& state);

// ============================================================================
// Size and Resource Limits
// ============================================================================

/// Get maximum allowed block size
/// @param params Consensus parameters
/// @return Maximum block size in bytes
inline uint32_t MaxBlockSize(const Params& params) {
    return params.nMaxBlockSize;
}

/// Get maximum signature operations per block
/// @param params Consensus parameters
/// @return Maximum sigops
inline uint32_t MaxBlockSigOps(const Params& params) {
    return params.nMaxBlockSigOps;
}

/// Count signature operations in a transaction
/// @param tx Transaction to count
/// @return Number of signature operations
unsigned int GetTransactionSigOpCount(const Transaction& tx);

/// Count signature operations in a block
/// @param block Block to count
/// @return Number of signature operations
unsigned int GetBlockSigOpCount(const Block& block);

} // namespace consensus
} // namespace shurium

#endif // SHURIUM_CONSENSUS_VALIDATION_H
