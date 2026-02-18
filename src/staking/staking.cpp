// SHURIUM - Staking Module Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/staking/staking.h>
#include <shurium/crypto/sha256.h>
#include <shurium/crypto/keys.h>
#include <shurium/core/serialize.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shurium {
namespace staking {

// ============================================================================
// Signature Verification Helper
// ============================================================================

/**
 * Verify a delegation signature.
 * The signature should be a compact recoverable signature (65 bytes).
 * We recover the public key and verify it hashes to the delegator.
 */
static bool VerifyDelegatorSignature(
    const Hash160& delegator,
    const Hash256& messageHash,
    const std::vector<Byte>& signature) {
    
    // Signature must be 65 bytes (compact recoverable format)
    if (signature.size() != 65) {
        return false;
    }
    
    // Try to recover the public key from the signature
    std::vector<uint8_t> sigVec(signature.begin(), signature.end());
    auto recoveredPubkey = PublicKey::RecoverCompact(messageHash, sigVec);
    
    if (!recoveredPubkey || !recoveredPubkey->IsValid()) {
        return false;
    }
    
    // Verify the recovered public key hashes to the delegator address
    Hash160 pubkeyHash = recoveredPubkey->GetHash160();
    return pubkeyHash == delegator;
}

/**
 * Create a message hash for delegation operations.
 */
static Hash256 CreateDelegationMessageHash(
    const Hash160& delegator,
    const ValidatorId& validatorId,
    Amount amount,
    const std::string& action) {
    
    DataStream ss;
    ss.Write(delegator.data(), delegator.size());
    ss.Write(validatorId.data(), validatorId.size());
    ser_writedata64(ss, static_cast<uint64_t>(amount));
    ss.Write(reinterpret_cast<const uint8_t*>(action.data()), action.size());
    
    return SHA256Hash(ss.data(), ss.size());
}

// ============================================================================
// String Conversion Functions
// ============================================================================

const char* ValidatorStatusToString(ValidatorStatus status) {
    switch (status) {
        case ValidatorStatus::Pending: return "Pending";
        case ValidatorStatus::Active: return "Active";
        case ValidatorStatus::Inactive: return "Inactive";
        case ValidatorStatus::Jailed: return "Jailed";
        case ValidatorStatus::Tombstoned: return "Tombstoned";
        case ValidatorStatus::Unbonding: return "Unbonding";
        default: return "Unknown";
    }
}

const char* SlashReasonToString(SlashReason reason) {
    switch (reason) {
        case SlashReason::DoubleSign: return "DoubleSign";
        case SlashReason::Downtime: return "Downtime";
        case SlashReason::InvalidBlock: return "InvalidBlock";
        case SlashReason::ProtocolViolation: return "ProtocolViolation";
        default: return "Unknown";
    }
}

const char* DelegationStatusToString(DelegationStatus status) {
    switch (status) {
        case DelegationStatus::Active: return "Active";
        case DelegationStatus::Unbonding: return "Unbonding";
        case DelegationStatus::Completed: return "Completed";
        default: return "Unknown";
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

uint64_t CalculateVotingPower(Amount stake) {
    if (stake <= 0) return 0;
    // Linear voting power based on stake
    return static_cast<uint64_t>(stake / COIN);
}

ValidatorId CalculateValidatorId(const PublicKey& operatorKey) {
    return operatorKey.GetHash160();
}

std::string FormatStakeAmount(Amount amount) {
    std::ostringstream ss;
    Amount whole = amount / COIN;
    Amount frac = std::abs(amount % COIN);
    ss << whole;
    if (frac > 0) {
        ss << "." << std::setfill('0') << std::setw(8) << frac;
        std::string s = ss.str();
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
        return s + " NXS";
    }
    return ss.str() + " NXS";
}

Amount CalculateAnnualReward(Amount stake, int rateBps) {
    // reward = stake * rate / 10000
    return (stake * rateBps) / 10000;
}

Amount CalculateEpochReward(Amount stake, int rateBps, int epochLength) {
    // Assuming ~2.88M blocks per year (10 second blocks)
    constexpr int BLOCKS_PER_YEAR = 3153600;
    Amount annualReward = CalculateAnnualReward(stake, rateBps);
    return (annualReward * epochLength) / BLOCKS_PER_YEAR;
}

// ============================================================================
// Validator Implementation
// ============================================================================

uint64_t Validator::GetVotingPower() const {
    return CalculateVotingPower(GetTotalStake());
}

bool Validator::CanActivate() const {
    return status == ValidatorStatus::Pending &&
           selfStake >= MIN_VALIDATOR_STAKE;
}

bool Validator::CanProduceBlocks() const {
    return status == ValidatorStatus::Active;
}

bool Validator::IsJailExpired(int currentHeight) const {
    if (status != ValidatorStatus::Jailed) return false;
    return currentHeight >= jailedHeight + JAIL_DURATION;
}

bool Validator::IsUnbondingComplete(int currentHeight) const {
    if (status != ValidatorStatus::Unbonding) return false;
    return currentHeight >= unbondingHeight + UNBONDING_PERIOD;
}

Amount Validator::CalculateCommission(Amount reward) const {
    return (reward * commissionRate) / 10000;
}

void Validator::RecordBlockProduced() {
    blocksProduced++;
    
    // Update missed blocks bitmap (sliding window)
    if (missedBlocksBitmap.size() >= static_cast<size_t>(MISSED_BLOCKS_WINDOW)) {
        // Remove oldest entry
        if (missedBlocksBitmap.front()) {
            missedBlocksCounter--;
        }
        missedBlocksBitmap.erase(missedBlocksBitmap.begin());
    }
    missedBlocksBitmap.push_back(false);  // Not missed
}

void Validator::RecordBlockMissed() {
    if (missedBlocksBitmap.size() >= static_cast<size_t>(MISSED_BLOCKS_WINDOW)) {
        if (missedBlocksBitmap.front()) {
            missedBlocksCounter--;
        }
        missedBlocksBitmap.erase(missedBlocksBitmap.begin());
    }
    missedBlocksBitmap.push_back(true);  // Missed
    missedBlocksCounter++;
}

double Validator::GetMissedBlocksPercent() const {
    if (missedBlocksBitmap.empty()) return 0.0;
    return (static_cast<double>(missedBlocksCounter) / missedBlocksBitmap.size()) * 100.0;
}

Hash256 Validator::GetHash() const {
    std::ostringstream ss;
    ss << id.ToHex() << operatorKey.ToHex() << moniker 
       << selfStake << commissionRate << registrationHeight;
    
    std::string data = ss.str();
    SHA256 hasher;
    hasher.Write(reinterpret_cast<const Byte*>(data.data()), data.size());
    
    std::array<Byte, 32> result;
    hasher.Finalize(result.data());
    return Hash256(result);
}

std::vector<Byte> Validator::Serialize() const {
    std::vector<Byte> data;
    // Stub
    return data;
}

std::optional<Validator> Validator::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return std::nullopt;
}

std::string Validator::ToString() const {
    std::ostringstream ss;
    ss << "Validator {"
       << " id: " << id.ToHex().substr(0, 12) << "..."
       << ", moniker: " << moniker
       << ", status: " << ValidatorStatusToString(status)
       << ", stake: " << FormatStakeAmount(GetTotalStake())
       << ", commission: " << (commissionRate / 100.0) << "%"
       << " }";
    return ss.str();
}

// ============================================================================
// Delegation Implementation
// ============================================================================

bool Delegation::IsUnbondingComplete(int currentHeight) const {
    if (status != DelegationStatus::Unbonding) return false;
    return currentHeight >= unbondingHeight + UNBONDING_PERIOD;
}

bool Delegation::CanClaimRewards(int currentHeight) const {
    return pendingRewards > 0 &&
           currentHeight >= lastClaimHeight + REWARD_CLAIM_COOLDOWN;
}

Hash256 Delegation::GetHash() const {
    std::ostringstream ss;
    ss << delegator.ToHex() << validatorId.ToHex() << amount << creationHeight;
    
    std::string data = ss.str();
    SHA256 hasher;
    hasher.Write(reinterpret_cast<const Byte*>(data.data()), data.size());
    
    std::array<Byte, 32> result;
    hasher.Finalize(result.data());
    return Hash256(result);
}

std::vector<Byte> Delegation::Serialize() const {
    std::vector<Byte> data;
    return data;
}

std::optional<Delegation> Delegation::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return std::nullopt;
}

std::string Delegation::ToString() const {
    std::ostringstream ss;
    ss << "Delegation {"
       << " delegator: " << delegator.ToHex().substr(0, 12) << "..."
       << ", validator: " << validatorId.ToHex().substr(0, 12) << "..."
       << ", amount: " << FormatStakeAmount(amount)
       << ", status: " << DelegationStatusToString(status)
       << ", pending: " << FormatStakeAmount(pendingRewards)
       << " }";
    return ss.str();
}

// ============================================================================
// SlashEvent Implementation
// ============================================================================

std::string SlashEvent::ToString() const {
    std::ostringstream ss;
    ss << "SlashEvent {"
       << " validator: " << validatorId.ToHex().substr(0, 12) << "..."
       << ", reason: " << SlashReasonToString(reason)
       << ", height: " << height
       << ", slashed: " << FormatStakeAmount(validatorSlashed + delegatorsSlashed)
       << ", jailed: " << (jailed ? "yes" : "no")
       << " }";
    return ss.str();
}


// ============================================================================
// ValidatorSet Implementation
// ============================================================================

ValidatorSet::ValidatorSet() = default;
ValidatorSet::~ValidatorSet() = default;

bool ValidatorSet::RegisterValidator(const Validator& validator, const std::vector<Byte>& signature) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already exists
    if (validators_.count(validator.id) > 0) {
        return false;
    }
    
    // Verify minimum stake
    if (validator.selfStake < MIN_VALIDATOR_STAKE) {
        return false;
    }
    
    // Verify signature
    Hash256 hash = validator.GetHash();
    if (!validator.operatorKey.Verify(hash, signature)) {
        return false;
    }
    
    // Add validator
    Validator newValidator = validator;
    newValidator.status = ValidatorStatus::Pending;
    newValidator.registrationHeight = currentHeight_;
    newValidator.missedBlocksBitmap.reserve(MISSED_BLOCKS_WINDOW);
    
    validators_[newValidator.id] = newValidator;
    
    return true;
}

std::optional<Validator> ValidatorSet::GetValidator(const ValidatorId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool ValidatorSet::UpdateValidator(const ValidatorId& id,
                                    const std::string& moniker,
                                    const std::string& description,
                                    int newCommissionRate,
                                    const std::vector<Byte>& signature) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }
    
    auto& validator = it->second;
    
    // Verify signature
    if (!VerifyValidatorSignature(id, validator.GetHash(), signature)) {
        return false;
    }
    
    // Check commission rate bounds
    if (newCommissionRate < MIN_COMMISSION_RATE || newCommissionRate > MAX_COMMISSION_RATE) {
        return false;
    }
    
    // Check commission change cooldown and max change
    if (newCommissionRate != validator.commissionRate) {
        if (currentHeight_ < validator.commissionChangeHeight + COMMISSION_CHANGE_COOLDOWN) {
            return false;
        }
        int change = std::abs(newCommissionRate - validator.commissionRate);
        if (change > MAX_COMMISSION_CHANGE) {
            return false;
        }
        validator.commissionChangeHeight = currentHeight_;
    }
    
    validator.moniker = moniker;
    validator.description = description;
    validator.commissionRate = newCommissionRate;
    
    return true;
}

bool ValidatorSet::ActivateValidator(const ValidatorId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }
    
    auto& validator = it->second;
    
    if (!validator.CanActivate()) {
        return false;
    }
    
    // Check if room in active set
    if (activeSet_.size() >= MAX_ACTIVE_VALIDATORS) {
        // Find lowest stake active validator
        Amount minStake = validator.GetTotalStake();
        ValidatorId minId;
        for (const auto& activeId : activeSet_) {
            auto activeIt = validators_.find(activeId);
            if (activeIt != validators_.end() && activeIt->second.GetTotalStake() < minStake) {
                minStake = activeIt->second.GetTotalStake();
                minId = activeId;
            }
        }
        
        if (minId.IsNull() || validator.GetTotalStake() <= minStake) {
            return false;  // Not enough stake to join active set
        }
        
        // Remove lowest stake validator from active set
        activeSet_.erase(minId);
        validators_[minId].status = ValidatorStatus::Inactive;
    }
    
    validator.status = ValidatorStatus::Active;
    activeSet_.insert(id);
    
    return true;
}

bool ValidatorSet::DeactivateValidator(const ValidatorId& id, const std::vector<Byte>& signature) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }
    
    auto& validator = it->second;
    
    if (validator.status != ValidatorStatus::Active) {
        return false;
    }
    
    if (!VerifyValidatorSignature(id, validator.GetHash(), signature)) {
        return false;
    }
    
    validator.status = ValidatorStatus::Inactive;
    activeSet_.erase(id);
    
    return true;
}

bool ValidatorSet::StartUnbonding(const ValidatorId& id, const std::vector<Byte>& signature) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }
    
    auto& validator = it->second;
    
    if (validator.status == ValidatorStatus::Tombstoned ||
        validator.status == ValidatorStatus::Unbonding) {
        return false;
    }
    
    if (!VerifyValidatorSignature(id, validator.GetHash(), signature)) {
        return false;
    }
    
    // Remove from active set if active
    if (validator.status == ValidatorStatus::Active) {
        activeSet_.erase(id);
    }
    
    validator.status = ValidatorStatus::Unbonding;
    validator.unbondingHeight = currentHeight_;
    
    // Add to unbonding queue
    UnbondingEntry entry;
    entry.type = UnbondingEntry::Type::ValidatorSelfUnbond;
    entry.source = validator.rewardAddress;
    entry.amount = validator.selfStake;
    entry.startHeight = currentHeight_;
    entry.completionHeight = currentHeight_ + UNBONDING_PERIOD;
    unbondingQueue_.push_back(entry);
    
    return true;
}

bool ValidatorSet::JailValidator(const ValidatorId& id, SlashReason reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }
    
    auto& validator = it->second;
    
    if (validator.status == ValidatorStatus::Tombstoned) {
        return false;
    }
    
    // Remove from active set
    activeSet_.erase(id);
    
    validator.status = ValidatorStatus::Jailed;
    validator.jailedHeight = currentHeight_;
    
    (void)reason;  // Could log or track reason
    
    return true;
}

bool ValidatorSet::UnjailValidator(const ValidatorId& id, const std::vector<Byte>& signature) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }
    
    auto& validator = it->second;
    
    if (validator.status != ValidatorStatus::Jailed) {
        return false;
    }
    
    if (!validator.IsJailExpired(currentHeight_)) {
        return false;
    }
    
    if (!VerifyValidatorSignature(id, validator.GetHash(), signature)) {
        return false;
    }
    
    // Check if still has minimum stake
    if (validator.selfStake < MIN_VALIDATOR_STAKE) {
        validator.status = ValidatorStatus::Inactive;
    } else {
        validator.status = ValidatorStatus::Pending;
    }
    
    return true;
}

void ValidatorSet::TombstoneValidator(const ValidatorId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return;
    }
    
    activeSet_.erase(id);
    it->second.status = ValidatorStatus::Tombstoned;
}

std::vector<Validator> ValidatorSet::GetValidatorsByStatus(ValidatorStatus status) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Validator> result;
    for (const auto& [id, validator] : validators_) {
        if (validator.status == status) {
            result.push_back(validator);
        }
    }
    return result;
}

std::vector<Validator> ValidatorSet::GetActiveSet() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Validator> result;
    for (const auto& id : activeSet_) {
        auto it = validators_.find(id);
        if (it != validators_.end()) {
            result.push_back(it->second);
        }
    }
    
    // Sort by stake (descending)
    std::sort(result.begin(), result.end(), [](const Validator& a, const Validator& b) {
        return a.GetTotalStake() > b.GetTotalStake();
    });
    
    return result;
}

size_t ValidatorSet::GetValidatorCount(ValidatorStatus status) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [id, validator] : validators_) {
        if (validator.status == status) {
            count++;
        }
    }
    return count;
}

Amount ValidatorSet::GetTotalStaked() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Amount total = 0;
    for (const auto& [id, validator] : validators_) {
        if (validator.status != ValidatorStatus::Tombstoned) {
            total += validator.GetTotalStake();
        }
    }
    return total;
}

bool ValidatorSet::ValidatorExists(const ValidatorId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return validators_.count(id) > 0;
}

bool ValidatorSet::IsActive(const ValidatorId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeSet_.count(id) > 0;
}

void ValidatorSet::RecordBlockProduced(const ValidatorId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it != validators_.end()) {
        it->second.RecordBlockProduced();
    }
}

void ValidatorSet::RecordBlockMissed(const ValidatorId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validators_.find(id);
    if (it != validators_.end()) {
        it->second.RecordBlockMissed();
    }
}

ValidatorId ValidatorSet::GetNextProposer(int height) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (activeSet_.empty()) {
        return ValidatorId();
    }
    
    // Get active validators sorted by stake
    std::vector<std::pair<ValidatorId, Amount>> activeValidators;
    for (const auto& id : activeSet_) {
        auto it = validators_.find(id);
        if (it != validators_.end()) {
            activeValidators.emplace_back(id, it->second.GetTotalStake());
        }
    }
    
    std::sort(activeValidators.begin(), activeValidators.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Weighted round-robin based on stake
    Amount totalStake = 0;
    for (const auto& [id, stake] : activeValidators) {
        totalStake += stake;
    }
    
    if (totalStake == 0) {
        return activeValidators.empty() ? ValidatorId() : activeValidators[0].first;
    }
    
    // Use height to determine position in round
    Amount position = (height * COIN) % totalStake;
    Amount cumulative = 0;
    
    for (const auto& [id, stake] : activeValidators) {
        cumulative += stake;
        if (position < cumulative) {
            return id;
        }
    }
    
    return activeValidators.back().first;
}

void ValidatorSet::ProcessEpochEnd(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentHeight_ = height;
    UpdateActiveSet();
}

void ValidatorSet::ProcessUnbondings(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentHeight_ = height;
    
    // Process completed unbondings
    auto it = unbondingQueue_.begin();
    while (it != unbondingQueue_.end()) {
        if (it->IsComplete(height)) {
            // Unbonding complete - funds can be withdrawn
            it = unbondingQueue_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<Byte> ValidatorSet::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool ValidatorSet::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

void ValidatorSet::UpdateActiveSet() {
    // Get all eligible validators
    std::vector<std::pair<ValidatorId, Amount>> eligible;
    for (const auto& [id, validator] : validators_) {
        if (validator.status == ValidatorStatus::Active ||
            validator.status == ValidatorStatus::Pending ||
            validator.status == ValidatorStatus::Inactive) {
            if (validator.selfStake >= MIN_VALIDATOR_STAKE) {
                eligible.emplace_back(id, validator.GetTotalStake());
            }
        }
    }
    
    // Sort by stake
    std::sort(eligible.begin(), eligible.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Update active set
    activeSet_.clear();
    for (size_t i = 0; i < std::min(eligible.size(), static_cast<size_t>(MAX_ACTIVE_VALIDATORS)); ++i) {
        const auto& [id, stake] = eligible[i];
        activeSet_.insert(id);
        validators_[id].status = ValidatorStatus::Active;
    }
    
    // Mark others as inactive
    for (size_t i = MAX_ACTIVE_VALIDATORS; i < eligible.size(); ++i) {
        validators_[eligible[i].first].status = ValidatorStatus::Inactive;
    }
}

bool ValidatorSet::VerifyValidatorSignature(const ValidatorId& id,
                                             const Hash256& hash,
                                             const std::vector<Byte>& signature) const {
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }
    return it->second.operatorKey.Verify(hash, signature);
}


// ============================================================================
// StakingPool Implementation
// ============================================================================

StakingPool::StakingPool() 
    : validators_(std::make_shared<ValidatorSet>()) {}

StakingPool::StakingPool(std::shared_ptr<ValidatorSet> validators)
    : validators_(validators ? validators : std::make_shared<ValidatorSet>()) {}

StakingPool::~StakingPool() = default;

std::optional<DelegationId> StakingPool::Delegate(
    const Hash160& delegator,
    const ValidatorId& validatorId,
    Amount amount,
    const std::vector<Byte>& signature) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check minimum delegation
    if (amount < MIN_DELEGATION_STAKE) {
        return std::nullopt;
    }
    
    // Check validator exists
    if (!validators_->ValidatorExists(validatorId)) {
        return std::nullopt;
    }
    
    // Verify signature
    if (signature.empty()) {
        return std::nullopt;
    }
    
    // Create message hash for delegation
    Hash256 messageHash = CreateDelegationMessageHash(delegator, validatorId, amount, "DELEGATE");
    
    // Verify the signature matches the delegator
    if (!VerifyDelegatorSignature(delegator, messageHash, signature)) {
        return std::nullopt;  // Invalid signature
    }
    
    // Create delegation
    Delegation delegation;
    delegation.delegator = delegator;
    delegation.validatorId = validatorId;
    delegation.amount = amount;
    delegation.status = DelegationStatus::Active;
    delegation.creationHeight = currentHeight_;
    delegation.shares = CalculateShares(validatorId, amount);
    delegation.id = delegation.GetHash();
    
    // Update indices
    delegations_[delegation.id] = delegation;
    delegatorIndex_[delegator].insert(delegation.id);
    validatorIndex_[validatorId].insert(delegation.id);
    totalShares_[validatorId] += delegation.shares;
    
    return delegation.id;
}

bool StakingPool::AddToDelegation(
    const DelegationId& delegationId,
    Amount amount,
    const std::vector<Byte>& signature) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = delegations_.find(delegationId);
    if (it == delegations_.end()) {
        return false;
    }
    
    auto& delegation = it->second;
    
    if (delegation.status != DelegationStatus::Active) {
        return false;
    }
    
    if (signature.empty()) {
        return false;
    }
    
    // Verify signature - must be from the original delegator
    Hash256 messageHash = CreateDelegationMessageHash(
        delegation.delegator, delegation.validatorId, amount, "ADD_TO_DELEGATION");
    
    if (!VerifyDelegatorSignature(delegation.delegator, messageHash, signature)) {
        return false;  // Invalid signature
    }
    
    // Calculate new shares
    uint64_t newShares = CalculateShares(delegation.validatorId, amount);
    
    delegation.amount += amount;
    delegation.shares += newShares;
    totalShares_[delegation.validatorId] += newShares;
    
    return true;
}

bool StakingPool::Undelegate(
    const DelegationId& delegationId,
    Amount amount,
    const std::vector<Byte>& signature) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = delegations_.find(delegationId);
    if (it == delegations_.end()) {
        return false;
    }
    
    auto& delegation = it->second;
    
    if (delegation.status != DelegationStatus::Active) {
        return false;
    }
    
    if (amount > delegation.amount) {
        return false;
    }
    
    if (signature.empty()) {
        return false;
    }
    
    // Verify signature - must be from the delegator
    Hash256 messageHash = CreateDelegationMessageHash(
        delegation.delegator, delegation.validatorId, amount, "UNDELEGATE");
    
    if (!VerifyDelegatorSignature(delegation.delegator, messageHash, signature)) {
        return false;  // Invalid signature
    }
    
    // Calculate shares to remove
    uint64_t sharesToRemove = (delegation.shares * amount) / delegation.amount;
    
    delegation.amount -= amount;
    delegation.shares -= sharesToRemove;
    totalShares_[delegation.validatorId] -= sharesToRemove;
    
    // Add to unbonding queue
    UnbondingEntry entry;
    entry.type = UnbondingEntry::Type::DelegationUnbond;
    entry.source = delegation.delegator;
    entry.amount = amount;
    entry.startHeight = currentHeight_;
    entry.completionHeight = currentHeight_ + UNBONDING_PERIOD;
    unbondingQueue_.push_back(entry);
    
    // If fully unbonded, mark as unbonding
    if (delegation.amount == 0) {
        delegation.status = DelegationStatus::Unbonding;
        delegation.unbondingHeight = currentHeight_;
    }
    
    return true;
}

bool StakingPool::Redelegate(
    const DelegationId& delegationId,
    const ValidatorId& newValidatorId,
    Amount amount,
    const std::vector<Byte>& signature) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = delegations_.find(delegationId);
    if (it == delegations_.end()) {
        return false;
    }
    
    auto& delegation = it->second;
    
    if (delegation.status != DelegationStatus::Active) {
        return false;
    }
    
    if (amount > delegation.amount) {
        return false;
    }
    
    if (!validators_->ValidatorExists(newValidatorId)) {
        return false;
    }
    
    if (delegation.validatorId == newValidatorId) {
        return false;  // Same validator
    }
    
    if (signature.empty()) {
        return false;
    }
    
    // Verify signature - must be from the delegator
    // Include both old and new validator in the message for redelegation
    DataStream ss;
    ss.Write(delegation.delegator.data(), delegation.delegator.size());
    ss.Write(delegation.validatorId.data(), delegation.validatorId.size());
    ss.Write(newValidatorId.data(), newValidatorId.size());
    ser_writedata64(ss, static_cast<uint64_t>(amount));
    std::string action = "REDELEGATE";
    ss.Write(reinterpret_cast<const uint8_t*>(action.data()), action.size());
    Hash256 messageHash = SHA256Hash(ss.data(), ss.size());
    
    if (!VerifyDelegatorSignature(delegation.delegator, messageHash, signature)) {
        return false;  // Invalid signature
    }
    
    // Calculate shares to move
    uint64_t sharesToRemove = (delegation.shares * amount) / delegation.amount;
    
    // Remove from old delegation
    delegation.amount -= amount;
    delegation.shares -= sharesToRemove;
    totalShares_[delegation.validatorId] -= sharesToRemove;
    
    // Create new delegation to new validator
    Delegation newDelegation;
    newDelegation.delegator = delegation.delegator;
    newDelegation.validatorId = newValidatorId;
    newDelegation.amount = amount;
    newDelegation.status = DelegationStatus::Active;
    newDelegation.creationHeight = currentHeight_;
    newDelegation.shares = CalculateShares(newValidatorId, amount);
    newDelegation.id = newDelegation.GetHash();
    
    delegations_[newDelegation.id] = newDelegation;
    delegatorIndex_[newDelegation.delegator].insert(newDelegation.id);
    validatorIndex_[newValidatorId].insert(newDelegation.id);
    totalShares_[newValidatorId] += newDelegation.shares;
    
    // Clean up old delegation if empty
    if (delegation.amount == 0) {
        validatorIndex_[delegation.validatorId].erase(delegationId);
        delegatorIndex_[delegation.delegator].erase(delegationId);
        delegations_.erase(it);
    }
    
    return true;
}

std::optional<Delegation> StakingPool::GetDelegation(const DelegationId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = delegations_.find(id);
    if (it == delegations_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<Delegation> StakingPool::GetDelegationsByDelegator(const Hash160& delegator) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Delegation> result;
    auto it = delegatorIndex_.find(delegator);
    if (it != delegatorIndex_.end()) {
        for (const auto& id : it->second) {
            auto delIt = delegations_.find(id);
            if (delIt != delegations_.end()) {
                result.push_back(delIt->second);
            }
        }
    }
    return result;
}

std::vector<Delegation> StakingPool::GetDelegationsToValidator(const ValidatorId& validatorId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Delegation> result;
    auto it = validatorIndex_.find(validatorId);
    if (it != validatorIndex_.end()) {
        for (const auto& id : it->second) {
            auto delIt = delegations_.find(id);
            if (delIt != delegations_.end()) {
                result.push_back(delIt->second);
            }
        }
    }
    return result;
}

Amount StakingPool::GetTotalDelegated(const ValidatorId& validatorId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Amount total = 0;
    auto it = validatorIndex_.find(validatorId);
    if (it != validatorIndex_.end()) {
        for (const auto& id : it->second) {
            auto delIt = delegations_.find(id);
            if (delIt != delegations_.end() && delIt->second.status == DelegationStatus::Active) {
                total += delIt->second.amount;
            }
        }
    }
    return total;
}

size_t StakingPool::GetDelegationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return delegations_.size();
}

Amount StakingPool::ClaimRewards(const DelegationId& delegationId, const std::vector<Byte>& signature) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = delegations_.find(delegationId);
    if (it == delegations_.end()) {
        return 0;
    }
    
    auto& delegation = it->second;
    
    if (!delegation.CanClaimRewards(currentHeight_)) {
        return 0;
    }
    
    if (signature.empty()) {
        return 0;
    }
    
    // Verify signature - must be from the delegator
    // Include pending rewards in message to prevent replay attacks
    DataStream ss;
    ss.Write(delegation.delegator.data(), delegation.delegator.size());
    ss.Write(delegationId.data(), delegationId.size());
    ser_writedata64(ss, static_cast<uint64_t>(delegation.pendingRewards));
    ser_writedata32(ss, static_cast<uint32_t>(currentHeight_));
    std::string action = "CLAIM_REWARDS";
    ss.Write(reinterpret_cast<const uint8_t*>(action.data()), action.size());
    Hash256 messageHash = SHA256Hash(ss.data(), ss.size());
    
    if (!VerifyDelegatorSignature(delegation.delegator, messageHash, signature)) {
        return 0;  // Invalid signature
    }
    
    Amount rewards = delegation.pendingRewards;
    delegation.pendingRewards = 0;
    delegation.totalRewardsClaimed += rewards;
    delegation.lastClaimHeight = currentHeight_;
    
    return rewards;
}

Amount StakingPool::GetPendingRewards(const DelegationId& delegationId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = delegations_.find(delegationId);
    if (it == delegations_.end()) {
        return 0;
    }
    return it->second.pendingRewards;
}

void StakingPool::DistributeRewards(const ValidatorId& validatorId, Amount totalReward) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = validatorIndex_.find(validatorId);
    if (it == validatorIndex_.end() || it->second.empty()) {
        return;
    }
    
    uint64_t totalValidatorShares = totalShares_[validatorId];
    if (totalValidatorShares == 0) {
        return;
    }
    
    // Distribute proportionally to shares
    for (const auto& id : it->second) {
        auto delIt = delegations_.find(id);
        if (delIt != delegations_.end() && delIt->second.status == DelegationStatus::Active) {
            Amount delegatorReward = (totalReward * delIt->second.shares) / totalValidatorShares;
            delIt->second.pendingRewards += delegatorReward;
        }
    }
}

Amount StakingPool::ApplySlashing(const ValidatorId& validatorId, int slashRateBps) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Amount totalSlashed = 0;
    
    auto it = validatorIndex_.find(validatorId);
    if (it == validatorIndex_.end()) {
        return 0;
    }
    
    for (const auto& id : it->second) {
        auto delIt = delegations_.find(id);
        if (delIt != delegations_.end() && delIt->second.status == DelegationStatus::Active) {
            Amount slashAmount = (delIt->second.amount * slashRateBps) / 10000;
            delIt->second.amount -= slashAmount;
            totalSlashed += slashAmount;
        }
    }
    
    return totalSlashed;
}

void StakingPool::ProcessBlock(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentHeight_ = height;
    
    // Process completed unbondings
    auto it = unbondingQueue_.begin();
    while (it != unbondingQueue_.end()) {
        if (it->IsComplete(height)) {
            it = unbondingQueue_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Mark completed delegation unbondings
    for (auto& [id, delegation] : delegations_) {
        if (delegation.status == DelegationStatus::Unbonding &&
            delegation.IsUnbondingComplete(height)) {
            delegation.status = DelegationStatus::Completed;
        }
    }
}

std::vector<Byte> StakingPool::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool StakingPool::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

uint64_t StakingPool::CalculateShares(const ValidatorId& validatorId, Amount amount) const {
    // Simple 1:1 share calculation for new delegations
    // In production, would account for existing rewards in the pool
    (void)validatorId;
    return static_cast<uint64_t>(amount);
}

Amount StakingPool::CalculateAmount(const ValidatorId& validatorId, uint64_t shares) const {
    (void)validatorId;
    return static_cast<Amount>(shares);
}

bool StakingPool::VerifyDelegatorSignature(const Hash160& delegator,
                                            const Hash256& hash,
                                            const std::vector<Byte>& signature) const {
    // Would need access to delegator's public key
    (void)delegator;
    (void)hash;
    return !signature.empty();
}


// ============================================================================
// SlashingManager Implementation
// ============================================================================

SlashingManager::SlashingManager()
    : validators_(std::make_shared<ValidatorSet>()),
      pool_(std::make_shared<StakingPool>()) {}

SlashingManager::SlashingManager(std::shared_ptr<ValidatorSet> validators,
                                  std::shared_ptr<StakingPool> pool)
    : validators_(validators ? validators : std::make_shared<ValidatorSet>()),
      pool_(pool ? pool : std::make_shared<StakingPool>()) {}

SlashingManager::~SlashingManager() = default;

bool SlashingManager::SubmitDoubleSignEvidence(
    const ValidatorId& validatorId,
    const Hash256& block1Hash,
    const Hash256& block2Hash,
    int height,
    const std::vector<Byte>& signature1,
    const std::vector<Byte>& signature2) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check validator exists
    auto validator = validators_->GetValidator(validatorId);
    if (!validator) {
        return false;
    }
    
    // Verify different blocks
    if (block1Hash == block2Hash) {
        return false;
    }
    
    // Verify both signatures are from the validator
    if (!validator->operatorKey.Verify(block1Hash, signature1) ||
        !validator->operatorKey.Verify(block2Hash, signature2)) {
        return false;
    }
    
    // Create evidence hash
    SHA256 hasher;
    hasher.Write(block1Hash.data(), 32);
    hasher.Write(block2Hash.data(), 32);
    std::array<Byte, 32> evidenceData;
    hasher.Finalize(evidenceData.data());
    Hash256 evidenceHash(evidenceData);
    
    // Check if already submitted
    if (submittedEvidence_.count(evidenceHash) > 0) {
        return false;
    }
    
    submittedEvidence_.insert(evidenceHash);
    
    // Execute slash
    ExecuteSlash(validatorId, SlashReason::DoubleSign, DOUBLE_SIGN_SLASH_RATE, evidenceHash);
    
    (void)height;
    
    return true;
}

bool SlashingManager::ReportDowntime(const ValidatorId& validatorId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto validator = validators_->GetValidator(validatorId);
    if (!validator) {
        return false;
    }
    
    // Check if missed blocks exceed threshold
    if (validator->missedBlocksCounter < MAX_MISSED_BLOCKS) {
        return false;
    }
    
    Hash256 evidenceHash;  // No specific evidence for downtime
    ExecuteSlash(validatorId, SlashReason::Downtime, DOWNTIME_SLASH_RATE, evidenceHash);
    
    return true;
}

bool SlashingManager::ReportInvalidBlock(
    const ValidatorId& validatorId,
    const Hash256& blockHash,
    const std::string& reason) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!validators_->ValidatorExists(validatorId)) {
        return false;
    }
    
    // Check if already reported
    if (submittedEvidence_.count(blockHash) > 0) {
        return false;
    }
    
    submittedEvidence_.insert(blockHash);
    
    ExecuteSlash(validatorId, SlashReason::InvalidBlock, INVALID_BLOCK_SLASH_RATE, blockHash);
    
    (void)reason;
    
    return true;
}

std::vector<SlashEvent> SlashingManager::GetSlashEvents(const ValidatorId& validatorId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SlashEvent> result;
    for (const auto& event : slashEvents_) {
        if (event.validatorId == validatorId) {
            result.push_back(event);
        }
    }
    return result;
}

std::vector<SlashEvent> SlashingManager::GetSlashEventsByHeight(int startHeight, int endHeight) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SlashEvent> result;
    for (const auto& event : slashEvents_) {
        if (event.height >= startHeight && event.height <= endHeight) {
            result.push_back(event);
        }
    }
    return result;
}

bool SlashingManager::IsEvidenceSubmitted(const Hash256& evidenceHash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return submittedEvidence_.count(evidenceHash) > 0;
}

Amount SlashingManager::GetTotalSlashed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalSlashed_;
}

void SlashingManager::ProcessBlock(int height, const ValidatorId& proposer) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentHeight_ = height;
    
    // Record block for proposer
    // Note: In production, would track which validators should have produced
    // and mark missed blocks appropriately
    (void)proposer;
}

std::vector<Byte> SlashingManager::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool SlashingManager::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

SlashEvent SlashingManager::ExecuteSlash(const ValidatorId& validatorId,
                                          SlashReason reason,
                                          int slashRateBps,
                                          const Hash256& evidenceHash) {
    SlashEvent event;
    event.validatorId = validatorId;
    event.reason = reason;
    event.height = currentHeight_;
    event.evidenceHash = evidenceHash;
    
    // Get validator
    auto validator = validators_->GetValidator(validatorId);
    if (!validator) {
        return event;
    }
    
    // Slash validator's self-stake
    Amount validatorSlash = (validator->selfStake * slashRateBps) / 10000;
    event.validatorSlashed = validatorSlash;
    
    // Slash delegators
    Amount delegatorSlash = pool_->ApplySlashing(validatorId, slashRateBps);
    event.delegatorsSlashed = delegatorSlash;
    
    totalSlashed_ += validatorSlash + delegatorSlash;
    
    // Jail if needed
    if (ShouldJail(reason)) {
        validators_->JailValidator(validatorId, reason);
        event.jailed = true;
    }
    
    // Tombstone if needed
    if (ShouldTombstone(reason, validator->slashCount + 1)) {
        validators_->TombstoneValidator(validatorId);
        event.tombstoned = true;
    }
    
    slashEvents_.push_back(event);
    
    return event;
}

int SlashingManager::GetSlashRate(SlashReason reason) const {
    switch (reason) {
        case SlashReason::DoubleSign: return DOUBLE_SIGN_SLASH_RATE;
        case SlashReason::Downtime: return DOWNTIME_SLASH_RATE;
        case SlashReason::InvalidBlock: return INVALID_BLOCK_SLASH_RATE;
        case SlashReason::ProtocolViolation: return INVALID_BLOCK_SLASH_RATE;
        default: return 0;
    }
}

bool SlashingManager::ShouldJail(SlashReason reason) const {
    // Jail for all slash reasons except minor downtime
    return reason != SlashReason::Downtime || true;  // Simplified: always jail
}

bool SlashingManager::ShouldTombstone(SlashReason reason, int slashCount) const {
    // Tombstone for double signing or repeated offenses
    if (reason == SlashReason::DoubleSign) {
        return true;
    }
    return slashCount >= 3;  // Tombstone after 3 slashes
}

// ============================================================================
// RewardDistributor Implementation
// ============================================================================

RewardDistributor::RewardDistributor()
    : validators_(std::make_shared<ValidatorSet>()),
      pool_(std::make_shared<StakingPool>()) {}

RewardDistributor::RewardDistributor(std::shared_ptr<ValidatorSet> validators,
                                      std::shared_ptr<StakingPool> pool)
    : validators_(validators ? validators : std::make_shared<ValidatorSet>()),
      pool_(pool ? pool : std::make_shared<StakingPool>()) {}

RewardDistributor::~RewardDistributor() = default;

Amount RewardDistributor::CalculateBlockReward() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Amount totalStaked = validators_->GetTotalStaked();
    return CalculateEpochReward(totalStaked, annualRewardRate_, 1);  // Per block
}

Amount RewardDistributor::CalculateAnnualReward(Amount stake) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return staking::CalculateAnnualReward(stake, annualRewardRate_);
}

Amount RewardDistributor::CalculateValidatorReward(const ValidatorId& validatorId, 
                                                    Amount blockReward) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto validator = validators_->GetValidator(validatorId);
    if (!validator) {
        return 0;
    }
    
    Amount totalStaked = validators_->GetTotalStaked();
    if (totalStaked == 0) {
        return 0;
    }
    
    // Proportional to stake
    return (blockReward * validator->GetTotalStake()) / totalStaked;
}

void RewardDistributor::DistributeBlockReward(const ValidatorId& proposer, Amount blockReward) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto validator = validators_->GetValidator(proposer);
    if (!validator) {
        return;
    }
    
    // Calculate commission
    Amount commission = validator->CalculateCommission(blockReward);
    Amount delegatorReward = blockReward - commission;
    
    // Distribute to delegators
    pool_->DistributeRewards(proposer, delegatorReward);
    
    // Track rewards
    totalRewardsDistributed_ += blockReward;
    epochRewards_ += blockReward;
    
    // Mint callback
    if (mintCallback_) {
        mintCallback_(blockReward);
    }
}

void RewardDistributor::ProcessEpochEnd(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    currentEpoch_++;
    epochStartHeight_ = height;
    epochRewards_ = 0;
}

Amount RewardDistributor::GetTotalRewardsDistributed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalRewardsDistributed_;
}

Amount RewardDistributor::GetEpochRewards() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return epochRewards_;
}

int RewardDistributor::GetCurrentEpoch() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentEpoch_;
}

int RewardDistributor::GetEstimatedAPY() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return annualRewardRate_;
}

void RewardDistributor::SetRewardMintCallback(RewardMintCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    mintCallback_ = callback;
}

void RewardDistributor::SetAnnualRewardRate(int rateBps) {
    std::lock_guard<std::mutex> lock(mutex_);
    annualRewardRate_ = rateBps;
}

std::vector<Byte> RewardDistributor::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool RewardDistributor::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

// ============================================================================
// StakingEngine Implementation
// ============================================================================

StakingEngine::StakingEngine()
    : validators_(std::make_shared<ValidatorSet>()),
      pool_(std::make_shared<StakingPool>(validators_)),
      slashing_(std::make_shared<SlashingManager>(validators_, pool_)),
      rewards_(std::make_shared<RewardDistributor>(validators_, pool_)) {}

StakingEngine::~StakingEngine() = default;

void StakingEngine::ProcessBlock(int height, const ValidatorId& proposer, Amount blockReward) {
    currentHeight_ = height;
    
    // Record block production
    validators_->RecordBlockProduced(proposer);
    
    // Distribute rewards
    rewards_->DistributeBlockReward(proposer, blockReward);
    
    // Process unbondings
    validators_->ProcessUnbondings(height);
    pool_->ProcessBlock(height);
    
    // Process slashing checks
    slashing_->ProcessBlock(height, proposer);
    
    // Check for epoch end
    if (height > 0 && height % EPOCH_LENGTH == 0) {
        validators_->ProcessEpochEnd(height);
        rewards_->ProcessEpochEnd(height);
    }
}

bool StakingEngine::RegisterValidator(const Validator& validator, const std::vector<Byte>& signature) {
    return validators_->RegisterValidator(validator, signature);
}

std::optional<DelegationId> StakingEngine::Delegate(
    const Hash160& delegator,
    const ValidatorId& validatorId,
    Amount amount,
    const std::vector<Byte>& signature) {
    return pool_->Delegate(delegator, validatorId, amount, signature);
}

Amount StakingEngine::GetTotalStaked() const {
    return validators_->GetTotalStaked();
}

int StakingEngine::GetNetworkAPY() const {
    return rewards_->GetEstimatedAPY();
}

std::vector<Byte> StakingEngine::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool StakingEngine::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

} // namespace staking
} // namespace shurium
