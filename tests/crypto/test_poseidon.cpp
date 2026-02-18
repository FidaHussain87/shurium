// SHURIUM - Poseidon Hash Tests (TDD)
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Tests for finite field arithmetic and Poseidon hash function

#include <gtest/gtest.h>
#include "shurium/crypto/field.h"
#include "shurium/crypto/poseidon.h"
#include "shurium/core/types.h"

#include <string>
#include <vector>

namespace shurium {
namespace test {

// ============================================================================
// Uint256 Tests
// ============================================================================

TEST(Uint256Test, DefaultConstructorIsZero) {
    Uint256 a;
    EXPECT_TRUE(a.IsZero());
}

TEST(Uint256Test, LimbConstructor) {
    Uint256 a(0x1234, 0x5678, 0x9ABC, 0xDEF0);
    EXPECT_EQ(a.limbs[0], 0x1234ULL);
    EXPECT_EQ(a.limbs[1], 0x5678ULL);
    EXPECT_EQ(a.limbs[2], 0x9ABCULL);
    EXPECT_EQ(a.limbs[3], 0xDEF0ULL);
}

TEST(Uint256Test, FromHex) {
    // Test with a simple value
    Uint256 a = Uint256::FromHex("0x0000000000000000000000000000000000000000000000000000000000000001");
    EXPECT_EQ(a.limbs[0], 1ULL);
    EXPECT_EQ(a.limbs[1], 0ULL);
    EXPECT_EQ(a.limbs[2], 0ULL);
    EXPECT_EQ(a.limbs[3], 0ULL);
}

TEST(Uint256Test, ToHex) {
    Uint256 a(1, 0, 0, 0);
    std::string hex = a.ToHex();
    EXPECT_EQ(hex, "0000000000000000000000000000000000000000000000000000000000000001");
}

TEST(Uint256Test, Comparison) {
    Uint256 a(1, 0, 0, 0);
    Uint256 b(2, 0, 0, 0);
    Uint256 c(1, 0, 0, 0);
    
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(a == c);
    EXPECT_TRUE(a <= c);
    EXPECT_TRUE(a >= c);
}

TEST(Uint256Test, Addition) {
    Uint256 a(0xFFFFFFFFFFFFFFFFULL, 0, 0, 0);
    Uint256 b(1, 0, 0, 0);
    bool carry;
    Uint256 c = Uint256::Add(a, b, carry);
    
    EXPECT_EQ(c.limbs[0], 0ULL);
    EXPECT_EQ(c.limbs[1], 1ULL);
    EXPECT_FALSE(carry);
}

TEST(Uint256Test, Subtraction) {
    Uint256 a(0, 1, 0, 0);
    Uint256 b(1, 0, 0, 0);
    bool borrow;
    Uint256 c = Uint256::Sub(a, b, borrow);
    
    EXPECT_EQ(c.limbs[0], 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(c.limbs[1], 0ULL);
    EXPECT_FALSE(borrow);
}

// ============================================================================
// FieldElement Tests
// ============================================================================

TEST(FieldElementTest, ZeroAndOne) {
    FieldElement zero = FieldElement::Zero();
    FieldElement one = FieldElement::One();
    
    EXPECT_TRUE(zero.IsZero());
    EXPECT_FALSE(one.IsZero());
    EXPECT_NE(zero, one);
    
    // Test that One converts back correctly
    Uint256 oneVal = one.ToUint256();
    EXPECT_EQ(oneVal.limbs[0], 1ULL);
    EXPECT_EQ(oneVal.limbs[1], 0ULL);
    EXPECT_EQ(oneVal.limbs[2], 0ULL);
    EXPECT_EQ(oneVal.limbs[3], 0ULL);
}

TEST(FieldElementTest, Addition) {
    FieldElement a(1);
    FieldElement b(2);
    FieldElement c = a + b;
    
    EXPECT_EQ(c.ToUint256().limbs[0], 3ULL);
}

TEST(FieldElementTest, Subtraction) {
    FieldElement a(5);
    FieldElement b(3);
    FieldElement c = a - b;
    
    EXPECT_EQ(c.ToUint256().limbs[0], 2ULL);
}

TEST(FieldElementTest, Multiplication) {
    FieldElement a(3);
    FieldElement b(4);
    FieldElement c = a * b;
    
    EXPECT_EQ(c.ToUint256().limbs[0], 12ULL);
}

TEST(FieldElementTest, Squaring) {
    FieldElement a(5);
    FieldElement b = a.Square();
    
    EXPECT_EQ(b.ToUint256().limbs[0], 25ULL);
}

TEST(FieldElementTest, Negation) {
    FieldElement a(1);
    FieldElement neg_a = -a;
    FieldElement sum = a + neg_a;
    
    EXPECT_TRUE(sum.IsZero());
}

TEST(FieldElementTest, Inverse) {
    FieldElement a(3);
    FieldElement inv_a = a.Inverse();
    FieldElement product = a * inv_a;
    
    EXPECT_EQ(product, FieldElement::One());
}

TEST(FieldElementTest, PoseidonSbox) {
    // S-box is x^5
    FieldElement a(2);
    FieldElement result = a.PoseidonSbox();
    
    // 2^5 = 32
    EXPECT_EQ(result.ToUint256().limbs[0], 32ULL);
}

TEST(FieldElementTest, Power) {
    // Test with small exponents
    FieldElement two(2);
    
    // 2^0 = 1
    FieldElement r0 = two.Pow(Uint256(0, 0, 0, 0));
    EXPECT_EQ(r0.ToUint256().limbs[0], 1ULL);
    
    // 2^1 = 2
    FieldElement r1 = two.Pow(Uint256(1, 0, 0, 0));
    EXPECT_EQ(r1.ToUint256().limbs[0], 2ULL);
    
    // 2^2 = 4
    FieldElement r2 = two.Pow(Uint256(2, 0, 0, 0));
    EXPECT_EQ(r2.ToUint256().limbs[0], 4ULL);
    
    // 2^3 = 8
    FieldElement r3 = two.Pow(Uint256(3, 0, 0, 0));
    EXPECT_EQ(r3.ToUint256().limbs[0], 8ULL);
    
    // 2^10 = 1024
    FieldElement r10 = two.Pow(Uint256(10, 0, 0, 0));
    EXPECT_EQ(r10.ToUint256().limbs[0], 1024ULL);
}

// ============================================================================
// Poseidon Hash Tests
// ============================================================================

TEST(PoseidonTest, DefaultConstructor) {
    Poseidon hasher;
    // Should create without error
    SUCCEED();
}

TEST(PoseidonTest, ConfiguredConstructor) {
    Poseidon hasher(PoseidonParams::CONFIG_2_1);
    SUCCEED();
}

TEST(PoseidonTest, HashSingleElement) {
    FieldElement input(42);
    FieldElement result = Poseidon::Hash({input});
    
    // Result should be deterministic and non-zero
    EXPECT_FALSE(result.IsZero());
}

TEST(PoseidonTest, HashTwoElements) {
    FieldElement a(1);
    FieldElement b(2);
    FieldElement result = Poseidon::Hash2(a, b);
    
    EXPECT_FALSE(result.IsZero());
}

TEST(PoseidonTest, HashDeterministic) {
    // Same inputs should produce same output
    FieldElement a(123);
    FieldElement b(456);
    
    FieldElement result1 = Poseidon::Hash2(a, b);
    FieldElement result2 = Poseidon::Hash2(a, b);
    
    EXPECT_EQ(result1, result2);
}

TEST(PoseidonTest, HashDifferentInputs) {
    // Different inputs should produce different outputs
    FieldElement a(1);
    FieldElement b(2);
    FieldElement c(3);
    
    FieldElement result1 = Poseidon::Hash2(a, b);
    FieldElement result2 = Poseidon::Hash2(a, c);
    FieldElement result3 = Poseidon::Hash2(b, c);
    
    EXPECT_NE(result1, result2);
    EXPECT_NE(result1, result3);
    EXPECT_NE(result2, result3);
}

TEST(PoseidonTest, HashOrderMatters) {
    // Hash(a, b) != Hash(b, a)
    FieldElement a(1);
    FieldElement b(2);
    
    FieldElement result1 = Poseidon::Hash2(a, b);
    FieldElement result2 = Poseidon::Hash2(b, a);
    
    EXPECT_NE(result1, result2);
}

TEST(PoseidonTest, HashBytes) {
    const Byte data[] = {0x01, 0x02, 0x03, 0x04};
    FieldElement result = Poseidon::HashBytes(data, sizeof(data));
    
    EXPECT_FALSE(result.IsZero());
}

TEST(PoseidonTest, HashBytesToBytes) {
    const Byte data[] = {0x01, 0x02, 0x03, 0x04};
    auto result = Poseidon::HashToBytes(data, sizeof(data));
    
    EXPECT_EQ(result.size(), 32);
    
    // Should not be all zeros
    bool allZero = true;
    for (auto b : result) {
        if (b != 0) {
            allZero = false;
            break;
        }
    }
    EXPECT_FALSE(allZero);
}

TEST(PoseidonTest, IncrementalAbsorption) {
    // Absorbing incrementally should give same result as absorbing all at once
    FieldElement a(1);
    FieldElement b(2);
    FieldElement c(3);
    
    // All at once
    FieldElement result1 = Poseidon::Hash({a, b, c});
    
    // Incrementally
    Poseidon hasher;
    hasher.Absorb(a);
    hasher.Absorb(b);
    hasher.Absorb(c);
    FieldElement result2 = hasher.Squeeze();
    
    EXPECT_EQ(result1, result2);
}

TEST(PoseidonTest, Reset) {
    Poseidon hasher;
    hasher.Absorb(FieldElement(42));
    FieldElement result1 = hasher.Squeeze();
    
    hasher.Reset();
    hasher.Absorb(FieldElement(42));
    FieldElement result2 = hasher.Squeeze();
    
    EXPECT_EQ(result1, result2);
}

TEST(PoseidonTest, MultipleSqueezes) {
    Poseidon hasher;
    hasher.Absorb(FieldElement(123));
    
    FieldElement result1 = hasher.Squeeze();
    FieldElement result2 = hasher.Squeeze();
    
    // Multiple squeezes should give different outputs
    EXPECT_NE(result1, result2);
}

// ============================================================================
// Merkle Tree Compatibility Test
// ============================================================================

TEST(PoseidonTest, MerkleTreeHash) {
    // Simulate a 2-level Merkle tree
    FieldElement leaf1(100);
    FieldElement leaf2(200);
    FieldElement leaf3(300);
    FieldElement leaf4(400);
    
    // Level 1
    FieldElement node1 = Poseidon::Hash2(leaf1, leaf2);
    FieldElement node2 = Poseidon::Hash2(leaf3, leaf4);
    
    // Root
    FieldElement root = Poseidon::Hash2(node1, node2);
    
    EXPECT_FALSE(root.IsZero());
    
    // Changing any leaf should change the root
    FieldElement leaf1_modified(101);
    FieldElement node1_modified = Poseidon::Hash2(leaf1_modified, leaf2);
    FieldElement root_modified = Poseidon::Hash2(node1_modified, node2);
    
    EXPECT_NE(root, root_modified);
}

} // namespace test
} // namespace shurium
