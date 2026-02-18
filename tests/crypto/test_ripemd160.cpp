// SHURIUM - RIPEMD160 Tests (TDD)
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// These tests define the expected behavior of RIPEMD160.
// Implementation should make all tests pass.

#include <gtest/gtest.h>
#include "shurium/crypto/ripemd160.h"
#include "shurium/crypto/sha256.h"
#include "shurium/core/types.h"
#include "shurium/core/hex.h"

#include <string>
#include <vector>
#include <array>
#include <cstring>

namespace shurium {
namespace test {

// ============================================================================
// Helper Functions
// ============================================================================

// Convert bytes to hex string
std::string RIPEMDTestBytesToHex(const Byte* data, size_t len) {
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
// RIPEMD160 Basic Interface Tests
// ============================================================================

TEST(RIPEMD160Test, OutputSizeIs20Bytes) {
    EXPECT_EQ(RIPEMD160::OUTPUT_SIZE, 20);
}

TEST(RIPEMD160Test, DefaultConstructor) {
    RIPEMD160 hasher;
    // Should be able to create without error
    SUCCEED();
}

TEST(RIPEMD160Test, WriteAndFinalize) {
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    // Write empty data
    hasher.Write(nullptr, 0);
    hasher.Finalize(hash.data());
    
    // Should produce a 20-byte output
    EXPECT_EQ(hash.size(), 20);
}

TEST(RIPEMD160Test, Reset) {
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash1, hash2;
    
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    
    hasher.Write(data, 3);
    hasher.Finalize(hash1.data());
    
    hasher.Reset();
    hasher.Write(data, 3);
    hasher.Finalize(hash2.data());
    
    // Same input should produce same output after reset
    EXPECT_EQ(hash1, hash2);
}

TEST(RIPEMD160Test, ChainedWrites) {
    RIPEMD160 hasher;
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    
    // Write should return reference to allow chaining
    auto& ref = hasher.Write(data, 3);
    EXPECT_EQ(&ref, &hasher);
}

// ============================================================================
// RIPEMD160 Test Vectors (from official spec)
// ============================================================================

TEST(RIPEMD160Test, EmptyString) {
    // RIPEMD160("") = 9c1185a5c5e9fc54612808977ee8f548b2258d31
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    hasher.Finalize(hash.data());
    
    std::string expected = "9c1185a5c5e9fc54612808977ee8f548b2258d31";
    EXPECT_EQ(RIPEMDTestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(RIPEMD160Test, ABCString) {
    // RIPEMD160("abc") = 8eb208f7e05d987a9b044a8e98c6b087f15a0bfc
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    hasher.Write(data, 3);
    hasher.Finalize(hash.data());
    
    std::string expected = "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc";
    EXPECT_EQ(RIPEMDTestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(RIPEMD160Test, MessageDigest) {
    // RIPEMD160("message digest") = 5d0689ef49d2fae572b881b123a85ffa21595f36
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    const char* msg = "message digest";
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "5d0689ef49d2fae572b881b123a85ffa21595f36";
    EXPECT_EQ(RIPEMDTestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(RIPEMD160Test, AlphabetLower) {
    // RIPEMD160("abcdefghijklmnopqrstuvwxyz") = f71c27109c692c1b56bbdceb5b9d2865b3708dbc
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    const char* msg = "abcdefghijklmnopqrstuvwxyz";
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "f71c27109c692c1b56bbdceb5b9d2865b3708dbc";
    EXPECT_EQ(RIPEMDTestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(RIPEMD160Test, TwoBlockInput) {
    // RIPEMD160("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
    // = 12a053384a9c0c88e405a06c27dcf49ada62eb2b
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    const char* msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "12a053384a9c0c88e405a06c27dcf49ada62eb2b";
    EXPECT_EQ(RIPEMDTestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(RIPEMD160Test, AlphanumericMixed) {
    // RIPEMD160("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")
    // = b0e20b6e3116640286ed3a87a5713079b21f5189
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    const char* msg = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    hasher.Write(reinterpret_cast<const Byte*>(msg), strlen(msg));
    hasher.Finalize(hash.data());
    
    std::string expected = "b0e20b6e3116640286ed3a87a5713079b21f5189";
    EXPECT_EQ(RIPEMDTestBytesToHex(hash.data(), hash.size()), expected);
}

TEST(RIPEMD160Test, LargeInput) {
    // RIPEMD160(one million 'a' characters)
    // = 52783243c1697bdbe16d37f97f68f08325dc1528
    RIPEMD160 hasher;
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash;
    
    std::string msg(1000000, 'a');
    hasher.Write(reinterpret_cast<const Byte*>(msg.data()), msg.size());
    hasher.Finalize(hash.data());
    
    std::string expected = "52783243c1697bdbe16d37f97f68f08325dc1528";
    EXPECT_EQ(RIPEMDTestBytesToHex(hash.data(), hash.size()), expected);
}

// ============================================================================
// Incremental Hashing Tests
// ============================================================================

TEST(RIPEMD160Test, IncrementalHashing) {
    // Hash "abc" in one go and incrementally should produce same result
    std::array<Byte, RIPEMD160::OUTPUT_SIZE> hash1, hash2;
    
    RIPEMD160 hasher1;
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    hasher1.Write(data, 3);
    hasher1.Finalize(hash1.data());
    
    RIPEMD160 hasher2;
    hasher2.Write(&data[0], 1);
    hasher2.Write(&data[1], 1);
    hasher2.Write(&data[2], 1);
    hasher2.Finalize(hash2.data());
    
    EXPECT_EQ(hash1, hash2);
}

// ============================================================================
// Hash160 Tests (RIPEMD160(SHA256(x))) - Used for Bitcoin addresses
// ============================================================================

TEST(Hash160Test, BasicHash160) {
    // Hash160 of empty data
    const Byte* data = nullptr;
    Hash160 result = Hash160FromData(data, 0);
    
    // Hash160("") = b472a266d0bd89c13706a4132ccfb16f7c3b9fcb
    // This is RIPEMD160(SHA256(""))
    // SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    // RIPEMD160(above) = b472a266d0bd89c13706a4132ccfb16f7c3b9fcb
    std::string expected = "b472a266d0bd89c13706a4132ccfb16f7c3b9fcb";
    EXPECT_EQ(RIPEMDTestBytesToHex(result.data(), result.size()), expected);
}

TEST(Hash160Test, ABCHash160) {
    // Hash160("abc")
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    Hash160 result = Hash160FromData(data, 3);
    
    // SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    // RIPEMD160(above) = bb1be98c142444d7a56aa3981c3942a978e4dc33
    std::string expected = "bb1be98c142444d7a56aa3981c3942a978e4dc33";
    EXPECT_EQ(RIPEMDTestBytesToHex(result.data(), result.size()), expected);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST(RIPEMD160Test, SingleCallFunction) {
    // Test the convenience function RIPEMD160Hash
    const Byte data[] = {0x61, 0x62, 0x63}; // "abc"
    Hash160 result = RIPEMD160Hash(data, 3);
    
    std::string expected = "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc";
    EXPECT_EQ(RIPEMDTestBytesToHex(result.data(), result.size()), expected);
}

} // namespace test
} // namespace shurium
