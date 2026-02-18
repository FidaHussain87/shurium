// SHURIUM - Serialization Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/core/serialize.h"
#include "shurium/core/types.h"
#include <vector>
#include <string>
#include <array>
#include <limits>

using namespace shurium;

// ============================================================================
// DataStream Basic Tests
// ============================================================================

TEST(DataStreamTest, DefaultConstructor) {
    DataStream ds;
    EXPECT_TRUE(ds.empty());
    EXPECT_EQ(ds.size(), 0);
}

TEST(DataStreamTest, WriteAndRead) {
    DataStream ds;
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    ds.Write(data.data(), data.size());
    
    EXPECT_EQ(ds.size(), 4);
    EXPECT_FALSE(ds.empty());
    
    std::vector<uint8_t> result(4);
    ds.Read(result.data(), result.size());
    EXPECT_EQ(result, data);
}

TEST(DataStreamTest, Clear) {
    DataStream ds;
    ds << uint32_t(42);
    EXPECT_FALSE(ds.empty());
    
    ds.clear();
    EXPECT_TRUE(ds.empty());
    EXPECT_EQ(ds.size(), 0);
}

TEST(DataStreamTest, ConstructFromVector) {
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    DataStream ds(data);
    
    EXPECT_EQ(ds.size(), 4);
    
    uint8_t b1, b2, b3, b4;
    ds >> b1 >> b2 >> b3 >> b4;
    EXPECT_EQ(b1, 0xDE);
    EXPECT_EQ(b2, 0xAD);
    EXPECT_EQ(b3, 0xBE);
    EXPECT_EQ(b4, 0xEF);
}

// ============================================================================
// Integer Serialization Tests (Little-Endian)
// ============================================================================

TEST(SerializeTest, Uint8) {
    DataStream ds;
    uint8_t val = 0xAB;
    ds << val;
    
    EXPECT_EQ(ds.size(), 1);
    
    uint8_t result;
    ds >> result;
    EXPECT_EQ(result, val);
}

TEST(SerializeTest, Int8) {
    DataStream ds;
    int8_t val = -42;
    ds << val;
    
    EXPECT_EQ(ds.size(), 1);
    
    int8_t result;
    ds >> result;
    EXPECT_EQ(result, val);
}

TEST(SerializeTest, Uint16LittleEndian) {
    DataStream ds;
    uint16_t val = 0x1234;
    ds << val;
    
    EXPECT_EQ(ds.size(), 2);
    
    // Verify little-endian byte order
    auto data = ds.Data();
    EXPECT_EQ(data[0], 0x34);  // LSB first
    EXPECT_EQ(data[1], 0x12);  // MSB second
    
    uint16_t result;
    ds >> result;
    EXPECT_EQ(result, val);
}

TEST(SerializeTest, Int16) {
    DataStream ds;
    int16_t val = -1234;
    ds << val;
    
    EXPECT_EQ(ds.size(), 2);
    
    int16_t result;
    ds >> result;
    EXPECT_EQ(result, val);
}

TEST(SerializeTest, Uint32LittleEndian) {
    DataStream ds;
    uint32_t val = 0x12345678;
    ds << val;
    
    EXPECT_EQ(ds.size(), 4);
    
    // Verify little-endian byte order
    auto data = ds.Data();
    EXPECT_EQ(data[0], 0x78);
    EXPECT_EQ(data[1], 0x56);
    EXPECT_EQ(data[2], 0x34);
    EXPECT_EQ(data[3], 0x12);
    
    uint32_t result;
    ds >> result;
    EXPECT_EQ(result, val);
}

TEST(SerializeTest, Int32) {
    DataStream ds;
    int32_t val = -12345678;
    ds << val;
    
    EXPECT_EQ(ds.size(), 4);
    
    int32_t result;
    ds >> result;
    EXPECT_EQ(result, val);
}

TEST(SerializeTest, Uint64LittleEndian) {
    DataStream ds;
    uint64_t val = 0x123456789ABCDEF0ULL;
    ds << val;
    
    EXPECT_EQ(ds.size(), 8);
    
    // Verify little-endian byte order
    auto data = ds.Data();
    EXPECT_EQ(data[0], 0xF0);
    EXPECT_EQ(data[1], 0xDE);
    EXPECT_EQ(data[2], 0xBC);
    EXPECT_EQ(data[3], 0x9A);
    EXPECT_EQ(data[4], 0x78);
    EXPECT_EQ(data[5], 0x56);
    EXPECT_EQ(data[6], 0x34);
    EXPECT_EQ(data[7], 0x12);
    
    uint64_t result;
    ds >> result;
    EXPECT_EQ(result, val);
}

TEST(SerializeTest, Int64) {
    DataStream ds;
    int64_t val = -123456789012345LL;
    ds << val;
    
    EXPECT_EQ(ds.size(), 8);
    
    int64_t result;
    ds >> result;
    EXPECT_EQ(result, val);
}

// ============================================================================
// Bool Serialization Tests
// ============================================================================

TEST(SerializeTest, BoolTrue) {
    DataStream ds;
    bool val = true;
    ds << val;
    
    EXPECT_EQ(ds.size(), 1);
    EXPECT_EQ(ds.Data()[0], 0x01);
    
    bool result;
    ds >> result;
    EXPECT_TRUE(result);
}

TEST(SerializeTest, BoolFalse) {
    DataStream ds;
    bool val = false;
    ds << val;
    
    EXPECT_EQ(ds.size(), 1);
    EXPECT_EQ(ds.Data()[0], 0x00);
    
    bool result;
    ds >> result;
    EXPECT_FALSE(result);
}

// ============================================================================
// CompactSize Tests (Variable-Length Integer Encoding)
// ============================================================================

TEST(CompactSizeTest, Small) {
    // Values 0-252: 1 byte
    DataStream ds;
    WriteCompactSize(ds, 0);
    EXPECT_EQ(ds.size(), 1);
    EXPECT_EQ(ReadCompactSize(ds), 0ULL);
    
    ds.clear();
    WriteCompactSize(ds, 100);
    EXPECT_EQ(ds.size(), 1);
    EXPECT_EQ(ReadCompactSize(ds), 100ULL);
    
    ds.clear();
    WriteCompactSize(ds, 252);
    EXPECT_EQ(ds.size(), 1);
    EXPECT_EQ(ReadCompactSize(ds), 252ULL);
}

TEST(CompactSizeTest, Medium) {
    // Values 253-65535: 3 bytes (0xFD + 2 bytes)
    DataStream ds;
    WriteCompactSize(ds, 253);
    EXPECT_EQ(ds.size(), 3);
    EXPECT_EQ(ds.Data()[0], 0xFD);
    EXPECT_EQ(ReadCompactSize(ds), 253ULL);
    
    ds.clear();
    WriteCompactSize(ds, 0xFFFF);
    EXPECT_EQ(ds.size(), 3);
    EXPECT_EQ(ReadCompactSize(ds), 0xFFFFULL);
}

TEST(CompactSizeTest, Large) {
    // Values 65536-4294967295: 5 bytes (0xFE + 4 bytes)
    DataStream ds;
    WriteCompactSize(ds, 0x10000);
    EXPECT_EQ(ds.size(), 5);
    EXPECT_EQ(ds.Data()[0], 0xFE);
    // Use range_check=false since these values exceed MAX_SIZE for vector purposes
    EXPECT_EQ(ReadCompactSize(ds, false), 0x10000ULL);
    
    ds.clear();
    WriteCompactSize(ds, 0xFFFFFFFF);
    EXPECT_EQ(ds.size(), 5);
    EXPECT_EQ(ReadCompactSize(ds, false), 0xFFFFFFFFULL);
}

TEST(CompactSizeTest, VeryLarge) {
    // Values > 4294967295: 9 bytes (0xFF + 8 bytes)
    DataStream ds;
    WriteCompactSize(ds, 0x100000000ULL);
    EXPECT_EQ(ds.size(), 9);
    EXPECT_EQ(ds.Data()[0], 0xFF);
    // Use range_check=false since these values exceed MAX_SIZE for vector purposes
    EXPECT_EQ(ReadCompactSize(ds, false), 0x100000000ULL);
    
    ds.clear();
    WriteCompactSize(ds, 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(ds.size(), 9);
    EXPECT_EQ(ReadCompactSize(ds, false), 0xFFFFFFFFFFFFFFFFULL);
}

// ============================================================================
// Vector Serialization Tests
// ============================================================================

TEST(SerializeTest, VectorUint8) {
    DataStream ds;
    std::vector<uint8_t> vec = {0x01, 0x02, 0x03, 0x04, 0x05};
    ds << vec;
    
    // CompactSize(5) = 1 byte + 5 data bytes = 6 bytes
    EXPECT_EQ(ds.size(), 6);
    
    std::vector<uint8_t> result;
    ds >> result;
    EXPECT_EQ(result, vec);
}

TEST(SerializeTest, VectorUint32) {
    DataStream ds;
    std::vector<uint32_t> vec = {100, 200, 300};
    ds << vec;
    
    // CompactSize(3) = 1 byte + 3 * 4 bytes = 13 bytes
    EXPECT_EQ(ds.size(), 13);
    
    std::vector<uint32_t> result;
    ds >> result;
    EXPECT_EQ(result, vec);
}

TEST(SerializeTest, EmptyVector) {
    DataStream ds;
    std::vector<uint8_t> vec;
    ds << vec;
    
    // CompactSize(0) = 1 byte
    EXPECT_EQ(ds.size(), 1);
    
    std::vector<uint8_t> result;
    ds >> result;
    EXPECT_TRUE(result.empty());
}

TEST(SerializeTest, LargeVector) {
    DataStream ds;
    std::vector<uint8_t> vec(1000, 0xAB);
    ds << vec;
    
    // CompactSize(1000) = 3 bytes + 1000 data bytes = 1003 bytes
    EXPECT_EQ(ds.size(), 1003);
    
    std::vector<uint8_t> result;
    ds >> result;
    EXPECT_EQ(result, vec);
}

// ============================================================================
// String Serialization Tests
// ============================================================================

TEST(SerializeTest, String) {
    DataStream ds;
    std::string str = "Hello, SHURIUM!";
    ds << str;
    
    // CompactSize(13) = 1 byte + 13 chars = 14 bytes
    EXPECT_EQ(ds.size(), 14);
    
    std::string result;
    ds >> result;
    EXPECT_EQ(result, str);
}

TEST(SerializeTest, EmptyString) {
    DataStream ds;
    std::string str;
    ds << str;
    
    EXPECT_EQ(ds.size(), 1);  // Just CompactSize(0)
    
    std::string result;
    ds >> result;
    EXPECT_TRUE(result.empty());
}

TEST(SerializeTest, StringWithNulls) {
    DataStream ds;
    std::string str = "Hello\0World";  // Note: this creates "Hello" due to C++ string literal
    str = std::string("Hello\0World", 11);  // Explicit length to include null
    ds << str;
    
    EXPECT_EQ(ds.size(), 12);  // 1 byte size + 11 chars
    
    std::string result;
    ds >> result;
    EXPECT_EQ(result.size(), 11);
    EXPECT_EQ(result, str);
}

// ============================================================================
// Array Serialization Tests
// ============================================================================

TEST(SerializeTest, ArrayUint8) {
    DataStream ds;
    std::array<uint8_t, 4> arr = {0xDE, 0xAD, 0xBE, 0xEF};
    ds << arr;
    
    // Fixed-size arrays: no length prefix
    EXPECT_EQ(ds.size(), 4);
    
    std::array<uint8_t, 4> result;
    ds >> result;
    EXPECT_EQ(result, arr);
}

TEST(SerializeTest, ArrayUint32) {
    DataStream ds;
    std::array<uint32_t, 3> arr = {0x12345678, 0x9ABCDEF0, 0x11223344};
    ds << arr;
    
    EXPECT_EQ(ds.size(), 12);
    
    std::array<uint32_t, 3> result;
    ds >> result;
    EXPECT_EQ(result, arr);
}

// ============================================================================
// Hash Type Serialization Tests
// ============================================================================

TEST(SerializeTest, Hash256) {
    DataStream ds;
    Hash256 hash;
    for (int i = 0; i < 32; i++) {
        hash[i] = i;
    }
    ds << hash;
    
    EXPECT_EQ(ds.size(), 32);
    
    Hash256 result;
    ds >> result;
    EXPECT_EQ(result, hash);
}

TEST(SerializeTest, Hash160) {
    DataStream ds;
    Hash160 hash;
    for (int i = 0; i < 20; i++) {
        hash[i] = i * 2;
    }
    ds << hash;
    
    EXPECT_EQ(ds.size(), 20);
    
    Hash160 result;
    ds >> result;
    EXPECT_EQ(result, hash);
}

// ============================================================================
// Multiple Values Tests
// ============================================================================

TEST(SerializeTest, MultipleValues) {
    DataStream ds;
    
    uint8_t a = 0x12;
    uint32_t b = 0x34567890;
    std::string c = "test";
    bool d = true;
    
    ds << a << b << c << d;
    
    // 1 + 4 + (1 + 4) + 1 = 11 bytes
    EXPECT_EQ(ds.size(), 11);
    
    uint8_t ra;
    uint32_t rb;
    std::string rc;
    bool rd;
    
    ds >> ra >> rb >> rc >> rd;
    
    EXPECT_EQ(ra, a);
    EXPECT_EQ(rb, b);
    EXPECT_EQ(rc, c);
    EXPECT_EQ(rd, d);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(SerializeTest, MaxValues) {
    DataStream ds;
    
    ds << std::numeric_limits<uint8_t>::max();
    ds << std::numeric_limits<uint16_t>::max();
    ds << std::numeric_limits<uint32_t>::max();
    ds << std::numeric_limits<uint64_t>::max();
    ds << std::numeric_limits<int8_t>::min();
    ds << std::numeric_limits<int16_t>::min();
    ds << std::numeric_limits<int32_t>::min();
    ds << std::numeric_limits<int64_t>::min();
    
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    
    ds >> u8 >> u16 >> u32 >> u64 >> i8 >> i16 >> i32 >> i64;
    
    EXPECT_EQ(u8, std::numeric_limits<uint8_t>::max());
    EXPECT_EQ(u16, std::numeric_limits<uint16_t>::max());
    EXPECT_EQ(u32, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(u64, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(i8, std::numeric_limits<int8_t>::min());
    EXPECT_EQ(i16, std::numeric_limits<int16_t>::min());
    EXPECT_EQ(i32, std::numeric_limits<int32_t>::min());
    EXPECT_EQ(i64, std::numeric_limits<int64_t>::min());
}

TEST(DataStreamTest, ReadPastEnd) {
    DataStream ds;
    ds << uint8_t(0x42);
    
    uint8_t val;
    ds >> val;
    EXPECT_EQ(val, 0x42);
    
    // Reading past end should throw
    EXPECT_THROW(ds >> val, std::ios_base::failure);
}

TEST(DataStreamTest, Rewind) {
    DataStream ds;
    ds << uint32_t(0x12345678) << uint32_t(0xDEADBEEF);
    
    uint32_t val1;
    ds >> val1;
    EXPECT_EQ(val1, 0x12345678);
    EXPECT_EQ(ds.size(), 4);  // Still 4 bytes left
    
    // Rewind to read first value again (partial rewind)
    EXPECT_TRUE(ds.Rewind(4));  // Rewind by 4 bytes
    EXPECT_EQ(ds.size(), 8);
    
    uint32_t val2;
    ds >> val2;
    EXPECT_EQ(val2, 0x12345678);  // Read same value again
    
    // Full rewind
    ds.Rewind();  // Back to start
    EXPECT_EQ(ds.size(), 8);
    
    ds >> val1 >> val2;
    EXPECT_EQ(val1, 0x12345678);
    EXPECT_EQ(val2, 0xDEADBEEF);
}

TEST(DataStreamTest, Ignore) {
    DataStream ds;
    ds << uint8_t(0x01) << uint8_t(0x02) << uint8_t(0x03) << uint8_t(0x04);
    
    ds.Ignore(2);
    EXPECT_EQ(ds.size(), 2);
    
    uint8_t b1, b2;
    ds >> b1 >> b2;
    EXPECT_EQ(b1, 0x03);
    EXPECT_EQ(b2, 0x04);
}

// ============================================================================
// GetSerializeSize Tests
// ============================================================================

TEST(SerializeSizeTest, BasicTypes) {
    EXPECT_EQ(GetSerializeSize(uint8_t(0)), 1);
    EXPECT_EQ(GetSerializeSize(uint16_t(0)), 2);
    EXPECT_EQ(GetSerializeSize(uint32_t(0)), 4);
    EXPECT_EQ(GetSerializeSize(uint64_t(0)), 8);
    EXPECT_EQ(GetSerializeSize(int8_t(0)), 1);
    EXPECT_EQ(GetSerializeSize(int16_t(0)), 2);
    EXPECT_EQ(GetSerializeSize(int32_t(0)), 4);
    EXPECT_EQ(GetSerializeSize(int64_t(0)), 8);
    EXPECT_EQ(GetSerializeSize(true), 1);
}

TEST(SerializeSizeTest, Vector) {
    std::vector<uint8_t> small(100);
    EXPECT_EQ(GetSerializeSize(small), 101);  // 1 byte size + 100 bytes
    
    std::vector<uint8_t> medium(1000);
    EXPECT_EQ(GetSerializeSize(medium), 1003);  // 3 bytes size + 1000 bytes
    
    std::vector<uint32_t> ints(10);
    EXPECT_EQ(GetSerializeSize(ints), 41);  // 1 byte size + 10 * 4 bytes
}

TEST(SerializeSizeTest, String) {
    std::string empty;
    EXPECT_EQ(GetSerializeSize(empty), 1);
    
    std::string small = "Hello";
    EXPECT_EQ(GetSerializeSize(small), 6);  // 1 byte size + 5 chars
}

// ============================================================================
// Nested Vector Tests
// ============================================================================

TEST(SerializeTest, NestedVector) {
    DataStream ds;
    std::vector<std::vector<uint8_t>> nested = {
        {1, 2, 3},
        {4, 5},
        {6, 7, 8, 9}
    };
    ds << nested;
    
    std::vector<std::vector<uint8_t>> result;
    ds >> result;
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], (std::vector<uint8_t>{1, 2, 3}));
    EXPECT_EQ(result[1], (std::vector<uint8_t>{4, 5}));
    EXPECT_EQ(result[2], (std::vector<uint8_t>{6, 7, 8, 9}));
}

// ============================================================================
// Hex Conversion Tests
// ============================================================================

TEST(DataStreamTest, ToHex) {
    DataStream ds;
    ds << uint32_t(0xDEADBEEF);
    
    std::string hex = ds.ToHex();
    EXPECT_EQ(hex, "efbeadde");  // Little-endian
}

TEST(DataStreamTest, FromHex) {
    DataStream ds;
    ds.FromHex("efbeadde");
    
    uint32_t val;
    ds >> val;
    EXPECT_EQ(val, 0xDEADBEEF);
}

// ============================================================================
// Amount Serialization Tests
// ============================================================================

TEST(SerializeTest, Amount) {
    DataStream ds;
    Amount amt = 12345678901234LL;
    ds << amt;
    
    EXPECT_EQ(ds.size(), 8);
    
    Amount result;
    ds >> result;
    EXPECT_EQ(result, amt);
}

TEST(SerializeTest, NegativeAmount) {
    DataStream ds;
    Amount amt = -5000LL;
    ds << amt;
    
    Amount result;
    ds >> result;
    EXPECT_EQ(result, amt);
}
