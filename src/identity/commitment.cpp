// SHURIUM - Cryptographic Commitment Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/identity/commitment.h>
#include <shurium/core/hex.h>
#include <shurium/core/random.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace shurium {
namespace identity {

// ============================================================================
// Commitment Opening
// ============================================================================

std::vector<Byte> CommitmentOpening::ToBytes() const {
    std::vector<Byte> result;
    result.reserve(64 + auxData.size() * 32 + 4);
    
    // Value
    auto valBytes = value.ToBytes();
    result.insert(result.end(), valBytes.begin(), valBytes.end());
    
    // Randomness
    auto randBytes = randomness.ToBytes();
    result.insert(result.end(), randBytes.begin(), randBytes.end());
    
    // Aux data count
    uint32_t auxCount = static_cast<uint32_t>(auxData.size());
    result.push_back(static_cast<Byte>(auxCount & 0xFF));
    result.push_back(static_cast<Byte>((auxCount >> 8) & 0xFF));
    result.push_back(static_cast<Byte>((auxCount >> 16) & 0xFF));
    result.push_back(static_cast<Byte>((auxCount >> 24) & 0xFF));
    
    // Aux data
    for (const auto& aux : auxData) {
        auto auxBytes = aux.ToBytes();
        result.insert(result.end(), auxBytes.begin(), auxBytes.end());
    }
    
    return result;
}

std::optional<CommitmentOpening> CommitmentOpening::FromBytes(const Byte* data, size_t len) {
    if (len < 68) {  // 32 + 32 + 4 minimum
        return std::nullopt;
    }
    
    CommitmentOpening opening;
    
    // Value
    opening.value = FieldElement::FromBytes(data, 32);
    data += 32;
    
    // Randomness
    opening.randomness = FieldElement::FromBytes(data, 32);
    data += 32;
    
    // Aux count
    uint32_t auxCount = static_cast<uint32_t>(data[0]) |
                       (static_cast<uint32_t>(data[1]) << 8) |
                       (static_cast<uint32_t>(data[2]) << 16) |
                       (static_cast<uint32_t>(data[3]) << 24);
    data += 4;
    len -= 68;
    
    // Validate remaining length
    if (len < auxCount * 32) {
        return std::nullopt;
    }
    
    // Aux data
    opening.auxData.reserve(auxCount);
    for (uint32_t i = 0; i < auxCount; ++i) {
        opening.auxData.push_back(FieldElement::FromBytes(data, 32));
        data += 32;
    }
    
    return opening;
}

// ============================================================================
// Pedersen Commitment
// ============================================================================

PedersenCommitment::PedersenCommitment(const FieldElement& element) {
    auto bytes = element.ToBytes();
    std::copy(bytes.begin(), bytes.end(), hash_.begin());
}

PedersenCommitment PedersenCommitment::Commit(const FieldElement& value,
                                              const FieldElement& randomness) {
    // C = Poseidon(value, randomness)
    FieldElement commitment = Poseidon::Hash2(value, randomness);
    return PedersenCommitment(commitment);
}

PedersenCommitment PedersenCommitment::CommitWithRandomness(const FieldElement& value,
                                                            FieldElement& randomness) {
    // Generate random blinding factor
    randomness = GenerateRandomFieldElement();
    return Commit(value, randomness);
}

bool PedersenCommitment::Verify(const CommitmentOpening& opening) const {
    return Verify(opening.value, opening.randomness);
}

bool PedersenCommitment::Verify(const FieldElement& value,
                                const FieldElement& randomness) const {
    PedersenCommitment computed = Commit(value, randomness);
    return *this == computed;
}

FieldElement PedersenCommitment::ToFieldElement() const {
    return FieldElement::FromBytes(hash_.data(), hash_.size());
}

std::string PedersenCommitment::ToHex() const {
    return BytesToHex(hash_.data(), hash_.size());
}

std::optional<PedersenCommitment> PedersenCommitment::FromHex(const std::string& hex) {
    if (hex.size() != SIZE * 2) {
        return std::nullopt;
    }
    
    CommitmentHash hash;
    auto bytes = HexToBytes(hex);
    if (bytes.size() != SIZE) {
        return std::nullopt;
    }
    
    std::copy(bytes.begin(), bytes.end(), hash.begin());
    return PedersenCommitment(hash);
}

bool PedersenCommitment::IsEmpty() const {
    for (const auto& byte : hash_) {
        if (byte != 0) return false;
    }
    return true;
}

bool PedersenCommitment::operator==(const PedersenCommitment& other) const {
    return hash_ == other.hash_;
}

bool PedersenCommitment::operator!=(const PedersenCommitment& other) const {
    return !(*this == other);
}

bool PedersenCommitment::operator<(const PedersenCommitment& other) const {
    return hash_ < other.hash_;
}

// ============================================================================
// Identity Commitment
// ============================================================================

IdentityCommitment IdentityCommitment::Create(const FieldElement& secretKey,
                                              const FieldElement& nullifierKey,
                                              const FieldElement& trapdoor) {
    // Commitment = Poseidon(secretKey, nullifierKey, trapdoor)
    std::vector<FieldElement> inputs = {secretKey, nullifierKey, trapdoor};
    FieldElement commitment = Poseidon::Hash(inputs);
    return IdentityCommitment(PedersenCommitment(commitment));
}

IdentityCommitment IdentityCommitment::Generate(FieldElement& secretKey,
                                                FieldElement& nullifierKey,
                                                FieldElement& trapdoor) {
    // Generate random secrets
    secretKey = GenerateRandomFieldElement();
    nullifierKey = GenerateRandomFieldElement();
    trapdoor = GenerateRandomFieldElement();
    
    return Create(secretKey, nullifierKey, trapdoor);
}

bool IdentityCommitment::Verify(const FieldElement& secretKey,
                                const FieldElement& nullifierKey,
                                const FieldElement& trapdoor) const {
    IdentityCommitment computed = Create(secretKey, nullifierKey, trapdoor);
    return *this == computed;
}

std::optional<IdentityCommitment> IdentityCommitment::FromHex(const std::string& hex) {
    auto commitment = PedersenCommitment::FromHex(hex);
    if (!commitment) {
        return std::nullopt;
    }
    return IdentityCommitment(*commitment);
}

bool IdentityCommitment::operator==(const IdentityCommitment& other) const {
    return commitment_ == other.commitment_;
}

bool IdentityCommitment::operator!=(const IdentityCommitment& other) const {
    return !(*this == other);
}

bool IdentityCommitment::operator<(const IdentityCommitment& other) const {
    return commitment_ < other.commitment_;
}

// ============================================================================
// Vector Commitment (Poseidon Merkle Tree)
// ============================================================================

std::vector<Byte> VectorCommitment::MerkleProof::ToBytes() const {
    std::vector<Byte> result;
    
    // Index (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((index >> (i * 8)) & 0xFF));
    }
    
    // Number of siblings (4 bytes)
    uint32_t numSiblings = static_cast<uint32_t>(siblings.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((numSiblings >> (i * 8)) & 0xFF));
    }
    
    // Siblings
    for (const auto& sibling : siblings) {
        auto bytes = sibling.ToBytes();
        result.insert(result.end(), bytes.begin(), bytes.end());
    }
    
    // Path bits (packed into bytes)
    size_t numPathBytes = (pathBits.size() + 7) / 8;
    result.push_back(static_cast<Byte>(pathBits.size() & 0xFF));
    for (size_t i = 0; i < numPathBytes; ++i) {
        Byte b = 0;
        for (size_t j = 0; j < 8 && i * 8 + j < pathBits.size(); ++j) {
            if (pathBits[i * 8 + j]) {
                b |= (1 << j);
            }
        }
        result.push_back(b);
    }
    
    return result;
}

std::optional<VectorCommitment::MerkleProof> 
VectorCommitment::MerkleProof::FromBytes(const Byte* data, size_t len) {
    if (len < 13) {  // 8 + 4 + 1 minimum
        return std::nullopt;
    }
    
    MerkleProof proof;
    
    // Index
    proof.index = 0;
    for (int i = 0; i < 8; ++i) {
        proof.index |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    data += 8;
    len -= 8;
    
    // Number of siblings
    uint32_t numSiblings = 0;
    for (int i = 0; i < 4; ++i) {
        numSiblings |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    // Validate length
    if (len < numSiblings * 32 + 1) {
        return std::nullopt;
    }
    
    // Siblings
    proof.siblings.reserve(numSiblings);
    for (uint32_t i = 0; i < numSiblings; ++i) {
        proof.siblings.push_back(FieldElement::FromBytes(data, 32));
        data += 32;
        len -= 32;
    }
    
    // Path bits
    size_t numBits = data[0];
    data++;
    len--;
    
    size_t numPathBytes = (numBits + 7) / 8;
    if (len < numPathBytes) {
        return std::nullopt;
    }
    
    proof.pathBits.reserve(numBits);
    for (size_t i = 0; i < numBits; ++i) {
        size_t byteIdx = i / 8;
        size_t bitIdx = i % 8;
        proof.pathBits.push_back((data[byteIdx] >> bitIdx) & 1);
    }
    
    return proof;
}

VectorCommitment::VectorCommitment() 
    : root_(FieldElement::Zero()), size_(0), depth_(0) {
}

VectorCommitment::VectorCommitment(const std::vector<FieldElement>& elements)
    : size_(0), depth_(0) {
    
    if (elements.empty()) {
        root_ = FieldElement::Zero();
        return;
    }
    
    // Calculate depth needed
    depth_ = 0;
    uint64_t capacity = 1;
    while (capacity < elements.size()) {
        depth_++;
        capacity *= 2;
    }
    
    // Initialize leaves
    leaves_ = elements;
    size_ = elements.size();
    
    // Pad to power of 2
    while (leaves_.size() < capacity) {
        leaves_.push_back(DefaultLeaf());
    }
    
    RebuildTree();
}

VectorCommitment::VectorCommitment(const FieldElement& root, uint64_t size)
    : root_(root), size_(size) {
    
    // Calculate depth
    depth_ = 0;
    uint64_t capacity = 1;
    while (capacity < size) {
        depth_++;
        capacity *= 2;
    }
    
    // Note: leaves and levels are not populated in this mode
}

uint64_t VectorCommitment::Add(const FieldElement& element) {
    uint64_t index = size_;
    
    // Expand tree if needed
    uint64_t capacity = leaves_.size();
    
    if (capacity == 0 || size_ >= capacity) {
        // Need to grow
        uint64_t newCapacity = capacity == 0 ? 1 : capacity * 2;
        if (capacity > 0) {
            depth_++;
        }
        
        // Pad existing leaves
        while (leaves_.size() < newCapacity) {
            leaves_.push_back(DefaultLeaf());
        }
    }
    
    // Add element
    leaves_[index] = element;
    size_++;
    
    // Rebuild tree
    RebuildTree();
    
    return index;
}

void VectorCommitment::AddBatch(const std::vector<FieldElement>& elements) {
    for (const auto& elem : elements) {
        Add(elem);
    }
}

std::optional<VectorCommitment::MerkleProof> VectorCommitment::Prove(uint64_t index) const {
    if (index >= size_ || leaves_.empty()) {
        return std::nullopt;
    }
    
    MerkleProof proof;
    proof.index = index;
    proof.siblings.reserve(depth_);
    proof.pathBits.reserve(depth_);
    
    uint64_t idx = index;
    for (uint32_t level = 0; level < depth_; ++level) {
        // Determine sibling
        bool isRight = (idx & 1);
        proof.pathBits.push_back(isRight);
        
        uint64_t siblingIdx = isRight ? idx - 1 : idx + 1;
        
        if (level == 0) {
            // Leaf level
            if (siblingIdx < leaves_.size()) {
                proof.siblings.push_back(leaves_[siblingIdx]);
            } else {
                proof.siblings.push_back(DefaultLeaf());
            }
        } else {
            // Internal level
            size_t levelIdx = level - 1;
            if (levelIdx < levels_.size() && siblingIdx < levels_[levelIdx].size()) {
                proof.siblings.push_back(levels_[levelIdx][siblingIdx]);
            } else {
                proof.siblings.push_back(DefaultLeaf());
            }
        }
        
        idx /= 2;
    }
    
    return proof;
}

bool VectorCommitment::Verify(const FieldElement& element,
                              const MerkleProof& proof) const {
    return VerifyProof(root_, element, proof);
}

bool VectorCommitment::VerifyProof(const FieldElement& root,
                                   const FieldElement& element,
                                   const MerkleProof& proof) {
    if (proof.siblings.size() != proof.pathBits.size()) {
        return false;
    }
    
    FieldElement current = element;
    
    for (size_t i = 0; i < proof.siblings.size(); ++i) {
        if (proof.pathBits[i]) {
            // Current is on the right
            current = HashPair(proof.siblings[i], current);
        } else {
            // Current is on the left
            current = HashPair(current, proof.siblings[i]);
        }
    }
    
    return current == root;
}

std::optional<FieldElement> VectorCommitment::GetElement(uint64_t index) const {
    if (index >= size_ || index >= leaves_.size()) {
        return std::nullopt;
    }
    return leaves_[index];
}

void VectorCommitment::RebuildTree() {
    if (leaves_.empty()) {
        root_ = FieldElement::Zero();
        return;
    }
    
    levels_.clear();
    
    // Build tree from leaves up
    std::vector<FieldElement>* currentLevel = &leaves_;
    
    while (currentLevel->size() > 1) {
        std::vector<FieldElement> nextLevel;
        nextLevel.reserve((currentLevel->size() + 1) / 2);
        
        for (size_t i = 0; i < currentLevel->size(); i += 2) {
            FieldElement left = (*currentLevel)[i];
            FieldElement right = (i + 1 < currentLevel->size()) 
                ? (*currentLevel)[i + 1] 
                : DefaultLeaf();
            nextLevel.push_back(HashPair(left, right));
        }
        
        levels_.push_back(std::move(nextLevel));
        currentLevel = &levels_.back();
    }
    
    root_ = levels_.empty() ? leaves_[0] : levels_.back()[0];
}

FieldElement VectorCommitment::DefaultLeaf() {
    // Use hash of empty bytes as default leaf
    return FieldElement::Zero();
}

FieldElement VectorCommitment::HashPair(const FieldElement& left,
                                        const FieldElement& right) {
    return Poseidon::Hash2(left, right);
}

// ============================================================================
// Utility Functions
// ============================================================================

FieldElement GenerateRandomFieldElement() {
    std::array<Byte, 32> randomBytes;
    GetRandBytes(randomBytes.data(), randomBytes.size());
    
    // Reduce to field
    return FieldElement::FromBytes(randomBytes.data(), randomBytes.size());
}

FieldElement HashToFieldElement(const Byte* data, size_t len) {
    return Poseidon::HashBytes(data, len);
}

} // namespace identity
} // namespace shurium
