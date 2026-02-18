// SHURIUM - Finite Field Arithmetic
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements arithmetic over prime fields for Poseidon hash and ZK proofs
// Primary field: BN254 scalar field (used in Ethereum, zkSNARKs)

#ifndef SHURIUM_CRYPTO_FIELD_H
#define SHURIUM_CRYPTO_FIELD_H

#include <cstdint>
#include <array>
#include <string>
#include "shurium/core/types.h"

namespace shurium {

// ============================================================================
// 256-bit Unsigned Integer (for field arithmetic)
// ============================================================================

/// 256-bit unsigned integer represented as 4 x 64-bit limbs (little-endian)
class Uint256 {
public:
    static constexpr size_t NUM_LIMBS = 4;
    
    /// Limbs in little-endian order (limb[0] is least significant)
    std::array<uint64_t, NUM_LIMBS> limbs;
    
    /// Default constructor - zero
    constexpr Uint256() : limbs{0, 0, 0, 0} {}
    
    /// Construct from limbs (little-endian)
    constexpr Uint256(uint64_t l0, uint64_t l1, uint64_t l2, uint64_t l3)
        : limbs{l0, l1, l2, l3} {}
    
    /// Construct from single value
    explicit constexpr Uint256(uint64_t val) : limbs{val, 0, 0, 0} {}
    
    /// Construct from byte array (little-endian)
    explicit Uint256(const Byte* data, size_t len);
    
    /// Construct from hex string
    static Uint256 FromHex(const std::string& hex);
    
    /// Convert to hex string
    std::string ToHex() const;
    
    /// Convert to byte array (little-endian, 32 bytes)
    std::array<Byte, 32> ToBytes() const;
    
    /// Check if zero
    bool IsZero() const;
    
    /// Comparison operators
    bool operator==(const Uint256& other) const;
    bool operator!=(const Uint256& other) const;
    bool operator<(const Uint256& other) const;
    bool operator<=(const Uint256& other) const;
    bool operator>(const Uint256& other) const;
    bool operator>=(const Uint256& other) const;
    
    /// Bitwise operations
    Uint256 operator&(const Uint256& other) const;
    Uint256 operator|(const Uint256& other) const;
    Uint256 operator^(const Uint256& other) const;
    Uint256 operator~() const;
    Uint256 operator<<(int shift) const;
    Uint256 operator>>(int shift) const;
    
    /// Arithmetic operations (modular arithmetic done in FieldElement)
    static Uint256 Add(const Uint256& a, const Uint256& b, bool& carry);
    static Uint256 Sub(const Uint256& a, const Uint256& b, bool& borrow);
    static Uint256 Mul(const Uint256& a, const Uint256& b, Uint256& high);
};

// ============================================================================
// Field Element over BN254 scalar field
// ============================================================================

/// Element of the BN254 scalar field
/// p = 21888242871839275222246405745257275088548364400416034343698204186575808495617
/// This is a 254-bit prime
class FieldElement {
public:
    /// The BN254 scalar field modulus
    static const Uint256 MODULUS;
    
    /// R = 2^256 mod p (for Montgomery form)
    static const Uint256 R;
    
    /// R^2 mod p (for Montgomery conversion)
    static const Uint256 R2;
    
    /// -p^(-1) mod 2^64 (for Montgomery reduction)
    static const uint64_t INV;
    
    /// Internal value (in Montgomery form for efficient multiplication)
    Uint256 value;
    
    /// Default constructor - zero
    FieldElement();
    
    /// Construct from Uint256 (converts to Montgomery form)
    explicit FieldElement(const Uint256& val);
    
    /// Construct from uint64_t
    explicit FieldElement(uint64_t val);
    
    /// Zero element
    static FieldElement Zero();
    
    /// One element
    static FieldElement One();
    
    /// Convert from Montgomery form to standard representation
    Uint256 ToUint256() const;
    
    /// Convert to bytes (32 bytes, little-endian)
    std::array<Byte, 32> ToBytes() const;
    
    /// Construct from bytes
    static FieldElement FromBytes(const Byte* data, size_t len);
    
    /// Construct from hex string
    static FieldElement FromHex(const std::string& hex);
    
    /// Check if zero
    bool IsZero() const;
    
    /// Comparison
    bool operator==(const FieldElement& other) const;
    bool operator!=(const FieldElement& other) const;
    
    /// Field arithmetic
    FieldElement operator+(const FieldElement& other) const;
    FieldElement operator-(const FieldElement& other) const;
    FieldElement operator*(const FieldElement& other) const;
    FieldElement operator-() const;  // Negation
    
    FieldElement& operator+=(const FieldElement& other);
    FieldElement& operator-=(const FieldElement& other);
    FieldElement& operator*=(const FieldElement& other);
    
    /// Square (more efficient than multiply)
    FieldElement Square() const;
    
    /// Power (exponentiation)
    FieldElement Pow(const Uint256& exp) const;
    
    /// Inverse (returns 0 if this is 0)
    FieldElement Inverse() const;
    
    /// S-box for Poseidon: x^5
    FieldElement PoseidonSbox() const;

private:
    /// Montgomery multiplication
    static Uint256 MontMul(const Uint256& a, const Uint256& b);
    
    /// Montgomery reduction
    static Uint256 MontReduce(const Uint256& lo, const Uint256& hi);
    
    /// Modular addition
    static Uint256 ModAdd(const Uint256& a, const Uint256& b);
    
    /// Modular subtraction
    static Uint256 ModSub(const Uint256& a, const Uint256& b);
};

} // namespace shurium

#endif // SHURIUM_CRYPTO_FIELD_H
