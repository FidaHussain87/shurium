// SHURIUM - Identity Management Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/identity/identity.h>
#include <shurium/core/hex.h>
#include <shurium/core/random.h>
#include <shurium/crypto/sha256.h>
#include <shurium/wallet/keystore.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace shurium {
namespace identity {

// ============================================================================
// IdentitySecrets
// ============================================================================

IdentitySecrets IdentitySecrets::Generate() {
    std::array<Byte, 32> seed;
    GetRandBytes(seed.data(), seed.size());
    return FromMasterSeed(seed);
}

IdentitySecrets IdentitySecrets::FromMasterSeed(const std::array<Byte, 32>& seed) {
    IdentitySecrets secrets;
    secrets.masterSeed = seed;
    
    // Derive keys using domain-separated hashing
    // secretKey = H(seed || "nexus_secret_key")
    SHA256 hasher;
    hasher.Write(seed.data(), seed.size());
    const char* domain1 = "nexus_secret_key";
    hasher.Write(reinterpret_cast<const Byte*>(domain1), strlen(domain1));
    std::array<Byte, 32> skBytes;
    hasher.Finalize(skBytes.data());
    secrets.secretKey = FieldElement::FromBytes(skBytes.data(), skBytes.size());
    
    // nullifierKey = H(seed || "nexus_nullifier_key")
    hasher.Reset();
    hasher.Write(seed.data(), seed.size());
    const char* domain2 = "nexus_nullifier_key";
    hasher.Write(reinterpret_cast<const Byte*>(domain2), strlen(domain2));
    std::array<Byte, 32> nkBytes;
    hasher.Finalize(nkBytes.data());
    secrets.nullifierKey = FieldElement::FromBytes(nkBytes.data(), nkBytes.size());
    
    // trapdoor = H(seed || "nexus_trapdoor")
    hasher.Reset();
    hasher.Write(seed.data(), seed.size());
    const char* domain3 = "nexus_trapdoor";
    hasher.Write(reinterpret_cast<const Byte*>(domain3), strlen(domain3));
    std::array<Byte, 32> tdBytes;
    hasher.Finalize(tdBytes.data());
    secrets.trapdoor = FieldElement::FromBytes(tdBytes.data(), tdBytes.size());
    
    secrets.treeIndex = 0;  // Set during registration
    
    return secrets;
}

IdentityCommitment IdentitySecrets::GetCommitment() const {
    return IdentityCommitment::Create(secretKey, nullifierKey, trapdoor);
}

Nullifier IdentitySecrets::DeriveNullifier(EpochId epoch,
                                           const FieldElement& domain) const {
    return Nullifier::Derive(nullifierKey, epoch, domain);
}

std::vector<Byte> IdentitySecrets::Encrypt(const std::array<Byte, 32>& key) const {
    // Use AES-256-GCM for authenticated encryption
    // Format: nonce (12 bytes) || ciphertext || auth_tag (16 bytes)
    
    // Prepare plaintext: master_seed (32) || tree_index (8) = 40 bytes
    std::vector<Byte> plaintext;
    plaintext.reserve(40);
    
    // Master seed
    plaintext.insert(plaintext.end(), masterSeed.begin(), masterSeed.end());
    
    // Tree index (little-endian)
    for (int i = 0; i < 8; ++i) {
        plaintext.push_back(static_cast<Byte>((treeIndex >> (i * 8)) & 0xFF));
    }
    
    // Generate random nonce
    std::array<Byte, wallet::AES_NONCE_SIZE> nonce = wallet::CryptoEngine::GenerateNonce();
    
    // Additional authenticated data: version byte for future compatibility
    std::vector<Byte> aad = {0x01};  // Version 1
    
    // Encrypt using AES-256-GCM
    std::vector<Byte> ciphertext = wallet::CryptoEngine::Encrypt(key, nonce, plaintext, aad);
    
    // Securely clear plaintext
    std::fill(plaintext.begin(), plaintext.end(), 0);
    
    // Build result: nonce || ciphertext (includes auth tag)
    std::vector<Byte> result;
    result.reserve(wallet::AES_NONCE_SIZE + ciphertext.size() + 1);
    
    // Version byte first
    result.push_back(0x01);
    
    // Nonce
    result.insert(result.end(), nonce.begin(), nonce.end());
    
    // Ciphertext with auth tag
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    
    return result;
}

std::optional<IdentitySecrets> IdentitySecrets::Decrypt(const Byte* data, size_t len,
                                                         const std::array<Byte, 32>& key) {
    // Format: version (1) || nonce (12) || ciphertext (40) || auth_tag (16) = 69 bytes minimum
    constexpr size_t MIN_LEN = 1 + wallet::AES_NONCE_SIZE + 40 + wallet::AES_TAG_SIZE;
    
    if (len < MIN_LEN) {
        return std::nullopt;
    }
    
    // Check version
    Byte version = data[0];
    if (version != 0x01) {
        // Unknown version - try legacy XOR decryption for backwards compatibility
        if (len >= 40) {
            // Legacy format: XOR'd seed (32) || XOR'd index (8)
            std::array<Byte, 32> seed;
            for (size_t i = 0; i < 32; ++i) {
                seed[i] = data[i] ^ key[i];
            }
            
            IdentitySecrets secrets = FromMasterSeed(seed);
            
            secrets.treeIndex = 0;
            for (int i = 0; i < 8; ++i) {
                secrets.treeIndex |= static_cast<uint64_t>(data[32 + i] ^ key[i + 16]) << (i * 8);
            }
            
            // Securely clear seed
            std::fill(seed.begin(), seed.end(), 0);
            
            return secrets;
        }
        return std::nullopt;
    }
    
    // Extract nonce
    std::array<Byte, wallet::AES_NONCE_SIZE> nonce;
    std::memcpy(nonce.data(), data + 1, wallet::AES_NONCE_SIZE);
    
    // Extract ciphertext with auth tag
    size_t ciphertextLen = len - 1 - wallet::AES_NONCE_SIZE;
    std::vector<Byte> ciphertext(data + 1 + wallet::AES_NONCE_SIZE,
                                  data + len);
    
    // Additional authenticated data
    std::vector<Byte> aad = {0x01};  // Version 1
    
    // Decrypt using AES-256-GCM
    auto plaintext = wallet::CryptoEngine::Decrypt(key, nonce, ciphertext, aad);
    
    if (!plaintext || plaintext->size() < 40) {
        // Authentication failed or invalid plaintext
        return std::nullopt;
    }
    
    // Extract master seed
    std::array<Byte, 32> seed;
    std::memcpy(seed.data(), plaintext->data(), 32);
    
    // Derive secrets from seed
    IdentitySecrets secrets = FromMasterSeed(seed);
    
    // Extract tree index (little-endian)
    secrets.treeIndex = 0;
    for (int i = 0; i < 8; ++i) {
        secrets.treeIndex |= static_cast<uint64_t>((*plaintext)[32 + i]) << (i * 8);
    }
    
    // Securely clear sensitive data
    std::fill(seed.begin(), seed.end(), 0);
    std::fill(plaintext->begin(), plaintext->end(), 0);
    
    return secrets;
}

void IdentitySecrets::Clear() {
    // Securely clear sensitive data
    std::fill(masterSeed.begin(), masterSeed.end(), 0);
    secretKey = FieldElement::Zero();
    nullifierKey = FieldElement::Zero();
    trapdoor = FieldElement::Zero();
    treeIndex = 0;
}

IdentitySecrets::~IdentitySecrets() {
    Clear();
}

// ============================================================================
// IdentityRecord
// ============================================================================

IdentityRecord IdentityRecord::Create(const IdentityCommitment& commitment,
                                      uint32_t height,
                                      int64_t timestamp) {
    IdentityRecord record;
    record.commitment = commitment;
    record.id = ComputeIdentityId(commitment);
    record.status = IdentityStatus::Pending;
    record.registrationHeight = height;
    record.lastUpdateHeight = height;
    record.expirationHeight = 0;  // Never expires by default
    record.treeIndex = 0;  // Set during registration
    record.registrationTime = timestamp;
    return record;
}

bool IdentityRecord::CanClaimUBI(uint32_t currentHeight) const {
    if (status != IdentityStatus::Active) {
        return false;
    }
    
    if (expirationHeight > 0 && currentHeight >= expirationHeight) {
        return false;
    }
    
    return true;
}

std::vector<Byte> IdentityRecord::ToBytes() const {
    std::vector<Byte> result;
    result.reserve(128);
    
    // ID (32 bytes)
    result.insert(result.end(), id.begin(), id.end());
    
    // Commitment (32 bytes)
    const auto& commitHash = commitment.GetHash();
    result.insert(result.end(), commitHash.begin(), commitHash.end());
    
    // Status (1 byte)
    result.push_back(static_cast<Byte>(status));
    
    // Heights (4 bytes each)
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((registrationHeight >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((lastUpdateHeight >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((expirationHeight >> (i * 8)) & 0xFF));
    }
    
    // Tree index (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((treeIndex >> (i * 8)) & 0xFF));
    }
    
    // Registration time (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((registrationTime >> (i * 8)) & 0xFF));
    }
    
    return result;
}

std::optional<IdentityRecord> IdentityRecord::FromBytes(const Byte* data, size_t len) {
    if (len < 97) {  // 32 + 32 + 1 + 12 + 8 + 8 + 4 = 97
        return std::nullopt;
    }
    
    IdentityRecord record;
    
    // ID
    std::copy(data, data + 32, record.id.begin());
    data += 32;
    
    // Commitment
    CommitmentHash commitHash;
    std::copy(data, data + 32, commitHash.begin());
    record.commitment = IdentityCommitment(PedersenCommitment(commitHash));
    data += 32;
    
    // Status
    record.status = static_cast<IdentityStatus>(data[0]);
    data++;
    
    // Heights
    record.registrationHeight = 0;
    for (int i = 0; i < 4; ++i) {
        record.registrationHeight |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    
    record.lastUpdateHeight = 0;
    for (int i = 0; i < 4; ++i) {
        record.lastUpdateHeight |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    
    record.expirationHeight = 0;
    for (int i = 0; i < 4; ++i) {
        record.expirationHeight |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    
    // Tree index
    record.treeIndex = 0;
    for (int i = 0; i < 8; ++i) {
        record.treeIndex |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    data += 8;
    
    // Registration time
    record.registrationTime = 0;
    for (int i = 0; i < 8; ++i) {
        record.registrationTime |= static_cast<int64_t>(data[i]) << (i * 8);
    }
    
    return record;
}

bool IdentityRecord::operator==(const IdentityRecord& other) const {
    return id == other.id;
}

bool IdentityRecord::operator<(const IdentityRecord& other) const {
    return id < other.id;
}

// ============================================================================
// RegistrationRequest
// ============================================================================

bool RegistrationRequest::IsValid() const {
    if (commitment.IsEmpty()) {
        return false;
    }
    
    if (timestamp <= 0) {
        return false;
    }
    
    return true;
}

std::vector<Byte> RegistrationRequest::ToBytes() const {
    std::vector<Byte> result;
    
    // Commitment (32 bytes)
    const auto& commitHash = commitment.GetHash();
    result.insert(result.end(), commitHash.begin(), commitHash.end());
    
    // Has proof? (1 byte)
    result.push_back(registrationProof.has_value() ? 1 : 0);
    
    // Proof (if present)
    if (registrationProof) {
        auto proofBytes = registrationProof->ToBytes();
        uint32_t proofLen = static_cast<uint32_t>(proofBytes.size());
        for (int i = 0; i < 4; ++i) {
            result.push_back(static_cast<Byte>((proofLen >> (i * 8)) & 0xFF));
        }
        result.insert(result.end(), proofBytes.begin(), proofBytes.end());
    }
    
    // Verification data length + data
    uint32_t verifyLen = static_cast<uint32_t>(verificationData.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((verifyLen >> (i * 8)) & 0xFF));
    }
    result.insert(result.end(), verificationData.begin(), verificationData.end());
    
    // Timestamp (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((timestamp >> (i * 8)) & 0xFF));
    }
    
    return result;
}

std::optional<RegistrationRequest> RegistrationRequest::FromBytes(const Byte* data, size_t len) {
    if (len < 45) {  // 32 + 1 + 4 + 8 minimum
        return std::nullopt;
    }
    
    RegistrationRequest request;
    
    // Commitment
    CommitmentHash commitHash;
    std::copy(data, data + 32, commitHash.begin());
    request.commitment = IdentityCommitment(PedersenCommitment(commitHash));
    data += 32;
    len -= 32;
    
    // Has proof?
    bool hasProof = data[0] != 0;
    data++;
    len--;
    
    // Proof
    if (hasProof) {
        if (len < 4) return std::nullopt;
        
        uint32_t proofLen = 0;
        for (int i = 0; i < 4; ++i) {
            proofLen |= static_cast<uint32_t>(data[i]) << (i * 8);
        }
        data += 4;
        len -= 4;
        
        if (len < proofLen) return std::nullopt;
        
        auto proof = ZKProof::FromBytes(data, proofLen);
        if (proof) {
            request.registrationProof = std::move(*proof);
        }
        data += proofLen;
        len -= proofLen;
    }
    
    // Verification data
    if (len < 4) return std::nullopt;
    
    uint32_t verifyLen = 0;
    for (int i = 0; i < 4; ++i) {
        verifyLen |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    if (len < verifyLen + 8) return std::nullopt;
    
    request.verificationData.assign(data, data + verifyLen);
    data += verifyLen;
    len -= verifyLen;
    
    // Timestamp
    request.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        request.timestamp |= static_cast<int64_t>(data[i]) << (i * 8);
    }
    
    return request;
}

// ============================================================================
// UBIClaim
// ============================================================================

bool UBIClaim::IsValid() const {
    if (nullifier.IsEmpty()) {
        return false;
    }
    
    if (recipientScript.empty()) {
        return false;
    }
    
    if (!proof.IsValid()) {
        return false;
    }
    
    return true;
}

bool UBIClaim::Verify(const FieldElement& identityRoot,
                      const NullifierSet& usedNullifiers) const {
    if (!IsValid()) {
        return false;
    }
    
    // Check epoch matches
    if (nullifier.GetEpoch() != epoch) {
        return false;
    }
    
    // Check nullifier not used
    if (usedNullifiers.Contains(nullifier)) {
        return false;
    }
    
    // Verify ZK proof
    return proof.Verify(identityRoot, usedNullifiers);
}

std::vector<Byte> UBIClaim::ToBytes() const {
    std::vector<Byte> result;
    
    // Nullifier (32 bytes)
    const auto& nullHash = nullifier.GetHash();
    result.insert(result.end(), nullHash.begin(), nullHash.end());
    
    // Epoch (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((epoch >> (i * 8)) & 0xFF));
    }
    
    // Recipient script length + data
    uint32_t scriptLen = static_cast<uint32_t>(recipientScript.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((scriptLen >> (i * 8)) & 0xFF));
    }
    result.insert(result.end(), recipientScript.begin(), recipientScript.end());
    
    // Proof
    auto proofBytes = proof.ToBytes();
    uint32_t proofLen = static_cast<uint32_t>(proofBytes.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((proofLen >> (i * 8)) & 0xFF));
    }
    result.insert(result.end(), proofBytes.begin(), proofBytes.end());
    
    // Timestamp
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((timestamp >> (i * 8)) & 0xFF));
    }
    
    return result;
}

std::optional<UBIClaim> UBIClaim::FromBytes(const Byte* data, size_t len) {
    if (len < 56) {  // 32 + 8 + 4 + 4 + 8 minimum
        return std::nullopt;
    }
    
    UBIClaim claim;
    
    // Nullifier
    NullifierHash nullHash;
    std::copy(data, data + 32, nullHash.begin());
    data += 32;
    len -= 32;
    
    // Epoch
    claim.epoch = 0;
    for (int i = 0; i < 8; ++i) {
        claim.epoch |= static_cast<EpochId>(data[i]) << (i * 8);
    }
    data += 8;
    len -= 8;
    
    claim.nullifier = Nullifier(nullHash, claim.epoch);
    
    // Recipient script
    if (len < 4) return std::nullopt;
    
    uint32_t scriptLen = 0;
    for (int i = 0; i < 4; ++i) {
        scriptLen |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    if (len < scriptLen + 12) return std::nullopt;
    
    claim.recipientScript.assign(data, data + scriptLen);
    data += scriptLen;
    len -= scriptLen;
    
    // Proof
    uint32_t proofLen = 0;
    for (int i = 0; i < 4; ++i) {
        proofLen |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    data += 4;
    len -= 4;
    
    if (len < proofLen + 8) return std::nullopt;
    
    auto proof = IdentityProof::FromBytes(data, proofLen);
    if (!proof) return std::nullopt;
    claim.proof = std::move(*proof);
    data += proofLen;
    len -= proofLen;
    
    // Timestamp
    claim.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        claim.timestamp |= static_cast<int64_t>(data[i]) << (i * 8);
    }
    
    return claim;
}

// ============================================================================
// IdentityManager
// ============================================================================

IdentityManager::IdentityManager() 
    : config_(),
      verifier_(std::make_unique<ProofVerifier>()) {
}

IdentityManager::IdentityManager(const Config& config)
    : config_(config),
      verifier_(std::make_unique<ProofVerifier>()) {
}

IdentityManager::~IdentityManager() = default;

bool IdentityManager::Initialize(const Byte* data, size_t len) {
    // Deserialize state
    // Format: tree root (32) + identity count (8) + identities + nullifier set
    if (len < 40) {
        return false;
    }
    
    // Would deserialize full state here
    // For now, just reset
    identities_.clear();
    idToCommitment_.clear();
    
    return true;
}

void IdentityManager::SetBlockContext(uint32_t height, int64_t timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentHeight_ = height;
    currentTime_ = timestamp;
    
    EpochId newEpoch = CalculateEpoch(timestamp, config_.epochDuration, config_.genesisTime);
    if (newEpoch != currentEpoch_) {
        currentEpoch_ = newEpoch;
        nullifierSet_.SetCurrentEpoch(newEpoch);
    }
}

EpochId IdentityManager::GetCurrentEpoch() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentEpoch_;
}

void IdentityManager::AdvanceEpoch(EpochId newEpoch) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentEpoch_ = newEpoch;
    nullifierSet_.SetCurrentEpoch(newEpoch);
}

std::optional<IdentityRecord> IdentityManager::RegisterIdentity(
    const RegistrationRequest& request) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate request
    if (!ValidateRegistration(request)) {
        return std::nullopt;
    }
    
    // Check not already registered
    if (IsCommitmentRegistered(request.commitment)) {
        return std::nullopt;
    }
    
    // Check capacity
    if (config_.maxIdentities > 0 && identities_.size() >= config_.maxIdentities) {
        return std::nullopt;
    }
    
    // Create record
    IdentityRecord record = IdentityRecord::Create(
        request.commitment, currentHeight_, currentTime_);
    
    // Add to identity tree
    record.treeIndex = AddToTree(request.commitment);
    
    // Set activation delay
    if (config_.activationDelay > 0) {
        record.status = IdentityStatus::Pending;
    } else {
        record.status = IdentityStatus::Active;
    }
    
    // Set expiration if configured
    if (config_.identityLifetime > 0) {
        record.expirationHeight = currentHeight_ + config_.identityLifetime;
    }
    
    // Store
    identities_[request.commitment.GetHash()] = record;
    idToCommitment_[record.id] = request.commitment.GetHash();
    
    return record;
}

bool IdentityManager::IsCommitmentRegistered(const IdentityCommitment& commitment) const {
    return identities_.find(commitment.GetHash()) != identities_.end();
}

std::optional<IdentityRecord> IdentityManager::GetIdentity(
    const IdentityCommitment& commitment) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = identities_.find(commitment.GetHash());
    if (it == identities_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<IdentityRecord> IdentityManager::GetIdentityById(const Hash256& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto idIt = idToCommitment_.find(id);
    if (idIt == idToCommitment_.end()) {
        return std::nullopt;
    }
    
    auto it = identities_.find(idIt->second);
    if (it == identities_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<IdentityRecord> IdentityManager::GetIdentityByIndex(uint64_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [_, record] : identities_) {
        if (record.treeIndex == index) {
            return record;
        }
    }
    return std::nullopt;
}

bool IdentityManager::UpdateIdentityStatus(const Hash256& id, IdentityStatus newStatus) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto idIt = idToCommitment_.find(id);
    if (idIt == idToCommitment_.end()) {
        return false;
    }
    
    auto it = identities_.find(idIt->second);
    if (it == identities_.end()) {
        return false;
    }
    
    it->second.status = newStatus;
    it->second.lastUpdateHeight = currentHeight_;
    return true;
}

std::vector<IdentityRecord> IdentityManager::GetIdentitiesByStatus(
    IdentityStatus status) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<IdentityRecord> result;
    for (const auto& [_, record] : identities_) {
        if (record.status == status) {
            result.push_back(record);
        }
    }
    return result;
}

FieldElement IdentityManager::GetIdentityRoot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return identityTree_.GetRoot();
}

std::optional<VectorCommitment::MerkleProof> IdentityManager::GetMembershipProof(
    const IdentityCommitment& commitment) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = identities_.find(commitment.GetHash());
    if (it == identities_.end()) {
        return std::nullopt;
    }
    
    return identityTree_.Prove(it->second.treeIndex);
}

bool IdentityManager::VerifyMembershipProof(const IdentityCommitment& commitment,
                                            const VectorCommitment::MerkleProof& proof) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return identityTree_.Verify(commitment.ToFieldElement(), proof);
}

bool IdentityManager::ProcessUBIClaim(const UBIClaim& claim) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Verify the claim
    FieldElement root = identityTree_.GetRoot();
    if (!claim.Verify(root, nullifierSet_)) {
        return false;
    }
    
    // Add nullifier to prevent double claims
    auto result = nullifierSet_.Add(claim.nullifier);
    if (result != NullifierSet::AddResult::Success) {
        return false;
    }
    
    return true;
}

bool IdentityManager::IsNullifierUsed(const Nullifier& nullifier) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nullifierSet_.Contains(nullifier);
}

uint64_t IdentityManager::GetClaimsThisEpoch() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nullifierSet_.CountForEpoch(currentEpoch_);
}

IdentityManager::Stats IdentityManager::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.totalIdentities = identities_.size();
    stats.activeIdentities = 0;
    stats.pendingIdentities = 0;
    stats.revokedIdentities = 0;
    
    for (const auto& [_, record] : identities_) {
        switch (record.status) {
            case IdentityStatus::Active:
                stats.activeIdentities++;
                break;
            case IdentityStatus::Pending:
                stats.pendingIdentities++;
                break;
            case IdentityStatus::Revoked:
                stats.revokedIdentities++;
                break;
            default:
                break;
        }
    }
    
    stats.claimsThisEpoch = nullifierSet_.CountForEpoch(currentEpoch_);
    stats.currentEpoch = currentEpoch_;
    
    return stats;
}

uint64_t IdentityManager::GetIdentityCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return identities_.size();
}

std::vector<Byte> IdentityManager::Serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Byte> result;
    
    // Current height and time
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((currentHeight_ >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((currentTime_ >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((currentEpoch_ >> (i * 8)) & 0xFF));
    }
    
    // Identity count
    uint64_t count = identities_.size();
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<Byte>((count >> (i * 8)) & 0xFF));
    }
    
    // Identities
    for (const auto& [_, record] : identities_) {
        auto recordBytes = record.ToBytes();
        uint32_t len = static_cast<uint32_t>(recordBytes.size());
        for (int i = 0; i < 4; ++i) {
            result.push_back(static_cast<Byte>((len >> (i * 8)) & 0xFF));
        }
        result.insert(result.end(), recordBytes.begin(), recordBytes.end());
    }
    
    // Nullifier set
    auto nullBytes = nullifierSet_.Serialize();
    uint32_t nullLen = static_cast<uint32_t>(nullBytes.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<Byte>((nullLen >> (i * 8)) & 0xFF));
    }
    result.insert(result.end(), nullBytes.begin(), nullBytes.end());
    
    return result;
}

Hash256 IdentityManager::GetStateHash() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SHA256 hasher;
    
    // Hash the identity tree root
    auto rootBytes = identityTree_.GetRoot().ToBytes();
    hasher.Write(rootBytes.data(), rootBytes.size());
    
    // Hash identity count
    uint64_t count = identities_.size();
    Byte countBytes[8];
    for (int i = 0; i < 8; ++i) {
        countBytes[i] = static_cast<Byte>((count >> (i * 8)) & 0xFF);
    }
    hasher.Write(countBytes, 8);
    
    // Hash nullifier count
    uint64_t nullCount = nullifierSet_.TotalCount();
    for (int i = 0; i < 8; ++i) {
        countBytes[i] = static_cast<Byte>((nullCount >> (i * 8)) & 0xFF);
    }
    hasher.Write(countBytes, 8);
    
    Hash256 result;
    hasher.Finalize(result.data());
    return result;
}

uint64_t IdentityManager::AddToTree(const IdentityCommitment& commitment) {
    return identityTree_.Add(commitment.ToFieldElement());
}

bool IdentityManager::ValidateRegistration(const RegistrationRequest& request) const {
    if (!request.IsValid()) {
        return false;
    }
    
    // Check registration proof if required
    if (config_.requireRegistrationProof && !request.registrationProof) {
        return false;
    }
    
    // Validate proof if present
    if (request.registrationProof) {
        // Would verify the proof here
    }
    
    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string IdentityStatusToString(IdentityStatus status) {
    switch (status) {
        case IdentityStatus::Pending: return "Pending";
        case IdentityStatus::Active: return "Active";
        case IdentityStatus::Suspended: return "Suspended";
        case IdentityStatus::Revoked: return "Revoked";
        case IdentityStatus::Expired: return "Expired";
        default: return "Unknown";
    }
}

std::optional<IdentityStatus> IdentityStatusFromString(const std::string& str) {
    if (str == "Pending") return IdentityStatus::Pending;
    if (str == "Active") return IdentityStatus::Active;
    if (str == "Suspended") return IdentityStatus::Suspended;
    if (str == "Revoked") return IdentityStatus::Revoked;
    if (str == "Expired") return IdentityStatus::Expired;
    return std::nullopt;
}

Hash256 ComputeIdentityId(const IdentityCommitment& commitment) {
    SHA256 hasher;
    const auto& hash = commitment.GetHash();
    hasher.Write(hash.data(), hash.size());
    
    // Add domain separator
    const char* domain = "nexus_identity_id";
    hasher.Write(reinterpret_cast<const Byte*>(domain), strlen(domain));
    
    Hash256 result;
    hasher.Finalize(result.data());
    return result;
}

std::array<Byte, 32> GenerateMasterSeed() {
    std::array<Byte, 32> seed;
    GetRandBytes(seed.data(), seed.size());
    return seed;
}

} // namespace identity
} // namespace shurium
