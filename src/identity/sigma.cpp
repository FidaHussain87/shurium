// SHURIUM - Sigma Protocol Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/identity/sigma.h>
#include <shurium/crypto/poseidon.h>
#include <shurium/core/random.h>
#include <shurium/core/hex.h>

#include <algorithm>
#include <cstring>

namespace shurium {
namespace identity {

// ============================================================================
// Generators (nothing-up-my-sleeve construction)
// ============================================================================

namespace {

// Pre-computed generators using Poseidon hash of domain strings
// These are deterministic "nothing-up-my-sleeve" numbers

FieldElement ComputeGenerator(const std::string& seed) {
    Poseidon hasher;
    hasher.AbsorbBytes(reinterpret_cast<const Byte*>(seed.data()), seed.size());
    return hasher.Squeeze();
}

} // anonymous namespace

FieldElement GetGeneratorG() {
    static FieldElement g = ComputeGenerator("SHURIUM_GENERATOR_G_v1");
    return g;
}

FieldElement GetGeneratorH() {
    static FieldElement h = ComputeGenerator("SHURIUM_GENERATOR_H_v1");
    return h;
}

std::vector<FieldElement> GetGenerators(size_t count) {
    std::vector<FieldElement> generators;
    generators.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        std::string seed = "SHURIUM_GENERATOR_" + std::to_string(i) + "_v1";
        generators.push_back(ComputeGenerator(seed));
    }
    
    return generators;
}

FieldElement DeriveGenerator(const std::string& seed) {
    return ComputeGenerator(seed);
}

// ============================================================================
// SchnorrProof
// ============================================================================

std::vector<Byte> SchnorrProof::ToBytes() const {
    std::vector<Byte> result;
    result.reserve(64);
    
    auto commBytes = commitment.ToBytes();
    result.insert(result.end(), commBytes.begin(), commBytes.end());
    
    auto respBytes = response.ToBytes();
    result.insert(result.end(), respBytes.begin(), respBytes.end());
    
    return result;
}

std::optional<SchnorrProof> SchnorrProof::FromBytes(const Byte* data, size_t len) {
    if (len < 64) {
        return std::nullopt;
    }
    
    SchnorrProof proof;
    proof.commitment = FieldElement::FromBytes(data, 32);
    proof.response = FieldElement::FromBytes(data + 32, 32);
    
    return proof;
}

bool SchnorrProof::IsWellFormed() const {
    return !commitment.IsZero();
}

// ============================================================================
// SchnorrProver
// ============================================================================

SchnorrProof SchnorrProver::Prove(
    const FieldElement& secretKey,
    const FieldElement& generator,
    const FieldElement& publicKey,
    const std::vector<Byte>& context) {
    
    // Generate random nonce
    std::array<Byte, 32> randBytes;
    GetRandBytes(randBytes.data(), randBytes.size());
    FieldElement r = FieldElement::FromBytes(randBytes.data(), 32);
    
    // Compute commitment R = g^r
    // In a finite field, this is multiplication: R = r * g
    FieldElement R = r * generator;
    
    // Compute challenge c = Hash(P, R, context)
    FieldElement c = SchnorrVerifier::ComputeChallenge(publicKey, R, context);
    
    // Compute response s = r + c*x
    FieldElement s = r + (c * secretKey);
    
    return SchnorrProof(R, s);
}

SchnorrProof SchnorrProver::Prove(
    const FieldElement& secretKey,
    const FieldElement& generator,
    const std::vector<Byte>& context) {
    
    // Compute public key P = g^x = x * g
    FieldElement publicKey = secretKey * generator;
    
    return Prove(secretKey, generator, publicKey, context);
}

// ============================================================================
// SchnorrVerifier
// ============================================================================

bool SchnorrVerifier::Verify(
    const SchnorrProof& proof,
    const FieldElement& generator,
    const FieldElement& publicKey,
    const std::vector<Byte>& context) {
    
    if (!proof.IsWellFormed()) {
        return false;
    }
    
    // Compute challenge c = Hash(P, R, context)
    FieldElement c = ComputeChallenge(publicKey, proof.commitment, context);
    
    // Verify: g^s = R * P^c
    // In additive notation: s * g = R + c * P
    FieldElement lhs = proof.response * generator;
    FieldElement rhs = proof.commitment + (c * publicKey);
    
    return lhs == rhs;
}

FieldElement SchnorrVerifier::ComputeChallenge(
    const FieldElement& publicKey,
    const FieldElement& commitment,
    const std::vector<Byte>& context) {
    
    Poseidon hasher;
    hasher.Absorb(publicKey);
    hasher.Absorb(commitment);
    
    if (!context.empty()) {
        hasher.AbsorbBytes(context.data(), context.size());
    }
    
    return hasher.Squeeze();
}

// ============================================================================
// PedersenOpeningProof
// ============================================================================

std::vector<Byte> PedersenOpeningProof::ToBytes() const {
    std::vector<Byte> result;
    result.reserve(96);
    
    auto commBytes = commitment.ToBytes();
    result.insert(result.end(), commBytes.begin(), commBytes.end());
    
    auto valBytes = responseValue.ToBytes();
    result.insert(result.end(), valBytes.begin(), valBytes.end());
    
    auto randBytes = responseRandomness.ToBytes();
    result.insert(result.end(), randBytes.begin(), randBytes.end());
    
    return result;
}

std::optional<PedersenOpeningProof> PedersenOpeningProof::FromBytes(const Byte* data, size_t len) {
    if (len < 96) {
        return std::nullopt;
    }
    
    PedersenOpeningProof proof;
    proof.commitment = FieldElement::FromBytes(data, 32);
    proof.responseValue = FieldElement::FromBytes(data + 32, 32);
    proof.responseRandomness = FieldElement::FromBytes(data + 64, 32);
    
    return proof;
}

bool PedersenOpeningProof::IsWellFormed() const {
    return !commitment.IsZero();
}

// ============================================================================
// PedersenOpeningProver
// ============================================================================

PedersenOpeningProof PedersenOpeningProver::Prove(
    const FieldElement& value,
    const FieldElement& randomness,
    const FieldElement& generatorG,
    const FieldElement& generatorH,
    const FieldElement& commitment,
    const std::vector<Byte>& context) {
    
    // Generate random values for the proof
    std::array<Byte, 32> randBytes1, randBytes2;
    GetRandBytes(randBytes1.data(), 32);
    GetRandBytes(randBytes2.data(), 32);
    
    FieldElement s_v = FieldElement::FromBytes(randBytes1.data(), 32);
    FieldElement s_r = FieldElement::FromBytes(randBytes2.data(), 32);
    
    // Compute commitment A = g^s_v * h^s_r
    FieldElement A = (s_v * generatorG) + (s_r * generatorH);
    
    // Compute challenge
    Poseidon hasher;
    hasher.Absorb(commitment);
    hasher.Absorb(A);
    if (!context.empty()) {
        hasher.AbsorbBytes(context.data(), context.size());
    }
    FieldElement c = hasher.Squeeze();
    
    // Compute responses
    FieldElement z_v = s_v + (c * value);
    FieldElement z_r = s_r + (c * randomness);
    
    PedersenOpeningProof proof;
    proof.commitment = A;
    proof.responseValue = z_v;
    proof.responseRandomness = z_r;
    
    return proof;
}

// ============================================================================
// PedersenOpeningVerifier
// ============================================================================

bool PedersenOpeningVerifier::Verify(
    const PedersenOpeningProof& proof,
    const FieldElement& generatorG,
    const FieldElement& generatorH,
    const FieldElement& commitment,
    const std::vector<Byte>& context) {
    
    if (!proof.IsWellFormed()) {
        return false;
    }
    
    // Recompute challenge
    Poseidon hasher;
    hasher.Absorb(commitment);
    hasher.Absorb(proof.commitment);
    if (!context.empty()) {
        hasher.AbsorbBytes(context.data(), context.size());
    }
    FieldElement c = hasher.Squeeze();
    
    // Verify: g^z_v * h^z_r = A * C^c
    FieldElement lhs = (proof.responseValue * generatorG) + (proof.responseRandomness * generatorH);
    FieldElement rhs = proof.commitment + (c * commitment);
    
    return lhs == rhs;
}

// ============================================================================
// EqualityProof
// ============================================================================

std::vector<Byte> EqualityProof::ToBytes() const {
    std::vector<Byte> result;
    result.reserve(160);
    
    auto comm1Bytes = commitment1.ToBytes();
    result.insert(result.end(), comm1Bytes.begin(), comm1Bytes.end());
    
    auto comm2Bytes = commitment2.ToBytes();
    result.insert(result.end(), comm2Bytes.begin(), comm2Bytes.end());
    
    auto valBytes = responseValue.ToBytes();
    result.insert(result.end(), valBytes.begin(), valBytes.end());
    
    auto rand1Bytes = responseRandom1.ToBytes();
    result.insert(result.end(), rand1Bytes.begin(), rand1Bytes.end());
    
    auto rand2Bytes = responseRandom2.ToBytes();
    result.insert(result.end(), rand2Bytes.begin(), rand2Bytes.end());
    
    return result;
}

std::optional<EqualityProof> EqualityProof::FromBytes(const Byte* data, size_t len) {
    if (len < 160) {
        return std::nullopt;
    }
    
    EqualityProof proof;
    proof.commitment1 = FieldElement::FromBytes(data, 32);
    proof.commitment2 = FieldElement::FromBytes(data + 32, 32);
    proof.responseValue = FieldElement::FromBytes(data + 64, 32);
    proof.responseRandom1 = FieldElement::FromBytes(data + 96, 32);
    proof.responseRandom2 = FieldElement::FromBytes(data + 128, 32);
    
    return proof;
}

bool EqualityProof::IsWellFormed() const {
    return !commitment1.IsZero() && !commitment2.IsZero();
}

// ============================================================================
// EqualityProver
// ============================================================================

EqualityProof EqualityProver::Prove(
    const FieldElement& value,
    const FieldElement& randomness1,
    const FieldElement& randomness2,
    const FieldElement& generatorG,
    const FieldElement& generatorH1,
    const FieldElement& generatorH2,
    const FieldElement& commitment1,
    const FieldElement& commitment2,
    const std::vector<Byte>& context) {
    
    // Generate random values
    std::array<Byte, 32> rand1, rand2, rand3;
    GetRandBytes(rand1.data(), 32);
    GetRandBytes(rand2.data(), 32);
    GetRandBytes(rand3.data(), 32);
    
    FieldElement s_v = FieldElement::FromBytes(rand1.data(), 32);
    FieldElement s_r1 = FieldElement::FromBytes(rand2.data(), 32);
    FieldElement s_r2 = FieldElement::FromBytes(rand3.data(), 32);
    
    // Compute two commitments with shared s_v
    // A1 = g^s_v * h1^s_r1
    // A2 = g^s_v * h2^s_r2
    FieldElement A1 = (s_v * generatorG) + (s_r1 * generatorH1);
    FieldElement A2 = (s_v * generatorG) + (s_r2 * generatorH2);
    
    // Compute challenge
    Poseidon hasher;
    hasher.Absorb(commitment1);
    hasher.Absorb(commitment2);
    hasher.Absorb(A1);
    hasher.Absorb(A2);
    if (!context.empty()) {
        hasher.AbsorbBytes(context.data(), context.size());
    }
    FieldElement c = hasher.Squeeze();
    
    // Compute responses
    FieldElement z_v = s_v + (c * value);
    FieldElement z_r1 = s_r1 + (c * randomness1);
    FieldElement z_r2 = s_r2 + (c * randomness2);
    
    EqualityProof proof;
    proof.commitment1 = A1;
    proof.commitment2 = A2;
    proof.responseValue = z_v;
    proof.responseRandom1 = z_r1;
    proof.responseRandom2 = z_r2;
    
    return proof;
}

// ============================================================================
// EqualityVerifier
// ============================================================================

bool EqualityVerifier::Verify(
    const EqualityProof& proof,
    const FieldElement& generatorG,
    const FieldElement& generatorH1,
    const FieldElement& generatorH2,
    const FieldElement& commitment1,
    const FieldElement& commitment2,
    const std::vector<Byte>& context) {
    
    if (!proof.IsWellFormed()) {
        return false;
    }
    
    // Recompute challenge
    Poseidon hasher;
    hasher.Absorb(commitment1);
    hasher.Absorb(commitment2);
    hasher.Absorb(proof.commitment1);
    hasher.Absorb(proof.commitment2);
    if (!context.empty()) {
        hasher.AbsorbBytes(context.data(), context.size());
    }
    FieldElement c = hasher.Squeeze();
    
    // Verify both equations:
    // g^z_v * h1^z_r1 = A1 * C1^c
    // g^z_v * h2^z_r2 = A2 * C2^c
    FieldElement lhs1 = (proof.responseValue * generatorG) + (proof.responseRandom1 * generatorH1);
    FieldElement rhs1 = proof.commitment1 + (c * commitment1);
    
    FieldElement lhs2 = (proof.responseValue * generatorG) + (proof.responseRandom2 * generatorH2);
    FieldElement rhs2 = proof.commitment2 + (c * commitment2);
    
    return (lhs1 == rhs1) && (lhs2 == rhs2);
}

// ============================================================================
// OrProof
// ============================================================================

std::vector<Byte> OrProof::ToBytes() const {
    std::vector<Byte> result;
    
    // Number of branches (4 bytes)
    uint32_t n = static_cast<uint32_t>(commitments.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((n >> (i * 8)) & 0xFF));
    }
    
    // Commitments
    for (const auto& c : commitments) {
        auto bytes = c.ToBytes();
        result.insert(result.end(), bytes.begin(), bytes.end());
    }
    
    // Challenges
    for (const auto& c : challenges) {
        auto bytes = c.ToBytes();
        result.insert(result.end(), bytes.begin(), bytes.end());
    }
    
    // Responses
    for (const auto& r : responses) {
        auto bytes = r.ToBytes();
        result.insert(result.end(), bytes.begin(), bytes.end());
    }
    
    return result;
}

std::optional<OrProof> OrProof::FromBytes(const Byte* data, size_t len) {
    if (len < 4) {
        return std::nullopt;
    }
    
    uint32_t n = 0;
    for (int i = 0; i < 4; ++i) {
        n |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    if (n == 0 || n > 1000 || len < n * 96) {
        return std::nullopt;
    }
    
    OrProof proof;
    proof.commitments.reserve(n);
    proof.challenges.reserve(n);
    proof.responses.reserve(n);
    
    for (uint32_t i = 0; i < n; ++i) {
        proof.commitments.push_back(FieldElement::FromBytes(data, 32));
        data += 32;
    }
    
    for (uint32_t i = 0; i < n; ++i) {
        proof.challenges.push_back(FieldElement::FromBytes(data, 32));
        data += 32;
    }
    
    for (uint32_t i = 0; i < n; ++i) {
        proof.responses.push_back(FieldElement::FromBytes(data, 32));
        data += 32;
    }
    
    return proof;
}

bool OrProof::IsWellFormed() const {
    return !commitments.empty() && 
           commitments.size() == challenges.size() && 
           commitments.size() == responses.size();
}

// ============================================================================
// OrProver
// ============================================================================

OrProof OrProver::Prove(
    size_t witnessIndex,
    const FieldElement& witness,
    const std::vector<FieldElement>& generators,
    const std::vector<FieldElement>& publicValues,
    const std::vector<Byte>& context) {
    
    size_t n = generators.size();
    if (n == 0 || witnessIndex >= n || publicValues.size() != n) {
        return OrProof();
    }
    
    OrProof proof;
    proof.commitments.resize(n);
    proof.challenges.resize(n);
    proof.responses.resize(n);
    
    // For branches where we don't know the witness, simulate the proof
    FieldElement challengeSum = FieldElement::Zero();
    
    for (size_t i = 0; i < n; ++i) {
        if (i == witnessIndex) {
            continue;  // Handle the real branch later
        }
        
        // Generate random challenge and response for simulated branches
        std::array<Byte, 32> randChallenge, randResponse;
        GetRandBytes(randChallenge.data(), 32);
        GetRandBytes(randResponse.data(), 32);
        
        proof.challenges[i] = FieldElement::FromBytes(randChallenge.data(), 32);
        proof.responses[i] = FieldElement::FromBytes(randResponse.data(), 32);
        challengeSum = challengeSum + proof.challenges[i];
        
        // Compute commitment that would verify: R = g^s / P^c
        // In additive notation: R = s*g - c*P
        proof.commitments[i] = (proof.responses[i] * generators[i]) - 
                               (proof.challenges[i] * publicValues[i]);
    }
    
    // For the real branch, compute honest commitment
    std::array<Byte, 32> randNonce;
    GetRandBytes(randNonce.data(), 32);
    FieldElement r = FieldElement::FromBytes(randNonce.data(), 32);
    proof.commitments[witnessIndex] = r * generators[witnessIndex];
    
    // Compute main challenge
    Poseidon hasher;
    for (const auto& P : publicValues) {
        hasher.Absorb(P);
    }
    for (const auto& R : proof.commitments) {
        hasher.Absorb(R);
    }
    if (!context.empty()) {
        hasher.AbsorbBytes(context.data(), context.size());
    }
    FieldElement mainChallenge = hasher.Squeeze();
    
    // Real challenge = main challenge - sum of simulated challenges
    proof.challenges[witnessIndex] = mainChallenge - challengeSum;
    
    // Real response = r + c*x
    proof.responses[witnessIndex] = r + (proof.challenges[witnessIndex] * witness);
    
    return proof;
}

// ============================================================================
// OrVerifier
// ============================================================================

bool OrVerifier::Verify(
    const OrProof& proof,
    const std::vector<FieldElement>& generators,
    const std::vector<FieldElement>& publicValues,
    const std::vector<Byte>& context) {
    
    if (!proof.IsWellFormed()) {
        return false;
    }
    
    size_t n = proof.NumBranches();
    if (generators.size() != n || publicValues.size() != n) {
        return false;
    }
    
    // Recompute main challenge
    Poseidon hasher;
    for (const auto& P : publicValues) {
        hasher.Absorb(P);
    }
    for (const auto& R : proof.commitments) {
        hasher.Absorb(R);
    }
    if (!context.empty()) {
        hasher.AbsorbBytes(context.data(), context.size());
    }
    FieldElement mainChallenge = hasher.Squeeze();
    
    // Check that challenges sum to main challenge
    FieldElement challengeSum = FieldElement::Zero();
    for (const auto& c : proof.challenges) {
        challengeSum = challengeSum + c;
    }
    if (challengeSum != mainChallenge) {
        return false;
    }
    
    // Verify each branch: g^s = R * P^c
    for (size_t i = 0; i < n; ++i) {
        FieldElement lhs = proof.responses[i] * generators[i];
        FieldElement rhs = proof.commitments[i] + (proof.challenges[i] * publicValues[i]);
        if (lhs != rhs) {
            return false;
        }
    }
    
    return true;
}

} // namespace identity
} // namespace shurium
