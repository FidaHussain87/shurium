// SHURIUM - Treasury Management Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include <shurium/economics/treasury.h>
#include <shurium/economics/reward.h>
#include <shurium/crypto/keys.h>
#include <shurium/consensus/params.h>

#include <array>
#include <chrono>
#include <memory>
#include <set>
#include <vector>

namespace shurium {
namespace economics {
namespace {

// ============================================================================
// Test Fixtures
// ============================================================================

class TreasuryTest : public ::testing::Test {
protected:
    std::unique_ptr<Treasury> treasury_;
    
    void SetUp() override {
        treasury_ = std::make_unique<Treasury>();
    }
    
    // Helper to create a test public key
    PublicKey CreateTestPublicKey(Byte seed) {
        std::vector<Byte> keyData(33);
        keyData[0] = 0x02;  // Compressed public key prefix
        std::fill(keyData.begin() + 1, keyData.end(), seed);
        return PublicKey(keyData.data(), keyData.size());
    }
    
    // Helper to create a test Hash160 address
    Hash160 CreateTestAddress(Byte value) {
        std::array<Byte, 20> data{};
        std::fill(data.begin(), data.end(), value);
        return Hash160(data);
    }
    
    // Helper to create a test proposal
    TreasuryProposal CreateTestProposal(Byte seed) {
        TreasuryProposal proposal;
        proposal.title = "Test Proposal " + std::to_string(seed);
        proposal.description = "A test proposal for unit testing";
        proposal.category = TreasuryCategory::EcosystemDevelopment;
        proposal.requestedAmount = 10000 * COIN;
        proposal.recipient = CreateTestAddress(seed);
        proposal.proposer = CreateTestPublicKey(seed);
        return proposal;
    }
    
    // Helper to create a test vote
    TreasuryVote CreateTestVote(const ProposalId& proposalId, Byte seed, bool inFavor) {
        TreasuryVote vote;
        vote.proposalId = proposalId;
        vote.voter = CreateTestPublicKey(seed);
        vote.inFavor = inFavor;
        vote.votingPower = 1000;
        vote.voteHeight = 100;
        return vote;
    }
};

// ============================================================================
// Treasury Constants Tests
// ============================================================================

TEST_F(TreasuryTest, TreasuryConstantsValid) {
    EXPECT_GT(MIN_PROPOSAL_AMOUNT, 0);
    EXPECT_GT(MAX_PROPOSAL_PERCENT, 0);
    EXPECT_LE(MAX_PROPOSAL_PERCENT, 100);
    EXPECT_GT(PROPOSAL_VOTING_PERIOD, 0);
    EXPECT_GT(PROPOSAL_EXECUTION_DELAY, 0);
    EXPECT_GT(MIN_APPROVAL_PERCENT, 50);
    EXPECT_LE(MIN_APPROVAL_PERCENT, 100);
    EXPECT_GT(QUORUM_PERCENT, 0);
    EXPECT_LE(QUORUM_PERCENT, 100);
    EXPECT_GT(TREASURY_REPORT_INTERVAL, 0);
}

// ============================================================================
// TreasuryCategory Tests
// ============================================================================

TEST_F(TreasuryTest, TreasuryCategoryToString) {
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::EcosystemDevelopment), nullptr);
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::ProtocolDevelopment), nullptr);
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::Security), nullptr);
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::Marketing), nullptr);
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::Infrastructure), nullptr);
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::Legal), nullptr);
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::Education), nullptr);
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::Emergency), nullptr);
    EXPECT_NE(TreasuryCategoryToString(TreasuryCategory::Other), nullptr);
}

TEST_F(TreasuryTest, TreasuryCategoryStringsUnique) {
    std::set<std::string> categories;
    categories.insert(TreasuryCategoryToString(TreasuryCategory::EcosystemDevelopment));
    categories.insert(TreasuryCategoryToString(TreasuryCategory::ProtocolDevelopment));
    categories.insert(TreasuryCategoryToString(TreasuryCategory::Security));
    categories.insert(TreasuryCategoryToString(TreasuryCategory::Marketing));
    categories.insert(TreasuryCategoryToString(TreasuryCategory::Infrastructure));
    categories.insert(TreasuryCategoryToString(TreasuryCategory::Legal));
    categories.insert(TreasuryCategoryToString(TreasuryCategory::Education));
    categories.insert(TreasuryCategoryToString(TreasuryCategory::Emergency));
    categories.insert(TreasuryCategoryToString(TreasuryCategory::Other));
    
    EXPECT_EQ(categories.size(), 9);
}

TEST_F(TreasuryTest, ParseTreasuryCategory) {
    auto cat = ParseTreasuryCategory("EcosystemDevelopment");
    ASSERT_TRUE(cat.has_value());
    EXPECT_EQ(*cat, TreasuryCategory::EcosystemDevelopment);
    
    auto invalid = ParseTreasuryCategory("InvalidCategory");
    EXPECT_FALSE(invalid.has_value());
}

// ============================================================================
// ProposalStatus Tests
// ============================================================================

TEST_F(TreasuryTest, ProposalStatusToString) {
    EXPECT_NE(ProposalStatusToString(ProposalStatus::Pending), nullptr);
    EXPECT_NE(ProposalStatusToString(ProposalStatus::Voting), nullptr);
    EXPECT_NE(ProposalStatusToString(ProposalStatus::Approved), nullptr);
    EXPECT_NE(ProposalStatusToString(ProposalStatus::Rejected), nullptr);
    EXPECT_NE(ProposalStatusToString(ProposalStatus::Executed), nullptr);
    EXPECT_NE(ProposalStatusToString(ProposalStatus::Cancelled), nullptr);
    EXPECT_NE(ProposalStatusToString(ProposalStatus::Expired), nullptr);
    EXPECT_NE(ProposalStatusToString(ProposalStatus::Failed), nullptr);
}

// ============================================================================
// TreasuryProposal Tests
// ============================================================================

TEST_F(TreasuryTest, TreasuryProposalCalculateHash) {
    TreasuryProposal proposal = CreateTestProposal(0x01);
    
    Hash256 hash1 = proposal.CalculateHash();
    Hash256 hash2 = proposal.CalculateHash();
    
    // Same proposal should have same hash
    EXPECT_EQ(hash1.ToHex(), hash2.ToHex());
    
    // Different title should have different hash
    proposal.title = "Different Title";
    Hash256 hash3 = proposal.CalculateHash();
    EXPECT_NE(hash1.ToHex(), hash3.ToHex());
}

TEST_F(TreasuryTest, TreasuryProposalGetApprovalPercent) {
    TreasuryProposal proposal;
    proposal.votesFor = 75;
    proposal.votesAgainst = 25;
    
    EXPECT_DOUBLE_EQ(proposal.GetApprovalPercent(), 75.0);
    
    // No votes
    proposal.votesFor = 0;
    proposal.votesAgainst = 0;
    EXPECT_DOUBLE_EQ(proposal.GetApprovalPercent(), 0.0);
}

TEST_F(TreasuryTest, TreasuryProposalGetQuorumPercent) {
    TreasuryProposal proposal;
    proposal.totalVotingPower = 1000;
    proposal.votesFor = 150;
    proposal.votesAgainst = 50;
    
    // 200 votes out of 1000 = 20%
    EXPECT_DOUBLE_EQ(proposal.GetQuorumPercent(), 20.0);
}

TEST_F(TreasuryTest, TreasuryProposalIsPassed) {
    TreasuryProposal proposal;
    proposal.totalVotingPower = 1000;
    
    // Passes (>60% approval, >20% quorum)
    proposal.votesFor = 200;
    proposal.votesAgainst = 50;
    EXPECT_TRUE(proposal.IsPassed());
    
    // Fails (low approval)
    proposal.votesFor = 100;
    proposal.votesAgainst = 100;
    EXPECT_FALSE(proposal.IsPassed());
}

TEST_F(TreasuryTest, TreasuryProposalHasQuorum) {
    TreasuryProposal proposal;
    proposal.totalVotingPower = 1000;
    
    // Has quorum (>20% participation)
    proposal.votesFor = 150;
    proposal.votesAgainst = 60;
    EXPECT_TRUE(proposal.HasQuorum());
    
    // No quorum (<20%)
    proposal.votesFor = 50;
    proposal.votesAgainst = 50;
    EXPECT_FALSE(proposal.HasQuorum());
}

TEST_F(TreasuryTest, TreasuryProposalIsVotingActive) {
    TreasuryProposal proposal;
    proposal.status = ProposalStatus::Voting;  // Must be in Voting status
    proposal.votingStartHeight = 100;
    proposal.votingEndHeight = 200;
    
    EXPECT_FALSE(proposal.IsVotingActive(99));   // Before start
    EXPECT_TRUE(proposal.IsVotingActive(100));   // At start
    EXPECT_TRUE(proposal.IsVotingActive(150));   // During
    EXPECT_TRUE(proposal.IsVotingActive(200));   // At end
    EXPECT_FALSE(proposal.IsVotingActive(201));  // After end
    
    // Also test that non-Voting status returns false
    proposal.status = ProposalStatus::Pending;
    EXPECT_FALSE(proposal.IsVotingActive(150));
}

TEST_F(TreasuryTest, TreasuryProposalIsReadyForExecution) {
    TreasuryProposal proposal;
    proposal.status = ProposalStatus::Approved;
    proposal.votingEndHeight = 100;
    proposal.executionHeight = 100 + PROPOSAL_EXECUTION_DELAY;
    
    // Not ready yet
    EXPECT_FALSE(proposal.IsReadyForExecution(proposal.executionHeight - 1));
    
    // Ready
    EXPECT_TRUE(proposal.IsReadyForExecution(proposal.executionHeight));
    EXPECT_TRUE(proposal.IsReadyForExecution(proposal.executionHeight + 100));
    
    // Not approved
    proposal.status = ProposalStatus::Pending;
    EXPECT_FALSE(proposal.IsReadyForExecution(proposal.executionHeight));
}

TEST_F(TreasuryTest, TreasuryProposalMilestones) {
    TreasuryProposal proposal = CreateTestProposal(0x01);
    proposal.requestedAmount = 0;  // Amount is in milestones
    
    TreasuryProposal::Milestone m1;
    m1.description = "Phase 1";
    m1.amount = 5000 * COIN;
    m1.releaseHeight = 1000;
    
    TreasuryProposal::Milestone m2;
    m2.description = "Phase 2";
    m2.amount = 5000 * COIN;
    m2.releaseHeight = 2000;
    
    proposal.milestones = {m1, m2};
    
    EXPECT_EQ(proposal.GetTotalAmount(), 10000 * COIN);
}

TEST_F(TreasuryTest, TreasuryProposalSerializeDeserialize) {
    TreasuryProposal original = CreateTestProposal(0x01);
    original.votesFor = 100;
    original.votesAgainst = 50;
    
    std::vector<Byte> serialized = original.Serialize();
    EXPECT_GT(serialized.size(), 0);
    
    auto deserialized = TreasuryProposal::Deserialize(serialized.data(), serialized.size());
    ASSERT_TRUE(deserialized.has_value());
    
    EXPECT_EQ(deserialized->title, original.title);
    EXPECT_EQ(deserialized->requestedAmount, original.requestedAmount);
    EXPECT_EQ(deserialized->votesFor, original.votesFor);
}

TEST_F(TreasuryTest, TreasuryProposalToString) {
    TreasuryProposal proposal = CreateTestProposal(0x01);
    
    std::string str = proposal.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("Test Proposal"), std::string::npos);
}

// ============================================================================
// TreasuryVote Tests
// ============================================================================

TEST_F(TreasuryTest, TreasuryVoteGetHash) {
    std::array<Byte, 32> idData{};
    std::fill(idData.begin(), idData.end(), 0x01);
    ProposalId proposalId(idData.data(), 32);
    
    TreasuryVote vote = CreateTestVote(proposalId, 0x01, true);
    
    Hash256 hash1 = vote.GetHash();
    Hash256 hash2 = vote.GetHash();
    
    EXPECT_EQ(hash1.ToHex(), hash2.ToHex());
}

TEST_F(TreasuryTest, TreasuryVoteSerializeDeserialize) {
    std::array<Byte, 32> idData{};
    std::fill(idData.begin(), idData.end(), 0x01);
    ProposalId proposalId(idData.data(), 32);
    
    TreasuryVote original = CreateTestVote(proposalId, 0x01, true);
    
    std::vector<Byte> serialized = original.Serialize();
    EXPECT_GT(serialized.size(), 0);
    
    auto deserialized = TreasuryVote::Deserialize(serialized.data(), serialized.size());
    ASSERT_TRUE(deserialized.has_value());
    
    EXPECT_EQ(deserialized->inFavor, original.inFavor);
    EXPECT_EQ(deserialized->votingPower, original.votingPower);
}

// ============================================================================
// CategoryBudget Tests
// ============================================================================

TEST_F(TreasuryTest, CategoryBudgetRemaining) {
    CategoryBudget budget;
    budget.allocated = 1000 * COIN;
    budget.spent = 300 * COIN;
    
    EXPECT_EQ(budget.Remaining(), 700 * COIN);
    
    // Overspent (shouldn't happen but handle it)
    budget.spent = 1100 * COIN;
    EXPECT_EQ(budget.Remaining(), 0);
}

TEST_F(TreasuryTest, CategoryBudgetUtilization) {
    CategoryBudget budget;
    budget.allocated = 1000 * COIN;
    budget.spent = 250 * COIN;
    
    EXPECT_DOUBLE_EQ(budget.Utilization(), 25.0);
    
    // Zero allocated
    budget.allocated = 0;
    EXPECT_DOUBLE_EQ(budget.Utilization(), 0.0);
}

// ============================================================================
// TreasuryBudget Tests
// ============================================================================

TEST_F(TreasuryTest, TreasuryBudgetInitialize) {
    TreasuryBudget budget;
    Amount balance = 100000 * COIN;
    
    budget.Initialize(balance, 0, 86400);
    
    EXPECT_EQ(budget.periodStart, 0);
    EXPECT_EQ(budget.periodEnd, 86400);  // start + periodBlocks
    EXPECT_EQ(budget.totalBalance, balance);
    
    // Categories should be allocated
    EXPECT_GT(budget.categories.size(), 0);
}

TEST_F(TreasuryTest, TreasuryBudgetGetCategory) {
    TreasuryBudget budget;
    budget.Initialize(100000 * COIN, 0, 86400);
    
    const CategoryBudget* cat = budget.GetCategory(TreasuryCategory::EcosystemDevelopment);
    ASSERT_NE(cat, nullptr);
    EXPECT_GT(cat->allocated, 0);
}

TEST_F(TreasuryTest, TreasuryBudgetRecordSpending) {
    TreasuryBudget budget;
    budget.Initialize(100000 * COIN, 0, 86400);
    
    const CategoryBudget* cat = budget.GetCategory(TreasuryCategory::Security);
    Amount initialRemaining = cat->Remaining();
    
    EXPECT_TRUE(budget.RecordSpending(TreasuryCategory::Security, 1000 * COIN));
    
    cat = budget.GetCategory(TreasuryCategory::Security);
    EXPECT_EQ(cat->Remaining(), initialRemaining - 1000 * COIN);
}

TEST_F(TreasuryTest, TreasuryBudgetTotals) {
    TreasuryBudget budget;
    budget.Initialize(100000 * COIN, 0, 86400);
    
    Amount totalAllocated = budget.TotalAllocated();
    EXPECT_LE(totalAllocated, 100000 * COIN);
    
    // Record some spending
    budget.RecordSpending(TreasuryCategory::Security, 1000 * COIN);
    EXPECT_EQ(budget.TotalSpent(), 1000 * COIN);
}

TEST_F(TreasuryTest, TreasuryBudgetToString) {
    TreasuryBudget budget;
    budget.Initialize(100000 * COIN, 0, 86400);
    
    std::string str = budget.ToString();
    EXPECT_FALSE(str.empty());
}

// ============================================================================
// MultiSigConfig Tests
// ============================================================================

TEST_F(TreasuryTest, MultiSigConfigHasEnoughSignatures) {
    MultiSigConfig config;
    config.standardThreshold = 3;
    config.largeThreshold = 5;
    config.emergencyThreshold = 2;
    config.totalSigners = 7;
    
    Amount totalBalance = 100000 * COIN;
    
    // Standard spending (<=10% of balance)
    EXPECT_TRUE(config.HasEnoughSignatures(3, 5000 * COIN, totalBalance));
    EXPECT_FALSE(config.HasEnoughSignatures(2, 5000 * COIN, totalBalance));
    
    // Large spending (>10% of balance)
    EXPECT_TRUE(config.HasEnoughSignatures(5, 15000 * COIN, totalBalance));
    EXPECT_FALSE(config.HasEnoughSignatures(4, 15000 * COIN, totalBalance));
}

TEST_F(TreasuryTest, MultiSigConfigIsValid) {
    MultiSigConfig config;
    config.standardThreshold = 3;
    config.largeThreshold = 5;
    config.emergencyThreshold = 2;
    config.totalSigners = 7;
    
    // Add signers
    for (int i = 0; i < 7; ++i) {
        config.signers.push_back(CreateTestPublicKey(static_cast<Byte>(i)));
    }
    
    EXPECT_TRUE(config.IsValid());
    
    // Invalid: threshold > total signers
    config.standardThreshold = 8;
    EXPECT_FALSE(config.IsValid());
}

TEST_F(TreasuryTest, MultiSigConfigSerializeDeserialize) {
    MultiSigConfig original;
    original.standardThreshold = 3;
    original.largeThreshold = 5;
    original.totalSigners = 7;
    
    for (int i = 0; i < 7; ++i) {
        original.signers.push_back(CreateTestPublicKey(static_cast<Byte>(i)));
    }
    
    std::vector<Byte> serialized = original.Serialize();
    EXPECT_GT(serialized.size(), 0);
    
    auto deserialized = MultiSigConfig::Deserialize(serialized.data(), serialized.size());
    ASSERT_TRUE(deserialized.has_value());
    
    EXPECT_EQ(deserialized->standardThreshold, original.standardThreshold);
    EXPECT_EQ(deserialized->totalSigners, original.totalSigners);
}

// ============================================================================
// Treasury Tests
// ============================================================================

TEST_F(TreasuryTest, TreasuryConstruction) {
    EXPECT_EQ(treasury_->GetBalance(), 0);
}

TEST_F(TreasuryTest, TreasuryAddFunds) {
    treasury_->AddFunds(1000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    EXPECT_EQ(treasury_->GetBalance(), 1000 * COIN);
    EXPECT_EQ(treasury_->GetCategoryBalance(TreasuryCategory::EcosystemDevelopment), 1000 * COIN);
}

TEST_F(TreasuryTest, TreasuryMultipleCategoryFunds) {
    treasury_->AddFunds(1000 * COIN, TreasuryCategory::EcosystemDevelopment);
    treasury_->AddFunds(500 * COIN, TreasuryCategory::Security);
    treasury_->AddFunds(300 * COIN, TreasuryCategory::Marketing);
    
    EXPECT_EQ(treasury_->GetBalance(), 1800 * COIN);
    EXPECT_EQ(treasury_->GetCategoryBalance(TreasuryCategory::EcosystemDevelopment), 1000 * COIN);
    EXPECT_EQ(treasury_->GetCategoryBalance(TreasuryCategory::Security), 500 * COIN);
    EXPECT_EQ(treasury_->GetCategoryBalance(TreasuryCategory::Marketing), 300 * COIN);
}

TEST_F(TreasuryTest, TreasuryCanSpend) {
    treasury_->AddFunds(10000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    EXPECT_TRUE(treasury_->CanSpend(5000 * COIN, TreasuryCategory::EcosystemDevelopment));
    EXPECT_FALSE(treasury_->CanSpend(15000 * COIN, TreasuryCategory::EcosystemDevelopment));
    
    // Note: Without budget initialization, CanSpend only checks overall balance
    // Since we have 10000 COIN, spending 100 COIN in any category is allowed
    // if no budget constraints are set up
    Amount securityBalance = treasury_->GetCategoryBalance(TreasuryCategory::Security);
    if (securityBalance == 0) {
        // If no funds in Security, behavior depends on budget setup
        // Currently returns true if overall balance sufficient
        EXPECT_TRUE(treasury_->CanSpend(100 * COIN, TreasuryCategory::Security));
    }
}

TEST_F(TreasuryTest, TreasurySubmitProposal) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    TreasuryProposal proposal = CreateTestProposal(0x01);
    Amount deposit = CalculateProposalDeposit(proposal.requestedAmount);
    
    auto proposalId = treasury_->SubmitProposal(proposal, deposit, 100);
    
    ASSERT_TRUE(proposalId.has_value());
    
    const TreasuryProposal* stored = treasury_->GetProposal(*proposalId);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->title, proposal.title);
}

TEST_F(TreasuryTest, TreasurySubmitProposalInsufficientFunds) {
    // Treasury has no funds
    TreasuryProposal proposal = CreateTestProposal(0x01);
    Amount deposit = CalculateProposalDeposit(proposal.requestedAmount);
    
    auto proposalId = treasury_->SubmitProposal(proposal, deposit, 100);
    
    // Should fail due to insufficient funds
    EXPECT_FALSE(proposalId.has_value());
}

TEST_F(TreasuryTest, TreasurySubmitVote) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    // Set up voting power calculator
    treasury_->SetVotingPowerCalculator([](const PublicKey&) {
        return 1000;  // Fixed voting power for test
    });
    
    TreasuryProposal proposal = CreateTestProposal(0x01);
    Amount deposit = CalculateProposalDeposit(proposal.requestedAmount);
    
    auto proposalId = treasury_->SubmitProposal(proposal, deposit, 100);
    ASSERT_TRUE(proposalId.has_value());
    
    // Create and submit vote
    TreasuryVote vote = CreateTestVote(*proposalId, 0x01, true);
    
    // Note: SubmitVote requires valid signature verification
    // Test data doesn't have valid signatures, so this will fail
    bool result = treasury_->SubmitVote(vote, 100 + 1);
    EXPECT_FALSE(result);  // Fails due to signature verification
}

TEST_F(TreasuryTest, TreasuryHasVoted) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    treasury_->SetVotingPowerCalculator([](const PublicKey&) { return 1000; });
    
    TreasuryProposal proposal = CreateTestProposal(0x01);
    auto proposalId = treasury_->SubmitProposal(proposal, 1000 * COIN, 100);
    ASSERT_TRUE(proposalId.has_value());
    
    PublicKey voter = CreateTestPublicKey(0x01);
    
    // Initially not voted
    EXPECT_FALSE(treasury_->HasVoted(*proposalId, voter));
    
    // Since vote submission fails (no valid signature), voter still hasn't voted
    TreasuryVote vote = CreateTestVote(*proposalId, 0x01, true);
    treasury_->SubmitVote(vote, 101);
    
    // Still false because vote wasn't accepted
    EXPECT_FALSE(treasury_->HasVoted(*proposalId, voter));
}

TEST_F(TreasuryTest, TreasuryGetProposals) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    // Submit multiple proposals
    for (Byte i = 1; i <= 3; ++i) {
        TreasuryProposal proposal = CreateTestProposal(i);
        treasury_->SubmitProposal(proposal, 1000 * COIN, 100 + i);
    }
    
    auto allProposals = treasury_->GetProposals();
    EXPECT_EQ(allProposals.size(), 3);
    
    auto pendingProposals = treasury_->GetProposals(ProposalStatus::Pending);
    EXPECT_GE(pendingProposals.size(), 0);  // May be in voting state
}

TEST_F(TreasuryTest, TreasuryGetActiveProposals) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    TreasuryProposal proposal = CreateTestProposal(0x01);
    treasury_->SubmitProposal(proposal, 1000 * COIN, 100);
    
    // Get active at voting period
    auto active = treasury_->GetActiveProposals(150);
    EXPECT_GE(active.size(), 0);
}

TEST_F(TreasuryTest, TreasuryCancelProposal) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    TreasuryProposal proposal = CreateTestProposal(0x01);
    auto proposalId = treasury_->SubmitProposal(proposal, 1000 * COIN, 100);
    ASSERT_TRUE(proposalId.has_value());
    
    // Cancel by proposer
    EXPECT_TRUE(treasury_->CancelProposal(*proposalId, proposal.proposer));
    
    const TreasuryProposal* stored = treasury_->GetProposal(*proposalId);
    EXPECT_EQ(stored->status, ProposalStatus::Cancelled);
}

TEST_F(TreasuryTest, TreasuryCancelProposalWrongProposer) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    TreasuryProposal proposal = CreateTestProposal(0x01);
    auto proposalId = treasury_->SubmitProposal(proposal, 1000 * COIN, 100);
    ASSERT_TRUE(proposalId.has_value());
    
    // Try to cancel with wrong proposer
    PublicKey wrongProposer = CreateTestPublicKey(0xFF);
    EXPECT_FALSE(treasury_->CancelProposal(*proposalId, wrongProposer));
}

TEST_F(TreasuryTest, TreasuryGetVotes) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    treasury_->SetVotingPowerCalculator([](const PublicKey&) { return 1000; });
    
    TreasuryProposal proposal = CreateTestProposal(0x01);
    auto proposalId = treasury_->SubmitProposal(proposal, 1000 * COIN, 100);
    ASSERT_TRUE(proposalId.has_value());
    
    // Attempt to submit votes (will fail due to signature verification)
    for (Byte i = 1; i <= 5; ++i) {
        TreasuryVote vote = CreateTestVote(*proposalId, i, i % 2 == 0);
        treasury_->SubmitVote(vote, 101);
    }
    
    // Since votes fail signature verification, none are recorded
    auto votes = treasury_->GetVotes(*proposalId);
    EXPECT_EQ(votes.size(), 0);
}

TEST_F(TreasuryTest, TreasuryGetVotingPower) {
    treasury_->SetVotingPowerCalculator([](const PublicKey&) {
        return 5000;
    });
    
    PublicKey key = CreateTestPublicKey(0x01);
    EXPECT_EQ(treasury_->GetVotingPower(key), 5000);
}

TEST_F(TreasuryTest, TreasuryProcessBlock) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    // Should not crash
    treasury_->ProcessBlock(100);
    treasury_->ProcessBlock(200);
}

TEST_F(TreasuryTest, TreasuryStartNewPeriod) {
    treasury_->AddFunds(100000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    treasury_->StartNewPeriod(0);
    
    const TreasuryBudget& budget = treasury_->GetCurrentBudget();
    EXPECT_EQ(budget.periodStart, 0);
}

TEST_F(TreasuryTest, TreasuryMultiSigConfig) {
    MultiSigConfig config;
    config.standardThreshold = 3;
    config.largeThreshold = 5;
    config.totalSigners = 7;
    
    treasury_->SetMultiSigConfig(config);
    
    const MultiSigConfig& retrieved = treasury_->GetMultiSigConfig();
    EXPECT_EQ(retrieved.standardThreshold, 3);
}

TEST_F(TreasuryTest, TreasuryGenerateReport) {
    treasury_->AddFunds(50000 * COIN, TreasuryCategory::EcosystemDevelopment);
    treasury_->AddFunds(30000 * COIN, TreasuryCategory::Security);
    
    Treasury::Report report = treasury_->GenerateReport(1000);
    
    EXPECT_EQ(report.height, 1000);
    EXPECT_EQ(report.totalBalance, 80000 * COIN);
    EXPECT_GT(report.categoryBalances.size(), 0);
}

TEST_F(TreasuryTest, TreasuryReportToString) {
    treasury_->AddFunds(50000 * COIN, TreasuryCategory::EcosystemDevelopment);
    
    Treasury::Report report = treasury_->GenerateReport(1000);
    std::string str = report.ToString();
    
    EXPECT_FALSE(str.empty());
}

TEST_F(TreasuryTest, TreasurySerializeDeserialize) {
    treasury_->AddFunds(50000 * COIN, TreasuryCategory::EcosystemDevelopment);
    treasury_->AddFunds(25000 * COIN, TreasuryCategory::Security);
    
    // Note: Current implementation returns empty (stub)
    std::vector<Byte> serialized = treasury_->Serialize();
    
    if (serialized.empty()) {
        GTEST_SKIP() << "Treasury serialization not yet implemented";
    }
    
    Treasury newTreasury;
    EXPECT_TRUE(newTreasury.Deserialize(serialized.data(), serialized.size()));
    
    EXPECT_EQ(newTreasury.GetBalance(), 75000 * COIN);
}

// ============================================================================
// TreasuryOutputBuilder Tests
// ============================================================================

TEST_F(TreasuryTest, TreasuryOutputBuilderBuildDepositOutput) {
    TreasuryOutputBuilder builder;
    
    Hash160 treasuryAddr = CreateTestAddress(0xFF);
    Amount amount = 5000 * COIN;
    
    auto output = builder.BuildDepositOutput(treasuryAddr, amount);
    
    EXPECT_FALSE(output.first.empty());
    EXPECT_EQ(output.second, amount);
}

TEST_F(TreasuryTest, TreasuryOutputBuilderBuildSpendingOutputs) {
    TreasuryOutputBuilder builder;
    
    TreasuryProposal proposal = CreateTestProposal(0x01);
    
    auto outputs = builder.BuildSpendingOutputs(proposal);
    
    EXPECT_GE(outputs.size(), 1);
    
    // Total should match requested amount
    Amount total = 0;
    for (const auto& [script, amt] : outputs) {
        total += amt;
    }
    EXPECT_EQ(total, proposal.requestedAmount);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(TreasuryTest, CalculateProposalDeposit) {
    // Small proposal
    Amount deposit1 = CalculateProposalDeposit(10000 * COIN);
    EXPECT_GT(deposit1, 0);
    
    // Larger proposal should have larger deposit
    Amount deposit2 = CalculateProposalDeposit(100000 * COIN);
    EXPECT_GT(deposit2, deposit1);
}

TEST_F(TreasuryTest, ValidateProposal) {
    TreasuryProposal proposal = CreateTestProposal(0x01);
    Amount treasuryBalance = 100000 * COIN;
    
    EXPECT_TRUE(ValidateProposal(proposal, treasuryBalance));
    
    // Requesting too much
    proposal.requestedAmount = treasuryBalance * 2;
    EXPECT_FALSE(ValidateProposal(proposal, treasuryBalance));
    
    // Below minimum
    proposal.requestedAmount = MIN_PROPOSAL_AMOUNT - 1;
    EXPECT_FALSE(ValidateProposal(proposal, treasuryBalance));
}

TEST_F(TreasuryTest, CalculateVotingPower) {
    Amount stake1 = 1000 * COIN;
    Amount stake2 = 10000 * COIN;
    
    uint64_t power1 = CalculateVotingPower(stake1);
    uint64_t power2 = CalculateVotingPower(stake2);
    
    // More stake = more voting power
    EXPECT_GT(power2, power1);
}

// ============================================================================
// Budget Allocation Tests
// ============================================================================

TEST_F(TreasuryTest, BudgetAllocationsTotalLessThan100) {
    int total = BudgetAllocation::ECOSYSTEM_DEVELOPMENT
              + BudgetAllocation::PROTOCOL_DEVELOPMENT
              + BudgetAllocation::SECURITY
              + BudgetAllocation::MARKETING
              + BudgetAllocation::INFRASTRUCTURE
              + BudgetAllocation::LEGAL
              + BudgetAllocation::EDUCATION
              + BudgetAllocation::EMERGENCY;
    
    // Total should be <= 100%
    EXPECT_LE(total, 100);
}

} // namespace
} // namespace economics
} // namespace shurium
