// SHURIUM - Merkle Tree Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/core/merkle.h"
#include "shurium/core/types.h"
#include "shurium/crypto/sha256.h"
#include <vector>

using namespace shurium;

// ============================================================================
// Helper Functions
// ============================================================================

// Create a Hash256 from a simple integer (for testing)
Hash256 MakeHash(uint64_t n) {
    Hash256 hash;
    hash.SetNull();
    std::memcpy(hash.data(), &n, sizeof(n));
    return hash;
}

// ============================================================================
// Basic Merkle Root Tests
// ============================================================================

TEST(MerkleTest, EmptyVector) {
    std::vector<Hash256> leaves;
    Hash256 root = ComputeMerkleRoot(leaves);
    
    // Empty tree should return null hash
    EXPECT_TRUE(root.IsNull());
}

TEST(MerkleTest, SingleLeaf) {
    std::vector<Hash256> leaves = { MakeHash(1) };
    Hash256 root = ComputeMerkleRoot(leaves);
    
    // Single leaf is its own root
    EXPECT_EQ(root, MakeHash(1));
}

TEST(MerkleTest, TwoLeaves) {
    Hash256 h1 = MakeHash(1);
    Hash256 h2 = MakeHash(2);
    std::vector<Hash256> leaves = { h1, h2 };
    
    Hash256 root = ComputeMerkleRoot(leaves);
    
    // Root should be DoubleSHA256(h1 || h2)
    std::vector<uint8_t> combined(64);
    std::memcpy(combined.data(), h1.data(), 32);
    std::memcpy(combined.data() + 32, h2.data(), 32);
    Hash256 expected = DoubleSHA256(combined.data(), combined.size());
    
    EXPECT_EQ(root, expected);
}

TEST(MerkleTest, ThreeLeavesOddDuplication) {
    // With 3 leaves, the last one gets duplicated
    // Tree structure:
    //        root
    //       /    \
    //    H(1,2)  H(3,3)
    //    /  \    /  \
    //   1    2  3    3
    
    Hash256 h1 = MakeHash(1);
    Hash256 h2 = MakeHash(2);
    Hash256 h3 = MakeHash(3);
    std::vector<Hash256> leaves = { h1, h2, h3 };
    
    Hash256 root = ComputeMerkleRoot(leaves);
    
    // Manual calculation
    std::vector<uint8_t> combined(64);
    
    // H(1,2)
    std::memcpy(combined.data(), h1.data(), 32);
    std::memcpy(combined.data() + 32, h2.data(), 32);
    Hash256 h12 = DoubleSHA256(combined.data(), combined.size());
    
    // H(3,3)
    std::memcpy(combined.data(), h3.data(), 32);
    std::memcpy(combined.data() + 32, h3.data(), 32);
    Hash256 h33 = DoubleSHA256(combined.data(), combined.size());
    
    // root = H(H(1,2), H(3,3))
    std::memcpy(combined.data(), h12.data(), 32);
    std::memcpy(combined.data() + 32, h33.data(), 32);
    Hash256 expected = DoubleSHA256(combined.data(), combined.size());
    
    EXPECT_EQ(root, expected);
}

TEST(MerkleTest, FourLeaves) {
    Hash256 h1 = MakeHash(1);
    Hash256 h2 = MakeHash(2);
    Hash256 h3 = MakeHash(3);
    Hash256 h4 = MakeHash(4);
    std::vector<Hash256> leaves = { h1, h2, h3, h4 };
    
    Hash256 root = ComputeMerkleRoot(leaves);
    
    // Tree structure:
    //          root
    //        /      \
    //    H(1,2)    H(3,4)
    //    /  \      /  \
    //   1    2    3    4
    
    std::vector<uint8_t> combined(64);
    
    std::memcpy(combined.data(), h1.data(), 32);
    std::memcpy(combined.data() + 32, h2.data(), 32);
    Hash256 h12 = DoubleSHA256(combined.data(), combined.size());
    
    std::memcpy(combined.data(), h3.data(), 32);
    std::memcpy(combined.data() + 32, h4.data(), 32);
    Hash256 h34 = DoubleSHA256(combined.data(), combined.size());
    
    std::memcpy(combined.data(), h12.data(), 32);
    std::memcpy(combined.data() + 32, h34.data(), 32);
    Hash256 expected = DoubleSHA256(combined.data(), combined.size());
    
    EXPECT_EQ(root, expected);
}

// ============================================================================
// Mutation Detection Tests (CVE-2012-2459 defense)
// ============================================================================

TEST(MerkleTest, DetectDuplicateMutation) {
    // When last two leaves are identical, mutation should be detected
    Hash256 h1 = MakeHash(1);
    Hash256 h2 = MakeHash(2);
    Hash256 h3 = MakeHash(3);
    
    std::vector<Hash256> leaves1 = { h1, h2, h3 };
    std::vector<Hash256> leaves2 = { h1, h2, h3, h3 };
    
    bool mutated1 = false, mutated2 = false;
    Hash256 root1 = ComputeMerkleRoot(leaves1, &mutated1);
    Hash256 root2 = ComputeMerkleRoot(leaves2, &mutated2);
    
    // Both should give same root (that's the vulnerability)
    EXPECT_EQ(root1, root2);
    
    // But mutation should be detected in leaves2
    EXPECT_FALSE(mutated1);
    EXPECT_TRUE(mutated2);
}

TEST(MerkleTest, NoMutationWithDifferentLeaves) {
    Hash256 h1 = MakeHash(1);
    Hash256 h2 = MakeHash(2);
    Hash256 h3 = MakeHash(3);
    Hash256 h4 = MakeHash(4);
    
    std::vector<Hash256> leaves = { h1, h2, h3, h4 };
    bool mutated = false;
    ComputeMerkleRoot(leaves, &mutated);
    
    EXPECT_FALSE(mutated);
}

// ============================================================================
// Merkle Proof Tests
// ============================================================================

TEST(MerkleProofTest, SingleLeafProof) {
    Hash256 h1 = MakeHash(1);
    std::vector<Hash256> leaves = { h1 };
    
    std::vector<Hash256> proof = ComputeMerklePath(leaves, 0);
    
    // Single leaf needs no proof
    EXPECT_TRUE(proof.empty());
}

TEST(MerkleProofTest, TwoLeavesProof) {
    Hash256 h1 = MakeHash(1);
    Hash256 h2 = MakeHash(2);
    std::vector<Hash256> leaves = { h1, h2 };
    
    // Proof for first leaf
    std::vector<Hash256> proof0 = ComputeMerklePath(leaves, 0);
    EXPECT_EQ(proof0.size(), 1);
    EXPECT_EQ(proof0[0], h2);  // Sibling
    
    // Proof for second leaf
    std::vector<Hash256> proof1 = ComputeMerklePath(leaves, 1);
    EXPECT_EQ(proof1.size(), 1);
    EXPECT_EQ(proof1[0], h1);  // Sibling
}

TEST(MerkleProofTest, FourLeavesProof) {
    Hash256 h1 = MakeHash(1);
    Hash256 h2 = MakeHash(2);
    Hash256 h3 = MakeHash(3);
    Hash256 h4 = MakeHash(4);
    std::vector<Hash256> leaves = { h1, h2, h3, h4 };
    
    // Compute intermediate hashes
    std::vector<uint8_t> combined(64);
    std::memcpy(combined.data(), h1.data(), 32);
    std::memcpy(combined.data() + 32, h2.data(), 32);
    Hash256 h12 = DoubleSHA256(combined.data(), combined.size());
    
    std::memcpy(combined.data(), h3.data(), 32);
    std::memcpy(combined.data() + 32, h4.data(), 32);
    Hash256 h34 = DoubleSHA256(combined.data(), combined.size());
    
    // Proof for leaf 0 (h1)
    std::vector<Hash256> proof0 = ComputeMerklePath(leaves, 0);
    EXPECT_EQ(proof0.size(), 2);
    EXPECT_EQ(proof0[0], h2);   // Direct sibling
    EXPECT_EQ(proof0[1], h34);  // Uncle
    
    // Proof for leaf 2 (h3)
    std::vector<Hash256> proof2 = ComputeMerklePath(leaves, 2);
    EXPECT_EQ(proof2.size(), 2);
    EXPECT_EQ(proof2[0], h4);   // Direct sibling
    EXPECT_EQ(proof2[1], h12);  // Uncle
}

TEST(MerkleProofTest, VerifyProof) {
    Hash256 h1 = MakeHash(1);
    Hash256 h2 = MakeHash(2);
    Hash256 h3 = MakeHash(3);
    Hash256 h4 = MakeHash(4);
    std::vector<Hash256> leaves = { h1, h2, h3, h4 };
    
    Hash256 root = ComputeMerkleRoot(leaves);
    
    // Get proof for leaf 1 (h2)
    std::vector<Hash256> proof = ComputeMerklePath(leaves, 1);
    
    // Verify the proof
    bool valid = VerifyMerkleProof(h2, 1, root, proof);
    EXPECT_TRUE(valid);
    
    // Verify fails with wrong leaf
    Hash256 wrongLeaf = MakeHash(999);
    EXPECT_FALSE(VerifyMerkleProof(wrongLeaf, 1, root, proof));
    
    // Verify fails with wrong position
    EXPECT_FALSE(VerifyMerkleProof(h2, 0, root, proof));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(MerkleTest, LargeTree) {
    // Test with 1000 leaves
    std::vector<Hash256> leaves;
    for (int i = 0; i < 1000; ++i) {
        leaves.push_back(MakeHash(i));
    }
    
    Hash256 root = ComputeMerkleRoot(leaves);
    EXPECT_FALSE(root.IsNull());
    
    // Verify proof for a random leaf
    std::vector<Hash256> proof = ComputeMerklePath(leaves, 500);
    EXPECT_TRUE(VerifyMerkleProof(MakeHash(500), 500, root, proof));
}

TEST(MerkleTest, PowerOfTwoLeaves) {
    // Powers of 2 are a special case (no duplication needed)
    for (int n = 1; n <= 64; n *= 2) {
        std::vector<Hash256> leaves;
        for (int i = 0; i < n; ++i) {
            leaves.push_back(MakeHash(i + 1));  // Use i+1 to avoid MakeHash(0) being null
        }
        
        bool mutated = false;
        Hash256 root = ComputeMerkleRoot(leaves, &mutated);
        EXPECT_FALSE(root.IsNull());
        EXPECT_FALSE(mutated);
    }
}

TEST(MerkleTest, DeterministicRoot) {
    // Same leaves should always produce same root
    std::vector<Hash256> leaves1 = { MakeHash(1), MakeHash(2), MakeHash(3) };
    std::vector<Hash256> leaves2 = { MakeHash(1), MakeHash(2), MakeHash(3) };
    
    Hash256 root1 = ComputeMerkleRoot(leaves1);
    Hash256 root2 = ComputeMerkleRoot(leaves2);
    
    EXPECT_EQ(root1, root2);
}

TEST(MerkleTest, OrderMatters) {
    // Different order should produce different root
    std::vector<Hash256> leaves1 = { MakeHash(1), MakeHash(2), MakeHash(3) };
    std::vector<Hash256> leaves2 = { MakeHash(3), MakeHash(2), MakeHash(1) };
    
    Hash256 root1 = ComputeMerkleRoot(leaves1);
    Hash256 root2 = ComputeMerkleRoot(leaves2);
    
    EXPECT_NE(root1, root2);
}
