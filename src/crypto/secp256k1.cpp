// SHURIUM - secp256k1 Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/crypto/secp256k1.h"
#include "shurium/crypto/sha256.h"
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

#ifdef SHURIUM_USE_OPENSSL
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#endif

namespace shurium {
namespace secp256k1 {

// ============================================================================
// Constants
// ============================================================================

const std::array<uint8_t, 32> FIELD_PRIME = {{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFC, 0x2F
}};

const std::array<uint8_t, 32> CURVE_ORDER = {{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0xBA, 0xAE, 0xDC, 0xE6, 0xAF, 0x48, 0xA0, 0x3B,
    0xBF, 0xD2, 0x5E, 0x8C, 0xD0, 0x36, 0x41, 0x41
}};

const std::array<uint8_t, 33> GENERATOR_COMPRESSED = {{
    0x02,
    0x79, 0xBE, 0x66, 0x7E, 0xF9, 0xDC, 0xBB, 0xAC,
    0x55, 0xA0, 0x62, 0x95, 0xCE, 0x87, 0x0B, 0x07,
    0x02, 0x9B, 0xFC, 0xDB, 0x2D, 0xCE, 0x28, 0xD9,
    0x59, 0xF2, 0x81, 0x5B, 0x16, 0xF8, 0x17, 0x98
}};

const std::array<uint8_t, 65> GENERATOR_UNCOMPRESSED = {{
    0x04,
    0x79, 0xBE, 0x66, 0x7E, 0xF9, 0xDC, 0xBB, 0xAC,
    0x55, 0xA0, 0x62, 0x95, 0xCE, 0x87, 0x0B, 0x07,
    0x02, 0x9B, 0xFC, 0xDB, 0x2D, 0xCE, 0x28, 0xD9,
    0x59, 0xF2, 0x81, 0x5B, 0x16, 0xF8, 0x17, 0x98,
    0x48, 0x3A, 0xDA, 0x77, 0x26, 0xA3, 0xC4, 0x65,
    0x5D, 0xA4, 0xFB, 0xFC, 0x0E, 0x11, 0x08, 0xA8,
    0xFD, 0x17, 0xB4, 0x48, 0xA6, 0x85, 0x54, 0x19,
    0x9C, 0x47, 0xD0, 0x8F, 0xFB, 0x10, 0xD4, 0xB8
}};

namespace {

inline void SecureClear(void* ptr, size_t len) {
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) *p++ = 0;
}

// Compare two 32-byte big-endian numbers
// Returns: -1 if a < b, 0 if a == b, 1 if a > b
int Compare32(const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < 32; ++i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

bool IsZero32(const uint8_t* a) {
    for (int i = 0; i < 32; ++i) {
        if (a[i] != 0) return false;
    }
    return true;
}

} // anonymous namespace

// ============================================================================
// Scalar Implementation
// ============================================================================

Scalar::Scalar() {
    data_.fill(0);
}

Scalar::Scalar(const uint8_t* data) {
    std::memcpy(data_.data(), data, SIZE);
}

Scalar::Scalar(const std::array<uint8_t, SIZE>& data) : data_(data) {}

Scalar::Scalar(const std::vector<uint8_t>& data) {
    if (data.size() >= SIZE) {
        std::memcpy(data_.data(), data.data(), SIZE);
    } else {
        data_.fill(0);
        std::memcpy(data_.data() + (SIZE - data.size()), data.data(), data.size());
    }
}

Scalar::~Scalar() {
    SecureClear(data_.data(), SIZE);
}

bool Scalar::IsZero() const {
    return IsZero32(data_.data());
}

bool Scalar::IsValid() const {
    if (IsZero()) return false;
    return Compare32(data_.data(), CURVE_ORDER.data()) < 0;
}

void Scalar::SetZero() {
    data_.fill(0);
}

bool Scalar::operator==(const Scalar& other) const {
    return Compare32(data_.data(), other.data_.data()) == 0;
}

bool Scalar::operator<(const Scalar& other) const {
    return Compare32(data_.data(), other.data_.data()) < 0;
}

Scalar Scalar::FromInt(uint64_t value) {
    Scalar result;
    for (int i = 0; i < 8; ++i) {
        result.data_[31 - i] = (value >> (i * 8)) & 0xff;
    }
    return result;
}

// ============================================================================
// Scalar Arithmetic (using OpenSSL BN or fallback)
// ============================================================================

#ifdef SHURIUM_USE_OPENSSL

Scalar Scalar::operator+(const Scalar& other) const {
    Scalar result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* b = BN_bin2bn(other.data_.data(), SIZE, nullptr);
    BIGNUM* n = BN_bin2bn(CURVE_ORDER.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && b && n && r && ctx) {
        BN_mod_add(r, a, b, n, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(b); BN_free(n); BN_free(r); BN_CTX_free(ctx);
    return result;
}

Scalar Scalar::operator-(const Scalar& other) const {
    Scalar result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* b = BN_bin2bn(other.data_.data(), SIZE, nullptr);
    BIGNUM* n = BN_bin2bn(CURVE_ORDER.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && b && n && r && ctx) {
        BN_mod_sub(r, a, b, n, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(b); BN_free(n); BN_free(r); BN_CTX_free(ctx);
    return result;
}

Scalar Scalar::operator*(const Scalar& other) const {
    Scalar result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* b = BN_bin2bn(other.data_.data(), SIZE, nullptr);
    BIGNUM* n = BN_bin2bn(CURVE_ORDER.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && b && n && r && ctx) {
        BN_mod_mul(r, a, b, n, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(b); BN_free(n); BN_free(r); BN_CTX_free(ctx);
    return result;
}

Scalar Scalar::operator-() const {
    Scalar result;
    if (IsZero()) return result;
    
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* n = BN_bin2bn(CURVE_ORDER.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && n && r && ctx) {
        BN_mod_sub(r, n, a, n, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(n); BN_free(r); BN_CTX_free(ctx);
    return result;
}

Scalar Scalar::Inverse() const {
    Scalar result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* n = BN_bin2bn(CURVE_ORDER.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && n && r && ctx) {
        BN_mod_inverse(r, a, n, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(n); BN_free(r); BN_CTX_free(ctx);
    return result;
}

Scalar Scalar::Random() {
    Scalar result;
    do {
        if (RAND_bytes(result.data_.data(), SIZE) != 1) {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            for (auto& b : result.data_) {
                b = static_cast<uint8_t>(gen() & 0xff);
            }
        }
    } while (!result.IsValid());
    return result;
}

#else
// ============================================================================
// Fallback implementation without OpenSSL - Pure C++ secp256k1
// ============================================================================

namespace {

// 512-bit intermediate for multiplication
struct Uint512 {
    uint32_t d[16]{};  // Little-endian limbs
    
    void SetZero() { std::memset(d, 0, sizeof(d)); }
    
    // Add two 256-bit numbers into 512-bit result
    static Uint512 FromProduct(const uint8_t* a, const uint8_t* b) {
        Uint512 result;
        result.SetZero();
        
        // Convert big-endian to little-endian 32-bit limbs
        uint32_t al[8], bl[8];
        for (int i = 0; i < 8; ++i) {
            al[i] = (static_cast<uint32_t>(a[31 - i*4]) |
                    (static_cast<uint32_t>(a[30 - i*4]) << 8) |
                    (static_cast<uint32_t>(a[29 - i*4]) << 16) |
                    (static_cast<uint32_t>(a[28 - i*4]) << 24));
            bl[i] = (static_cast<uint32_t>(b[31 - i*4]) |
                    (static_cast<uint32_t>(b[30 - i*4]) << 8) |
                    (static_cast<uint32_t>(b[29 - i*4]) << 16) |
                    (static_cast<uint32_t>(b[28 - i*4]) << 24));
        }
        
        // Schoolbook multiplication
        for (int i = 0; i < 8; ++i) {
            uint64_t carry = 0;
            for (int j = 0; j < 8; ++j) {
                uint64_t prod = static_cast<uint64_t>(al[i]) * bl[j] + 
                               result.d[i + j] + carry;
                result.d[i + j] = static_cast<uint32_t>(prod);
                carry = prod >> 32;
            }
            result.d[i + 8] = static_cast<uint32_t>(carry);
        }
        return result;
    }
};

// Reduce 512-bit number mod 256-bit modulus using Barrett reduction
void ReduceMod(const Uint512& x, const uint8_t* mod, uint8_t* result) {
    // Convert modulus to limbs
    uint32_t m[8];
    for (int i = 0; i < 8; ++i) {
        m[i] = (static_cast<uint32_t>(mod[31 - i*4]) |
               (static_cast<uint32_t>(mod[30 - i*4]) << 8) |
               (static_cast<uint32_t>(mod[29 - i*4]) << 16) |
               (static_cast<uint32_t>(mod[28 - i*4]) << 24));
    }
    
    // Simple reduction: subtract mod while >= mod
    uint32_t r[16];
    std::memcpy(r, x.d, sizeof(r));
    
    // Reduce from 512 bits to 256 bits
    // For secp256k1 n, we can use the fact that 2^256 ≡ λ (mod n) where λ is small
    // But for simplicity, we'll do iterative subtraction for the high bits
    
    // First, get the result to within 2*mod
    while (true) {
        // Check if r >= mod (shifted appropriately)
        bool geq = false;
        int shift = 0;
        
        // Find highest non-zero limb
        for (int i = 15; i >= 8; --i) {
            if (r[i] != 0) {
                shift = i - 7;
                break;
            }
        }
        
        if (shift == 0) break;  // r < 2^256
        
        // Subtract mod << (shift * 32)
        uint64_t borrow = 0;
        for (int i = 0; i < 8; ++i) {
            uint64_t sub = static_cast<uint64_t>(r[i + shift]) - m[i] - borrow;
            r[i + shift] = static_cast<uint32_t>(sub);
            borrow = (sub >> 63) & 1;  // Borrow if negative
        }
        for (int i = 8 + shift; i < 16; ++i) {
            uint64_t sub = static_cast<uint64_t>(r[i]) - borrow;
            r[i] = static_cast<uint32_t>(sub);
            borrow = (sub >> 63) & 1;
            if (borrow == 0) break;
        }
    }
    
    // Final reduction if still >= mod
    while (true) {
        bool geq = true;
        for (int i = 7; i >= 0; --i) {
            if (r[i] < m[i]) { geq = false; break; }
            if (r[i] > m[i]) break;
        }
        if (!geq) break;
        
        uint64_t borrow = 0;
        for (int i = 0; i < 8; ++i) {
            uint64_t sub = static_cast<uint64_t>(r[i]) - m[i] - borrow;
            r[i] = static_cast<uint32_t>(sub);
            borrow = (sub >> 63) & 1;
        }
    }
    
    // Convert back to big-endian bytes
    for (int i = 0; i < 8; ++i) {
        result[31 - i*4] = r[i] & 0xff;
        result[30 - i*4] = (r[i] >> 8) & 0xff;
        result[29 - i*4] = (r[i] >> 16) & 0xff;
        result[28 - i*4] = (r[i] >> 24) & 0xff;
    }
}

// Extended Euclidean algorithm for modular inverse
// Computes a^-1 mod m
void ModInverse(const uint8_t* a, const uint8_t* m, uint8_t* result) {
    // Using binary extended GCD for constant-time-ish operation
    // Initialize: u = m, v = a, x1 = 0, x2 = 1
    // While v != 0: if u is even, u/=2, x1 = x1/2 or (x1+m)/2
    //               else if v is even, v/=2, x2 = x2/2 or (x2+m)/2
    //               else if u >= v, u = (u-v)/2, x1 = (x1-x2)/2 or adjusted
    //               else v = (v-u)/2, x2 = (x2-x1)/2 or adjusted
    
    // For simplicity, use the standard extended Euclidean algorithm
    // Convert to big integers (using arrays of uint64_t for efficiency)
    
    // Simplified approach: use Fermat's little theorem
    // a^-1 = a^(m-2) mod m (when m is prime)
    // For curve order n, this works since n is prime
    
    // Compute a^(n-2) mod n using square-and-multiply
    // n-2 in binary requires computing the exponent
    
    uint8_t exp[32];
    std::memcpy(exp, m, 32);
    
    // Subtract 2 from exp (big-endian)
    int borrow = 2;
    for (int i = 31; i >= 0 && borrow > 0; --i) {
        int diff = static_cast<int>(exp[i]) - borrow;
        if (diff < 0) {
            exp[i] = static_cast<uint8_t>(diff + 256);
            borrow = 1;
        } else {
            exp[i] = static_cast<uint8_t>(diff);
            borrow = 0;
        }
    }
    
    // Initialize result to 1
    std::memset(result, 0, 32);
    result[31] = 1;
    
    uint8_t base[32];
    std::memcpy(base, a, 32);
    
    uint8_t temp[32];
    
    // Square and multiply (from LSB to MSB)
    for (int i = 31; i >= 0; --i) {
        for (int bit = 0; bit < 8; ++bit) {
            if (exp[i] & (1 << bit)) {
                // result = result * base mod m
                Uint512 prod = Uint512::FromProduct(result, base);
                ReduceMod(prod, m, result);
            }
            // base = base * base mod m
            Uint512 sq = Uint512::FromProduct(base, base);
            ReduceMod(sq, m, base);
        }
    }
}

} // anonymous namespace

Scalar Scalar::operator+(const Scalar& other) const {
    Scalar result;
    uint16_t carry = 0;
    for (int i = 31; i >= 0; --i) {
        uint16_t sum = static_cast<uint16_t>(data_[i]) + other.data_[i] + carry;
        result.data_[i] = sum & 0xff;
        carry = sum >> 8;
    }
    // Reduce mod n if needed
    if (carry || Compare32(result.data_.data(), CURVE_ORDER.data()) >= 0) {
        uint16_t borrow = 0;
        for (int i = 31; i >= 0; --i) {
            int16_t diff = static_cast<int16_t>(result.data_[i]) - CURVE_ORDER[i] - borrow;
            if (diff < 0) { diff += 256; borrow = 1; } else { borrow = 0; }
            result.data_[i] = diff & 0xff;
        }
    }
    return result;
}

Scalar Scalar::operator-(const Scalar& other) const {
    return *this + (-other);
}

Scalar Scalar::operator*(const Scalar& other) const {
    Scalar result;
    Uint512 prod = Uint512::FromProduct(data_.data(), other.data_.data());
    ReduceMod(prod, CURVE_ORDER.data(), result.data_.data());
    return result;
}

Scalar Scalar::operator-() const {
    if (IsZero()) return *this;
    Scalar result;
    uint16_t borrow = 0;
    for (int i = 31; i >= 0; --i) {
        int16_t diff = static_cast<int16_t>(CURVE_ORDER[i]) - data_[i] - borrow;
        if (diff < 0) { diff += 256; borrow = 1; } else { borrow = 0; }
        result.data_[i] = diff & 0xff;
    }
    return result;
}

Scalar Scalar::Inverse() const {
    if (IsZero()) return Scalar();  // No inverse for zero
    Scalar result;
    ModInverse(data_.data(), CURVE_ORDER.data(), result.data_.data());
    return result;
}

Scalar Scalar::Random() {
    Scalar result;
    std::random_device rd;
    std::mt19937_64 gen(rd());
    do {
        for (auto& b : result.data_) {
            b = static_cast<uint8_t>(gen() & 0xff);
        }
    } while (!result.IsValid());
    return result;
}

#endif

Scalar& Scalar::operator+=(const Scalar& other) {
    *this = *this + other;
    return *this;
}

Scalar& Scalar::operator-=(const Scalar& other) {
    *this = *this - other;
    return *this;
}

Scalar& Scalar::operator*=(const Scalar& other) {
    *this = *this * other;
    return *this;
}

// ============================================================================
// FieldElement Implementation
// ============================================================================

FieldElement::FieldElement() {
    data_.fill(0);
}

FieldElement::FieldElement(const uint8_t* data) {
    std::memcpy(data_.data(), data, SIZE);
}

FieldElement::FieldElement(const std::array<uint8_t, SIZE>& data) : data_(data) {}

bool FieldElement::IsZero() const {
    return IsZero32(data_.data());
}

bool FieldElement::IsValid() const {
    return Compare32(data_.data(), FIELD_PRIME.data()) < 0;
}

bool FieldElement::IsOdd() const {
    return (data_[31] & 1) != 0;
}

bool FieldElement::operator==(const FieldElement& other) const {
    return Compare32(data_.data(), other.data_.data()) == 0;
}

// Field arithmetic - similar to Scalar but mod p instead of n
#ifdef SHURIUM_USE_OPENSSL

FieldElement FieldElement::operator+(const FieldElement& other) const {
    FieldElement result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* b = BN_bin2bn(other.data_.data(), SIZE, nullptr);
    BIGNUM* p = BN_bin2bn(FIELD_PRIME.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && b && p && r && ctx) {
        BN_mod_add(r, a, b, p, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(b); BN_free(p); BN_free(r); BN_CTX_free(ctx);
    return result;
}

FieldElement FieldElement::operator-(const FieldElement& other) const {
    FieldElement result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* b = BN_bin2bn(other.data_.data(), SIZE, nullptr);
    BIGNUM* p = BN_bin2bn(FIELD_PRIME.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && b && p && r && ctx) {
        BN_mod_sub(r, a, b, p, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(b); BN_free(p); BN_free(r); BN_CTX_free(ctx);
    return result;
}

FieldElement FieldElement::operator*(const FieldElement& other) const {
    FieldElement result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* b = BN_bin2bn(other.data_.data(), SIZE, nullptr);
    BIGNUM* p = BN_bin2bn(FIELD_PRIME.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && b && p && r && ctx) {
        BN_mod_mul(r, a, b, p, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(b); BN_free(p); BN_free(r); BN_CTX_free(ctx);
    return result;
}

FieldElement FieldElement::operator-() const {
    if (IsZero()) return *this;
    FieldElement result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* p = BN_bin2bn(FIELD_PRIME.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && p && r && ctx) {
        BN_mod_sub(r, p, a, p, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(p); BN_free(r); BN_CTX_free(ctx);
    return result;
}

FieldElement FieldElement::Inverse() const {
    FieldElement result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* p = BN_bin2bn(FIELD_PRIME.data(), 32, nullptr);
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    if (a && p && r && ctx) {
        BN_mod_inverse(r, a, p, ctx);
        int len = BN_num_bytes(r);
        if (len <= 32) {
            BN_bn2bin(r, result.data_.data() + (32 - len));
        }
    }
    
    BN_free(a); BN_free(p); BN_free(r); BN_CTX_free(ctx);
    return result;
}

FieldElement FieldElement::Square() const {
    return *this * *this;
}

std::optional<FieldElement> FieldElement::Sqrt() const {
    // For secp256k1, p = 3 mod 4, so sqrt(a) = a^((p+1)/4) if it exists
    FieldElement result;
    BIGNUM* a = BN_bin2bn(data_.data(), SIZE, nullptr);
    BIGNUM* p = BN_bin2bn(FIELD_PRIME.data(), 32, nullptr);
    BIGNUM* exp = BN_new();
    BIGNUM* r = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    
    bool valid = false;
    if (a && p && exp && r && ctx) {
        // exp = (p + 1) / 4
        BN_add_word(p, 1);
        BN_rshift(exp, p, 2);
        BN_sub_word(p, 1);  // restore p
        
        BN_mod_exp(r, a, exp, p, ctx);
        
        // Verify: r^2 mod p == a
        BIGNUM* check = BN_new();
        BN_mod_sqr(check, r, p, ctx);
        if (BN_cmp(check, a) == 0) {
            valid = true;
            int len = BN_num_bytes(r);
            if (len <= 32) {
                BN_bn2bin(r, result.data_.data() + (32 - len));
            }
        }
        BN_free(check);
    }
    
    BN_free(a); BN_free(p); BN_free(exp); BN_free(r); BN_CTX_free(ctx);
    
    if (valid) return result;
    return std::nullopt;
}

#else
// ============================================================================
// FieldElement Fallback - Pure C++ implementation
// ============================================================================

FieldElement FieldElement::operator+(const FieldElement& other) const {
    FieldElement result;
    uint16_t carry = 0;
    for (int i = 31; i >= 0; --i) {
        uint16_t sum = static_cast<uint16_t>(data_[i]) + other.data_[i] + carry;
        result.data_[i] = sum & 0xff;
        carry = sum >> 8;
    }
    // Reduce mod p if needed
    if (carry || Compare32(result.data_.data(), FIELD_PRIME.data()) >= 0) {
        uint16_t borrow = 0;
        for (int i = 31; i >= 0; --i) {
            int16_t diff = static_cast<int16_t>(result.data_[i]) - FIELD_PRIME[i] - borrow;
            if (diff < 0) { diff += 256; borrow = 1; } else { borrow = 0; }
            result.data_[i] = diff & 0xff;
        }
    }
    return result;
}

FieldElement FieldElement::operator-(const FieldElement& other) const {
    return *this + (-other);
}

FieldElement FieldElement::operator*(const FieldElement& other) const {
    FieldElement result;
    Uint512 prod = Uint512::FromProduct(data_.data(), other.data_.data());
    ReduceMod(prod, FIELD_PRIME.data(), result.data_.data());
    return result;
}

FieldElement FieldElement::operator-() const {
    if (IsZero()) return *this;
    FieldElement result;
    uint16_t borrow = 0;
    for (int i = 31; i >= 0; --i) {
        int16_t diff = static_cast<int16_t>(FIELD_PRIME[i]) - data_[i] - borrow;
        if (diff < 0) { diff += 256; borrow = 1; } else { borrow = 0; }
        result.data_[i] = diff & 0xff;
    }
    return result;
}

FieldElement FieldElement::Inverse() const {
    if (IsZero()) return FieldElement();
    FieldElement result;
    ModInverse(data_.data(), FIELD_PRIME.data(), result.data_.data());
    return result;
}

FieldElement FieldElement::Square() const {
    return *this * *this;
}

std::optional<FieldElement> FieldElement::Sqrt() const {
    // For secp256k1, p ≡ 3 (mod 4), so sqrt(a) = a^((p+1)/4) if it exists
    // (p+1)/4 = 0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffbfffff0c
    
    // Compute a^((p+1)/4) using square-and-multiply
    // First, compute (p+1)/4
    uint8_t exp[32];
    std::memcpy(exp, FIELD_PRIME.data(), 32);
    
    // Add 1 to p (it won't overflow since p < 2^256 - 1)
    int carry = 1;
    for (int i = 31; i >= 0 && carry; --i) {
        int sum = static_cast<int>(exp[i]) + carry;
        exp[i] = sum & 0xff;
        carry = sum >> 8;
    }
    
    // Divide by 4 (right shift by 2)
    for (int i = 0; i < 32; ++i) {
        uint8_t next = (i < 31) ? exp[i + 1] : 0;
        exp[i] = (exp[i] >> 2) | ((next & 3) << 6);
    }
    
    // Compute a^exp mod p
    FieldElement result;
    result.data_[31] = 1;  // Start with 1
    
    FieldElement base = *this;
    
    // Square and multiply (from LSB to MSB)
    for (int i = 31; i >= 0; --i) {
        for (int bit = 0; bit < 8; ++bit) {
            if (exp[i] & (1 << bit)) {
                result = result * base;
            }
            base = base.Square();
        }
    }
    
    // Verify: result^2 should equal *this
    FieldElement check = result.Square();
    if (check == *this) {
        return result;
    }
    
    return std::nullopt;
}
#endif

// ============================================================================
// Point Implementation
// ============================================================================

struct Point::Impl {
#ifdef SHURIUM_USE_OPENSSL
    EC_GROUP* group{nullptr};
    EC_POINT* point{nullptr};
    BN_CTX* ctx{nullptr};
    
    Impl() {
        group = EC_GROUP_new_by_curve_name(NID_secp256k1);
        ctx = BN_CTX_new();
        if (group) {
            point = EC_POINT_new(group);
            if (point) {
                EC_POINT_set_to_infinity(group, point);
            }
        }
    }
    
    Impl(const Impl& other) {
        group = EC_GROUP_new_by_curve_name(NID_secp256k1);
        ctx = BN_CTX_new();
        if (group && other.point) {
            point = EC_POINT_dup(other.point, group);
        }
    }
    
    ~Impl() {
        if (point) EC_POINT_free(point);
        if (group) EC_GROUP_free(group);
        if (ctx) BN_CTX_free(ctx);
    }
#else
    FieldElement x, y;
    bool infinity{true};
    
    Impl() : infinity(true) {}
    Impl(const FieldElement& px, const FieldElement& py) : x(px), y(py), infinity(false) {}
#endif
};

Point::Point() : impl_(std::make_unique<Impl>()) {}

Point::~Point() = default;

Point::Point(const Point& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

Point& Point::operator=(const Point& other) {
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

Point::Point(Point&& other) noexcept = default;
Point& Point::operator=(Point&& other) noexcept = default;

Point::Point(const FieldElement& x, const FieldElement& y)
    : impl_(std::make_unique<Impl>()) {
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point) {
        BIGNUM* bx = BN_bin2bn(x.data(), 32, nullptr);
        BIGNUM* by = BN_bin2bn(y.data(), 32, nullptr);
        if (bx && by) {
            EC_POINT_set_affine_coordinates(impl_->group, impl_->point, bx, by, impl_->ctx);
        }
        BN_free(bx);
        BN_free(by);
    }
#else
    impl_->x = x;
    impl_->y = y;
    impl_->infinity = false;
#endif
}

Point::Point(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

std::optional<Point> Point::FromCompressed(const uint8_t* data) {
    if (!data) return std::nullopt;
    if (data[0] != 0x02 && data[0] != 0x03) return std::nullopt;
    
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (result.impl_->group && result.impl_->point) {
        if (EC_POINT_oct2point(result.impl_->group, result.impl_->point,
                               data, COMPRESSED_SIZE, result.impl_->ctx) != 1) {
            return std::nullopt;
        }
    }
#else
    // Decompress: solve y^2 = x^3 + 7
    FieldElement x(data + 1);
    FieldElement x3 = x * x * x;
    FieldElement seven;
    seven.data_[31] = 7;
    FieldElement y2 = x3 + seven;
    auto y = y2.Sqrt();
    if (!y) return std::nullopt;
    
    bool needOdd = (data[0] == 0x03);
    if (y->IsOdd() != needOdd) {
        *y = -*y;
    }
    result.impl_->x = x;
    result.impl_->y = *y;
    result.impl_->infinity = false;
#endif
    return result;
}

std::optional<Point> Point::FromCompressed(const std::array<uint8_t, COMPRESSED_SIZE>& data) {
    return FromCompressed(data.data());
}

std::optional<Point> Point::FromUncompressed(const uint8_t* data) {
    if (!data || data[0] != 0x04) return std::nullopt;
    
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (result.impl_->group && result.impl_->point) {
        if (EC_POINT_oct2point(result.impl_->group, result.impl_->point,
                               data, UNCOMPRESSED_SIZE, result.impl_->ctx) != 1) {
            return std::nullopt;
        }
    }
#else
    result.impl_->x = FieldElement(data + 1);
    result.impl_->y = FieldElement(data + 33);
    result.impl_->infinity = false;
#endif
    return result;
}

std::optional<Point> Point::FromUncompressed(const std::array<uint8_t, UNCOMPRESSED_SIZE>& data) {
    return FromUncompressed(data.data());
}

std::optional<Point> Point::FromBytes(const uint8_t* data, size_t len) {
    if (len == COMPRESSED_SIZE) return FromCompressed(data);
    if (len == UNCOMPRESSED_SIZE) return FromUncompressed(data);
    return std::nullopt;
}

std::optional<Point> Point::FromBytes(const std::vector<uint8_t>& data) {
    return FromBytes(data.data(), data.size());
}

bool Point::IsInfinity() const {
#ifdef SHURIUM_USE_OPENSSL
    return impl_->group && impl_->point && 
           EC_POINT_is_at_infinity(impl_->group, impl_->point);
#else
    return impl_->infinity;
#endif
}

bool Point::IsOnCurve() const {
#ifdef SHURIUM_USE_OPENSSL
    return impl_->group && impl_->point &&
           EC_POINT_is_on_curve(impl_->group, impl_->point, impl_->ctx);
#else
    if (impl_->infinity) return true;
    // Check y^2 = x^3 + 7
    FieldElement y2 = impl_->y.Square();
    FieldElement x3 = impl_->x * impl_->x * impl_->x;
    FieldElement seven; seven.data_[31] = 7;
    return y2 == (x3 + seven);
#endif
}

FieldElement Point::GetX() const {
    FieldElement result;
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point && !IsInfinity()) {
        BIGNUM* x = BN_new();
        if (x && EC_POINT_get_affine_coordinates(impl_->group, impl_->point, x, nullptr, impl_->ctx)) {
            int len = BN_num_bytes(x);
            if (len <= 32) {
                BN_bn2bin(x, result.data_.data() + (32 - len));
            }
        }
        BN_free(x);
    }
#else
    result = impl_->x;
#endif
    return result;
}

FieldElement Point::GetY() const {
    FieldElement result;
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point && !IsInfinity()) {
        BIGNUM* y = BN_new();
        if (y && EC_POINT_get_affine_coordinates(impl_->group, impl_->point, nullptr, y, impl_->ctx)) {
            int len = BN_num_bytes(y);
            if (len <= 32) {
                BN_bn2bin(y, result.data_.data() + (32 - len));
            }
        }
        BN_free(y);
    }
#else
    result = impl_->y;
#endif
    return result;
}

std::array<uint8_t, Point::COMPRESSED_SIZE> Point::ToCompressed() const {
    std::array<uint8_t, COMPRESSED_SIZE> result{};
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point) {
        EC_POINT_point2oct(impl_->group, impl_->point, POINT_CONVERSION_COMPRESSED,
                           result.data(), COMPRESSED_SIZE, impl_->ctx);
    }
#else
    if (!impl_->infinity) {
        result[0] = impl_->y.IsOdd() ? 0x03 : 0x02;
        std::memcpy(result.data() + 1, impl_->x.data(), 32);
    }
#endif
    return result;
}

std::array<uint8_t, Point::UNCOMPRESSED_SIZE> Point::ToUncompressed() const {
    std::array<uint8_t, UNCOMPRESSED_SIZE> result{};
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point) {
        EC_POINT_point2oct(impl_->group, impl_->point, POINT_CONVERSION_UNCOMPRESSED,
                           result.data(), UNCOMPRESSED_SIZE, impl_->ctx);
    }
#else
    if (!impl_->infinity) {
        result[0] = 0x04;
        std::memcpy(result.data() + 1, impl_->x.data(), 32);
        std::memcpy(result.data() + 33, impl_->y.data(), 32);
    }
#endif
    return result;
}

Point Point::operator+(const Point& other) const {
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point && other.impl_->point && result.impl_->point) {
        EC_POINT_add(impl_->group, result.impl_->point, impl_->point, other.impl_->point, impl_->ctx);
    }
#else
    // Point addition on secp256k1 using affine coordinates
    if (impl_->infinity) return other;
    if (other.impl_->infinity) return *this;
    
    // Check for P + (-P) = infinity
    if (impl_->x == other.impl_->x) {
        if (impl_->y == other.impl_->y) {
            // P + P = 2P (point doubling)
            return Double();
        } else {
            // P + (-P) = O (point at infinity)
            return Point();
        }
    }
    
    // Standard point addition formula:
    // λ = (y2 - y1) / (x2 - x1)
    // x3 = λ² - x1 - x2
    // y3 = λ(x1 - x3) - y1
    
    FieldElement dy = other.impl_->y - impl_->y;
    FieldElement dx = other.impl_->x - impl_->x;
    FieldElement lambda = dy * dx.Inverse();
    
    FieldElement x3 = lambda.Square() - impl_->x - other.impl_->x;
    FieldElement y3 = lambda * (impl_->x - x3) - impl_->y;
    
    result.impl_->x = x3;
    result.impl_->y = y3;
    result.impl_->infinity = false;
#endif
    return result;
}

Point Point::operator-(const Point& other) const {
    return *this + (-other);
}

Point Point::operator-() const {
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point && result.impl_->point) {
        EC_POINT_copy(result.impl_->point, impl_->point);
        EC_POINT_invert(impl_->group, result.impl_->point, impl_->ctx);
    }
#else
    if (!impl_->infinity) {
        result.impl_->x = impl_->x;
        result.impl_->y = -impl_->y;
        result.impl_->infinity = false;
    }
#endif
    return result;
}

Point Point::operator*(const Scalar& scalar) const {
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point && result.impl_->point) {
        BIGNUM* k = BN_bin2bn(scalar.data(), 32, nullptr);
        if (k) {
            EC_POINT_mul(impl_->group, result.impl_->point, nullptr, impl_->point, k, impl_->ctx);
            BN_free(k);
        }
    }
#else
    // Double-and-add scalar multiplication
    if (scalar.IsZero() || impl_->infinity) {
        return Point();  // Return infinity
    }
    
    Point R;  // Result (starts at infinity)
    Point Q = *this;  // Current point (starts at P)
    
    // Process each bit of the scalar from LSB to MSB
    const uint8_t* k = scalar.data();
    for (int i = 31; i >= 0; --i) {
        for (int bit = 0; bit < 8; ++bit) {
            if (k[i] & (1 << bit)) {
                R = R + Q;
            }
            Q = Q.Double();
        }
    }
    
    result = R;
#endif
    return result;
}

Point Point::Double() const {
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point && result.impl_->point) {
        EC_POINT_dbl(impl_->group, result.impl_->point, impl_->point, impl_->ctx);
    }
#else
    // Point doubling on secp256k1
    if (impl_->infinity || impl_->y.IsZero()) {
        return Point();  // Return infinity
    }
    
    // Point doubling formula (a = 0 for secp256k1):
    // λ = 3x² / 2y
    // x3 = λ² - 2x
    // y3 = λ(x - x3) - y
    
    FieldElement x2 = impl_->x.Square();
    
    // 3x² = x² + x² + x²
    FieldElement three_x2 = x2 + x2 + x2;
    
    // 2y
    FieldElement two_y = impl_->y + impl_->y;
    
    // λ = 3x² / 2y
    FieldElement lambda = three_x2 * two_y.Inverse();
    
    // x3 = λ² - 2x
    FieldElement two_x = impl_->x + impl_->x;
    FieldElement x3 = lambda.Square() - two_x;
    
    // y3 = λ(x - x3) - y
    FieldElement y3 = lambda * (impl_->x - x3) - impl_->y;
    
    result.impl_->x = x3;
    result.impl_->y = y3;
    result.impl_->infinity = false;
#endif
    return result;
}

bool Point::operator==(const Point& other) const {
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->group && impl_->point && other.impl_->point) {
        return EC_POINT_cmp(impl_->group, impl_->point, other.impl_->point, impl_->ctx) == 0;
    }
    return false;
#else
    if (impl_->infinity && other.impl_->infinity) return true;
    if (impl_->infinity || other.impl_->infinity) return false;
    return impl_->x == other.impl_->x && impl_->y == other.impl_->y;
#endif
}

Point Point::Generator() {
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (result.impl_->group && result.impl_->point) {
        const EC_POINT* gen = EC_GROUP_get0_generator(result.impl_->group);
        if (gen) {
            EC_POINT_copy(result.impl_->point, gen);
        }
    }
#else
    auto p = FromCompressed(GENERATOR_COMPRESSED);
    if (p) result = std::move(*p);
#endif
    return result;
}

Point Point::Infinity() {
    return Point();
}

// ============================================================================
// High-Level Operations
// ============================================================================

std::optional<Point> ScalarBaseMultiply(const Scalar& scalar) {
    if (!scalar.IsValid()) return std::nullopt;
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (result.impl_->group && result.impl_->point) {
        BIGNUM* k = BN_bin2bn(scalar.data(), 32, nullptr);
        if (k) {
            EC_POINT_mul(result.impl_->group, result.impl_->point, k, nullptr, nullptr, result.impl_->ctx);
            BN_free(k);
        }
    }
#else
    result = Point::Generator() * scalar;
#endif
    return result;
}

Point ScalarMultiply(const Scalar& scalar, const Point& point) {
    return point * scalar;
}

Point DoubleScalarMultiply(const Scalar& a, const Scalar& b, const Point& P) {
    Point result;
#ifdef SHURIUM_USE_OPENSSL
    if (result.impl_->group && result.impl_->point && P.impl_->point) {
        BIGNUM* ba = BN_bin2bn(a.data(), 32, nullptr);
        BIGNUM* bb = BN_bin2bn(b.data(), 32, nullptr);
        if (ba && bb) {
            EC_POINT_mul(result.impl_->group, result.impl_->point, ba, P.impl_->point, bb, result.impl_->ctx);
        }
        BN_free(ba);
        BN_free(bb);
    }
#else
    auto aG = ScalarBaseMultiply(a);
    if (aG) {
        result = *aG + (P * b);
    }
#endif
    return result;
}

bool IsValidPrivateKey(const uint8_t* key) {
    if (IsZero32(key)) return false;
    return Compare32(key, CURVE_ORDER.data()) < 0;
}

bool IsValidPrivateKey(const std::array<uint8_t, 32>& key) {
    return IsValidPrivateKey(key.data());
}

bool IsValidPublicKey(const uint8_t* point, size_t len) {
    auto p = Point::FromBytes(point, len);
    return p && p->IsOnCurve() && !p->IsInfinity();
}

bool IsValidPublicKey(const std::vector<uint8_t>& point) {
    return IsValidPublicKey(point.data(), point.size());
}

bool PrivateKeyTweakAdd(const uint8_t* key, const uint8_t* tweak, uint8_t* result) {
    Scalar k(key);
    Scalar t(tweak);
    Scalar r = k + t;
    if (!r.IsValid()) return false;
    std::memcpy(result, r.data(), 32);
    return true;
}

bool PublicKeyTweakAdd(const uint8_t* pubkey, size_t pubkeyLen,
                       const uint8_t* tweak, uint8_t* result) {
    auto P = Point::FromBytes(pubkey, pubkeyLen);
    if (!P) return false;
    
    Scalar t(tweak);
    auto tG = ScalarBaseMultiply(t);
    if (!tG) return false;
    
    Point R = *P + *tG;
    if (R.IsInfinity()) return false;
    
    if (pubkeyLen == 33) {
        auto compressed = R.ToCompressed();
        std::memcpy(result, compressed.data(), 33);
    } else {
        auto uncompressed = R.ToUncompressed();
        std::memcpy(result, uncompressed.data(), 65);
    }
    return true;
}

void PrivateKeyNegate(const uint8_t* key, uint8_t* result) {
    Scalar k(key);
    Scalar neg = -k;
    std::memcpy(result, neg.data(), 32);
}

bool PublicKeyNegate(const uint8_t* pubkey, size_t len, uint8_t* result) {
    auto P = Point::FromBytes(pubkey, len);
    if (!P) return false;
    
    Point neg = -*P;
    if (len == 33) {
        auto compressed = neg.ToCompressed();
        std::memcpy(result, compressed.data(), 33);
    } else {
        auto uncompressed = neg.ToUncompressed();
        std::memcpy(result, uncompressed.data(), 65);
    }
    return true;
}

bool PublicKeyCombine(const uint8_t* const* pubkeys, const size_t* sizes,
                      size_t count, uint8_t* result) {
    if (count == 0) return false;
    
    auto P = Point::FromBytes(pubkeys[0], sizes[0]);
    if (!P) return false;
    
    Point sum = *P;
    for (size_t i = 1; i < count; ++i) {
        auto Q = Point::FromBytes(pubkeys[i], sizes[i]);
        if (!Q) return false;
        sum = sum + *Q;
    }
    
    if (sum.IsInfinity()) return false;
    
    auto compressed = sum.ToCompressed();
    std::memcpy(result, compressed.data(), 33);
    return true;
}

// ============================================================================
// ECDSA Operations
// ============================================================================

bool ECDSASign(const uint8_t* hash, const uint8_t* privateKey,
               uint8_t* signature, size_t* sigLen) {
#ifdef SHURIUM_USE_OPENSSL
    EC_KEY* key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!key) return false;
    
    BIGNUM* priv = BN_bin2bn(privateKey, 32, nullptr);
    if (!priv) { EC_KEY_free(key); return false; }
    
    if (EC_KEY_set_private_key(key, priv) != 1) {
        BN_free(priv); EC_KEY_free(key); return false;
    }
    
    unsigned int len = 72;
    ECDSA_SIG* sig = ECDSA_do_sign(hash, 32, key);
    if (!sig) {
        BN_free(priv); EC_KEY_free(key); return false;
    }
    
    *sigLen = i2d_ECDSA_SIG(sig, &signature);
    
    ECDSA_SIG_free(sig);
    BN_free(priv);
    EC_KEY_free(key);
    return *sigLen > 0;
#else
    // Pure C++ ECDSA signing
    // ECDSA: (r, s) where r = (k*G).x mod n, s = k^-1 * (hash + r*privkey) mod n
    
    Scalar d(privateKey);
    if (!d.IsValid()) return false;
    
    Scalar z(hash);  // Message hash as scalar
    
    // Generate deterministic k using RFC 6979-style approach
    // k = HMAC-DRBG(privateKey, hash)
    // For simplicity, we'll use a hash-based approach
    uint8_t kData[64];
    std::memcpy(kData, privateKey, 32);
    std::memcpy(kData + 32, hash, 32);
    Hash256 kHash = SHA256Hash(kData, 64);
    SecureClear(kData, sizeof(kData));
    
    Scalar k(kHash.data());
    
    // Ensure k is valid
    int attempts = 0;
    while (!k.IsValid() && attempts < 100) {
        // Rehash if invalid
        Hash256 newK = SHA256Hash(kHash.data(), 32);
        k = Scalar(newK.data());
        kHash = newK;
        attempts++;
    }
    if (!k.IsValid()) return false;
    
    // Compute R = k * G
    auto R = ScalarBaseMultiply(k);
    if (!R) return false;
    
    // r = R.x mod n
    auto Rx = R->GetX();
    Scalar r(Rx.data());
    if (r.IsZero()) return false;
    
    // s = k^-1 * (z + r * d) mod n
    Scalar kInv = k.Inverse();
    Scalar rd = r * d;
    Scalar sum = z + rd;
    Scalar s = kInv * sum;
    
    if (s.IsZero()) return false;
    
    // Ensure low S (BIP 62)
    // If s > n/2, set s = n - s
    // n/2 ≈ 0x7FFFFFFF...
    uint8_t halfN[32];
    std::memcpy(halfN, CURVE_ORDER.data(), 32);
    // Divide by 2
    for (int i = 0; i < 32; ++i) {
        uint8_t next = (i < 31) ? halfN[i + 1] : 0;
        halfN[i] = (halfN[i] >> 1) | ((next & 1) << 7);
    }
    
    if (Compare32(s.data(), halfN) > 0) {
        s = -s;
    }
    
    // Encode as DER
    // 30 <len> 02 <rlen> <r> 02 <slen> <s>
    
    // Calculate lengths (skip leading zeros, but ensure high bit isn't set)
    int rLen = 32;
    const uint8_t* rData = r.data();
    while (rLen > 1 && rData[32 - rLen] == 0 && !(rData[33 - rLen] & 0x80)) {
        rLen--;
    }
    if (rData[32 - rLen] & 0x80) rLen++;  // Need padding byte
    
    int sLen = 32;
    const uint8_t* sData = s.data();
    while (sLen > 1 && sData[32 - sLen] == 0 && !(sData[33 - sLen] & 0x80)) {
        sLen--;
    }
    if (sData[32 - sLen] & 0x80) sLen++;  // Need padding byte
    
    int totalLen = 2 + rLen + 2 + sLen;
    
    signature[0] = 0x30;
    signature[1] = totalLen;
    signature[2] = 0x02;
    signature[3] = rLen;
    
    int rPad = (rData[32 - (rLen - (rLen > 32 ? 1 : 0))] & 0x80) ? 1 : 0;
    if (rPad) {
        signature[4] = 0;
        std::memcpy(signature + 5, rData + (32 - rLen + 1), rLen - 1);
    } else {
        std::memcpy(signature + 4, rData + (32 - rLen), rLen);
    }
    
    int sOffset = 4 + rLen;
    signature[sOffset] = 0x02;
    signature[sOffset + 1] = sLen;
    
    int sPad = (sData[32 - (sLen - (sLen > 32 ? 1 : 0))] & 0x80) ? 1 : 0;
    if (sPad) {
        signature[sOffset + 2] = 0;
        std::memcpy(signature + sOffset + 3, sData + (32 - sLen + 1), sLen - 1);
    } else {
        std::memcpy(signature + sOffset + 2, sData + (32 - sLen), sLen);
    }
    
    *sigLen = 2 + totalLen;
    return true;
#endif
}

bool ECDSAVerify(const uint8_t* hash,
                 const uint8_t* signature, size_t sigLen,
                 const uint8_t* publicKey, size_t pubkeyLen) {
#ifdef SHURIUM_USE_OPENSSL
    EC_KEY* key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!key) return false;
    
    EC_POINT* pub = EC_POINT_new(EC_KEY_get0_group(key));
    if (!pub) { EC_KEY_free(key); return false; }
    
    if (EC_POINT_oct2point(EC_KEY_get0_group(key), pub, publicKey, pubkeyLen, nullptr) != 1) {
        EC_POINT_free(pub); EC_KEY_free(key); return false;
    }
    
    if (EC_KEY_set_public_key(key, pub) != 1) {
        EC_POINT_free(pub); EC_KEY_free(key); return false;
    }
    
    const uint8_t* sigPtr = signature;
    ECDSA_SIG* sig = d2i_ECDSA_SIG(nullptr, &sigPtr, static_cast<long>(sigLen));
    if (!sig) {
        EC_POINT_free(pub); EC_KEY_free(key); return false;
    }
    
    int result = ECDSA_do_verify(hash, 32, sig, key);
    
    ECDSA_SIG_free(sig);
    EC_POINT_free(pub);
    EC_KEY_free(key);
    return result == 1;
#else
    // Pure C++ ECDSA verification
    // Verify: (u1*G + u2*P).x == r where u1 = z*s^-1, u2 = r*s^-1
    
    // Parse DER signature to get r and s
    if (sigLen < 8 || signature[0] != 0x30) return false;
    
    size_t pos = 2;  // Skip 30 <len>
    if (signature[pos] != 0x02) return false;
    pos++;
    
    uint8_t rLen = signature[pos++];
    if (rLen > 33 || pos + rLen > sigLen) return false;
    
    uint8_t rBytes[32] = {0};
    if (rLen <= 32) {
        std::memcpy(rBytes + (32 - rLen), signature + pos, rLen);
    } else {
        // Skip leading zero padding
        std::memcpy(rBytes, signature + pos + (rLen - 32), 32);
    }
    pos += rLen;
    
    if (signature[pos] != 0x02) return false;
    pos++;
    
    uint8_t sLen = signature[pos++];
    if (sLen > 33 || pos + sLen > sigLen) return false;
    
    uint8_t sBytes[32] = {0};
    if (sLen <= 32) {
        std::memcpy(sBytes + (32 - sLen), signature + pos, sLen);
    } else {
        std::memcpy(sBytes, signature + pos + (sLen - 32), 32);
    }
    
    Scalar r(rBytes);
    Scalar s(sBytes);
    Scalar z(hash);
    
    if (!r.IsValid() || !s.IsValid()) return false;
    
    // Parse public key
    auto P = Point::FromBytes(publicKey, pubkeyLen);
    if (!P || P->IsInfinity()) return false;
    
    // u1 = z * s^-1 mod n
    // u2 = r * s^-1 mod n
    Scalar sInv = s.Inverse();
    Scalar u1 = z * sInv;
    Scalar u2 = r * sInv;
    
    // R = u1*G + u2*P
    Point R = DoubleScalarMultiply(u1, u2, *P);
    
    if (R.IsInfinity()) return false;
    
    // Verify R.x mod n == r
    auto Rx = R.GetX();
    Scalar rCalc(Rx.data());
    
    return rCalc == r;
#endif
}

bool ECDSASignCompact(const uint8_t* hash, const uint8_t* privateKey,
                      uint8_t signature[65]) {
#ifdef SHURIUM_USE_OPENSSL
    EC_KEY* key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!key) return false;
    
    BIGNUM* priv = BN_bin2bn(privateKey, 32, nullptr);
    if (!priv) { EC_KEY_free(key); return false; }
    
    if (EC_KEY_set_private_key(key, priv) != 1) {
        BN_free(priv); EC_KEY_free(key); return false;
    }
    
    // Compute public key
    const EC_GROUP* group = EC_KEY_get0_group(key);
    EC_POINT* pub = EC_POINT_new(group);
    EC_POINT_mul(group, pub, priv, nullptr, nullptr, nullptr);
    EC_KEY_set_public_key(key, pub);
    
    ECDSA_SIG* sig = ECDSA_do_sign(hash, 32, key);
    if (!sig) {
        EC_POINT_free(pub); BN_free(priv); EC_KEY_free(key); return false;
    }
    
    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(sig, &r, &s);
    
    // Recovery ID calculation (simplified)
    signature[0] = 27;  // Base recovery ID
    
    // Copy r and s (32 bytes each)
    int rLen = BN_num_bytes(r);
    int sLen = BN_num_bytes(s);
    std::memset(signature + 1, 0, 64);
    BN_bn2bin(r, signature + 1 + (32 - rLen));
    BN_bn2bin(s, signature + 33 + (32 - sLen));
    
    ECDSA_SIG_free(sig);
    EC_POINT_free(pub);
    BN_free(priv);
    EC_KEY_free(key);
    return true;
#else
    // Pure C++ compact ECDSA signing
    Scalar d(privateKey);
    if (!d.IsValid()) return false;
    
    Scalar z(hash);
    
    // Generate deterministic k
    uint8_t kData[64];
    std::memcpy(kData, privateKey, 32);
    std::memcpy(kData + 32, hash, 32);
    Hash256 kHash = SHA256Hash(kData, 64);
    SecureClear(kData, sizeof(kData));
    
    Scalar k(kHash.data());
    while (!k.IsValid()) {
        Hash256 newK = SHA256Hash(kHash.data(), 32);
        k = Scalar(newK.data());
        kHash = newK;
    }
    
    // R = k * G
    auto R = ScalarBaseMultiply(k);
    if (!R) return false;
    
    // r = R.x mod n
    auto Rx = R->GetX();
    Scalar r(Rx.data());
    if (r.IsZero()) return false;
    
    // s = k^-1 * (z + r * d) mod n
    Scalar kInv = k.Inverse();
    Scalar s = kInv * (z + r * d);
    if (s.IsZero()) return false;
    
    // Determine recovery ID
    int recid = R->GetY().IsOdd() ? 1 : 0;
    // Check if r >= n (rare, requires incrementing recid by 2)
    
    // Ensure low S
    uint8_t halfN[32];
    std::memcpy(halfN, CURVE_ORDER.data(), 32);
    for (int i = 0; i < 32; ++i) {
        uint8_t next = (i < 31) ? halfN[i + 1] : 0;
        halfN[i] = (halfN[i] >> 1) | ((next & 1) << 7);
    }
    if (Compare32(s.data(), halfN) > 0) {
        s = -s;
        recid ^= 1;  // Flip parity when negating s
    }
    
    // Output: recid || r || s
    signature[0] = 27 + recid;
    std::memcpy(signature + 1, r.data(), 32);
    std::memcpy(signature + 33, s.data(), 32);
    
    return true;
#endif
}

bool ECDSARecoverCompact(const uint8_t* hash, const uint8_t signature[65],
                         uint8_t publicKey[33]) {
#ifdef SHURIUM_USE_OPENSSL
    int recid = (signature[0] - 27) & 3;
    
    BIGNUM* r = BN_bin2bn(signature + 1, 32, nullptr);
    BIGNUM* s = BN_bin2bn(signature + 33, 32, nullptr);
    BIGNUM* n = BN_bin2bn(CURVE_ORDER.data(), 32, nullptr);
    BIGNUM* e = BN_bin2bn(hash, 32, nullptr);
    
    if (!r || !s || !n || !e) {
        BN_free(r); BN_free(s); BN_free(n); BN_free(e);
        return false;
    }
    
    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX* ctx = BN_CTX_new();
    
    if (!group || !ctx) {
        BN_free(r); BN_free(s); BN_free(n); BN_free(e);
        if (group) EC_GROUP_free(group);
        if (ctx) BN_CTX_free(ctx);
        return false;
    }
    
    // Calculate R point from r (x-coordinate) and recid
    EC_POINT* R = EC_POINT_new(group);
    EC_POINT* Q = EC_POINT_new(group);
    
    // x = r + (recid / 2) * n  (usually recid < 2 so x = r)
    BIGNUM* x = BN_dup(r);
    if (recid >= 2) {
        BN_add(x, x, n);
    }
    
    // Try to find point with this x-coordinate
    // y^2 = x^3 + 7 mod p
    if (!EC_POINT_set_compressed_coordinates(group, R, x, recid & 1, ctx)) {
        BN_free(r); BN_free(s); BN_free(n); BN_free(e); BN_free(x);
        EC_POINT_free(R); EC_POINT_free(Q);
        EC_GROUP_free(group); BN_CTX_free(ctx);
        return false;
    }
    
    // Q = r^-1 * (s*R - e*G)
    BIGNUM* rInv = BN_mod_inverse(nullptr, r, n, ctx);
    BIGNUM* sTimesRInv = BN_new();
    BIGNUM* eTimesRInv = BN_new();
    
    BN_mod_mul(sTimesRInv, s, rInv, n, ctx);
    BN_mod_mul(eTimesRInv, e, rInv, n, ctx);
    BN_mod_sub(eTimesRInv, n, eTimesRInv, n, ctx);  // Negate
    
    // Q = sTimesRInv * R + eTimesRInv * G
    EC_POINT_mul(group, Q, eTimesRInv, R, sTimesRInv, ctx);
    
    // Convert to compressed format
    EC_POINT_point2oct(group, Q, POINT_CONVERSION_COMPRESSED,
                       publicKey, 33, ctx);
    
    // Cleanup
    BN_free(r); BN_free(s); BN_free(n); BN_free(e); BN_free(x);
    BN_free(rInv); BN_free(sTimesRInv); BN_free(eTimesRInv);
    EC_POINT_free(R); EC_POINT_free(Q);
    EC_GROUP_free(group); BN_CTX_free(ctx);
    return true;
#else
    // Pure C++ ECDSA recovery
    int recid = (signature[0] - 27) & 3;
    
    Scalar r(signature + 1);
    Scalar s(signature + 33);
    Scalar z(hash);
    
    if (!r.IsValid() || !s.IsValid()) return false;
    
    // Compute R from r (the x-coordinate)
    // R.x = r (or r + n if recid >= 2)
    uint8_t Rx[32];
    std::memcpy(Rx, r.data(), 32);
    
    // Try to construct point R with x = r and appropriate y parity
    uint8_t compressed[33];
    compressed[0] = (recid & 1) ? 0x03 : 0x02;  // Odd/even y
    std::memcpy(compressed + 1, Rx, 32);
    
    auto R = Point::FromCompressed(compressed);
    if (!R) return false;
    
    // Compute public key: P = r^-1 * (s*R - z*G)
    Scalar rInv = r.Inverse();
    Scalar sTimesRInv = s * rInv;
    Scalar zTimesRInv = z * rInv;
    Scalar negZTimesRInv = -zTimesRInv;
    
    // P = sTimesRInv * R + negZTimesRInv * G
    Point P = DoubleScalarMultiply(negZTimesRInv, sTimesRInv, *R);
    
    if (P.IsInfinity()) return false;
    
    // Output compressed public key
    auto compressedPub = P.ToCompressed();
    std::memcpy(publicKey, compressedPub.data(), 33);
    
    return true;
#endif
}

// ============================================================================
// Schnorr Signatures (BIP340)
// ============================================================================

namespace {

/// BIP340 tagged hash: SHA256(SHA256(tag) || SHA256(tag) || msg)
/// Precomputed tag hashes for efficiency
Hash256 TaggedHash(const char* tag, const uint8_t* data, size_t len) {
    // Compute SHA256(tag)
    Hash256 tagHash = SHA256Hash(reinterpret_cast<const uint8_t*>(tag), std::strlen(tag));
    
    // Build input: tagHash || tagHash || data
    std::vector<uint8_t> input;
    input.reserve(64 + len);
    input.insert(input.end(), tagHash.begin(), tagHash.end());
    input.insert(input.end(), tagHash.begin(), tagHash.end());
    input.insert(input.end(), data, data + len);
    
    return SHA256Hash(input.data(), input.size());
}

/// Precomputed tag hash midstates for BIP340 tags (optimization)
/// BIP0340/aux: used for auxiliary randomness
/// BIP0340/nonce: used for nonce generation
/// BIP0340/challenge: used for challenge computation

} // anonymous namespace

bool SchnorrSign(const uint8_t* hash, const uint8_t* privateKey,
                 uint8_t signature[64]) {
#ifdef SHURIUM_USE_OPENSSL
    // BIP340 Schnorr signing
    // https://github.com/bitcoin/bips/blob/master/bip-0340.mediawiki
    
    Scalar d(privateKey);
    if (!d.IsValid()) return false;
    
    // Compute public key P = d*G
    auto P = ScalarBaseMultiply(d);
    if (!P) return false;
    
    // Get x-coordinate of P (for x-only pubkey)
    auto Px = P->GetX();
    
    // If P.y is odd, negate d (BIP340: key with even y)
    if (P->GetY().IsOdd()) {
        d = -d;
    }
    
    // Generate deterministic nonce k using BIP340 algorithm:
    // Let t = bytes(d) XOR tagged_hash("BIP0340/aux", a)
    // where a = auxiliary random data (we use hash as aux data for determinism)
    // Let rand = tagged_hash("BIP0340/nonce", t || bytes(P) || m)
    // Let k' = int(rand) mod n
    
    // Step 1: Compute t = d XOR tagged_hash("BIP0340/aux", hash)
    Hash256 auxHash = TaggedHash("BIP0340/aux", hash, 32);
    uint8_t t[32];
    for (int i = 0; i < 32; ++i) {
        t[i] = d.data()[i] ^ auxHash[i];
    }
    
    // Step 2: Compute k' = tagged_hash("BIP0340/nonce", t || P.x || m)
    uint8_t nonceInput[96];
    std::memcpy(nonceInput, t, 32);
    std::memcpy(nonceInput + 32, Px.data(), 32);
    std::memcpy(nonceInput + 64, hash, 32);
    Hash256 kHash = TaggedHash("BIP0340/nonce", nonceInput, 96);
    Scalar k(kHash.data());
    
    // k must be non-zero
    if (k.IsZero()) {
        SecureClear(t, sizeof(t));
        SecureClear(nonceInput, sizeof(nonceInput));
        return false;
    }
    
    // Compute R = k*G
    auto R = ScalarBaseMultiply(k);
    if (!R) {
        SecureClear(t, sizeof(t));
        SecureClear(nonceInput, sizeof(nonceInput));
        return false;
    }
    
    // If R.y is odd, negate k (BIP340: R must have even y)
    if (R->GetY().IsOdd()) {
        k = -k;
    }
    
    auto Rx = R->GetX();
    
    // Step 3: Compute challenge e = tagged_hash("BIP0340/challenge", R.x || P.x || m)
    uint8_t challengeInput[96];
    std::memcpy(challengeInput, Rx.data(), 32);
    std::memcpy(challengeInput + 32, Px.data(), 32);
    std::memcpy(challengeInput + 64, hash, 32);
    Hash256 eHash = TaggedHash("BIP0340/challenge", challengeInput, 96);
    Scalar e(eHash.data());
    
    // Step 4: Compute s = k + e*d mod n
    Scalar s = k + (e * d);
    
    // Output signature (R.x || s)
    std::memcpy(signature, Rx.data(), 32);
    std::memcpy(signature + 32, s.data(), 32);
    
    // Secure cleanup
    SecureClear(t, sizeof(t));
    SecureClear(nonceInput, sizeof(nonceInput));
    SecureClear(challengeInput, sizeof(challengeInput));
    
    return true;
#else
    // Pure C++ BIP340 Schnorr signing
    Scalar d(privateKey);
    if (!d.IsValid()) return false;
    
    // Compute public key P = d*G
    auto P = ScalarBaseMultiply(d);
    if (!P) return false;
    
    auto Px = P->GetX();
    
    // If P.y is odd, negate d
    if (P->GetY().IsOdd()) {
        d = -d;
    }
    
    // Generate nonce k using BIP340 algorithm
    Hash256 auxHash = TaggedHash("BIP0340/aux", hash, 32);
    uint8_t t[32];
    for (int i = 0; i < 32; ++i) {
        t[i] = d.data()[i] ^ auxHash[i];
    }
    
    uint8_t nonceInput[96];
    std::memcpy(nonceInput, t, 32);
    std::memcpy(nonceInput + 32, Px.data(), 32);
    std::memcpy(nonceInput + 64, hash, 32);
    Hash256 kHash = TaggedHash("BIP0340/nonce", nonceInput, 96);
    Scalar k(kHash.data());
    
    if (k.IsZero()) {
        SecureClear(t, sizeof(t));
        SecureClear(nonceInput, sizeof(nonceInput));
        return false;
    }
    
    // R = k*G
    auto R = ScalarBaseMultiply(k);
    if (!R) {
        SecureClear(t, sizeof(t));
        SecureClear(nonceInput, sizeof(nonceInput));
        return false;
    }
    
    // If R.y is odd, negate k
    if (R->GetY().IsOdd()) {
        k = -k;
    }
    
    auto Rx = R->GetX();
    
    // Challenge e = tagged_hash("BIP0340/challenge", R.x || P.x || m)
    uint8_t challengeInput[96];
    std::memcpy(challengeInput, Rx.data(), 32);
    std::memcpy(challengeInput + 32, Px.data(), 32);
    std::memcpy(challengeInput + 64, hash, 32);
    Hash256 eHash = TaggedHash("BIP0340/challenge", challengeInput, 96);
    Scalar e(eHash.data());
    
    // s = k + e*d mod n
    Scalar s = k + (e * d);
    
    // Output signature (R.x || s)
    std::memcpy(signature, Rx.data(), 32);
    std::memcpy(signature + 32, s.data(), 32);
    
    SecureClear(t, sizeof(t));
    SecureClear(nonceInput, sizeof(nonceInput));
    SecureClear(challengeInput, sizeof(challengeInput));
    
    return true;
#endif
}

bool SchnorrVerify(const uint8_t* hash, const uint8_t signature[64],
                   const uint8_t publicKey[32]) {
#ifdef SHURIUM_USE_OPENSSL
    // BIP340 Schnorr verification
    // https://github.com/bitcoin/bips/blob/master/bip-0340.mediawiki
    
    // Step 1: Verify signature components are valid
    // r = signature[0:32] (x-coordinate of R)
    // s = signature[32:64] (scalar)
    
    // Check r < p (field prime)
    if (Compare32(signature, FIELD_PRIME.data()) >= 0) {
        return false;
    }
    
    Scalar s(signature + 32);
    
    // Check s < n (curve order)
    if (!s.IsValid() && !s.IsZero()) {
        return false;
    }
    // s = 0 is technically valid but unusual
    
    // Step 2: Lift x-only public key to full point
    // BIP340 specifies P has even y coordinate
    uint8_t compressed[33];
    compressed[0] = 0x02;  // Try even y first
    std::memcpy(compressed + 1, publicKey, 32);
    
    auto P = Point::FromCompressed(compressed);
    if (!P || P->IsInfinity()) {
        // If even y doesn't work, the public key is invalid
        // (BIP340 keys always have even y)
        return false;
    }
    
    // Step 3: Compute challenge e = tagged_hash("BIP0340/challenge", r || P.x || m)
    uint8_t challengeInput[96];
    std::memcpy(challengeInput, signature, 32);      // r (R.x from signature)
    std::memcpy(challengeInput + 32, publicKey, 32); // P.x
    std::memcpy(challengeInput + 64, hash, 32);      // message hash
    Hash256 eHash = TaggedHash("BIP0340/challenge", challengeInput, 96);
    Scalar e(eHash.data());
    
    // Step 4: Compute R' = s*G - e*P
    Scalar negE = -e;
    Point Rprime = DoubleScalarMultiply(s, negE, *P);
    
    // Step 5: Verify R'
    // R' must not be infinity
    if (Rprime.IsInfinity()) {
        return false;
    }
    
    // R'.y must be even (BIP340 requirement)
    if (Rprime.GetY().IsOdd()) {
        return false;
    }
    
    // R'.x must equal r (first 32 bytes of signature)
    auto RprimeX = Rprime.GetX();
    return std::memcmp(RprimeX.data(), signature, 32) == 0;
#else
    // Pure C++ BIP340 Schnorr verification
    
    // Check r < p
    if (Compare32(signature, FIELD_PRIME.data()) >= 0) {
        return false;
    }
    
    Scalar s(signature + 32);
    if (!s.IsValid() && !s.IsZero()) {
        return false;
    }
    
    // Lift x-only public key (BIP340 keys have even y)
    uint8_t compressed[33];
    compressed[0] = 0x02;
    std::memcpy(compressed + 1, publicKey, 32);
    
    auto P = Point::FromCompressed(compressed);
    if (!P || P->IsInfinity()) {
        return false;
    }
    
    // Challenge e = tagged_hash("BIP0340/challenge", r || P.x || m)
    uint8_t challengeInput[96];
    std::memcpy(challengeInput, signature, 32);
    std::memcpy(challengeInput + 32, publicKey, 32);
    std::memcpy(challengeInput + 64, hash, 32);
    Hash256 eHash = TaggedHash("BIP0340/challenge", challengeInput, 96);
    Scalar e(eHash.data());
    
    // R' = s*G - e*P
    Scalar negE = -e;
    Point Rprime = DoubleScalarMultiply(s, negE, *P);
    
    if (Rprime.IsInfinity()) {
        return false;
    }
    
    // R'.y must be even
    if (Rprime.GetY().IsOdd()) {
        return false;
    }
    
    // R'.x must equal r
    auto RprimeX = Rprime.GetX();
    return std::memcmp(RprimeX.data(), signature, 32) == 0;
#endif
}

} // namespace secp256k1
} // namespace shurium
