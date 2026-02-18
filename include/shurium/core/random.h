// SHURIUM - Secure Random Number Generation Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file provides cryptographically secure random number generation
// using OS entropy sources.

#ifndef SHURIUM_CORE_RANDOM_H
#define SHURIUM_CORE_RANDOM_H

#include "shurium/core/types.h"
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <iterator>

namespace shurium {

// ============================================================================
// Core Random Functions
// ============================================================================

/// Fill buffer with cryptographically secure random bytes
/// Uses OS entropy source (getrandom on Linux, arc4random on macOS/BSD)
void GetRandBytes(uint8_t* buf, size_t len);

/// Fill buffer with random bytes (Span version)
inline void GetRandBytes(Span<uint8_t> buf) {
    GetRandBytes(buf.data(), buf.size());
}

/// Generate random 64-bit unsigned integer
uint64_t GetRandUint64();

/// Generate random 32-bit unsigned integer
uint32_t GetRandUint32();

/// Generate random integer in range [0, max)
/// Uses rejection sampling to avoid modulo bias
uint64_t GetRandInt(uint64_t max);

/// Generate random boolean
inline bool GetRandBool() {
    return GetRandInt(2) == 1;
}

// ============================================================================
// Random Hash Generation
// ============================================================================

/// Generate random 256-bit hash
Hash256 GetRandHash256();

/// Generate random 160-bit hash
Hash160 GetRandHash160();

// ============================================================================
// Shuffle
// ============================================================================

/// Shuffle a range using Fisher-Yates algorithm with secure randomness
template<typename RandomIt>
void Shuffle(RandomIt first, RandomIt last) {
    auto n = std::distance(first, last);
    for (auto i = n - 1; i > 0; --i) {
        auto j = static_cast<decltype(i)>(GetRandInt(static_cast<uint64_t>(i + 1)));
        std::swap(*(first + i), *(first + j));
    }
}

// ============================================================================
// Internal Entropy Functions (Platform-Specific)
// ============================================================================

namespace detail {

/// Get entropy from OS - implementation is platform-specific
/// Returns true on success, false on failure
bool GetOSEntropy(uint8_t* buf, size_t len);

/// Seed initial random state
void InitializeRNG();

} // namespace detail

} // namespace shurium

#endif // SHURIUM_CORE_RANDOM_H
