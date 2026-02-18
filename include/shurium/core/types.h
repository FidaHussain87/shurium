// SHURIUM - Core Types Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines fundamental types used throughout SHURIUM.

#ifndef SHURIUM_CORE_TYPES_H
#define SHURIUM_CORE_TYPES_H

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <cstring>

namespace shurium {

// ============================================================================
// Basic Types
// ============================================================================

/// Single byte type
using Byte = uint8_t;

/// Amount in smallest units (like satoshis)
using Amount = int64_t;

/// Timestamp (Unix epoch seconds)
using Timestamp = int64_t;

/// Constants
constexpr Amount COIN = 100000000LL;  // 1 NXS = 100 million base units
constexpr Amount MAX_MONEY = 21000000000LL * COIN;  // 21 billion NXS max

/// Check if amount is in valid range
inline bool MoneyRange(Amount value) {
    return value >= 0 && value <= MAX_MONEY;
}

// ============================================================================
// Time Functions
// ============================================================================

/// Get current Unix timestamp
inline Timestamp GetTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

/// Get current time in milliseconds
inline int64_t GetTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// ============================================================================
// Span - Non-owning view of contiguous memory
// ============================================================================

template<typename T>
class Span {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using size_type = std::size_t;
    
    constexpr Span() noexcept : data_(nullptr), size_(0) {}
    
    constexpr Span(pointer data, size_type size) noexcept 
        : data_(data), size_(size) {}
    
    template<size_t N>
    constexpr Span(std::array<T, N>& arr) noexcept 
        : data_(arr.data()), size_(N) {}
    
    template<size_t N>
    constexpr Span(const std::array<std::remove_const_t<T>, N>& arr) noexcept 
        : data_(arr.data()), size_(N) {}
    
    Span(std::vector<std::remove_const_t<T>>& vec) noexcept 
        : data_(vec.data()), size_(vec.size()) {}
    
    Span(const std::vector<std::remove_const_t<T>>& vec) noexcept 
        : data_(vec.data()), size_(vec.size()) {}
    
    constexpr pointer data() const noexcept { return data_; }
    constexpr size_type size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }
    
    constexpr reference operator[](size_type idx) const { return data_[idx]; }
    
    constexpr iterator begin() const noexcept { return data_; }
    constexpr iterator end() const noexcept { return data_ + size_; }
    
    constexpr Span subspan(size_type offset, size_type count) const {
        return Span(data_ + offset, count);
    }
    
    constexpr Span first(size_type count) const {
        return Span(data_, count);
    }
    
    constexpr Span last(size_type count) const {
        return Span(data_ + size_ - count, count);
    }

private:
    pointer data_;
    size_type size_;
};

// ============================================================================
// Hash Templates
// ============================================================================

/// Generic hash template
template<size_t BITS>
class BaseHash {
public:
    static constexpr size_t SIZE = BITS / 8;
    
    /// Default constructor - creates null hash
    BaseHash() noexcept {
        data_.fill(0);
    }
    
    /// Construct from byte array
    explicit BaseHash(const std::array<Byte, SIZE>& data) noexcept 
        : data_(data) {}
    
    /// Construct from raw bytes
    BaseHash(const Byte* data, size_t len) noexcept {
        if (len >= SIZE) {
            std::memcpy(data_.data(), data, SIZE);
        } else {
            data_.fill(0);
            if (data && len > 0) {
                std::memcpy(data_.data(), data, len);
            }
        }
    }
    
    /// Check if hash is all zeros
    bool IsNull() const noexcept {
        for (auto b : data_) {
            if (b != 0) return false;
        }
        return true;
    }
    
    /// Set hash to all zeros
    void SetNull() noexcept {
        data_.fill(0);
    }
    
    /// Size in bytes
    constexpr size_t size() const noexcept { return SIZE; }
    
    /// Element access
    Byte& operator[](size_t idx) { return data_[idx]; }
    const Byte& operator[](size_t idx) const { return data_[idx]; }
    
    /// Raw data access
    Byte* data() noexcept { return data_.data(); }
    const Byte* data() const noexcept { return data_.data(); }
    
    /// Iterators
    Byte* begin() noexcept { return data_.data(); }
    const Byte* begin() const noexcept { return data_.data(); }
    Byte* end() noexcept { return data_.data() + SIZE; }
    const Byte* end() const noexcept { return data_.data() + SIZE; }
    
    /// Comparison operators
    bool operator==(const BaseHash& other) const noexcept {
        return data_ == other.data_;
    }
    
    bool operator!=(const BaseHash& other) const noexcept {
        return !(*this == other);
    }
    
    bool operator<(const BaseHash& other) const noexcept {
        // Compare in reverse order (most significant byte last in storage)
        for (int i = SIZE - 1; i >= 0; --i) {
            if (data_[i] < other.data_[i]) return true;
            if (data_[i] > other.data_[i]) return false;
        }
        return false;
    }
    
    /// Convert to hex string (displayed in reverse byte order)
    std::string ToHex() const;
    
    /// Create from hex string
    static BaseHash FromHex(const std::string& hex);

protected:
    std::array<Byte, SIZE> data_;
};

// ============================================================================
// Specific Hash Types
// ============================================================================

/// 256-bit hash (32 bytes)
class Hash256 : public BaseHash<256> {
public:
    using BaseHash<256>::BaseHash;
    
    static Hash256 FromHex(const std::string& hex) {
        return Hash256(BaseHash<256>::FromHex(hex).data(), SIZE);
    }
};

/// 512-bit hash (64 bytes)
class Hash512 : public BaseHash<512> {
public:
    using BaseHash<512>::BaseHash;
};

/// 160-bit hash (20 bytes) - for addresses
class Hash160 : public BaseHash<160> {
public:
    using BaseHash<160>::BaseHash;
    
    static Hash160 FromHex(const std::string& hex) {
        auto base = BaseHash<160>::FromHex(hex);
        return Hash160(base.data(), SIZE);
    }
};

// ============================================================================
// Type-safe Hash Aliases
// ============================================================================

/// Block hash (256-bit)
class BlockHash : public Hash256 {
public:
    using Hash256::Hash256;
    BlockHash() = default;
    explicit BlockHash(const Hash256& h) : Hash256(h) {}
};

/// Transaction hash (256-bit)
class TxHash : public Hash256 {
public:
    using Hash256::Hash256;
    TxHash() = default;
    explicit TxHash(const Hash256& h) : Hash256(h) {}
};

/// Problem hash (256-bit)
class ProblemHash : public Hash256 {
public:
    using Hash256::Hash256;
    ProblemHash() = default;
    explicit ProblemHash(const Hash256& h) : Hash256(h) {}
};

/// Identity hash (256-bit)
class IdentityHash : public Hash256 {
public:
    using Hash256::Hash256;
    IdentityHash() = default;
    explicit IdentityHash(const Hash256& h) : Hash256(h) {}
};

// ============================================================================
// CompactSize Encoding
// ============================================================================

/// Get size of compact size encoding for a value
inline size_t GetCompactSizeSize(uint64_t value) {
    if (value < 253) return 1;
    if (value <= 0xFFFF) return 3;
    if (value <= 0xFFFFFFFF) return 5;
    return 9;
}

// ============================================================================
// Result Type
// ============================================================================

/// Validation error codes
enum class ValidationError {
    OK = 0,
    
    // Block errors
    BLOCK_INVALID_HEADER,
    BLOCK_INVALID_MERKLE_ROOT,
    BLOCK_INVALID_TIMESTAMP,
    BLOCK_TOO_LARGE,
    BLOCK_DUPLICATE,
    
    // Transaction errors
    TX_INVALID_FORMAT,
    TX_DOUBLE_SPEND,
    TX_INSUFFICIENT_FUNDS,
    TX_INVALID_SIGNATURE,
    TX_FEE_TOO_LOW,
    
    // Work errors
    WORK_INVALID_PROBLEM,
    WORK_INVALID_SOLUTION,
    WORK_VERIFICATION_FAILED,
    WORK_DUPLICATE_SUBMISSION,
    
    // Identity errors
    IDENTITY_INVALID_PROOF,
    IDENTITY_DUPLICATE,
    IDENTITY_EXPIRED,
    
    // Economic errors
    UBI_CLAIM_TOO_EARLY,
    UBI_ALREADY_CLAIMED,
    STABILITY_LIMIT_EXCEEDED,
};

/// Convert error to string
const char* ValidationErrorToString(ValidationError err);

} // namespace shurium

#endif // SHURIUM_CORE_TYPES_H
