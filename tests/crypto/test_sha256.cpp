// SHURIUM - SHA256 Tests (TDD)
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// These tests define the expected behavior of SHA256.
// Implementation should make all tests pass.

#include <gtest/gtest.h>
#include "shurium/crypto/sha256.h"
#include "shurium/core/types.h"
#include "shurium/core/hex.h"

#include <string>
#include <vector>
#include <array>

namespace shurium {
namespace test {

// ============================================================================
// Helper Functions
// ============================================================================

// Convert hex string to bytes
std::vector<Byte> HexToTestBytes(const std::string& hex) {
    std::vector<Byte> result;
    result.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        auto high = hex[i];
        auto low = hex[i + 1];
        auto hexCharToNibble = [](char c) -> Byte {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        result.push_back((hexCharToNibble(high) << 4) | hexCharToNibble(low));
    }
    return result;
}

// Convert bytes to hex string
std::string TestBytesToHex(const Byte* data, size_t len) {
    static const char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        result.push_back(hexChars[data[i] >> 4]);
        result.push_back(hexChars[data[i] & 0x0F]);
    }
    return result;
}

// ============================================================================
// SHA256 Basic Interface Tests
// ============================================================================

TEST(SHA256Test, OutputSizeIs32Bytes) {
    EXPECT_EQ(SHA256::OUTPUT_SIZE, 32);
}

TEST(SHA256Test, DefaultConstructor) {
    SHA256 hasher;
    // Should be able to create without error
    SUCCEED();
}

TEST(SHA256Test, WriteAndFinalize) {
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    // Write empty data
    hasher.Write(nullptr, 0);
    hasher.Finalize(hash.data());
    
    // Should produce a 32-byte output
    EXPECT_EQ(hash.size(), 32);
}

TEST(SHA256Test, Reset) {
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash1, hash2;
    
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    
    hasher.Write(data, 3);
    hasher.Finalize(hash1.data());
    
    hasher.Reset();
    hasher.Write(data, 3);
    hasher.Finalize(hash2.data());
    
    // Same input should produce same output after reset
    EXPECT_EQ(hash1, hash2);
}

TEST(SHA256Test, ChainedWrites) {
    SHA256 hasher;
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    
    // Write should return reference to allow chaining
    auto& ref = hasher.Write(data, 3);
    EXPECT_EQ(&ref, &hasher);
}

// ============================================================================
// SHA256 Test Vectors (NIST FIPS 180-4)
// ============================================================================

TEST(SHA256Test, EmptyString) {
    // SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    hasher.Finalize(hash.data());
    
    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, ABCString) {
    // SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    hasher.Write(data, 3);
    hasher.Finalize(hash.data());
    
    std::string expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, MessageDigest) {
    // SHA256("message digest") = f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    const char* msg = "message digest";
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, SecureHashAlgorithm) {
    // SHA256("secure hash algorithm") = f30ceb2bb2829e79e4ca9753d35a8ecc00262d164cc077080295381cbd643f0d
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    const char* msg = "secure hash algorithm";
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "f30ceb2bb2829e79e4ca9753d35a8ecc00262d164cc077080295381cbd643f0d";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, SHA256ConsideredSafe) {
    // SHA256("SHA256 is considered to be safe")
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    const char* msg = "SHA256 is considered to be safe";
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    // Verified with: echo -n "SHA256 is considered to be safe" | shasum -a 256
    std::string expected = "6819d915c73f4d1e77e4e1b52d1fa0f9cf9beaead3939f15874bd988e2a23630";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, TwoBlockInput) {
    // Test input that spans two blocks (>64 bytes)
    // SHA256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") 
    // = 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    const char* msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, Exactly63Bytes) {
    // SHA256("For this sample, this 63-byte string will be used as input data")
    // = f08a78cbbaee082b052ae0708f32fa1e50c5c421aa772ba5dbb406a2ea6be342
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    const char* msg = "For this sample, this 63-byte string will be used as input data";
    EXPECT_EQ(strlen(msg), 63);
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "f08a78cbbaee082b052ae0708f32fa1e50c5c421aa772ba5dbb406a2ea6be342";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, Exactly64Bytes) {
    // SHA256("This is exactly 64 bytes long, not counting the terminating byte")
    // = ab64eff7e88e2e46165e29f2bce41826bd4c7b3552f6b382a9e7d3af47c245f8
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    const char* msg = "This is exactly 64 bytes long, not counting the terminating byte";
    EXPECT_EQ(strlen(msg), 64);
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "ab64eff7e88e2e46165e29f2bce41826bd4c7b3552f6b382a9e7d3af47c245f8";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, BitcoinHeaderSize) {
    // SHA256 of 80 bytes (Bitcoin header size)
    // "As Bitcoin relies on 80 byte header hashes, we want to have an example for that."
    // = 7406e8de7d6e4fffc573daef05aefb8806e7790f55eab5576f31349743cca743
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    const char* msg = "As Bitcoin relies on 80 byte header hashes, we want to have an example for that.";
    EXPECT_EQ(strlen(msg), 80);
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "7406e8de7d6e4fffc573daef05aefb8806e7790f55eab5576f31349743cca743";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, LargeInput) {
    // SHA256(one million 'a' characters)
    // = cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    std::string msg(1000000, 'a');
    hasher.Write(reinterpret_cast<const Byte*>(msg.data()), msg.size());
    hasher.Finalize(hash.data());
    
    std::string expected = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

// ============================================================================
// Incremental Hashing Tests
// ============================================================================

TEST(SHA256Test, IncrementalHashing) {
    // Hash "abc" in one go and incrementally should produce same result
    std::array<Byte, SHA256::OUTPUT_SIZE> hash1, hash2;
    
    SHA256 hasher1;
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    hasher1.Write(data, 3);
    hasher1.Finalize(hash1.data());
    
    SHA256 hasher2;
    hasher2.Write(&data[0], 1);
    hasher2.Write(&data[1], 1);
    hasher2.Write(&data[2], 1);
    hasher2.Finalize(hash2.data());
    
    EXPECT_EQ(hash1, hash2);
}

TEST(SHA256Test, IncrementalLargeBlocks) {
    // Hash a message incrementally in various chunk sizes
    const char* msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    size_t len = strlen(msg);
    
    // Hash all at once
    std::array<Byte, SHA256::OUTPUT_SIZE> expected;
    {
        SHA256 hasher;
        hasher.Write(reinterpret_cast<const Byte*>(msg), len);
        hasher.Finalize(expected.data());
    }
    
    // Hash byte by byte
    {
        std::array<Byte, SHA256::OUTPUT_SIZE> result;
        SHA256 hasher;
        for (size_t i = 0; i < len; ++i) {
            hasher.Write(reinterpret_cast<const Byte*>(&msg[i]), 1);
        }
        hasher.Finalize(result.data());
        EXPECT_EQ(result, expected);
    }
    
    // Hash in chunks of various sizes
    for (size_t chunkSize = 1; chunkSize <= 16; ++chunkSize) {
        std::array<Byte, SHA256::OUTPUT_SIZE> result;
        SHA256 hasher;
        size_t pos = 0;
        while (pos < len) {
            size_t thisChunk = std::min(chunkSize, len - pos);
            hasher.Write(reinterpret_cast<const Byte*>(&msg[pos]), thisChunk);
            pos += thisChunk;
        }
        hasher.Finalize(result.data());
        EXPECT_EQ(result, expected) << "Failed for chunk size " << chunkSize;
    }
}

// ============================================================================
// Double SHA256 Tests (Used in Bitcoin)
// ============================================================================

TEST(SHA256Test, DoubleSHA256) {
    // Double SHA256 is commonly used in Bitcoin
    // SHA256(SHA256("abc"))
    std::array<Byte, SHA256::OUTPUT_SIZE> hash1, hash2;
    
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    
    SHA256 hasher1;
    hasher1.Write(data, 3);
    hasher1.Finalize(hash1.data());
    
    SHA256 hasher2;
    hasher2.Write(hash1.data(), hash1.size());
    hasher2.Finalize(hash2.data());
    
    // SHA256(SHA256("abc")) = 4f8b42c22dd3729b519ba6f68d2da7cc5b2d606d05daed5ad5128cc03e6c6358
    std::string expected = "4f8b42c22dd3729b519ba6f68d2da7cc5b2d606d05daed5ad5128cc03e6c6358";
    EXPECT_EQ(TestBytesToHex(hash2.data(), hash2.size()), expected);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST(SHA256Test, SingleCallFunction) {
    // Test the convenience function SHA256Hash
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    Hash256 result = SHA256Hash(data, 3);
    
    // Compare with expected
    std::string expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    EXPECT_EQ(TestBytesToHex(result.data(), result.size()), expected);
}

TEST(SHA256Test, DoubleSHA256Function) {
    // Test the convenience function DoubleSHA256
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    Hash256 result = DoubleSHA256(data, 3);
    
    // SHA256(SHA256("abc"))
    std::string expected = "4f8b42c22dd3729b519ba6f68d2da7cc5b2d606d05daed5ad5128cc03e6c6358";
    EXPECT_EQ(TestBytesToHex(result.data(), result.size()), expected);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(SHA256Test, SingleByte) {
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    Byte data = 0x00;
    hasher.Write(&data, 1);
    hasher.Finalize(hash.data());
    
    // SHA256(0x00) = 6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d
    std::string expected = "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, AllZeroes64Bytes) {
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    std::array<Byte, 64> data;
    data.fill(0x00);
    hasher.Write(data.data(), data.size());
    hasher.Finalize(hash.data());
    
    // SHA256(64 zero bytes) = f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b
    std::string expected = "f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b";
    EXPECT_EQ(TestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(SHA256Test, AllOnes32Bytes) {
    SHA256 hasher;
    std::array<Byte, SHA256::OUTPUT_SIZE> hash;
    
    std::array<Byte, 32> data;
    data.fill(0xFF);
    hasher.Write(data.data(), data.size());
    hasher.Finalize(hash.data());
    
    // Should produce a valid hash (verify manually)
    EXPECT_FALSE(hash[0] == 0 && hash[1] == 0 && hash[2] == 0);
}

} // namespace test
} // namespace shurium
