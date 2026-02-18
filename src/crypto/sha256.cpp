// SHURIUM - SHA256 Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// SHA-256 implementation following FIPS 180-4
// Reference: https://csrc.nist.gov/publications/detail/fips/180/4/final

#include "shurium/crypto/sha256.h"
#include <cstring>

namespace shurium {

// ============================================================================
// SHA-256 Constants
// ============================================================================

namespace {

/// Initial hash values (first 32 bits of fractional parts of square roots of first 8 primes)
constexpr uint32_t SHA256_INIT[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/// Round constants (first 32 bits of fractional parts of cube roots of first 64 primes)
constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// ============================================================================
// Helper Functions
// ============================================================================

/// Right rotate a 32-bit word
inline uint32_t ROTR(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

/// SHA-256 Ch function
inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

/// SHA-256 Maj function
inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

/// SHA-256 Sigma0 function
inline uint32_t Sigma0(uint32_t x) {
    return ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22);
}

/// SHA-256 Sigma1 function
inline uint32_t Sigma1(uint32_t x) {
    return ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25);
}

/// SHA-256 sigma0 function (message schedule)
inline uint32_t sigma0(uint32_t x) {
    return ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3);
}

/// SHA-256 sigma1 function (message schedule)
inline uint32_t sigma1(uint32_t x) {
    return ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10);
}

/// Read a 32-bit big-endian word from byte array
inline uint32_t ReadBE32(const Byte* ptr) {
    return (static_cast<uint32_t>(ptr[0]) << 24) |
           (static_cast<uint32_t>(ptr[1]) << 16) |
           (static_cast<uint32_t>(ptr[2]) << 8) |
           static_cast<uint32_t>(ptr[3]);
}

/// Write a 32-bit big-endian word to byte array
inline void WriteBE32(Byte* ptr, uint32_t val) {
    ptr[0] = static_cast<Byte>(val >> 24);
    ptr[1] = static_cast<Byte>(val >> 16);
    ptr[2] = static_cast<Byte>(val >> 8);
    ptr[3] = static_cast<Byte>(val);
}

/// Write a 64-bit big-endian word to byte array
inline void WriteBE64(Byte* ptr, uint64_t val) {
    ptr[0] = static_cast<Byte>(val >> 56);
    ptr[1] = static_cast<Byte>(val >> 48);
    ptr[2] = static_cast<Byte>(val >> 40);
    ptr[3] = static_cast<Byte>(val >> 32);
    ptr[4] = static_cast<Byte>(val >> 24);
    ptr[5] = static_cast<Byte>(val >> 16);
    ptr[6] = static_cast<Byte>(val >> 8);
    ptr[7] = static_cast<Byte>(val);
}

} // anonymous namespace

// ============================================================================
// SHA256 Implementation
// ============================================================================

SHA256::SHA256() {
    Reset();
}

SHA256& SHA256::Reset() {
    // Initialize state with SHA-256 initial values
    std::memcpy(state_, SHA256_INIT, sizeof(state_));
    bytes_ = 0;
    return *this;
}

void SHA256::Transform(const Byte block[BLOCK_SIZE]) {
    // Message schedule array
    uint32_t W[64];
    
    // Prepare the message schedule
    for (int i = 0; i < 16; ++i) {
        W[i] = ReadBE32(block + i * 4);
    }
    for (int i = 16; i < 64; ++i) {
        W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];
    }
    
    // Working variables
    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];
    
    // Main loop (64 rounds)
    for (int i = 0; i < 64; ++i) {
        uint32_t T1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
        uint32_t T2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }
    
    // Update state
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

SHA256& SHA256::Write(const Byte* data, size_t len) {
    if (data == nullptr || len == 0) {
        return *this;
    }
    
    // Current position in buffer
    size_t bufPos = bytes_ % BLOCK_SIZE;
    bytes_ += len;
    
    // If we have data in buffer, try to complete a block
    if (bufPos > 0) {
        size_t needed = BLOCK_SIZE - bufPos;
        if (len < needed) {
            std::memcpy(buffer_ + bufPos, data, len);
            return *this;
        }
        std::memcpy(buffer_ + bufPos, data, needed);
        Transform(buffer_);
        data += needed;
        len -= needed;
    }
    
    // Process complete blocks
    while (len >= BLOCK_SIZE) {
        Transform(data);
        data += BLOCK_SIZE;
        len -= BLOCK_SIZE;
    }
    
    // Store remaining bytes in buffer
    if (len > 0) {
        std::memcpy(buffer_, data, len);
    }
    
    return *this;
}

void SHA256::Finalize(Byte hash[OUTPUT_SIZE]) {
    // Padding
    Byte pad[BLOCK_SIZE];
    size_t bufPos = bytes_ % BLOCK_SIZE;
    
    // Copy remaining data to pad
    std::memcpy(pad, buffer_, bufPos);
    
    // Append bit '1'
    pad[bufPos++] = 0x80;
    
    // If not enough room for length, pad to block boundary and process
    if (bufPos > 56) {
        std::memset(pad + bufPos, 0, BLOCK_SIZE - bufPos);
        Transform(pad);
        bufPos = 0;
        std::memset(pad, 0, 56);
    } else {
        std::memset(pad + bufPos, 0, 56 - bufPos);
    }
    
    // Append length in bits (big-endian, 64-bit)
    uint64_t bits = bytes_ * 8;
    WriteBE64(pad + 56, bits);
    Transform(pad);
    
    // Output hash (big-endian)
    for (int i = 0; i < 8; ++i) {
        WriteBE32(hash + i * 4, state_[i]);
    }
}

// ============================================================================
// Convenience Functions
// ============================================================================

Hash256 SHA256Hash(const Byte* data, size_t len) {
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    hasher.Write(data, len);
    hasher.Finalize(hash.data());
    
    return Hash256(hash);
}

Hash256 DoubleSHA256(const Byte* data, size_t len) {
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash1, hash2;
    
    // First SHA256
    hasher.Write(data, len);
    hasher.Finalize(hash1.data());
    
    // Second SHA256
    hasher.Reset();
    hasher.Write(hash1.data(), hash1.size());
    hasher.Finalize(hash2.data());
    
    return Hash256(hash2);
}

} // namespace shurium
