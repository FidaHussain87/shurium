// SHURIUM - Random Number Generation Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/core/random.h"
#include "shurium/core/types.h"
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>

using namespace shurium;

// ============================================================================
// GetRandBytes Tests
// ============================================================================

TEST(RandomTest, GetRandBytesNonZero) {
    // Random bytes should not all be zero (extremely unlikely)
    std::vector<uint8_t> bytes(32);
    GetRandBytes(bytes.data(), bytes.size());
    
    bool allZero = std::all_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(allZero);
}

TEST(RandomTest, GetRandBytesDifferent) {
    // Two calls should produce different results
    std::vector<uint8_t> bytes1(32);
    std::vector<uint8_t> bytes2(32);
    
    GetRandBytes(bytes1.data(), bytes1.size());
    GetRandBytes(bytes2.data(), bytes2.size());
    
    EXPECT_NE(bytes1, bytes2);
}

TEST(RandomTest, GetRandBytesZeroLength) {
    // Should handle zero-length request without error
    uint8_t dummy = 0;
    EXPECT_NO_THROW(GetRandBytes(&dummy, 0));
}

TEST(RandomTest, GetRandBytesLargeBuffer) {
    // Should handle large buffers
    std::vector<uint8_t> bytes(4096);
    EXPECT_NO_THROW(GetRandBytes(bytes.data(), bytes.size()));
    
    // Check that it's not all zeros
    bool allZero = std::all_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(allZero);
}

// ============================================================================
// GetRandHash Tests
// ============================================================================

TEST(RandomTest, GetRandHash256) {
    Hash256 hash = GetRandHash256();
    
    // Should not be null
    EXPECT_FALSE(hash.IsNull());
}

TEST(RandomTest, GetRandHash256Different) {
    Hash256 hash1 = GetRandHash256();
    Hash256 hash2 = GetRandHash256();
    
    EXPECT_NE(hash1, hash2);
}

// ============================================================================
// GetRandInt Tests
// ============================================================================

TEST(RandomTest, GetRandUint64) {
    uint64_t val1 = GetRandUint64();
    uint64_t val2 = GetRandUint64();
    
    // Very unlikely to be equal
    EXPECT_NE(val1, val2);
}

TEST(RandomTest, GetRandUint32) {
    uint32_t val1 = GetRandUint32();
    uint32_t val2 = GetRandUint32();
    
    // Very unlikely to be equal
    EXPECT_NE(val1, val2);
}

TEST(RandomTest, GetRandIntRange) {
    // Generate many random numbers in range [0, 100)
    std::set<uint64_t> values;
    for (int i = 0; i < 1000; ++i) {
        uint64_t val = GetRandInt(100);
        EXPECT_LT(val, 100ULL);
        values.insert(val);
    }
    
    // Should have generated many different values
    EXPECT_GT(values.size(), 50UL);
}

TEST(RandomTest, GetRandIntRangeOne) {
    // Range of 1 should always return 0
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(GetRandInt(1), 0ULL);
    }
}

TEST(RandomTest, GetRandBool) {
    int trueCount = 0;
    int falseCount = 0;
    
    for (int i = 0; i < 1000; ++i) {
        if (GetRandBool()) {
            trueCount++;
        } else {
            falseCount++;
        }
    }
    
    // Should be roughly 50/50 (allow wide margin)
    EXPECT_GT(trueCount, 300);
    EXPECT_GT(falseCount, 300);
}

// ============================================================================
// Entropy Quality Tests (Basic Statistical Tests)
// ============================================================================

TEST(RandomTest, ByteDistribution) {
    // Generate a large amount of random data
    std::vector<uint8_t> bytes(10000);
    GetRandBytes(bytes.data(), bytes.size());
    
    // Count occurrences of each byte value
    std::vector<int> counts(256, 0);
    for (uint8_t b : bytes) {
        counts[b]++;
    }
    
    // Expected count per value: 10000/256 ≈ 39
    // Check that no value appears too many or too few times
    // Allow range of [10, 80] which is very generous
    for (int i = 0; i < 256; ++i) {
        EXPECT_GT(counts[i], 10) << "Byte value " << i << " appeared too few times";
        EXPECT_LT(counts[i], 80) << "Byte value " << i << " appeared too many times";
    }
}

TEST(RandomTest, BitDistribution) {
    // Generate random data and check bit distribution
    std::vector<uint8_t> bytes(1000);
    GetRandBytes(bytes.data(), bytes.size());
    
    int ones = 0;
    int zeros = 0;
    
    for (uint8_t b : bytes) {
        for (int i = 0; i < 8; ++i) {
            if (b & (1 << i)) {
                ones++;
            } else {
                zeros++;
            }
        }
    }
    
    // Total bits: 8000
    // Expected: ~4000 each
    // Allow 40-60% range
    EXPECT_GT(ones, 3200);
    EXPECT_LT(ones, 4800);
    EXPECT_GT(zeros, 3200);
    EXPECT_LT(zeros, 4800);
}

// ============================================================================
// Thread Safety Test (Basic)
// ============================================================================

TEST(RandomTest, MultipleCalls) {
    // Make many calls to ensure no crashes or hangs
    for (int i = 0; i < 100; ++i) {
        std::vector<uint8_t> bytes(64);
        GetRandBytes(bytes.data(), bytes.size());
        
        uint64_t val = GetRandUint64();
        (void)val;  // Suppress unused warning
        
        Hash256 hash = GetRandHash256();
        (void)hash;
    }
}

// ============================================================================
// Span Interface Tests
// ============================================================================

TEST(RandomTest, GetRandBytesSpan) {
    std::vector<uint8_t> bytes(32, 0);
    Span<uint8_t> span(bytes);
    
    GetRandBytes(span);
    
    bool allZero = std::all_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(allZero);
}

// ============================================================================
// Shuffle Test
// ============================================================================

TEST(RandomTest, Shuffle) {
    std::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> shuffled = original;
    
    Shuffle(shuffled.begin(), shuffled.end());
    
    // Should contain same elements
    std::sort(shuffled.begin(), shuffled.end());
    EXPECT_EQ(shuffled, original);
}

TEST(RandomTest, ShuffleChangesOrder) {
    std::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> shuffled = original;
    
    // Shuffle multiple times - at least one should be different
    bool foundDifferent = false;
    for (int i = 0; i < 10; ++i) {
        shuffled = original;
        Shuffle(shuffled.begin(), shuffled.end());
        if (shuffled != original) {
            foundDifferent = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundDifferent);
}
