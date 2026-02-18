// SHURIUM - AES Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// AES-128/192/256 with CBC and CTR modes.
// Uses OpenSSL when available, pure C++ fallback otherwise.

#include "shurium/crypto/aes.h"
#include <cstring>
#include <random>
#include <stdexcept>

#ifdef SHURIUM_USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace shurium {

// ============================================================================
// AES S-Box and Tables (for fallback implementation)
// ============================================================================

namespace {

// AES S-Box
static const uint8_t SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

// Inverse S-Box
static const uint8_t INV_SBOX[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

// Round constants
static const uint8_t RCON[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

// GF(2^8) multiplication
inline uint8_t GfMul(uint8_t a, uint8_t b) {
    uint8_t result = 0;
    while (b) {
        if (b & 1) result ^= a;
        bool hiBit = (a & 0x80) != 0;
        a <<= 1;
        if (hiBit) a ^= 0x1b;  // x^8 + x^4 + x^3 + x + 1
        b >>= 1;
    }
    return result;
}

// Secure memory clear
inline void SecureClear(void* ptr, size_t len) {
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) *p++ = 0;
}

} // anonymous namespace

// ============================================================================
// AESContext Implementation
// ============================================================================

AESContext::AESContext(const Byte* key, size_t keyLen) {
    if (keyLen != 16 && keyLen != 24 && keyLen != 32) {
        throw std::invalid_argument("AES key must be 16, 24, or 32 bytes");
    }
    
    switch (keyLen) {
        case 16: keySize_ = AESKeySize::AES_128; rounds_ = 10; break;
        case 24: keySize_ = AESKeySize::AES_192; rounds_ = 12; break;
        case 32: keySize_ = AESKeySize::AES_256; rounds_ = 14; break;
        default: throw std::invalid_argument("Invalid key length");
    }
    
    ExpandKey(key, keyLen);
}

AESContext::~AESContext() {
    if (!encKey_.empty()) {
        SecureClear(encKey_.data(), encKey_.size() * sizeof(uint32_t));
    }
    if (!decKey_.empty()) {
        SecureClear(decKey_.data(), decKey_.size() * sizeof(uint32_t));
    }
}

AESContext::AESContext(AESContext&& other) noexcept
    : encKey_(std::move(other.encKey_))
    , decKey_(std::move(other.decKey_))
    , keySize_(other.keySize_)
    , rounds_(other.rounds_) {
}

AESContext& AESContext::operator=(AESContext&& other) noexcept {
    if (this != &other) {
        if (!encKey_.empty()) {
            SecureClear(encKey_.data(), encKey_.size() * sizeof(uint32_t));
        }
        if (!decKey_.empty()) {
            SecureClear(decKey_.data(), decKey_.size() * sizeof(uint32_t));
        }
        encKey_ = std::move(other.encKey_);
        decKey_ = std::move(other.decKey_);
        keySize_ = other.keySize_;
        rounds_ = other.rounds_;
    }
    return *this;
}

void AESContext::ExpandKey(const Byte* key, size_t keyLen) {
    int Nk = static_cast<int>(keyLen / 4);  // Key length in 32-bit words
    int Nr = rounds_;
    int Nb = 4;  // Block size in words (always 4 for AES)
    
    encKey_.resize(Nb * (Nr + 1));
    
    // Copy key to first Nk words
    for (int i = 0; i < Nk; ++i) {
        encKey_[i] = (static_cast<uint32_t>(key[4*i]) << 24) |
                     (static_cast<uint32_t>(key[4*i+1]) << 16) |
                     (static_cast<uint32_t>(key[4*i+2]) << 8) |
                     static_cast<uint32_t>(key[4*i+3]);
    }
    
    // Expand key
    for (int i = Nk; i < Nb * (Nr + 1); ++i) {
        uint32_t temp = encKey_[i - 1];
        if (i % Nk == 0) {
            // RotWord + SubWord + Rcon
            temp = ((static_cast<uint32_t>(SBOX[(temp >> 16) & 0xff]) << 24) |
                    (static_cast<uint32_t>(SBOX[(temp >> 8) & 0xff]) << 16) |
                    (static_cast<uint32_t>(SBOX[temp & 0xff]) << 8) |
                    static_cast<uint32_t>(SBOX[(temp >> 24) & 0xff])) ^
                   (static_cast<uint32_t>(RCON[i / Nk]) << 24);
        } else if (Nk > 6 && i % Nk == 4) {
            // SubWord only for AES-256
            temp = (static_cast<uint32_t>(SBOX[(temp >> 24) & 0xff]) << 24) |
                   (static_cast<uint32_t>(SBOX[(temp >> 16) & 0xff]) << 16) |
                   (static_cast<uint32_t>(SBOX[(temp >> 8) & 0xff]) << 8) |
                   static_cast<uint32_t>(SBOX[temp & 0xff]);
        }
        encKey_[i] = encKey_[i - Nk] ^ temp;
    }
    
    // Generate decryption key schedule (inverse)
    decKey_.resize(encKey_.size());
    for (int i = 0; i <= Nr; ++i) {
        for (int j = 0; j < Nb; ++j) {
            decKey_[i * Nb + j] = encKey_[(Nr - i) * Nb + j];
        }
    }
    
    // Apply InvMixColumns to all round keys except first and last
    for (int i = 1; i < Nr; ++i) {
        for (int j = 0; j < Nb; ++j) {
            uint32_t w = decKey_[i * Nb + j];
            uint8_t b0 = (w >> 24) & 0xff;
            uint8_t b1 = (w >> 16) & 0xff;
            uint8_t b2 = (w >> 8) & 0xff;
            uint8_t b3 = w & 0xff;
            
            decKey_[i * Nb + j] = 
                (static_cast<uint32_t>(GfMul(0x0e, b0) ^ GfMul(0x0b, b1) ^ GfMul(0x0d, b2) ^ GfMul(0x09, b3)) << 24) |
                (static_cast<uint32_t>(GfMul(0x09, b0) ^ GfMul(0x0e, b1) ^ GfMul(0x0b, b2) ^ GfMul(0x0d, b3)) << 16) |
                (static_cast<uint32_t>(GfMul(0x0d, b0) ^ GfMul(0x09, b1) ^ GfMul(0x0e, b2) ^ GfMul(0x0b, b3)) << 8) |
                static_cast<uint32_t>(GfMul(0x0b, b0) ^ GfMul(0x0d, b1) ^ GfMul(0x09, b2) ^ GfMul(0x0e, b3));
        }
    }
}

void AESContext::EncryptBlock(const Byte input[aes::BLOCK_SIZE], 
                               Byte output[aes::BLOCK_SIZE]) const {
    uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
    
    // Load input and add initial round key
    s0 = (static_cast<uint32_t>(input[0]) << 24) | (static_cast<uint32_t>(input[1]) << 16) |
         (static_cast<uint32_t>(input[2]) << 8) | static_cast<uint32_t>(input[3]);
    s1 = (static_cast<uint32_t>(input[4]) << 24) | (static_cast<uint32_t>(input[5]) << 16) |
         (static_cast<uint32_t>(input[6]) << 8) | static_cast<uint32_t>(input[7]);
    s2 = (static_cast<uint32_t>(input[8]) << 24) | (static_cast<uint32_t>(input[9]) << 16) |
         (static_cast<uint32_t>(input[10]) << 8) | static_cast<uint32_t>(input[11]);
    s3 = (static_cast<uint32_t>(input[12]) << 24) | (static_cast<uint32_t>(input[13]) << 16) |
         (static_cast<uint32_t>(input[14]) << 8) | static_cast<uint32_t>(input[15]);
    
    s0 ^= encKey_[0]; s1 ^= encKey_[1]; s2 ^= encKey_[2]; s3 ^= encKey_[3];
    
    // Main rounds
    for (int round = 1; round < rounds_; ++round) {
        // SubBytes + ShiftRows + MixColumns combined
        t0 = (static_cast<uint32_t>(GfMul(2, SBOX[(s0 >> 24) & 0xff]) ^ GfMul(3, SBOX[(s1 >> 16) & 0xff]) ^ SBOX[(s2 >> 8) & 0xff] ^ SBOX[s3 & 0xff]) << 24) |
             (static_cast<uint32_t>(SBOX[(s0 >> 24) & 0xff] ^ GfMul(2, SBOX[(s1 >> 16) & 0xff]) ^ GfMul(3, SBOX[(s2 >> 8) & 0xff]) ^ SBOX[s3 & 0xff]) << 16) |
             (static_cast<uint32_t>(SBOX[(s0 >> 24) & 0xff] ^ SBOX[(s1 >> 16) & 0xff] ^ GfMul(2, SBOX[(s2 >> 8) & 0xff]) ^ GfMul(3, SBOX[s3 & 0xff])) << 8) |
             static_cast<uint32_t>(GfMul(3, SBOX[(s0 >> 24) & 0xff]) ^ SBOX[(s1 >> 16) & 0xff] ^ SBOX[(s2 >> 8) & 0xff] ^ GfMul(2, SBOX[s3 & 0xff]));
        
        t1 = (static_cast<uint32_t>(GfMul(2, SBOX[(s1 >> 24) & 0xff]) ^ GfMul(3, SBOX[(s2 >> 16) & 0xff]) ^ SBOX[(s3 >> 8) & 0xff] ^ SBOX[s0 & 0xff]) << 24) |
             (static_cast<uint32_t>(SBOX[(s1 >> 24) & 0xff] ^ GfMul(2, SBOX[(s2 >> 16) & 0xff]) ^ GfMul(3, SBOX[(s3 >> 8) & 0xff]) ^ SBOX[s0 & 0xff]) << 16) |
             (static_cast<uint32_t>(SBOX[(s1 >> 24) & 0xff] ^ SBOX[(s2 >> 16) & 0xff] ^ GfMul(2, SBOX[(s3 >> 8) & 0xff]) ^ GfMul(3, SBOX[s0 & 0xff])) << 8) |
             static_cast<uint32_t>(GfMul(3, SBOX[(s1 >> 24) & 0xff]) ^ SBOX[(s2 >> 16) & 0xff] ^ SBOX[(s3 >> 8) & 0xff] ^ GfMul(2, SBOX[s0 & 0xff]));
        
        t2 = (static_cast<uint32_t>(GfMul(2, SBOX[(s2 >> 24) & 0xff]) ^ GfMul(3, SBOX[(s3 >> 16) & 0xff]) ^ SBOX[(s0 >> 8) & 0xff] ^ SBOX[s1 & 0xff]) << 24) |
             (static_cast<uint32_t>(SBOX[(s2 >> 24) & 0xff] ^ GfMul(2, SBOX[(s3 >> 16) & 0xff]) ^ GfMul(3, SBOX[(s0 >> 8) & 0xff]) ^ SBOX[s1 & 0xff]) << 16) |
             (static_cast<uint32_t>(SBOX[(s2 >> 24) & 0xff] ^ SBOX[(s3 >> 16) & 0xff] ^ GfMul(2, SBOX[(s0 >> 8) & 0xff]) ^ GfMul(3, SBOX[s1 & 0xff])) << 8) |
             static_cast<uint32_t>(GfMul(3, SBOX[(s2 >> 24) & 0xff]) ^ SBOX[(s3 >> 16) & 0xff] ^ SBOX[(s0 >> 8) & 0xff] ^ GfMul(2, SBOX[s1 & 0xff]));
        
        t3 = (static_cast<uint32_t>(GfMul(2, SBOX[(s3 >> 24) & 0xff]) ^ GfMul(3, SBOX[(s0 >> 16) & 0xff]) ^ SBOX[(s1 >> 8) & 0xff] ^ SBOX[s2 & 0xff]) << 24) |
             (static_cast<uint32_t>(SBOX[(s3 >> 24) & 0xff] ^ GfMul(2, SBOX[(s0 >> 16) & 0xff]) ^ GfMul(3, SBOX[(s1 >> 8) & 0xff]) ^ SBOX[s2 & 0xff]) << 16) |
             (static_cast<uint32_t>(SBOX[(s3 >> 24) & 0xff] ^ SBOX[(s0 >> 16) & 0xff] ^ GfMul(2, SBOX[(s1 >> 8) & 0xff]) ^ GfMul(3, SBOX[s2 & 0xff])) << 8) |
             static_cast<uint32_t>(GfMul(3, SBOX[(s3 >> 24) & 0xff]) ^ SBOX[(s0 >> 16) & 0xff] ^ SBOX[(s1 >> 8) & 0xff] ^ GfMul(2, SBOX[s2 & 0xff]));
        
        s0 = t0 ^ encKey_[round * 4];
        s1 = t1 ^ encKey_[round * 4 + 1];
        s2 = t2 ^ encKey_[round * 4 + 2];
        s3 = t3 ^ encKey_[round * 4 + 3];
    }
    
    // Final round (no MixColumns)
    t0 = (static_cast<uint32_t>(SBOX[(s0 >> 24) & 0xff]) << 24) |
         (static_cast<uint32_t>(SBOX[(s1 >> 16) & 0xff]) << 16) |
         (static_cast<uint32_t>(SBOX[(s2 >> 8) & 0xff]) << 8) |
         static_cast<uint32_t>(SBOX[s3 & 0xff]);
    t1 = (static_cast<uint32_t>(SBOX[(s1 >> 24) & 0xff]) << 24) |
         (static_cast<uint32_t>(SBOX[(s2 >> 16) & 0xff]) << 16) |
         (static_cast<uint32_t>(SBOX[(s3 >> 8) & 0xff]) << 8) |
         static_cast<uint32_t>(SBOX[s0 & 0xff]);
    t2 = (static_cast<uint32_t>(SBOX[(s2 >> 24) & 0xff]) << 24) |
         (static_cast<uint32_t>(SBOX[(s3 >> 16) & 0xff]) << 16) |
         (static_cast<uint32_t>(SBOX[(s0 >> 8) & 0xff]) << 8) |
         static_cast<uint32_t>(SBOX[s1 & 0xff]);
    t3 = (static_cast<uint32_t>(SBOX[(s3 >> 24) & 0xff]) << 24) |
         (static_cast<uint32_t>(SBOX[(s0 >> 16) & 0xff]) << 16) |
         (static_cast<uint32_t>(SBOX[(s1 >> 8) & 0xff]) << 8) |
         static_cast<uint32_t>(SBOX[s2 & 0xff]);
    
    s0 = t0 ^ encKey_[rounds_ * 4];
    s1 = t1 ^ encKey_[rounds_ * 4 + 1];
    s2 = t2 ^ encKey_[rounds_ * 4 + 2];
    s3 = t3 ^ encKey_[rounds_ * 4 + 3];
    
    // Store output
    output[0] = (s0 >> 24) & 0xff; output[1] = (s0 >> 16) & 0xff;
    output[2] = (s0 >> 8) & 0xff;  output[3] = s0 & 0xff;
    output[4] = (s1 >> 24) & 0xff; output[5] = (s1 >> 16) & 0xff;
    output[6] = (s1 >> 8) & 0xff;  output[7] = s1 & 0xff;
    output[8] = (s2 >> 24) & 0xff; output[9] = (s2 >> 16) & 0xff;
    output[10] = (s2 >> 8) & 0xff; output[11] = s2 & 0xff;
    output[12] = (s3 >> 24) & 0xff; output[13] = (s3 >> 16) & 0xff;
    output[14] = (s3 >> 8) & 0xff; output[15] = s3 & 0xff;
}

void AESContext::DecryptBlock(const Byte input[aes::BLOCK_SIZE],
                               Byte output[aes::BLOCK_SIZE]) const {
    uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
    
    // Load input and add initial round key
    s0 = (static_cast<uint32_t>(input[0]) << 24) | (static_cast<uint32_t>(input[1]) << 16) |
         (static_cast<uint32_t>(input[2]) << 8) | static_cast<uint32_t>(input[3]);
    s1 = (static_cast<uint32_t>(input[4]) << 24) | (static_cast<uint32_t>(input[5]) << 16) |
         (static_cast<uint32_t>(input[6]) << 8) | static_cast<uint32_t>(input[7]);
    s2 = (static_cast<uint32_t>(input[8]) << 24) | (static_cast<uint32_t>(input[9]) << 16) |
         (static_cast<uint32_t>(input[10]) << 8) | static_cast<uint32_t>(input[11]);
    s3 = (static_cast<uint32_t>(input[12]) << 24) | (static_cast<uint32_t>(input[13]) << 16) |
         (static_cast<uint32_t>(input[14]) << 8) | static_cast<uint32_t>(input[15]);
    
    s0 ^= decKey_[0]; s1 ^= decKey_[1]; s2 ^= decKey_[2]; s3 ^= decKey_[3];
    
    // Main rounds (inverse operations)
    for (int round = 1; round < rounds_; ++round) {
        // InvSubBytes + InvShiftRows + InvMixColumns
        t0 = (static_cast<uint32_t>(GfMul(0x0e, INV_SBOX[(s0 >> 24) & 0xff]) ^ GfMul(0x0b, INV_SBOX[(s3 >> 16) & 0xff]) ^ GfMul(0x0d, INV_SBOX[(s2 >> 8) & 0xff]) ^ GfMul(0x09, INV_SBOX[s1 & 0xff])) << 24) |
             (static_cast<uint32_t>(GfMul(0x09, INV_SBOX[(s0 >> 24) & 0xff]) ^ GfMul(0x0e, INV_SBOX[(s3 >> 16) & 0xff]) ^ GfMul(0x0b, INV_SBOX[(s2 >> 8) & 0xff]) ^ GfMul(0x0d, INV_SBOX[s1 & 0xff])) << 16) |
             (static_cast<uint32_t>(GfMul(0x0d, INV_SBOX[(s0 >> 24) & 0xff]) ^ GfMul(0x09, INV_SBOX[(s3 >> 16) & 0xff]) ^ GfMul(0x0e, INV_SBOX[(s2 >> 8) & 0xff]) ^ GfMul(0x0b, INV_SBOX[s1 & 0xff])) << 8) |
             static_cast<uint32_t>(GfMul(0x0b, INV_SBOX[(s0 >> 24) & 0xff]) ^ GfMul(0x0d, INV_SBOX[(s3 >> 16) & 0xff]) ^ GfMul(0x09, INV_SBOX[(s2 >> 8) & 0xff]) ^ GfMul(0x0e, INV_SBOX[s1 & 0xff]));
        
        t1 = (static_cast<uint32_t>(GfMul(0x0e, INV_SBOX[(s1 >> 24) & 0xff]) ^ GfMul(0x0b, INV_SBOX[(s0 >> 16) & 0xff]) ^ GfMul(0x0d, INV_SBOX[(s3 >> 8) & 0xff]) ^ GfMul(0x09, INV_SBOX[s2 & 0xff])) << 24) |
             (static_cast<uint32_t>(GfMul(0x09, INV_SBOX[(s1 >> 24) & 0xff]) ^ GfMul(0x0e, INV_SBOX[(s0 >> 16) & 0xff]) ^ GfMul(0x0b, INV_SBOX[(s3 >> 8) & 0xff]) ^ GfMul(0x0d, INV_SBOX[s2 & 0xff])) << 16) |
             (static_cast<uint32_t>(GfMul(0x0d, INV_SBOX[(s1 >> 24) & 0xff]) ^ GfMul(0x09, INV_SBOX[(s0 >> 16) & 0xff]) ^ GfMul(0x0e, INV_SBOX[(s3 >> 8) & 0xff]) ^ GfMul(0x0b, INV_SBOX[s2 & 0xff])) << 8) |
             static_cast<uint32_t>(GfMul(0x0b, INV_SBOX[(s1 >> 24) & 0xff]) ^ GfMul(0x0d, INV_SBOX[(s0 >> 16) & 0xff]) ^ GfMul(0x09, INV_SBOX[(s3 >> 8) & 0xff]) ^ GfMul(0x0e, INV_SBOX[s2 & 0xff]));
        
        t2 = (static_cast<uint32_t>(GfMul(0x0e, INV_SBOX[(s2 >> 24) & 0xff]) ^ GfMul(0x0b, INV_SBOX[(s1 >> 16) & 0xff]) ^ GfMul(0x0d, INV_SBOX[(s0 >> 8) & 0xff]) ^ GfMul(0x09, INV_SBOX[s3 & 0xff])) << 24) |
             (static_cast<uint32_t>(GfMul(0x09, INV_SBOX[(s2 >> 24) & 0xff]) ^ GfMul(0x0e, INV_SBOX[(s1 >> 16) & 0xff]) ^ GfMul(0x0b, INV_SBOX[(s0 >> 8) & 0xff]) ^ GfMul(0x0d, INV_SBOX[s3 & 0xff])) << 16) |
             (static_cast<uint32_t>(GfMul(0x0d, INV_SBOX[(s2 >> 24) & 0xff]) ^ GfMul(0x09, INV_SBOX[(s1 >> 16) & 0xff]) ^ GfMul(0x0e, INV_SBOX[(s0 >> 8) & 0xff]) ^ GfMul(0x0b, INV_SBOX[s3 & 0xff])) << 8) |
             static_cast<uint32_t>(GfMul(0x0b, INV_SBOX[(s2 >> 24) & 0xff]) ^ GfMul(0x0d, INV_SBOX[(s1 >> 16) & 0xff]) ^ GfMul(0x09, INV_SBOX[(s0 >> 8) & 0xff]) ^ GfMul(0x0e, INV_SBOX[s3 & 0xff]));
        
        t3 = (static_cast<uint32_t>(GfMul(0x0e, INV_SBOX[(s3 >> 24) & 0xff]) ^ GfMul(0x0b, INV_SBOX[(s2 >> 16) & 0xff]) ^ GfMul(0x0d, INV_SBOX[(s1 >> 8) & 0xff]) ^ GfMul(0x09, INV_SBOX[s0 & 0xff])) << 24) |
             (static_cast<uint32_t>(GfMul(0x09, INV_SBOX[(s3 >> 24) & 0xff]) ^ GfMul(0x0e, INV_SBOX[(s2 >> 16) & 0xff]) ^ GfMul(0x0b, INV_SBOX[(s1 >> 8) & 0xff]) ^ GfMul(0x0d, INV_SBOX[s0 & 0xff])) << 16) |
             (static_cast<uint32_t>(GfMul(0x0d, INV_SBOX[(s3 >> 24) & 0xff]) ^ GfMul(0x09, INV_SBOX[(s2 >> 16) & 0xff]) ^ GfMul(0x0e, INV_SBOX[(s1 >> 8) & 0xff]) ^ GfMul(0x0b, INV_SBOX[s0 & 0xff])) << 8) |
             static_cast<uint32_t>(GfMul(0x0b, INV_SBOX[(s3 >> 24) & 0xff]) ^ GfMul(0x0d, INV_SBOX[(s2 >> 16) & 0xff]) ^ GfMul(0x09, INV_SBOX[(s1 >> 8) & 0xff]) ^ GfMul(0x0e, INV_SBOX[s0 & 0xff]));
        
        s0 = t0 ^ decKey_[round * 4];
        s1 = t1 ^ decKey_[round * 4 + 1];
        s2 = t2 ^ decKey_[round * 4 + 2];
        s3 = t3 ^ decKey_[round * 4 + 3];
    }
    
    // Final round (no InvMixColumns)
    t0 = (static_cast<uint32_t>(INV_SBOX[(s0 >> 24) & 0xff]) << 24) |
         (static_cast<uint32_t>(INV_SBOX[(s3 >> 16) & 0xff]) << 16) |
         (static_cast<uint32_t>(INV_SBOX[(s2 >> 8) & 0xff]) << 8) |
         static_cast<uint32_t>(INV_SBOX[s1 & 0xff]);
    t1 = (static_cast<uint32_t>(INV_SBOX[(s1 >> 24) & 0xff]) << 24) |
         (static_cast<uint32_t>(INV_SBOX[(s0 >> 16) & 0xff]) << 16) |
         (static_cast<uint32_t>(INV_SBOX[(s3 >> 8) & 0xff]) << 8) |
         static_cast<uint32_t>(INV_SBOX[s2 & 0xff]);
    t2 = (static_cast<uint32_t>(INV_SBOX[(s2 >> 24) & 0xff]) << 24) |
         (static_cast<uint32_t>(INV_SBOX[(s1 >> 16) & 0xff]) << 16) |
         (static_cast<uint32_t>(INV_SBOX[(s0 >> 8) & 0xff]) << 8) |
         static_cast<uint32_t>(INV_SBOX[s3 & 0xff]);
    t3 = (static_cast<uint32_t>(INV_SBOX[(s3 >> 24) & 0xff]) << 24) |
         (static_cast<uint32_t>(INV_SBOX[(s2 >> 16) & 0xff]) << 16) |
         (static_cast<uint32_t>(INV_SBOX[(s1 >> 8) & 0xff]) << 8) |
         static_cast<uint32_t>(INV_SBOX[s0 & 0xff]);
    
    s0 = t0 ^ decKey_[rounds_ * 4];
    s1 = t1 ^ decKey_[rounds_ * 4 + 1];
    s2 = t2 ^ decKey_[rounds_ * 4 + 2];
    s3 = t3 ^ decKey_[rounds_ * 4 + 3];
    
    // Store output
    output[0] = (s0 >> 24) & 0xff; output[1] = (s0 >> 16) & 0xff;
    output[2] = (s0 >> 8) & 0xff;  output[3] = s0 & 0xff;
    output[4] = (s1 >> 24) & 0xff; output[5] = (s1 >> 16) & 0xff;
    output[6] = (s1 >> 8) & 0xff;  output[7] = s1 & 0xff;
    output[8] = (s2 >> 24) & 0xff; output[9] = (s2 >> 16) & 0xff;
    output[10] = (s2 >> 8) & 0xff; output[11] = s2 & 0xff;
    output[12] = (s3 >> 24) & 0xff; output[13] = (s3 >> 16) & 0xff;
    output[14] = (s3 >> 8) & 0xff; output[15] = s3 & 0xff;
}

// ============================================================================
// AESEncryptor Implementation
// ============================================================================

struct AESEncryptor::Impl {
    AESContext ctx;
    AESMode mode;
    Byte iv[aes::IV_SIZE];
    Byte buffer[aes::BLOCK_SIZE];
    size_t bufferLen{0};
    uint64_t counter{0};
    
#ifdef SHURIUM_USE_OPENSSL
    EVP_CIPHER_CTX* evpCtx{nullptr};
    bool useOpenSSL{true};
#endif
    
    Impl(const Byte* key, size_t keyLen, AESMode m, const Byte* initIv)
        : ctx(key, keyLen), mode(m), bufferLen(0), counter(0) {
        if (initIv) {
            std::memcpy(iv, initIv, aes::IV_SIZE);
        } else {
            // Generate random IV
            auto randomIv = GenerateRandomIV();
            std::memcpy(iv, randomIv.data(), aes::IV_SIZE);
        }
        
#ifdef SHURIUM_USE_OPENSSL
        evpCtx = EVP_CIPHER_CTX_new();
        if (evpCtx) {
            const EVP_CIPHER* cipher = nullptr;
            switch (keyLen) {
                case 16: cipher = (m == AESMode::CTR) ? EVP_aes_128_ctr() : EVP_aes_128_cbc(); break;
                case 24: cipher = (m == AESMode::CTR) ? EVP_aes_192_ctr() : EVP_aes_192_cbc(); break;
                case 32: cipher = (m == AESMode::CTR) ? EVP_aes_256_ctr() : EVP_aes_256_cbc(); break;
            }
            if (cipher && EVP_EncryptInit_ex(evpCtx, cipher, nullptr, key, iv) == 1) {
                if (m == AESMode::CBC) {
                    EVP_CIPHER_CTX_set_padding(evpCtx, 0);  // We handle padding ourselves
                }
            } else {
                EVP_CIPHER_CTX_free(evpCtx);
                evpCtx = nullptr;
                useOpenSSL = false;
            }
        } else {
            useOpenSSL = false;
        }
#endif
    }
    
    ~Impl() {
#ifdef SHURIUM_USE_OPENSSL
        if (evpCtx) {
            EVP_CIPHER_CTX_free(evpCtx);
        }
#endif
        SecureClear(buffer, sizeof(buffer));
        SecureClear(iv, sizeof(iv));
    }
};

AESEncryptor::AESEncryptor(const Byte* key, size_t keyLen,
                           AESMode mode, const Byte* iv)
    : impl_(std::make_unique<Impl>(key, keyLen, mode, iv)) {
}

AESEncryptor::~AESEncryptor() = default;

size_t AESEncryptor::Update(const Byte* input, size_t inputLen, Byte* output) {
    size_t outputLen = 0;
    
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->useOpenSSL && impl_->evpCtx) {
        int len = 0;
        if (EVP_EncryptUpdate(impl_->evpCtx, output, &len, input, static_cast<int>(inputLen)) == 1) {
            return static_cast<size_t>(len);
        }
        // Fall through to software implementation on failure
    }
#endif
    
    if (impl_->mode == AESMode::CTR) {
        // CTR mode - stream cipher
        Byte keystream[aes::BLOCK_SIZE];
        Byte counterBlock[aes::BLOCK_SIZE];
        
        for (size_t i = 0; i < inputLen; ++i) {
            if (impl_->bufferLen == 0) {
                // Generate new keystream block
                std::memcpy(counterBlock, impl_->iv, 8);
                for (int j = 0; j < 8; ++j) {
                    counterBlock[15 - j] = (impl_->counter >> (j * 8)) & 0xff;
                }
                impl_->ctx.EncryptBlock(counterBlock, keystream);
                impl_->counter++;
            }
            output[i] = input[i] ^ keystream[impl_->bufferLen];
            impl_->bufferLen = (impl_->bufferLen + 1) % aes::BLOCK_SIZE;
            outputLen++;
        }
    } else {
        // CBC mode
        size_t i = 0;
        
        // Process any buffered data first
        while (impl_->bufferLen > 0 && impl_->bufferLen < aes::BLOCK_SIZE && i < inputLen) {
            impl_->buffer[impl_->bufferLen++] = input[i++];
        }
        
        if (impl_->bufferLen == aes::BLOCK_SIZE) {
            // XOR with IV and encrypt
            for (size_t j = 0; j < aes::BLOCK_SIZE; ++j) {
                impl_->buffer[j] ^= impl_->iv[j];
            }
            impl_->ctx.EncryptBlock(impl_->buffer, output);
            std::memcpy(impl_->iv, output, aes::BLOCK_SIZE);
            output += aes::BLOCK_SIZE;
            outputLen += aes::BLOCK_SIZE;
            impl_->bufferLen = 0;
        }
        
        // Process full blocks
        while (i + aes::BLOCK_SIZE <= inputLen) {
            Byte block[aes::BLOCK_SIZE];
            for (size_t j = 0; j < aes::BLOCK_SIZE; ++j) {
                block[j] = input[i + j] ^ impl_->iv[j];
            }
            impl_->ctx.EncryptBlock(block, output);
            std::memcpy(impl_->iv, output, aes::BLOCK_SIZE);
            output += aes::BLOCK_SIZE;
            outputLen += aes::BLOCK_SIZE;
            i += aes::BLOCK_SIZE;
        }
        
        // Buffer remaining bytes
        while (i < inputLen) {
            impl_->buffer[impl_->bufferLen++] = input[i++];
        }
    }
    
    return outputLen;
}

size_t AESEncryptor::Finalize(Byte* output) {
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->useOpenSSL && impl_->evpCtx) {
        int len = 0;
        EVP_EncryptFinal_ex(impl_->evpCtx, output, &len);
        return static_cast<size_t>(len);
    }
#endif
    
    if (impl_->mode == AESMode::CTR) {
        return 0;  // CTR mode has no finalization
    }
    
    // CBC mode - add PKCS7 padding and encrypt final block
    size_t padLen = aes::BLOCK_SIZE - impl_->bufferLen;
    for (size_t i = impl_->bufferLen; i < aes::BLOCK_SIZE; ++i) {
        impl_->buffer[i] = static_cast<Byte>(padLen);
    }
    
    for (size_t j = 0; j < aes::BLOCK_SIZE; ++j) {
        impl_->buffer[j] ^= impl_->iv[j];
    }
    impl_->ctx.EncryptBlock(impl_->buffer, output);
    impl_->bufferLen = 0;
    
    return aes::BLOCK_SIZE;
}

std::vector<Byte> AESEncryptor::Encrypt(const std::vector<Byte>& plaintext) {
    size_t maxOutput = plaintext.size() + aes::BLOCK_SIZE;
    std::vector<Byte> result(maxOutput);
    
    size_t len = Update(plaintext.data(), plaintext.size(), result.data());
    len += Finalize(result.data() + len);
    
    result.resize(len);
    return result;
}

std::vector<Byte> AESEncryptor::Encrypt(const std::vector<Byte>& plaintext,
                                         std::array<Byte, aes::IV_SIZE>& ivOut) {
    std::memcpy(ivOut.data(), impl_->iv, aes::IV_SIZE);
    return Encrypt(plaintext);
}

// ============================================================================
// AESDecryptor Implementation
// ============================================================================

struct AESDecryptor::Impl {
    AESContext ctx;
    AESMode mode;
    Byte iv[aes::IV_SIZE];
    Byte buffer[aes::BLOCK_SIZE];
    Byte lastBlock[aes::BLOCK_SIZE];
    size_t bufferLen{0};
    uint64_t counter{0};
    bool hasLastBlock{false};
    
#ifdef SHURIUM_USE_OPENSSL
    EVP_CIPHER_CTX* evpCtx{nullptr};
    bool useOpenSSL{true};
#endif
    
    Impl(const Byte* key, size_t keyLen, AESMode m, const Byte* initIv)
        : ctx(key, keyLen), mode(m), bufferLen(0), counter(0), hasLastBlock(false) {
        std::memcpy(iv, initIv, aes::IV_SIZE);
        
#ifdef SHURIUM_USE_OPENSSL
        evpCtx = EVP_CIPHER_CTX_new();
        if (evpCtx) {
            const EVP_CIPHER* cipher = nullptr;
            switch (keyLen) {
                case 16: cipher = (m == AESMode::CTR) ? EVP_aes_128_ctr() : EVP_aes_128_cbc(); break;
                case 24: cipher = (m == AESMode::CTR) ? EVP_aes_192_ctr() : EVP_aes_192_cbc(); break;
                case 32: cipher = (m == AESMode::CTR) ? EVP_aes_256_ctr() : EVP_aes_256_cbc(); break;
            }
            if (cipher && EVP_DecryptInit_ex(evpCtx, cipher, nullptr, key, initIv) == 1) {
                if (m == AESMode::CBC) {
                    EVP_CIPHER_CTX_set_padding(evpCtx, 0);
                }
            } else {
                EVP_CIPHER_CTX_free(evpCtx);
                evpCtx = nullptr;
                useOpenSSL = false;
            }
        } else {
            useOpenSSL = false;
        }
#endif
    }
    
    ~Impl() {
#ifdef SHURIUM_USE_OPENSSL
        if (evpCtx) {
            EVP_CIPHER_CTX_free(evpCtx);
        }
#endif
        SecureClear(buffer, sizeof(buffer));
        SecureClear(lastBlock, sizeof(lastBlock));
        SecureClear(iv, sizeof(iv));
    }
};

AESDecryptor::AESDecryptor(const Byte* key, size_t keyLen,
                           AESMode mode, const Byte* iv)
    : impl_(std::make_unique<Impl>(key, keyLen, mode, iv)) {
}

AESDecryptor::~AESDecryptor() = default;

size_t AESDecryptor::Update(const Byte* input, size_t inputLen, Byte* output) {
    size_t outputLen = 0;
    
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->useOpenSSL && impl_->evpCtx) {
        int len = 0;
        if (EVP_DecryptUpdate(impl_->evpCtx, output, &len, input, static_cast<int>(inputLen)) == 1) {
            return static_cast<size_t>(len);
        }
    }
#endif
    
    if (impl_->mode == AESMode::CTR) {
        // CTR mode - same as encryption
        Byte keystream[aes::BLOCK_SIZE];
        Byte counterBlock[aes::BLOCK_SIZE];
        
        for (size_t i = 0; i < inputLen; ++i) {
            if (impl_->bufferLen == 0) {
                std::memcpy(counterBlock, impl_->iv, 8);
                for (int j = 0; j < 8; ++j) {
                    counterBlock[15 - j] = (impl_->counter >> (j * 8)) & 0xff;
                }
                impl_->ctx.EncryptBlock(counterBlock, keystream);
                impl_->counter++;
            }
            output[i] = input[i] ^ keystream[impl_->bufferLen];
            impl_->bufferLen = (impl_->bufferLen + 1) % aes::BLOCK_SIZE;
            outputLen++;
        }
    } else {
        // CBC mode
        size_t i = 0;
        
        // Output any previous last block (we delay by one block for padding)
        if (impl_->hasLastBlock) {
            std::memcpy(output, impl_->lastBlock, aes::BLOCK_SIZE);
            output += aes::BLOCK_SIZE;
            outputLen += aes::BLOCK_SIZE;
            impl_->hasLastBlock = false;
        }
        
        // Buffer incoming data
        while (impl_->bufferLen > 0 && impl_->bufferLen < aes::BLOCK_SIZE && i < inputLen) {
            impl_->buffer[impl_->bufferLen++] = input[i++];
        }
        
        if (impl_->bufferLen == aes::BLOCK_SIZE) {
            Byte decrypted[aes::BLOCK_SIZE];
            impl_->ctx.DecryptBlock(impl_->buffer, decrypted);
            for (size_t j = 0; j < aes::BLOCK_SIZE; ++j) {
                decrypted[j] ^= impl_->iv[j];
            }
            std::memcpy(impl_->iv, impl_->buffer, aes::BLOCK_SIZE);
            std::memcpy(impl_->lastBlock, decrypted, aes::BLOCK_SIZE);
            impl_->hasLastBlock = true;
            impl_->bufferLen = 0;
        }
        
        // Process full blocks
        while (i + aes::BLOCK_SIZE <= inputLen) {
            if (impl_->hasLastBlock) {
                std::memcpy(output, impl_->lastBlock, aes::BLOCK_SIZE);
                output += aes::BLOCK_SIZE;
                outputLen += aes::BLOCK_SIZE;
            }
            
            Byte decrypted[aes::BLOCK_SIZE];
            impl_->ctx.DecryptBlock(input + i, decrypted);
            for (size_t j = 0; j < aes::BLOCK_SIZE; ++j) {
                decrypted[j] ^= impl_->iv[j];
            }
            std::memcpy(impl_->iv, input + i, aes::BLOCK_SIZE);
            std::memcpy(impl_->lastBlock, decrypted, aes::BLOCK_SIZE);
            impl_->hasLastBlock = true;
            i += aes::BLOCK_SIZE;
        }
        
        // Buffer remaining
        while (i < inputLen) {
            impl_->buffer[impl_->bufferLen++] = input[i++];
        }
    }
    
    return outputLen;
}

size_t AESDecryptor::Finalize(Byte* output) {
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->useOpenSSL && impl_->evpCtx) {
        int len = 0;
        EVP_DecryptFinal_ex(impl_->evpCtx, output, &len);
        return static_cast<size_t>(len);
    }
#endif
    
    if (impl_->mode == AESMode::CTR) {
        return 0;
    }
    
    // CBC mode - remove PKCS7 padding from last block
    if (!impl_->hasLastBlock) {
        throw std::runtime_error("No data to finalize");
    }
    
    Byte padLen = impl_->lastBlock[aes::BLOCK_SIZE - 1];
    if (padLen == 0 || padLen > aes::BLOCK_SIZE) {
        throw std::runtime_error("Invalid PKCS7 padding");
    }
    
    for (size_t i = aes::BLOCK_SIZE - padLen; i < aes::BLOCK_SIZE; ++i) {
        if (impl_->lastBlock[i] != padLen) {
            throw std::runtime_error("Invalid PKCS7 padding");
        }
    }
    
    size_t dataLen = aes::BLOCK_SIZE - padLen;
    std::memcpy(output, impl_->lastBlock, dataLen);
    impl_->hasLastBlock = false;
    
    return dataLen;
}

std::vector<Byte> AESDecryptor::Decrypt(const std::vector<Byte>& ciphertext) {
    std::vector<Byte> result(ciphertext.size());
    
    size_t len = Update(ciphertext.data(), ciphertext.size(), result.data());
    len += Finalize(result.data() + len);
    
    result.resize(len);
    return result;
}

// ============================================================================
// Convenience Functions
// ============================================================================

std::vector<Byte> AES256Encrypt(const std::vector<Byte>& plaintext,
                                 const std::array<Byte, aes::KEY_SIZE_256>& key,
                                 const std::array<Byte, aes::IV_SIZE>& iv) {
    AESEncryptor enc(key.data(), key.size(), AESMode::CBC, iv.data());
    return enc.Encrypt(plaintext);
}

std::vector<Byte> AES256Decrypt(const std::vector<Byte>& ciphertext,
                                 const std::array<Byte, aes::KEY_SIZE_256>& key,
                                 const std::array<Byte, aes::IV_SIZE>& iv) {
    AESDecryptor dec(key.data(), key.size(), AESMode::CBC, iv.data());
    return dec.Decrypt(ciphertext);
}

std::vector<Byte> AES256CTREncrypt(const std::vector<Byte>& data,
                                    const std::array<Byte, aes::KEY_SIZE_256>& key,
                                    const std::array<Byte, aes::IV_SIZE>& nonce) {
    AESEncryptor enc(key.data(), key.size(), AESMode::CTR, nonce.data());
    std::vector<Byte> result(data.size());
    enc.Update(data.data(), data.size(), result.data());
    return result;
}

// ============================================================================
// PKCS7 Padding
// ============================================================================

std::vector<Byte> PKCS7Pad(const std::vector<Byte>& data, size_t blockSize) {
    size_t padLen = blockSize - (data.size() % blockSize);
    std::vector<Byte> result = data;
    result.resize(data.size() + padLen, static_cast<Byte>(padLen));
    return result;
}

std::vector<Byte> PKCS7Unpad(const std::vector<Byte>& data) {
    if (data.empty()) {
        throw std::runtime_error("Cannot unpad empty data");
    }
    
    Byte padLen = data.back();
    if (padLen == 0 || padLen > data.size()) {
        throw std::runtime_error("Invalid PKCS7 padding");
    }
    
    for (size_t i = data.size() - padLen; i < data.size(); ++i) {
        if (data[i] != padLen) {
            throw std::runtime_error("Invalid PKCS7 padding");
        }
    }
    
    return std::vector<Byte>(data.begin(), data.end() - padLen);
}

bool PKCS7ValidatePadding(const std::vector<Byte>& data) {
    if (data.empty()) return false;
    
    Byte padLen = data.back();
    if (padLen == 0 || padLen > data.size()) return false;
    
    for (size_t i = data.size() - padLen; i < data.size(); ++i) {
        if (data[i] != padLen) return false;
    }
    
    return true;
}

// ============================================================================
// Random IV Generation
// ============================================================================

std::array<Byte, aes::IV_SIZE> GenerateRandomIV() {
    std::array<Byte, aes::IV_SIZE> iv;
    
#ifdef SHURIUM_USE_OPENSSL
    if (RAND_bytes(iv.data(), aes::IV_SIZE) == 1) {
        return iv;
    }
#endif
    
    // Fallback to C++ random (not cryptographically secure, but better than nothing)
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    
    for (auto& b : iv) {
        b = static_cast<Byte>(dist(gen));
    }
    
    return iv;
}

} // namespace shurium
