// SHURIUM - Decentralized Price Oracle System Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/economics/oracle.h>
#include <shurium/crypto/sha256.h>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <sstream>

namespace shurium {
namespace economics {

// ============================================================================
// OracleStatus Utilities
// ============================================================================

const char* OracleStatusToString(OracleStatus status) {
    switch (status) {
        case OracleStatus::Active:     return "Active";
        case OracleStatus::Pending:    return "Pending";
        case OracleStatus::Suspended:  return "Suspended";
        case OracleStatus::Slashed:    return "Slashed";
        case OracleStatus::Withdrawn:  return "Withdrawn";
        case OracleStatus::Offline:    return "Offline";
        default:                       return "Unknown";
    }
}

// ============================================================================
// OracleInfo
// ============================================================================

double OracleInfo::AccuracyRate() const {
    if (submissionCount == 0) {
        return 0.0;
    }
    return static_cast<double>(successfulSubmissions) / submissionCount * 100.0;
}

bool OracleInfo::CanSubmit(int currentHeight) const {
    if (status != OracleStatus::Active) {
        return false;
    }
    return currentHeight >= lastActiveHeight + ORACLE_UPDATE_COOLDOWN;
}

bool OracleInfo::IsTimedOut() const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastHeartbeat
    ).count();
    return elapsed > ORACLE_TIMEOUT_SECONDS;
}

double OracleInfo::GetWeight() const {
    // Weight based on reputation (0-1000 scale mapped to 0.1-2.0)
    return 0.1 + (reputation / 1000.0) * 1.9;
}

std::string OracleInfo::ToString() const {
    std::ostringstream ss;
    ss << "OracleInfo {"
       << " id: " << id.ToHex().substr(0, 16) << "..."
       << ", status: " << OracleStatusToString(status)
       << ", reputation: " << reputation
       << ", stake: " << FormatAmount(stakedAmount)
       << ", accuracy: " << AccuracyRate() << "%"
       << " }";
    return ss.str();
}

// ============================================================================
// PriceSubmission
// ============================================================================

Hash256 PriceSubmission::GetHash() const {
    std::vector<Byte> data = GetSignatureMessage();
    return SHA256Hash(data.data(), data.size());
}

std::vector<Byte> PriceSubmission::GetSignatureMessage() const {
    std::vector<Byte> msg;
    msg.reserve(64);
    
    // Oracle ID (32 bytes)
    msg.insert(msg.end(), oracleId.begin(), oracleId.end());
    
    // Price (8 bytes)
    for (int i = 0; i < 8; ++i) {
        msg.push_back(static_cast<Byte>((price >> (i * 8)) & 0xFF));
    }
    
    // Block height (4 bytes)
    for (int i = 0; i < 4; ++i) {
        msg.push_back(static_cast<Byte>((blockHeight >> (i * 8)) & 0xFF));
    }
    
    // Confidence (1 byte)
    msg.push_back(static_cast<Byte>(confidence));
    
    return msg;
}

bool PriceSubmission::VerifySignature(const PublicKey& pubkey) const {
    if (signature.empty()) {
        return false;
    }
    
    // In a real implementation, this would verify the ECDSA signature
    // For now, return true if signature exists
    auto message = GetSignatureMessage();
    return pubkey.IsValid() && !signature.empty();
}

std::vector<Byte> PriceSubmission::Serialize() const {
    std::vector<Byte> data;
    data.reserve(128);
    
    // Oracle ID
    data.insert(data.end(), oracleId.begin(), oracleId.end());
    
    // Price
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((price >> (i * 8)) & 0xFF));
    }
    
    // Block height
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((blockHeight >> (i * 8)) & 0xFF));
    }
    
    // Timestamp (8 bytes, seconds since epoch)
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()
    ).count();
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((ts >> (i * 8)) & 0xFF));
    }
    
    // Confidence
    data.push_back(static_cast<Byte>(confidence));
    
    // Signature length and data
    uint16_t sigLen = static_cast<uint16_t>(signature.size());
    data.push_back(static_cast<Byte>(sigLen & 0xFF));
    data.push_back(static_cast<Byte>((sigLen >> 8) & 0xFF));
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<PriceSubmission> PriceSubmission::Deserialize(const Byte* data, size_t len) {
    if (len < 55) { // Minimum: 32 + 8 + 4 + 8 + 1 + 2 = 55
        return std::nullopt;
    }
    
    PriceSubmission sub;
    size_t offset = 0;
    
    // Oracle ID
    sub.oracleId = Hash256(data + offset, 32);
    offset += 32;
    
    // Price
    sub.price = 0;
    for (int i = 0; i < 8; ++i) {
        sub.price |= static_cast<PriceMillicents>(data[offset++]) << (i * 8);
    }
    
    // Block height
    sub.blockHeight = 0;
    for (int i = 0; i < 4; ++i) {
        sub.blockHeight |= static_cast<int>(data[offset++]) << (i * 8);
    }
    
    // Timestamp
    int64_t ts = 0;
    for (int i = 0; i < 8; ++i) {
        ts |= static_cast<int64_t>(data[offset++]) << (i * 8);
    }
    sub.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(ts));
    
    // Confidence
    sub.confidence = data[offset++];
    
    // Signature
    if (offset + 2 > len) {
        return std::nullopt;
    }
    uint16_t sigLen = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    
    if (offset + sigLen > len) {
        return std::nullopt;
    }
    sub.signature.assign(data + offset, data + offset + sigLen);
    
    return sub;
}

std::string PriceSubmission::ToString() const {
    std::ostringstream ss;
    ss << "PriceSubmission {"
       << " oracle: " << oracleId.ToHex().substr(0, 16) << "..."
       << ", price: " << MillicentsToString(price)
       << ", height: " << blockHeight
       << ", confidence: " << confidence
       << " }";
    return ss.str();
}

// ============================================================================
// PriceCommitment
// ============================================================================

PriceCommitment PriceCommitment::Create(
    const OracleId& oracle,
    PriceMillicents price,
    int commitHeight,
    int revealWindow
) {
    PriceCommitment pc;
    pc.oracleId = oracle;
    pc.commitHeight = commitHeight;
    pc.revealDeadline = commitHeight + revealWindow;
    
    // Generate random salt
    std::random_device rd;
    std::array<Byte, 32> saltBytes;
    for (auto& b : saltBytes) {
        b = static_cast<Byte>(rd() & 0xFF);
    }
    pc.salt = Hash256(saltBytes.data(), 32);
    
    // Create commitment: H(price || salt)
    std::vector<Byte> preimage;
    preimage.reserve(40);
    for (int i = 0; i < 8; ++i) {
        preimage.push_back(static_cast<Byte>((price >> (i * 8)) & 0xFF));
    }
    preimage.insert(preimage.end(), pc.salt.begin(), pc.salt.end());
    
    pc.commitment = SHA256Hash(preimage.data(), preimage.size());
    
    return pc;
}

bool PriceCommitment::VerifyReveal(PriceMillicents price, const Hash256& revealSalt) const {
    // Recreate commitment and compare
    std::vector<Byte> preimage;
    preimage.reserve(40);
    for (int i = 0; i < 8; ++i) {
        preimage.push_back(static_cast<Byte>((price >> (i * 8)) & 0xFF));
    }
    preimage.insert(preimage.end(), revealSalt.begin(), revealSalt.end());
    
    Hash256 expectedCommitment = SHA256Hash(preimage.data(), preimage.size());
    return commitment == expectedCommitment;
}

bool PriceCommitment::IsExpired(int currentHeight) const {
    return currentHeight > revealDeadline;
}

std::vector<Byte> PriceCommitment::Serialize() const {
    std::vector<Byte> data;
    data.reserve(128);
    
    // Oracle ID
    data.insert(data.end(), oracleId.begin(), oracleId.end());
    
    // Commitment
    data.insert(data.end(), commitment.begin(), commitment.end());
    
    // Heights
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((commitHeight >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((revealDeadline >> (i * 8)) & 0xFF));
    }
    
    // Flags and revealed data
    data.push_back(revealed ? 1 : 0);
    
    if (revealed) {
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<Byte>((revealedPrice >> (i * 8)) & 0xFF));
        }
        data.insert(data.end(), salt.begin(), salt.end());
    }
    
    return data;
}

std::optional<PriceCommitment> PriceCommitment::Deserialize(const Byte* data, size_t len) {
    if (len < 73) { // Minimum: 32 + 32 + 4 + 4 + 1 = 73
        return std::nullopt;
    }
    
    PriceCommitment pc;
    size_t offset = 0;
    
    // Oracle ID
    pc.oracleId = Hash256(data + offset, 32);
    offset += 32;
    
    // Commitment
    pc.commitment = Hash256(data + offset, 32);
    offset += 32;
    
    // Heights
    pc.commitHeight = 0;
    for (int i = 0; i < 4; ++i) {
        pc.commitHeight |= static_cast<int>(data[offset++]) << (i * 8);
    }
    pc.revealDeadline = 0;
    for (int i = 0; i < 4; ++i) {
        pc.revealDeadline |= static_cast<int>(data[offset++]) << (i * 8);
    }
    
    // Revealed
    pc.revealed = data[offset++] != 0;
    
    if (pc.revealed) {
        if (offset + 40 > len) {
            return std::nullopt;
        }
        pc.revealedPrice = 0;
        for (int i = 0; i < 8; ++i) {
            pc.revealedPrice |= static_cast<PriceMillicents>(data[offset++]) << (i * 8);
        }
        pc.salt = Hash256(data + offset, 32);
    }
    
    return pc;
}

// ============================================================================
// OracleRegistry
// ============================================================================

OracleRegistry::OracleRegistry() = default;
OracleRegistry::~OracleRegistry() = default;

std::optional<OracleId> OracleRegistry::Register(
    const PublicKey& pubkey,
    const Hash160& operatorAddr,
    Amount stake,
    int height,
    const std::string& name
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check minimum stake
    if (stake < MIN_ORACLE_STAKE) {
        return std::nullopt;
    }
    
    // Check if already registered
    if (pubkeyIndex_.find(pubkey) != pubkeyIndex_.end()) {
        return std::nullopt;
    }
    
    // Generate ID
    OracleId id = GenerateId(pubkey);
    
    // Create oracle info
    OracleInfo info;
    info.id = id;
    info.publicKey = pubkey;
    info.operatorAddress = operatorAddr;
    info.stakedAmount = stake;
    info.status = OracleStatus::Pending;
    info.registrationHeight = height;
    info.lastActiveHeight = height;
    info.lastHeartbeat = std::chrono::system_clock::now();
    info.reputation = 500; // Start neutral
    info.name = name;
    
    oracles_[id] = info;
    pubkeyIndex_[pubkey] = id;
    
    return id;
}

bool OracleRegistry::AddStake(const OracleId& id, Amount amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it == oracles_.end()) {
        return false;
    }
    
    it->second.stakedAmount += amount;
    return true;
}

Amount OracleRegistry::Withdraw(const OracleId& id, int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it == oracles_.end()) {
        return 0;
    }
    
    Amount stake = it->second.stakedAmount;
    it->second.status = OracleStatus::Withdrawn;
    it->second.stakedAmount = 0;
    it->second.lastActiveHeight = height;
    
    // Remove from pubkey index
    pubkeyIndex_.erase(it->second.publicKey);
    
    return stake;
}

const OracleInfo* OracleRegistry::GetOracle(const OracleId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it == oracles_.end()) {
        return nullptr;
    }
    return &it->second;
}

const OracleInfo* OracleRegistry::GetOracleByPubkey(const PublicKey& pubkey) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pubkeyIndex_.find(pubkey);
    if (it == pubkeyIndex_.end()) {
        return nullptr;
    }
    return &oracles_.at(it->second);
}

std::vector<const OracleInfo*> OracleRegistry::GetActiveOracles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<const OracleInfo*> result;
    for (const auto& [id, info] : oracles_) {
        if (info.status == OracleStatus::Active) {
            result.push_back(&info);
        }
    }
    return result;
}

size_t OracleRegistry::GetOracleCount(OracleStatus status) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [id, info] : oracles_) {
        if (info.status == status) {
            count++;
        }
    }
    return count;
}

bool OracleRegistry::HasOracle(const OracleId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return oracles_.find(id) != oracles_.end();
}

void OracleRegistry::UpdateStatus(const OracleId& id, OracleStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it != oracles_.end()) {
        it->second.status = status;
    }
}

void OracleRegistry::RecordHeartbeat(const OracleId& id, int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it != oracles_.end()) {
        it->second.lastHeartbeat = std::chrono::system_clock::now();
        it->second.lastActiveHeight = height;
        
        // If offline, mark as active again
        if (it->second.status == OracleStatus::Offline) {
            it->second.status = OracleStatus::Active;
        }
    }
}

void OracleRegistry::RecordSubmission(const OracleId& id, bool wasIncluded) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it != oracles_.end()) {
        it->second.submissionCount++;
        if (wasIncluded) {
            it->second.successfulSubmissions++;
        } else {
            it->second.outlierCount++;
        }
    }
}

void OracleRegistry::UpdateTimeouts() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [id, info] : oracles_) {
        if (info.status == OracleStatus::Active && info.IsTimedOut()) {
            info.status = OracleStatus::Offline;
        }
    }
}

void OracleRegistry::IncreaseReputation(const OracleId& id, int amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it != oracles_.end()) {
        it->second.reputation = std::min(1000, it->second.reputation + amount);
    }
}

void OracleRegistry::DecreaseReputation(const OracleId& id, int amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it != oracles_.end()) {
        it->second.reputation = std::max(0, it->second.reputation - amount);
    }
}

Amount OracleRegistry::Slash(const OracleId& id, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = oracles_.find(id);
    if (it == oracles_.end()) {
        return 0;
    }
    
    // Calculate slash amount
    Amount slashAmount = (it->second.stakedAmount * ORACLE_SLASH_PERCENT) / 100;
    
    it->second.stakedAmount -= slashAmount;
    it->second.slashCount++;
    it->second.reputation = std::max(0, it->second.reputation - 100);
    
    // If stake falls below minimum, suspend
    if (it->second.stakedAmount < MIN_ORACLE_STAKE) {
        it->second.status = OracleStatus::Slashed;
    }
    
    return slashAmount;
}

std::vector<Byte> OracleRegistry::Serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Byte> data;
    
    // Version byte
    data.push_back(0x01);
    
    // Number of oracles (4 bytes, little-endian)
    uint32_t count = static_cast<uint32_t>(oracles_.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((count >> (i * 8)) & 0xFF));
    }
    
    // Serialize each oracle
    for (const auto& [id, info] : oracles_) {
        // Oracle ID (32 bytes)
        data.insert(data.end(), info.id.begin(), info.id.end());
        
        // Public key (33 bytes for compressed)
        auto pkData = info.publicKey.ToVector();
        data.push_back(static_cast<Byte>(pkData.size()));
        data.insert(data.end(), pkData.begin(), pkData.end());
        
        // Operator address (20 bytes)
        data.insert(data.end(), info.operatorAddress.begin(), info.operatorAddress.end());
        
        // Staked amount (8 bytes)
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<Byte>((info.stakedAmount >> (i * 8)) & 0xFF));
        }
        
        // Status (1 byte)
        data.push_back(static_cast<Byte>(info.status));
        
        // Registration height (4 bytes)
        for (int i = 0; i < 4; ++i) {
            data.push_back(static_cast<Byte>((info.registrationHeight >> (i * 8)) & 0xFF));
        }
        
        // Last active height (4 bytes)
        for (int i = 0; i < 4; ++i) {
            data.push_back(static_cast<Byte>((info.lastActiveHeight >> (i * 8)) & 0xFF));
        }
        
        // Last heartbeat timestamp (8 bytes)
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
            info.lastHeartbeat.time_since_epoch()
        ).count();
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<Byte>((ts >> (i * 8)) & 0xFF));
        }
        
        // Reputation (2 bytes)
        data.push_back(static_cast<Byte>(info.reputation & 0xFF));
        data.push_back(static_cast<Byte>((info.reputation >> 8) & 0xFF));
        
        // Submission count (8 bytes)
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<Byte>((info.submissionCount >> (i * 8)) & 0xFF));
        }
        
        // Successful submissions (8 bytes)
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<Byte>((info.successfulSubmissions >> (i * 8)) & 0xFF));
        }
        
        // Outlier count (8 bytes)
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<Byte>((info.outlierCount >> (i * 8)) & 0xFF));
        }
        
        // Slash count (4 bytes)
        for (int i = 0; i < 4; ++i) {
            data.push_back(static_cast<Byte>((info.slashCount >> (i * 8)) & 0xFF));
        }
        
        // Name (length-prefixed string)
        uint16_t nameLen = static_cast<uint16_t>(std::min(info.name.size(), size_t(256)));
        data.push_back(static_cast<Byte>(nameLen & 0xFF));
        data.push_back(static_cast<Byte>((nameLen >> 8) & 0xFF));
        data.insert(data.end(), info.name.begin(), info.name.begin() + nameLen);
        
        // URL (length-prefixed string)
        uint16_t urlLen = static_cast<uint16_t>(std::min(info.url.size(), size_t(256)));
        data.push_back(static_cast<Byte>(urlLen & 0xFF));
        data.push_back(static_cast<Byte>((urlLen >> 8) & 0xFF));
        data.insert(data.end(), info.url.begin(), info.url.begin() + urlLen);
    }
    
    return data;
}

bool OracleRegistry::Deserialize(const Byte* data, size_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (len < 5) {  // Version + count
        return false;
    }
    
    size_t offset = 0;
    
    // Check version
    if (data[offset++] != 0x01) {
        return false;
    }
    
    // Read oracle count
    uint32_t count = 0;
    for (int i = 0; i < 4; ++i) {
        count |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    // Clear existing data
    oracles_.clear();
    pubkeyIndex_.clear();
    
    // Deserialize each oracle
    for (uint32_t n = 0; n < count; ++n) {
        OracleInfo info;
        
        // Oracle ID (32 bytes)
        if (offset + 32 > len) return false;
        info.id = Hash256(data + offset, 32);
        offset += 32;
        
        // Public key (length-prefixed)
        if (offset + 1 > len) return false;
        uint8_t pkLen = data[offset++];
        if (offset + pkLen > len) return false;
        info.publicKey = PublicKey(data + offset, pkLen);
        offset += pkLen;
        
        // Operator address (20 bytes)
        if (offset + 20 > len) return false;
        info.operatorAddress = Hash160(data + offset, 20);
        offset += 20;
        
        // Staked amount (8 bytes)
        if (offset + 8 > len) return false;
        info.stakedAmount = 0;
        for (int i = 0; i < 8; ++i) {
            info.stakedAmount |= static_cast<Amount>(data[offset++]) << (i * 8);
        }
        
        // Status (1 byte)
        if (offset + 1 > len) return false;
        info.status = static_cast<OracleStatus>(data[offset++]);
        
        // Registration height (4 bytes)
        if (offset + 4 > len) return false;
        info.registrationHeight = 0;
        for (int i = 0; i < 4; ++i) {
            info.registrationHeight |= static_cast<int>(data[offset++]) << (i * 8);
        }
        
        // Last active height (4 bytes)
        if (offset + 4 > len) return false;
        info.lastActiveHeight = 0;
        for (int i = 0; i < 4; ++i) {
            info.lastActiveHeight |= static_cast<int>(data[offset++]) << (i * 8);
        }
        
        // Last heartbeat timestamp (8 bytes)
        if (offset + 8 > len) return false;
        int64_t ts = 0;
        for (int i = 0; i < 8; ++i) {
            ts |= static_cast<int64_t>(data[offset++]) << (i * 8);
        }
        info.lastHeartbeat = std::chrono::system_clock::time_point(std::chrono::seconds(ts));
        
        // Reputation (2 bytes)
        if (offset + 2 > len) return false;
        info.reputation = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        
        // Submission count (8 bytes)
        if (offset + 8 > len) return false;
        info.submissionCount = 0;
        for (int i = 0; i < 8; ++i) {
            info.submissionCount |= static_cast<uint64_t>(data[offset++]) << (i * 8);
        }
        
        // Successful submissions (8 bytes)
        if (offset + 8 > len) return false;
        info.successfulSubmissions = 0;
        for (int i = 0; i < 8; ++i) {
            info.successfulSubmissions |= static_cast<uint64_t>(data[offset++]) << (i * 8);
        }
        
        // Outlier count (8 bytes)
        if (offset + 8 > len) return false;
        info.outlierCount = 0;
        for (int i = 0; i < 8; ++i) {
            info.outlierCount |= static_cast<uint64_t>(data[offset++]) << (i * 8);
        }
        
        // Slash count (4 bytes)
        if (offset + 4 > len) return false;
        info.slashCount = 0;
        for (int i = 0; i < 4; ++i) {
            info.slashCount |= static_cast<uint32_t>(data[offset++]) << (i * 8);
        }
        
        // Name (length-prefixed string)
        if (offset + 2 > len) return false;
        uint16_t nameLen = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        if (offset + nameLen > len) return false;
        info.name.assign(reinterpret_cast<const char*>(data + offset), nameLen);
        offset += nameLen;
        
        // URL (length-prefixed string)
        if (offset + 2 > len) return false;
        uint16_t urlLen = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        if (offset + urlLen > len) return false;
        info.url.assign(reinterpret_cast<const char*>(data + offset), urlLen);
        offset += urlLen;
        
        // Add to registry
        oracles_[info.id] = info;
        pubkeyIndex_[info.publicKey] = info.id;
    }
    
    return true;
}

OracleId OracleRegistry::GenerateId(const PublicKey& pubkey) {
    // Hash the public key to generate ID
    auto pkData = pubkey.ToVector();
    return SHA256Hash(pkData.data(), pkData.size());
}

// ============================================================================
// PriceAggregator
// ============================================================================

PriceAggregator::PriceAggregator(OracleRegistry& registry)
    : registry_(registry) {
}

PriceAggregator::PriceAggregator(OracleRegistry& registry, const Config& config)
    : registry_(registry), config_(config) {
}

bool PriceAggregator::ProcessSubmission(const PriceSubmission& submission) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Verify oracle exists and is active
    const OracleInfo* oracle = registry_.GetOracle(submission.oracleId);
    if (!oracle || oracle->status != OracleStatus::Active) {
        return false;
    }
    
    // Verify signature
    if (!submission.VerifySignature(oracle->publicKey)) {
        return false;
    }
    
    // Add to current submissions
    currentSubmissions_.push_back(submission);
    return true;
}

bool PriceAggregator::ProcessCommitment(const PriceCommitment& commitment) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Verify oracle exists
    if (!registry_.HasOracle(commitment.oracleId)) {
        return false;
    }
    
    pendingCommitments_[commitment.oracleId] = commitment;
    return true;
}

bool PriceAggregator::ProcessReveal(
    const OracleId& oracleId, 
    PriceMillicents price, 
    const Hash256& salt
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pendingCommitments_.find(oracleId);
    if (it == pendingCommitments_.end()) {
        return false;
    }
    
    // Verify reveal matches commitment
    if (!it->second.VerifyReveal(price, salt)) {
        return false;
    }
    
    // Mark as revealed and create submission
    it->second.revealed = true;
    it->second.revealedPrice = price;
    it->second.salt = salt;
    
    PriceSubmission sub;
    sub.oracleId = oracleId;
    sub.price = price;
    sub.blockHeight = it->second.commitHeight;
    sub.timestamp = std::chrono::system_clock::now();
    sub.confidence = 100;
    
    currentSubmissions_.push_back(sub);
    
    return true;
}

std::optional<AggregatedPrice> PriceAggregator::Aggregate(int currentHeight) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check minimum sources
    if (currentSubmissions_.size() < config_.minSources) {
        return std::nullopt;
    }
    
    // Remove outliers
    auto filteredSubmissions = RemoveOutliers(currentSubmissions_);
    
    if (filteredSubmissions.size() < config_.minSources) {
        return std::nullopt;
    }
    
    AggregatedPrice agg;
    agg.timestamp = std::chrono::system_clock::now();
    agg.sourceCount = filteredSubmissions.size();
    
    // Calculate weighted prices
    std::vector<std::pair<PriceMillicents, double>> weightedPrices;
    double totalWeight = 0.0;
    int64_t totalConfidence = 0;
    
    agg.minPrice = INT64_MAX;
    agg.maxPrice = INT64_MIN;
    
    for (const auto& sub : filteredSubmissions) {
        const OracleInfo* oracle = registry_.GetOracle(sub.oracleId);
        double weight = oracle ? oracle->GetWeight() : 1.0;
        
        weightedPrices.emplace_back(sub.price, weight);
        totalWeight += weight;
        totalConfidence += sub.confidence;
        
        agg.minPrice = std::min(agg.minPrice, sub.price);
        agg.maxPrice = std::max(agg.maxPrice, sub.price);
    }
    
    // Calculate weighted median
    agg.medianPrice = CalculateWeightedMedian(weightedPrices);
    
    // Calculate weighted average
    PriceMillicents weightedSum = 0;
    for (const auto& [price, weight] : weightedPrices) {
        weightedSum += static_cast<PriceMillicents>(price * weight);
    }
    agg.weightedPrice = static_cast<PriceMillicents>(weightedSum / totalWeight);
    
    // Calculate spread
    agg.spreadBps = CalculateSpread(filteredSubmissions);
    
    // Average confidence
    agg.avgConfidence = static_cast<int>(totalConfidence / filteredSubmissions.size());
    
    // Update oracle statistics
    for (const auto& sub : currentSubmissions_) {
        bool included = std::any_of(filteredSubmissions.begin(), filteredSubmissions.end(),
            [&sub](const PriceSubmission& s) { return s.oracleId == sub.oracleId; });
        registry_.RecordSubmission(sub.oracleId, included);
        
        if (included) {
            registry_.IncreaseReputation(sub.oracleId, 1);
        } else {
            registry_.DecreaseReputation(sub.oracleId, 5);
        }
    }
    
    latestPrice_ = agg;
    priceHistory_.push_back(agg);
    
    return agg;
}

std::optional<AggregatedPrice> PriceAggregator::GetLatestPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestPrice_;
}

size_t PriceAggregator::GetPendingSubmissionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentSubmissions_.size();
}

std::vector<PriceSubmission> PriceAggregator::GetCurrentSubmissions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentSubmissions_;
}

void PriceAggregator::StartNewRound(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentRoundHeight_ = height;
    currentSubmissions_.clear();
    pendingCommitments_.clear();
}

void PriceAggregator::FinalizeRound() {
    std::lock_guard<std::mutex> lock(mutex_);
    currentSubmissions_.clear();
}

void PriceAggregator::Prune(int keepRounds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    while (priceHistory_.size() > static_cast<size_t>(keepRounds)) {
        priceHistory_.pop_front();
    }
}

void PriceAggregator::UpdateConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

PriceMillicents PriceAggregator::CalculateWeightedMedian(
    const std::vector<std::pair<PriceMillicents, double>>& weightedPrices
) const {
    if (weightedPrices.empty()) {
        return 0;
    }
    
    // Sort by price
    auto sorted = weightedPrices;
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    
    // Calculate total weight
    double totalWeight = 0.0;
    for (const auto& [price, weight] : sorted) {
        totalWeight += weight;
    }
    
    // Find weighted median
    double cumWeight = 0.0;
    double medianWeight = totalWeight / 2.0;
    
    for (const auto& [price, weight] : sorted) {
        cumWeight += weight;
        if (cumWeight >= medianWeight) {
            return price;
        }
    }
    
    return sorted.back().first;
}

std::vector<PriceSubmission> PriceAggregator::RemoveOutliers(
    const std::vector<PriceSubmission>& submissions
) const {
    if (submissions.size() < 3) {
        return submissions;
    }
    
    // Calculate median
    std::vector<PriceMillicents> prices;
    for (const auto& sub : submissions) {
        prices.push_back(sub.price);
    }
    std::sort(prices.begin(), prices.end());
    PriceMillicents median = prices[prices.size() / 2];
    
    // Filter outliers
    std::vector<PriceSubmission> filtered;
    for (const auto& sub : submissions) {
        int64_t deviationBps = std::abs(CalculateDeviationBps(sub.price, median));
        if (deviationBps <= config_.maxDeviationBps) {
            filtered.push_back(sub);
        }
    }
    
    return filtered;
}

int64_t PriceAggregator::CalculateSpread(
    const std::vector<PriceSubmission>& submissions
) const {
    if (submissions.empty()) {
        return 0;
    }
    
    PriceMillicents minPrice = INT64_MAX;
    PriceMillicents maxPrice = INT64_MIN;
    
    for (const auto& sub : submissions) {
        minPrice = std::min(minPrice, sub.price);
        maxPrice = std::max(maxPrice, sub.price);
    }
    
    if (minPrice == 0) {
        return 0;
    }
    
    return ((maxPrice - minPrice) * 10000) / minPrice;
}

// ============================================================================
// OraclePriceFeed
// ============================================================================

OraclePriceFeed::OraclePriceFeed() = default;
OraclePriceFeed::~OraclePriceFeed() = default;

void OraclePriceFeed::Initialize(std::shared_ptr<OracleRegistry> registry) {
    std::lock_guard<std::mutex> lock(mutex_);
    registry_ = registry;
    aggregator_ = std::make_unique<PriceAggregator>(*registry_);
}

void OraclePriceFeed::ProcessBlock(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!aggregator_) {
        return;
    }
    
    // Update oracle timeouts
    registry_->UpdateTimeouts();
    
    // Try to aggregate
    auto price = aggregator_->Aggregate(height);
    if (price) {
        // Notify callbacks
        for (const auto& cb : callbacks_) {
            cb(*price);
        }
        
        // Start new round
        aggregator_->StartNewRound(height);
    }
}

std::optional<AggregatedPrice> OraclePriceFeed::GetCurrentPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!aggregator_) {
        return std::nullopt;
    }
    return aggregator_->GetLatestPrice();
}

std::optional<AggregatedPrice> OraclePriceFeed::GetPriceAtHeight(int height) const {
    // Would need historical storage
    (void)height;
    return GetCurrentPrice();
}

std::vector<AggregatedPrice> OraclePriceFeed::GetPriceHistory(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Would return from price history
    (void)count;
    std::vector<AggregatedPrice> result;
    return result;
}

void OraclePriceFeed::OnPriceUpdate(PriceCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

// ============================================================================
// OracleRewardCalculator
// ============================================================================

Amount OracleRewardCalculator::CalculateReward(
    const OracleInfo& oracle,
    Amount totalRewardPool,
    size_t totalActiveOracles
) {
    if (totalActiveOracles == 0 || oracle.status != OracleStatus::Active) {
        return 0;
    }
    
    // Base share
    Amount baseShare = totalRewardPool / totalActiveOracles;
    
    // Adjust based on reputation (0.5x to 1.5x)
    double reputationMultiplier = 0.5 + (oracle.reputation / 1000.0);
    
    // Adjust based on accuracy
    double accuracyMultiplier = oracle.AccuracyRate() / 100.0;
    
    return static_cast<Amount>(baseShare * reputationMultiplier * accuracyMultiplier);
}

Amount OracleRewardCalculator::CalculatePenalty(
    const OracleInfo& oracle,
    int missedSubmissions
) {
    // Penalty: 0.1% of stake per missed submission
    return (oracle.stakedAmount * missedSubmissions) / 1000;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool ValidateSubmission(
    const PriceSubmission& submission, 
    const OracleRegistry& registry
) {
    const OracleInfo* oracle = registry.GetOracle(submission.oracleId);
    if (!oracle) {
        return false;
    }
    
    if (oracle->status != OracleStatus::Active) {
        return false;
    }
    
    return submission.VerifySignature(oracle->publicKey);
}

OracleId CalculateOracleId(const PublicKey& pubkey) {
    auto pkData = pubkey.ToVector();
    return SHA256Hash(pkData.data(), pkData.size());
}

bool IsPriceReasonable(
    PriceMillicents price, 
    PriceMillicents reference, 
    int64_t maxDeviationBps
) {
    if (reference == 0) {
        return true;
    }
    
    int64_t deviationBps = std::abs(CalculateDeviationBps(price, reference));
    return deviationBps <= maxDeviationBps;
}

} // namespace economics
} // namespace shurium
