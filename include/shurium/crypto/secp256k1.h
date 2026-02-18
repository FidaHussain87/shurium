// SHURIUM - secp256k1 Elliptic Curve Operations
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Low-level secp256k1 elliptic curve operations.
// For high-level key operations, see keys.h instead.

#ifndef SHURIUM_CRYPTO_SECP256K1_H
#define SHURIUM_CRYPTO_SECP256K1_H

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <optional>
#include "shurium/core/types.h"

namespace shurium {
namespace secp256k1 {

// ============================================================================
// Constants
// ============================================================================

/// Field size p (prime)
extern const std::array<uint8_t, 32> FIELD_PRIME;

/// Curve order n
extern const std::array<uint8_t, 32> CURVE_ORDER;

/// Generator point G (compressed)
extern const std::array<uint8_t, 33> GENERATOR_COMPRESSED;

/// Generator point G (uncompressed)
extern const std::array<uint8_t, 65> GENERATOR_UNCOMPRESSED;

/// Curve parameter a (= 0 for secp256k1)
constexpr uint32_t CURVE_A = 0;

/// Curve parameter b (= 7 for secp256k1)
constexpr uint32_t CURVE_B = 7;

// ============================================================================
// Scalar (256-bit integer mod n)
// ============================================================================

/**
 * A 256-bit scalar value (mod curve order n).
 * Used for private keys and scalar multiplications.
 */
class Scalar {
public:
    static constexpr size_t SIZE = 32;
    
    /// Default constructor (zero)
    Scalar();
    
    /// Construct from bytes (big-endian)
    explicit Scalar(const uint8_t* data);
    explicit Scalar(const std::array<uint8_t, SIZE>& data);
    explicit Scalar(const std::vector<uint8_t>& data);
    
    /// Destructor - securely clears memory
    ~Scalar();
    
    /// Check if scalar is zero
    bool IsZero() const;
    
    /// Check if scalar is valid (non-zero and < n)
    bool IsValid() const;
    
    /// Get raw bytes (big-endian)
    const uint8_t* data() const { return data_.data(); }
    
    /// Convert to bytes
    std::array<uint8_t, SIZE> ToBytes() const { return data_; }
    std::vector<uint8_t> ToVector() const {
        return std::vector<uint8_t>(data_.begin(), data_.end());
    }
    
    /// Arithmetic operations (mod n)
    Scalar operator+(const Scalar& other) const;
    Scalar operator-(const Scalar& other) const;
    Scalar operator*(const Scalar& other) const;
    Scalar operator-() const;  // Negate
    
    /// In-place operations
    Scalar& operator+=(const Scalar& other);
    Scalar& operator-=(const Scalar& other);
    Scalar& operator*=(const Scalar& other);
    
    /// Modular inverse (a^-1 mod n)
    Scalar Inverse() const;
    
    /// Comparison
    bool operator==(const Scalar& other) const;
    bool operator!=(const Scalar& other) const { return !(*this == other); }
    bool operator<(const Scalar& other) const;
    
    /// Set to zero
    void SetZero();
    
    /// Generate random scalar in [1, n-1]
    static Scalar Random();
    
    /// Create from integer
    static Scalar FromInt(uint64_t value);

private:
    std::array<uint8_t, SIZE> data_;
};

// ============================================================================
// Field Element (256-bit integer mod p)
// ============================================================================

/**
 * A field element (mod field prime p).
 * Used for point coordinates.
 */
class FieldElement {
public:
    static constexpr size_t SIZE = 32;
    
    /// Default constructor (zero)
    FieldElement();
    
    /// Construct from bytes (big-endian)
    explicit FieldElement(const uint8_t* data);
    explicit FieldElement(const std::array<uint8_t, SIZE>& data);
    
    /// Check if zero
    bool IsZero() const;
    
    /// Check if valid (< p)
    bool IsValid() const;
    
    /// Check if odd (for point compression)
    bool IsOdd() const;
    
    /// Get raw bytes
    const uint8_t* data() const { return data_.data(); }
    std::array<uint8_t, SIZE> ToBytes() const { return data_; }
    
    /// Arithmetic (mod p)
    FieldElement operator+(const FieldElement& other) const;
    FieldElement operator-(const FieldElement& other) const;
    FieldElement operator*(const FieldElement& other) const;
    FieldElement operator-() const;
    
    /// Square root (mod p) - returns nullopt if not a quadratic residue
    std::optional<FieldElement> Sqrt() const;
    
    /// Modular inverse
    FieldElement Inverse() const;
    
    /// Square
    FieldElement Square() const;
    
    /// Comparison
    bool operator==(const FieldElement& other) const;
    bool operator!=(const FieldElement& other) const { return !(*this == other); }

private:
    std::array<uint8_t, SIZE> data_;
    
    // Allow Point to access internals for decompression
    friend class Point;
};

// ============================================================================
// Point (secp256k1 curve point)
// ============================================================================

/**
 * A point on the secp256k1 curve.
 * Supports both affine and internal Jacobian representation.
 */
class Point {
public:
    /// Compressed point size
    static constexpr size_t COMPRESSED_SIZE = 33;
    
    /// Uncompressed point size
    static constexpr size_t UNCOMPRESSED_SIZE = 65;
    
    /// Default constructor - point at infinity
    Point();
    
    /// Construct from affine coordinates
    Point(const FieldElement& x, const FieldElement& y);
    
    /// Construct from compressed bytes (33 bytes: 02/03 || x)
    static std::optional<Point> FromCompressed(const uint8_t* data);
    static std::optional<Point> FromCompressed(const std::array<uint8_t, COMPRESSED_SIZE>& data);
    
    /// Construct from uncompressed bytes (65 bytes: 04 || x || y)
    static std::optional<Point> FromUncompressed(const uint8_t* data);
    static std::optional<Point> FromUncompressed(const std::array<uint8_t, UNCOMPRESSED_SIZE>& data);
    
    /// Parse from either format
    static std::optional<Point> FromBytes(const uint8_t* data, size_t len);
    static std::optional<Point> FromBytes(const std::vector<uint8_t>& data);
    
    /// Check if point at infinity
    bool IsInfinity() const;
    
    /// Check if point is on the curve
    bool IsOnCurve() const;
    
    /// Get X coordinate
    FieldElement GetX() const;
    
    /// Get Y coordinate  
    FieldElement GetY() const;
    
    /// Serialize to compressed form
    std::array<uint8_t, COMPRESSED_SIZE> ToCompressed() const;
    
    /// Serialize to uncompressed form
    std::array<uint8_t, UNCOMPRESSED_SIZE> ToUncompressed() const;
    
    /// Point addition
    Point operator+(const Point& other) const;
    
    /// Point subtraction
    Point operator-(const Point& other) const;
    
    /// Point negation
    Point operator-() const;
    
    /// Scalar multiplication (returns scalar * this)
    Point operator*(const Scalar& scalar) const;
    
    /// Point doubling
    Point Double() const;
    
    /// Comparison
    bool operator==(const Point& other) const;
    bool operator!=(const Point& other) const { return !(*this == other); }
    
    /// Get the generator point G
    static Point Generator();
    
    /// Get point at infinity
    static Point Infinity();
    
    /// Copy constructor
    Point(const Point& other);
    
    /// Copy assignment
    Point& operator=(const Point& other);
    
    /// Move constructor
    Point(Point&& other) noexcept;
    
    /// Move assignment
    Point& operator=(Point&& other) noexcept;
    
    /// Destructor
    ~Point();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    // For internal construction
    explicit Point(std::unique_ptr<Impl> impl);
    
    // Allow high-level functions to access impl_
    friend std::optional<Point> ScalarBaseMultiply(const Scalar& scalar);
    friend Point DoubleScalarMultiply(const Scalar& a, const Scalar& b, const Point& P);
};

// ============================================================================
// High-Level Operations
// ============================================================================

/**
 * Scalar multiplication: scalar * G (generator).
 * This is the key operation for deriving public keys.
 * 
 * @param scalar 32-byte scalar (private key)
 * @return Public point, or nullopt if scalar is invalid
 */
std::optional<Point> ScalarBaseMultiply(const Scalar& scalar);

/**
 * Scalar multiplication: scalar * point.
 * 
 * @param scalar Scalar value
 * @param point EC point
 * @return Result point
 */
Point ScalarMultiply(const Scalar& scalar, const Point& point);

/**
 * Double scalar multiplication: a*G + b*P.
 * More efficient than separate multiplications.
 * Used for signature verification.
 * 
 * @param a First scalar
 * @param b Second scalar
 * @param P Point (typically public key)
 * @return Resulting point
 */
Point DoubleScalarMultiply(const Scalar& a, const Scalar& b, const Point& P);

/**
 * Verify that a scalar is a valid private key.
 * Must be in range [1, n-1].
 * 
 * @param key 32-byte key to validate
 * @return true if valid
 */
bool IsValidPrivateKey(const uint8_t* key);
bool IsValidPrivateKey(const std::array<uint8_t, 32>& key);

/**
 * Verify that a point is a valid public key.
 * Must be on the curve and not at infinity.
 * 
 * @param point Serialized public key (33 or 65 bytes)
 * @param len Length of serialized data
 * @return true if valid
 */
bool IsValidPublicKey(const uint8_t* point, size_t len);
bool IsValidPublicKey(const std::vector<uint8_t>& point);

/**
 * Tweak a private key by adding a scalar.
 * result = (key + tweak) mod n
 * 
 * @param key Original private key (32 bytes)
 * @param tweak Tweak value (32 bytes)
 * @param result Output buffer (32 bytes)
 * @return true if result is valid, false if it would be zero
 */
bool PrivateKeyTweakAdd(const uint8_t* key, const uint8_t* tweak, uint8_t* result);

/**
 * Tweak a public key by adding tweak*G.
 * result = P + tweak*G
 * 
 * @param pubkey Public key (33 or 65 bytes)
 * @param pubkeyLen Length of public key
 * @param tweak Tweak value (32 bytes)
 * @param result Output buffer (same size as input)
 * @return true if successful
 */
bool PublicKeyTweakAdd(const uint8_t* pubkey, size_t pubkeyLen,
                       const uint8_t* tweak, uint8_t* result);

/**
 * Negate a private key.
 * result = -key mod n
 */
void PrivateKeyNegate(const uint8_t* key, uint8_t* result);

/**
 * Negate a public key (flip Y coordinate).
 */
bool PublicKeyNegate(const uint8_t* pubkey, size_t len, uint8_t* result);

/**
 * Combine (add) multiple public keys.
 * 
 * @param pubkeys Array of public key pointers
 * @param sizes Array of public key sizes
 * @param count Number of keys
 * @param result Output buffer (33 bytes compressed)
 * @return true if successful
 */
bool PublicKeyCombine(const uint8_t* const* pubkeys, const size_t* sizes,
                      size_t count, uint8_t* result);

// ============================================================================
// ECDSA Operations
// ============================================================================

/**
 * Sign a 32-byte message hash with ECDSA.
 * 
 * @param hash 32-byte message hash
 * @param privateKey 32-byte private key
 * @param signature Output: DER-encoded signature (max 72 bytes)
 * @param sigLen Output: actual signature length
 * @return true if successful
 */
bool ECDSASign(const uint8_t* hash, const uint8_t* privateKey,
               uint8_t* signature, size_t* sigLen);

/**
 * Verify an ECDSA signature.
 * 
 * @param hash 32-byte message hash
 * @param signature DER-encoded signature
 * @param sigLen Signature length
 * @param publicKey Public key (33 or 65 bytes)
 * @param pubkeyLen Public key length
 * @return true if signature is valid
 */
bool ECDSAVerify(const uint8_t* hash,
                 const uint8_t* signature, size_t sigLen,
                 const uint8_t* publicKey, size_t pubkeyLen);

/**
 * Sign with compact/recoverable signature format.
 * 
 * @param hash 32-byte message hash
 * @param privateKey 32-byte private key
 * @param signature Output: 65-byte compact signature (recid || r || s)
 * @return true if successful
 */
bool ECDSASignCompact(const uint8_t* hash, const uint8_t* privateKey,
                      uint8_t signature[65]);

/**
 * Recover public key from compact signature.
 * 
 * @param hash 32-byte message hash
 * @param signature 65-byte compact signature
 * @param publicKey Output: 33-byte compressed public key
 * @return true if recovery successful
 */
bool ECDSARecoverCompact(const uint8_t* hash, const uint8_t signature[65],
                         uint8_t publicKey[33]);

// ============================================================================
// Schnorr Signatures (BIP340)
// ============================================================================

/**
 * Sign with BIP340 Schnorr signature.
 * 
 * @param hash 32-byte message hash
 * @param privateKey 32-byte private key
 * @param signature Output: 64-byte Schnorr signature
 * @return true if successful
 */
bool SchnorrSign(const uint8_t* hash, const uint8_t* privateKey,
                 uint8_t signature[64]);

/**
 * Verify a BIP340 Schnorr signature.
 * 
 * @param hash 32-byte message hash
 * @param signature 64-byte signature
 * @param publicKey 32-byte x-only public key
 * @return true if valid
 */
bool SchnorrVerify(const uint8_t* hash, const uint8_t signature[64],
                   const uint8_t publicKey[32]);

} // namespace secp256k1
} // namespace shurium

#endif // SHURIUM_CRYPTO_SECP256K1_H
