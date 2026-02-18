// SHURIUM - HMAC Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// HMAC-SHA256 and HMAC-SHA512 implementation.
// Uses OpenSSL when available, pure C++ fallback otherwise.

#include "shurium/crypto/hmac.h"
#include "shurium/crypto/sha256.h"
#include <cstring>
#include <stdexcept>

#ifdef SHURIUM_USE_OPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#endif

namespace shurium {

namespace {

// Secure memory clear
inline void SecureClear(void* ptr, size_t len) {
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) *p++ = 0;
}

// ============================================================================
// SHA512 Implementation (needed for HMAC-SHA512 fallback)
// ============================================================================

class SHA512Internal {
public:
    static constexpr size_t OUTPUT_SIZE = 64;
    static constexpr size_t BLOCK_SIZE = 128;
    
    SHA512Internal() { Reset(); }
    
    SHA512Internal& Write(const Byte* data, size_t len) {
        while (len > 0) {
            size_t space = BLOCK_SIZE - bufferLen_;
            size_t toCopy = std::min(len, space);
            std::memcpy(buffer_ + bufferLen_, data, toCopy);
            bufferLen_ += toCopy;
            data += toCopy;
            len -= toCopy;
            bytes_ += toCopy;
            
            if (bufferLen_ == BLOCK_SIZE) {
                Transform(buffer_);
                bufferLen_ = 0;
            }
        }
        return *this;
    }
    
    void Finalize(Byte hash[OUTPUT_SIZE]) {
        // Padding
        buffer_[bufferLen_++] = 0x80;
        if (bufferLen_ > 112) {
            while (bufferLen_ < BLOCK_SIZE) buffer_[bufferLen_++] = 0;
            Transform(buffer_);
            bufferLen_ = 0;
        }
        while (bufferLen_ < 112) buffer_[bufferLen_++] = 0;
        
        // Length in bits (big-endian, 128-bit)
        uint64_t bits = bytes_ * 8;
        for (int i = 0; i < 8; ++i) buffer_[112 + i] = 0;
        for (int i = 0; i < 8; ++i) buffer_[120 + i] = (bits >> (56 - i * 8)) & 0xff;
        Transform(buffer_);
        
        for (int i = 0; i < 8; ++i) {
            hash[i * 8 + 0] = (state_[i] >> 56) & 0xff;
            hash[i * 8 + 1] = (state_[i] >> 48) & 0xff;
            hash[i * 8 + 2] = (state_[i] >> 40) & 0xff;
            hash[i * 8 + 3] = (state_[i] >> 32) & 0xff;
            hash[i * 8 + 4] = (state_[i] >> 24) & 0xff;
            hash[i * 8 + 5] = (state_[i] >> 16) & 0xff;
            hash[i * 8 + 6] = (state_[i] >> 8) & 0xff;
            hash[i * 8 + 7] = state_[i] & 0xff;
        }
    }
    
    SHA512Internal& Reset() {
        state_[0] = 0x6a09e667f3bcc908ULL;
        state_[1] = 0xbb67ae8584caa73bULL;
        state_[2] = 0x3c6ef372fe94f82bULL;
        state_[3] = 0xa54ff53a5f1d36f1ULL;
        state_[4] = 0x510e527fade682d1ULL;
        state_[5] = 0x9b05688c2b3e6c1fULL;
        state_[6] = 0x1f83d9abfb41bd6bULL;
        state_[7] = 0x5be0cd19137e2179ULL;
        bytes_ = 0;
        bufferLen_ = 0;
        return *this;
    }

private:
    uint64_t state_[8];
    Byte buffer_[BLOCK_SIZE];
    uint64_t bytes_;
    size_t bufferLen_;
    
    static constexpr uint64_t K[80] = {
        0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
        0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
        0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
        0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
        0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
        0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
        0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
        0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
        0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
        0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
        0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
        0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
        0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
        0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
        0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
        0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
        0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
    };
    
    static inline uint64_t ROTR(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }
    static inline uint64_t Ch(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (~x & z); }
    static inline uint64_t Maj(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static inline uint64_t Sigma0(uint64_t x) { return ROTR(x, 28) ^ ROTR(x, 34) ^ ROTR(x, 39); }
    static inline uint64_t Sigma1(uint64_t x) { return ROTR(x, 14) ^ ROTR(x, 18) ^ ROTR(x, 41); }
    static inline uint64_t sigma0(uint64_t x) { return ROTR(x, 1) ^ ROTR(x, 8) ^ (x >> 7); }
    static inline uint64_t sigma1(uint64_t x) { return ROTR(x, 19) ^ ROTR(x, 61) ^ (x >> 6); }
    
    void Transform(const Byte block[BLOCK_SIZE]) {
        uint64_t W[80];
        for (int i = 0; i < 16; ++i) {
            W[i] = (static_cast<uint64_t>(block[i*8]) << 56) |
                   (static_cast<uint64_t>(block[i*8+1]) << 48) |
                   (static_cast<uint64_t>(block[i*8+2]) << 40) |
                   (static_cast<uint64_t>(block[i*8+3]) << 32) |
                   (static_cast<uint64_t>(block[i*8+4]) << 24) |
                   (static_cast<uint64_t>(block[i*8+5]) << 16) |
                   (static_cast<uint64_t>(block[i*8+6]) << 8) |
                   static_cast<uint64_t>(block[i*8+7]);
        }
        for (int i = 16; i < 80; ++i) {
            W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];
        }
        
        uint64_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint64_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        
        for (int i = 0; i < 80; ++i) {
            uint64_t T1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
            uint64_t T2 = Sigma0(a) + Maj(a, b, c);
            h = g; g = f; f = e; e = d + T1;
            d = c; c = b; b = a; a = T1 + T2;
        }
        
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }
};

constexpr uint64_t SHA512Internal::K[80];

} // anonymous namespace

// ============================================================================
// HMAC_SHA256 Implementation
// ============================================================================

struct HMAC_SHA256::Impl {
    Byte iKeyPad[hmac::SHA256_BLOCK_SIZE];
    Byte oKeyPad[hmac::SHA256_BLOCK_SIZE];
    SHA256 innerHash;
    bool finalized{false};
    
#ifdef SHURIUM_USE_OPENSSL
    HMAC_CTX* ctx{nullptr};
    bool useOpenSSL{true};
#endif
    
    Impl(const Byte* key, size_t keyLen) {
#ifdef SHURIUM_USE_OPENSSL
        ctx = HMAC_CTX_new();
        if (ctx && HMAC_Init_ex(ctx, key, static_cast<int>(keyLen), EVP_sha256(), nullptr) == 1) {
            useOpenSSL = true;
        } else {
            if (ctx) HMAC_CTX_free(ctx);
            ctx = nullptr;
            useOpenSSL = false;
        }
        if (useOpenSSL) return;
#endif
        
        // Prepare key (hash if > block size, pad with zeros if < block size)
        Byte keyBlock[hmac::SHA256_BLOCK_SIZE] = {0};
        if (keyLen > hmac::SHA256_BLOCK_SIZE) {
            Hash256 keyHash = SHA256Hash(key, keyLen);
            std::memcpy(keyBlock, keyHash.data(), SHA256::OUTPUT_SIZE);
        } else {
            std::memcpy(keyBlock, key, keyLen);
        }
        
        // Create inner and outer key pads
        for (size_t i = 0; i < hmac::SHA256_BLOCK_SIZE; ++i) {
            iKeyPad[i] = keyBlock[i] ^ 0x36;
            oKeyPad[i] = keyBlock[i] ^ 0x5c;
        }
        
        SecureClear(keyBlock, sizeof(keyBlock));
        
        // Initialize inner hash with inner key pad
        innerHash.Reset();
        innerHash.Write(iKeyPad, hmac::SHA256_BLOCK_SIZE);
    }
    
    ~Impl() {
#ifdef SHURIUM_USE_OPENSSL
        if (ctx) HMAC_CTX_free(ctx);
#endif
        SecureClear(iKeyPad, sizeof(iKeyPad));
        SecureClear(oKeyPad, sizeof(oKeyPad));
    }
    
    void Reset() {
#ifdef SHURIUM_USE_OPENSSL
        if (useOpenSSL && ctx) {
            HMAC_Init_ex(ctx, nullptr, 0, nullptr, nullptr);
            finalized = false;
            return;
        }
#endif
        innerHash.Reset();
        innerHash.Write(iKeyPad, hmac::SHA256_BLOCK_SIZE);
        finalized = false;
    }
};

HMAC_SHA256::HMAC_SHA256(const Byte* key, size_t keyLen)
    : impl_(std::make_unique<Impl>(key, keyLen)) {
}

HMAC_SHA256::~HMAC_SHA256() = default;

HMAC_SHA256::HMAC_SHA256(HMAC_SHA256&& other) noexcept = default;
HMAC_SHA256& HMAC_SHA256::operator=(HMAC_SHA256&& other) noexcept = default;

HMAC_SHA256& HMAC_SHA256::Write(const Byte* data, size_t len) {
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->useOpenSSL && impl_->ctx) {
        HMAC_Update(impl_->ctx, data, len);
        return *this;
    }
#endif
    impl_->innerHash.Write(data, len);
    return *this;
}

void HMAC_SHA256::Finalize(Byte mac[OUTPUT_SIZE]) {
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->useOpenSSL && impl_->ctx) {
        unsigned int len = OUTPUT_SIZE;
        HMAC_Final(impl_->ctx, mac, &len);
        impl_->finalized = true;
        return;
    }
#endif
    
    // Get inner hash
    Byte innerResult[SHA256::OUTPUT_SIZE];
    impl_->innerHash.Finalize(innerResult);
    
    // Compute outer hash: H(oKeyPad || innerResult)
    SHA256 outerHash;
    outerHash.Write(impl_->oKeyPad, hmac::SHA256_BLOCK_SIZE);
    outerHash.Write(innerResult, SHA256::OUTPUT_SIZE);
    outerHash.Finalize(mac);
    
    SecureClear(innerResult, sizeof(innerResult));
    impl_->finalized = true;
}

Hash256 HMAC_SHA256::Finalize() {
    Hash256 result;
    Finalize(result.data());
    return result;
}

HMAC_SHA256& HMAC_SHA256::Reset() {
    impl_->Reset();
    return *this;
}

// ============================================================================
// HMAC_SHA512 Implementation
// ============================================================================

struct HMAC_SHA512::Impl {
    Byte iKeyPad[hmac::SHA512_BLOCK_SIZE];
    Byte oKeyPad[hmac::SHA512_BLOCK_SIZE];
    SHA512Internal innerHash;
    bool finalized{false};
    
#ifdef SHURIUM_USE_OPENSSL
    HMAC_CTX* ctx{nullptr};
    bool useOpenSSL{true};
#endif
    
    Impl(const Byte* key, size_t keyLen) {
#ifdef SHURIUM_USE_OPENSSL
        ctx = HMAC_CTX_new();
        if (ctx && HMAC_Init_ex(ctx, key, static_cast<int>(keyLen), EVP_sha512(), nullptr) == 1) {
            useOpenSSL = true;
        } else {
            if (ctx) HMAC_CTX_free(ctx);
            ctx = nullptr;
            useOpenSSL = false;
        }
        if (useOpenSSL) return;
#endif
        
        Byte keyBlock[hmac::SHA512_BLOCK_SIZE] = {0};
        if (keyLen > hmac::SHA512_BLOCK_SIZE) {
            SHA512Internal keyHasher;
            keyHasher.Write(key, keyLen);
            keyHasher.Finalize(keyBlock);
        } else {
            std::memcpy(keyBlock, key, keyLen);
        }
        
        for (size_t i = 0; i < hmac::SHA512_BLOCK_SIZE; ++i) {
            iKeyPad[i] = keyBlock[i] ^ 0x36;
            oKeyPad[i] = keyBlock[i] ^ 0x5c;
        }
        
        SecureClear(keyBlock, sizeof(keyBlock));
        innerHash.Reset();
        innerHash.Write(iKeyPad, hmac::SHA512_BLOCK_SIZE);
    }
    
    ~Impl() {
#ifdef SHURIUM_USE_OPENSSL
        if (ctx) HMAC_CTX_free(ctx);
#endif
        SecureClear(iKeyPad, sizeof(iKeyPad));
        SecureClear(oKeyPad, sizeof(oKeyPad));
    }
    
    void Reset() {
#ifdef SHURIUM_USE_OPENSSL
        if (useOpenSSL && ctx) {
            HMAC_Init_ex(ctx, nullptr, 0, nullptr, nullptr);
            finalized = false;
            return;
        }
#endif
        innerHash.Reset();
        innerHash.Write(iKeyPad, hmac::SHA512_BLOCK_SIZE);
        finalized = false;
    }
};

HMAC_SHA512::HMAC_SHA512(const Byte* key, size_t keyLen)
    : impl_(std::make_unique<Impl>(key, keyLen)) {
}

HMAC_SHA512::~HMAC_SHA512() = default;

HMAC_SHA512::HMAC_SHA512(HMAC_SHA512&& other) noexcept = default;
HMAC_SHA512& HMAC_SHA512::operator=(HMAC_SHA512&& other) noexcept = default;

HMAC_SHA512& HMAC_SHA512::Write(const Byte* data, size_t len) {
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->useOpenSSL && impl_->ctx) {
        HMAC_Update(impl_->ctx, data, len);
        return *this;
    }
#endif
    impl_->innerHash.Write(data, len);
    return *this;
}

void HMAC_SHA512::Finalize(Byte mac[OUTPUT_SIZE]) {
#ifdef SHURIUM_USE_OPENSSL
    if (impl_->useOpenSSL && impl_->ctx) {
        unsigned int len = OUTPUT_SIZE;
        HMAC_Final(impl_->ctx, mac, &len);
        impl_->finalized = true;
        return;
    }
#endif
    
    Byte innerResult[SHA512Internal::OUTPUT_SIZE];
    impl_->innerHash.Finalize(innerResult);
    
    SHA512Internal outerHash;
    outerHash.Write(impl_->oKeyPad, hmac::SHA512_BLOCK_SIZE);
    outerHash.Write(innerResult, SHA512Internal::OUTPUT_SIZE);
    outerHash.Finalize(mac);
    
    SecureClear(innerResult, sizeof(innerResult));
    impl_->finalized = true;
}

Hash512 HMAC_SHA512::Finalize() {
    Hash512 result;
    Finalize(result.data());
    return result;
}

HMAC_SHA512& HMAC_SHA512::Reset() {
    impl_->Reset();
    return *this;
}

// ============================================================================
// Convenience Functions
// ============================================================================

Hash256 ComputeHMAC_SHA256(const Byte* key, size_t keyLen,
                           const Byte* data, size_t dataLen) {
    HMAC_SHA256 hmac(key, keyLen);
    hmac.Write(data, dataLen);
    return hmac.Finalize();
}

Hash512 ComputeHMAC_SHA512(const Byte* key, size_t keyLen,
                           const Byte* data, size_t dataLen) {
    HMAC_SHA512 hmac(key, keyLen);
    hmac.Write(data, dataLen);
    return hmac.Finalize();
}

// ============================================================================
// HKDF Implementation
// ============================================================================

Hash256 HKDFExtract(const std::vector<Byte>& salt,
                    const std::vector<Byte>& ikm) {
    std::vector<Byte> actualSalt = salt;
    if (actualSalt.empty()) {
        actualSalt.resize(hmac::SHA256_SIZE, 0);
    }
    return ComputeHMAC_SHA256(actualSalt, ikm);
}

std::vector<Byte> HKDFExpand(const Hash256& prk,
                              const std::vector<Byte>& info,
                              size_t length) {
    if (length > 255 * hmac::SHA256_SIZE) {
        throw std::invalid_argument("HKDF output too long");
    }
    
    std::vector<Byte> okm;
    okm.reserve(length);
    
    Byte T[hmac::SHA256_SIZE] = {0};
    size_t tLen = 0;
    uint8_t counter = 1;
    
    while (okm.size() < length) {
        HMAC_SHA256 hmac(prk.data(), prk.size());
        if (tLen > 0) {
            hmac.Write(T, tLen);
        }
        hmac.Write(info.data(), info.size());
        hmac.Write(&counter, 1);
        hmac.Finalize(T);
        tLen = hmac::SHA256_SIZE;
        
        size_t toCopy = std::min(static_cast<size_t>(hmac::SHA256_SIZE), length - okm.size());
        okm.insert(okm.end(), T, T + toCopy);
        counter++;
    }
    
    SecureClear(T, sizeof(T));
    return okm;
}

std::vector<Byte> HKDF(const std::vector<Byte>& salt,
                        const std::vector<Byte>& ikm,
                        const std::vector<Byte>& info,
                        size_t length) {
    Hash256 prk = HKDFExtract(salt, ikm);
    return HKDFExpand(prk, info, length);
}

std::vector<Byte> HKDF_SHA512(const std::vector<Byte>& salt,
                               const std::vector<Byte>& ikm,
                               const std::vector<Byte>& info,
                               size_t length) {
    if (length > 255 * hmac::SHA512_SIZE) {
        throw std::invalid_argument("HKDF output too long");
    }
    
    // Extract
    std::vector<Byte> actualSalt = salt;
    if (actualSalt.empty()) {
        actualSalt.resize(hmac::SHA512_SIZE, 0);
    }
    Hash512 prk = ComputeHMAC_SHA512(actualSalt, ikm);
    
    // Expand
    std::vector<Byte> okm;
    okm.reserve(length);
    
    Byte T[hmac::SHA512_SIZE] = {0};
    size_t tLen = 0;
    uint8_t counter = 1;
    
    while (okm.size() < length) {
        HMAC_SHA512 hmac(prk.data(), prk.size());
        if (tLen > 0) {
            hmac.Write(T, tLen);
        }
        hmac.Write(info.data(), info.size());
        hmac.Write(&counter, 1);
        hmac.Finalize(T);
        tLen = hmac::SHA512_SIZE;
        
        size_t toCopy = std::min(static_cast<size_t>(hmac::SHA512_SIZE), length - okm.size());
        okm.insert(okm.end(), T, T + toCopy);
        counter++;
    }
    
    SecureClear(T, sizeof(T));
    return okm;
}

// ============================================================================
// PBKDF2 Implementation
// ============================================================================

std::vector<Byte> PBKDF2_SHA256(const std::string& password,
                                 const std::vector<Byte>& salt,
                                 uint32_t iterations,
                                 size_t keyLen) {
    if (iterations == 0) {
        throw std::invalid_argument("PBKDF2 iterations must be > 0");
    }
    
    std::vector<Byte> dk;
    dk.reserve(keyLen);
    
    std::vector<Byte> passwordBytes(password.begin(), password.end());
    uint32_t blockNum = 1;
    
    while (dk.size() < keyLen) {
        // U1 = HMAC(password, salt || INT(blockNum))
        std::vector<Byte> saltBlock = salt;
        saltBlock.push_back((blockNum >> 24) & 0xff);
        saltBlock.push_back((blockNum >> 16) & 0xff);
        saltBlock.push_back((blockNum >> 8) & 0xff);
        saltBlock.push_back(blockNum & 0xff);
        
        Hash256 U = ComputeHMAC_SHA256(passwordBytes, saltBlock);
        Hash256 T = U;
        
        // T = U1 ^ U2 ^ ... ^ Uc
        for (uint32_t i = 1; i < iterations; ++i) {
            U = ComputeHMAC_SHA256(passwordBytes.data(), passwordBytes.size(),
                                   U.data(), U.size());
            for (size_t j = 0; j < hmac::SHA256_SIZE; ++j) {
                T[j] ^= U[j];
            }
        }
        
        size_t toCopy = std::min(static_cast<size_t>(hmac::SHA256_SIZE), keyLen - dk.size());
        dk.insert(dk.end(), T.begin(), T.begin() + toCopy);
        blockNum++;
    }
    
    return dk;
}

std::vector<Byte> PBKDF2_SHA512(const std::string& password,
                                 const std::vector<Byte>& salt,
                                 uint32_t iterations,
                                 size_t keyLen) {
    if (iterations == 0) {
        throw std::invalid_argument("PBKDF2 iterations must be > 0");
    }
    
    std::vector<Byte> dk;
    dk.reserve(keyLen);
    
    std::vector<Byte> passwordBytes(password.begin(), password.end());
    uint32_t blockNum = 1;
    
    while (dk.size() < keyLen) {
        std::vector<Byte> saltBlock = salt;
        saltBlock.push_back((blockNum >> 24) & 0xff);
        saltBlock.push_back((blockNum >> 16) & 0xff);
        saltBlock.push_back((blockNum >> 8) & 0xff);
        saltBlock.push_back(blockNum & 0xff);
        
        Hash512 U = ComputeHMAC_SHA512(passwordBytes, saltBlock);
        Hash512 T = U;
        
        for (uint32_t i = 1; i < iterations; ++i) {
            U = ComputeHMAC_SHA512(passwordBytes.data(), passwordBytes.size(),
                                   U.data(), U.size());
            for (size_t j = 0; j < hmac::SHA512_SIZE; ++j) {
                T[j] ^= U[j];
            }
        }
        
        size_t toCopy = std::min(static_cast<size_t>(hmac::SHA512_SIZE), keyLen - dk.size());
        dk.insert(dk.end(), T.begin(), T.begin() + toCopy);
        blockNum++;
    }
    
    return dk;
}

// ============================================================================
// Constant-Time Comparison
// ============================================================================

bool ConstantTimeCompare(const Byte* a, const Byte* b, size_t len) {
    volatile Byte result = 0;
    for (size_t i = 0; i < len; ++i) {
        result |= a[i] ^ b[i];
    }
    return result == 0;
}

} // namespace shurium
