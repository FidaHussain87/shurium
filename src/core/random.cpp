// SHURIUM - Secure Random Number Generation Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/core/random.h"
#include <cstring>
#include <stdexcept>

// Platform-specific includes
#if defined(__linux__)
    #include <sys/random.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    #include <stdlib.h>  // arc4random_buf
#elif defined(_WIN32)
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#else
    #include <fstream>
#endif

namespace shurium {

namespace detail {

bool GetOSEntropy(uint8_t* buf, size_t len) {
    if (len == 0) return true;

#if defined(__linux__)
    // Linux: use getrandom() syscall
    ssize_t ret = getrandom(buf, len, 0);
    return ret == static_cast<ssize_t>(len);

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    // macOS/BSD: use arc4random_buf (always succeeds)
    arc4random_buf(buf, len);
    return true;

#elif defined(_WIN32)
    // Windows: use BCryptGenRandom
    NTSTATUS status = BCryptGenRandom(nullptr, buf, static_cast<ULONG>(len),
                                       BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(status);

#else
    // Fallback: read from /dev/urandom
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom) return false;
    urandom.read(reinterpret_cast<char*>(buf), len);
    return urandom.good();
#endif
}

void InitializeRNG() {
    // For now, we rely solely on OS entropy which is self-seeding
    // In a production system, we might add additional entropy sources:
    // - Hardware RNG (RDRAND/RDSEED)
    // - High-precision timestamps
    // - Stack pointer
    // - etc.
}

} // namespace detail

// ============================================================================
// Core Random Functions
// ============================================================================

void GetRandBytes(uint8_t* buf, size_t len) {
    if (!detail::GetOSEntropy(buf, len)) {
        // In a production system, we might fall back to a software PRNG
        // that was seeded earlier. For now, we throw an exception.
        throw std::runtime_error("Failed to get random bytes from OS");
    }
}

uint64_t GetRandUint64() {
    uint64_t result;
    GetRandBytes(reinterpret_cast<uint8_t*>(&result), sizeof(result));
    return result;
}

uint32_t GetRandUint32() {
    uint32_t result;
    GetRandBytes(reinterpret_cast<uint8_t*>(&result), sizeof(result));
    return result;
}

uint64_t GetRandInt(uint64_t max) {
    if (max == 0) return 0;
    if (max == 1) return 0;
    
    // Use rejection sampling to avoid modulo bias
    // Find the largest multiple of max that fits in 64 bits
    uint64_t threshold = (static_cast<uint64_t>(-1) / max) * max;
    
    uint64_t result;
    do {
        result = GetRandUint64();
    } while (result >= threshold);
    
    return result % max;
}

// ============================================================================
// Random Hash Generation
// ============================================================================

Hash256 GetRandHash256() {
    Hash256 hash;
    GetRandBytes(hash.data(), 32);
    return hash;
}

Hash160 GetRandHash160() {
    Hash160 hash;
    GetRandBytes(hash.data(), 20);
    return hash;
}

} // namespace shurium
