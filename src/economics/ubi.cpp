// SHURIUM - Universal Basic Income (UBI) System Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/economics/ubi.h>
#include <shurium/crypto/sha256.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

namespace shurium {
namespace economics {

// ============================================================================
// ClaimStatus Utilities
// ============================================================================

const char* ClaimStatusToString(ClaimStatus status) {
    switch (status) {
        case ClaimStatus::Pending:          return "Pending";
        case ClaimStatus::Valid:            return "Valid";
        case ClaimStatus::InvalidProof:     return "InvalidProof";
        case ClaimStatus::DoubleClaim:      return "DoubleClaim";
        case ClaimStatus::IdentityNotFound: return "IdentityNotFound";
        case ClaimStatus::EpochExpired:     return "EpochExpired";
        case ClaimStatus::EpochNotComplete: return "EpochNotComplete";
        case ClaimStatus::PoolEmpty:        return "PoolEmpty";
        default:                            return "Unknown";
    }
}

// ============================================================================
// UBIClaim
// ============================================================================

UBIClaim UBIClaim::Create(
    EpochId epoch,
    const identity::IdentitySecrets& secrets,
    const Hash160& recipient,
    const identity::VectorCommitment::MerkleProof& membershipProof
) {
    UBIClaim claim;
    claim.epoch = epoch;
    claim.recipient = recipient;
    claim.status = ClaimStatus::Pending;
    
    // Generate nullifier for this epoch using the correct API
    claim.nullifier = secrets.DeriveNullifier(epoch);
    
    // Generate ZK proof using the ProofGenerator
    identity::ProofGenerator& generator = identity::ProofGenerator::Instance();
    
    // Get the identity tree root from public inputs (would come from chain state)
    // For now, we create a placeholder proof
    identity::PublicInputs inputs;
    inputs.Add(FieldElement::FromBytes(claim.nullifier.GetHash().data(), 32));
    
    claim.proof = generator.GeneratePlaceholderProof(identity::ProofType::UBIClaim, inputs);
    
    return claim;
}

std::vector<Byte> UBIClaim::Serialize() const {
    std::vector<Byte> data;
    data.reserve(256);
    
    // Epoch (4 bytes)
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((epoch >> (i * 8)) & 0xFF));
    }
    
    // Nullifier (32 bytes + 8 bytes epoch)
    const auto& nullifierHash = nullifier.GetHash();
    data.insert(data.end(), nullifierHash.begin(), nullifierHash.end());
    uint64_t nullifierEpoch = nullifier.GetEpoch();
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((nullifierEpoch >> (i * 8)) & 0xFF));
    }
    
    // Recipient (20 bytes)
    data.insert(data.end(), recipient.begin(), recipient.end());
    
    // Submit height (4 bytes)
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((submitHeight >> (i * 8)) & 0xFF));
    }
    
    // Status (1 byte)
    data.push_back(static_cast<Byte>(status));
    
    // Amount (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((amount >> (i * 8)) & 0xFF));
    }
    
    // Proof (variable) - using ToBytes()
    auto proofBytes = proof.ToBytes();
    uint32_t proofSize = static_cast<uint32_t>(proofBytes.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((proofSize >> (i * 8)) & 0xFF));
    }
    data.insert(data.end(), proofBytes.begin(), proofBytes.end());
    
    return data;
}

std::optional<UBIClaim> UBIClaim::Deserialize(const Byte* data, size_t len) {
    if (len < 77) { // Minimum size: 4 + 32 + 8 + 20 + 4 + 1 + 8 = 77 bytes
        return std::nullopt;
    }
    
    UBIClaim claim;
    size_t offset = 0;
    
    // Epoch
    claim.epoch = 0;
    for (int i = 0; i < 4; ++i) {
        claim.epoch |= static_cast<EpochId>(data[offset++]) << (i * 8);
    }
    
    // Nullifier hash
    identity::NullifierHash nullifierHash;
    std::copy(data + offset, data + offset + 32, nullifierHash.begin());
    offset += 32;
    
    // Nullifier epoch
    uint64_t nullifierEpoch = 0;
    for (int i = 0; i < 8; ++i) {
        nullifierEpoch |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    claim.nullifier = identity::Nullifier(nullifierHash, nullifierEpoch);
    
    // Recipient
    std::copy(data + offset, data + offset + 20, claim.recipient.begin());
    offset += 20;
    
    // Submit height
    claim.submitHeight = 0;
    for (int i = 0; i < 4; ++i) {
        claim.submitHeight |= static_cast<int>(data[offset++]) << (i * 8);
    }
    
    // Status
    claim.status = static_cast<ClaimStatus>(data[offset++]);
    
    // Amount
    claim.amount = 0;
    for (int i = 0; i < 8; ++i) {
        claim.amount |= static_cast<Amount>(data[offset++]) << (i * 8);
    }
    
    // Proof
    if (offset + 4 > len) {
        return std::nullopt;
    }
    uint32_t proofSize = 0;
    for (int i = 0; i < 4; ++i) {
        proofSize |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    if (offset + proofSize > len) {
        return std::nullopt;
    }
    
    auto proofOpt = identity::ZKProof::FromBytes(&data[offset], proofSize);
    if (!proofOpt) {
        return std::nullopt;
    }
    claim.proof = *proofOpt;
    
    return claim;
}

Hash256 UBIClaim::GetHash() const {
    auto serialized = Serialize();
    return SHA256Hash(serialized.data(), serialized.size());
}

std::string UBIClaim::ToString() const {
    std::ostringstream ss;
    ss << "UBIClaim {"
       << " epoch: " << epoch
       << ", nullifier: " << nullifier.ToHex().substr(0, 16) << "..."
       << ", status: " << ClaimStatusToString(status)
       << ", amount: " << FormatAmount(amount)
       << " }";
    return ss.str();
}

// ============================================================================
// EpochUBIPool
// ============================================================================

void EpochUBIPool::Finalize(uint32_t identityCount) {
    eligibleCount = identityCount;
    
    if (identityCount >= MIN_IDENTITIES_FOR_UBI) {
        amountPerPerson = totalPool / identityCount;
        // Cap at maximum
        amountPerPerson = std::min(amountPerPerson, MAX_UBI_PER_PERSON);
    } else {
        // Not enough identities, no distribution
        amountPerPerson = 0;
    }
    
    isFinalized = true;
}

bool EpochUBIPool::IsNullifierUsed(const identity::Nullifier& nullifier) const {
    return usedNullifiers.find(nullifier) != usedNullifiers.end();
}

void EpochUBIPool::RecordClaim(const identity::Nullifier& nullifier, Amount amount) {
    usedNullifiers.insert(nullifier);
    amountClaimed += amount;
    claimCount++;
}

Amount EpochUBIPool::UnclaimedAmount() const {
    return totalPool > amountClaimed ? totalPool - amountClaimed : 0;
}

double EpochUBIPool::ClaimRate() const {
    if (eligibleCount == 0) {
        return 0.0;
    }
    return static_cast<double>(claimCount) / eligibleCount * 100.0;
}

bool EpochUBIPool::AcceptingClaims(int currentHeight) const {
    if (!isFinalized) {
        return false;
    }
    return currentHeight <= claimDeadline;
}

std::string EpochUBIPool::ToString() const {
    std::ostringstream ss;
    ss << "EpochUBIPool {"
       << " epoch: " << epoch
       << ", pool: " << FormatAmount(totalPool)
       << ", eligible: " << eligibleCount
       << ", perPerson: " << FormatAmount(amountPerPerson)
       << ", claimed: " << FormatAmount(amountClaimed)
       << " (" << claimCount << " claims)"
       << ", rate: " << ClaimRate() << "%"
       << " }";
    return ss.str();
}

// ============================================================================
// UBIDistributor
// ============================================================================

UBIDistributor::UBIDistributor(const RewardCalculator& calculator)
    : calculator_(calculator) {
}

UBIDistributor::~UBIDistributor() = default;

void UBIDistributor::AddBlockReward(int height, Amount amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    EpochId epoch = HeightToEpoch(height);
    currentEpoch_ = epoch;
    
    EpochUBIPool& pool = GetOrCreatePool(epoch);
    pool.totalPool += amount;
}

void UBIDistributor::FinalizeEpoch(EpochId epoch, uint32_t identityCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pools_.find(epoch);
    if (it == pools_.end()) {
        return;
    }
    
    EpochUBIPool& pool = it->second;
    pool.endHeight = EpochEndHeight(epoch);
    pool.claimDeadline = pool.endHeight + UBI_CLAIM_WINDOW + (UBI_GRACE_EPOCHS * EPOCH_BLOCKS);
    pool.Finalize(identityCount);
}

const EpochUBIPool* UBIDistributor::GetPool(EpochId epoch) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pools_.find(epoch);
    if (it == pools_.end()) {
        return nullptr;
    }
    return &it->second;
}

Amount UBIDistributor::GetAmountPerPerson(EpochId epoch) const {
    const EpochUBIPool* pool = GetPool(epoch);
    if (!pool || !pool->isFinalized) {
        return 0;
    }
    return pool->amountPerPerson;
}

ClaimStatus UBIDistributor::ProcessClaim(
    UBIClaim& claim,
    const Hash256& identityTreeRoot,
    int currentHeight
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    claim.submitHeight = currentHeight;
    
    // Check if epoch pool exists
    auto it = pools_.find(claim.epoch);
    if (it == pools_.end()) {
        claim.status = ClaimStatus::EpochNotComplete;
        return claim.status;
    }
    
    EpochUBIPool& pool = it->second;
    
    // Check if epoch is finalized
    if (!pool.isFinalized) {
        claim.status = ClaimStatus::EpochNotComplete;
        return claim.status;
    }
    
    // Check if still accepting claims
    if (!pool.AcceptingClaims(currentHeight)) {
        claim.status = ClaimStatus::EpochExpired;
        return claim.status;
    }
    
    // Check pool has funds
    if (pool.amountPerPerson == 0) {
        claim.status = ClaimStatus::PoolEmpty;
        return claim.status;
    }
    
    // Check for double-claim
    if (pool.IsNullifierUsed(claim.nullifier)) {
        claim.status = ClaimStatus::DoubleClaim;
        return claim.status;
    }
    
    // Verify the ZK proof using the ProofVerifier
    identity::ProofVerifier& verifier = identity::ProofVerifier::Instance();
    // For a real implementation, we would verify against the identity tree root
    // For now, we do a basic structural check
    if (!claim.proof.IsValid()) {
        claim.status = ClaimStatus::InvalidProof;
        return claim.status;
    }
    
    // Claim is valid!
    claim.amount = pool.amountPerPerson;
    claim.status = ClaimStatus::Valid;
    
    // Record the claim
    pool.RecordClaim(claim.nullifier, claim.amount);
    totalDistributed_ += claim.amount;
    totalClaims_++;
    
    return claim.status;
}

bool UBIDistributor::VerifyClaim(
    const UBIClaim& claim,
    const Hash256& identityTreeRoot,
    int currentHeight
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if epoch pool exists
    auto it = pools_.find(claim.epoch);
    if (it == pools_.end()) {
        return false;
    }
    
    const EpochUBIPool& pool = it->second;
    
    // Check if epoch is finalized
    if (!pool.isFinalized) {
        return false;
    }
    
    // Check if still accepting claims
    if (!pool.AcceptingClaims(currentHeight)) {
        return false;
    }
    
    // Check for double-claim
    if (pool.IsNullifierUsed(claim.nullifier)) {
        return false;
    }
    
    // Verify the ZK proof
    return claim.proof.IsValid();
}

bool UBIDistributor::IsEpochClaimable(EpochId epoch, int currentHeight) const {
    const EpochUBIPool* pool = GetPool(epoch);
    if (!pool) {
        return false;
    }
    return pool->isFinalized && pool->AcceptingClaims(currentHeight);
}

int UBIDistributor::GetClaimDeadline(EpochId epoch) const {
    const EpochUBIPool* pool = GetPool(epoch);
    if (!pool) {
        return -1;
    }
    return pool->claimDeadline;
}

double UBIDistributor::GetAverageClaimRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pools_.empty()) {
        return 0.0;
    }
    
    double totalRate = 0.0;
    int count = 0;
    
    for (const auto& [epoch, pool] : pools_) {
        if (pool.isFinalized) {
            totalRate += pool.ClaimRate();
            count++;
        }
    }
    
    return count > 0 ? totalRate / count : 0.0;
}

UBIDistributor::EpochStats UBIDistributor::GetEpochStats(EpochId epoch) const {
    EpochStats stats;
    stats.epoch = epoch;
    
    const EpochUBIPool* pool = GetPool(epoch);
    if (pool) {
        stats.poolSize = pool->totalPool;
        stats.distributed = pool->amountClaimed;
        stats.unclaimed = pool->UnclaimedAmount();
        stats.eligibleCount = pool->eligibleCount;
        stats.claimCount = pool->claimCount;
        stats.claimRate = pool->ClaimRate();
    }
    
    return stats;
}

std::vector<Byte> UBIDistributor::Serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Byte> data;
    // Implementation would serialize all pools and statistics
    // Simplified for now
    return data;
}

bool UBIDistributor::Deserialize(const Byte* data, size_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Implementation would deserialize state
    (void)data;
    (void)len;
    return true;
}

EpochUBIPool& UBIDistributor::GetOrCreatePool(EpochId epoch) {
    auto it = pools_.find(epoch);
    if (it == pools_.end()) {
        EpochUBIPool pool;
        pool.epoch = epoch;
        pool.endHeight = EpochEndHeight(epoch);
        pools_[epoch] = pool;
        return pools_[epoch];
    }
    return it->second;
}

void UBIDistributor::PruneOldPools(EpochId currentEpoch) {
    // Keep pools for the grace period + some buffer
    EpochId cutoff = currentEpoch > (UBI_GRACE_EPOCHS + 10) 
                   ? currentEpoch - UBI_GRACE_EPOCHS - 10 
                   : 0;
    
    for (auto it = pools_.begin(); it != pools_.end(); ) {
        if (it->first < cutoff) {
            it = pools_.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// UBITransactionBuilder
// ============================================================================

std::vector<std::pair<std::vector<Byte>, Amount>> UBITransactionBuilder::BuildClaimOutputs(
    const UBIClaim& claim,
    Amount amount
) const {
    std::vector<std::pair<std::vector<Byte>, Amount>> outputs;
    
    // Create P2PKH script for recipient
    std::vector<Byte> script;
    script.reserve(25);
    script.push_back(0x76); // OP_DUP
    script.push_back(0xa9); // OP_HASH160
    script.push_back(0x14); // Push 20 bytes
    script.insert(script.end(), claim.recipient.begin(), claim.recipient.end());
    script.push_back(0x88); // OP_EQUALVERIFY
    script.push_back(0xac); // OP_CHECKSIG
    
    outputs.emplace_back(std::move(script), amount);
    
    return outputs;
}

bool UBITransactionBuilder::VerifyClaimOutputs(
    const UBIClaim& claim,
    const std::vector<std::pair<std::vector<Byte>, Amount>>& outputs
) const {
    if (outputs.empty()) {
        return false;
    }
    
    // Verify at least one output goes to the claim recipient
    for (const auto& [script, amount] : outputs) {
        if (script.size() >= 25 && 
            script[0] == 0x76 && script[1] == 0xa9 && script[2] == 0x14) {
            // Extract the hash from the script
            Hash160 scriptHash;
            std::copy(script.begin() + 3, script.begin() + 23, scriptHash.begin());
            
            if (scriptHash == claim.recipient && amount > 0) {
                return true;
            }
        }
    }
    
    return false;
}

// ============================================================================
// UBIClaimGenerator
// ============================================================================

UBIClaim UBIClaimGenerator::GenerateClaim(
    EpochId epoch,
    const identity::IdentitySecrets& secrets,
    const Hash160& recipient,
    const identity::VectorCommitment::MerkleProof& membershipProof
) {
    return UBIClaim::Create(epoch, secrets, recipient, membershipProof);
}

bool UBIClaimGenerator::CanClaim(
    EpochId epoch,
    const identity::IdentitySecrets& secrets,
    const UBIDistributor& distributor
) {
    // Check if epoch is claimable (using a high block height to ensure we're past it)
    int checkHeight = EpochEndHeight(epoch) + 1;
    
    if (!distributor.IsEpochClaimable(epoch, checkHeight)) {
        return false;
    }
    
    // Generate the nullifier that would be used
    identity::Nullifier nullifier = secrets.DeriveNullifier(epoch);
    
    // Check if this nullifier has already been used
    const EpochUBIPool* pool = distributor.GetPool(epoch);
    if (!pool) {
        return false;
    }
    
    return !pool->IsNullifierUsed(nullifier);
}

// ============================================================================
// Utility Functions
// ============================================================================

Amount CalculateExpectedUBI(uint32_t identityCount, const RewardCalculator& calculator) {
    if (identityCount < MIN_IDENTITIES_FOR_UBI) {
        return 0;
    }
    
    // Calculate total UBI pool for one epoch
    Amount epochPool = 0;
    for (int i = 0; i < EPOCH_BLOCKS; ++i) {
        epochPool += calculator.GetUBIPoolAmount(i);
    }
    
    return epochPool / identityCount;
}

Amount EstimateAnnualUBI(uint32_t identityCount, const RewardCalculator& calculator) {
    // ~365 epochs per year (one per day)
    constexpr int EPOCHS_PER_YEAR = 365;
    
    Amount perEpoch = CalculateExpectedUBI(identityCount, calculator);
    return perEpoch * EPOCHS_PER_YEAR;
}

} // namespace economics
} // namespace shurium
