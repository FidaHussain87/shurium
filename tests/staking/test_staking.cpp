// SHURIUM - Staking Module Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include <shurium/staking/staking.h>
#include <shurium/crypto/keys.h>

#include <memory>
#include <vector>

using namespace shurium;
using namespace shurium::staking;

// ============================================================================
// Test Fixture
// ============================================================================

class StakingTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<StakingEngine>();
        
        // Create test keys
        for (int i = 0; i < 5; ++i) {
            std::vector<Byte> privKeyBytes(32);
            for (int j = 0; j < 32; ++j) {
                privKeyBytes[j] = static_cast<Byte>((i + 1) * 10 + j);
            }
            testPrivateKeys_.push_back(privKeyBytes);
            testPublicKeys_.push_back(PrivateKey(privKeyBytes.data(), 32).GetPublicKey());
        }
    }
    
    ValidatorId CreateTestValidatorId(int index) {
        return CalculateValidatorId(testPublicKeys_[index]);
    }
    
    Hash160 CreateTestAddress(uint8_t id) {
        std::array<Byte, 20> data{};
        data[0] = id;
        data[19] = id;
        return Hash160(data);
    }
    
    Validator CreateTestValidator(int index, Amount stake, const std::string& moniker) {
        Validator validator;
        validator.operatorKey = testPublicKeys_[index];
        validator.id = CalculateValidatorId(validator.operatorKey);
        validator.rewardAddress = CreateTestAddress(index);
        validator.moniker = moniker;
        validator.description = "Test validator " + moniker;
        validator.selfStake = stake;
        validator.commissionRate = DEFAULT_COMMISSION_RATE;
        return validator;
    }
    
    std::vector<Byte> SignValidator(const Validator& validator, int keyIndex) {
        Hash256 hash = validator.GetHash();
        PrivateKey privKey(testPrivateKeys_[keyIndex].data(), 32);
        return privKey.Sign(hash);
    }
    
    std::unique_ptr<StakingEngine> engine_;
    std::vector<std::vector<Byte>> testPrivateKeys_;
    std::vector<PublicKey> testPublicKeys_;
};

// ============================================================================
// String Conversion Tests
// ============================================================================

TEST_F(StakingTest, ValidatorStatusToStringConversion) {
    EXPECT_STREQ(ValidatorStatusToString(ValidatorStatus::Pending), "Pending");
    EXPECT_STREQ(ValidatorStatusToString(ValidatorStatus::Active), "Active");
    EXPECT_STREQ(ValidatorStatusToString(ValidatorStatus::Inactive), "Inactive");
    EXPECT_STREQ(ValidatorStatusToString(ValidatorStatus::Jailed), "Jailed");
    EXPECT_STREQ(ValidatorStatusToString(ValidatorStatus::Tombstoned), "Tombstoned");
    EXPECT_STREQ(ValidatorStatusToString(ValidatorStatus::Unbonding), "Unbonding");
}

TEST_F(StakingTest, SlashReasonToStringConversion) {
    EXPECT_STREQ(SlashReasonToString(SlashReason::DoubleSign), "DoubleSign");
    EXPECT_STREQ(SlashReasonToString(SlashReason::Downtime), "Downtime");
    EXPECT_STREQ(SlashReasonToString(SlashReason::InvalidBlock), "InvalidBlock");
}

TEST_F(StakingTest, DelegationStatusToStringConversion) {
    EXPECT_STREQ(DelegationStatusToString(DelegationStatus::Active), "Active");
    EXPECT_STREQ(DelegationStatusToString(DelegationStatus::Unbonding), "Unbonding");
    EXPECT_STREQ(DelegationStatusToString(DelegationStatus::Completed), "Completed");
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(StakingTest, CalculateVotingPower) {
    EXPECT_EQ(CalculateVotingPower(0), 0);
    EXPECT_EQ(CalculateVotingPower(COIN), 1);
    EXPECT_EQ(CalculateVotingPower(100 * COIN), 100);
    EXPECT_EQ(CalculateVotingPower(1000000 * COIN), 1000000);
}

TEST_F(StakingTest, CalculateValidatorId) {
    ValidatorId id1 = CalculateValidatorId(testPublicKeys_[0]);
    ValidatorId id2 = CalculateValidatorId(testPublicKeys_[0]);
    ValidatorId id3 = CalculateValidatorId(testPublicKeys_[1]);
    
    // Same key = same ID
    EXPECT_EQ(id1.ToHex(), id2.ToHex());
    
    // Different key = different ID
    EXPECT_NE(id1.ToHex(), id3.ToHex());
}

TEST_F(StakingTest, FormatStakeAmount) {
    EXPECT_NE(FormatStakeAmount(0).find("0"), std::string::npos);
    EXPECT_NE(FormatStakeAmount(COIN).find("SHR"), std::string::npos);
    EXPECT_NE(FormatStakeAmount(100 * COIN).find("100"), std::string::npos);
}

TEST_F(StakingTest, CalculateAnnualReward) {
    // 5% of 1000 NXS = 50 NXS
    Amount reward = CalculateAnnualReward(1000 * COIN, 500);
    EXPECT_EQ(reward, 50 * COIN);
    
    // 10% of 10000 NXS = 1000 NXS
    reward = CalculateAnnualReward(10000 * COIN, 1000);
    EXPECT_EQ(reward, 1000 * COIN);
}

// ============================================================================
// Validator Tests
// ============================================================================

TEST_F(StakingTest, ValidatorGetTotalStake) {
    Validator validator = CreateTestValidator(0, 100000 * COIN, "Test");
    validator.delegatedStake = 50000 * COIN;
    
    EXPECT_EQ(validator.GetTotalStake(), 150000 * COIN);
}

TEST_F(StakingTest, ValidatorGetVotingPower) {
    Validator validator = CreateTestValidator(0, 100000 * COIN, "Test");
    EXPECT_EQ(validator.GetVotingPower(), 100000);
}

TEST_F(StakingTest, ValidatorCanActivate) {
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Test");
    validator.status = ValidatorStatus::Pending;
    EXPECT_TRUE(validator.CanActivate());
    
    // Below minimum stake
    validator.selfStake = MIN_VALIDATOR_STAKE - 1;
    EXPECT_FALSE(validator.CanActivate());
    
    // Wrong status
    validator.selfStake = MIN_VALIDATOR_STAKE;
    validator.status = ValidatorStatus::Active;
    EXPECT_FALSE(validator.CanActivate());
}

TEST_F(StakingTest, ValidatorCanProduceBlocks) {
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Test");
    
    validator.status = ValidatorStatus::Active;
    EXPECT_TRUE(validator.CanProduceBlocks());
    
    validator.status = ValidatorStatus::Jailed;
    EXPECT_FALSE(validator.CanProduceBlocks());
}

TEST_F(StakingTest, ValidatorIsJailExpired) {
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Test");
    validator.status = ValidatorStatus::Jailed;
    validator.jailedHeight = 1000;
    
    // Before expiry
    EXPECT_FALSE(validator.IsJailExpired(1000 + JAIL_DURATION - 1));
    
    // At expiry
    EXPECT_TRUE(validator.IsJailExpired(1000 + JAIL_DURATION));
    
    // After expiry
    EXPECT_TRUE(validator.IsJailExpired(1000 + JAIL_DURATION + 100));
}

TEST_F(StakingTest, ValidatorCalculateCommission) {
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Test");
    validator.commissionRate = 1000;  // 10%
    
    Amount reward = 1000 * COIN;
    Amount commission = validator.CalculateCommission(reward);
    EXPECT_EQ(commission, 100 * COIN);
}

TEST_F(StakingTest, ValidatorRecordBlockProducedAndMissed) {
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Test");
    
    // Record some blocks
    for (int i = 0; i < 100; ++i) {
        validator.RecordBlockProduced();
    }
    EXPECT_EQ(validator.blocksProduced, 100);
    EXPECT_EQ(validator.missedBlocksCounter, 0);
    
    // Miss some blocks
    for (int i = 0; i < 10; ++i) {
        validator.RecordBlockMissed();
    }
    EXPECT_EQ(validator.missedBlocksCounter, 10);
    EXPECT_NEAR(validator.GetMissedBlocksPercent(), 9.09, 0.1);  // 10/110
}

TEST_F(StakingTest, ValidatorGetHash) {
    Validator v1 = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Test1");
    Validator v2 = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Test1");
    Validator v3 = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Test2");
    
    // Same validator = same hash
    EXPECT_EQ(v1.GetHash().ToHex(), v2.GetHash().ToHex());
    
    // Different name = different hash
    EXPECT_NE(v1.GetHash().ToHex(), v3.GetHash().ToHex());
}

TEST_F(StakingTest, ValidatorToString) {
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "TestNode");
    validator.status = ValidatorStatus::Active;
    
    std::string str = validator.ToString();
    EXPECT_NE(str.find("Validator"), std::string::npos);
    EXPECT_NE(str.find("TestNode"), std::string::npos);
    EXPECT_NE(str.find("Active"), std::string::npos);
}


// ============================================================================
// Delegation Tests
// ============================================================================

TEST_F(StakingTest, DelegationIsUnbondingComplete) {
    Delegation delegation;
    delegation.status = DelegationStatus::Unbonding;
    delegation.unbondingHeight = 1000;
    
    EXPECT_FALSE(delegation.IsUnbondingComplete(1000 + UNBONDING_PERIOD - 1));
    EXPECT_TRUE(delegation.IsUnbondingComplete(1000 + UNBONDING_PERIOD));
}

TEST_F(StakingTest, DelegationCanClaimRewards) {
    Delegation delegation;
    delegation.pendingRewards = 100 * COIN;
    delegation.lastClaimHeight = 1000;
    
    // During cooldown
    EXPECT_FALSE(delegation.CanClaimRewards(1000 + REWARD_CLAIM_COOLDOWN - 1));
    
    // After cooldown
    EXPECT_TRUE(delegation.CanClaimRewards(1000 + REWARD_CLAIM_COOLDOWN));
    
    // No rewards
    delegation.pendingRewards = 0;
    EXPECT_FALSE(delegation.CanClaimRewards(1000 + REWARD_CLAIM_COOLDOWN));
}

TEST_F(StakingTest, DelegationGetHash) {
    Delegation d1;
    d1.delegator = CreateTestAddress(1);
    d1.validatorId = CreateTestValidatorId(0);
    d1.amount = 1000 * COIN;
    d1.creationHeight = 100;
    
    Delegation d2 = d1;
    
    EXPECT_EQ(d1.GetHash().ToHex(), d2.GetHash().ToHex());
    
    d2.amount = 2000 * COIN;
    EXPECT_NE(d1.GetHash().ToHex(), d2.GetHash().ToHex());
}

TEST_F(StakingTest, DelegationToString) {
    Delegation delegation;
    delegation.delegator = CreateTestAddress(1);
    delegation.validatorId = CreateTestValidatorId(0);
    delegation.amount = 1000 * COIN;
    delegation.status = DelegationStatus::Active;
    
    std::string str = delegation.ToString();
    EXPECT_NE(str.find("Delegation"), std::string::npos);
    EXPECT_NE(str.find("Active"), std::string::npos);
}

// ============================================================================
// ValidatorSet Tests
// ============================================================================

TEST_F(StakingTest, ValidatorSetRegisterValidator) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    
    EXPECT_TRUE(validatorSet.RegisterValidator(validator, signature));
    EXPECT_TRUE(validatorSet.ValidatorExists(validator.id));
    
    auto retrieved = validatorSet.GetValidator(validator.id);
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->moniker, "Node1");
    EXPECT_EQ(retrieved->status, ValidatorStatus::Pending);
}

TEST_F(StakingTest, ValidatorSetRegisterDuplicate) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    
    EXPECT_TRUE(validatorSet.RegisterValidator(validator, signature));
    EXPECT_FALSE(validatorSet.RegisterValidator(validator, signature));  // Duplicate
}

TEST_F(StakingTest, ValidatorSetRegisterInsufficientStake) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE - 1, "Node1");
    auto signature = SignValidator(validator, 0);
    
    EXPECT_FALSE(validatorSet.RegisterValidator(validator, signature));
}

TEST_F(StakingTest, ValidatorSetActivateValidator) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    
    validatorSet.RegisterValidator(validator, signature);
    EXPECT_TRUE(validatorSet.ActivateValidator(validator.id));
    EXPECT_TRUE(validatorSet.IsActive(validator.id));
    
    auto retrieved = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(retrieved->status, ValidatorStatus::Active);
}

TEST_F(StakingTest, ValidatorSetDeactivateValidator) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    EXPECT_TRUE(validatorSet.DeactivateValidator(validator.id, signature));
    EXPECT_FALSE(validatorSet.IsActive(validator.id));
    
    auto retrieved = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(retrieved->status, ValidatorStatus::Inactive);
}

TEST_F(StakingTest, ValidatorSetJailValidator) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    EXPECT_TRUE(validatorSet.JailValidator(validator.id, SlashReason::DoubleSign));
    EXPECT_FALSE(validatorSet.IsActive(validator.id));
    
    auto retrieved = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(retrieved->status, ValidatorStatus::Jailed);
}

TEST_F(StakingTest, ValidatorSetGetValidatorsByStatus) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    // Register multiple validators
    for (int i = 0; i < 3; ++i) {
        Validator validator = CreateTestValidator(i, MIN_VALIDATOR_STAKE, "Node" + std::to_string(i));
        auto signature = SignValidator(validator, i);
        validatorSet.RegisterValidator(validator, signature);
    }
    
    auto pending = validatorSet.GetValidatorsByStatus(ValidatorStatus::Pending);
    EXPECT_EQ(pending.size(), 3);
    
    auto active = validatorSet.GetValidatorsByStatus(ValidatorStatus::Active);
    EXPECT_EQ(active.size(), 0);
}

TEST_F(StakingTest, ValidatorSetGetActiveSet) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    // Register and activate validators with different stakes
    for (int i = 0; i < 3; ++i) {
        Amount stake = MIN_VALIDATOR_STAKE + (i * 10000 * COIN);
        Validator validator = CreateTestValidator(i, stake, "Node" + std::to_string(i));
        auto signature = SignValidator(validator, i);
        validatorSet.RegisterValidator(validator, signature);
        validatorSet.ActivateValidator(validator.id);
    }
    
    auto activeSet = validatorSet.GetActiveSet();
    EXPECT_EQ(activeSet.size(), 3);
    
    // Should be sorted by stake (descending)
    EXPECT_GE(activeSet[0].GetTotalStake(), activeSet[1].GetTotalStake());
    EXPECT_GE(activeSet[1].GetTotalStake(), activeSet[2].GetTotalStake());
}

TEST_F(StakingTest, ValidatorSetGetTotalStaked) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    Amount totalExpected = 0;
    for (int i = 0; i < 3; ++i) {
        Amount stake = MIN_VALIDATOR_STAKE + (i * 10000 * COIN);
        Validator validator = CreateTestValidator(i, stake, "Node" + std::to_string(i));
        auto signature = SignValidator(validator, i);
        validatorSet.RegisterValidator(validator, signature);
        totalExpected += stake;
    }
    
    EXPECT_EQ(validatorSet.GetTotalStaked(), totalExpected);
}

TEST_F(StakingTest, ValidatorSetGetNextProposer) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    // Register and activate validators
    for (int i = 0; i < 3; ++i) {
        Validator validator = CreateTestValidator(i, MIN_VALIDATOR_STAKE, "Node" + std::to_string(i));
        auto signature = SignValidator(validator, i);
        validatorSet.RegisterValidator(validator, signature);
        validatorSet.ActivateValidator(validator.id);
    }
    
    // Get proposers for different heights
    ValidatorId p1 = validatorSet.GetNextProposer(100);
    ValidatorId p2 = validatorSet.GetNextProposer(101);
    
    // Both should be valid validator IDs
    EXPECT_TRUE(validatorSet.ValidatorExists(p1));
    EXPECT_TRUE(validatorSet.ValidatorExists(p2));
}

// ============================================================================
// StakingPool Tests
// ============================================================================

TEST_F(StakingTest, StakingPoolDelegate) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& pool = engine_->GetStakingPool();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    // Delegate
    Hash160 delegator = CreateTestAddress(10);
    std::vector<Byte> delegatorSig(64, 0x01);
    
    auto delegationId = pool.Delegate(delegator, validator.id, 1000 * COIN, delegatorSig);
    EXPECT_TRUE(delegationId.has_value());
    
    auto delegation = pool.GetDelegation(*delegationId);
    EXPECT_TRUE(delegation.has_value());
    EXPECT_EQ(delegation->amount, 1000 * COIN);
    EXPECT_EQ(delegation->status, DelegationStatus::Active);
}

TEST_F(StakingTest, StakingPoolDelegateMinimum) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& pool = engine_->GetStakingPool();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    
    Hash160 delegator = CreateTestAddress(10);
    std::vector<Byte> delegatorSig(64, 0x01);
    
    // Below minimum
    auto result = pool.Delegate(delegator, validator.id, MIN_DELEGATION_STAKE - 1, delegatorSig);
    EXPECT_FALSE(result.has_value());
    
    // At minimum
    result = pool.Delegate(delegator, validator.id, MIN_DELEGATION_STAKE, delegatorSig);
    EXPECT_TRUE(result.has_value());
}

TEST_F(StakingTest, StakingPoolUndelegate) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& pool = engine_->GetStakingPool();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    
    Hash160 delegator = CreateTestAddress(10);
    std::vector<Byte> delegatorSig(64, 0x01);
    
    auto delegationId = pool.Delegate(delegator, validator.id, 1000 * COIN, delegatorSig);
    EXPECT_TRUE(delegationId.has_value());
    
    // Partial undelegate
    EXPECT_TRUE(pool.Undelegate(*delegationId, 500 * COIN, delegatorSig));
    
    auto delegation = pool.GetDelegation(*delegationId);
    EXPECT_EQ(delegation->amount, 500 * COIN);
}

TEST_F(StakingTest, StakingPoolGetDelegationsByDelegator) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& pool = engine_->GetStakingPool();
    
    // Register validators
    for (int i = 0; i < 2; ++i) {
        Validator validator = CreateTestValidator(i, MIN_VALIDATOR_STAKE, "Node" + std::to_string(i));
        auto signature = SignValidator(validator, i);
        validatorSet.RegisterValidator(validator, signature);
    }
    
    // Delegate to both
    Hash160 delegator = CreateTestAddress(10);
    std::vector<Byte> delegatorSig(64, 0x01);
    
    pool.Delegate(delegator, CreateTestValidatorId(0), 1000 * COIN, delegatorSig);
    pool.Delegate(delegator, CreateTestValidatorId(1), 2000 * COIN, delegatorSig);
    
    auto delegations = pool.GetDelegationsByDelegator(delegator);
    EXPECT_EQ(delegations.size(), 2);
}

TEST_F(StakingTest, StakingPoolGetTotalDelegated) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& pool = engine_->GetStakingPool();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    
    std::vector<Byte> delegatorSig(64, 0x01);
    
    // Multiple delegations to same validator
    pool.Delegate(CreateTestAddress(10), validator.id, 1000 * COIN, delegatorSig);
    pool.Delegate(CreateTestAddress(11), validator.id, 2000 * COIN, delegatorSig);
    
    EXPECT_EQ(pool.GetTotalDelegated(validator.id), 3000 * COIN);
}

// ============================================================================
// SlashingManager Tests
// ============================================================================

TEST_F(StakingTest, SlashingManagerSubmitDoubleSignEvidence) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& slashing = engine_->GetSlashingManager();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    // Create two conflicting block hashes for double sign evidence
    std::array<Byte, 32> hash1Data{};
    hash1Data[0] = 0x01;
    Hash256 blockHash1(hash1Data);
    
    std::array<Byte, 32> hash2Data{};
    hash2Data[0] = 0x02;
    Hash256 blockHash2(hash2Data);
    
    // Create valid signatures using validator's private key
    PrivateKey privKey(testPrivateKeys_[0].data(), 32);
    auto sig1 = privKey.Sign(blockHash1);
    auto sig2 = privKey.Sign(blockHash2);
    
    // Submit evidence - should succeed with valid signatures
    EXPECT_TRUE(slashing.SubmitDoubleSignEvidence(
        validator.id, blockHash1, blockHash2, 1000, sig1, sig2));
    
    // Validator should be jailed or tombstoned for double sign
    auto updated = validatorSet.GetValidator(validator.id);
    EXPECT_TRUE(updated->status == ValidatorStatus::Jailed || 
                updated->status == ValidatorStatus::Tombstoned);
    
    // Check slash event was recorded
    auto events = slashing.GetSlashEvents(validator.id);
    EXPECT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].reason, SlashReason::DoubleSign);
}

TEST_F(StakingTest, SlashingManagerSubmitDoubleSignEvidenceInvalidSig) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& slashing = engine_->GetSlashingManager();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    // Create two conflicting block hashes
    std::array<Byte, 32> hash1Data{};
    hash1Data[0] = 0x01;
    Hash256 blockHash1(hash1Data);
    
    std::array<Byte, 32> hash2Data{};
    hash2Data[0] = 0x02;
    Hash256 blockHash2(hash2Data);
    
    // Use invalid signatures (random bytes)
    std::vector<Byte> sig1(64, 0x01);
    std::vector<Byte> sig2(64, 0x02);
    
    // Should fail with invalid signatures
    EXPECT_FALSE(slashing.SubmitDoubleSignEvidence(
        validator.id, blockHash1, blockHash2, 1000, sig1, sig2));
    
    // Validator should still be active
    auto updated = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(updated->status, ValidatorStatus::Active);
}

TEST_F(StakingTest, SlashingManagerReportDowntime) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& slashing = engine_->GetSlashingManager();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    // Simulate missing enough blocks to trigger downtime slash
    // MAX_MISSED_BLOCKS = 500
    for (int i = 0; i < MAX_MISSED_BLOCKS; ++i) {
        validatorSet.RecordBlockMissed(validator.id);
    }
    
    // Report downtime - should succeed now
    EXPECT_TRUE(slashing.ReportDowntime(validator.id));
    
    // Validator should be jailed
    auto updated = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(updated->status, ValidatorStatus::Jailed);
}

TEST_F(StakingTest, SlashingManagerReportDowntimeInsufficientMissed) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& slashing = engine_->GetSlashingManager();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    // Miss some blocks but not enough
    for (int i = 0; i < MAX_MISSED_BLOCKS - 1; ++i) {
        validatorSet.RecordBlockMissed(validator.id);
    }
    
    // Report downtime - should fail (not enough missed blocks)
    EXPECT_FALSE(slashing.ReportDowntime(validator.id));
    
    // Validator should still be active
    auto updated = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(updated->status, ValidatorStatus::Active);
}

TEST_F(StakingTest, SlashingManagerGetSlashEvents) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& slashing = engine_->GetSlashingManager();
    
    // Register validators
    for (int i = 0; i < 2; ++i) {
        Validator validator = CreateTestValidator(i, MIN_VALIDATOR_STAKE, "Node" + std::to_string(i));
        auto signature = SignValidator(validator, i);
        validatorSet.RegisterValidator(validator, signature);
        validatorSet.ActivateValidator(validator.id);
    }
    
    // Make first validator miss enough blocks
    for (int i = 0; i < MAX_MISSED_BLOCKS; ++i) {
        validatorSet.RecordBlockMissed(CreateTestValidatorId(0));
    }
    
    // Slash first validator for downtime
    slashing.ReportDowntime(CreateTestValidatorId(0));
    
    // Get events for first validator
    auto events = slashing.GetSlashEvents(CreateTestValidatorId(0));
    EXPECT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].reason, SlashReason::Downtime);
    
    // Second validator should have no events
    auto events2 = slashing.GetSlashEvents(CreateTestValidatorId(1));
    EXPECT_EQ(events2.size(), 0);
}

TEST_F(StakingTest, SlashingManagerReportInvalidBlock) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& slashing = engine_->GetSlashingManager();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    // Create block hash
    std::array<Byte, 32> hashData{};
    hashData[0] = 0x01;
    Hash256 blockHash(hashData);
    
    // Report invalid block
    EXPECT_TRUE(slashing.ReportInvalidBlock(validator.id, blockHash, "Invalid state root"));
    
    // Validator should be jailed
    auto updated = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(updated->status, ValidatorStatus::Jailed);
}

TEST_F(StakingTest, SlashingManagerSlashAmounts) {
    // Double sign slashing should be more severe than downtime
    Amount stakeAmount = 100000 * COIN;
    
    Amount doubleSignSlash = (stakeAmount * DOUBLE_SIGN_SLASH_RATE) / 10000;
    Amount downtimeSlash = (stakeAmount * DOWNTIME_SLASH_RATE) / 10000;
    
    EXPECT_GT(doubleSignSlash, downtimeSlash);
    EXPECT_EQ(doubleSignSlash, 5000 * COIN);  // 5% of 100k
    EXPECT_EQ(downtimeSlash, 100 * COIN);     // 0.1% of 100k
}

TEST_F(StakingTest, SlashingManagerGetTotalSlashed) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& slashing = engine_->GetSlashingManager();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    Amount initialTotal = slashing.GetTotalSlashed();
    
    // Make validator miss enough blocks for downtime
    for (int i = 0; i < MAX_MISSED_BLOCKS; ++i) {
        validatorSet.RecordBlockMissed(validator.id);
    }
    
    // Slash for downtime
    EXPECT_TRUE(slashing.ReportDowntime(validator.id));
    
    // Total slashed should increase
    EXPECT_GT(slashing.GetTotalSlashed(), initialTotal);
}

TEST_F(StakingTest, SlashingManagerIsEvidenceSubmitted) {
    auto& slashing = engine_->GetSlashingManager();
    
    // Create evidence hash
    std::array<Byte, 32> hashData{};
    hashData[0] = 0xAB;
    Hash256 evidenceHash(hashData);
    
    // Initially not submitted
    EXPECT_FALSE(slashing.IsEvidenceSubmitted(evidenceHash));
}

// ============================================================================
// RewardDistributor Tests
// ============================================================================

TEST_F(StakingTest, RewardDistributorCalculateBlockReward) {
    auto& rewards = engine_->GetRewardDistributor();
    
    // Base reward should be positive
    Amount reward = rewards.CalculateBlockReward();
    EXPECT_GE(reward, 0);
}

TEST_F(StakingTest, RewardDistributorDistributeBlockReward) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& rewards = engine_->GetRewardDistributor();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    Amount initialTotal = rewards.GetTotalRewardsDistributed();
    
    // Distribute reward
    Amount blockReward = 10 * COIN;
    rewards.DistributeBlockReward(validator.id, blockReward);
    
    // Total should increase
    EXPECT_EQ(rewards.GetTotalRewardsDistributed(), initialTotal + blockReward);
}

TEST_F(StakingTest, RewardDistributorCommissionDeduction) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& rewards = engine_->GetRewardDistributor();
    
    // Register validator with specific commission
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    validator.commissionRate = 1000;  // 10%
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    Amount blockReward = 100 * COIN;
    rewards.DistributeBlockReward(validator.id, blockReward);
    
    // Rewards should be distributed
    EXPECT_EQ(rewards.GetTotalRewardsDistributed(), blockReward);
}

TEST_F(StakingTest, RewardDistributorCalculateValidatorReward) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& rewards = engine_->GetRewardDistributor();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    Amount blockReward = 100 * COIN;
    Amount validatorReward = rewards.CalculateValidatorReward(validator.id, blockReward);
    
    // Validator reward should be positive and <= block reward
    EXPECT_GE(validatorReward, 0);
    EXPECT_LE(validatorReward, blockReward);
}

TEST_F(StakingTest, RewardDistributorGetTotalRewardsDistributed) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& rewards = engine_->GetRewardDistributor();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    Amount initialTotal = rewards.GetTotalRewardsDistributed();
    
    // Distribute some rewards
    rewards.DistributeBlockReward(validator.id, 100 * COIN);
    rewards.DistributeBlockReward(validator.id, 100 * COIN);
    
    EXPECT_EQ(rewards.GetTotalRewardsDistributed(), initialTotal + 200 * COIN);
}

TEST_F(StakingTest, RewardDistributorGetEpochRewards) {
    auto& rewards = engine_->GetRewardDistributor();
    
    // Initially epoch rewards should be 0
    Amount epochRewards = rewards.GetEpochRewards();
    EXPECT_GE(epochRewards, 0);
}

TEST_F(StakingTest, RewardDistributorGetCurrentEpoch) {
    auto& rewards = engine_->GetRewardDistributor();
    
    int epoch = rewards.GetCurrentEpoch();
    EXPECT_GE(epoch, 0);
}

TEST_F(StakingTest, RewardDistributorGetEstimatedAPY) {
    auto& rewards = engine_->GetRewardDistributor();
    
    int apy = rewards.GetEstimatedAPY();
    // APY should be reasonable (0-100% in basis points = 0-10000)
    EXPECT_GE(apy, 0);
    EXPECT_LE(apy, 10000);
}

TEST_F(StakingTest, RewardDistributorCalculateAnnualReward) {
    auto& rewards = engine_->GetRewardDistributor();
    
    Amount stake = 100000 * COIN;
    Amount annual = rewards.CalculateAnnualReward(stake);
    
    // Annual reward should be positive for positive stake
    EXPECT_GT(annual, 0);
}

// ============================================================================
// StakingEngine Integration Tests
// ============================================================================

TEST_F(StakingTest, StakingEngineInitialization) {
    // Component accessors should work
    ValidatorSet& vs = engine_->GetValidatorSet();
    StakingPool& sp = engine_->GetStakingPool();
    SlashingManager& sm = engine_->GetSlashingManager();
    RewardDistributor& rd = engine_->GetRewardDistributor();
    
    // Just verify they're accessible (no nullptr checks needed for references)
    EXPECT_EQ(vs.GetValidatorCount(ValidatorStatus::Active), 0);
    EXPECT_EQ(sp.GetTotalDelegated(CreateTestValidatorId(0)), 0);
    EXPECT_EQ(sm.GetTotalSlashed(), 0);
    EXPECT_GE(rd.GetTotalRewardsDistributed(), 0);
}

TEST_F(StakingTest, StakingEngineProcessBlock) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    // Process block (returns void)
    engine_->ProcessBlock(1, validator.id, 10 * COIN);
    
    // Validator should have recorded a block
    auto updated = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(updated->blocksProduced, 1);
}

TEST_F(StakingTest, StakingEngineProcessBlockMultiple) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    // Process multiple blocks
    for (int height = 1; height <= 100; ++height) {
        engine_->ProcessBlock(height, validator.id, 10 * COIN);
    }
    
    auto updated = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(updated->blocksProduced, 100);
}

TEST_F(StakingTest, StakingEngineFullWorkflow) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& pool = engine_->GetStakingPool();
    auto& rewards = engine_->GetRewardDistributor();
    
    // 1. Register validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto validatorSig = SignValidator(validator, 0);
    ASSERT_TRUE(validatorSet.RegisterValidator(validator, validatorSig));
    
    // 2. Activate validator
    ASSERT_TRUE(validatorSet.ActivateValidator(validator.id));
    EXPECT_TRUE(validatorSet.IsActive(validator.id));
    
    // 3. Add delegations
    Hash160 delegator1 = CreateTestAddress(10);
    Hash160 delegator2 = CreateTestAddress(11);
    std::vector<Byte> delegatorSig(64, 0x01);
    
    auto d1 = pool.Delegate(delegator1, validator.id, 5000 * COIN, delegatorSig);
    auto d2 = pool.Delegate(delegator2, validator.id, 10000 * COIN, delegatorSig);
    ASSERT_TRUE(d1.has_value());
    ASSERT_TRUE(d2.has_value());
    
    // 4. Process blocks (simulate block production)
    for (int height = 1; height <= 50; ++height) {
        engine_->ProcessBlock(height, validator.id, 10 * COIN);
    }
    
    // 5. Verify rewards distributed
    EXPECT_GT(rewards.GetTotalRewardsDistributed(), 0);
    
    // 6. Verify validator stats
    auto updated = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(updated->blocksProduced, 50);
    
    // 7. Verify total delegated
    EXPECT_EQ(pool.GetTotalDelegated(validator.id), 15000 * COIN);
}

TEST_F(StakingTest, StakingEngineMultipleValidators) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    // Register multiple validators
    std::vector<ValidatorId> validatorIds;
    for (int i = 0; i < 3; ++i) {
        Amount stake = MIN_VALIDATOR_STAKE + (i * 50000 * COIN);
        Validator validator = CreateTestValidator(i, stake, "Node" + std::to_string(i));
        auto signature = SignValidator(validator, i);
        
        ASSERT_TRUE(validatorSet.RegisterValidator(validator, signature));
        ASSERT_TRUE(validatorSet.ActivateValidator(validator.id));
        validatorIds.push_back(validator.id);
    }
    
    // Verify active set
    auto activeSet = validatorSet.GetActiveSet();
    EXPECT_EQ(activeSet.size(), 3);
    
    // Process blocks from different validators
    for (int height = 1; height <= 30; ++height) {
        int proposerIndex = height % 3;
        engine_->ProcessBlock(height, validatorIds[proposerIndex], 10 * COIN);
    }
    
    // Each validator should have produced 10 blocks
    for (const auto& id : validatorIds) {
        auto v = validatorSet.GetValidator(id);
        EXPECT_EQ(v->blocksProduced, 10);
    }
}

TEST_F(StakingTest, StakingEngineValidatorJailAndUnjail) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& slashing = engine_->GetSlashingManager();
    
    // Register and activate validator
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    EXPECT_TRUE(validatorSet.IsActive(validator.id));
    
    // Make validator miss enough blocks for downtime
    for (int i = 0; i < MAX_MISSED_BLOCKS; ++i) {
        validatorSet.RecordBlockMissed(validator.id);
    }
    
    // Jail for downtime
    EXPECT_TRUE(slashing.ReportDowntime(validator.id));
    
    EXPECT_FALSE(validatorSet.IsActive(validator.id));
    auto jailed = validatorSet.GetValidator(validator.id);
    EXPECT_EQ(jailed->status, ValidatorStatus::Jailed);
    
    // Process blocks to advance past jail duration
    // JAIL_DURATION = 8640 blocks
    for (int h = 1; h <= JAIL_DURATION + 1; ++h) {
        engine_->ProcessBlock(h, validator.id, 0);  // 0 reward since jailed
    }
    
    // Unjail after jail period
    EXPECT_TRUE(validatorSet.UnjailValidator(validator.id, signature));
    
    auto unjailed = validatorSet.GetValidator(validator.id);
    // After unjail, validator goes to Pending status (needs reactivation)
    EXPECT_EQ(unjailed->status, ValidatorStatus::Pending);
}

TEST_F(StakingTest, StakingEngineDelegationRewardsFlow) {
    auto& validatorSet = engine_->GetValidatorSet();
    auto& pool = engine_->GetStakingPool();
    auto& rewards = engine_->GetRewardDistributor();
    
    // Register and activate validator with 10% commission
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    validator.commissionRate = 1000;  // 10%
    auto validatorSig = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, validatorSig);
    validatorSet.ActivateValidator(validator.id);
    
    // Add large delegation
    Hash160 delegator = CreateTestAddress(10);
    std::vector<Byte> delegatorSig(64, 0x01);
    pool.Delegate(delegator, validator.id, 100000 * COIN, delegatorSig);
    
    // Process many blocks
    for (int height = 1; height <= 100; ++height) {
        engine_->ProcessBlock(height, validator.id, 10 * COIN);
    }
    
    // Check that rewards have been distributed
    Amount totalDistributed = rewards.GetTotalRewardsDistributed();
    EXPECT_EQ(totalDistributed, 1000 * COIN);  // 100 blocks * 10 COIN each
}

TEST_F(StakingTest, StakingEngineGetCurrentHeight) {
    // Process blocks and verify height tracking
    auto& validatorSet = engine_->GetValidatorSet();
    
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    validatorSet.RegisterValidator(validator, signature);
    validatorSet.ActivateValidator(validator.id);
    
    engine_->ProcessBlock(1, validator.id, 10 * COIN);
    engine_->ProcessBlock(2, validator.id, 10 * COIN);
    engine_->ProcessBlock(3, validator.id, 10 * COIN);
    
    EXPECT_EQ(engine_->GetCurrentHeight(), 3);
}

TEST_F(StakingTest, StakingEngineGetTotalStaked) {
    auto& validatorSet = engine_->GetValidatorSet();
    
    // Register validators
    for (int i = 0; i < 3; ++i) {
        Validator validator = CreateTestValidator(i, MIN_VALIDATOR_STAKE, "Node" + std::to_string(i));
        auto signature = SignValidator(validator, i);
        validatorSet.RegisterValidator(validator, signature);
    }
    
    EXPECT_EQ(engine_->GetTotalStaked(), 3 * MIN_VALIDATOR_STAKE);
}

TEST_F(StakingTest, StakingEngineGetNetworkAPY) {
    int apy = engine_->GetNetworkAPY();
    
    // APY should be reasonable
    EXPECT_GE(apy, 0);
    EXPECT_LE(apy, 10000);  // Up to 100%
}

TEST_F(StakingTest, StakingEngineConvenienceMethods) {
    // Test RegisterValidator convenience wrapper
    Validator validator = CreateTestValidator(0, MIN_VALIDATOR_STAKE, "Node1");
    auto signature = SignValidator(validator, 0);
    
    EXPECT_TRUE(engine_->RegisterValidator(validator, signature));
    
    // Test Delegate convenience wrapper
    engine_->GetValidatorSet().ActivateValidator(validator.id);
    
    Hash160 delegator = CreateTestAddress(10);
    std::vector<Byte> delegatorSig(64, 0x01);
    
    auto delegationId = engine_->Delegate(delegator, validator.id, 1000 * COIN, delegatorSig);
    EXPECT_TRUE(delegationId.has_value());
}

