// SHURIUM - Nullifier System Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/identity/nullifier.h>
#include <shurium/core/hex.h>
#include <shurium/core/random.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace shurium {
namespace identity {

// ============================================================================
// Domain Separators
// ============================================================================

// Domain separator for UBI claims: Poseidon("SHURIUM_UBI_CLAIM")
const FieldElement Nullifier::DOMAIN_UBI = FieldElement(Uint256(0x01));

// Domain separator for voting: Poseidon("SHURIUM_VOTE")
const FieldElement Nullifier::DOMAIN_VOTE = FieldElement(Uint256(0x02));

// Domain separator for identity refresh: Poseidon("SHURIUM_REFRESH")
const FieldElement Nullifier::DOMAIN_REFRESH = FieldElement(Uint256(0x03));

// ============================================================================
// Nullifier
// ============================================================================

Nullifier::Nullifier(const FieldElement& element, EpochId epoch)
    : epochId_(epoch) {
    auto bytes = element.ToBytes();
    std::copy(bytes.begin(), bytes.end(), hash_.begin());
}

Nullifier Nullifier::Derive(const FieldElement& nullifierKey,
                            EpochId epochId,
                            const FieldElement& domain) {
    // Nullifier = Poseidon(nullifierKey, epochId, domain)
    FieldElement epochField = FieldElement(Uint256(epochId));
    std::vector<FieldElement> inputs;
    inputs.reserve(3);
    inputs.push_back(nullifierKey);
    inputs.push_back(epochField);
    inputs.push_back(domain);
    
    FieldElement hash = Poseidon::Hash(inputs);
    return Nullifier(hash, epochId);
}

FieldElement Nullifier::ToFieldElement() const {
    return FieldElement::FromBytes(hash_.data(), hash_.size());
}

std::string Nullifier::ToHex() const {
    return BytesToHex(hash_.data(), hash_.size());
}

std::optional<Nullifier> Nullifier::FromHex(const std::string& hex, EpochId epoch) {
    if (hex.size() != SIZE * 2) {
        return std::nullopt;
    }
    
    auto bytes = HexToBytes(hex);
    if (bytes.size() != SIZE) {
        return std::nullopt;
    }
    
    NullifierHash hash;
    std::copy(bytes.begin(), bytes.end(), hash.begin());
    return Nullifier(hash, epoch);
}

bool Nullifier::IsEmpty() const {
    for (const auto& byte : hash_) {
        if (byte != 0) return false;
    }
    return true;
}

bool Nullifier::operator==(const Nullifier& other) const {
    return hash_ == other.hash_ && epochId_ == other.epochId_;
}

bool Nullifier::operator!=(const Nullifier& other) const {
    return !(*this == other);
}

bool Nullifier::operator<(const Nullifier& other) const {
    if (epochId_ != other.epochId_) {
        return epochId_ < other.epochId_;
    }
    return hash_ < other.hash_;
}

// ============================================================================
// NullifierSet
// ============================================================================

NullifierSet::NullifierSet() : config_(), mutex_(std::make_unique<std::mutex>()) {}

NullifierSet::NullifierSet(const Config& config) 
    : config_(config), mutex_(std::make_unique<std::mutex>()) {}

NullifierSet::~NullifierSet() = default;

void NullifierSet::SetCurrentEpoch(EpochId epoch) {
    std::lock_guard<std::mutex> lock(*mutex_);
    currentEpoch_ = epoch;
}

bool NullifierSet::Contains(const Nullifier& nullifier) const {
    return Contains(nullifier.GetHash(), nullifier.GetEpoch());
}

bool NullifierSet::Contains(const NullifierHash& hash, EpochId epoch) const {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    auto epochIt = epochNullifiers_.find(epoch);
    if (epochIt == epochNullifiers_.end()) {
        return false;
    }
    
    return epochIt->second.find(hash) != epochIt->second.end();
}

NullifierSet::AddResult NullifierSet::Add(const Nullifier& nullifier) {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    EpochId epoch = nullifier.GetEpoch();
    
    // Validate epoch
    if (!IsValidEpoch(epoch)) {
        return AddResult::InvalidEpoch;
    }
    
    // Check capacity
    if (config_.maxPerEpoch > 0) {
        auto epochIt = epochNullifiers_.find(epoch);
        if (epochIt != epochNullifiers_.end() && 
            epochIt->second.size() >= config_.maxPerEpoch) {
            return AddResult::SetFull;
        }
    }
    
    // Try to add
    auto& epochSet = epochNullifiers_[epoch];
    auto result = epochSet.insert(nullifier.GetHash());
    
    if (!result.second) {
        return AddResult::AlreadyExists;
    }
    
    return AddResult::Success;
}

bool NullifierSet::AddBatch(const std::vector<Nullifier>& nullifiers) {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    // First check all are valid and unique
    std::map<EpochId, std::set<NullifierHash>> toAdd;
    
    for (const auto& nullifier : nullifiers) {
        EpochId epoch = nullifier.GetEpoch();
        
        if (!IsValidEpoch(epoch)) {
            return false;
        }
        
        // Check not already in set
        auto epochIt = epochNullifiers_.find(epoch);
        if (epochIt != epochNullifiers_.end()) {
            if (epochIt->second.find(nullifier.GetHash()) != epochIt->second.end()) {
                return false;  // Duplicate
            }
        }
        
        // Check not in batch already
        if (toAdd[epoch].find(nullifier.GetHash()) != toAdd[epoch].end()) {
            return false;  // Duplicate in batch
        }
        
        toAdd[epoch].insert(nullifier.GetHash());
    }
    
    // Check capacity constraints
    if (config_.maxPerEpoch > 0) {
        for (const auto& [epoch, hashes] : toAdd) {
            size_t existingCount = 0;
            auto epochIt = epochNullifiers_.find(epoch);
            if (epochIt != epochNullifiers_.end()) {
                existingCount = epochIt->second.size();
            }
            
            if (existingCount + hashes.size() > config_.maxPerEpoch) {
                return false;
            }
        }
    }
    
    // All valid, add them
    for (const auto& [epoch, hashes] : toAdd) {
        for (const auto& hash : hashes) {
            epochNullifiers_[epoch].insert(hash);
        }
    }
    
    return true;
}

bool NullifierSet::Remove(const Nullifier& nullifier) {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    auto epochIt = epochNullifiers_.find(nullifier.GetEpoch());
    if (epochIt == epochNullifiers_.end()) {
        return false;
    }
    
    return epochIt->second.erase(nullifier.GetHash()) > 0;
}

uint64_t NullifierSet::CountForEpoch(EpochId epoch) const {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    auto epochIt = epochNullifiers_.find(epoch);
    if (epochIt == epochNullifiers_.end()) {
        return 0;
    }
    
    return epochIt->second.size();
}

uint64_t NullifierSet::TotalCount() const {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    uint64_t total = 0;
    for (const auto& [epoch, nullifiers] : epochNullifiers_) {
        total += nullifiers.size();
    }
    return total;
}

std::vector<EpochId> NullifierSet::GetEpochs() const {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    std::vector<EpochId> epochs;
    epochs.reserve(epochNullifiers_.size());
    
    for (const auto& [epoch, _] : epochNullifiers_) {
        epochs.push_back(epoch);
    }
    
    return epochs;
}

uint64_t NullifierSet::Prune(uint32_t keepEpochs) {
    if (currentEpoch_ < keepEpochs) {
        return 0;
    }
    return PruneOlderThan(currentEpoch_ - keepEpochs);
}

uint64_t NullifierSet::PruneOlderThan(EpochId epoch) {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    uint64_t pruned = 0;
    
    auto it = epochNullifiers_.begin();
    while (it != epochNullifiers_.end()) {
        if (it->first < epoch) {
            pruned += it->second.size();
            it = epochNullifiers_.erase(it);
        } else {
            ++it;
        }
    }
    
    return pruned;
}

void NullifierSet::Clear() {
    std::lock_guard<std::mutex> lock(*mutex_);
    epochNullifiers_.clear();
}

std::vector<Byte> NullifierSet::Serialize() const {
    std::lock_guard<std::mutex> lock(*mutex_);
    
    std::vector<Byte> result;
    
    // Current epoch (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((currentEpoch_ >> (i * 8)) & 0xFF));
    }
    
    // Number of epochs (4 bytes)
    uint32_t numEpochs = static_cast<uint32_t>(epochNullifiers_.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((numEpochs >> (i * 8)) & 0xFF));
    }
    
    // Each epoch
    for (const auto& [epoch, nullifiers] : epochNullifiers_) {
        // Epoch ID (8 bytes)
        for (int i = 0; i < 8; ++i) {
            result.push_back(static_cast<Byte>((epoch >> (i * 8)) & 0xFF));
        }
        
        // Number of nullifiers (4 bytes)
        uint32_t count = static_cast<uint32_t>(nullifiers.size());
        for (int i = 0; i < 4; ++i) {
            result.push_back(static_cast<Byte>((count >> (i * 8)) & 0xFF));
        }
        
        // Nullifier hashes
        for (const auto& hash : nullifiers) {
            result.insert(result.end(), hash.begin(), hash.end());
        }
    }
    
    return result;
}

std::unique_ptr<NullifierSet> NullifierSet::Deserialize(const Byte* data, size_t len,
                                                       const Config& config) {
    if (len < 12) {  // 8 + 4 minimum
        return nullptr;
    }
    
    auto set = std::make_unique<NullifierSet>(config);
    
    // Current epoch
    set->currentEpoch_ = 0;
    for (int i = 0; i < 8; ++i) {
        set->currentEpoch_ |= static_cast<EpochId>(data[i]) << (i * 8);
    }
    data += 8;
    len -= 8;
    
    // Number of epochs
    uint32_t numEpochs = 0;
    for (int i = 0; i < 4; ++i) {
        numEpochs |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    // Each epoch
    for (uint32_t e = 0; e < numEpochs; ++e) {
        if (len < 12) {
            return nullptr;
        }
        
        // Epoch ID
        EpochId epoch = 0;
        for (int i = 0; i < 8; ++i) {
            epoch |= static_cast<EpochId>(data[i]) << (i * 8);
        }
        data += 8;
        len -= 8;
        
        // Number of nullifiers
        uint32_t count = 0;
        for (int i = 0; i < 4; ++i) {
            count |= static_cast<uint32_t>(data[i]) << (i * 8);
        }
        data += 4;
        len -= 4;
        
        // Validate length
        if (len < count * 32) {
            return nullptr;
        }
        
        // Nullifier hashes
        for (uint32_t n = 0; n < count; ++n) {
            NullifierHash hash;
            std::copy(data, data + 32, hash.begin());
            set->epochNullifiers_[epoch].insert(hash);
            data += 32;
            len -= 32;
        }
    }
    
    return set;
}

bool NullifierSet::IsValidEpoch(EpochId epoch) const {
    // Check not too old
    if (epoch + config_.maxEpochHistory < currentEpoch_) {
        return false;
    }
    
    // Check not too far in future
    if (!config_.allowFutureEpochs && epoch > currentEpoch_) {
        return false;
    }
    
    if (config_.allowFutureEpochs && 
        epoch > currentEpoch_ + config_.maxFutureOffset) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Epoch Utilities
// ============================================================================

EpochId CalculateEpoch(int64_t timestamp, int64_t epochDuration, int64_t genesisTime) {
    if (timestamp < genesisTime) {
        return 0;
    }
    
    return static_cast<EpochId>((timestamp - genesisTime) / epochDuration);
}

int64_t GetEpochStartTime(EpochId epoch, int64_t epochDuration, int64_t genesisTime) {
    return genesisTime + static_cast<int64_t>(epoch) * epochDuration;
}

int64_t GetEpochEndTime(EpochId epoch, int64_t epochDuration, int64_t genesisTime) {
    return GetEpochStartTime(epoch + 1, epochDuration, genesisTime) - 1;
}

bool IsInEpoch(int64_t timestamp, EpochId epoch, int64_t epochDuration, int64_t genesisTime) {
    int64_t start = GetEpochStartTime(epoch, epochDuration, genesisTime);
    int64_t end = GetEpochEndTime(epoch, epochDuration, genesisTime);
    return timestamp >= start && timestamp <= end;
}

// ============================================================================
// NullifierProof
// ============================================================================

std::vector<Byte> NullifierProof::ToBytes() const {
    std::vector<Byte> result;
    
    // Nullifier
    auto nullifierBytes = nullifier.GetHash();
    result.insert(result.end(), nullifierBytes.begin(), nullifierBytes.end());
    
    // Epoch (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((epoch >> (i * 8)) & 0xFF));
    }
    
    // Merkle proof length (4 bytes)
    uint32_t proofLen = static_cast<uint32_t>(merkleProof.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((proofLen >> (i * 8)) & 0xFF));
    }
    
    // Merkle proof elements
    for (const auto& elem : merkleProof) {
        auto bytes = elem.ToBytes();
        result.insert(result.end(), bytes.begin(), bytes.end());
    }
    
    // Path bits (1 byte length + packed bits)
    result.push_back(static_cast<Byte>(pathBits.size()));
    size_t numBytes = (pathBits.size() + 7) / 8;
    for (size_t i = 0; i < numBytes; ++i) {
        Byte b = 0;
        for (size_t j = 0; j < 8 && i * 8 + j < pathBits.size(); ++j) {
            if (pathBits[i * 8 + j]) {
                b |= (1 << j);
            }
        }
        result.push_back(b);
    }
    
    // ZK proof data length (4 bytes)
    uint32_t zkLen = static_cast<uint32_t>(zkProofData.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((zkLen >> (i * 8)) & 0xFF));
    }
    
    // ZK proof data
    result.insert(result.end(), zkProofData.begin(), zkProofData.end());
    
    return result;
}

std::optional<NullifierProof> NullifierProof::FromBytes(const Byte* data, size_t len) {
    if (len < 45) {  // 32 + 8 + 4 + 1 minimum
        return std::nullopt;
    }
    
    NullifierProof proof;
    
    // Nullifier
    NullifierHash hash;
    std::copy(data, data + 32, hash.begin());
    data += 32;
    len -= 32;
    
    // Epoch
    proof.epoch = 0;
    for (int i = 0; i < 8; ++i) {
        proof.epoch |= static_cast<EpochId>(data[i]) << (i * 8);
    }
    data += 8;
    len -= 8;
    
    proof.nullifier = Nullifier(hash, proof.epoch);
    
    // Merkle proof length
    uint32_t proofLen = 0;
    for (int i = 0; i < 4; ++i) {
        proofLen |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    // Validate
    if (len < proofLen * 32 + 1) {
        return std::nullopt;
    }
    
    // Merkle proof elements
    proof.merkleProof.reserve(proofLen);
    for (uint32_t i = 0; i < proofLen; ++i) {
        proof.merkleProof.push_back(FieldElement::FromBytes(data, 32));
        data += 32;
        len -= 32;
    }
    
    // Path bits
    size_t numBits = data[0];
    data++;
    len--;
    
    size_t numBytes = (numBits + 7) / 8;
    if (len < numBytes + 4) {
        return std::nullopt;
    }
    
    proof.pathBits.reserve(numBits);
    for (size_t i = 0; i < numBits; ++i) {
        size_t byteIdx = i / 8;
        size_t bitIdx = i % 8;
        proof.pathBits.push_back((data[byteIdx] >> bitIdx) & 1);
    }
    data += numBytes;
    len -= numBytes;
    
    // ZK proof data length
    uint32_t zkLen = 0;
    for (int i = 0; i < 4; ++i) {
        zkLen |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    if (len < zkLen) {
        return std::nullopt;
    }
    
    // ZK proof data
    proof.zkProofData.assign(data, data + zkLen);
    
    return proof;
}

bool NullifierProof::IsWellFormed() const {
    // Check nullifier is not empty
    if (nullifier.IsEmpty()) {
        return false;
    }
    
    // Check merkle proof and path bits match
    if (merkleProof.size() != pathBits.size()) {
        return false;
    }
    
    return true;
}

} // namespace identity
} // namespace shurium
