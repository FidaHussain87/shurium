// SHURIUM - RIPEMD160 Hash Function
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// RIPEMD-160 implementation

#ifndef SHURIUM_CRYPTO_RIPEMD160_H
#define SHURIUM_CRYPTO_RIPEMD160_H

#include <cstdint>
#include <cstddef>
#include "shurium/core/types.h"

namespace shurium {

/// RIPEMD-160 hasher class
/// Provides incremental hashing capability following Bitcoin's design pattern
class RIPEMD160 {
public:
    /// Output size in bytes
    static constexpr size_t OUTPUT_SIZE = 20;
    
    /// Block size in bytes
    static constexpr size_t BLOCK_SIZE = 64;
    
    /// Default constructor - initializes to empty state
    RIPEMD160();
    
    /// Write data to the hasher
    /// @param data Pointer to input data
    /// @param len Length of input data
    /// @return Reference to this hasher (for chaining)
    RIPEMD160& Write(const Byte* data, size_t len);
    
    /// Finalize the hash and write to output
    /// @param hash Pointer to output buffer (must be at least OUTPUT_SIZE bytes)
    void Finalize(Byte hash[OUTPUT_SIZE]);
    
    /// Reset hasher to initial state
    /// @return Reference to this hasher (for chaining)
    RIPEMD160& Reset();

private:
    /// Internal state (5 x 32-bit words)
    uint32_t state_[5];
    
    /// Buffer for partial block
    Byte buffer_[BLOCK_SIZE];
    
    /// Total bytes processed
    uint64_t bytes_;
    
    /// Transform a single 64-byte block
    void Transform(const Byte block[BLOCK_SIZE]);
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Compute RIPEMD160 hash of data in a single call
/// @param data Input data
/// @param len Length of input
/// @return Hash160 containing the result
Hash160 RIPEMD160Hash(const Byte* data, size_t len);

/// Compute RIPEMD160 hash of a vector
inline Hash160 RIPEMD160Hash(const std::vector<Byte>& data) {
    return RIPEMD160Hash(data.data(), data.size());
}

/// Compute Hash160 (RIPEMD160(SHA256(data)))
/// This is the standard address hash used in Bitcoin
/// @param data Input data
/// @param len Length of input
/// @return Hash160 containing the result
Hash160 Hash160FromData(const Byte* data, size_t len);

/// Compute Hash160 of a vector
inline Hash160 Hash160FromData(const std::vector<Byte>& data) {
    return Hash160FromData(data.data(), data.size());
}

} // namespace shurium

#endif // SHURIUM_CRYPTO_RIPEMD160_H
