// SHURIUM - ZK Proof Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include <shurium/identity/zkproof.h>
#include <shurium/identity/sigma.h>
#include <shurium/identity/rangeproof.h>
#include <shurium/identity/commitment.h>
#include <shurium/identity/nullifier.h>
#include <shurium/crypto/poseidon.h>
#include <shurium/core/random.h>

namespace shurium {
namespace identity {
namespace {

// ============================================================================
// Helper Functions
// ============================================================================

FieldElement RandomFieldElement() {
    std::array<Byte, 32> bytes;
    GetRandBytes(bytes.data(), bytes.size());
    return FieldElement::FromBytes(bytes.data(), 32);
}

// ============================================================================
// Groth16Proof Tests
// ============================================================================

TEST(Groth16ProofTest, DefaultConstructor) {
    Groth16Proof proof;
    // Should be all zeros
    bool allZero = true;
    for (const auto& b : proof.proofA) {
        if (b != 0) { allZero = false; break; }
    }
    EXPECT_TRUE(allZero);
}

TEST(Groth16ProofTest, Serialization) {
    Groth16Proof proof;
    // Fill with some data
    for (size_t i = 0; i < proof.proofA.size(); ++i) {
        proof.proofA[i] = static_cast<Byte>(i);
    }
    for (size_t i = 0; i < proof.proofB.size(); ++i) {
        proof.proofB[i] = static_cast<Byte>(i + 64);
    }
    for (size_t i = 0; i < proof.proofC.size(); ++i) {
        proof.proofC[i] = static_cast<Byte>(i + 192);
    }
    
    // Serialize
    auto bytes = proof.ToBytes();
    EXPECT_EQ(bytes.size(), 256u);
    
    // Deserialize
    auto restored = Groth16Proof::FromBytes(bytes.data(), bytes.size());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->proofA, proof.proofA);
    EXPECT_EQ(restored->proofB, proof.proofB);
    EXPECT_EQ(restored->proofC, proof.proofC);
}

TEST(Groth16ProofTest, IsWellFormed) {
    Groth16Proof emptyProof;
    EXPECT_FALSE(emptyProof.IsWellFormed());
    
    Groth16Proof validProof;
    validProof.proofA[0] = 1;
    EXPECT_TRUE(validProof.IsWellFormed());
}

TEST(Groth16ProofTest, HexConversion) {
    Groth16Proof proof;
    proof.proofA[0] = 0xAB;
    proof.proofA[1] = 0xCD;
    
    std::string hex = proof.ToHex();
    EXPECT_FALSE(hex.empty());
    
    auto restored = Groth16Proof::FromHex(hex);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->proofA[0], 0xAB);
    EXPECT_EQ(restored->proofA[1], 0xCD);
}

// ============================================================================
// VerificationKey Tests
// ============================================================================

TEST(VerificationKeyTest, Serialization) {
    VerificationKey key;
    key.circuitId = "test_circuit";
    key.system = ProofSystem::Groth16;
    key.numPublicInputs = 3;
    key.keyData = {0x01, 0x02, 0x03, 0x04};
    
    auto bytes = key.ToBytes();
    EXPECT_GT(bytes.size(), 0u);
    
    auto restored = VerificationKey::FromBytes(bytes.data(), bytes.size());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->circuitId, "test_circuit");
    EXPECT_EQ(restored->system, ProofSystem::Groth16);
    EXPECT_EQ(restored->numPublicInputs, 3u);
    EXPECT_EQ(restored->keyData, key.keyData);
}

TEST(VerificationKeyTest, IsValid) {
    VerificationKey key;
    EXPECT_FALSE(key.IsValid());
    
    key.circuitId = "test";
    EXPECT_FALSE(key.IsValid());
    
    key.numPublicInputs = 1;
    EXPECT_TRUE(key.IsValid());
}

// ============================================================================
// PublicInputs Tests
// ============================================================================

TEST(PublicInputsTest, DefaultConstructor) {
    PublicInputs inputs;
    EXPECT_TRUE(inputs.IsEmpty());
    EXPECT_EQ(inputs.Count(), 0u);
}

TEST(PublicInputsTest, AddAndCount) {
    PublicInputs inputs;
    inputs.Add(FieldElement::One());
    inputs.Add(FieldElement(42));
    
    EXPECT_EQ(inputs.Count(), 2u);
    EXPECT_FALSE(inputs.IsEmpty());
}

TEST(PublicInputsTest, Serialization) {
    PublicInputs inputs({
        FieldElement::One(),
        FieldElement(42),
        FieldElement(12345)
    });
    
    auto bytes = inputs.ToBytes();
    EXPECT_GT(bytes.size(), 0u);
    
    auto restored = PublicInputs::FromBytes(bytes.data(), bytes.size());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->Count(), 3u);
    EXPECT_EQ(restored->values[0], FieldElement::One());
    EXPECT_EQ(restored->values[1], FieldElement(42));
    EXPECT_EQ(restored->values[2], FieldElement(12345));
}

// ============================================================================
// ZKProof Tests
// ============================================================================

TEST(ZKProofTest, DefaultConstructor) {
    ZKProof proof;
    EXPECT_EQ(proof.GetType(), ProofType::Custom);
    EXPECT_EQ(proof.GetSystem(), ProofSystem::Placeholder);
    EXPECT_FALSE(proof.IsValid());
}

TEST(ZKProofTest, TypedConstructor) {
    ZKProof proof(ProofType::UBIClaim, ProofSystem::Groth16);
    EXPECT_EQ(proof.GetType(), ProofType::UBIClaim);
    EXPECT_EQ(proof.GetSystem(), ProofSystem::Groth16);
}

TEST(ZKProofTest, SetProofData) {
    ZKProof proof;
    std::vector<Byte> data(100, 0x42);
    proof.SetProofData(data);
    
    EXPECT_TRUE(proof.IsValid());
    EXPECT_EQ(proof.GetProofData(), data);
}

TEST(ZKProofTest, Groth16ProofAccess) {
    ZKProof proof(ProofType::Custom, ProofSystem::Groth16);
    
    Groth16Proof g16;
    g16.proofA[0] = 0x12;
    proof.SetGroth16Proof(g16);
    
    auto retrieved = proof.GetGroth16Proof();
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->proofA[0], 0x12);
}

TEST(ZKProofTest, Serialization) {
    ZKProof proof(ProofType::UBIClaim, ProofSystem::Placeholder);
    proof.SetPublicInputs(PublicInputs({FieldElement::One(), FieldElement(42)}));
    proof.SetProofData({0x01, 0x02, 0x03});
    
    auto bytes = proof.ToBytes();
    EXPECT_GT(bytes.size(), 0u);
    
    auto restored = ZKProof::FromBytes(bytes.data(), bytes.size());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->GetType(), ProofType::UBIClaim);
    EXPECT_EQ(restored->GetSystem(), ProofSystem::Placeholder);
    EXPECT_EQ(restored->GetPublicInputs().Count(), 2u);
}

// ============================================================================
// Schnorr Proof Tests
// ============================================================================

TEST(SchnorrProofTest, ProveAndVerify) {
    FieldElement secretKey = RandomFieldElement();
    FieldElement generator = GetGeneratorG();
    FieldElement publicKey = secretKey * generator;
    
    auto proof = SchnorrProver::Prove(secretKey, generator, publicKey);
    
    EXPECT_TRUE(proof.IsWellFormed());
    EXPECT_TRUE(SchnorrVerifier::Verify(proof, generator, publicKey));
}

TEST(SchnorrProofTest, ProveWithAutoPublicKey) {
    FieldElement secretKey = RandomFieldElement();
    FieldElement generator = GetGeneratorG();
    
    auto proof = SchnorrProver::Prove(secretKey, generator);
    
    // Compute expected public key
    FieldElement publicKey = secretKey * generator;
    
    EXPECT_TRUE(proof.IsWellFormed());
    EXPECT_TRUE(SchnorrVerifier::Verify(proof, generator, publicKey));
}

TEST(SchnorrProofTest, VerifyFailsWithWrongPublicKey) {
    FieldElement secretKey = RandomFieldElement();
    FieldElement generator = GetGeneratorG();
    FieldElement publicKey = secretKey * generator;
    FieldElement wrongKey = RandomFieldElement() * generator;
    
    auto proof = SchnorrProver::Prove(secretKey, generator, publicKey);
    
    EXPECT_FALSE(SchnorrVerifier::Verify(proof, generator, wrongKey));
}

TEST(SchnorrProofTest, VerifyFailsWithWrongGenerator) {
    FieldElement secretKey = RandomFieldElement();
    FieldElement generator = GetGeneratorG();
    FieldElement publicKey = secretKey * generator;
    FieldElement wrongGen = GetGeneratorH();
    
    auto proof = SchnorrProver::Prove(secretKey, generator, publicKey);
    
    EXPECT_FALSE(SchnorrVerifier::Verify(proof, wrongGen, publicKey));
}

TEST(SchnorrProofTest, WithContext) {
    FieldElement secretKey = RandomFieldElement();
    FieldElement generator = GetGeneratorG();
    FieldElement publicKey = secretKey * generator;
    
    std::vector<Byte> context = {0x01, 0x02, 0x03, 0x04};
    
    auto proof = SchnorrProver::Prove(secretKey, generator, publicKey, context);
    
    // Should verify with same context
    EXPECT_TRUE(SchnorrVerifier::Verify(proof, generator, publicKey, context));
    
    // Should fail with different context
    std::vector<Byte> wrongContext = {0x05, 0x06};
    EXPECT_FALSE(SchnorrVerifier::Verify(proof, generator, publicKey, wrongContext));
}

TEST(SchnorrProofTest, Serialization) {
    FieldElement secretKey = RandomFieldElement();
    FieldElement generator = GetGeneratorG();
    
    auto proof = SchnorrProver::Prove(secretKey, generator);
    
    auto bytes = proof.ToBytes();
    EXPECT_EQ(bytes.size(), 64u);
    
    auto restored = SchnorrProof::FromBytes(bytes.data(), bytes.size());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->commitment, proof.commitment);
    EXPECT_EQ(restored->response, proof.response);
}

// ============================================================================
// Pedersen Opening Proof Tests
// ============================================================================

TEST(PedersenOpeningProofTest, ProveAndVerify) {
    FieldElement value = RandomFieldElement();
    FieldElement randomness = RandomFieldElement();
    FieldElement g = GetGeneratorG();
    FieldElement h = GetGeneratorH();
    
    // Create commitment C = g^v * h^r
    FieldElement commitment = (value * g) + (randomness * h);
    
    auto proof = PedersenOpeningProver::Prove(value, randomness, g, h, commitment);
    
    EXPECT_TRUE(proof.IsWellFormed());
    EXPECT_TRUE(PedersenOpeningVerifier::Verify(proof, g, h, commitment));
}

TEST(PedersenOpeningProofTest, VerifyFailsWithWrongCommitment) {
    FieldElement value = RandomFieldElement();
    FieldElement randomness = RandomFieldElement();
    FieldElement g = GetGeneratorG();
    FieldElement h = GetGeneratorH();
    
    FieldElement commitment = (value * g) + (randomness * h);
    FieldElement wrongCommitment = commitment + g;
    
    auto proof = PedersenOpeningProver::Prove(value, randomness, g, h, commitment);
    
    EXPECT_FALSE(PedersenOpeningVerifier::Verify(proof, g, h, wrongCommitment));
}

TEST(PedersenOpeningProofTest, Serialization) {
    FieldElement value = RandomFieldElement();
    FieldElement randomness = RandomFieldElement();
    FieldElement g = GetGeneratorG();
    FieldElement h = GetGeneratorH();
    FieldElement commitment = (value * g) + (randomness * h);
    
    auto proof = PedersenOpeningProver::Prove(value, randomness, g, h, commitment);
    
    auto bytes = proof.ToBytes();
    EXPECT_EQ(bytes.size(), 96u);
    
    auto restored = PedersenOpeningProof::FromBytes(bytes.data(), bytes.size());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->commitment, proof.commitment);
    EXPECT_EQ(restored->responseValue, proof.responseValue);
    EXPECT_EQ(restored->responseRandomness, proof.responseRandomness);
}

// ============================================================================
// Equality Proof Tests
// ============================================================================

TEST(EqualityProofTest, ProveAndVerify) {
    FieldElement value = RandomFieldElement();
    FieldElement r1 = RandomFieldElement();
    FieldElement r2 = RandomFieldElement();
    
    FieldElement g = GetGeneratorG();
    FieldElement h1 = GetGeneratorH();
    FieldElement h2 = DeriveGenerator("H2");
    
    // Create two commitments to the same value
    FieldElement c1 = (value * g) + (r1 * h1);
    FieldElement c2 = (value * g) + (r2 * h2);
    
    auto proof = EqualityProver::Prove(value, r1, r2, g, h1, h2, c1, c2);
    
    EXPECT_TRUE(proof.IsWellFormed());
    EXPECT_TRUE(EqualityVerifier::Verify(proof, g, h1, h2, c1, c2));
}

TEST(EqualityProofTest, VerifyFailsWithDifferentValues) {
    FieldElement v1 = RandomFieldElement();
    FieldElement v2 = RandomFieldElement();
    FieldElement r1 = RandomFieldElement();
    FieldElement r2 = RandomFieldElement();
    
    FieldElement g = GetGeneratorG();
    FieldElement h1 = GetGeneratorH();
    FieldElement h2 = DeriveGenerator("H2");
    
    // Create commitments to different values
    FieldElement c1 = (v1 * g) + (r1 * h1);
    FieldElement c2 = (v2 * g) + (r2 * h2);
    
    // This proof is invalid because v1 != v2
    // The prover would need to cheat, which should fail verification
    // We test that a "wrong" proof doesn't verify
    auto proof = EqualityProver::Prove(v1, r1, r2, g, h1, h2, c1, c2);
    
    // This should fail because c2 doesn't commit to v1
    EXPECT_FALSE(EqualityVerifier::Verify(proof, g, h1, h2, c1, c2));
}

// ============================================================================
// OR Proof Tests
// ============================================================================

TEST(OrProofTest, ProveAndVerify) {
    // Create a 1-of-3 OR proof
    FieldElement secret = RandomFieldElement();
    std::vector<FieldElement> generators = GetGenerators(3);
    
    // Public values: only the second one is the real one
    std::vector<FieldElement> publicValues(3);
    publicValues[0] = RandomFieldElement() * generators[0];  // Random (not known)
    publicValues[1] = secret * generators[1];                 // Real (known)
    publicValues[2] = RandomFieldElement() * generators[2];  // Random (not known)
    
    auto proof = OrProver::Prove(1, secret, generators, publicValues);
    
    EXPECT_TRUE(proof.IsWellFormed());
    EXPECT_TRUE(OrVerifier::Verify(proof, generators, publicValues));
}

TEST(OrProofTest, VerifyFailsWithAllWrongValues) {
    FieldElement secret = RandomFieldElement();
    std::vector<FieldElement> generators = GetGenerators(3);
    
    // Create public values where none match the secret
    std::vector<FieldElement> publicValues(3);
    for (int i = 0; i < 3; i++) {
        publicValues[i] = RandomFieldElement() * generators[i];
    }
    
    // Claim index 1 is the real one (but it's not)
    auto proof = OrProver::Prove(1, secret, generators, publicValues);
    
    // Should fail verification
    EXPECT_FALSE(OrVerifier::Verify(proof, generators, publicValues));
}

// ============================================================================
// Range Proof Tests
// ============================================================================

TEST(RangeProofTest, SmallValue) {
    uint64_t value = 42;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = RangeProofProver::Prove(value, randomness, 8);
    ASSERT_TRUE(proof.has_value());
    
    EXPECT_TRUE(proof->IsWellFormed());
    EXPECT_EQ(proof->numBits, 8);
    
    FieldElement commitment = PedersenCommit(value, randomness);
    EXPECT_TRUE(RangeProofVerifier::Verify(*proof, commitment));
}

TEST(RangeProofTest, ZeroValue) {
    uint64_t value = 0;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = RangeProofProver::Prove(value, randomness, 8);
    ASSERT_TRUE(proof.has_value());
    
    FieldElement commitment = PedersenCommit(value, randomness);
    EXPECT_TRUE(RangeProofVerifier::Verify(*proof, commitment));
}

TEST(RangeProofTest, MaxValue) {
    uint64_t value = 255;  // Max for 8 bits
    FieldElement randomness = GenerateBlinding();
    
    auto proof = RangeProofProver::Prove(value, randomness, 8);
    ASSERT_TRUE(proof.has_value());
    
    FieldElement commitment = PedersenCommit(value, randomness);
    EXPECT_TRUE(RangeProofVerifier::Verify(*proof, commitment));
}

TEST(RangeProofTest, OutOfRange) {
    uint64_t value = 256;  // Out of range for 8 bits
    FieldElement randomness = GenerateBlinding();
    
    auto proof = RangeProofProver::Prove(value, randomness, 8);
    EXPECT_FALSE(proof.has_value());
}

TEST(RangeProofTest, LargerRange) {
    uint64_t value = 12345;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = RangeProofProver::Prove(value, randomness, 16);
    ASSERT_TRUE(proof.has_value());
    
    FieldElement commitment = PedersenCommit(value, randomness);
    EXPECT_TRUE(RangeProofVerifier::Verify(*proof, commitment));
}

TEST(RangeProofTest, Serialization) {
    uint64_t value = 100;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = RangeProofProver::Prove(value, randomness, 8);
    ASSERT_TRUE(proof.has_value());
    
    auto bytes = proof->ToBytes();
    EXPECT_GT(bytes.size(), 0u);
    
    auto restored = RangeProof::FromBytes(bytes.data(), bytes.size());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->numBits, proof->numBits);
    EXPECT_EQ(restored->A, proof->A);
    EXPECT_EQ(restored->S, proof->S);
}

TEST(RangeProofTest, VerifyFailsWithWrongCommitment) {
    uint64_t value = 42;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = RangeProofProver::Prove(value, randomness, 8);
    ASSERT_TRUE(proof.has_value());
    
    // Wrong commitment (different value)
    FieldElement wrongCommitment = PedersenCommit(43, randomness);
    EXPECT_FALSE(RangeProofVerifier::Verify(*proof, wrongCommitment));
}

// ============================================================================
// Simple Range Proof Tests
// ============================================================================

TEST(SimpleRangeProofTest, WithinRange) {
    uint64_t value = 50;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = SimpleRangeProofProver::Prove(value, randomness, 0, 100);
    ASSERT_TRUE(proof.has_value());
    
    EXPECT_TRUE(proof->IsWellFormed());
    EXPECT_EQ(proof->minValue, 0u);
    EXPECT_EQ(proof->maxValue, 100u);
}

TEST(SimpleRangeProofTest, AtMinimum) {
    uint64_t value = 10;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = SimpleRangeProofProver::Prove(value, randomness, 10, 100);
    ASSERT_TRUE(proof.has_value());
}

TEST(SimpleRangeProofTest, AtMaximum) {
    uint64_t value = 100;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = SimpleRangeProofProver::Prove(value, randomness, 0, 100);
    ASSERT_TRUE(proof.has_value());
}

TEST(SimpleRangeProofTest, BelowRange) {
    uint64_t value = 5;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = SimpleRangeProofProver::Prove(value, randomness, 10, 100);
    EXPECT_FALSE(proof.has_value());
}

TEST(SimpleRangeProofTest, AboveRange) {
    uint64_t value = 150;
    FieldElement randomness = GenerateBlinding();
    
    auto proof = SimpleRangeProofProver::Prove(value, randomness, 0, 100);
    EXPECT_FALSE(proof.has_value());
}

// ============================================================================
// Identity Proof Tests
// ============================================================================

TEST(IdentityProofTest, CreateUBIClaimProof) {
    // Setup identity
    FieldElement secretKey = RandomFieldElement();
    FieldElement nullifierKey = RandomFieldElement();
    FieldElement trapdoor = RandomFieldElement();
    
    // Create identity commitment
    auto identity = IdentityCommitment::Create(secretKey, nullifierKey, trapdoor);
    
    // Create a simple Merkle proof (single element tree)
    VectorCommitment tree({identity.ToFieldElement()});
    auto merkleProof = tree.Prove(0);
    ASSERT_TRUE(merkleProof.has_value());
    
    // Derive nullifier for epoch 100
    EpochId epoch = 100;
    Nullifier nullifier = Nullifier::Derive(nullifierKey, epoch);
    
    // Create proof
    auto proof = IdentityProof::CreateUBIClaimProof(
        tree.GetRoot(),
        nullifier,
        epoch,
        secretKey,
        nullifierKey,
        trapdoor,
        *merkleProof
    );
    
    EXPECT_TRUE(proof.IsValid());
    EXPECT_EQ(proof.GetEpoch(), epoch);
    EXPECT_EQ(proof.GetNullifier(), nullifier);
}

TEST(IdentityProofTest, VerifyProof) {
    // Setup
    FieldElement secretKey = RandomFieldElement();
    FieldElement nullifierKey = RandomFieldElement();
    FieldElement trapdoor = RandomFieldElement();
    
    auto identity = IdentityCommitment::Create(secretKey, nullifierKey, trapdoor);
    VectorCommitment tree({identity.ToFieldElement()});
    auto merkleProof = tree.Prove(0);
    ASSERT_TRUE(merkleProof.has_value());
    
    EpochId epoch = 100;
    Nullifier nullifier = Nullifier::Derive(nullifierKey, epoch);
    
    auto proof = IdentityProof::CreateUBIClaimProof(
        tree.GetRoot(),
        nullifier,
        epoch,
        secretKey,
        nullifierKey,
        trapdoor,
        *merkleProof
    );
    
    // Verify
    EXPECT_TRUE(proof.VerifyProof(tree.GetRoot()));
}

TEST(IdentityProofTest, VerifyWithNullifierSet) {
    // Setup
    FieldElement secretKey = RandomFieldElement();
    FieldElement nullifierKey = RandomFieldElement();
    FieldElement trapdoor = RandomFieldElement();
    
    auto identity = IdentityCommitment::Create(secretKey, nullifierKey, trapdoor);
    VectorCommitment tree({identity.ToFieldElement()});
    auto merkleProof = tree.Prove(0);
    ASSERT_TRUE(merkleProof.has_value());
    
    EpochId epoch = 100;
    Nullifier nullifier = Nullifier::Derive(nullifierKey, epoch);
    
    auto proof = IdentityProof::CreateUBIClaimProof(
        tree.GetRoot(),
        nullifier,
        epoch,
        secretKey,
        nullifierKey,
        trapdoor,
        *merkleProof
    );
    
    // Fresh nullifier set - should pass
    NullifierSet nullifierSet;
    nullifierSet.SetCurrentEpoch(epoch);
    EXPECT_TRUE(proof.Verify(tree.GetRoot(), nullifierSet));
    
    // Add nullifier to set - should now fail
    nullifierSet.Add(nullifier);
    EXPECT_FALSE(proof.Verify(tree.GetRoot(), nullifierSet));
}

TEST(IdentityProofTest, Serialization) {
    FieldElement secretKey = RandomFieldElement();
    FieldElement nullifierKey = RandomFieldElement();
    FieldElement trapdoor = RandomFieldElement();
    
    auto identity = IdentityCommitment::Create(secretKey, nullifierKey, trapdoor);
    VectorCommitment tree({identity.ToFieldElement()});
    auto merkleProof = tree.Prove(0);
    ASSERT_TRUE(merkleProof.has_value());
    
    EpochId epoch = 100;
    Nullifier nullifier = Nullifier::Derive(nullifierKey, epoch);
    
    auto proof = IdentityProof::CreateUBIClaimProof(
        tree.GetRoot(),
        nullifier,
        epoch,
        secretKey,
        nullifierKey,
        trapdoor,
        *merkleProof
    );
    
    // Serialize
    auto bytes = proof.ToBytes();
    EXPECT_GT(bytes.size(), 0u);
    
    // Deserialize
    auto restored = IdentityProof::FromBytes(bytes.data(), bytes.size());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->GetEpoch(), epoch);
    EXPECT_EQ(restored->GetNullifier(), nullifier);
}

// ============================================================================
// ProofVerifier Tests
// ============================================================================

TEST(ProofVerifierTest, RegisterAndRetrieveKey) {
    ProofVerifier& verifier = ProofVerifier::Instance();
    
    VerificationKey key;
    key.circuitId = "test_circuit_unique";
    key.system = ProofSystem::Placeholder;
    key.numPublicInputs = 2;
    
    verifier.RegisterKey("test_circuit_unique", key);
    
    EXPECT_TRUE(verifier.HasKey("test_circuit_unique"));
    EXPECT_FALSE(verifier.HasKey("nonexistent"));
    
    auto retrieved = verifier.GetKey("test_circuit_unique");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->numPublicInputs, 2u);
}

TEST(ProofVerifierTest, VerifyPlaceholderProof) {
    ProofVerifier& verifier = ProofVerifier::Instance();
    
    VerificationKey key;
    key.circuitId = "placeholder_circuit";
    key.system = ProofSystem::Placeholder;
    key.numPublicInputs = 1;
    
    verifier.RegisterKey("placeholder_circuit", key);
    
    ZKProof proof(ProofType::Custom, ProofSystem::Placeholder);
    proof.SetPublicInputs(PublicInputs({FieldElement::One()}));
    proof.SetProofData({0x01, 0x02, 0x03});
    
    EXPECT_TRUE(verifier.Verify(proof, "placeholder_circuit"));
}

// ============================================================================
// ProofGenerator Tests
// ============================================================================

TEST(ProofGeneratorTest, GeneratePlaceholderProof) {
    ProofGenerator& generator = ProofGenerator::Instance();
    
    PublicInputs inputs({FieldElement(42), FieldElement(123)});
    
    auto proof = generator.GeneratePlaceholderProof(ProofType::Custom, inputs);
    
    EXPECT_TRUE(proof.IsValid());
    EXPECT_EQ(proof.GetType(), ProofType::Custom);
    EXPECT_EQ(proof.GetSystem(), ProofSystem::Placeholder);
    EXPECT_EQ(proof.GetPublicInputs().Count(), 2u);
}

TEST(ProofGeneratorTest, GenerateUBIClaimProof) {
    ProofGenerator& generator = ProofGenerator::Instance();
    
    FieldElement secretKey = RandomFieldElement();
    FieldElement nullifierKey = RandomFieldElement();
    FieldElement trapdoor = RandomFieldElement();
    
    auto identity = IdentityCommitment::Create(secretKey, nullifierKey, trapdoor);
    VectorCommitment tree({identity.ToFieldElement()});
    auto merkleProof = tree.Prove(0);
    ASSERT_TRUE(merkleProof.has_value());
    
    EpochId epoch = 100;
    
    auto proof = generator.GenerateUBIClaimProof(
        secretKey,
        nullifierKey,
        trapdoor,
        tree.GetRoot(),
        *merkleProof,
        epoch
    );
    
    ASSERT_TRUE(proof.has_value());
    EXPECT_TRUE(proof->IsValid());
    EXPECT_EQ(proof->GetEpoch(), epoch);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST(UtilityTest, ProofTypeStrings) {
    EXPECT_EQ(ProofTypeToString(ProofType::Registration), "Registration");
    EXPECT_EQ(ProofTypeToString(ProofType::UBIClaim), "UBIClaim");
    EXPECT_EQ(ProofTypeToString(ProofType::Update), "Update");
    EXPECT_EQ(ProofTypeToString(ProofType::Membership), "Membership");
    EXPECT_EQ(ProofTypeToString(ProofType::Range), "Range");
    EXPECT_EQ(ProofTypeToString(ProofType::Custom), "Custom");
    
    EXPECT_EQ(ProofTypeFromString("Registration"), ProofType::Registration);
    EXPECT_EQ(ProofTypeFromString("UBIClaim"), ProofType::UBIClaim);
    EXPECT_FALSE(ProofTypeFromString("invalid").has_value());
}

TEST(UtilityTest, ProofSystemStrings) {
    EXPECT_EQ(ProofSystemToString(ProofSystem::Groth16), "Groth16");
    EXPECT_EQ(ProofSystemToString(ProofSystem::PLONK), "PLONK");
    EXPECT_EQ(ProofSystemToString(ProofSystem::Bulletproofs), "Bulletproofs");
    EXPECT_EQ(ProofSystemToString(ProofSystem::STARK), "STARK");
    EXPECT_EQ(ProofSystemToString(ProofSystem::Placeholder), "Placeholder");
    
    EXPECT_EQ(ProofSystemFromString("Groth16"), ProofSystem::Groth16);
    EXPECT_EQ(ProofSystemFromString("Placeholder"), ProofSystem::Placeholder);
    EXPECT_FALSE(ProofSystemFromString("invalid").has_value());
}

TEST(UtilityTest, Generators) {
    FieldElement g = GetGeneratorG();
    FieldElement h = GetGeneratorH();
    
    // Generators should be different
    EXPECT_NE(g, h);
    
    // Generators should be non-zero
    EXPECT_FALSE(g.IsZero());
    EXPECT_FALSE(h.IsZero());
    
    // Generators should be deterministic
    EXPECT_EQ(g, GetGeneratorG());
    EXPECT_EQ(h, GetGeneratorH());
}

TEST(UtilityTest, GetGenerators) {
    auto gens = GetGenerators(10);
    
    EXPECT_EQ(gens.size(), 10u);
    
    // All generators should be unique
    for (size_t i = 0; i < gens.size(); ++i) {
        for (size_t j = i + 1; j < gens.size(); ++j) {
            EXPECT_NE(gens[i], gens[j]);
        }
    }
}

TEST(UtilityTest, DeriveGenerator) {
    auto g1 = DeriveGenerator("seed1");
    auto g2 = DeriveGenerator("seed2");
    auto g1_again = DeriveGenerator("seed1");
    
    EXPECT_NE(g1, g2);
    EXPECT_EQ(g1, g1_again);
}

TEST(UtilityTest, PedersenCommit) {
    uint64_t value = 42;
    FieldElement r = GenerateBlinding();
    
    FieldElement c1 = PedersenCommit(value, r);
    FieldElement c2 = PedersenCommit(FieldElement(value), r);
    
    // Both methods should produce same commitment
    EXPECT_EQ(c1, c2);
    
    // Different values should produce different commitments
    FieldElement c3 = PedersenCommit(43, r);
    EXPECT_NE(c1, c3);
}

TEST(UtilityTest, InnerProduct) {
    std::vector<FieldElement> a = {FieldElement(1), FieldElement(2), FieldElement(3)};
    std::vector<FieldElement> b = {FieldElement(4), FieldElement(5), FieldElement(6)};
    
    // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    FieldElement result = InnerProduct(a, b);
    EXPECT_EQ(result, FieldElement(32));
}

TEST(UtilityTest, HadamardProduct) {
    std::vector<FieldElement> a = {FieldElement(1), FieldElement(2), FieldElement(3)};
    std::vector<FieldElement> b = {FieldElement(4), FieldElement(5), FieldElement(6)};
    
    auto result = HadamardProduct(a, b);
    
    EXPECT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], FieldElement(4));   // 1*4
    EXPECT_EQ(result[1], FieldElement(10));  // 2*5
    EXPECT_EQ(result[2], FieldElement(18));  // 3*6
}

} // namespace
} // namespace identity
} // namespace shurium
