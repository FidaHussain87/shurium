// SHURIUM - HMAC (Hash-based Message Authentication Code)
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// HMAC implementation following RFC 2104.
// Supports HMAC-SHA256 and HMAC-SHA512.

#ifndef SHURIUM_CRYPTO_HMAC_H
#define SHURIUM_CRYPTO_HMAC_H

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include "shurium/core/types.h"

namespace shurium {

// ============================================================================
// HMAC Constants
// ============================================================================

namespace hmac {
    /// HMAC-SHA256 output size
    constexpr size_t SHA256_SIZE = 32;
    
    /// HMAC-SHA512 output size
    constexpr size_t SHA512_SIZE = 64;
    
    /// SHA256 block size
    constexpr size_t SHA256_BLOCK_SIZE = 64;
    
    /// SHA512 block size
    constexpr size_t SHA512_BLOCK_SIZE = 128;
}

// ============================================================================
// HMAC-SHA256
// ============================================================================

/**
 * HMAC-SHA256 message authentication code.
 * 
 * Provides incremental hashing capability for streaming data.
 */
class HMAC_SHA256 {
public:
    /// Output size in bytes
    static constexpr size_t OUTPUT_SIZE = hmac::SHA256_SIZE;
    
    /// Block size in bytes
    static constexpr size_t BLOCK_SIZE = hmac::SHA256_BLOCK_SIZE;
    
    /// Create HMAC with key
    /// @param key Secret key bytes
    /// @param keyLen Length of key
    explicit HMAC_SHA256(const Byte* key, size_t keyLen);
    
    /// Create HMAC with vector key
    explicit HMAC_SHA256(const std::vector<Byte>& key)
        : HMAC_SHA256(key.data(), key.size()) {}
    
    /// Create HMAC with array key
    template<size_t N>
    explicit HMAC_SHA256(const std::array<Byte, N>& key)
        : HMAC_SHA256(key.data(), N) {}
    
    /// Destructor - securely clear key material
    ~HMAC_SHA256();
    
    /// Non-copyable (contains key material)
    HMAC_SHA256(const HMAC_SHA256&) = delete;
    HMAC_SHA256& operator=(const HMAC_SHA256&) = delete;
    
    /// Movable
    HMAC_SHA256(HMAC_SHA256&& other) noexcept;
    HMAC_SHA256& operator=(HMAC_SHA256&& other) noexcept;
    
    /// Write data to HMAC
    /// @param data Input data
    /// @param len Length of data
    /// @return Reference to this HMAC (for chaining)
    HMAC_SHA256& Write(const Byte* data, size_t len);
    
    /// Write vector data
    HMAC_SHA256& Write(const std::vector<Byte>& data) {
        return Write(data.data(), data.size());
    }
    
    /// Finalize and get the MAC
    /// @param mac Output buffer (32 bytes)
    void Finalize(Byte mac[OUTPUT_SIZE]);
    
    /// Finalize and get MAC as Hash256
    Hash256 Finalize();
    
    /// Reset to initial state (allows reuse with same key)
    HMAC_SHA256& Reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// HMAC-SHA512
// ============================================================================

/**
 * HMAC-SHA512 message authentication code.
 */
class HMAC_SHA512 {
public:
    /// Output size in bytes
    static constexpr size_t OUTPUT_SIZE = hmac::SHA512_SIZE;
    
    /// Block size in bytes
    static constexpr size_t BLOCK_SIZE = hmac::SHA512_BLOCK_SIZE;
    
    /// Create HMAC with key
    explicit HMAC_SHA512(const Byte* key, size_t keyLen);
    
    /// Create HMAC with vector key
    explicit HMAC_SHA512(const std::vector<Byte>& key)
        : HMAC_SHA512(key.data(), key.size()) {}
    
    /// Create HMAC with array key
    template<size_t N>
    explicit HMAC_SHA512(const std::array<Byte, N>& key)
        : HMAC_SHA512(key.data(), N) {}
    
    /// Destructor
    ~HMAC_SHA512();
    
    /// Non-copyable
    HMAC_SHA512(const HMAC_SHA512&) = delete;
    HMAC_SHA512& operator=(const HMAC_SHA512&) = delete;
    
    /// Movable
    HMAC_SHA512(HMAC_SHA512&& other) noexcept;
    HMAC_SHA512& operator=(HMAC_SHA512&& other) noexcept;
    
    /// Write data to HMAC
    HMAC_SHA512& Write(const Byte* data, size_t len);
    
    /// Write vector data
    HMAC_SHA512& Write(const std::vector<Byte>& data) {
        return Write(data.data(), data.size());
    }
    
    /// Finalize and get the MAC
    void Finalize(Byte mac[OUTPUT_SIZE]);
    
    /// Finalize and get MAC as Hash512
    Hash512 Finalize();
    
    /// Reset to initial state
    HMAC_SHA512& Reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Compute HMAC-SHA256 in one call.
 * 
 * @param key Secret key
 * @param keyLen Key length
 * @param data Data to authenticate
 * @param dataLen Data length
 * @return 32-byte MAC
 */
Hash256 ComputeHMAC_SHA256(const Byte* key, size_t keyLen,
                           const Byte* data, size_t dataLen);

/// Compute HMAC-SHA256 with vectors
inline Hash256 ComputeHMAC_SHA256(const std::vector<Byte>& key,
                                   const std::vector<Byte>& data) {
    return ComputeHMAC_SHA256(key.data(), key.size(), data.data(), data.size());
}

/// Compute HMAC-SHA256 with array key
template<size_t N>
inline Hash256 ComputeHMAC_SHA256(const std::array<Byte, N>& key,
                                   const std::vector<Byte>& data) {
    return ComputeHMAC_SHA256(key.data(), N, data.data(), data.size());
}

/**
 * Compute HMAC-SHA512 in one call.
 * 
 * @param key Secret key
 * @param keyLen Key length
 * @param data Data to authenticate
 * @param dataLen Data length
 * @return 64-byte MAC
 */
Hash512 ComputeHMAC_SHA512(const Byte* key, size_t keyLen,
                           const Byte* data, size_t dataLen);

/// Compute HMAC-SHA512 with vectors
inline Hash512 ComputeHMAC_SHA512(const std::vector<Byte>& key,
                                   const std::vector<Byte>& data) {
    return ComputeHMAC_SHA512(key.data(), key.size(), data.data(), data.size());
}

// ============================================================================
// HKDF (HMAC-based Key Derivation Function) - RFC 5869
// ============================================================================

/**
 * HKDF-Extract: Extract pseudorandom key from input keying material.
 * 
 * @param salt Optional salt (if empty, uses zeros)
 * @param ikm Input keying material
 * @return Pseudorandom key (32 bytes for SHA256)
 */
Hash256 HKDFExtract(const std::vector<Byte>& salt,
                    const std::vector<Byte>& ikm);

/**
 * HKDF-Expand: Expand pseudorandom key to desired length.
 * 
 * @param prk Pseudorandom key (from HKDFExtract)
 * @param info Context/application-specific info
 * @param length Desired output length (max 255 * 32 for SHA256)
 * @return Derived key material
 */
std::vector<Byte> HKDFExpand(const Hash256& prk,
                              const std::vector<Byte>& info,
                              size_t length);

/**
 * HKDF: Complete extract-then-expand operation.
 * 
 * @param salt Optional salt
 * @param ikm Input keying material
 * @param info Context info
 * @param length Output length
 * @return Derived key
 */
std::vector<Byte> HKDF(const std::vector<Byte>& salt,
                        const std::vector<Byte>& ikm,
                        const std::vector<Byte>& info,
                        size_t length);

/**
 * HKDF with SHA512.
 * Allows longer output keys.
 */
std::vector<Byte> HKDF_SHA512(const std::vector<Byte>& salt,
                               const std::vector<Byte>& ikm,
                               const std::vector<Byte>& info,
                               size_t length);

// ============================================================================
// PBKDF2 (Password-Based Key Derivation Function 2)
// ============================================================================

/**
 * PBKDF2 with HMAC-SHA256.
 * 
 * @param password User password
 * @param salt Random salt (should be at least 16 bytes)
 * @param iterations Number of iterations (recommend >= 100000)
 * @param keyLen Desired key length
 * @return Derived key
 */
std::vector<Byte> PBKDF2_SHA256(const std::string& password,
                                 const std::vector<Byte>& salt,
                                 uint32_t iterations,
                                 size_t keyLen);

/**
 * PBKDF2 with HMAC-SHA512.
 */
std::vector<Byte> PBKDF2_SHA512(const std::string& password,
                                 const std::vector<Byte>& salt,
                                 uint32_t iterations,
                                 size_t keyLen);

// ============================================================================
// Verification Helper
// ============================================================================

/**
 * Constant-time comparison of two MACs.
 * Prevents timing attacks.
 * 
 * @param a First MAC
 * @param b Second MAC
 * @param len Length to compare
 * @return true if equal
 */
bool ConstantTimeCompare(const Byte* a, const Byte* b, size_t len);

/// Compare two Hash256 values in constant time
inline bool ConstantTimeCompare(const Hash256& a, const Hash256& b) {
    return ConstantTimeCompare(a.data(), b.data(), Hash256::SIZE);
}

/// Compare two Hash512 values in constant time
inline bool ConstantTimeCompare(const Hash512& a, const Hash512& b) {
    return ConstantTimeCompare(a.data(), b.data(), Hash512::SIZE);
}

} // namespace shurium

#endif // SHURIUM_CRYPTO_HMAC_H
