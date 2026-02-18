// SHURIUM - Poseidon Hash Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// ZK-friendly algebraic hash function over the BN254 scalar field

#include "shurium/crypto/poseidon.h"
#include "shurium/crypto/sha256.h"
#include <cstring>

namespace shurium {

// ============================================================================
// Standard Configurations
// ============================================================================

namespace PoseidonParams {
    // 2-to-1 compression: width=3, capacity=1, rate=2
    // Full rounds: 8, Partial rounds: 57
    const PoseidonConfig CONFIG_2_1{3, 8, 57, 1};
    
    // 4-to-1 compression: width=5, capacity=1, rate=4
    // Full rounds: 8, Partial rounds: 60
    const PoseidonConfig CONFIG_4_1{5, 8, 60, 1};
    
    // Standard hash: width=5, capacity=1, rate=4
    const PoseidonConfig CONFIG_STANDARD{5, 8, 60, 1};
}

// ============================================================================
// Round Constants Generation
// ============================================================================

namespace {

/// Generate round constants deterministically using SHA256
/// This follows the Poseidon paper's PRNG approach
std::vector<FieldElement> GenerateRoundConstants(size_t width, size_t totalRounds) {
    std::vector<FieldElement> constants;
    constants.reserve(width * totalRounds);
    
    // Domain separator
    std::array<Byte, 32> seed;
    const char* domain = "SHURIUM_POSEIDON_RC";
    
    // Use SHA256 to generate pseudorandom field elements
    SHA256 hasher;
    hasher.Write(reinterpret_cast<const Byte*>(domain), strlen(domain));
    hasher.Write(reinterpret_cast<const Byte*>(&width), sizeof(width));
    hasher.Write(reinterpret_cast<const Byte*>(&totalRounds), sizeof(totalRounds));
    hasher.Finalize(seed.data());
    
    size_t count = 0;
    size_t needed = width * totalRounds;
    
    while (count < needed) {
        // Hash the seed to get a field element
        hasher.Reset();
        hasher.Write(seed.data(), seed.size());
        hasher.Write(reinterpret_cast<const Byte*>(&count), sizeof(count));
        
        std::array<Byte, 32> hash;
        hasher.Finalize(hash.data());
        
        // Convert to field element (reducing mod p if needed)
        FieldElement fe = FieldElement::FromBytes(hash.data(), hash.size());
        constants.push_back(fe);
        ++count;
        
        // Update seed for next iteration
        seed = hash;
    }
    
    return constants;
}

/// Generate MDS matrix deterministically
/// Uses a Cauchy matrix construction for guaranteed MDS property
std::vector<std::vector<FieldElement>> GenerateMDSMatrix(size_t width) {
    std::vector<std::vector<FieldElement>> mds(width, std::vector<FieldElement>(width));
    
    // Cauchy matrix: M[i][j] = 1 / (x_i + y_j)
    // where x_i = i, y_j = width + j
    // This guarantees the MDS property
    
    for (size_t i = 0; i < width; ++i) {
        for (size_t j = 0; j < width; ++j) {
            // x_i + y_j = i + (width + j)
            FieldElement sum = FieldElement(static_cast<uint64_t>(i + width + j));
            mds[i][j] = sum.Inverse();
        }
    }
    
    return mds;
}

} // anonymous namespace

// ============================================================================
// Poseidon Implementation
// ============================================================================

Poseidon::Poseidon() : Poseidon(PoseidonParams::CONFIG_STANDARD) {}

Poseidon::Poseidon(const PoseidonConfig& config)
    : config_(config)
    , state_(config.width, FieldElement::Zero())
    , absorbPos_(0)
    , squeezing_(false) {
    InitConstants();
}

void Poseidon::InitConstants() {
    // Generate round constants
    auto flatConstants = GenerateRoundConstants(config_.width, config_.totalRounds());
    
    roundConstants_.resize(config_.totalRounds());
    size_t idx = 0;
    for (size_t r = 0; r < config_.totalRounds(); ++r) {
        roundConstants_[r].resize(config_.width);
        for (size_t i = 0; i < config_.width; ++i) {
            roundConstants_[r][i] = flatConstants[idx++];
        }
    }
    
    // Generate MDS matrix
    mdsMatrix_ = GenerateMDSMatrix(config_.width);
}

Poseidon& Poseidon::Reset() {
    state_.assign(config_.width, FieldElement::Zero());
    absorbPos_ = 0;
    squeezing_ = false;
    return *this;
}

FieldElement Poseidon::Sbox(const FieldElement& x) {
    return x.PoseidonSbox();  // x^5
}

void Poseidon::AddRoundConstants(size_t roundIdx) {
    for (size_t i = 0; i < config_.width; ++i) {
        state_[i] += roundConstants_[roundIdx][i];
    }
}

void Poseidon::MixColumns() {
    std::vector<FieldElement> newState(config_.width, FieldElement::Zero());
    
    for (size_t i = 0; i < config_.width; ++i) {
        for (size_t j = 0; j < config_.width; ++j) {
            newState[i] += mdsMatrix_[i][j] * state_[j];
        }
    }
    
    state_ = std::move(newState);
}

void Poseidon::FullRound(size_t roundIdx) {
    // Add round constants
    AddRoundConstants(roundIdx);
    
    // Apply S-box to ALL state elements
    for (size_t i = 0; i < config_.width; ++i) {
        state_[i] = Sbox(state_[i]);
    }
    
    // Mix columns (MDS matrix multiplication)
    MixColumns();
}

void Poseidon::PartialRound(size_t roundIdx) {
    // Add round constants
    AddRoundConstants(roundIdx);
    
    // Apply S-box to FIRST element only
    state_[0] = Sbox(state_[0]);
    
    // Mix columns (MDS matrix multiplication)
    MixColumns();
}

void Poseidon::Permute() {
    size_t roundIdx = 0;
    size_t halfFullRounds = config_.fullRounds / 2;
    
    // First half of full rounds
    for (size_t i = 0; i < halfFullRounds; ++i) {
        FullRound(roundIdx++);
    }
    
    // Partial rounds
    for (size_t i = 0; i < config_.partialRounds; ++i) {
        PartialRound(roundIdx++);
    }
    
    // Second half of full rounds
    for (size_t i = 0; i < halfFullRounds; ++i) {
        FullRound(roundIdx++);
    }
}

Poseidon& Poseidon::Absorb(const FieldElement& element) {
    if (squeezing_) {
        Reset();  // Reset if we were squeezing
    }
    
    // Add element to rate portion of state
    state_[absorbPos_] += element;
    ++absorbPos_;
    
    // If rate is full, apply permutation
    if (absorbPos_ >= config_.rate()) {
        Permute();
        absorbPos_ = 0;
    }
    
    return *this;
}

Poseidon& Poseidon::Absorb(const std::vector<FieldElement>& elements) {
    for (const auto& elem : elements) {
        Absorb(elem);
    }
    return *this;
}

Poseidon& Poseidon::AbsorbBytes(const Byte* data, size_t len) {
    // Convert bytes to field elements (31 bytes per element to stay in field)
    const size_t BYTES_PER_ELEMENT = 31;
    
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = std::min(BYTES_PER_ELEMENT, len - offset);
        
        // Pad with zeros if needed
        std::array<Byte, 32> buffer = {0};
        std::memcpy(buffer.data(), data + offset, chunk);
        
        FieldElement fe = FieldElement::FromBytes(buffer.data(), 32);
        Absorb(fe);
        
        offset += chunk;
    }
    
    // Absorb padding indicator (length)
    Absorb(FieldElement(static_cast<uint64_t>(len)));
    
    return *this;
}

FieldElement Poseidon::Squeeze() {
    if (!squeezing_) {
        // Pad and finalize absorption
        if (absorbPos_ > 0) {
            Permute();
        }
        squeezing_ = true;
        absorbPos_ = 0;
    }
    
    // Get element from rate portion
    FieldElement result = state_[absorbPos_];
    ++absorbPos_;
    
    // If we've exhausted the rate, permute for more output
    if (absorbPos_ >= config_.rate()) {
        Permute();
        absorbPos_ = 0;
    }
    
    return result;
}

std::vector<FieldElement> Poseidon::Squeeze(size_t count) {
    std::vector<FieldElement> results;
    results.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        results.push_back(Squeeze());
    }
    
    return results;
}

// ============================================================================
// Static Convenience Methods
// ============================================================================

FieldElement Poseidon::Hash(const std::vector<FieldElement>& inputs) {
    Poseidon hasher;
    hasher.Absorb(inputs);
    return hasher.Squeeze();
}

FieldElement Poseidon::Hash2(const FieldElement& left, const FieldElement& right) {
    // Use 2-to-1 compression configuration for efficiency
    Poseidon hasher(PoseidonParams::CONFIG_2_1);
    hasher.Absorb(left);
    hasher.Absorb(right);
    return hasher.Squeeze();
}

FieldElement Poseidon::HashBytes(const Byte* data, size_t len) {
    Poseidon hasher;
    hasher.AbsorbBytes(data, len);
    return hasher.Squeeze();
}

std::array<Byte, 32> Poseidon::HashToBytes(const Byte* data, size_t len) {
    FieldElement result = HashBytes(data, len);
    return result.ToBytes();
}

} // namespace shurium
