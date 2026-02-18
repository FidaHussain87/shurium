// SHURIUM - Governance Module Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include <shurium/governance/governance.h>
#include <shurium/crypto/keys.h>

#include <memory>
#include <vector>

using namespace shurium;
using namespace shurium::governance;

// ============================================================================
// Test Fixture
// ============================================================================

class GovernanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<GovernanceEngine>();
        
        // Create test keys
        testPrivateKey_.resize(32);
        for (int i = 0; i < 32; ++i) {
            testPrivateKey_[i] = static_cast<Byte>(i + 1);
        }
        testPublicKey_ = PrivateKey(testPrivateKey_.data(), 32).GetPublicKey();
        
        // Set up some initial voting power
        SetupVoters();
    }
    
    void SetupVoters() {
        for (int i = 0; i < 10; ++i) {
            VoterId voter = CreateTestVoterId(i);
            engine_->UpdateVotingPower(voter, 1000 + i * 100);
        }
    }
    
    VoterId CreateTestVoterId(uint8_t id) {
        std::array<Byte, 20> data{};
        data[0] = id;
        data[19] = id;
        return VoterId(data);
    }
    
    GovernanceProposalId CreateTestProposalId(uint8_t id) {
        std::array<Byte, 32> data{};
        data[0] = id;
        data[31] = id;
        return GovernanceProposalId(data);
    }
    
    GovernanceProposal CreateTestProposal(ProposalType type, const std::string& title) {
        GovernanceProposal proposal;
        proposal.type = type;
        proposal.title = title;
        proposal.description = "Test proposal description for " + title;
        proposal.proposer = testPublicKey_;
        proposal.deposit = 1000 * COIN;
        
        // Set appropriate payload based on type
        if (type == ProposalType::Parameter) {
            std::vector<ParameterChange> changes;
            ParameterChange change;
            change.parameter = GovernableParameter::TransactionFeeMultiplier;
            change.currentValue = int64_t(100);
            change.newValue = int64_t(110);
            changes.push_back(change);
            proposal.payload = changes;
        } else if (type == ProposalType::Protocol) {
            ProtocolUpgrade upgrade;
            upgrade.newVersion = 0x00010100; // 1.1.0
            upgrade.minClientVersion = 0x00010000;
            upgrade.activationHeight = 10000;
            proposal.payload = upgrade;
        } else if (type == ProposalType::Constitutional) {
            ConstitutionalChange change;
            change.article = ConstitutionalArticle::GovernanceProcess;
            change.currentText = "Old text";
            change.newText = "New text";
            change.rationale = "Improvement";
            proposal.payload = change;
        } else {
            proposal.payload = std::string("Signal message");
        }
        
        return proposal;
    }
    
    std::vector<Byte> SignProposal(const GovernanceProposal& proposal) {
        Hash256 hash = proposal.CalculateHash();
        PrivateKey privKey(testPrivateKey_.data(), testPrivateKey_.size());
        return privKey.Sign(hash);
    }
    
    Vote CreateTestVote(const GovernanceProposalId& proposalId, 
                        const VoterId& voter, 
                        VoteChoice choice,
                        uint64_t power) {
        Vote vote;
        vote.proposalId = proposalId;
        vote.voter = voter;
        vote.choice = choice;
        vote.votingPower = power;
        vote.voteHeight = engine_->GetCurrentHeight();
        vote.reason = "Test vote";
        // Add dummy signature for test
        vote.signature.resize(64, 0x01);
        return vote;
    }
    
    std::unique_ptr<GovernanceEngine> engine_;
    std::vector<Byte> testPrivateKey_;
    PublicKey testPublicKey_;
};

// ============================================================================
// String Conversion Tests
// ============================================================================

TEST_F(GovernanceTest, ProposalTypeToStringConversion) {
    EXPECT_STREQ(ProposalTypeToString(ProposalType::Parameter), "Parameter");
    EXPECT_STREQ(ProposalTypeToString(ProposalType::Protocol), "Protocol");
    EXPECT_STREQ(ProposalTypeToString(ProposalType::Constitutional), "Constitutional");
    EXPECT_STREQ(ProposalTypeToString(ProposalType::Emergency), "Emergency");
    EXPECT_STREQ(ProposalTypeToString(ProposalType::Signal), "Signal");
}

TEST_F(GovernanceTest, ParseProposalType) {
    EXPECT_EQ(ParseProposalType("Parameter"), ProposalType::Parameter);
    EXPECT_EQ(ParseProposalType("protocol"), ProposalType::Protocol);
    EXPECT_EQ(ParseProposalType("Constitutional"), ProposalType::Constitutional);
    EXPECT_FALSE(ParseProposalType("invalid").has_value());
}

TEST_F(GovernanceTest, GovernanceStatusToStringConversion) {
    EXPECT_STREQ(GovernanceStatusToString(GovernanceStatus::Draft), "Draft");
    EXPECT_STREQ(GovernanceStatusToString(GovernanceStatus::Active), "Active");
    EXPECT_STREQ(GovernanceStatusToString(GovernanceStatus::Approved), "Approved");
    EXPECT_STREQ(GovernanceStatusToString(GovernanceStatus::Rejected), "Rejected");
    EXPECT_STREQ(GovernanceStatusToString(GovernanceStatus::Executed), "Executed");
}

TEST_F(GovernanceTest, VoteChoiceToStringConversion) {
    EXPECT_STREQ(VoteChoiceToString(VoteChoice::Yes), "Yes");
    EXPECT_STREQ(VoteChoiceToString(VoteChoice::No), "No");
    EXPECT_STREQ(VoteChoiceToString(VoteChoice::Abstain), "Abstain");
    EXPECT_STREQ(VoteChoiceToString(VoteChoice::NoWithVeto), "NoWithVeto");
}

TEST_F(GovernanceTest, GovernableParameterToStringConversion) {
    EXPECT_STREQ(GovernableParameterToString(GovernableParameter::TransactionFeeMultiplier), 
                 "TransactionFeeMultiplier");
    EXPECT_STREQ(GovernableParameterToString(GovernableParameter::BlockSizeLimit), 
                 "BlockSizeLimit");
}

TEST_F(GovernanceTest, ParseGovernableParameter) {
    EXPECT_EQ(ParseGovernableParameter("TransactionFeeMultiplier"), 
              GovernableParameter::TransactionFeeMultiplier);
    EXPECT_EQ(ParseGovernableParameter("BlockSizeLimit"), 
              GovernableParameter::BlockSizeLimit);
    EXPECT_FALSE(ParseGovernableParameter("invalid").has_value());
}

TEST_F(GovernanceTest, ConstitutionalArticleToStringConversion) {
    EXPECT_STREQ(ConstitutionalArticleToString(ConstitutionalArticle::GovernanceProcess),
                 "GovernanceProcess");
    EXPECT_STREQ(ConstitutionalArticleToString(ConstitutionalArticle::EconomicPolicy),
                 "EconomicPolicy");
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(GovernanceTest, FormatGovernanceAmount) {
    EXPECT_NE(FormatGovernanceAmount(0).find("0"), std::string::npos);
    EXPECT_NE(FormatGovernanceAmount(COIN).find("SHR"), std::string::npos);
    EXPECT_NE(FormatGovernanceAmount(100 * COIN).find("100"), std::string::npos);
}

TEST_F(GovernanceTest, CalculateVotingPower) {
    // Below minimum stake = 0 power
    EXPECT_EQ(CalculateVotingPower(MIN_VOTING_STAKE - 1), 0);
    
    // At minimum stake = some power
    EXPECT_GT(CalculateVotingPower(MIN_VOTING_STAKE), 0);
    
    // More stake = more power (but diminishing)
    uint64_t power100 = CalculateVotingPower(100 * COIN);
    uint64_t power400 = CalculateVotingPower(400 * COIN);
    
    // 4x stake should give < 4x power (sqrt curve)
    EXPECT_GT(power400, power100);
    EXPECT_LT(power400, 4 * power100);
}

TEST_F(GovernanceTest, GetParameterDefault) {
    auto defaultFee = GetParameterDefault(GovernableParameter::TransactionFeeMultiplier);
    EXPECT_TRUE(std::holds_alternative<int64_t>(defaultFee));
    EXPECT_EQ(std::get<int64_t>(defaultFee), 100);
    
    auto defaultBlockSize = GetParameterDefault(GovernableParameter::BlockSizeLimit);
    EXPECT_TRUE(std::holds_alternative<int64_t>(defaultBlockSize));
    EXPECT_EQ(std::get<int64_t>(defaultBlockSize), 4 * 1024 * 1024);
}

TEST_F(GovernanceTest, GetParameterMinMax) {
    auto minFee = GetParameterMin(GovernableParameter::TransactionFeeMultiplier);
    auto maxFee = GetParameterMax(GovernableParameter::TransactionFeeMultiplier);
    
    EXPECT_TRUE(minFee.has_value());
    EXPECT_TRUE(maxFee.has_value());
    EXPECT_LT(*minFee, *maxFee);
}

TEST_F(GovernanceTest, ValidateParameterBounds) {
    // Valid value
    EXPECT_TRUE(ValidateParameterBounds(GovernableParameter::TransactionFeeMultiplier, int64_t(100)));
    
    // Below minimum
    EXPECT_FALSE(ValidateParameterBounds(GovernableParameter::TransactionFeeMultiplier, int64_t(1)));
    
    // Above maximum
    EXPECT_FALSE(ValidateParameterBounds(GovernableParameter::TransactionFeeMultiplier, int64_t(100000)));
}


// ============================================================================
// ParameterChange Tests
// ============================================================================

TEST_F(GovernanceTest, ParameterChangeIsValid) {
    ParameterChange change;
    change.parameter = GovernableParameter::TransactionFeeMultiplier;
    change.currentValue = int64_t(100);
    change.newValue = int64_t(150);
    
    EXPECT_TRUE(change.IsValid());
    
    // Invalid: out of bounds
    change.newValue = int64_t(1); // Too low
    EXPECT_FALSE(change.IsValid());
}

TEST_F(GovernanceTest, ParameterChangeToString) {
    ParameterChange change;
    change.parameter = GovernableParameter::TransactionFeeMultiplier;
    change.currentValue = int64_t(100);
    change.newValue = int64_t(150);
    
    std::string str = change.ToString();
    EXPECT_NE(str.find("TransactionFeeMultiplier"), std::string::npos);
    EXPECT_NE(str.find("100"), std::string::npos);
    EXPECT_NE(str.find("150"), std::string::npos);
}

// ============================================================================
// ProtocolUpgrade Tests
// ============================================================================

TEST_F(GovernanceTest, ProtocolUpgradeFormatVersion) {
    EXPECT_EQ(ProtocolUpgrade::FormatVersion(0x00010000), "1.0.0");
    EXPECT_EQ(ProtocolUpgrade::FormatVersion(0x00010100), "1.1.0");
    EXPECT_EQ(ProtocolUpgrade::FormatVersion(0x00020305), "2.3.5");
}

TEST_F(GovernanceTest, ProtocolUpgradeParseVersion) {
    auto v1 = ProtocolUpgrade::ParseVersion("1.0.0");
    EXPECT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, 0x00010000u);
    
    auto v2 = ProtocolUpgrade::ParseVersion("2.3.5");
    EXPECT_TRUE(v2.has_value());
    EXPECT_EQ(*v2, 0x00020305u);
    
    EXPECT_FALSE(ProtocolUpgrade::ParseVersion("invalid").has_value());
}

TEST_F(GovernanceTest, ProtocolUpgradeIsBackwardCompatible) {
    ProtocolUpgrade upgrade;
    upgrade.deprecatedFeatures = 0;
    EXPECT_TRUE(upgrade.IsBackwardCompatible());
    
    upgrade.deprecatedFeatures = 1;
    EXPECT_FALSE(upgrade.IsBackwardCompatible());
}

TEST_F(GovernanceTest, ProtocolUpgradeToString) {
    ProtocolUpgrade upgrade;
    upgrade.newVersion = 0x00010100;
    upgrade.minClientVersion = 0x00010000;
    upgrade.activationHeight = 10000;
    upgrade.deadlineHeight = 20000;
    
    std::string str = upgrade.ToString();
    EXPECT_NE(str.find("1.1.0"), std::string::npos);
    EXPECT_NE(str.find("10000"), std::string::npos);
}

// ============================================================================
// ConstitutionalChange Tests
// ============================================================================

TEST_F(GovernanceTest, ConstitutionalChangeGetHash) {
    ConstitutionalChange change1;
    change1.article = ConstitutionalArticle::GovernanceProcess;
    change1.currentText = "Old";
    change1.newText = "New";
    change1.rationale = "Reason";
    
    ConstitutionalChange change2 = change1;
    
    // Same content = same hash
    EXPECT_EQ(change1.GetHash().ToHex(), change2.GetHash().ToHex());
    
    // Different content = different hash
    change2.newText = "Different";
    EXPECT_NE(change1.GetHash().ToHex(), change2.GetHash().ToHex());
}

TEST_F(GovernanceTest, ConstitutionalChangeToString) {
    ConstitutionalChange change;
    change.article = ConstitutionalArticle::EconomicPolicy;
    change.rationale = "Economic improvement rationale";
    
    std::string str = change.ToString();
    EXPECT_NE(str.find("EconomicPolicy"), std::string::npos);
}

// ============================================================================
// Vote Tests
// ============================================================================

TEST_F(GovernanceTest, VoteGetHash) {
    Vote vote1;
    vote1.proposalId = CreateTestProposalId(1);
    vote1.voter = CreateTestVoterId(1);
    vote1.choice = VoteChoice::Yes;
    vote1.votingPower = 1000;
    vote1.voteHeight = 100;
    
    Vote vote2 = vote1;
    
    // Same content = same hash
    EXPECT_EQ(vote1.GetHash().ToHex(), vote2.GetHash().ToHex());
    
    // Different choice = different hash
    vote2.choice = VoteChoice::No;
    EXPECT_NE(vote1.GetHash().ToHex(), vote2.GetHash().ToHex());
}

TEST_F(GovernanceTest, VoteToString) {
    Vote vote;
    vote.proposalId = CreateTestProposalId(1);
    vote.voter = CreateTestVoterId(1);
    vote.choice = VoteChoice::Yes;
    vote.votingPower = 1000;
    
    std::string str = vote.ToString();
    EXPECT_NE(str.find("Vote"), std::string::npos);
    EXPECT_NE(str.find("Yes"), std::string::npos);
    EXPECT_NE(str.find("1000"), std::string::npos);
}

// ============================================================================
// Delegation Tests
// ============================================================================

TEST_F(GovernanceTest, DelegationGetHash) {
    Delegation d1;
    d1.delegator = CreateTestVoterId(1);
    d1.delegate = CreateTestVoterId(2);
    d1.creationHeight = 100;
    
    Delegation d2 = d1;
    
    EXPECT_EQ(d1.GetHash().ToHex(), d2.GetHash().ToHex());
    
    d2.delegate = CreateTestVoterId(3);
    EXPECT_NE(d1.GetHash().ToHex(), d2.GetHash().ToHex());
}

TEST_F(GovernanceTest, DelegationIsValidAt) {
    Delegation delegation;
    delegation.delegator = CreateTestVoterId(1);
    delegation.delegate = CreateTestVoterId(2);
    delegation.creationHeight = 100;
    delegation.expirationHeight = 200;
    delegation.isActive = true;
    
    // Before creation
    EXPECT_FALSE(delegation.IsValidAt(50));
    
    // During validity period
    EXPECT_TRUE(delegation.IsValidAt(150));
    
    // After expiration
    EXPECT_FALSE(delegation.IsValidAt(250));
    
    // Inactive
    delegation.isActive = false;
    EXPECT_FALSE(delegation.IsValidAt(150));
}

TEST_F(GovernanceTest, DelegationToString) {
    Delegation delegation;
    delegation.delegator = CreateTestVoterId(1);
    delegation.delegate = CreateTestVoterId(2);
    delegation.isActive = true;
    
    std::string str = delegation.ToString();
    EXPECT_NE(str.find("Delegation"), std::string::npos);
    EXPECT_NE(str.find("yes"), std::string::npos);
}

// ============================================================================
// GovernanceProposal Tests
// ============================================================================

TEST_F(GovernanceTest, GovernanceProposalCalculateHash) {
    GovernanceProposal p1 = CreateTestProposal(ProposalType::Signal, "Test");
    GovernanceProposal p2 = CreateTestProposal(ProposalType::Signal, "Test");
    
    // Same content = same hash
    EXPECT_EQ(p1.CalculateHash().ToHex(), p2.CalculateHash().ToHex());
    
    // Different title = different hash
    p2.title = "Different";
    EXPECT_NE(p1.CalculateHash().ToHex(), p2.CalculateHash().ToHex());
}

TEST_F(GovernanceTest, GovernanceProposalGetVotingPeriod) {
    GovernanceProposal paramProposal = CreateTestProposal(ProposalType::Parameter, "Param");
    GovernanceProposal protoProposal = CreateTestProposal(ProposalType::Protocol, "Proto");
    GovernanceProposal constProposal = CreateTestProposal(ProposalType::Constitutional, "Const");
    
    EXPECT_EQ(paramProposal.GetVotingPeriod(), PARAMETER_VOTING_PERIOD);
    EXPECT_EQ(protoProposal.GetVotingPeriod(), PROTOCOL_VOTING_PERIOD);
    EXPECT_EQ(constProposal.GetVotingPeriod(), CONSTITUTIONAL_VOTING_PERIOD);
    
    // Protocol > Parameter
    EXPECT_GT(protoProposal.GetVotingPeriod(), paramProposal.GetVotingPeriod());
    
    // Constitutional > Protocol
    EXPECT_GT(constProposal.GetVotingPeriod(), protoProposal.GetVotingPeriod());
}

TEST_F(GovernanceTest, GovernanceProposalGetExecutionDelay) {
    GovernanceProposal paramProposal = CreateTestProposal(ProposalType::Parameter, "Param");
    GovernanceProposal protoProposal = CreateTestProposal(ProposalType::Protocol, "Proto");
    GovernanceProposal signalProposal = CreateTestProposal(ProposalType::Signal, "Signal");
    
    EXPECT_EQ(paramProposal.GetExecutionDelay(), PARAMETER_EXECUTION_DELAY);
    EXPECT_EQ(protoProposal.GetExecutionDelay(), PROTOCOL_EXECUTION_DELAY);
    EXPECT_EQ(signalProposal.GetExecutionDelay(), 0); // No execution needed
}

TEST_F(GovernanceTest, GovernanceProposalGetApprovalThreshold) {
    GovernanceProposal paramProposal = CreateTestProposal(ProposalType::Parameter, "Param");
    GovernanceProposal protoProposal = CreateTestProposal(ProposalType::Protocol, "Proto");
    GovernanceProposal constProposal = CreateTestProposal(ProposalType::Constitutional, "Const");
    
    EXPECT_EQ(paramProposal.GetApprovalThreshold(), PARAMETER_APPROVAL_THRESHOLD);
    EXPECT_EQ(protoProposal.GetApprovalThreshold(), PROTOCOL_APPROVAL_THRESHOLD);
    EXPECT_EQ(constProposal.GetApprovalThreshold(), CONSTITUTIONAL_APPROVAL_THRESHOLD);
    
    // Constitutional requires highest approval
    EXPECT_GT(constProposal.GetApprovalThreshold(), protoProposal.GetApprovalThreshold());
    EXPECT_GT(protoProposal.GetApprovalThreshold(), paramProposal.GetApprovalThreshold());
}

TEST_F(GovernanceTest, GovernanceProposalGetQuorumRequirement) {
    GovernanceProposal paramProposal = CreateTestProposal(ProposalType::Parameter, "Param");
    GovernanceProposal protoProposal = CreateTestProposal(ProposalType::Protocol, "Proto");
    GovernanceProposal constProposal = CreateTestProposal(ProposalType::Constitutional, "Const");
    
    EXPECT_EQ(paramProposal.GetQuorumRequirement(), PARAMETER_QUORUM);
    EXPECT_EQ(protoProposal.GetQuorumRequirement(), PROTOCOL_QUORUM);
    EXPECT_EQ(constProposal.GetQuorumRequirement(), CONSTITUTIONAL_QUORUM);
}

TEST_F(GovernanceTest, GovernanceProposalApprovalPercent) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    
    // No votes
    EXPECT_EQ(proposal.GetApprovalPercent(), 0.0);
    
    // Add some votes
    proposal.votesYes = 70;
    proposal.votesNo = 30;
    
    EXPECT_NEAR(proposal.GetApprovalPercent(), 70.0, 0.1);
}

TEST_F(GovernanceTest, GovernanceProposalParticipationPercent) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    proposal.totalVotingPower = 1000;
    
    // No votes
    EXPECT_EQ(proposal.GetParticipationPercent(), 0.0);
    
    // 20% participation
    proposal.votesYes = 150;
    proposal.votesNo = 50;
    
    EXPECT_NEAR(proposal.GetParticipationPercent(), 20.0, 0.1);
}


TEST_F(GovernanceTest, GovernanceProposalHasQuorum) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Parameter, "Test");
    proposal.totalVotingPower = 1000;
    
    // Below quorum (10%)
    proposal.votesYes = 50;
    EXPECT_FALSE(proposal.HasQuorum());
    
    // At quorum
    proposal.votesYes = 100;
    EXPECT_TRUE(proposal.HasQuorum());
}

TEST_F(GovernanceTest, GovernanceProposalHasApproval) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Parameter, "Test");
    proposal.totalVotingPower = 1000;
    
    // Below approval threshold (50%)
    proposal.votesYes = 40;
    proposal.votesNo = 60;
    EXPECT_FALSE(proposal.HasApproval());
    
    // Above approval threshold
    proposal.votesYes = 60;
    proposal.votesNo = 40;
    EXPECT_TRUE(proposal.HasApproval());
}

TEST_F(GovernanceTest, GovernanceProposalIsVetoed) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    
    // Not vetoed
    proposal.votesYes = 50;
    proposal.votesNo = 30;
    proposal.votesNoWithVeto = 20;
    EXPECT_FALSE(proposal.IsVetoed());
    
    // Vetoed (>33% NoWithVeto)
    proposal.votesYes = 30;
    proposal.votesNo = 20;
    proposal.votesNoWithVeto = 50;
    EXPECT_TRUE(proposal.IsVetoed());
}

TEST_F(GovernanceTest, GovernanceProposalIsVotingActive) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    proposal.status = GovernanceStatus::Active;
    proposal.votingStartHeight = 100;
    proposal.votingEndHeight = 200;
    
    // Before start
    EXPECT_FALSE(proposal.IsVotingActive(50));
    
    // During voting
    EXPECT_TRUE(proposal.IsVotingActive(150));
    
    // After end
    EXPECT_FALSE(proposal.IsVotingActive(250));
    
    // Wrong status
    proposal.status = GovernanceStatus::Pending;
    EXPECT_FALSE(proposal.IsVotingActive(150));
}

TEST_F(GovernanceTest, GovernanceProposalIsReadyForExecution) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Parameter, "Test");
    proposal.status = GovernanceStatus::Approved;
    proposal.executionHeight = 1000;
    
    // Before execution height
    EXPECT_FALSE(proposal.IsReadyForExecution(500));
    
    // At execution height
    EXPECT_TRUE(proposal.IsReadyForExecution(1000));
    
    // After execution height
    EXPECT_TRUE(proposal.IsReadyForExecution(1500));
    
    // Wrong status
    proposal.status = GovernanceStatus::Active;
    EXPECT_FALSE(proposal.IsReadyForExecution(1500));
}

TEST_F(GovernanceTest, GovernanceProposalGetTotalVotes) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    proposal.votesYes = 100;
    proposal.votesNo = 50;
    proposal.votesAbstain = 30;
    proposal.votesNoWithVeto = 20;
    
    EXPECT_EQ(proposal.GetTotalVotes(), 200);
}

TEST_F(GovernanceTest, GovernanceProposalToString) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Parameter, "Fee Increase");
    proposal.status = GovernanceStatus::Active;
    
    std::string str = proposal.ToString();
    EXPECT_NE(str.find("GovernanceProposal"), std::string::npos);
    EXPECT_NE(str.find("Parameter"), std::string::npos);
    EXPECT_NE(str.find("Active"), std::string::npos);
}

// ============================================================================
// VotingPowerTracker Tests
// ============================================================================

TEST_F(GovernanceTest, VotingPowerTrackerConstruction) {
    VotingPowerTracker tracker;
    EXPECT_EQ(tracker.GetTotalVotingPower(), 0);
    EXPECT_EQ(tracker.GetVoterCount(), 0);
}

TEST_F(GovernanceTest, VotingPowerTrackerUpdateVotingPower) {
    VotingPowerTracker tracker;
    VoterId voter = CreateTestVoterId(1);
    
    tracker.UpdateVotingPower(voter, 1000);
    EXPECT_EQ(tracker.GetVotingPower(voter), 1000);
    EXPECT_EQ(tracker.GetTotalVotingPower(), 1000);
    EXPECT_EQ(tracker.GetVoterCount(), 1);
    
    // Update existing
    tracker.UpdateVotingPower(voter, 2000);
    EXPECT_EQ(tracker.GetVotingPower(voter), 2000);
    EXPECT_EQ(tracker.GetTotalVotingPower(), 2000);
    EXPECT_EQ(tracker.GetVoterCount(), 1);
    
    // Add another
    VoterId voter2 = CreateTestVoterId(2);
    tracker.UpdateVotingPower(voter2, 500);
    EXPECT_EQ(tracker.GetTotalVotingPower(), 2500);
    EXPECT_EQ(tracker.GetVoterCount(), 2);
}

TEST_F(GovernanceTest, VotingPowerTrackerRemoveVoter) {
    VotingPowerTracker tracker;
    VoterId voter = CreateTestVoterId(1);
    
    tracker.UpdateVotingPower(voter, 1000);
    tracker.RemoveVoter(voter);
    
    EXPECT_EQ(tracker.GetVotingPower(voter), 0);
    EXPECT_EQ(tracker.GetTotalVotingPower(), 0);
    EXPECT_EQ(tracker.GetVoterCount(), 0);
}

TEST_F(GovernanceTest, VotingPowerTrackerTakeSnapshot) {
    VotingPowerTracker tracker;
    tracker.UpdateVotingPower(CreateTestVoterId(1), 1000);
    tracker.UpdateVotingPower(CreateTestVoterId(2), 2000);
    
    auto snapshot = tracker.TakeSnapshot();
    EXPECT_EQ(snapshot.size(), 2);
}

TEST_F(GovernanceTest, VotingPowerTrackerClear) {
    VotingPowerTracker tracker;
    tracker.UpdateVotingPower(CreateTestVoterId(1), 1000);
    tracker.UpdateVotingPower(CreateTestVoterId(2), 2000);
    
    tracker.Clear();
    EXPECT_EQ(tracker.GetTotalVotingPower(), 0);
    EXPECT_EQ(tracker.GetVoterCount(), 0);
}

// ============================================================================
// DelegationRegistry Tests
// ============================================================================

TEST_F(GovernanceTest, DelegationRegistryConstruction) {
    DelegationRegistry registry;
    EXPECT_EQ(registry.GetActiveDelegationCount(), 0);
}

TEST_F(GovernanceTest, DelegationRegistryAddDelegation) {
    DelegationRegistry registry;
    
    Delegation delegation;
    delegation.delegator = CreateTestVoterId(1);
    delegation.delegate = CreateTestVoterId(2);
    delegation.creationHeight = 100;
    delegation.isActive = true;
    
    EXPECT_TRUE(registry.AddDelegation(delegation));
    EXPECT_EQ(registry.GetActiveDelegationCount(), 1);
    
    auto retrieved = registry.GetDelegation(delegation.delegator);
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->delegate, delegation.delegate);
}

TEST_F(GovernanceTest, DelegationRegistryRemoveDelegation) {
    DelegationRegistry registry;
    
    Delegation delegation;
    delegation.delegator = CreateTestVoterId(1);
    delegation.delegate = CreateTestVoterId(2);
    delegation.isActive = true;
    
    registry.AddDelegation(delegation);
    EXPECT_TRUE(registry.RemoveDelegation(delegation.delegator));
    EXPECT_EQ(registry.GetActiveDelegationCount(), 0);
    
    // Remove non-existent
    EXPECT_FALSE(registry.RemoveDelegation(CreateTestVoterId(99)));
}

TEST_F(GovernanceTest, DelegationRegistryGetDelegators) {
    DelegationRegistry registry;
    
    VoterId delegate = CreateTestVoterId(10);
    
    // Add multiple delegators to same delegate
    for (int i = 1; i <= 3; ++i) {
        Delegation delegation;
        delegation.delegator = CreateTestVoterId(i);
        delegation.delegate = delegate;
        delegation.isActive = true;
        registry.AddDelegation(delegation);
    }
    
    auto delegators = registry.GetDelegators(delegate);
    EXPECT_EQ(delegators.size(), 3);
}

TEST_F(GovernanceTest, DelegationRegistryCycleDetection) {
    DelegationRegistry registry;
    
    // A -> B
    Delegation d1;
    d1.delegator = CreateTestVoterId(1);
    d1.delegate = CreateTestVoterId(2);
    d1.isActive = true;
    EXPECT_TRUE(registry.AddDelegation(d1));
    
    // B -> C
    Delegation d2;
    d2.delegator = CreateTestVoterId(2);
    d2.delegate = CreateTestVoterId(3);
    d2.isActive = true;
    EXPECT_TRUE(registry.AddDelegation(d2));
    
    // C -> A would create cycle
    Delegation d3;
    d3.delegator = CreateTestVoterId(3);
    d3.delegate = CreateTestVoterId(1);
    d3.isActive = true;
    EXPECT_FALSE(registry.AddDelegation(d3)); // Should fail due to cycle
}

TEST_F(GovernanceTest, DelegationRegistryMaxDepth) {
    DelegationRegistry registry;
    
    // Create chain at max depth
    for (int i = 0; i < MAX_DELEGATION_DEPTH; ++i) {
        Delegation d;
        d.delegator = CreateTestVoterId(i);
        d.delegate = CreateTestVoterId(i + 1);
        d.isActive = true;
        
        bool result = registry.AddDelegation(d);
        if (i < MAX_DELEGATION_DEPTH - 1) {
            EXPECT_TRUE(result);
        }
    }
}

TEST_F(GovernanceTest, DelegationRegistryExpireDelegations) {
    DelegationRegistry registry;
    
    Delegation delegation;
    delegation.delegator = CreateTestVoterId(1);
    delegation.delegate = CreateTestVoterId(2);
    delegation.creationHeight = 100;
    delegation.expirationHeight = 200;
    delegation.isActive = true;
    
    registry.AddDelegation(delegation);
    EXPECT_EQ(registry.GetActiveDelegationCount(), 1);
    
    // Expire at height 200
    registry.ExpireDelegations(200);
    EXPECT_EQ(registry.GetActiveDelegationCount(), 0);
}


// ============================================================================
// ParameterRegistry Tests
// ============================================================================

TEST_F(GovernanceTest, ParameterRegistryConstruction) {
    ParameterRegistry registry;
    
    // Should have defaults
    auto feeMultiplier = registry.GetParameterInt(GovernableParameter::TransactionFeeMultiplier);
    EXPECT_EQ(feeMultiplier, 100);
}

TEST_F(GovernanceTest, ParameterRegistrySetParameter) {
    ParameterRegistry registry;
    
    EXPECT_TRUE(registry.SetParameter(GovernableParameter::TransactionFeeMultiplier, int64_t(150)));
    EXPECT_EQ(registry.GetParameterInt(GovernableParameter::TransactionFeeMultiplier), 150);
    
    // Invalid value (out of bounds)
    EXPECT_FALSE(registry.SetParameter(GovernableParameter::TransactionFeeMultiplier, int64_t(1)));
}

TEST_F(GovernanceTest, ParameterRegistryApplyChanges) {
    ParameterRegistry registry;
    
    std::vector<ParameterChange> changes;
    ParameterChange change;
    change.parameter = GovernableParameter::TransactionFeeMultiplier;
    change.currentValue = int64_t(100);
    change.newValue = int64_t(120);
    changes.push_back(change);
    
    EXPECT_TRUE(registry.ApplyChanges(changes));
    EXPECT_EQ(registry.GetParameterInt(GovernableParameter::TransactionFeeMultiplier), 120);
}

TEST_F(GovernanceTest, ParameterRegistryGetAllParameters) {
    ParameterRegistry registry;
    
    auto allParams = registry.GetAllParameters();
    EXPECT_GT(allParams.size(), 0);
}

// ============================================================================
// GuardianRegistry Tests
// ============================================================================

TEST_F(GovernanceTest, GuardianRegistryConstruction) {
    GuardianRegistry registry;
    EXPECT_EQ(registry.GetActiveGuardianCount(), 0);
}

TEST_F(GovernanceTest, GuardianRegistryAddGuardian) {
    GuardianRegistry registry;
    
    Guardian guardian;
    guardian.id = CreateTestVoterId(1);
    guardian.publicKey = testPublicKey_;
    guardian.appointmentHeight = 100;
    guardian.isActive = true;
    
    EXPECT_TRUE(registry.AddGuardian(guardian));
    EXPECT_EQ(registry.GetActiveGuardianCount(), 1);
    
    auto retrieved = registry.GetGuardian(guardian.id);
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->appointmentHeight, 100);
    
    // Duplicate should fail
    EXPECT_FALSE(registry.AddGuardian(guardian));
}

TEST_F(GovernanceTest, GuardianRegistryRemoveGuardian) {
    GuardianRegistry registry;
    
    Guardian guardian;
    guardian.id = CreateTestVoterId(1);
    guardian.isActive = true;
    
    registry.AddGuardian(guardian);
    EXPECT_TRUE(registry.RemoveGuardian(guardian.id));
    EXPECT_EQ(registry.GetActiveGuardianCount(), 0);
}

TEST_F(GovernanceTest, GuardianRegistryRecordVeto) {
    GuardianRegistry registry;
    
    Guardian guardian;
    guardian.id = CreateTestVoterId(1);
    guardian.isActive = true;
    guardian.vetosUsed = 0;
    
    registry.AddGuardian(guardian);
    
    GovernanceProposalId proposalId = CreateTestProposalId(1);
    EXPECT_TRUE(registry.RecordVeto(guardian.id, proposalId));
    EXPECT_EQ(registry.GetVetoCount(proposalId), 1);
    
    // Non-existent guardian
    EXPECT_FALSE(registry.RecordVeto(CreateTestVoterId(99), proposalId));
}

TEST_F(GovernanceTest, GuardianRegistryVetoLimit) {
    GuardianRegistry registry;
    
    Guardian guardian;
    guardian.id = CreateTestVoterId(1);
    guardian.isActive = true;
    guardian.vetosUsed = Guardian::MAX_VETOS_PER_PERIOD;
    
    registry.AddGuardian(guardian);
    
    // Should fail - veto limit reached
    EXPECT_FALSE(registry.RecordVeto(guardian.id, CreateTestProposalId(1)));
}

TEST_F(GovernanceTest, GuardianRegistryResetVetoCounts) {
    GuardianRegistry registry;
    
    Guardian guardian;
    guardian.id = CreateTestVoterId(1);
    guardian.isActive = true;
    
    registry.AddGuardian(guardian);
    registry.RecordVeto(guardian.id, CreateTestProposalId(1));
    
    registry.ResetVetoCounts();
    
    // Should be able to veto again
    EXPECT_TRUE(registry.RecordVeto(guardian.id, CreateTestProposalId(2)));
}

// ============================================================================
// GovernanceEngine Tests
// ============================================================================

TEST_F(GovernanceTest, GovernanceEngineConstruction) {
    EXPECT_EQ(engine_->GetActiveProposalCount(), 0);
    EXPECT_EQ(engine_->GetTotalProposalCount(), 0);
    EXPECT_EQ(engine_->GetCurrentHeight(), 0);
}

TEST_F(GovernanceTest, GovernanceEngineSubmitProposal) {
    
    
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test Signal");
    auto signature = SignProposal(proposal);
    
    auto proposalId = engine_->SubmitProposal(proposal, signature);
    EXPECT_TRUE(proposalId.has_value());
    EXPECT_EQ(engine_->GetTotalProposalCount(), 1);
    
    auto retrieved = engine_->GetProposal(*proposalId);
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->title, "Test Signal");
    EXPECT_EQ(retrieved->status, GovernanceStatus::Pending);
}

TEST_F(GovernanceTest, GovernanceEngineSubmitProposalInvalidSignature) {
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    
    // Empty signature
    std::vector<Byte> emptySignature;
    auto result = engine_->SubmitProposal(proposal, emptySignature);
    EXPECT_FALSE(result.has_value());
}

TEST_F(GovernanceTest, GovernanceEngineSubmitProposalMaxLimit) {
    
    
    // Submit max proposals
    for (int i = 0; i < MAX_ACTIVE_PROPOSALS_PER_USER; ++i) {
        GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test " + std::to_string(i));
        proposal.submitHeight = i; // Make unique
        auto signature = SignProposal(proposal);
        auto result = engine_->SubmitProposal(proposal, signature);
        EXPECT_TRUE(result.has_value());
    }
    
    // Next should fail
    GovernanceProposal extraProposal = CreateTestProposal(ProposalType::Signal, "Extra");
    extraProposal.submitHeight = 100;
    auto signature = SignProposal(extraProposal);
    auto result = engine_->SubmitProposal(extraProposal, signature);
    EXPECT_FALSE(result.has_value());
}

TEST_F(GovernanceTest, GovernanceEngineGetProposalsByStatus) {
    
    
    // Submit a proposal
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    auto signature = SignProposal(proposal);
    engine_->SubmitProposal(proposal, signature);
    
    auto pendingProposals = engine_->GetProposalsByStatus(GovernanceStatus::Pending);
    EXPECT_EQ(pendingProposals.size(), 1);
    
    auto activeProposals = engine_->GetProposalsByStatus(GovernanceStatus::Active);
    EXPECT_EQ(activeProposals.size(), 0);
}

TEST_F(GovernanceTest, GovernanceEngineGetProposalsByProposer) {
    
    
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    auto signature = SignProposal(proposal);
    engine_->SubmitProposal(proposal, signature);
    
    auto proposals = engine_->GetProposalsByProposer(testPublicKey_);
    EXPECT_EQ(proposals.size(), 1);
}

TEST_F(GovernanceTest, GovernanceEngineCastVote) {
    
    
    // Submit and activate proposal
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    auto signature = SignProposal(proposal);
    auto proposalId = engine_->SubmitProposal(proposal, signature);
    ASSERT_TRUE(proposalId.has_value());
    
    // Process blocks to start voting
    engine_->ProcessBlock(1);
    engine_->ProcessBlock(2);
    
    // Verify proposal is now active
    auto activeProposal = engine_->GetProposal(*proposalId);
    ASSERT_TRUE(activeProposal.has_value());
    EXPECT_EQ(activeProposal->status, GovernanceStatus::Active);
    
    // Cast vote
    VoterId voter = CreateTestVoterId(1);
    Vote vote = CreateTestVote(*proposalId, voter, VoteChoice::Yes, 1000);
    vote.voteHeight = engine_->GetCurrentHeight();
    
    EXPECT_TRUE(engine_->CastVote(vote));
    EXPECT_TRUE(engine_->HasVoted(*proposalId, voter));
    
    auto recordedVote = engine_->GetVote(*proposalId, voter);
    EXPECT_TRUE(recordedVote.has_value());
    EXPECT_EQ(recordedVote->choice, VoteChoice::Yes);
}

TEST_F(GovernanceTest, GovernanceEngineCastVoteDuplicate) {
    
    
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    auto signature = SignProposal(proposal);
    auto proposalId = engine_->SubmitProposal(proposal, signature);
    
    engine_->ProcessBlock(1);
    engine_->ProcessBlock(2);
    
    VoterId voter = CreateTestVoterId(1);
    Vote vote = CreateTestVote(*proposalId, voter, VoteChoice::Yes, 1000);
    vote.voteHeight = engine_->GetCurrentHeight();
    
    EXPECT_TRUE(engine_->CastVote(vote));
    
    // Duplicate vote should fail
    EXPECT_FALSE(engine_->CastVote(vote));
}

TEST_F(GovernanceTest, GovernanceEngineGetVotes) {
    
    
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    auto signature = SignProposal(proposal);
    auto proposalId = engine_->SubmitProposal(proposal, signature);
    
    engine_->ProcessBlock(1);
    engine_->ProcessBlock(2);
    
    // Cast multiple votes
    for (int i = 0; i < 5; ++i) {
        VoterId voter = CreateTestVoterId(i);
        Vote vote = CreateTestVote(*proposalId, voter, VoteChoice::Yes, 1000 + i * 100);
        vote.voteHeight = engine_->GetCurrentHeight();
        engine_->CastVote(vote);
    }
    
    auto votes = engine_->GetVotes(*proposalId);
    EXPECT_EQ(votes.size(), 5);
}

TEST_F(GovernanceTest, GovernanceEngineUpdateVotingPower) {
    VoterId voter = CreateTestVoterId(99);
    
    engine_->UpdateVotingPower(voter, 5000);
    EXPECT_EQ(engine_->GetVotingPower(voter), 5000);
    
    engine_->UpdateVotingPower(voter, 0);
    EXPECT_EQ(engine_->GetVotingPower(voter), 0);
}

TEST_F(GovernanceTest, GovernanceEngineDelegate) {
    VoterId delegator = CreateTestVoterId(1);
    VoterId delegate = CreateTestVoterId(2);
    
    Delegation delegation;
    delegation.delegator = delegator;
    delegation.delegate = delegate;
    delegation.creationHeight = 0;
    delegation.isActive = true;
    
    std::vector<Byte> sig(64, 0x01);
    EXPECT_TRUE(engine_->Delegate(delegation, sig));
    
    auto retrieved = engine_->GetDelegations().GetDelegation(delegator);
    EXPECT_TRUE(retrieved.has_value());
}

TEST_F(GovernanceTest, GovernanceEngineRevokeDelegation) {
    VoterId delegator = CreateTestVoterId(1);
    VoterId delegate = CreateTestVoterId(2);
    
    Delegation delegation;
    delegation.delegator = delegator;
    delegation.delegate = delegate;
    delegation.isActive = true;
    
    std::vector<Byte> sig(64, 0x01);
    engine_->Delegate(delegation, sig);
    
    EXPECT_TRUE(engine_->RevokeDelegation(delegator, sig));
    
    auto retrieved = engine_->GetDelegations().GetDelegation(delegator);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(GovernanceTest, GovernanceEngineGetParameter) {
    auto value = engine_->GetParameter(GovernableParameter::TransactionFeeMultiplier);
    EXPECT_TRUE(std::holds_alternative<int64_t>(value));
    EXPECT_EQ(std::get<int64_t>(value), 100);
}

TEST_F(GovernanceTest, GovernanceEngineProcessBlock) {
    
    
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    auto signature = SignProposal(proposal);
    auto proposalId = engine_->SubmitProposal(proposal, signature);
    
    // Initially pending
    auto p1 = engine_->GetProposal(*proposalId);
    EXPECT_EQ(p1->status, GovernanceStatus::Pending);
    
    // Process block to start voting
    engine_->ProcessBlock(2);
    
    auto p2 = engine_->GetProposal(*proposalId);
    EXPECT_EQ(p2->status, GovernanceStatus::Active);
}

TEST_F(GovernanceTest, GovernanceEngineVotingEndsAndRejects) {
    
    
    GovernanceProposal proposal = CreateTestProposal(ProposalType::Signal, "Test");
    auto signature = SignProposal(proposal);
    auto proposalId = engine_->SubmitProposal(proposal, signature);
    
    // Start voting
    engine_->ProcessBlock(2);
    
    // No votes cast - process blocks until voting ends
    int votingEnd = PARAMETER_VOTING_PERIOD + 10;
    for (int i = 3; i <= votingEnd; ++i) {
        engine_->ProcessBlock(i);
    }
    
    auto finalProposal = engine_->GetProposal(*proposalId);
    // Should be QuorumFailed since no votes cast
    EXPECT_EQ(finalProposal->status, GovernanceStatus::QuorumFailed);
}

TEST_F(GovernanceTest, GovernanceEngineSerializationStub) {
    auto serialized = engine_->Serialize();
    // Stub returns empty vector
    EXPECT_TRUE(serialized.empty());
}

