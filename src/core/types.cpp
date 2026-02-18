// SHURIUM - Core Types Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/core/types.h"
#include "shurium/core/hex.h"

namespace shurium {

// ============================================================================
// BaseHash Implementation
// ============================================================================

template<size_t BITS>
std::string BaseHash<BITS>::ToHex() const {
    // Display in reverse byte order (big-endian display for hashes)
    std::string result;
    result.reserve(SIZE * 2);
    
    static const char hexChars[] = "0123456789abcdef";
    
    for (int i = SIZE - 1; i >= 0; --i) {
        result.push_back(hexChars[data_[i] >> 4]);
        result.push_back(hexChars[data_[i] & 0x0F]);
    }
    
    return result;
}

template<size_t BITS>
BaseHash<BITS> BaseHash<BITS>::FromHex(const std::string& hex) {
    if (hex.length() != SIZE * 2) {
        throw std::invalid_argument("Invalid hex string length for hash");
    }
    
    BaseHash result;
    
    auto hexCharToNibble = [](char c) -> Byte {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("Invalid hex character");
    };
    
    // Parse in reverse order (hex string is big-endian display)
    for (size_t i = 0; i < SIZE; ++i) {
        size_t hexIdx = (SIZE - 1 - i) * 2;
        Byte high = hexCharToNibble(hex[hexIdx]);
        Byte low = hexCharToNibble(hex[hexIdx + 1]);
        result.data_[i] = (high << 4) | low;
    }
    
    return result;
}

// Explicit template instantiations
template class BaseHash<256>;
template class BaseHash<512>;
template class BaseHash<160>;

// ============================================================================
// ValidationError Implementation
// ============================================================================

const char* ValidationErrorToString(ValidationError err) {
    switch (err) {
        case ValidationError::OK: return "OK";
        
        // Block errors
        case ValidationError::BLOCK_INVALID_HEADER: return "Invalid block header";
        case ValidationError::BLOCK_INVALID_MERKLE_ROOT: return "Invalid merkle root";
        case ValidationError::BLOCK_INVALID_TIMESTAMP: return "Invalid timestamp";
        case ValidationError::BLOCK_TOO_LARGE: return "Block too large";
        case ValidationError::BLOCK_DUPLICATE: return "Duplicate block";
        
        // Transaction errors
        case ValidationError::TX_INVALID_FORMAT: return "Invalid transaction format";
        case ValidationError::TX_DOUBLE_SPEND: return "Double spend detected";
        case ValidationError::TX_INSUFFICIENT_FUNDS: return "Insufficient funds";
        case ValidationError::TX_INVALID_SIGNATURE: return "Invalid signature";
        case ValidationError::TX_FEE_TOO_LOW: return "Fee too low";
        
        // Work errors
        case ValidationError::WORK_INVALID_PROBLEM: return "Invalid problem";
        case ValidationError::WORK_INVALID_SOLUTION: return "Invalid solution";
        case ValidationError::WORK_VERIFICATION_FAILED: return "Work verification failed";
        case ValidationError::WORK_DUPLICATE_SUBMISSION: return "Duplicate work submission";
        
        // Identity errors
        case ValidationError::IDENTITY_INVALID_PROOF: return "Invalid identity proof";
        case ValidationError::IDENTITY_DUPLICATE: return "Duplicate identity";
        case ValidationError::IDENTITY_EXPIRED: return "Identity expired";
        
        // Economic errors
        case ValidationError::UBI_CLAIM_TOO_EARLY: return "UBI claim too early";
        case ValidationError::UBI_ALREADY_CLAIMED: return "UBI already claimed";
        case ValidationError::STABILITY_LIMIT_EXCEEDED: return "Stability limit exceeded";
        
        default: return "Unknown error";
    }
}

} // namespace shurium
