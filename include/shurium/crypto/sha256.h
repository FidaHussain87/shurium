// SHURIUM - SHA256 Hash Function
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// SHA-256 implementation following FIPS 180-4

#ifndef SHURIUM_CRYPTO_SHA256_H
#define SHURIUM_CRYPTO_SHA256_H

#include <cstdint>
#include <cstddef>
#include "shurium/core/types.h"

namespace shurium {

/// SHA-256 hasher class
/// Provides incremental hashing capability following Bitcoin's design pattern
class SHA256 {
public:
    /// Output size in bytes
    static constexpr size_t OUTPUT_SIZE = 32;
    
    /// Block size in bytes
    static constexpr size_t BLOCK_SIZE = 64;
    
    /// Default constructor - initializes to empty state
    SHA256();
    
    /// Write data to the hasher
    /// @param data Pointer to input data
    /// @param len Length of input data
    /// @return Reference to this hasher (for chaining)
    SHA256& Write(const Byte* data, size_t len);
    
    /// Finalize the hash and write to output
    /// @param hash Pointer to output buffer (must be at least OUTPUT_SIZE bytes)
    void Finalize(Byte hash[OUTPUT_SIZE]);
    
    /// Reset hasher to initial state
    /// @return Reference to this hasher (for chaining)
    SHA256& Reset();

private:
    /// Internal state (8 x 32-bit words)
    uint32_t state_[8];
    
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

/// Compute SHA256 hash of data in a single call
/// @param data Input data
/// @param len Length of input
/// @return Hash256 containing the result
Hash256 SHA256Hash(const Byte* data, size_t len);

/// Compute SHA256 hash of a vector
inline Hash256 SHA256Hash(const std::vector<Byte>& data) {
    return SHA256Hash(data.data(), data.size());
}

/// Compute double SHA256 (SHA256(SHA256(data)))
/// This is commonly used in Bitcoin for transaction/block hashing
/// @param data Input data
/// @param len Length of input
/// @return Hash256 containing the result
Hash256 DoubleSHA256(const Byte* data, size_t len);

/// Compute double SHA256 of a vector
inline Hash256 DoubleSHA256(const std::vector<Byte>& data) {
    return DoubleSHA256(data.data(), data.size());
}

} // namespace shurium

#endif // SHURIUM_CRYPTO_SHA256_H
