// SHURIUM - Block Checkpoints Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/consensus/checkpoints.h>
#include <shurium/chain/blockindex.h>
#include <shurium/core/hex.h>
#include <shurium/util/logging.h>

#include <algorithm>
#include <stdexcept>

namespace shurium {
namespace consensus {

// ============================================================================
// Checkpoint Implementation
// ============================================================================

Checkpoint::Checkpoint(int32_t h, const std::string& hashHex)
    : height(h) {
    if (hashHex.length() != 64) {
        throw std::invalid_argument("Invalid hash hex length");
    }
    
    // Parse hex string to hash (reverse byte order for display)
    auto bytes = HexToBytes(hashHex);
    if (bytes.size() != 32) {
        throw std::invalid_argument("Invalid hash hex");
    }
    
    // Reverse for internal representation (hex display is big-endian, internal is little-endian)
    std::reverse(bytes.begin(), bytes.end());
    std::copy(bytes.begin(), bytes.end(), hash.begin());
}

// ============================================================================
// CheckpointResult to String
// ============================================================================

const char* CheckpointResultToString(CheckpointResult result) {
    switch (result) {
        case CheckpointResult::VALID:
            return "VALID";
        case CheckpointResult::HASH_MISMATCH:
            return "HASH_MISMATCH";
        case CheckpointResult::FORK_BEFORE_CHECKPOINT:
            return "FORK_BEFORE_CHECKPOINT";
        case CheckpointResult::INVALID_HEIGHT:
            return "INVALID_HEIGHT";
        default:
            return "UNKNOWN";
    }
}

// ============================================================================
// CheckpointManager Implementation
// ============================================================================

void CheckpointManager::LoadMainnetCheckpoints() {
    Clear();
    AddCheckpoints(Checkpoints::GetMainnetCheckpoints());
}

void CheckpointManager::LoadTestnetCheckpoints() {
    Clear();
    AddCheckpoints(Checkpoints::GetTestnetCheckpoints());
}

void CheckpointManager::LoadCheckpoints(const std::string& networkId) {
    Clear();
    AddCheckpoints(Checkpoints::GetCheckpointsForNetwork(networkId));
}

void CheckpointManager::AddCheckpoint(const Checkpoint& checkpoint) {
    checkpoints_[checkpoint.height] = checkpoint;
}

void CheckpointManager::AddCheckpoint(int32_t height, const BlockHash& hash) {
    AddCheckpoint(Checkpoint(height, hash));
}

void CheckpointManager::AddCheckpoint(int32_t height, const std::string& hashHex) {
    AddCheckpoint(Checkpoint(height, hashHex));
}

void CheckpointManager::AddCheckpoints(const std::vector<Checkpoint>& checkpoints) {
    for (const auto& cp : checkpoints) {
        AddCheckpoint(cp);
    }
}

bool CheckpointManager::RemoveCheckpoint(int32_t height) {
    return checkpoints_.erase(height) > 0;
}

void CheckpointManager::Clear() {
    checkpoints_.clear();
}

std::optional<Checkpoint> CheckpointManager::GetCheckpoint(int32_t height) const {
    auto it = checkpoints_.find(height);
    if (it != checkpoints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool CheckpointManager::HasCheckpoint(int32_t height) const {
    return checkpoints_.find(height) != checkpoints_.end();
}

std::optional<Checkpoint> CheckpointManager::GetLastCheckpoint() const {
    if (checkpoints_.empty()) {
        return std::nullopt;
    }
    return checkpoints_.rbegin()->second;
}

std::optional<Checkpoint> CheckpointManager::GetLastCheckpointBefore(int32_t height) const {
    auto it = checkpoints_.upper_bound(height);
    if (it == checkpoints_.begin()) {
        // All checkpoints are after the given height
        return std::nullopt;
    }
    --it;
    return it->second;
}

std::optional<Checkpoint> CheckpointManager::GetFirstCheckpointAfter(int32_t height) const {
    auto it = checkpoints_.upper_bound(height);
    if (it == checkpoints_.end()) {
        return std::nullopt;
    }
    return it->second;
}

// ============================================================================
// Block Validation
// ============================================================================

CheckpointResult CheckpointManager::ValidateBlock(int32_t height, 
                                                   const BlockHash& hash) const {
    if (height < 0) {
        return CheckpointResult::INVALID_HEIGHT;
    }
    
    // Check if there's a checkpoint at this exact height
    auto it = checkpoints_.find(height);
    if (it != checkpoints_.end()) {
        if (it->second.hash != hash) {
            // Reverse hashes for display (internal is little-endian, display is big-endian)
            auto expectedReversed = ReverseBytes(it->second.hash.data(), it->second.hash.size());
            auto gotReversed = ReverseBytes(hash.data(), hash.size());
            LOG_WARN(util::LogCategory::VALIDATION) 
                << "Checkpoint mismatch at height " << height
                << ": expected " << BytesToHex(expectedReversed)
                << ", got " << BytesToHex(gotReversed);
            return CheckpointResult::HASH_MISMATCH;
        }
    }
    
    return CheckpointResult::VALID;
}

CheckpointResult CheckpointManager::ValidateBlock(const BlockIndex* pindex) const {
    if (!pindex) {
        return CheckpointResult::INVALID_HEIGHT;
    }
    return ValidateBlock(pindex->nHeight, pindex->GetBlockHash());
}

bool CheckpointManager::CanReorgAtHeight(int32_t height) const {
    if (checkpoints_.empty()) {
        return true;  // No checkpoints means any reorg is allowed
    }
    
    // Cannot reorg at or before the last checkpoint
    return height > checkpoints_.rbegin()->first;
}

int32_t CheckpointManager::GetReorgProtectionHeight() const {
    if (checkpoints_.empty()) {
        return -1;
    }
    return checkpoints_.rbegin()->first;
}

bool CheckpointManager::IsPastLastCheckpoint(int32_t currentHeight) const {
    if (checkpoints_.empty()) {
        return true;
    }
    return currentHeight > checkpoints_.rbegin()->first;
}

// ============================================================================
// Initial Sync Support
// ============================================================================

bool CheckpointManager::CanSkipScriptVerification(int32_t height) const {
    // Can skip script verification for blocks before the last checkpoint
    // This is the "assume-valid" optimization
    if (checkpoints_.empty()) {
        return false;
    }
    return height < checkpoints_.rbegin()->first;
}

double CheckpointManager::EstimateSyncProgress(int32_t currentHeight) const {
    if (checkpoints_.empty()) {
        return 0.0;
    }
    
    int32_t lastCpHeight = checkpoints_.rbegin()->first;
    if (lastCpHeight <= 0) {
        return 100.0;
    }
    
    if (currentHeight >= lastCpHeight) {
        return 100.0;
    }
    
    return (static_cast<double>(currentHeight) / lastCpHeight) * 100.0;
}

int64_t CheckpointManager::EstimateTimeRemaining(int32_t currentHeight,
                                                  double blocksPerSecond) const {
    if (checkpoints_.empty() || blocksPerSecond <= 0) {
        return 0;
    }
    
    int32_t lastCpHeight = checkpoints_.rbegin()->first;
    if (currentHeight >= lastCpHeight) {
        return 0;
    }
    
    int32_t blocksRemaining = lastCpHeight - currentHeight;
    return static_cast<int64_t>(blocksRemaining / blocksPerSecond);
}

// ============================================================================
// Statistics
// ============================================================================

uint64_t CheckpointManager::GetTotalTxsAtLastCheckpoint() const {
    if (checkpoints_.empty()) {
        return 0;
    }
    return checkpoints_.rbegin()->second.totalTxs;
}

int64_t CheckpointManager::GetLastCheckpointTime() const {
    if (checkpoints_.empty()) {
        return 0;
    }
    return checkpoints_.rbegin()->second.timestamp;
}

// ============================================================================
// Predefined Checkpoints
// ============================================================================

namespace Checkpoints {

std::vector<Checkpoint> GetMainnetCheckpoints() {
    // Checkpoints for SHURIUM mainnet
    // Genesis hash computed from CreateGenesisBlock with mined nonce 171163
    std::vector<Checkpoint> checkpoints;
    
    // Genesis block (height 0)
    // Hash: 00000cfc4e649be69c27c167f6abdc7cdd0ac751b39f7aa50dc694f6ff4b56c6
    Checkpoint genesis;
    genesis.height = 0;
    genesis.timestamp = 1700000000;  // Genesis timestamp
    genesis.totalTxs = 1;
    genesis.description = "Genesis block";
    genesis.hash = BlockHash(Hash256::FromHex("00000cfc4e649be69c27c167f6abdc7cdd0ac751b39f7aa50dc694f6ff4b56c6"));
    checkpoints.push_back(genesis);
    
    // Future checkpoints will be added as the chain grows
    // Block 10000 - First major milestone (~3.5 days at 30s blocks)
    // checkpoints.emplace_back(10000, BlockHash(Hash256::FromHex("...")), 1700300000, 50000);
    
    // Block 100000 - ~35 days at 30s blocks
    // checkpoints.emplace_back(100000, BlockHash(Hash256::FromHex("...")), 1703000000, 500000);
    
    // Block 1000000 - ~347 days
    // checkpoints.emplace_back(1000000, BlockHash(Hash256::FromHex("...")), 1730000000, 5000000);
    
    return checkpoints;
}

std::vector<Checkpoint> GetTestnetCheckpoints() {
    // Testnet checkpoints
    // Genesis hash computed from CreateGenesisBlock with mined nonce 811478
    std::vector<Checkpoint> checkpoints;
    
    // Genesis
    // Hash: 000009deb6c76c91b812804272b582fe96e51f17299007ad60b21871f6756c52
    Checkpoint genesis;
    genesis.height = 0;
    genesis.timestamp = 1700000001;  // Testnet genesis timestamp (1 second after mainnet)
    genesis.totalTxs = 1;
    genesis.description = "Testnet genesis";
    genesis.hash = BlockHash(Hash256::FromHex("000009deb6c76c91b812804272b582fe96e51f17299007ad60b21871f6756c52"));
    checkpoints.push_back(genesis);
    
    return checkpoints;
}

std::vector<Checkpoint> GetCheckpointsForNetwork(const std::string& networkId) {
    if (networkId == "main" || networkId == "mainnet") {
        return GetMainnetCheckpoints();
    } else if (networkId == "test" || networkId == "testnet") {
        return GetTestnetCheckpoints();
    } else {
        // Regtest and other networks have no checkpoints
        return {};
    }
}

} // namespace Checkpoints

// ============================================================================
// Global Checkpoint Manager
// ============================================================================

static CheckpointManager* g_checkpointManager = nullptr;

CheckpointManager& GetCheckpointManager() {
    if (!g_checkpointManager) {
        g_checkpointManager = new CheckpointManager();
    }
    return *g_checkpointManager;
}

void InitCheckpoints(const std::string& networkId) {
    GetCheckpointManager().LoadCheckpoints(networkId);
    
    size_t numCheckpoints = GetCheckpointManager().NumCheckpoints();
    if (numCheckpoints > 0) {
        auto lastCp = GetCheckpointManager().GetLastCheckpoint();
        LOG_INFO(util::LogCategory::VALIDATION) 
            << "Loaded " << numCheckpoints << " checkpoints for " << networkId
            << " (last at height " << lastCp->height << ")";
    } else {
        LOG_INFO(util::LogCategory::VALIDATION) 
            << "No checkpoints configured for " << networkId;
    }
}

} // namespace consensus
} // namespace shurium
