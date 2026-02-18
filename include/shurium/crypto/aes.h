// SHURIUM - AES Symmetric Encryption
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// AES-128/192/256 implementation with CBC and CTR modes.
// Uses OpenSSL when available, with pure C++ fallback.

#ifndef SHURIUM_CRYPTO_AES_H
#define SHURIUM_CRYPTO_AES_H

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <stdexcept>
#include "shurium/core/types.h"

namespace shurium {

// ============================================================================
// AES Constants
// ============================================================================

namespace aes {
    /// Block size is always 16 bytes for AES
    constexpr size_t BLOCK_SIZE = 16;
    
    /// Key sizes
    constexpr size_t KEY_SIZE_128 = 16;
    constexpr size_t KEY_SIZE_192 = 24;
    constexpr size_t KEY_SIZE_256 = 32;
    
    /// IV size (same as block size)
    constexpr size_t IV_SIZE = 16;
}

// ============================================================================
// AES Key Sizes Enum
// ============================================================================

enum class AESKeySize {
    AES_128 = 128,
    AES_192 = 192,
    AES_256 = 256
};

// ============================================================================
// AES Encryption Mode
// ============================================================================

enum class AESMode {
    CBC,    ///< Cipher Block Chaining (requires padding)
    CTR,    ///< Counter mode (stream cipher, no padding needed)
    ECB     ///< Electronic Codebook (not recommended, no IV)
};

// ============================================================================
// AES Context - Low-level encryption/decryption
// ============================================================================

/**
 * Low-level AES encryption context.
 * 
 * This class manages the key schedule and provides block-level operations.
 * For most uses, prefer the higher-level AESEncryptor/AESDecryptor classes.
 */
class AESContext {
public:
    /// Create AES context with key
    /// @param key Key bytes (16, 24, or 32 bytes for AES-128/192/256)
    /// @param keyLen Length of key in bytes
    explicit AESContext(const Byte* key, size_t keyLen);
    
    /// Create AES context with key array
    template<size_t N>
    explicit AESContext(const std::array<Byte, N>& key) 
        : AESContext(key.data(), N) {
        static_assert(N == 16 || N == 24 || N == 32, 
                      "AES key must be 16, 24, or 32 bytes");
    }
    
    /// Create AES context with key vector
    explicit AESContext(const std::vector<Byte>& key)
        : AESContext(key.data(), key.size()) {}
    
    /// Destructor - securely clear key schedule
    ~AESContext();
    
    /// Non-copyable
    AESContext(const AESContext&) = delete;
    AESContext& operator=(const AESContext&) = delete;
    
    /// Movable
    AESContext(AESContext&& other) noexcept;
    AESContext& operator=(AESContext&& other) noexcept;
    
    /// Encrypt a single 16-byte block
    /// @param input 16 bytes of plaintext
    /// @param output 16 bytes of ciphertext (can be same as input)
    void EncryptBlock(const Byte input[aes::BLOCK_SIZE], 
                      Byte output[aes::BLOCK_SIZE]) const;
    
    /// Decrypt a single 16-byte block
    /// @param input 16 bytes of ciphertext
    /// @param output 16 bytes of plaintext (can be same as input)
    void DecryptBlock(const Byte input[aes::BLOCK_SIZE], 
                      Byte output[aes::BLOCK_SIZE]) const;
    
    /// Get the key size being used
    AESKeySize GetKeySize() const { return keySize_; }
    
    /// Get number of rounds (10, 12, or 14 for AES-128/192/256)
    int GetRounds() const { return rounds_; }

private:
    /// Key schedule (expanded key)
    std::vector<uint32_t> encKey_;
    std::vector<uint32_t> decKey_;
    
    /// Key size enum
    AESKeySize keySize_;
    
    /// Number of rounds
    int rounds_;
    
    /// Expand key into key schedule
    void ExpandKey(const Byte* key, size_t keyLen);
};

// ============================================================================
// AES Encryptor - High-level encryption
// ============================================================================

/**
 * High-level AES encryptor supporting various modes.
 */
class AESEncryptor {
public:
    /// Create encryptor with key and mode
    /// @param key Encryption key
    /// @param keyLen Key length (16, 24, or 32)
    /// @param mode Encryption mode (CBC, CTR)
    /// @param iv Initialization vector (16 bytes, required for CBC/CTR)
    AESEncryptor(const Byte* key, size_t keyLen, 
                 AESMode mode, const Byte* iv = nullptr);
    
    /// Create encryptor with vector key
    AESEncryptor(const std::vector<Byte>& key, AESMode mode, 
                 const std::vector<Byte>& iv = {})
        : AESEncryptor(key.data(), key.size(), mode, 
                       iv.empty() ? nullptr : iv.data()) {}
    
    /// Destructor
    ~AESEncryptor();
    
    /// Encrypt data (streaming)
    /// @param input Plaintext bytes
    /// @param inputLen Length of plaintext
    /// @param output Output buffer (must be large enough)
    /// @return Number of bytes written to output
    size_t Update(const Byte* input, size_t inputLen, Byte* output);
    
    /// Finalize encryption (adds padding for CBC mode)
    /// @param output Output buffer (up to BLOCK_SIZE bytes)
    /// @return Number of bytes written
    size_t Finalize(Byte* output);
    
    /// Encrypt all data at once with PKCS7 padding
    /// @param plaintext Input data
    /// @return Ciphertext (padded for CBC mode)
    std::vector<Byte> Encrypt(const std::vector<Byte>& plaintext);
    
    /// Encrypt with separate IV output
    /// @param plaintext Input data
    /// @param iv Output: the IV used (16 bytes)
    /// @return Ciphertext
    std::vector<Byte> Encrypt(const std::vector<Byte>& plaintext,
                              std::array<Byte, aes::IV_SIZE>& iv);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// AES Decryptor - High-level decryption
// ============================================================================

/**
 * High-level AES decryptor supporting various modes.
 */
class AESDecryptor {
public:
    /// Create decryptor with key and mode
    /// @param key Decryption key
    /// @param keyLen Key length (16, 24, or 32)
    /// @param mode Decryption mode (CBC, CTR)
    /// @param iv Initialization vector (16 bytes)
    AESDecryptor(const Byte* key, size_t keyLen,
                 AESMode mode, const Byte* iv);
    
    /// Create decryptor with vector key
    AESDecryptor(const std::vector<Byte>& key, AESMode mode,
                 const std::vector<Byte>& iv)
        : AESDecryptor(key.data(), key.size(), mode, iv.data()) {}
    
    /// Destructor
    ~AESDecryptor();
    
    /// Decrypt data (streaming)
    /// @param input Ciphertext bytes
    /// @param inputLen Length of ciphertext
    /// @param output Output buffer
    /// @return Number of bytes written to output
    size_t Update(const Byte* input, size_t inputLen, Byte* output);
    
    /// Finalize decryption (removes padding for CBC mode)
    /// @param output Output buffer
    /// @return Number of bytes written
    size_t Finalize(Byte* output);
    
    /// Decrypt all data at once, removing PKCS7 padding
    /// @param ciphertext Input data
    /// @return Plaintext
    /// @throws std::runtime_error if padding is invalid
    std::vector<Byte> Decrypt(const std::vector<Byte>& ciphertext);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Encrypt data with AES-256-CBC.
 * 
 * @param plaintext Data to encrypt
 * @param key 32-byte encryption key
 * @param iv 16-byte initialization vector
 * @return Ciphertext with PKCS7 padding
 */
std::vector<Byte> AES256Encrypt(const std::vector<Byte>& plaintext,
                                 const std::array<Byte, aes::KEY_SIZE_256>& key,
                                 const std::array<Byte, aes::IV_SIZE>& iv);

/**
 * Decrypt data with AES-256-CBC.
 * 
 * @param ciphertext Data to decrypt
 * @param key 32-byte decryption key
 * @param iv 16-byte initialization vector
 * @return Plaintext with padding removed
 * @throws std::runtime_error if padding is invalid
 */
std::vector<Byte> AES256Decrypt(const std::vector<Byte>& ciphertext,
                                 const std::array<Byte, aes::KEY_SIZE_256>& key,
                                 const std::array<Byte, aes::IV_SIZE>& iv);

/**
 * Encrypt data with AES-256-CTR.
 * CTR mode produces ciphertext the same length as plaintext.
 * 
 * @param data Data to encrypt
 * @param key 32-byte encryption key
 * @param nonce 16-byte nonce/IV
 * @return Ciphertext (same length as input)
 */
std::vector<Byte> AES256CTREncrypt(const std::vector<Byte>& data,
                                    const std::array<Byte, aes::KEY_SIZE_256>& key,
                                    const std::array<Byte, aes::IV_SIZE>& nonce);

/**
 * Decrypt data with AES-256-CTR.
 * CTR decryption is identical to encryption.
 */
inline std::vector<Byte> AES256CTRDecrypt(const std::vector<Byte>& data,
                                           const std::array<Byte, aes::KEY_SIZE_256>& key,
                                           const std::array<Byte, aes::IV_SIZE>& nonce) {
    return AES256CTREncrypt(data, key, nonce);  // CTR is symmetric
}

// ============================================================================
// PKCS7 Padding Utilities
// ============================================================================

/**
 * Add PKCS7 padding to data.
 * Pads data to a multiple of the block size.
 * 
 * @param data Input data
 * @param blockSize Block size (default 16 for AES)
 * @return Padded data
 */
std::vector<Byte> PKCS7Pad(const std::vector<Byte>& data, 
                           size_t blockSize = aes::BLOCK_SIZE);

/**
 * Remove PKCS7 padding from data.
 * 
 * @param data Padded data
 * @return Unpadded data
 * @throws std::runtime_error if padding is invalid
 */
std::vector<Byte> PKCS7Unpad(const std::vector<Byte>& data);

/**
 * Validate PKCS7 padding.
 * 
 * @param data Potentially padded data
 * @return true if padding is valid
 */
bool PKCS7ValidatePadding(const std::vector<Byte>& data);

// ============================================================================
// Random IV Generation
// ============================================================================

/**
 * Generate a cryptographically secure random IV.
 * 
 * @return Random 16-byte IV
 */
std::array<Byte, aes::IV_SIZE> GenerateRandomIV();

} // namespace shurium

#endif // SHURIUM_CRYPTO_AES_H
