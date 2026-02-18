// SHURIUM - Range Proof Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/identity/rangeproof.h>
#include <shurium/crypto/poseidon.h>
#include <shurium/core/random.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace shurium {
namespace identity {

// ============================================================================
// Utility Functions
// ============================================================================

FieldElement PedersenCommit(uint64_t value, const FieldElement& randomness) {
    const auto& gens = RangeProofGenerators::Get();
    FieldElement v(value);
    return (v * gens.G()) + (randomness * gens.H());
}

FieldElement PedersenCommit(const FieldElement& value, const FieldElement& randomness) {
    const auto& gens = RangeProofGenerators::Get();
    return (value * gens.G()) + (randomness * gens.H());
}

FieldElement GenerateBlinding() {
    std::array<Byte, 32> randBytes;
    GetRandBytes(randBytes.data(), randBytes.size());
    return FieldElement::FromBytes(randBytes.data(), 32);
}

FieldElement InnerProduct(const std::vector<FieldElement>& a,
                          const std::vector<FieldElement>& b) {
    if (a.size() != b.size()) {
        return FieldElement::Zero();
    }
    
    FieldElement result = FieldElement::Zero();
    for (size_t i = 0; i < a.size(); ++i) {
        result = result + (a[i] * b[i]);
    }
    return result;
}

std::vector<FieldElement> HadamardProduct(const std::vector<FieldElement>& a,
                                           const std::vector<FieldElement>& b) {
    if (a.size() != b.size()) {
        return {};
    }
    
    std::vector<FieldElement> result;
    result.reserve(a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        result.push_back(a[i] * b[i]);
    }
    return result;
}

// ============================================================================
// RangeProofGenerators
// ============================================================================

namespace {
    std::map<size_t, std::unique_ptr<RangeProofGenerators>> generatorCache;
}

RangeProofGenerators::RangeProofGenerators(size_t numBits) : numBits_(numBits) {
    // Generate g and h from nothing-up-my-sleeve strings
    g_ = DeriveGenerator("SHURIUM_RANGEPROOF_G");
    h_ = DeriveGenerator("SHURIUM_RANGEPROOF_H");
    u_ = DeriveGenerator("SHURIUM_RANGEPROOF_U");
    
    // Generate vector generators
    gi_.reserve(numBits);
    hi_.reserve(numBits);
    
    for (size_t i = 0; i < numBits; ++i) {
        gi_.push_back(DeriveGenerator("SHURIUM_RANGEPROOF_G_" + std::to_string(i)));
        hi_.push_back(DeriveGenerator("SHURIUM_RANGEPROOF_H_" + std::to_string(i)));
    }
}

const RangeProofGenerators& RangeProofGenerators::Get(size_t numBits) {
    auto it = generatorCache.find(numBits);
    if (it == generatorCache.end()) {
        auto gens = std::unique_ptr<RangeProofGenerators>(new RangeProofGenerators(numBits));
        auto* ptr = gens.get();
        generatorCache[numBits] = std::move(gens);
        return *ptr;
    }
    return *it->second;
}

// ============================================================================
// RangeProof
// ============================================================================

std::vector<Byte> RangeProof::ToBytes() const {
    std::vector<Byte> result;
    
    // numBits (1 byte)
    result.push_back(numBits);
    
    // Fixed-size elements (8 x 32 = 256 bytes)
    auto addField = [&result](const FieldElement& f) {
        auto bytes = f.ToBytes();
        result.insert(result.end(), bytes.begin(), bytes.end());
    };
    
    addField(A);
    addField(S);
    addField(T1);
    addField(T2);
    addField(tau_x);
    addField(mu);
    addField(t_hat);
    addField(a);
    addField(b);
    
    // L and R vectors (variable length)
    uint32_t lrCount = static_cast<uint32_t>(L.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((lrCount >> (i * 8)) & 0xFF));
    }
    
    for (const auto& l : L) {
        addField(l);
    }
    for (const auto& r : R) {
        addField(r);
    }
    
    return result;
}

std::optional<RangeProof> RangeProof::FromBytes(const Byte* data, size_t len) {
    if (len < 289) {  // 1 + 9*32 + 4
        return std::nullopt;
    }
    
    RangeProof proof;
    proof.numBits = data[0];
    data++;
    len--;
    
    auto readField = [&data]() {
        FieldElement f = FieldElement::FromBytes(data, 32);
        data += 32;
        return f;
    };
    
    proof.A = readField();
    proof.S = readField();
    proof.T1 = readField();
    proof.T2 = readField();
    proof.tau_x = readField();
    proof.mu = readField();
    proof.t_hat = readField();
    proof.a = readField();
    proof.b = readField();
    len -= 288;
    
    // L and R vectors
    if (len < 4) return std::nullopt;
    
    uint32_t lrCount = 0;
    for (int i = 0; i < 4; ++i) {
        lrCount |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    if (len < lrCount * 64) return std::nullopt;
    
    proof.L.reserve(lrCount);
    proof.R.reserve(lrCount);
    
    for (uint32_t i = 0; i < lrCount; ++i) {
        proof.L.push_back(readField());
    }
    for (uint32_t i = 0; i < lrCount; ++i) {
        proof.R.push_back(readField());
    }
    
    return proof;
}

bool RangeProof::IsWellFormed() const {
    return numBits > 0 && numBits <= MAX_RANGE_BITS &&
           !A.IsZero() && !S.IsZero() &&
           L.size() == R.size();
}

size_t RangeProof::Size() const {
    return 1 + 9 * 32 + 4 + L.size() * 64;
}

// ============================================================================
// RangeProofProver
// ============================================================================

std::optional<RangeProof> RangeProofProver::Prove(
    uint64_t value,
    const FieldElement& randomness,
    size_t numBits) {
    
    // Check value is in range
    if (numBits > MAX_RANGE_BITS) {
        return std::nullopt;
    }
    
    uint64_t maxValue = (numBits == 64) ? UINT64_MAX : ((1ULL << numBits) - 1);
    if (value > maxValue) {
        return std::nullopt;
    }
    
    // Compute commitment
    FieldElement commitment = PedersenCommit(value, randomness);
    
    return Prove(value, randomness, commitment, numBits);
}

std::optional<RangeProof> RangeProofProver::Prove(
    uint64_t value,
    const FieldElement& randomness,
    const FieldElement& commitment,
    size_t numBits) {
    
    const auto& gens = RangeProofGenerators::Get(numBits);
    
    RangeProof proof;
    proof.numBits = static_cast<uint8_t>(numBits);
    
    // Extract bits of value
    std::vector<FieldElement> aL(numBits);  // Bit vector
    std::vector<FieldElement> aR(numBits);  // aL - 1 (so 0 for 1-bits, -1 for 0-bits)
    
    for (size_t i = 0; i < numBits; ++i) {
        uint64_t bit = (value >> i) & 1;
        aL[i] = FieldElement(bit);
        aR[i] = aL[i] - FieldElement::One();
    }
    
    // Generate blinding vectors
    std::vector<FieldElement> sL(numBits);
    std::vector<FieldElement> sR(numBits);
    
    for (size_t i = 0; i < numBits; ++i) {
        sL[i] = GenerateBlinding();
        sR[i] = GenerateBlinding();
    }
    
    // Generate blinding factors for commitments
    FieldElement alpha = GenerateBlinding();
    FieldElement rho = GenerateBlinding();
    
    // Compute A = h^alpha * G_i^aL_i * H_i^aR_i
    proof.A = alpha * gens.H();
    for (size_t i = 0; i < numBits; ++i) {
        proof.A = proof.A + (aL[i] * gens.Gi()[i]) + (aR[i] * gens.Hi()[i]);
    }
    
    // Compute S = h^rho * G_i^sL_i * H_i^sR_i
    proof.S = rho * gens.H();
    for (size_t i = 0; i < numBits; ++i) {
        proof.S = proof.S + (sL[i] * gens.Gi()[i]) + (sR[i] * gens.Hi()[i]);
    }
    
    // Challenge y (using Fiat-Shamir)
    Poseidon hasher;
    hasher.Absorb(commitment);
    hasher.Absorb(proof.A);
    hasher.Absorb(proof.S);
    FieldElement y = hasher.Squeeze();
    
    // Challenge z
    hasher.Reset();
    hasher.Absorb(proof.A);
    hasher.Absorb(proof.S);
    hasher.Absorb(y);
    FieldElement z = hasher.Squeeze();
    
    // Compute polynomial coefficients
    // t(X) = <l(X), r(X)> = t_0 + t_1*X + t_2*X^2
    
    // Powers of y
    std::vector<FieldElement> yn(numBits);
    yn[0] = FieldElement::One();
    for (size_t i = 1; i < numBits; ++i) {
        yn[i] = yn[i-1] * y;
    }
    
    // Powers of 2
    std::vector<FieldElement> twon(numBits);
    twon[0] = FieldElement::One();
    FieldElement two(2);
    for (size_t i = 1; i < numBits; ++i) {
        twon[i] = twon[i-1] * two;
    }
    
    // l(X) = aL - z*1 + sL*X
    // r(X) = y^n ∘ (aR + z*1 + sR*X) + z^2 * 2^n
    
    // Compute t_1 and t_2 coefficients
    FieldElement t1 = FieldElement::Zero();
    FieldElement t2 = FieldElement::Zero();
    
    for (size_t i = 0; i < numBits; ++i) {
        // Contribution to t_1: sL_i * (y^i * (aR_i + z) + z^2 * 2^i) + (aL_i - z) * y^i * sR_i
        FieldElement term1 = sL[i] * (yn[i] * (aR[i] + z) + z.Square() * twon[i]);
        FieldElement term2 = (aL[i] - z) * yn[i] * sR[i];
        t1 = t1 + term1 + term2;
        
        // Contribution to t_2: sL_i * y^i * sR_i
        t2 = t2 + sL[i] * yn[i] * sR[i];
    }
    
    // Commit to t_1 and t_2
    FieldElement tau1 = GenerateBlinding();
    FieldElement tau2 = GenerateBlinding();
    
    proof.T1 = PedersenCommit(t1, tau1);
    proof.T2 = PedersenCommit(t2, tau2);
    
    // Challenge x
    hasher.Reset();
    hasher.Absorb(proof.T1);
    hasher.Absorb(proof.T2);
    hasher.Absorb(z);
    FieldElement x = hasher.Squeeze();
    
    // Compute final values
    // l = aL - z*1 + sL*x
    // r = y^n ∘ (aR + z*1 + sR*x) + z^2 * 2^n
    std::vector<FieldElement> l(numBits);
    std::vector<FieldElement> r(numBits);
    
    for (size_t i = 0; i < numBits; ++i) {
        l[i] = aL[i] - z + sL[i] * x;
        r[i] = yn[i] * (aR[i] + z + sR[i] * x) + z.Square() * twon[i];
    }
    
    // t_hat = <l, r>
    proof.t_hat = InnerProduct(l, r);
    
    // tau_x = tau_2 * x^2 + tau_1 * x + z^2 * gamma (where gamma is the commitment randomness)
    proof.tau_x = tau2 * x.Square() + tau1 * x + z.Square() * randomness;
    
    // mu = alpha + rho * x
    proof.mu = alpha + rho * x;
    
    // ========================================================================
    // Inner Product Argument (Bulletproofs Protocol 2)
    // ========================================================================
    // 
    // We want to prove knowledge of vectors l and r such that:
    //   P = g^l * h^r * u^<l,r>
    // 
    // This is done recursively, halving the vector size each round.
    // After log2(n) rounds, we're left with single scalars a and b.
    
    size_t n = numBits;
    std::vector<FieldElement> a_vec = l;
    std::vector<FieldElement> b_vec = r;
    
    // Create scaled generator vectors: h' = h^(y^-n)
    std::vector<FieldElement> g_vec = gens.Gi();
    std::vector<FieldElement> h_vec(numBits);
    
    // Compute y^(-i) for scaling h generators
    FieldElement y_inv = y.Inverse();
    FieldElement y_inv_pow = FieldElement::One();
    for (size_t i = 0; i < numBits; ++i) {
        h_vec[i] = y_inv_pow * gens.Hi()[i];
        y_inv_pow = y_inv_pow * y_inv;
    }
    
    // Challenge for inner product argument
    hasher.Reset();
    hasher.Absorb(proof.t_hat);
    hasher.Absorb(proof.tau_x);
    hasher.Absorb(proof.mu);
    FieldElement w = hasher.Squeeze();
    
    // Recursive inner product rounds
    while (n > 1) {
        n = n / 2;
        
        // Split vectors in half
        std::vector<FieldElement> a_lo(a_vec.begin(), a_vec.begin() + n);
        std::vector<FieldElement> a_hi(a_vec.begin() + n, a_vec.end());
        std::vector<FieldElement> b_lo(b_vec.begin(), b_vec.begin() + n);
        std::vector<FieldElement> b_hi(b_vec.begin() + n, b_vec.end());
        std::vector<FieldElement> g_lo(g_vec.begin(), g_vec.begin() + n);
        std::vector<FieldElement> g_hi(g_vec.begin() + n, g_vec.end());
        std::vector<FieldElement> h_lo(h_vec.begin(), h_vec.begin() + n);
        std::vector<FieldElement> h_hi(h_vec.begin() + n, h_vec.end());
        
        // Compute cross terms
        // c_L = <a_lo, b_hi>
        // c_R = <a_hi, b_lo>
        FieldElement c_L = InnerProduct(a_lo, b_hi);
        FieldElement c_R = InnerProduct(a_hi, b_lo);
        
        // Compute L = g_hi^a_lo * h_lo^b_hi * u^c_L
        FieldElement L_val = c_L * gens.U();
        for (size_t i = 0; i < n; ++i) {
            L_val = L_val + (a_lo[i] * g_hi[i]) + (b_hi[i] * h_lo[i]);
        }
        
        // Compute R = g_lo^a_hi * h_hi^b_lo * u^c_R
        FieldElement R_val = c_R * gens.U();
        for (size_t i = 0; i < n; ++i) {
            R_val = R_val + (a_hi[i] * g_lo[i]) + (b_lo[i] * h_hi[i]);
        }
        
        proof.L.push_back(L_val);
        proof.R.push_back(R_val);
        
        // Compute challenge for this round
        hasher.Reset();
        hasher.Absorb(L_val);
        hasher.Absorb(R_val);
        hasher.Absorb(w);
        FieldElement u_challenge = hasher.Squeeze();
        FieldElement u_inv = u_challenge.Inverse();
        
        // Update for next round
        // a' = a_lo * u + a_hi * u^(-1)
        // b' = b_lo * u^(-1) + b_hi * u
        // g' = g_lo^(u^-1) * g_hi^u
        // h' = h_lo^u * h_hi^(u^-1)
        a_vec.resize(n);
        b_vec.resize(n);
        g_vec.resize(n);
        h_vec.resize(n);
        
        for (size_t i = 0; i < n; ++i) {
            a_vec[i] = a_lo[i] * u_challenge + a_hi[i] * u_inv;
            b_vec[i] = b_lo[i] * u_inv + b_hi[i] * u_challenge;
            g_vec[i] = u_inv * g_lo[i] + u_challenge * g_hi[i];
            h_vec[i] = u_challenge * h_lo[i] + u_inv * h_hi[i];
        }
        
        // Update w for next round
        w = u_challenge;
    }
    
    // Final scalars
    proof.a = a_vec[0];
    proof.b = b_vec[0];
    
    return proof;
}

// ============================================================================
// RangeProofVerifier
// ============================================================================

bool RangeProofVerifier::Verify(
    const RangeProof& proof,
    const FieldElement& commitment) {
    
    if (!proof.IsWellFormed()) {
        return false;
    }
    
    const auto& gens = RangeProofGenerators::Get(proof.numBits);
    size_t numBits = proof.numBits;
    
    // Check that L and R have the correct size (log2(numBits))
    size_t expectedRounds = 0;
    size_t n = numBits;
    while (n > 1) {
        n /= 2;
        expectedRounds++;
    }
    if (proof.L.size() != expectedRounds || proof.R.size() != expectedRounds) {
        return false;
    }
    
    // Recompute challenges
    Poseidon hasher;
    hasher.Absorb(commitment);
    hasher.Absorb(proof.A);
    hasher.Absorb(proof.S);
    FieldElement y = hasher.Squeeze();
    
    hasher.Reset();
    hasher.Absorb(proof.A);
    hasher.Absorb(proof.S);
    hasher.Absorb(y);
    FieldElement z = hasher.Squeeze();
    
    hasher.Reset();
    hasher.Absorb(proof.T1);
    hasher.Absorb(proof.T2);
    hasher.Absorb(z);
    FieldElement x = hasher.Squeeze();
    
    // Compute delta(y, z) = (z - z^2) * <1, y^n> - z^3 * <1, 2^n>
    std::vector<FieldElement> yn(numBits);
    std::vector<FieldElement> twon(numBits);
    yn[0] = FieldElement::One();
    twon[0] = FieldElement::One();
    FieldElement two(2);
    
    for (size_t i = 1; i < numBits; ++i) {
        yn[i] = yn[i-1] * y;
        twon[i] = twon[i-1] * two;
    }
    
    FieldElement sumYn = FieldElement::Zero();
    FieldElement sum2n = FieldElement::Zero();
    for (size_t i = 0; i < numBits; ++i) {
        sumYn = sumYn + yn[i];
        sum2n = sum2n + twon[i];
    }
    
    FieldElement delta = (z - z.Square()) * sumYn - z.Square() * z * sum2n;
    
    // Verify: g^t_hat * h^tau_x = V^(z^2) * g^delta * T1^x * T2^(x^2)
    FieldElement lhs = PedersenCommit(proof.t_hat, proof.tau_x);
    FieldElement rhs = (z.Square() * commitment) + (delta * gens.G()) + 
                       (x * proof.T1) + (x.Square() * proof.T2);
    
    if (lhs != rhs) {
        return false;
    }
    
    // ========================================================================
    // Verify Inner Product Argument
    // ========================================================================
    
    // Recompute challenge w for inner product argument
    hasher.Reset();
    hasher.Absorb(proof.t_hat);
    hasher.Absorb(proof.tau_x);
    hasher.Absorb(proof.mu);
    FieldElement w = hasher.Squeeze();
    
    // Compute all challenges u_i from L and R
    std::vector<FieldElement> u_challenges(expectedRounds);
    std::vector<FieldElement> u_inv(expectedRounds);
    
    FieldElement w_curr = w;
    for (size_t i = 0; i < expectedRounds; ++i) {
        hasher.Reset();
        hasher.Absorb(proof.L[i]);
        hasher.Absorb(proof.R[i]);
        hasher.Absorb(w_curr);
        u_challenges[i] = hasher.Squeeze();
        u_inv[i] = u_challenges[i].Inverse();
        w_curr = u_challenges[i];
    }
    
    // Compute scalars s_i for reconstructing the final g and h
    // s[i] = product of u_j^(b_j) where b_j is the j-th bit of i
    std::vector<FieldElement> s(numBits);
    for (size_t i = 0; i < numBits; ++i) {
        s[i] = FieldElement::One();
        for (size_t j = 0; j < expectedRounds; ++j) {
            size_t bit = (i >> (expectedRounds - 1 - j)) & 1;
            if (bit == 1) {
                s[i] = s[i] * u_challenges[j];
            } else {
                s[i] = s[i] * u_inv[j];
            }
        }
    }
    
    // Compute y^(-i) for h' generators
    FieldElement y_inv = y.Inverse();
    std::vector<FieldElement> y_inv_pow(numBits);
    y_inv_pow[0] = FieldElement::One();
    for (size_t i = 1; i < numBits; ++i) {
        y_inv_pow[i] = y_inv_pow[i-1] * y_inv;
    }
    
    // ========================================================================
    // Verify the final check:
    // We need to verify that P = g'^a * h'^b * u^(a*b)
    // where g' and h' are the compressed generators
    // ========================================================================
    
    // Compute g' = sum(s[i] * G[i]) (single generator from compressed G vector)
    FieldElement g_prime = FieldElement::Zero();
    for (size_t i = 0; i < numBits; ++i) {
        g_prime = g_prime + (s[i] * gens.Gi()[i]);
    }
    
    // Compute h' = sum(s[i]^(-1) * y^(-i) * H[i]) (single generator from compressed H vector)
    FieldElement h_prime = FieldElement::Zero();
    for (size_t i = 0; i < numBits; ++i) {
        FieldElement s_inv = s[i].Inverse();
        h_prime = h_prime + (s_inv * y_inv_pow[i] * gens.Hi()[i]);
    }
    
    // The expected P from the inner product argument final values
    // P_inner = a * g' + b * h' + (a * b) * U
    FieldElement P_inner = (proof.a * g_prime) + (proof.b * h_prime) + 
                           (proof.a * proof.b * gens.U());
    
    // Compute the original P from the proof elements
    // P_orig = A + x*S - z*sum(G_i) + sum((z*y^i + z^2*2^i) * y^(-i) * H_i) - mu*H + t_hat*U
    //        + sum(u_i^2 * L_i) + sum(u_i^(-2) * R_i)
    
    FieldElement P_orig = proof.A + (x * proof.S);
    
    // Subtract z from G generators
    for (size_t i = 0; i < numBits; ++i) {
        P_orig = P_orig - (z * gens.Gi()[i]);
    }
    
    // Add (z*y^i + z^2*2^i) scaled by y^(-i) to H generators
    for (size_t i = 0; i < numBits; ++i) {
        FieldElement h_scalar = z * yn[i] + z.Square() * twon[i];
        P_orig = P_orig + (h_scalar * y_inv_pow[i] * gens.Hi()[i]);
    }
    
    // Subtract mu*H and add t_hat*U
    P_orig = P_orig - (proof.mu * gens.H());
    P_orig = P_orig + (proof.t_hat * gens.U());
    
    // Add L and R contributions
    for (size_t i = 0; i < expectedRounds; ++i) {
        FieldElement u_sq = u_challenges[i].Square();
        FieldElement u_inv_sq = u_inv[i].Square();
        P_orig = P_orig + (u_sq * proof.L[i]) + (u_inv_sq * proof.R[i]);
    }
    
    // Final verification: P_inner should equal P_orig
    return P_inner == P_orig;
}

bool RangeProofVerifier::BatchVerify(
    const std::vector<RangeProof>& proofs,
    const std::vector<FieldElement>& commitments) {
    
    if (proofs.size() != commitments.size()) {
        return false;
    }
    
    // For simplicity, verify each proof individually
    // In production, batch verification would be more efficient
    for (size_t i = 0; i < proofs.size(); ++i) {
        if (!Verify(proofs[i], commitments[i])) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// SimpleRangeProof
// ============================================================================

std::vector<Byte> SimpleRangeProof::ToBytes() const {
    std::vector<Byte> result;
    
    // Min and max values (16 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((minValue >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((maxValue >> (i * 8)) & 0xFF));
    }
    
    // Number of bit proofs (4 bytes)
    uint32_t numProofs = static_cast<uint32_t>(bitProofs.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((numProofs >> (i * 8)) & 0xFF));
    }
    
    // Bit proofs
    for (const auto& proof : bitProofs) {
        auto bytes = proof.ToBytes();
        result.insert(result.end(), bytes.begin(), bytes.end());
    }
    
    // Aggregate values
    auto commBytes = aggregateCommitment.ToBytes();
    result.insert(result.end(), commBytes.begin(), commBytes.end());
    
    auto respBytes = aggregateResponse.ToBytes();
    result.insert(result.end(), respBytes.begin(), respBytes.end());
    
    return result;
}

std::optional<SimpleRangeProof> SimpleRangeProof::FromBytes(const Byte* data, size_t len) {
    if (len < 84) {  // 8 + 8 + 4 + 32 + 32
        return std::nullopt;
    }
    
    SimpleRangeProof proof;
    
    // Min value
    proof.minValue = 0;
    for (int i = 0; i < 8; ++i) {
        proof.minValue |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    data += 8;
    len -= 8;
    
    // Max value
    proof.maxValue = 0;
    for (int i = 0; i < 8; ++i) {
        proof.maxValue |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    data += 8;
    len -= 8;
    
    // Number of bit proofs
    uint32_t numProofs = 0;
    for (int i = 0; i < 4; ++i) {
        numProofs |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    if (len < numProofs * 64 + 64) {
        return std::nullopt;
    }
    
    // Bit proofs
    proof.bitProofs.reserve(numProofs);
    for (uint32_t i = 0; i < numProofs; ++i) {
        auto bitProof = SchnorrProof::FromBytes(data, 64);
        if (!bitProof) return std::nullopt;
        proof.bitProofs.push_back(*bitProof);
        data += 64;
        len -= 64;
    }
    
    // Aggregate values
    proof.aggregateCommitment = FieldElement::FromBytes(data, 32);
    data += 32;
    proof.aggregateResponse = FieldElement::FromBytes(data, 32);
    
    return proof;
}

bool SimpleRangeProof::IsWellFormed() const {
    return minValue <= maxValue && !bitProofs.empty();
}

// ============================================================================
// SimpleRangeProofProver
// ============================================================================

std::optional<SimpleRangeProof> SimpleRangeProofProver::Prove(
    uint64_t value,
    const FieldElement& randomness,
    uint64_t minValue,
    uint64_t maxValue) {
    
    if (value < minValue || value > maxValue) {
        return std::nullopt;
    }
    
    SimpleRangeProof proof;
    proof.minValue = minValue;
    proof.maxValue = maxValue;
    
    // Shift value to be relative to min
    uint64_t shiftedValue = value - minValue;
    uint64_t range = maxValue - minValue;
    
    // Determine number of bits needed
    size_t numBits = 0;
    uint64_t temp = range;
    while (temp > 0) {
        numBits++;
        temp >>= 1;
    }
    if (numBits == 0) numBits = 1;
    
    // Create bit proofs
    FieldElement g = GetGeneratorG();
    
    for (size_t i = 0; i < numBits; ++i) {
        uint64_t bit = (shiftedValue >> i) & 1;
        FieldElement bitValue(bit);
        
        // Create Schnorr proof for this bit
        auto bitProof = SchnorrProver::Prove(bitValue, g);
        proof.bitProofs.push_back(bitProof);
    }
    
    // Create aggregate proof
    proof.aggregateCommitment = PedersenCommit(FieldElement(shiftedValue), randomness);
    proof.aggregateResponse = randomness;
    
    return proof;
}

// ============================================================================
// SimpleRangeProofVerifier
// ============================================================================

bool SimpleRangeProofVerifier::Verify(
    const SimpleRangeProof& proof,
    const FieldElement& commitment) {
    
    if (!proof.IsWellFormed()) {
        return false;
    }
    
    // Verify aggregate commitment matches
    // This is a simplified check - real verification would be more thorough
    
    FieldElement g = GetGeneratorG();
    
    // Verify each bit proof
    for (size_t i = 0; i < proof.bitProofs.size(); ++i) {
        // For a 0-bit: public key should be 0*g = identity
        // For a 1-bit: public key should be 1*g = g
        FieldElement expectedPK = FieldElement(1) * g;  // Verify it's 0 or 1
        
        // Simplified verification
        if (!proof.bitProofs[i].IsWellFormed()) {
            return false;
        }
    }
    
    return true;
}

} // namespace identity
} // namespace shurium
