// SHURIUM - Identity Module Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include <shurium/identity/identity.h>
#include <shurium/identity/commitment.h>
#include <shurium/identity/nullifier.h>
#include <shurium/identity/zkproof.h>

#include <algorithm>
#include <set>

using namespace shurium;
using namespace shurium::identity;

// ============================================================================
// Commitment Tests
// ============================================================================

class CommitmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test field elements
        value1_ = GenerateRandomFieldElement();
        value2_ = GenerateRandomFieldElement();
        randomness_ = GenerateRandomFieldElement();
    }
    
    FieldElement value1_;
    FieldElement value2_;
    FieldElement randomness_;
};

TEST_F(CommitmentTest, PedersenCommitmentBasic) {
    auto commitment = PedersenCommitment::Commit(value1_, randomness_);
    
    EXPECT_FALSE(commitment.IsEmpty());
    EXPECT_EQ(commitment.size(), PedersenCommitment::SIZE);
}

TEST_F(CommitmentTest, PedersenCommitmentVerify) {
    auto commitment = PedersenCommitment::Commit(value1_, randomness_);
    
    EXPECT_TRUE(commitment.Verify(value1_, randomness_));
    EXPECT_FALSE(commitment.Verify(value2_, randomness_));
    EXPECT_FALSE(commitment.Verify(value1_, value2_));
}

TEST_F(CommitmentTest, PedersenCommitmentWithRandomness) {
    FieldElement generatedRandom;
    auto commitment = PedersenCommitment::CommitWithRandomness(value1_, generatedRandom);
    
    EXPECT_FALSE(commitment.IsEmpty());
    EXPECT_FALSE(generatedRandom.IsZero());
    EXPECT_TRUE(commitment.Verify(value1_, generatedRandom));
}

TEST_F(CommitmentTest, PedersenCommitmentDeterministic) {
    auto c1 = PedersenCommitment::Commit(value1_, randomness_);
    auto c2 = PedersenCommitment::Commit(value1_, randomness_);
    
    EXPECT_EQ(c1, c2);
}

TEST_F(CommitmentTest, PedersenCommitmentDifferentInputs) {
    auto c1 = PedersenCommitment::Commit(value1_, randomness_);
    auto c2 = PedersenCommitment::Commit(value2_, randomness_);
    
    EXPECT_NE(c1, c2);
}

TEST_F(CommitmentTest, PedersenCommitmentHexRoundtrip) {
    auto commitment = PedersenCommitment::Commit(value1_, randomness_);
    std::string hex = commitment.ToHex();
    
    auto parsed = PedersenCommitment::FromHex(hex);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(commitment, *parsed);
}

TEST_F(CommitmentTest, CommitmentOpeningSerialization) {
    CommitmentOpening opening;
    opening.value = value1_;
    opening.randomness = randomness_;
    opening.auxData.push_back(value2_);
    
    auto bytes = opening.ToBytes();
    auto parsed = CommitmentOpening::FromBytes(bytes.data(), bytes.size());
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(opening.value, parsed->value);
    EXPECT_EQ(opening.randomness, parsed->randomness);
    EXPECT_EQ(opening.auxData.size(), parsed->auxData.size());
}

// ============================================================================
// Identity Commitment Tests
// ============================================================================

TEST_F(CommitmentTest, IdentityCommitmentCreation) {
    FieldElement secretKey = GenerateRandomFieldElement();
    FieldElement nullifierKey = GenerateRandomFieldElement();
    FieldElement trapdoor = GenerateRandomFieldElement();
    
    auto commitment = IdentityCommitment::Create(secretKey, nullifierKey, trapdoor);
    
    EXPECT_FALSE(commitment.IsEmpty());
    EXPECT_TRUE(commitment.Verify(secretKey, nullifierKey, trapdoor));
}

TEST_F(CommitmentTest, IdentityCommitmentGenerate) {
    FieldElement secretKey, nullifierKey, trapdoor;
    auto commitment = IdentityCommitment::Generate(secretKey, nullifierKey, trapdoor);
    
    EXPECT_FALSE(commitment.IsEmpty());
    EXPECT_FALSE(secretKey.IsZero());
    EXPECT_FALSE(nullifierKey.IsZero());
    EXPECT_FALSE(trapdoor.IsZero());
    EXPECT_TRUE(commitment.Verify(secretKey, nullifierKey, trapdoor));
}

TEST_F(CommitmentTest, IdentityCommitmentUnique) {
    FieldElement sk1, nk1, td1;
    auto c1 = IdentityCommitment::Generate(sk1, nk1, td1);
    
    FieldElement sk2, nk2, td2;
    auto c2 = IdentityCommitment::Generate(sk2, nk2, td2);
    
    EXPECT_NE(c1, c2);
}

// ============================================================================
// Vector Commitment (Merkle Tree) Tests
// ============================================================================

class VectorCommitmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        for (int i = 0; i < 8; ++i) {
            elements_.push_back(GenerateRandomFieldElement());
        }
    }
    
    std::vector<FieldElement> elements_;
};

TEST_F(VectorCommitmentTest, EmptyTree) {
    VectorCommitment tree;
    
    EXPECT_TRUE(tree.IsEmpty());
    EXPECT_EQ(tree.Size(), 0);
    EXPECT_TRUE(tree.GetRoot().IsZero());
}

TEST_F(VectorCommitmentTest, AddElement) {
    VectorCommitment tree;
    uint64_t idx = tree.Add(elements_[0]);
    
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(tree.Size(), 1);
    EXPECT_FALSE(tree.GetRoot().IsZero());
}

TEST_F(VectorCommitmentTest, AddMultipleElements) {
    VectorCommitment tree;
    
    for (size_t i = 0; i < elements_.size(); ++i) {
        uint64_t idx = tree.Add(elements_[i]);
        EXPECT_EQ(idx, i);
    }
    
    EXPECT_EQ(tree.Size(), elements_.size());
}

TEST_F(VectorCommitmentTest, MembershipProof) {
    VectorCommitment tree(elements_);
    
    for (size_t i = 0; i < elements_.size(); ++i) {
        auto proof = tree.Prove(i);
        ASSERT_TRUE(proof.has_value());
        EXPECT_TRUE(tree.Verify(elements_[i], *proof));
    }
}

TEST_F(VectorCommitmentTest, StaticProofVerification) {
    VectorCommitment tree(elements_);
    FieldElement root = tree.GetRoot();
    
    auto proof = tree.Prove(0);
    ASSERT_TRUE(proof.has_value());
    
    EXPECT_TRUE(VectorCommitment::VerifyProof(root, elements_[0], *proof));
    EXPECT_FALSE(VectorCommitment::VerifyProof(root, elements_[1], *proof));
}

TEST_F(VectorCommitmentTest, ProofSerialization) {
    VectorCommitment tree(elements_);
    
    auto proof = tree.Prove(3);
    ASSERT_TRUE(proof.has_value());
    
    auto bytes = proof->ToBytes();
    auto parsed = VectorCommitment::MerkleProof::FromBytes(bytes.data(), bytes.size());
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(proof->index, parsed->index);
    EXPECT_EQ(proof->siblings.size(), parsed->siblings.size());
}

// ============================================================================
// Nullifier Tests
// ============================================================================

class NullifierTest : public ::testing::Test {
protected:
    void SetUp() override {
        nullifierKey_ = GenerateRandomFieldElement();
        epoch_ = 100;
    }
    
    FieldElement nullifierKey_;
    EpochId epoch_;
};

TEST_F(NullifierTest, DeriveNullifier) {
    auto nullifier = Nullifier::Derive(nullifierKey_, epoch_);
    
    EXPECT_FALSE(nullifier.IsEmpty());
    EXPECT_EQ(nullifier.GetEpoch(), epoch_);
}

TEST_F(NullifierTest, NullifierDeterministic) {
    auto n1 = Nullifier::Derive(nullifierKey_, epoch_);
    auto n2 = Nullifier::Derive(nullifierKey_, epoch_);
    
    EXPECT_EQ(n1, n2);
}

TEST_F(NullifierTest, NullifierDifferentEpochs) {
    auto n1 = Nullifier::Derive(nullifierKey_, epoch_);
    auto n2 = Nullifier::Derive(nullifierKey_, epoch_ + 1);
    
    EXPECT_NE(n1.GetHash(), n2.GetHash());
}

TEST_F(NullifierTest, NullifierDifferentKeys) {
    auto key2 = GenerateRandomFieldElement();
    
    auto n1 = Nullifier::Derive(nullifierKey_, epoch_);
    auto n2 = Nullifier::Derive(key2, epoch_);
    
    EXPECT_NE(n1.GetHash(), n2.GetHash());
}

TEST_F(NullifierTest, NullifierDifferentDomains) {
    auto n1 = Nullifier::Derive(nullifierKey_, epoch_, Nullifier::DOMAIN_UBI);
    auto n2 = Nullifier::Derive(nullifierKey_, epoch_, Nullifier::DOMAIN_VOTE);
    
    EXPECT_NE(n1.GetHash(), n2.GetHash());
}

TEST_F(NullifierTest, NullifierHexRoundtrip) {
    auto nullifier = Nullifier::Derive(nullifierKey_, epoch_);
    std::string hex = nullifier.ToHex();
    
    auto parsed = Nullifier::FromHex(hex, epoch_);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(nullifier.GetHash(), parsed->GetHash());
}

// ============================================================================
// NullifierSet Tests
// ============================================================================

class NullifierSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        set_ = std::make_unique<NullifierSet>();
        set_->SetCurrentEpoch(100);
        
        nullifierKey_ = GenerateRandomFieldElement();
    }
    
    std::unique_ptr<NullifierSet> set_;
    FieldElement nullifierKey_;
};

TEST_F(NullifierSetTest, AddNullifier) {
    auto nullifier = Nullifier::Derive(nullifierKey_, 100);
    
    auto result = set_->Add(nullifier);
    EXPECT_EQ(result, NullifierSet::AddResult::Success);
    EXPECT_TRUE(set_->Contains(nullifier));
}

TEST_F(NullifierSetTest, DoubleAddReturnsAlreadyExists) {
    auto nullifier = Nullifier::Derive(nullifierKey_, 100);
    
    auto r1 = set_->Add(nullifier);
    auto r2 = set_->Add(nullifier);
    
    EXPECT_EQ(r1, NullifierSet::AddResult::Success);
    EXPECT_EQ(r2, NullifierSet::AddResult::AlreadyExists);
}

TEST_F(NullifierSetTest, CountForEpoch) {
    for (int i = 0; i < 10; ++i) {
        auto key = GenerateRandomFieldElement();
        auto nullifier = Nullifier::Derive(key, 100);
        set_->Add(nullifier);
    }
    
    EXPECT_EQ(set_->CountForEpoch(100), 10);
    EXPECT_EQ(set_->CountForEpoch(101), 0);
}

TEST_F(NullifierSetTest, TotalCount) {
    for (int i = 0; i < 5; ++i) {
        auto key = GenerateRandomFieldElement();
        set_->Add(Nullifier::Derive(key, 100));
    }
    for (int i = 0; i < 3; ++i) {
        auto key = GenerateRandomFieldElement();
        set_->Add(Nullifier::Derive(key, 99));
    }
    
    EXPECT_EQ(set_->TotalCount(), 8);
}

TEST_F(NullifierSetTest, BatchAdd) {
    std::vector<Nullifier> nullifiers;
    for (int i = 0; i < 5; ++i) {
        auto key = GenerateRandomFieldElement();
        nullifiers.push_back(Nullifier::Derive(key, 100));
    }
    
    EXPECT_TRUE(set_->AddBatch(nullifiers));
    EXPECT_EQ(set_->CountForEpoch(100), 5);
}

TEST_F(NullifierSetTest, BatchAddWithDuplicateFails) {
    auto nullifier = Nullifier::Derive(nullifierKey_, 100);
    set_->Add(nullifier);
    
    std::vector<Nullifier> nullifiers;
    nullifiers.push_back(Nullifier::Derive(GenerateRandomFieldElement(), 100));
    nullifiers.push_back(nullifier);  // Duplicate
    
    EXPECT_FALSE(set_->AddBatch(nullifiers));
    EXPECT_EQ(set_->CountForEpoch(100), 1);  // Original only
}

TEST_F(NullifierSetTest, Prune) {
    set_->SetCurrentEpoch(110);
    
    // Add nullifiers from epochs 100-109
    for (EpochId e = 100; e < 110; ++e) {
        auto key = GenerateRandomFieldElement();
        set_->Add(Nullifier::Derive(key, e));
    }
    
    EXPECT_EQ(set_->TotalCount(), 10);
    
    // Prune keeping only last 5 epochs
    uint64_t pruned = set_->Prune(5);
    
    EXPECT_EQ(pruned, 5);
    EXPECT_EQ(set_->TotalCount(), 5);
}

TEST_F(NullifierSetTest, Serialization) {
    for (int i = 0; i < 5; ++i) {
        auto key = GenerateRandomFieldElement();
        set_->Add(Nullifier::Derive(key, 100));
    }
    
    auto bytes = set_->Serialize();
    NullifierSet::Config config;
    auto deserialized = NullifierSet::Deserialize(bytes.data(), bytes.size(), config);
    
    ASSERT_NE(deserialized, nullptr);
    EXPECT_EQ(deserialized->TotalCount(), 5);
    EXPECT_EQ(deserialized->GetCurrentEpoch(), 100);
}

// ============================================================================
// Epoch Utility Tests
// ============================================================================

TEST(EpochUtilTest, CalculateEpoch) {
    int64_t epochDuration = 604800;  // 1 week
    int64_t genesisTime = 1000000;
    
    EXPECT_EQ(CalculateEpoch(1000000, epochDuration, genesisTime), 0);
    EXPECT_EQ(CalculateEpoch(1604799, epochDuration, genesisTime), 0);
    EXPECT_EQ(CalculateEpoch(1604800, epochDuration, genesisTime), 1);
    EXPECT_EQ(CalculateEpoch(2209599, epochDuration, genesisTime), 1);
    EXPECT_EQ(CalculateEpoch(2209600, epochDuration, genesisTime), 2);
}

TEST(EpochUtilTest, GetEpochBoundaries) {
    int64_t epochDuration = 604800;
    int64_t genesisTime = 1000000;
    
    EXPECT_EQ(GetEpochStartTime(0, epochDuration, genesisTime), 1000000);
    EXPECT_EQ(GetEpochEndTime(0, epochDuration, genesisTime), 1604799);
    EXPECT_EQ(GetEpochStartTime(1, epochDuration, genesisTime), 1604800);
}

TEST(EpochUtilTest, IsInEpoch) {
    int64_t epochDuration = 604800;
    int64_t genesisTime = 1000000;
    
    EXPECT_TRUE(IsInEpoch(1000000, 0, epochDuration, genesisTime));
    EXPECT_TRUE(IsInEpoch(1604799, 0, epochDuration, genesisTime));
    EXPECT_FALSE(IsInEpoch(1604800, 0, epochDuration, genesisTime));
    EXPECT_TRUE(IsInEpoch(1604800, 1, epochDuration, genesisTime));
}

// ============================================================================
// ZK Proof Tests
// ============================================================================

class ZKProofTest : public ::testing::Test {
protected:
    void SetUp() override {
        secrets_ = IdentitySecrets::Generate();
    }
    
    IdentitySecrets secrets_;
};

TEST_F(ZKProofTest, PublicInputsSerialization) {
    PublicInputs inputs;
    inputs.Add(GenerateRandomFieldElement());
    inputs.Add(GenerateRandomFieldElement());
    inputs.Add(GenerateRandomFieldElement());
    
    auto bytes = inputs.ToBytes();
    auto parsed = PublicInputs::FromBytes(bytes.data(), bytes.size());
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(inputs.Count(), parsed->Count());
}

TEST_F(ZKProofTest, Groth16ProofWellFormed) {
    Groth16Proof proof;
    
    // Empty proof is not well-formed
    EXPECT_FALSE(proof.IsWellFormed());
    
    // Set some non-zero bytes
    proof.proofA[0] = 0x01;
    EXPECT_TRUE(proof.IsWellFormed());
}

TEST_F(ZKProofTest, Groth16ProofSerialization) {
    Groth16Proof proof;
    proof.proofA[0] = 0x01;
    proof.proofB[0] = 0x02;
    proof.proofC[0] = 0x03;
    
    auto bytes = proof.ToBytes();
    auto parsed = Groth16Proof::FromBytes(bytes.data(), bytes.size());
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(proof.proofA, parsed->proofA);
    EXPECT_EQ(proof.proofB, parsed->proofB);
    EXPECT_EQ(proof.proofC, parsed->proofC);
}

TEST_F(ZKProofTest, ZKProofSerialization) {
    ZKProof proof(ProofType::UBIClaim, ProofSystem::Placeholder);
    
    PublicInputs inputs;
    inputs.Add(GenerateRandomFieldElement());
    proof.SetPublicInputs(inputs);
    
    std::vector<Byte> proofData(64);
    proofData[0] = 0xFF;
    proof.SetProofData(proofData);
    
    auto bytes = proof.ToBytes();
    auto parsed = ZKProof::FromBytes(bytes.data(), bytes.size());
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->GetType(), ProofType::UBIClaim);
    EXPECT_EQ(parsed->GetSystem(), ProofSystem::Placeholder);
    EXPECT_EQ(parsed->GetPublicInputs().Count(), 1);
}

TEST_F(ZKProofTest, IdentityProofCreation) {
    // Create identity tree
    auto commitment = secrets_.GetCommitment();
    VectorCommitment tree;
    tree.Add(commitment.ToFieldElement());
    
    auto merkleProof = tree.Prove(0);
    ASSERT_TRUE(merkleProof.has_value());
    
    // Create proof
    EpochId epoch = 100;
    auto nullifier = secrets_.DeriveNullifier(epoch);
    
    auto proof = IdentityProof::CreateUBIClaimProof(
        tree.GetRoot(),
        nullifier,
        epoch,
        secrets_.secretKey,
        secrets_.nullifierKey,
        secrets_.trapdoor,
        *merkleProof
    );
    
    EXPECT_TRUE(proof.IsValid());
    EXPECT_EQ(proof.GetEpoch(), epoch);
    EXPECT_EQ(proof.GetNullifier().GetHash(), nullifier.GetHash());
}

TEST_F(ZKProofTest, IdentityProofVerification) {
    auto commitment = secrets_.GetCommitment();
    VectorCommitment tree;
    tree.Add(commitment.ToFieldElement());
    
    auto merkleProof = tree.Prove(0);
    ASSERT_TRUE(merkleProof.has_value());
    
    EpochId epoch = 100;
    auto nullifier = secrets_.DeriveNullifier(epoch);
    
    auto proof = IdentityProof::CreateUBIClaimProof(
        tree.GetRoot(),
        nullifier,
        epoch,
        secrets_.secretKey,
        secrets_.nullifierKey,
        secrets_.trapdoor,
        *merkleProof
    );
    
    // Verify against correct root
    EXPECT_TRUE(proof.VerifyProof(tree.GetRoot()));
    
    // Verify against wrong root should fail
    auto wrongRoot = GenerateRandomFieldElement();
    EXPECT_FALSE(proof.VerifyProof(wrongRoot));
}

TEST_F(ZKProofTest, ProofGenerator) {
    auto commitment = secrets_.GetCommitment();
    VectorCommitment tree;
    tree.Add(commitment.ToFieldElement());
    
    auto merkleProof = tree.Prove(0);
    ASSERT_TRUE(merkleProof.has_value());
    
    EpochId epoch = 100;
    
    auto proof = ProofGenerator::Instance().GenerateUBIClaimProof(
        secrets_.secretKey,
        secrets_.nullifierKey,
        secrets_.trapdoor,
        tree.GetRoot(),
        *merkleProof,
        epoch
    );
    
    ASSERT_TRUE(proof.has_value());
    EXPECT_TRUE(proof->IsValid());
}

// ============================================================================
// Identity Manager Tests
// ============================================================================

class IdentityManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        IdentityManager::Config config;
        config.activationDelay = 0;  // Immediate activation
        config.epochDuration = 604800;
        config.genesisTime = 0;
        manager_ = std::make_unique<IdentityManager>(config);
        manager_->SetBlockContext(1000, 1000000);
    }
    
    std::unique_ptr<IdentityManager> manager_;
};

TEST_F(IdentityManagerTest, RegisterIdentity) {
    auto secrets = IdentitySecrets::Generate();
    auto commitment = secrets.GetCommitment();
    
    RegistrationRequest request;
    request.commitment = commitment;
    request.timestamp = 1000000;
    
    auto record = manager_->RegisterIdentity(request);
    
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->commitment, commitment);
    EXPECT_EQ(record->status, IdentityStatus::Active);
}

TEST_F(IdentityManagerTest, DoubleRegistrationFails) {
    auto secrets = IdentitySecrets::Generate();
    auto commitment = secrets.GetCommitment();
    
    RegistrationRequest request;
    request.commitment = commitment;
    request.timestamp = 1000000;
    
    auto r1 = manager_->RegisterIdentity(request);
    auto r2 = manager_->RegisterIdentity(request);
    
    EXPECT_TRUE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
}

TEST_F(IdentityManagerTest, GetIdentity) {
    auto secrets = IdentitySecrets::Generate();
    auto commitment = secrets.GetCommitment();
    
    RegistrationRequest request;
    request.commitment = commitment;
    request.timestamp = 1000000;
    
    auto registered = manager_->RegisterIdentity(request);
    ASSERT_TRUE(registered.has_value());
    
    auto retrieved = manager_->GetIdentity(commitment);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->commitment, commitment);
}

TEST_F(IdentityManagerTest, GetIdentityById) {
    auto secrets = IdentitySecrets::Generate();
    auto commitment = secrets.GetCommitment();
    
    RegistrationRequest request;
    request.commitment = commitment;
    request.timestamp = 1000000;
    
    auto registered = manager_->RegisterIdentity(request);
    ASSERT_TRUE(registered.has_value());
    
    auto retrieved = manager_->GetIdentityById(registered->id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, registered->id);
}

TEST_F(IdentityManagerTest, UpdateStatus) {
    auto secrets = IdentitySecrets::Generate();
    auto commitment = secrets.GetCommitment();
    
    RegistrationRequest request;
    request.commitment = commitment;
    request.timestamp = 1000000;
    
    auto registered = manager_->RegisterIdentity(request);
    ASSERT_TRUE(registered.has_value());
    
    EXPECT_TRUE(manager_->UpdateIdentityStatus(registered->id, IdentityStatus::Suspended));
    
    auto updated = manager_->GetIdentityById(registered->id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->status, IdentityStatus::Suspended);
}

TEST_F(IdentityManagerTest, GetMembershipProof) {
    auto secrets = IdentitySecrets::Generate();
    auto commitment = secrets.GetCommitment();
    
    RegistrationRequest request;
    request.commitment = commitment;
    request.timestamp = 1000000;
    
    auto registered = manager_->RegisterIdentity(request);
    ASSERT_TRUE(registered.has_value());
    
    auto proof = manager_->GetMembershipProof(commitment);
    ASSERT_TRUE(proof.has_value());
    
    EXPECT_TRUE(manager_->VerifyMembershipProof(commitment, *proof));
}

TEST_F(IdentityManagerTest, ProcessUBIClaim) {
    // Register identity
    auto secrets = IdentitySecrets::Generate();
    auto commitment = secrets.GetCommitment();
    
    RegistrationRequest request;
    request.commitment = commitment;
    request.timestamp = 1000000;
    
    auto registered = manager_->RegisterIdentity(request);
    ASSERT_TRUE(registered.has_value());
    
    // Get membership proof
    auto merkleProof = manager_->GetMembershipProof(commitment);
    ASSERT_TRUE(merkleProof.has_value());
    
    // Create UBI claim
    EpochId epoch = CalculateEpoch(1000000, 604800, 0);
    auto nullifier = secrets.DeriveNullifier(epoch);
    
    auto identityProof = IdentityProof::CreateUBIClaimProof(
        manager_->GetIdentityRoot(),
        nullifier,
        epoch,
        secrets.secretKey,
        secrets.nullifierKey,
        secrets.trapdoor,
        *merkleProof
    );
    
    UBIClaim claim;
    claim.nullifier = nullifier;
    claim.epoch = epoch;
    claim.recipientScript = {0x76, 0xa9, 0x14};  // Dummy script
    claim.proof = identityProof;
    claim.timestamp = 1000000;
    
    // Process claim
    EXPECT_TRUE(manager_->ProcessUBIClaim(claim));
    
    // Second claim with same nullifier should fail
    EXPECT_FALSE(manager_->ProcessUBIClaim(claim));
}

TEST_F(IdentityManagerTest, Stats) {
    // Register some identities
    for (int i = 0; i < 5; ++i) {
        auto secrets = IdentitySecrets::Generate();
        RegistrationRequest request;
        request.commitment = secrets.GetCommitment();
        request.timestamp = 1000000;
        manager_->RegisterIdentity(request);
    }
    
    auto stats = manager_->GetStats();
    
    EXPECT_EQ(stats.totalIdentities, 5);
    EXPECT_EQ(stats.activeIdentities, 5);
    EXPECT_EQ(stats.pendingIdentities, 0);
}

// ============================================================================
// Identity Secrets Tests
// ============================================================================

TEST(IdentitySecretsTest, Generate) {
    auto secrets = IdentitySecrets::Generate();
    
    EXPECT_FALSE(secrets.secretKey.IsZero());
    EXPECT_FALSE(secrets.nullifierKey.IsZero());
    EXPECT_FALSE(secrets.trapdoor.IsZero());
}

TEST(IdentitySecretsTest, FromMasterSeed) {
    std::array<Byte, 32> seed;
    std::fill(seed.begin(), seed.end(), 0x42);
    
    auto s1 = IdentitySecrets::FromMasterSeed(seed);
    auto s2 = IdentitySecrets::FromMasterSeed(seed);
    
    // Same seed should produce same secrets
    EXPECT_EQ(s1.secretKey, s2.secretKey);
    EXPECT_EQ(s1.nullifierKey, s2.nullifierKey);
    EXPECT_EQ(s1.trapdoor, s2.trapdoor);
}

TEST(IdentitySecretsTest, DifferentSeedsDifferentSecrets) {
    std::array<Byte, 32> seed1, seed2;
    std::fill(seed1.begin(), seed1.end(), 0x42);
    std::fill(seed2.begin(), seed2.end(), 0x43);
    
    auto s1 = IdentitySecrets::FromMasterSeed(seed1);
    auto s2 = IdentitySecrets::FromMasterSeed(seed2);
    
    EXPECT_NE(s1.secretKey, s2.secretKey);
}

TEST(IdentitySecretsTest, GetCommitment) {
    auto secrets = IdentitySecrets::Generate();
    auto commitment = secrets.GetCommitment();
    
    EXPECT_FALSE(commitment.IsEmpty());
    EXPECT_TRUE(commitment.Verify(secrets.secretKey, secrets.nullifierKey, secrets.trapdoor));
}

TEST(IdentitySecretsTest, DeriveNullifier) {
    auto secrets = IdentitySecrets::Generate();
    
    auto n1 = secrets.DeriveNullifier(100);
    auto n2 = secrets.DeriveNullifier(101);
    
    EXPECT_FALSE(n1.IsEmpty());
    EXPECT_NE(n1.GetHash(), n2.GetHash());
}

TEST(IdentitySecretsTest, EncryptDecrypt) {
    auto secrets = IdentitySecrets::Generate();
    secrets.treeIndex = 42;
    
    std::array<Byte, 32> key;
    std::fill(key.begin(), key.end(), 0xAB);
    
    auto encrypted = secrets.Encrypt(key);
    auto decrypted = IdentitySecrets::Decrypt(encrypted.data(), encrypted.size(), key);
    
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(decrypted->treeIndex, 42);
    // The other fields are derived from seed, so we verify commitment matches
    EXPECT_EQ(secrets.GetCommitment(), decrypted->GetCommitment());
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST(UtilityTest, ProofTypeStringConversion) {
    EXPECT_EQ(ProofTypeToString(ProofType::UBIClaim), "UBIClaim");
    EXPECT_EQ(ProofTypeFromString("UBIClaim"), ProofType::UBIClaim);
    EXPECT_EQ(ProofTypeFromString("Invalid"), std::nullopt);
}

TEST(UtilityTest, ProofSystemStringConversion) {
    EXPECT_EQ(ProofSystemToString(ProofSystem::Groth16), "Groth16");
    EXPECT_EQ(ProofSystemFromString("Groth16"), ProofSystem::Groth16);
    EXPECT_EQ(ProofSystemFromString("Invalid"), std::nullopt);
}

TEST(UtilityTest, IdentityStatusStringConversion) {
    EXPECT_EQ(IdentityStatusToString(IdentityStatus::Active), "Active");
    EXPECT_EQ(IdentityStatusFromString("Active"), IdentityStatus::Active);
    EXPECT_EQ(IdentityStatusFromString("Invalid"), std::nullopt);
}

TEST(UtilityTest, ComputeIdentityId) {
    FieldElement sk, nk, td;
    auto commitment = IdentityCommitment::Generate(sk, nk, td);
    
    auto id1 = ComputeIdentityId(commitment);
    auto id2 = ComputeIdentityId(commitment);
    
    EXPECT_EQ(id1, id2);  // Deterministic
    
    FieldElement sk2, nk2, td2;
    auto commitment2 = IdentityCommitment::Generate(sk2, nk2, td2);
    auto id3 = ComputeIdentityId(commitment2);
    
    EXPECT_NE(id1, id3);  // Different commitments produce different IDs
}

TEST(UtilityTest, GenerateMasterSeed) {
    auto seed1 = GenerateMasterSeed();
    auto seed2 = GenerateMasterSeed();
    
    // Seeds should be different (extremely unlikely to be same)
    EXPECT_NE(seed1, seed2);
}

TEST(UtilityTest, GenerateRandomFieldElement) {
    auto e1 = GenerateRandomFieldElement();
    auto e2 = GenerateRandomFieldElement();
    
    EXPECT_FALSE(e1.IsZero());
    EXPECT_FALSE(e2.IsZero());
    EXPECT_NE(e1, e2);
}

TEST(UtilityTest, HashToFieldElement) {
    Byte data[] = {0x01, 0x02, 0x03, 0x04};
    
    auto e1 = HashToFieldElement(data, sizeof(data));
    auto e2 = HashToFieldElement(data, sizeof(data));
    
    EXPECT_FALSE(e1.IsZero());
    EXPECT_EQ(e1, e2);  // Deterministic
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
