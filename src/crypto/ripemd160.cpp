// SHURIUM - RIPEMD160 Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// RIPEMD-160 implementation

#include "shurium/crypto/ripemd160.h"
#include "shurium/crypto/sha256.h"
#include <cstring>

namespace shurium {

// ============================================================================
// RIPEMD-160 Constants and Helper Functions
// ============================================================================

namespace {

/// Initial hash values
constexpr uint32_t RIPEMD160_INIT[5] = {
    0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
};

// Non-linear functions for each round
inline uint32_t f1(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
inline uint32_t f2(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
inline uint32_t f3(uint32_t x, uint32_t y, uint32_t z) { return (x | ~y) ^ z; }
inline uint32_t f4(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
inline uint32_t f5(uint32_t x, uint32_t y, uint32_t z) { return x ^ (y | ~z); }

// Left rotate
inline uint32_t ROL(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

// Round function
inline void Round(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e,
                  uint32_t f, uint32_t x, uint32_t k, int r) {
    a = ROL(a + f + x + k, r) + e;
    c = ROL(c, 10);
}

// Round functions for left path
inline void R11(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f1(b, c, d), x, 0, r);
}
inline void R21(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f2(b, c, d), x, 0x5A827999, r);
}
inline void R31(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f3(b, c, d), x, 0x6ED9EBA1, r);
}
inline void R41(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f4(b, c, d), x, 0x8F1BBCDC, r);
}
inline void R51(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f5(b, c, d), x, 0xA953FD4E, r);
}

// Round functions for right path
inline void R12(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f5(b, c, d), x, 0x50A28BE6, r);
}
inline void R22(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f4(b, c, d), x, 0x5C4DD124, r);
}
inline void R32(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f3(b, c, d), x, 0x6D703EF3, r);
}
inline void R42(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f2(b, c, d), x, 0x7A6D76E9, r);
}
inline void R52(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) {
    Round(a, b, c, d, e, f1(b, c, d), x, 0, r);
}

/// Read a 32-bit little-endian word from byte array
inline uint32_t ReadLE32(const Byte* ptr) {
    return static_cast<uint32_t>(ptr[0]) |
           (static_cast<uint32_t>(ptr[1]) << 8) |
           (static_cast<uint32_t>(ptr[2]) << 16) |
           (static_cast<uint32_t>(ptr[3]) << 24);
}

/// Write a 32-bit little-endian word to byte array
inline void WriteLE32(Byte* ptr, uint32_t val) {
    ptr[0] = static_cast<Byte>(val);
    ptr[1] = static_cast<Byte>(val >> 8);
    ptr[2] = static_cast<Byte>(val >> 16);
    ptr[3] = static_cast<Byte>(val >> 24);
}

/// Write a 64-bit little-endian word to byte array
inline void WriteLE64(Byte* ptr, uint64_t val) {
    ptr[0] = static_cast<Byte>(val);
    ptr[1] = static_cast<Byte>(val >> 8);
    ptr[2] = static_cast<Byte>(val >> 16);
    ptr[3] = static_cast<Byte>(val >> 24);
    ptr[4] = static_cast<Byte>(val >> 32);
    ptr[5] = static_cast<Byte>(val >> 40);
    ptr[6] = static_cast<Byte>(val >> 48);
    ptr[7] = static_cast<Byte>(val >> 56);
}

} // anonymous namespace

// ============================================================================
// RIPEMD160 Implementation
// ============================================================================

RIPEMD160::RIPEMD160() {
    Reset();
}

RIPEMD160& RIPEMD160::Reset() {
    std::memcpy(state_, RIPEMD160_INIT, sizeof(state_));
    bytes_ = 0;
    return *this;
}

void RIPEMD160::Transform(const Byte block[BLOCK_SIZE]) {
    uint32_t a1 = state_[0], b1 = state_[1], c1 = state_[2], d1 = state_[3], e1 = state_[4];
    uint32_t a2 = a1, b2 = b1, c2 = c1, d2 = d1, e2 = e1;
    
    // Read message words
    uint32_t w0  = ReadLE32(block + 0),  w1  = ReadLE32(block + 4);
    uint32_t w2  = ReadLE32(block + 8),  w3  = ReadLE32(block + 12);
    uint32_t w4  = ReadLE32(block + 16), w5  = ReadLE32(block + 20);
    uint32_t w6  = ReadLE32(block + 24), w7  = ReadLE32(block + 28);
    uint32_t w8  = ReadLE32(block + 32), w9  = ReadLE32(block + 36);
    uint32_t w10 = ReadLE32(block + 40), w11 = ReadLE32(block + 44);
    uint32_t w12 = ReadLE32(block + 48), w13 = ReadLE32(block + 52);
    uint32_t w14 = ReadLE32(block + 56), w15 = ReadLE32(block + 60);
    
    // Round 1 - both paths
    R11(a1, b1, c1, d1, e1, w0, 11);  R12(a2, b2, c2, d2, e2, w5, 8);
    R11(e1, a1, b1, c1, d1, w1, 14);  R12(e2, a2, b2, c2, d2, w14, 9);
    R11(d1, e1, a1, b1, c1, w2, 15);  R12(d2, e2, a2, b2, c2, w7, 9);
    R11(c1, d1, e1, a1, b1, w3, 12);  R12(c2, d2, e2, a2, b2, w0, 11);
    R11(b1, c1, d1, e1, a1, w4, 5);   R12(b2, c2, d2, e2, a2, w9, 13);
    R11(a1, b1, c1, d1, e1, w5, 8);   R12(a2, b2, c2, d2, e2, w2, 15);
    R11(e1, a1, b1, c1, d1, w6, 7);   R12(e2, a2, b2, c2, d2, w11, 15);
    R11(d1, e1, a1, b1, c1, w7, 9);   R12(d2, e2, a2, b2, c2, w4, 5);
    R11(c1, d1, e1, a1, b1, w8, 11);  R12(c2, d2, e2, a2, b2, w13, 7);
    R11(b1, c1, d1, e1, a1, w9, 13);  R12(b2, c2, d2, e2, a2, w6, 7);
    R11(a1, b1, c1, d1, e1, w10, 14); R12(a2, b2, c2, d2, e2, w15, 8);
    R11(e1, a1, b1, c1, d1, w11, 15); R12(e2, a2, b2, c2, d2, w8, 11);
    R11(d1, e1, a1, b1, c1, w12, 6);  R12(d2, e2, a2, b2, c2, w1, 14);
    R11(c1, d1, e1, a1, b1, w13, 7);  R12(c2, d2, e2, a2, b2, w10, 14);
    R11(b1, c1, d1, e1, a1, w14, 9);  R12(b2, c2, d2, e2, a2, w3, 12);
    R11(a1, b1, c1, d1, e1, w15, 8);  R12(a2, b2, c2, d2, e2, w12, 6);
    
    // Round 2 - both paths
    R21(e1, a1, b1, c1, d1, w7, 7);   R22(e2, a2, b2, c2, d2, w6, 9);
    R21(d1, e1, a1, b1, c1, w4, 6);   R22(d2, e2, a2, b2, c2, w11, 13);
    R21(c1, d1, e1, a1, b1, w13, 8);  R22(c2, d2, e2, a2, b2, w3, 15);
    R21(b1, c1, d1, e1, a1, w1, 13);  R22(b2, c2, d2, e2, a2, w7, 7);
    R21(a1, b1, c1, d1, e1, w10, 11); R22(a2, b2, c2, d2, e2, w0, 12);
    R21(e1, a1, b1, c1, d1, w6, 9);   R22(e2, a2, b2, c2, d2, w13, 8);
    R21(d1, e1, a1, b1, c1, w15, 7);  R22(d2, e2, a2, b2, c2, w5, 9);
    R21(c1, d1, e1, a1, b1, w3, 15);  R22(c2, d2, e2, a2, b2, w10, 11);
    R21(b1, c1, d1, e1, a1, w12, 7);  R22(b2, c2, d2, e2, a2, w14, 7);
    R21(a1, b1, c1, d1, e1, w0, 12);  R22(a2, b2, c2, d2, e2, w15, 7);
    R21(e1, a1, b1, c1, d1, w9, 15);  R22(e2, a2, b2, c2, d2, w8, 12);
    R21(d1, e1, a1, b1, c1, w5, 9);   R22(d2, e2, a2, b2, c2, w12, 7);
    R21(c1, d1, e1, a1, b1, w2, 11);  R22(c2, d2, e2, a2, b2, w4, 6);
    R21(b1, c1, d1, e1, a1, w14, 7);  R22(b2, c2, d2, e2, a2, w9, 15);
    R21(a1, b1, c1, d1, e1, w11, 13); R22(a2, b2, c2, d2, e2, w1, 13);
    R21(e1, a1, b1, c1, d1, w8, 12);  R22(e2, a2, b2, c2, d2, w2, 11);
    
    // Round 3 - both paths
    R31(d1, e1, a1, b1, c1, w3, 11);  R32(d2, e2, a2, b2, c2, w15, 9);
    R31(c1, d1, e1, a1, b1, w10, 13); R32(c2, d2, e2, a2, b2, w5, 7);
    R31(b1, c1, d1, e1, a1, w14, 6);  R32(b2, c2, d2, e2, a2, w1, 15);
    R31(a1, b1, c1, d1, e1, w4, 7);   R32(a2, b2, c2, d2, e2, w3, 11);
    R31(e1, a1, b1, c1, d1, w9, 14);  R32(e2, a2, b2, c2, d2, w7, 8);
    R31(d1, e1, a1, b1, c1, w15, 9);  R32(d2, e2, a2, b2, c2, w14, 6);
    R31(c1, d1, e1, a1, b1, w8, 13);  R32(c2, d2, e2, a2, b2, w6, 6);
    R31(b1, c1, d1, e1, a1, w1, 15);  R32(b2, c2, d2, e2, a2, w9, 14);
    R31(a1, b1, c1, d1, e1, w2, 14);  R32(a2, b2, c2, d2, e2, w11, 12);
    R31(e1, a1, b1, c1, d1, w7, 8);   R32(e2, a2, b2, c2, d2, w8, 13);
    R31(d1, e1, a1, b1, c1, w0, 13);  R32(d2, e2, a2, b2, c2, w12, 5);
    R31(c1, d1, e1, a1, b1, w6, 6);   R32(c2, d2, e2, a2, b2, w2, 14);
    R31(b1, c1, d1, e1, a1, w13, 5);  R32(b2, c2, d2, e2, a2, w10, 13);
    R31(a1, b1, c1, d1, e1, w11, 12); R32(a2, b2, c2, d2, e2, w0, 13);
    R31(e1, a1, b1, c1, d1, w5, 7);   R32(e2, a2, b2, c2, d2, w4, 7);
    R31(d1, e1, a1, b1, c1, w12, 5);  R32(d2, e2, a2, b2, c2, w13, 5);
    
    // Round 4 - both paths
    R41(c1, d1, e1, a1, b1, w1, 11);  R42(c2, d2, e2, a2, b2, w8, 15);
    R41(b1, c1, d1, e1, a1, w9, 12);  R42(b2, c2, d2, e2, a2, w6, 5);
    R41(a1, b1, c1, d1, e1, w11, 14); R42(a2, b2, c2, d2, e2, w4, 8);
    R41(e1, a1, b1, c1, d1, w10, 15); R42(e2, a2, b2, c2, d2, w1, 11);
    R41(d1, e1, a1, b1, c1, w0, 14);  R42(d2, e2, a2, b2, c2, w3, 14);
    R41(c1, d1, e1, a1, b1, w8, 15);  R42(c2, d2, e2, a2, b2, w11, 14);
    R41(b1, c1, d1, e1, a1, w12, 9);  R42(b2, c2, d2, e2, a2, w15, 6);
    R41(a1, b1, c1, d1, e1, w4, 8);   R42(a2, b2, c2, d2, e2, w0, 14);
    R41(e1, a1, b1, c1, d1, w13, 9);  R42(e2, a2, b2, c2, d2, w5, 6);
    R41(d1, e1, a1, b1, c1, w3, 14);  R42(d2, e2, a2, b2, c2, w12, 9);
    R41(c1, d1, e1, a1, b1, w7, 5);   R42(c2, d2, e2, a2, b2, w2, 12);
    R41(b1, c1, d1, e1, a1, w15, 6);  R42(b2, c2, d2, e2, a2, w13, 9);
    R41(a1, b1, c1, d1, e1, w14, 8);  R42(a2, b2, c2, d2, e2, w9, 12);
    R41(e1, a1, b1, c1, d1, w5, 6);   R42(e2, a2, b2, c2, d2, w7, 5);
    R41(d1, e1, a1, b1, c1, w6, 5);   R42(d2, e2, a2, b2, c2, w10, 15);
    R41(c1, d1, e1, a1, b1, w2, 12);  R42(c2, d2, e2, a2, b2, w14, 8);
    
    // Round 5 - both paths
    R51(b1, c1, d1, e1, a1, w4, 9);   R52(b2, c2, d2, e2, a2, w12, 8);
    R51(a1, b1, c1, d1, e1, w0, 15);  R52(a2, b2, c2, d2, e2, w15, 5);
    R51(e1, a1, b1, c1, d1, w5, 5);   R52(e2, a2, b2, c2, d2, w10, 12);
    R51(d1, e1, a1, b1, c1, w9, 11);  R52(d2, e2, a2, b2, c2, w4, 9);
    R51(c1, d1, e1, a1, b1, w7, 6);   R52(c2, d2, e2, a2, b2, w1, 12);
    R51(b1, c1, d1, e1, a1, w12, 8);  R52(b2, c2, d2, e2, a2, w5, 5);
    R51(a1, b1, c1, d1, e1, w2, 13);  R52(a2, b2, c2, d2, e2, w8, 14);
    R51(e1, a1, b1, c1, d1, w10, 12); R52(e2, a2, b2, c2, d2, w7, 6);
    R51(d1, e1, a1, b1, c1, w14, 5);  R52(d2, e2, a2, b2, c2, w6, 8);
    R51(c1, d1, e1, a1, b1, w1, 12);  R52(c2, d2, e2, a2, b2, w2, 13);
    R51(b1, c1, d1, e1, a1, w3, 13);  R52(b2, c2, d2, e2, a2, w13, 6);
    R51(a1, b1, c1, d1, e1, w8, 14);  R52(a2, b2, c2, d2, e2, w14, 5);
    R51(e1, a1, b1, c1, d1, w11, 11); R52(e2, a2, b2, c2, d2, w0, 15);
    R51(d1, e1, a1, b1, c1, w6, 8);   R52(d2, e2, a2, b2, c2, w3, 13);
    R51(c1, d1, e1, a1, b1, w15, 5);  R52(c2, d2, e2, a2, b2, w9, 11);
    R51(b1, c1, d1, e1, a1, w13, 6);  R52(b2, c2, d2, e2, a2, w11, 11);
    
    // Final addition
    uint32_t t = state_[0];
    state_[0] = state_[1] + c1 + d2;
    state_[1] = state_[2] + d1 + e2;
    state_[2] = state_[3] + e1 + a2;
    state_[3] = state_[4] + a1 + b2;
    state_[4] = t + b1 + c2;
}

RIPEMD160& RIPEMD160::Write(const Byte* data, size_t len) {
    if (data == nullptr || len == 0) {
        return *this;
    }
    
    const Byte* end = data + len;
    size_t bufPos = bytes_ % BLOCK_SIZE;
    
    if (bufPos && bufPos + len >= BLOCK_SIZE) {
        // Fill and process buffer
        std::memcpy(buffer_ + bufPos, data, BLOCK_SIZE - bufPos);
        bytes_ += BLOCK_SIZE - bufPos;
        data += BLOCK_SIZE - bufPos;
        Transform(buffer_);
        bufPos = 0;
    }
    
    while (end - data >= static_cast<ptrdiff_t>(BLOCK_SIZE)) {
        Transform(data);
        bytes_ += BLOCK_SIZE;
        data += BLOCK_SIZE;
    }
    
    if (end > data) {
        std::memcpy(buffer_ + bufPos, data, end - data);
        bytes_ += end - data;
    }
    
    return *this;
}

void RIPEMD160::Finalize(Byte hash[OUTPUT_SIZE]) {
    static const Byte pad[64] = {0x80};
    Byte sizedesc[8];
    WriteLE64(sizedesc, bytes_ << 3);
    Write(pad, 1 + ((119 - (bytes_ % 64)) % 64));
    Write(sizedesc, 8);
    WriteLE32(hash, state_[0]);
    WriteLE32(hash + 4, state_[1]);
    WriteLE32(hash + 8, state_[2]);
    WriteLE32(hash + 12, state_[3]);
    WriteLE32(hash + 16, state_[4]);
}

// ============================================================================
// Convenience Functions
// ============================================================================

Hash160 RIPEMD160Hash(const Byte* data, size_t len) {
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    hasher.Write(data, len);
    hasher.Finalize(hash.data());
    
    return Hash160(hash);
}

Hash160 Hash160FromData(const Byte* data, size_t len) {
    // Hash160 = RIPEMD160(SHA256(data))
    std::array<Byte, SHA256::OUTPUT_SIZE> sha256Hash;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> ripemd160Hash;
    
    // First compute SHA256
    SHA256 sha256;
    sha256.Write(data, len);
    sha256.Finalize(sha256Hash.data());
    
    // Then RIPEMD160 of the SHA256 result
    RIPEMD160 ripemd160;
    ripemd160.Write(sha256Hash.data(), sha256Hash.size());
    ripemd160.Finalize(ripemd160Hash.data());
    
    return Hash160(ripemd160Hash);
}

} // namespace shurium
