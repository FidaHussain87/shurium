// SHURIUM - Finite Field Arithmetic Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements Montgomery arithmetic over the BN254 scalar field

#include "shurium/crypto/field.h"
#include <cstring>
#include <stdexcept>

namespace shurium {

// ============================================================================
// BN254 Scalar Field Constants
// ============================================================================

// p = 21888242871839275222246405745257275088548364400416034343698204186575808495617
// In hex: 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001
const Uint256 FieldElement::MODULUS{
    0x43e1f593f0000001ULL,  // limb 0 (least significant)
    0x2833e84879b97091ULL,  // limb 1
    0xb85045b68181585dULL,  // limb 2
    0x30644e72e131a029ULL   // limb 3 (most significant)
};

// R = 2^256 mod p
// R = 0x0e0a77c19a07df2f666ea36f7879462e36fc76959f60cd29ac96341c4ffffffb
const Uint256 FieldElement::R{
    0xac96341c4ffffffbULL,
    0x36fc76959f60cd29ULL,
    0x666ea36f7879462eULL,
    0x0e0a77c19a07df2fULL
};

// R^2 mod p
// R2 = 0x0216d0b17f4e44a58c49833d53bb808553fe3ab1e35c59e31bb8e645ae216da7
const Uint256 FieldElement::R2{
    0x1bb8e645ae216da7ULL,
    0x53fe3ab1e35c59e3ULL,
    0x8c49833d53bb8085ULL,
    0x0216d0b17f4e44a5ULL
};

// -p^(-1) mod 2^64
// INV = 0xc2e1f593efffffff
const uint64_t FieldElement::INV = 0xc2e1f593efffffffULL;

// ============================================================================
// Uint256 Implementation
// ============================================================================

Uint256::Uint256(const Byte* data, size_t len) : limbs{0, 0, 0, 0} {
    size_t bytes_to_copy = std::min(len, size_t(32));
    Byte temp[32] = {0};
    std::memcpy(temp, data, bytes_to_copy);
    
    for (int i = 0; i < 4; ++i) {
        limbs[i] = static_cast<uint64_t>(temp[i*8]) |
                   (static_cast<uint64_t>(temp[i*8+1]) << 8) |
                   (static_cast<uint64_t>(temp[i*8+2]) << 16) |
                   (static_cast<uint64_t>(temp[i*8+3]) << 24) |
                   (static_cast<uint64_t>(temp[i*8+4]) << 32) |
                   (static_cast<uint64_t>(temp[i*8+5]) << 40) |
                   (static_cast<uint64_t>(temp[i*8+6]) << 48) |
                   (static_cast<uint64_t>(temp[i*8+7]) << 56);
    }
}

Uint256 Uint256::FromHex(const std::string& hex) {
    Uint256 result;
    std::string h = hex;
    
    // Remove 0x prefix if present
    if (h.size() >= 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) {
        h = h.substr(2);
    }
    
    // Pad to 64 characters
    while (h.size() < 64) {
        h = "0" + h;
    }
    
    // Parse from most significant to least significant
    auto hexCharToNibble = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("Invalid hex character");
    };
    
    // Parse into bytes (big-endian in string -> little-endian in limbs)
    Byte bytes[32];
    for (size_t i = 0; i < 32; ++i) {
        size_t hexIdx = (31 - i) * 2;
        bytes[i] = (hexCharToNibble(h[hexIdx]) << 4) | hexCharToNibble(h[hexIdx + 1]);
    }
    
    return Uint256(bytes, 32);
}

std::string Uint256::ToHex() const {
    static const char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    
    // Output from most significant limb to least
    for (int i = 3; i >= 0; --i) {
        for (int j = 56; j >= 0; j -= 8) {
            uint8_t byte = (limbs[i] >> j) & 0xFF;
            result.push_back(hexChars[byte >> 4]);
            result.push_back(hexChars[byte & 0x0F]);
        }
    }
    
    return result;
}

std::array<Byte, 32> Uint256::ToBytes() const {
    std::array<Byte, 32> result;
    for (int i = 0; i < 4; ++i) {
        result[i*8]   = limbs[i] & 0xFF;
        result[i*8+1] = (limbs[i] >> 8) & 0xFF;
        result[i*8+2] = (limbs[i] >> 16) & 0xFF;
        result[i*8+3] = (limbs[i] >> 24) & 0xFF;
        result[i*8+4] = (limbs[i] >> 32) & 0xFF;
        result[i*8+5] = (limbs[i] >> 40) & 0xFF;
        result[i*8+6] = (limbs[i] >> 48) & 0xFF;
        result[i*8+7] = (limbs[i] >> 56) & 0xFF;
    }
    return result;
}

bool Uint256::IsZero() const {
    return limbs[0] == 0 && limbs[1] == 0 && limbs[2] == 0 && limbs[3] == 0;
}

bool Uint256::operator==(const Uint256& other) const {
    return limbs[0] == other.limbs[0] && limbs[1] == other.limbs[1] &&
           limbs[2] == other.limbs[2] && limbs[3] == other.limbs[3];
}

bool Uint256::operator!=(const Uint256& other) const {
    return !(*this == other);
}

bool Uint256::operator<(const Uint256& other) const {
    for (int i = 3; i >= 0; --i) {
        if (limbs[i] < other.limbs[i]) return true;
        if (limbs[i] > other.limbs[i]) return false;
    }
    return false;
}

bool Uint256::operator<=(const Uint256& other) const {
    return *this < other || *this == other;
}

bool Uint256::operator>(const Uint256& other) const {
    return other < *this;
}

bool Uint256::operator>=(const Uint256& other) const {
    return other <= *this;
}

Uint256 Uint256::Add(const Uint256& a, const Uint256& b, bool& carry) {
    Uint256 result;
    uint64_t c = 0;
    
    for (int i = 0; i < 4; ++i) {
        __uint128_t sum = static_cast<__uint128_t>(a.limbs[i]) + 
                          static_cast<__uint128_t>(b.limbs[i]) + c;
        result.limbs[i] = static_cast<uint64_t>(sum);
        c = static_cast<uint64_t>(sum >> 64);
    }
    
    carry = (c != 0);
    return result;
}

Uint256 Uint256::Sub(const Uint256& a, const Uint256& b, bool& borrow) {
    Uint256 result;
    uint64_t bw = 0;
    
    for (int i = 0; i < 4; ++i) {
        __uint128_t diff = static_cast<__uint128_t>(a.limbs[i]) - 
                           static_cast<__uint128_t>(b.limbs[i]) - bw;
        result.limbs[i] = static_cast<uint64_t>(diff);
        bw = (diff >> 127) & 1;  // Check if negative (borrow occurred)
    }
    
    borrow = (bw != 0);
    return result;
}

Uint256 Uint256::Mul(const Uint256& a, const Uint256& b, Uint256& high) {
    // 256-bit x 256-bit multiplication -> 512-bit result
    // Using schoolbook multiplication with 64-bit limbs
    __uint128_t products[8] = {0};
    
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            __uint128_t prod = static_cast<__uint128_t>(a.limbs[i]) * 
                               static_cast<__uint128_t>(b.limbs[j]);
            int k = i + j;
            products[k] += prod & 0xFFFFFFFFFFFFFFFFULL;
            products[k + 1] += prod >> 64;
        }
    }
    
    // Propagate carries
    uint64_t result[8];
    __uint128_t carry = 0;
    for (int i = 0; i < 8; ++i) {
        __uint128_t sum = products[i] + carry;
        result[i] = static_cast<uint64_t>(sum);
        carry = sum >> 64;
    }
    
    Uint256 lo(result[0], result[1], result[2], result[3]);
    high = Uint256(result[4], result[5], result[6], result[7]);
    return lo;
}

Uint256 Uint256::operator<<(int shift) const {
    if (shift == 0) return *this;
    if (shift >= 256) return Uint256();
    
    Uint256 result;
    int limbShift = shift / 64;
    int bitShift = shift % 64;
    
    for (int i = 3; i >= limbShift; --i) {
        result.limbs[i] = limbs[i - limbShift] << bitShift;
        if (bitShift != 0 && i > limbShift) {
            result.limbs[i] |= limbs[i - limbShift - 1] >> (64 - bitShift);
        }
    }
    
    return result;
}

Uint256 Uint256::operator>>(int shift) const {
    if (shift == 0) return *this;
    if (shift >= 256) return Uint256();
    
    Uint256 result;
    int limbShift = shift / 64;
    int bitShift = shift % 64;
    
    for (int i = 0; i < 4 - limbShift; ++i) {
        result.limbs[i] = limbs[i + limbShift] >> bitShift;
        if (bitShift != 0 && i + limbShift + 1 < 4) {
            result.limbs[i] |= limbs[i + limbShift + 1] << (64 - bitShift);
        }
    }
    
    return result;
}

// ============================================================================
// FieldElement Implementation
// ============================================================================

FieldElement::FieldElement() : value() {}

FieldElement::FieldElement(const Uint256& val) {
    // Convert to Montgomery form: value = val * R mod p
    value = MontMul(val, R2);
}

FieldElement::FieldElement(uint64_t val) {
    Uint256 v(val);
    value = MontMul(v, R2);
}

FieldElement FieldElement::Zero() {
    FieldElement fe;
    fe.value = Uint256();
    return fe;
}

FieldElement FieldElement::One() {
    FieldElement fe;
    fe.value = R;  // R is the Montgomery form of 1
    return fe;
}

Uint256 FieldElement::ToUint256() const {
    // Convert from Montgomery form to standard representation
    // Montgomery form: value = a * R mod p
    // To get a: multiply by R^(-1) which is done by MontMul(value, 1)
    // Or equivalently, MontReduce(value, 0)
    Uint256 one(1, 0, 0, 0);
    return MontMul(value, one);
}

std::array<Byte, 32> FieldElement::ToBytes() const {
    return ToUint256().ToBytes();
}

FieldElement FieldElement::FromBytes(const Byte* data, size_t len) {
    Uint256 val(data, len);
    return FieldElement(val);
}

FieldElement FieldElement::FromHex(const std::string& hex) {
    Uint256 val = Uint256::FromHex(hex);
    return FieldElement(val);
}

bool FieldElement::IsZero() const {
    return value.IsZero();
}

bool FieldElement::operator==(const FieldElement& other) const {
    return value == other.value;
}

bool FieldElement::operator!=(const FieldElement& other) const {
    return value != other.value;
}

Uint256 FieldElement::ModAdd(const Uint256& a, const Uint256& b) {
    bool carry;
    Uint256 sum = Uint256::Add(a, b, carry);
    
    // If carry or sum >= MODULUS, subtract MODULUS
    if (carry || sum >= MODULUS) {
        bool borrow;
        sum = Uint256::Sub(sum, MODULUS, borrow);
    }
    
    return sum;
}

Uint256 FieldElement::ModSub(const Uint256& a, const Uint256& b) {
    bool borrow;
    Uint256 diff = Uint256::Sub(a, b, borrow);
    
    // If borrow, add MODULUS
    if (borrow) {
        bool carry;
        diff = Uint256::Add(diff, MODULUS, carry);
    }
    
    return diff;
}

Uint256 FieldElement::MontMul(const Uint256& a, const Uint256& b) {
    // Montgomery multiplication: (a * b * R^(-1)) mod p
    Uint256 hi;
    Uint256 lo = Uint256::Mul(a, b, hi);
    return MontReduce(lo, hi);
}

Uint256 FieldElement::MontReduce(const Uint256& lo, const Uint256& hi) {
    // Montgomery reduction using the CIOS method
    Uint256 result = lo;
    Uint256 high = hi;
    
    for (int i = 0; i < 4; ++i) {
        // m = (result[0] * INV) mod 2^64
        uint64_t m = result.limbs[0] * INV;
        
        // result += m * MODULUS (shifted by i limbs conceptually)
        __uint128_t carry = 0;
        for (int j = 0; j < 4; ++j) {
            __uint128_t prod = static_cast<__uint128_t>(m) * MODULUS.limbs[j];
            __uint128_t sum = static_cast<__uint128_t>(result.limbs[j]) + prod + carry;
            result.limbs[j] = static_cast<uint64_t>(sum);
            carry = sum >> 64;
        }
        
        // Add carry to high part
        for (int j = 0; j < 4 && carry; ++j) {
            __uint128_t sum = static_cast<__uint128_t>(high.limbs[j]) + carry;
            high.limbs[j] = static_cast<uint64_t>(sum);
            carry = sum >> 64;
        }
        
        // Shift result right by 64 bits
        result.limbs[0] = result.limbs[1];
        result.limbs[1] = result.limbs[2];
        result.limbs[2] = result.limbs[3];
        result.limbs[3] = high.limbs[0];
        
        high.limbs[0] = high.limbs[1];
        high.limbs[1] = high.limbs[2];
        high.limbs[2] = high.limbs[3];
        high.limbs[3] = 0;
    }
    
    // Final reduction if needed
    if (result >= MODULUS) {
        bool borrow;
        result = Uint256::Sub(result, MODULUS, borrow);
    }
    
    return result;
}

FieldElement FieldElement::operator+(const FieldElement& other) const {
    FieldElement result;
    result.value = ModAdd(value, other.value);
    return result;
}

FieldElement FieldElement::operator-(const FieldElement& other) const {
    FieldElement result;
    result.value = ModSub(value, other.value);
    return result;
}

FieldElement FieldElement::operator*(const FieldElement& other) const {
    FieldElement result;
    result.value = MontMul(value, other.value);
    return result;
}

FieldElement FieldElement::operator-() const {
    if (IsZero()) return *this;
    FieldElement result;
    bool borrow;
    result.value = Uint256::Sub(MODULUS, value, borrow);
    return result;
}

FieldElement& FieldElement::operator+=(const FieldElement& other) {
    value = ModAdd(value, other.value);
    return *this;
}

FieldElement& FieldElement::operator-=(const FieldElement& other) {
    value = ModSub(value, other.value);
    return *this;
}

FieldElement& FieldElement::operator*=(const FieldElement& other) {
    value = MontMul(value, other.value);
    return *this;
}

FieldElement FieldElement::Square() const {
    return (*this) * (*this);
}

FieldElement FieldElement::Pow(const Uint256& exp) const {
    if (exp.IsZero()) return One();
    
    FieldElement result = One();
    FieldElement base = *this;
    
    // Square-and-multiply from LSB
    // Process each limb
    for (int i = 0; i < 4; ++i) {
        uint64_t limb = exp.limbs[i];
        if (limb == 0 && i > 0) {
            // Skip zero limbs but still square base 64 times
            for (int j = 0; j < 64; ++j) {
                base = base.Square();
            }
            continue;
        }
        for (int j = 0; j < 64; ++j) {
            if (limb & 1) {
                result = result * base;
            }
            base = base.Square();
            limb >>= 1;
        }
    }
    
    return result;
}

FieldElement FieldElement::Inverse() const {
    if (IsZero()) return Zero();
    
    // Use Fermat's little theorem: a^(-1) = a^(p-2) mod p
    // p - 2 = 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593efffffff
    Uint256 pMinus2(
        0x43e1f593efffffffULL,
        0x2833e84879b97091ULL,
        0xb85045b68181585dULL,
        0x30644e72e131a029ULL
    );
    
    return Pow(pMinus2);
}

FieldElement FieldElement::PoseidonSbox() const {
    // S-box: x^5
    FieldElement x2 = Square();
    FieldElement x4 = x2.Square();
    return x4 * (*this);
}

} // namespace shurium
