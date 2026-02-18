// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#pragma once

#include <shurium/core/serialize.h>
#include <shurium/core/types.h>
#include <shurium/crypto/sha256.h>
#include <shurium/crypto/ripemd160.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shurium {

// Forward declarations
class PublicKey;
class PrivateKey;

// ============================================================================
// secp256k1 Constants
// ============================================================================

namespace secp256k1 {
    /// Private key size (32 bytes)
    constexpr size_t PRIVATE_KEY_SIZE = 32;
    
    /// Uncompressed public key size (65 bytes: 0x04 + 32 bytes X + 32 bytes Y)
    constexpr size_t UNCOMPRESSED_PUBKEY_SIZE = 65;
    
    /// Compressed public key size (33 bytes: 0x02/0x03 + 32 bytes X)
    constexpr size_t COMPRESSED_PUBKEY_SIZE = 33;
    
    /// ECDSA signature size (DER encoded, typically 70-72 bytes)
    constexpr size_t MAX_SIGNATURE_SIZE = 72;
    
    /// Compact signature size (65 bytes: 1 byte header + 32 R + 32 S)
    constexpr size_t COMPACT_SIGNATURE_SIZE = 65;
    
    /// Schnorr signature size (64 bytes: 32 R + 32 S)
    constexpr size_t SCHNORR_SIGNATURE_SIZE = 64;
    
    /// Order n of the secp256k1 curve (as bytes)
    extern const std::array<uint8_t, 32> CURVE_ORDER;
}

// ============================================================================
// Hash160 - RIPEMD160(SHA256(x))
// ============================================================================

/// Compute Hash160 of data (using the Hash160 class from types.h)
Hash160 ComputeHash160(const uint8_t* data, size_t len);

inline Hash160 ComputeHash160(const std::vector<uint8_t>& data) {
    return ComputeHash160(data.data(), data.size());
}

// ============================================================================
// PublicKey
// ============================================================================

/**
 * A secp256k1 public key.
 * 
 * Can be in compressed (33 bytes) or uncompressed (65 bytes) format.
 * Compressed format is preferred for smaller transaction sizes.
 */
class PublicKey {
public:
    /// Maximum size (uncompressed)
    static constexpr size_t MAX_SIZE = secp256k1::UNCOMPRESSED_PUBKEY_SIZE;
    
    /// Compressed size
    static constexpr size_t COMPRESSED_SIZE = secp256k1::COMPRESSED_PUBKEY_SIZE;
    
    /// Default constructor - invalid/empty key
    PublicKey() : size_(0) { data_.fill(0); }
    
    /// Construct from raw bytes
    explicit PublicKey(const uint8_t* data, size_t len);
    
    /// Construct from vector
    explicit PublicKey(const std::vector<uint8_t>& data) 
        : PublicKey(data.data(), data.size()) {}
    
    /// Construct from array (compressed)
    explicit PublicKey(const std::array<uint8_t, COMPRESSED_SIZE>& data)
        : PublicKey(data.data(), data.size()) {}
    
    /// Check if key is valid
    bool IsValid() const;
    
    /// Check if key is compressed
    bool IsCompressed() const { return size_ == COMPRESSED_SIZE; }
    
    /// Get size in bytes
    size_t size() const { return size_; }
    
    /// Get raw data
    const uint8_t* data() const { return data_.data(); }
    const uint8_t* begin() const { return data_.data(); }
    const uint8_t* end() const { return data_.data() + size_; }
    
    /// Convert to vector
    std::vector<uint8_t> ToVector() const {
        return std::vector<uint8_t>(begin(), end());
    }
    
    /// Get compressed form
    PublicKey GetCompressed() const;
    
    /// Get uncompressed form
    PublicKey GetUncompressed() const;
    
    /// Compute Hash160 (for address derivation)
    Hash160 GetHash160() const;
    
    /// Get key ID (same as Hash160)
    Hash160 GetID() const { return GetHash160(); }
    
    /// Verify ECDSA signature
    bool Verify(const Hash256& hash, const std::vector<uint8_t>& signature) const;
    
    /// Verify BIP340 Schnorr signature (64 bytes)
    /// Uses x-only public key format internally
    bool VerifySchnorr(const Hash256& hash, const std::array<uint8_t, 64>& signature) const;
    
    /// Get x-only public key (32 bytes) for BIP340/Taproot
    /// Returns just the X coordinate (Y is implicitly even)
    std::array<uint8_t, 32> GetXOnly() const;
    
    /// Recover public key from compact signature
    static std::optional<PublicKey> RecoverCompact(const Hash256& hash,
                                                    const std::vector<uint8_t>& signature);
    
    /// Comparison operators
    bool operator==(const PublicKey& other) const;
    bool operator!=(const PublicKey& other) const { return !(*this == other); }
    bool operator<(const PublicKey& other) const;
    
    /// Convert to hex string
    std::string ToHex() const;
    
    /// Parse from hex string
    static std::optional<PublicKey> FromHex(const std::string& hex);
    
    /// Serialization
    template<typename Stream>
    void Serialize(Stream& s) const {
        uint8_t len = static_cast<uint8_t>(size_);
        ::shurium::Serialize(s, len);
        s.Write(reinterpret_cast<const char*>(data_.data()), size_);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        uint8_t len;
        ::shurium::Unserialize(s, len);
        if (len > MAX_SIZE) {
            throw std::runtime_error("PublicKey too large");
        }
        size_ = len;
        data_.fill(0);
        s.Read(reinterpret_cast<char*>(data_.data()), len);
    }
    
private:
    std::array<uint8_t, MAX_SIZE> data_;
    uint8_t size_{0};
};

// ============================================================================
// PrivateKey
// ============================================================================

/**
 * A secp256k1 private key.
 * 
 * Always exactly 32 bytes. Must be in the valid range [1, n-1] where n
 * is the order of the secp256k1 curve.
 */
class PrivateKey {
public:
    /// Size in bytes
    static constexpr size_t SIZE = secp256k1::PRIVATE_KEY_SIZE;
    
    /// Default constructor - invalid key
    PrivateKey() : valid_(false), compressed_(true) { data_.fill(0); }
    
    /// Construct from raw 32 bytes
    explicit PrivateKey(const uint8_t* data, bool compressed = true);
    
    /// Construct from array
    explicit PrivateKey(const std::array<uint8_t, SIZE>& data, bool compressed = true)
        : PrivateKey(data.data(), compressed) {}
    
    /// Construct from vector
    explicit PrivateKey(const std::vector<uint8_t>& data, bool compressed = true)
        : PrivateKey(data.data(), compressed) {
        if (data.size() != SIZE) {
            valid_ = false;
        }
    }
    
    /// Destructor - securely clear memory
    ~PrivateKey();
    
    /// Move constructor
    PrivateKey(PrivateKey&& other) noexcept;
    
    /// Move assignment
    PrivateKey& operator=(PrivateKey&& other) noexcept;
    
    /// Copy constructor (allowed but discouraged for security)
    PrivateKey(const PrivateKey& other);
    
    /// Copy assignment
    PrivateKey& operator=(const PrivateKey& other);
    
    /// Generate a new random private key
    static PrivateKey Generate(bool compressed = true);
    
    /// Check if key is valid
    bool IsValid() const { return valid_; }
    
    /// Check if compressed public keys should be used
    bool IsCompressed() const { return compressed_; }
    
    /// Set compression flag
    void SetCompressed(bool compressed) { compressed_ = compressed; }
    
    /// Get raw data (const only for security)
    const uint8_t* data() const { return data_.data(); }
    const uint8_t* begin() const { return data_.data(); }
    const uint8_t* end() const { return data_.data() + SIZE; }
    
    /// Get size
    static constexpr size_t size() { return SIZE; }
    
    /// Derive public key
    PublicKey GetPublicKey() const;
    
    /// Sign a 32-byte hash (ECDSA)
    /// Returns DER-encoded signature
    std::vector<uint8_t> Sign(const Hash256& hash) const;
    
    /// Sign a 32-byte hash (compact/recoverable signature)
    /// Returns 65-byte signature with recovery ID
    std::vector<uint8_t> SignCompact(const Hash256& hash) const;
    
    /// Sign a 32-byte hash (Schnorr/BIP340)
    /// Returns 64-byte signature
    std::array<uint8_t, 64> SignSchnorr(const Hash256& hash) const;
    
    /// Negate the key (for certain Taproot operations)
    PrivateKey Negate() const;
    
    /// Tweak key by adding a scalar
    std::optional<PrivateKey> TweakAdd(const Hash256& tweak) const;
    
    /// Comparison (constant-time)
    bool operator==(const PrivateKey& other) const;
    bool operator!=(const PrivateKey& other) const { return !(*this == other); }
    
    /// Convert to WIF (Wallet Import Format)
    std::string ToWIF(uint8_t versionByte = 0x80) const;
    
    /// Parse from WIF
    static std::optional<PrivateKey> FromWIF(const std::string& wif);
    
    /// Convert to hex (use with caution!)
    std::string ToHex() const;
    
    /// Parse from hex
    static std::optional<PrivateKey> FromHex(const std::string& hex);
    
    /// Clear and invalidate the key
    void Clear();
    
private:
    std::array<uint8_t, SIZE> data_;
    bool valid_{false};
    bool compressed_{true};
    
    /// Validate that key is in valid range
    bool Validate() const;
};

// ============================================================================
// KeyPair - Combined private and public key
// ============================================================================

/**
 * A key pair containing both private and public keys.
 * 
 * Useful for signing operations where both keys are needed.
 */
class KeyPair {
public:
    /// Default constructor - invalid
    KeyPair() = default;
    
    /// Construct from private key
    explicit KeyPair(const PrivateKey& priv);
    
    /// Construct from private key (move)
    explicit KeyPair(PrivateKey&& priv);
    
    /// Generate a new random key pair
    static KeyPair Generate(bool compressed = true);
    
    /// Check if valid
    bool IsValid() const { return privateKey_.IsValid(); }
    
    /// Get private key
    const PrivateKey& GetPrivateKey() const { return privateKey_; }
    
    /// Get public key
    const PublicKey& GetPublicKey() const { return publicKey_; }
    
    /// Sign a hash
    std::vector<uint8_t> Sign(const Hash256& hash) const {
        return privateKey_.Sign(hash);
    }
    
    /// Verify a signature
    bool Verify(const Hash256& hash, const std::vector<uint8_t>& sig) const {
        return publicKey_.Verify(hash, sig);
    }
    
private:
    PrivateKey privateKey_;
    PublicKey publicKey_;
};

// ============================================================================
// Address Types
// ============================================================================

/// Address version bytes for SHURIUM
namespace AddressVersion {
    constexpr uint8_t MAINNET_PUBKEY = 0x3C;   // 'N' prefix
    constexpr uint8_t MAINNET_SCRIPT = 0x3D;   // 'N' or 'P' prefix
    constexpr uint8_t TESTNET_PUBKEY = 0x6F;   // 'm' or 'n' prefix
    constexpr uint8_t TESTNET_SCRIPT = 0xC4;   // '2' prefix
    constexpr uint8_t WIF_MAINNET = 0xBC;      // SHURIUM WIF prefix
    constexpr uint8_t WIF_TESTNET = 0xEF;      // Testnet WIF prefix
}

/// Human-readable part for Bech32 addresses
namespace Bech32HRP {
    constexpr const char* MAINNET = "nx";
    constexpr const char* TESTNET = "tnx";
}

/**
 * SHURIUM address types.
 */
enum class AddressType {
    P2PKH,      ///< Pay to Public Key Hash (legacy)
    P2SH,       ///< Pay to Script Hash
    P2WPKH,     ///< Pay to Witness Public Key Hash (SegWit v0)
    P2WSH,      ///< Pay to Witness Script Hash (SegWit v0)
    P2TR,       ///< Pay to Taproot (SegWit v1)
    INVALID
};

/**
 * Encode a P2PKH address from a public key.
 */
std::string EncodeP2PKH(const PublicKey& pubkey, bool testnet = false);

/**
 * Encode a P2PKH address from a key hash.
 */
std::string EncodeP2PKH(const Hash160& hash, bool testnet = false);

/**
 * Encode a P2WPKH (SegWit) address from a public key.
 */
std::string EncodeP2WPKH(const PublicKey& pubkey, bool testnet = false);

/**
 * Encode a P2WPKH address from a key hash.
 */
std::string EncodeP2WPKH(const Hash160& hash, bool testnet = false);

/**
 * Decode an address and return the script pubkey.
 * Returns empty vector if invalid.
 */
std::vector<uint8_t> DecodeAddress(const std::string& address);

/**
 * Get the type of an address.
 */
AddressType GetAddressType(const std::string& address);

// ============================================================================
// Base58Check Encoding
// ============================================================================

/**
 * Encode data with Base58Check (version + payload + 4-byte checksum).
 */
std::string EncodeBase58Check(const std::vector<uint8_t>& data);

/**
 * Decode Base58Check encoded string.
 * Returns empty vector if checksum fails.
 */
std::vector<uint8_t> DecodeBase58Check(const std::string& str);

/**
 * Encode raw bytes as Base58 (no checksum).
 */
std::string EncodeBase58(const std::vector<uint8_t>& data);

/**
 * Decode Base58 string to bytes.
 */
std::vector<uint8_t> DecodeBase58(const std::string& str);

// ============================================================================
// Bech32/Bech32m Encoding
// ============================================================================

/**
 * Encode data as Bech32.
 * @param hrp Human-readable part (e.g., "nx" for SHURIUM mainnet)
 * @param version Witness version (0 for P2WPKH/P2WSH, 1 for P2TR)
 * @param data Program data (20 bytes for P2WPKH, 32 bytes for P2WSH/P2TR)
 */
std::string EncodeBech32(const std::string& hrp, uint8_t version,
                          const std::vector<uint8_t>& data);

/**
 * Encode data as Bech32m (for witness version 1+).
 */
std::string EncodeBech32m(const std::string& hrp, uint8_t version,
                           const std::vector<uint8_t>& data);

/**
 * Decode a Bech32/Bech32m address.
 * Returns (hrp, version, program) or nullopt if invalid.
 */
std::optional<std::tuple<std::string, uint8_t, std::vector<uint8_t>>>
DecodeBech32(const std::string& str);

} // namespace shurium
