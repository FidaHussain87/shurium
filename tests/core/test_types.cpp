// SHURIUM - Core Types Tests (TDD)
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// These tests define the expected behavior of core types.
// Implementation should make all tests pass.

#include <gtest/gtest.h>
#include "shurium/core/types.h"
#include "shurium/core/hex.h"

namespace shurium {
namespace test {

// ============================================================================
// Byte Type Tests
// ============================================================================

TEST(ByteTest, SizeIsOneByte) {
    EXPECT_EQ(sizeof(Byte), 1);
}

TEST(ByteTest, CanHoldFullRange) {
    Byte b = 0;
    EXPECT_EQ(b, 0);
    b = 255;
    EXPECT_EQ(b, 255);
}

// ============================================================================
// Hash256 Tests
// ============================================================================

TEST(Hash256Test, DefaultConstructorCreatesZeroHash) {
    Hash256 h;
    EXPECT_TRUE(h.IsNull());
    for (size_t i = 0; i < Hash256::SIZE; ++i) {
        EXPECT_EQ(h[i], 0);
    }
}

TEST(Hash256Test, SizeIs32Bytes) {
    EXPECT_EQ(Hash256::SIZE, 32);
    Hash256 h;
    EXPECT_EQ(h.size(), 32);
}

TEST(Hash256Test, ConstructFromBytes) {
    std::array<Byte, 32> data;
    for (size_t i = 0; i < 32; ++i) {
        data[i] = static_cast<Byte>(i);
    }
    
    Hash256 h(data);
    for (size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(h[i], i);
    }
}

TEST(Hash256Test, EqualityOperator) {
    Hash256 h1, h2;
    EXPECT_EQ(h1, h2);
    
    std::array<Byte, 32> data;
    data.fill(0x42);
    Hash256 h3(data);
    Hash256 h4(data);
    
    EXPECT_EQ(h3, h4);
    EXPECT_NE(h1, h3);
}

TEST(Hash256Test, LessThanOperator) {
    std::array<Byte, 32> data1, data2;
    data1.fill(0x00);
    data2.fill(0x00);
    data2[31] = 0x01;
    
    Hash256 h1(data1);
    Hash256 h2(data2);
    
    EXPECT_LT(h1, h2);
    EXPECT_FALSE(h2 < h1);
}

TEST(Hash256Test, SetNull) {
    std::array<Byte, 32> data;
    data.fill(0xFF);
    Hash256 h(data);
    
    EXPECT_FALSE(h.IsNull());
    h.SetNull();
    EXPECT_TRUE(h.IsNull());
}

TEST(Hash256Test, ToHex) {
    std::array<Byte, 32> data;
    data.fill(0x00);
    data[0] = 0xAB;
    data[31] = 0xCD;
    
    Hash256 h(data);
    std::string hex = h.ToHex();
    
    // Hash is displayed in reverse byte order (little-endian display)
    EXPECT_EQ(hex.length(), 64);
    EXPECT_EQ(hex.substr(0, 2), "cd");  // Last byte first
    EXPECT_EQ(hex.substr(62, 2), "ab"); // First byte last
}

TEST(Hash256Test, FromHex) {
    std::string hex = "0000000000000000000000000000000000000000000000000000000000000001";
    Hash256 h = Hash256::FromHex(hex);
    
    EXPECT_FALSE(h.IsNull());
    // Hex display is big-endian (MSB first), internal storage is little-endian
    // So "01" at the end of hex string maps to data_[0]
    EXPECT_EQ(h[0], 0x01);  // Little-endian storage: LSB at index 0
}

TEST(Hash256Test, FromHexInvalid) {
    EXPECT_THROW(Hash256::FromHex("invalid"), std::invalid_argument);
    EXPECT_THROW(Hash256::FromHex("0123"), std::invalid_argument); // Too short
}

TEST(Hash256Test, BeginEnd) {
    Hash256 h;
    EXPECT_EQ(h.end() - h.begin(), 32);
    
    std::array<Byte, 32> data;
    data.fill(0x42);
    Hash256 h2(data);
    
    size_t count = 0;
    for (auto it = h2.begin(); it != h2.end(); ++it) {
        EXPECT_EQ(*it, 0x42);
        ++count;
    }
    EXPECT_EQ(count, 32);
}

TEST(Hash256Test, DataPointer) {
    Hash256 h;
    EXPECT_NE(h.data(), nullptr);
    EXPECT_EQ(h.data(), h.begin());
}

// ============================================================================
// Hash512 Tests
// ============================================================================

TEST(Hash512Test, SizeIs64Bytes) {
    EXPECT_EQ(Hash512::SIZE, 64);
    Hash512 h;
    EXPECT_EQ(h.size(), 64);
}

TEST(Hash512Test, DefaultIsNull) {
    Hash512 h;
    EXPECT_TRUE(h.IsNull());
}

// ============================================================================
// Hash160 Tests
// ============================================================================

TEST(Hash160Test, SizeIs20Bytes) {
    EXPECT_EQ(Hash160::SIZE, 20);
    Hash160 h;
    EXPECT_EQ(h.size(), 20);
}

// ============================================================================
// Amount Tests
// ============================================================================

TEST(AmountTest, Coin) {
    EXPECT_EQ(COIN, 100000000LL);  // 1 NXS = 100 million smallest units
}

TEST(AmountTest, MaxMoney) {
    // Maximum supply: ~21 billion NXS (more than Bitcoin since we have UBI)
    EXPECT_GT(MAX_MONEY, 0);
    EXPECT_EQ(MAX_MONEY, 21000000000LL * COIN);
}

TEST(AmountTest, MoneyRange) {
    EXPECT_TRUE(MoneyRange(0));
    EXPECT_TRUE(MoneyRange(COIN));
    EXPECT_TRUE(MoneyRange(MAX_MONEY));
    EXPECT_FALSE(MoneyRange(-1));
    EXPECT_FALSE(MoneyRange(MAX_MONEY + 1));
}

// ============================================================================
// Timestamp Tests
// ============================================================================

TEST(TimestampTest, Now) {
    Timestamp t1 = GetTime();
    EXPECT_GT(t1, 0);
    
    Timestamp t2 = GetTime();
    EXPECT_GE(t2, t1);
}

TEST(TimestampTest, GetTimeMillis) {
    int64_t ms = GetTimeMillis();
    EXPECT_GT(ms, 0);
}

// ============================================================================
// CompactSize Tests
// ============================================================================

TEST(CompactSizeTest, SmallValues) {
    // Values < 253 are encoded as single byte
    EXPECT_EQ(GetCompactSizeSize(0), 1);
    EXPECT_EQ(GetCompactSizeSize(252), 1);
}

TEST(CompactSizeTest, MediumValues) {
    // Values 253-65535 are encoded as 3 bytes
    EXPECT_EQ(GetCompactSizeSize(253), 3);
    EXPECT_EQ(GetCompactSizeSize(65535), 3);
}

TEST(CompactSizeTest, LargeValues) {
    // Values 65536-4294967295 are encoded as 5 bytes
    EXPECT_EQ(GetCompactSizeSize(65536), 5);
    EXPECT_EQ(GetCompactSizeSize(4294967295ULL), 5);
}

TEST(CompactSizeTest, VeryLargeValues) {
    // Values > 4294967295 are encoded as 9 bytes
    EXPECT_EQ(GetCompactSizeSize(4294967296ULL), 9);
}

// ============================================================================
// Span Tests
// ============================================================================

TEST(SpanTest, CreateFromVector) {
    std::vector<Byte> vec = {1, 2, 3, 4, 5};
    Span<Byte> span(vec);
    
    EXPECT_EQ(span.size(), 5);
    EXPECT_EQ(span[0], 1);
    EXPECT_EQ(span[4], 5);
}

TEST(SpanTest, CreateFromArray) {
    std::array<Byte, 4> arr = {10, 20, 30, 40};
    Span<const Byte> span(arr);
    
    EXPECT_EQ(span.size(), 4);
    EXPECT_EQ(span[0], 10);
}

TEST(SpanTest, Subspan) {
    std::vector<Byte> vec = {1, 2, 3, 4, 5};
    Span<Byte> span(vec);
    
    auto sub = span.subspan(1, 3);
    EXPECT_EQ(sub.size(), 3);
    EXPECT_EQ(sub[0], 2);
    EXPECT_EQ(sub[2], 4);
}

TEST(SpanTest, First) {
    std::vector<Byte> vec = {1, 2, 3, 4, 5};
    Span<Byte> span(vec);
    
    auto first3 = span.first(3);
    EXPECT_EQ(first3.size(), 3);
    EXPECT_EQ(first3[2], 3);
}

TEST(SpanTest, Last) {
    std::vector<Byte> vec = {1, 2, 3, 4, 5};
    Span<Byte> span(vec);
    
    auto last2 = span.last(2);
    EXPECT_EQ(last2.size(), 2);
    EXPECT_EQ(last2[0], 4);
    EXPECT_EQ(last2[1], 5);
}

} // namespace test
} // namespace shurium
