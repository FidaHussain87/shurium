// SHURIUM - Treasury Management System Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/economics/treasury.h>
#include <shurium/crypto/sha256.h>
#include <shurium/core/serialize.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace shurium {
namespace economics {

// ============================================================================
// Category Utilities
// ============================================================================

const char* TreasuryCategoryToString(TreasuryCategory category) {
    switch (category) {
        case TreasuryCategory::EcosystemDevelopment: return "EcosystemDevelopment";
        case TreasuryCategory::ProtocolDevelopment:  return "ProtocolDevelopment";
        case TreasuryCategory::Security:             return "Security";
        case TreasuryCategory::Marketing:            return "Marketing";
        case TreasuryCategory::Infrastructure:       return "Infrastructure";
        case TreasuryCategory::Legal:                return "Legal";
        case TreasuryCategory::Education:            return "Education";
        case TreasuryCategory::Emergency:            return "Emergency";
        case TreasuryCategory::Other:                return "Other";
        default:                                     return "Unknown";
    }
}

std::optional<TreasuryCategory> ParseTreasuryCategory(const std::string& str) {
    if (str == "EcosystemDevelopment") return TreasuryCategory::EcosystemDevelopment;
    if (str == "ProtocolDevelopment") return TreasuryCategory::ProtocolDevelopment;
    if (str == "Security") return TreasuryCategory::Security;
    if (str == "Marketing") return TreasuryCategory::Marketing;
    if (str == "Infrastructure") return TreasuryCategory::Infrastructure;
    if (str == "Legal") return TreasuryCategory::Legal;
    if (str == "Education") return TreasuryCategory::Education;
    if (str == "Emergency") return TreasuryCategory::Emergency;
    if (str == "Other") return TreasuryCategory::Other;
    return std::nullopt;
}

// ============================================================================
// ProposalStatus Utilities
// ============================================================================

const char* ProposalStatusToString(ProposalStatus status) {
    switch (status) {
        case ProposalStatus::Pending:   return "Pending";
        case ProposalStatus::Voting:    return "Voting";
        case ProposalStatus::Approved:  return "Approved";
        case ProposalStatus::Rejected:  return "Rejected";
        case ProposalStatus::Executed:  return "Executed";
        case ProposalStatus::Cancelled: return "Cancelled";
        case ProposalStatus::Expired:   return "Expired";
        case ProposalStatus::Failed:    return "Failed";
        default:                        return "Unknown";
    }
}

// ============================================================================
// TreasuryProposal
// ============================================================================

Hash256 TreasuryProposal::CalculateHash() const {
    std::vector<Byte> data;
    
    // Title
    data.insert(data.end(), title.begin(), title.end());
    
    // Amount
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((requestedAmount >> (i * 8)) & 0xFF));
    }
    
    // Recipient
    data.insert(data.end(), recipient.begin(), recipient.end());
    
    // Category
    data.push_back(static_cast<Byte>(category));
    
    // Submit height
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((submitHeight >> (i * 8)) & 0xFF));
    }
    
    return SHA256Hash(data.data(), data.size());
}

double TreasuryProposal::GetApprovalPercent() const {
    uint64_t totalVotes = votesFor + votesAgainst;
    if (totalVotes == 0) {
        return 0.0;
    }
    return static_cast<double>(votesFor) / totalVotes * 100.0;
}

double TreasuryProposal::GetQuorumPercent() const {
    if (totalVotingPower == 0) {
        return 0.0;
    }
    uint64_t totalVotes = votesFor + votesAgainst;
    return static_cast<double>(totalVotes) / totalVotingPower * 100.0;
}

bool TreasuryProposal::IsPassed() const {
    return GetApprovalPercent() >= MIN_APPROVAL_PERCENT && HasQuorum();
}

bool TreasuryProposal::HasQuorum() const {
    return GetQuorumPercent() >= QUORUM_PERCENT;
}

bool TreasuryProposal::IsVotingActive(int currentHeight) const {
    return status == ProposalStatus::Voting &&
           currentHeight >= votingStartHeight &&
           currentHeight <= votingEndHeight;
}

bool TreasuryProposal::IsReadyForExecution(int currentHeight) const {
    return status == ProposalStatus::Approved &&
           currentHeight >= executionHeight;
}

Amount TreasuryProposal::GetTotalAmount() const {
    if (milestones.empty()) {
        return requestedAmount;
    }
    
    Amount total = 0;
    for (const auto& m : milestones) {
        total += m.amount;
    }
    return total;
}

std::vector<Byte> TreasuryProposal::Serialize() const {
    std::vector<Byte> data;
    
    // ID
    data.insert(data.end(), id.begin(), id.end());
    
    // Title length and data
    uint16_t titleLen = static_cast<uint16_t>(title.size());
    data.push_back(static_cast<Byte>(titleLen & 0xFF));
    data.push_back(static_cast<Byte>((titleLen >> 8) & 0xFF));
    data.insert(data.end(), title.begin(), title.end());
    
    // Description length and data
    uint32_t descLen = static_cast<uint32_t>(description.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((descLen >> (i * 8)) & 0xFF));
    }
    data.insert(data.end(), description.begin(), description.end());
    
    // Category
    data.push_back(static_cast<Byte>(category));
    
    // Requested amount
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((requestedAmount >> (i * 8)) & 0xFF));
    }
    
    // Recipient
    data.insert(data.end(), recipient.begin(), recipient.end());
    
    // Status
    data.push_back(static_cast<Byte>(status));
    
    // Heights
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((submitHeight >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((votingStartHeight >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((votingEndHeight >> (i * 8)) & 0xFF));
    }
    
    // Votes
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((votesFor >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((votesAgainst >> (i * 8)) & 0xFF));
    }
    
    return data;
}

std::optional<TreasuryProposal> TreasuryProposal::Deserialize(const Byte* data, size_t len) {
    if (len < 100) {
        return std::nullopt;
    }
    
    TreasuryProposal proposal;
    size_t offset = 0;
    
    // ID
    proposal.id = Hash256(data + offset, 32);
    offset += 32;
    
    // Title
    if (offset + 2 > len) return std::nullopt;
    uint16_t titleLen = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    if (offset + titleLen > len) return std::nullopt;
    proposal.title.assign(reinterpret_cast<const char*>(data + offset), titleLen);
    offset += titleLen;
    
    // Description
    if (offset + 4 > len) return std::nullopt;
    uint32_t descLen = 0;
    for (int i = 0; i < 4; ++i) {
        descLen |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    if (offset + descLen > len) return std::nullopt;
    proposal.description.assign(reinterpret_cast<const char*>(data + offset), descLen);
    offset += descLen;
    
    // Category
    proposal.category = static_cast<TreasuryCategory>(data[offset++]);
    
    // Amount
    proposal.requestedAmount = 0;
    for (int i = 0; i < 8; ++i) {
        proposal.requestedAmount |= static_cast<Amount>(data[offset++]) << (i * 8);
    }
    
    // Recipient
    std::copy(data + offset, data + offset + 20, proposal.recipient.begin());
    offset += 20;
    
    // Status
    proposal.status = static_cast<ProposalStatus>(data[offset++]);
    
    // Heights
    proposal.submitHeight = 0;
    for (int i = 0; i < 4; ++i) {
        proposal.submitHeight |= static_cast<int>(data[offset++]) << (i * 8);
    }
    proposal.votingStartHeight = 0;
    for (int i = 0; i < 4; ++i) {
        proposal.votingStartHeight |= static_cast<int>(data[offset++]) << (i * 8);
    }
    proposal.votingEndHeight = 0;
    for (int i = 0; i < 4; ++i) {
        proposal.votingEndHeight |= static_cast<int>(data[offset++]) << (i * 8);
    }
    
    // Votes
    proposal.votesFor = 0;
    for (int i = 0; i < 8; ++i) {
        proposal.votesFor |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    proposal.votesAgainst = 0;
    for (int i = 0; i < 8; ++i) {
        proposal.votesAgainst |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    return proposal;
}

std::string TreasuryProposal::ToString() const {
    std::ostringstream ss;
    ss << "TreasuryProposal {"
       << " id: " << id.ToHex().substr(0, 16) << "..."
       << ", title: \"" << title.substr(0, 30) << (title.length() > 30 ? "..." : "") << "\""
       << ", amount: " << FormatAmount(requestedAmount)
       << ", status: " << ProposalStatusToString(status)
       << ", approval: " << GetApprovalPercent() << "%"
       << " }";
    return ss.str();
}

// ============================================================================
// TreasuryVote
// ============================================================================

Hash256 TreasuryVote::GetHash() const {
    std::vector<Byte> data = GetSignatureMessage();
    return SHA256Hash(data.data(), data.size());
}

std::vector<Byte> TreasuryVote::GetSignatureMessage() const {
    std::vector<Byte> msg;
    
    // Proposal ID
    msg.insert(msg.end(), proposalId.begin(), proposalId.end());
    
    // Voter public key
    auto voterData = voter.ToVector();
    msg.insert(msg.end(), voterData.begin(), voterData.end());
    
    // Vote
    msg.push_back(inFavor ? 1 : 0);
    
    // Voting power
    for (int i = 0; i < 8; ++i) {
        msg.push_back(static_cast<Byte>((votingPower >> (i * 8)) & 0xFF));
    }
    
    // Height
    for (int i = 0; i < 4; ++i) {
        msg.push_back(static_cast<Byte>((voteHeight >> (i * 8)) & 0xFF));
    }
    
    return msg;
}

bool TreasuryVote::VerifySignature() const {
    if (signature.empty() || !voter.IsValid()) {
        return false;
    }
    
    // In production, verify ECDSA signature
    auto message = GetSignatureMessage();
    Hash256 msgHash = SHA256Hash(message.data(), message.size());
    return voter.Verify(msgHash, signature);
}

std::vector<Byte> TreasuryVote::Serialize() const {
    std::vector<Byte> data;
    
    // Proposal ID
    data.insert(data.end(), proposalId.begin(), proposalId.end());
    
    // Voter
    auto voterData = voter.ToVector();
    data.push_back(static_cast<Byte>(voterData.size()));
    data.insert(data.end(), voterData.begin(), voterData.end());
    
    // Vote
    data.push_back(inFavor ? 1 : 0);
    
    // Voting power
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<Byte>((votingPower >> (i * 8)) & 0xFF));
    }
    
    // Height
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<Byte>((voteHeight >> (i * 8)) & 0xFF));
    }
    
    // Signature
    uint16_t sigLen = static_cast<uint16_t>(signature.size());
    data.push_back(static_cast<Byte>(sigLen & 0xFF));
    data.push_back(static_cast<Byte>((sigLen >> 8) & 0xFF));
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<TreasuryVote> TreasuryVote::Deserialize(const Byte* data, size_t len) {
    if (len < 50) {
        return std::nullopt;
    }
    
    TreasuryVote vote;
    size_t offset = 0;
    
    // Proposal ID
    vote.proposalId = Hash256(data + offset, 32);
    offset += 32;
    
    // Voter
    if (offset >= len) return std::nullopt;
    uint8_t voterLen = data[offset++];
    if (offset + voterLen > len) return std::nullopt;
    vote.voter = PublicKey(data + offset, voterLen);
    offset += voterLen;
    
    // Vote
    vote.inFavor = data[offset++] != 0;
    
    // Voting power
    vote.votingPower = 0;
    for (int i = 0; i < 8; ++i) {
        vote.votingPower |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Height
    vote.voteHeight = 0;
    for (int i = 0; i < 4; ++i) {
        vote.voteHeight |= static_cast<int>(data[offset++]) << (i * 8);
    }
    
    // Signature
    if (offset + 2 > len) return std::nullopt;
    uint16_t sigLen = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    if (offset + sigLen > len) return std::nullopt;
    vote.signature.assign(data + offset, data + offset + sigLen);
    
    return vote;
}

// ============================================================================
// TreasuryBudget
// ============================================================================

void TreasuryBudget::Initialize(Amount balance, int startHeight, int periodBlocks) {
    periodStart = startHeight;
    periodEnd = startHeight + periodBlocks;
    totalBalance = balance;
    
    categories.clear();
    
    // Allocate budgets by category
    auto allocateCategory = [this, balance](TreasuryCategory cat, int percent) {
        CategoryBudget budget;
        budget.category = cat;
        budget.allocated = (balance * percent) / 100;
        budget.spent = 0;
        categories[cat] = budget;
    };
    
    allocateCategory(TreasuryCategory::EcosystemDevelopment, BudgetAllocation::ECOSYSTEM_DEVELOPMENT);
    allocateCategory(TreasuryCategory::ProtocolDevelopment, BudgetAllocation::PROTOCOL_DEVELOPMENT);
    allocateCategory(TreasuryCategory::Security, BudgetAllocation::SECURITY);
    allocateCategory(TreasuryCategory::Marketing, BudgetAllocation::MARKETING);
    allocateCategory(TreasuryCategory::Infrastructure, BudgetAllocation::INFRASTRUCTURE);
    allocateCategory(TreasuryCategory::Legal, BudgetAllocation::LEGAL);
    allocateCategory(TreasuryCategory::Education, BudgetAllocation::EDUCATION);
    allocateCategory(TreasuryCategory::Emergency, BudgetAllocation::EMERGENCY);
}

const CategoryBudget* TreasuryBudget::GetCategory(TreasuryCategory cat) const {
    auto it = categories.find(cat);
    if (it == categories.end()) {
        return nullptr;
    }
    return &it->second;
}

bool TreasuryBudget::RecordSpending(TreasuryCategory cat, Amount amount) {
    auto it = categories.find(cat);
    if (it == categories.end()) {
        return false;
    }
    
    if (it->second.Remaining() < amount) {
        return false;
    }
    
    it->second.spent += amount;
    return true;
}

Amount TreasuryBudget::TotalAllocated() const {
    Amount total = 0;
    for (const auto& [cat, budget] : categories) {
        total += budget.allocated;
    }
    return total;
}

Amount TreasuryBudget::TotalSpent() const {
    Amount total = 0;
    for (const auto& [cat, budget] : categories) {
        total += budget.spent;
    }
    return total;
}

std::string TreasuryBudget::ToString() const {
    std::ostringstream ss;
    ss << "TreasuryBudget {"
       << " period: " << periodStart << "-" << periodEnd
       << ", balance: " << FormatAmount(totalBalance)
       << ", allocated: " << FormatAmount(TotalAllocated())
       << ", spent: " << FormatAmount(TotalSpent())
       << " }";
    return ss.str();
}

// ============================================================================
// MultiSigConfig
// ============================================================================

bool MultiSigConfig::HasEnoughSignatures(int count, Amount amount, Amount totalBalance) const {
    // Determine required threshold
    int required;
    
    Amount largeThresholdAmount = (totalBalance * 10) / 100; // 10% of balance
    
    if (amount > largeThresholdAmount) {
        required = largeThreshold;
    } else {
        required = standardThreshold;
    }
    
    return count >= required;
}

bool MultiSigConfig::IsValid() const {
    return standardThreshold > 0 &&
           standardThreshold <= totalSigners &&
           largeThreshold >= standardThreshold &&
           largeThreshold <= totalSigners &&
           emergencyThreshold > 0 &&
           emergencyThreshold <= totalSigners &&
           signers.size() == static_cast<size_t>(totalSigners);
}

std::vector<Byte> MultiSigConfig::Serialize() const {
    std::vector<Byte> data;
    
    data.push_back(static_cast<Byte>(standardThreshold));
    data.push_back(static_cast<Byte>(largeThreshold));
    data.push_back(static_cast<Byte>(emergencyThreshold));
    data.push_back(static_cast<Byte>(totalSigners));
    
    for (const auto& signer : signers) {
        auto signerData = signer.ToVector();
        data.push_back(static_cast<Byte>(signerData.size()));
        data.insert(data.end(), signerData.begin(), signerData.end());
    }
    
    return data;
}

std::optional<MultiSigConfig> MultiSigConfig::Deserialize(const Byte* data, size_t len) {
    if (len < 4) {
        return std::nullopt;
    }
    
    MultiSigConfig config;
    size_t offset = 0;
    
    config.standardThreshold = data[offset++];
    config.largeThreshold = data[offset++];
    config.emergencyThreshold = data[offset++];
    config.totalSigners = data[offset++];
    
    for (int i = 0; i < config.totalSigners; ++i) {
        if (offset >= len) return std::nullopt;
        uint8_t signerLen = data[offset++];
        if (offset + signerLen > len) return std::nullopt;
        config.signers.emplace_back(data + offset, signerLen);
        offset += signerLen;
    }
    
    return config;
}

// ============================================================================
// Treasury
// ============================================================================

Treasury::Treasury() {
    votingPowerCalculator_ = [](const PublicKey&) -> uint64_t {
        return 1; // Default: each voter has power 1
    };
}

Treasury::~Treasury() = default;

void Treasury::AddFunds(Amount amount, TreasuryCategory category) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    balance_ += amount;
    categoryBalances_[category] += amount;
}

Amount Treasury::GetCategoryBalance(TreasuryCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = categoryBalances_.find(category);
    if (it == categoryBalances_.end()) {
        return 0;
    }
    return it->second;
}

bool Treasury::CanSpend(Amount amount, TreasuryCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check overall balance
    if (amount > balance_) {
        return false;
    }
    
    // Check category budget
    const CategoryBudget* budget = currentBudget_.GetCategory(category);
    if (budget && budget->Remaining() < amount) {
        return false;
    }
    
    return true;
}

bool Treasury::ExecuteSpending(const ProposalId& proposalId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = proposals_.find(proposalId);
    if (it == proposals_.end()) {
        return false;
    }
    
    TreasuryProposal& proposal = it->second;
    
    if (proposal.status != ProposalStatus::Approved) {
        return false;
    }
    
    if (proposal.requestedAmount > balance_) {
        proposal.status = ProposalStatus::Failed;
        return false;
    }
    
    // Deduct from balance
    balance_ -= proposal.requestedAmount;
    
    // Deduct from category
    auto catIt = categoryBalances_.find(proposal.category);
    if (catIt != categoryBalances_.end()) {
        catIt->second = catIt->second > proposal.requestedAmount 
                      ? catIt->second - proposal.requestedAmount 
                      : 0;
    }
    
    // Record in budget
    currentBudget_.RecordSpending(proposal.category, proposal.requestedAmount);
    
    proposal.status = ProposalStatus::Executed;
    return true;
}

std::optional<ProposalId> Treasury::SubmitProposal(
    TreasuryProposal proposal,
    Amount deposit,
    int currentHeight
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate proposal
    if (proposal.requestedAmount < MIN_PROPOSAL_AMOUNT) {
        return std::nullopt;
    }
    
    // Check maximum proposal amount
    Amount maxAmount = (balance_ * MAX_PROPOSAL_PERCENT) / 100;
    if (proposal.requestedAmount > maxAmount) {
        return std::nullopt;
    }
    
    // Calculate required deposit
    Amount requiredDeposit = CalculateProposalDeposit(proposal.requestedAmount);
    if (deposit < requiredDeposit) {
        return std::nullopt;
    }
    
    // Set proposal fields
    proposal.id = proposal.CalculateHash();
    proposal.deposit = deposit;
    proposal.status = ProposalStatus::Voting;
    proposal.submitHeight = currentHeight;
    proposal.votingStartHeight = currentHeight;
    proposal.votingEndHeight = currentHeight + PROPOSAL_VOTING_PERIOD;
    proposal.executionHeight = proposal.votingEndHeight + PROPOSAL_EXECUTION_DELAY;
    
    // Calculate total voting power at this snapshot
    // In production, this would query stake distribution
    proposal.totalVotingPower = 1000000; // Placeholder
    
    proposals_[proposal.id] = proposal;
    
    return proposal.id;
}

bool Treasury::SubmitVote(const TreasuryVote& vote, int currentHeight) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find proposal
    auto it = proposals_.find(vote.proposalId);
    if (it == proposals_.end()) {
        return false;
    }
    
    TreasuryProposal& proposal = it->second;
    
    // Check voting is active
    if (!proposal.IsVotingActive(currentHeight)) {
        return false;
    }
    
    // Check not already voted
    auto& proposalVotes = votes_[vote.proposalId];
    for (const auto& v : proposalVotes) {
        if (v.voter == vote.voter) {
            return false; // Already voted
        }
    }
    
    // Verify signature
    if (!vote.VerifySignature()) {
        return false;
    }
    
    // Record vote
    proposalVotes.push_back(vote);
    
    // Update proposal vote counts
    if (vote.inFavor) {
        proposal.votesFor += vote.votingPower;
    } else {
        proposal.votesAgainst += vote.votingPower;
    }
    
    return true;
}

const TreasuryProposal* Treasury::GetProposal(const ProposalId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = proposals_.find(id);
    if (it == proposals_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<const TreasuryProposal*> Treasury::GetProposals(
    std::optional<ProposalStatus> status
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<const TreasuryProposal*> result;
    for (const auto& [id, proposal] : proposals_) {
        if (!status || proposal.status == *status) {
            result.push_back(&proposal);
        }
    }
    return result;
}

std::vector<const TreasuryProposal*> Treasury::GetActiveProposals(int currentHeight) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<const TreasuryProposal*> result;
    for (const auto& [id, proposal] : proposals_) {
        if (proposal.IsVotingActive(currentHeight)) {
            result.push_back(&proposal);
        }
    }
    return result;
}

bool Treasury::CancelProposal(const ProposalId& id, const PublicKey& proposer) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = proposals_.find(id);
    if (it == proposals_.end()) {
        return false;
    }
    
    TreasuryProposal& proposal = it->second;
    
    // Can only cancel if still in voting and by the proposer
    if (proposal.status != ProposalStatus::Voting) {
        return false;
    }
    
    if (proposal.proposer != proposer) {
        return false;
    }
    
    proposal.status = ProposalStatus::Cancelled;
    return true;
}

std::vector<TreasuryVote> Treasury::GetVotes(const ProposalId& proposalId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = votes_.find(proposalId);
    if (it == votes_.end()) {
        return {};
    }
    return it->second;
}

bool Treasury::HasVoted(const ProposalId& proposalId, const PublicKey& voter) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = votes_.find(proposalId);
    if (it == votes_.end()) {
        return false;
    }
    
    for (const auto& vote : it->second) {
        if (vote.voter == voter) {
            return true;
        }
    }
    return false;
}

uint64_t Treasury::GetVotingPower(const PublicKey& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return votingPowerCalculator_(key);
}

void Treasury::SetVotingPowerCalculator(VotingPowerCalculator calculator) {
    std::lock_guard<std::mutex> lock(mutex_);
    votingPowerCalculator_ = std::move(calculator);
}

void Treasury::ProcessBlock(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Update proposal statuses
    for (auto& [id, proposal] : proposals_) {
        UpdateProposalStatus(proposal, height);
    }
    
    // Execute approved proposals
    ExecuteApprovedProposals(height);
    
    // Process milestones
    ProcessMilestones(height);
    
    // Process deposits
    ProcessDeposits(height);
}

void Treasury::StartNewPeriod(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentBudget_.Initialize(balance_, height, TREASURY_REPORT_INTERVAL);
}

void Treasury::SetMultiSigConfig(const MultiSigConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    multiSigConfig_ = config;
}

bool Treasury::EmergencyWithdraw(
    Amount amount,
    const Hash160& recipient,
    const std::vector<std::pair<PublicKey, std::vector<Byte>>>& signatures
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check we have enough balance
    if (amount > balance_) {
        return false;
    }
    
    // Create message to sign for emergency withdrawal
    // Include all relevant data to prevent replay attacks
    DataStream ss;
    std::string prefix = "SHURIUM_EMERGENCY_WITHDRAW";
    ss.Write(reinterpret_cast<const uint8_t*>(prefix.data()), prefix.size());
    ser_writedata64(ss, static_cast<uint64_t>(amount));
    ss.Write(recipient.data(), recipient.size());
    ser_writedata64(ss, static_cast<uint64_t>(balance_));  // Include current balance
    Hash256 messageHash = SHA256Hash(ss.data(), ss.size());
    
    // Verify signatures
    int validSigs = 0;
    std::set<PublicKey> usedSigners;  // Prevent duplicate signatures
    
    for (const auto& [pubkey, sig] : signatures) {
        // Check if signer is authorized
        bool isAuthorized = false;
        for (const auto& signer : multiSigConfig_.signers) {
            if (signer == pubkey) {
                isAuthorized = true;
                break;
            }
        }
        
        if (!isAuthorized) {
            continue;
        }
        
        // Check for duplicate signer
        if (usedSigners.count(pubkey) > 0) {
            continue;  // Same signer can't sign twice
        }
        
        // Verify signature
        if (!pubkey.IsValid() || sig.empty()) {
            continue;
        }
        
        // Verify the ECDSA signature
        if (pubkey.Verify(messageHash, sig)) {
            validSigs++;
            usedSigners.insert(pubkey);
        }
    }
    
    // Check threshold
    if (validSigs < multiSigConfig_.emergencyThreshold) {
        return false;
    }
    
    // Execute withdrawal
    balance_ -= amount;
    
    return true;
}

Treasury::Report Treasury::GenerateReport(int height) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Report report;
    report.timestamp = std::chrono::system_clock::now();
    report.height = height;
    report.totalBalance = balance_;
    report.categoryBalances = categoryBalances_;
    
    // Count proposals
    for (const auto& [id, proposal] : proposals_) {
        if (proposal.IsVotingActive(height)) {
            report.activeProposals++;
        }
        if (proposal.status == ProposalStatus::Executed &&
            proposal.executionHeight >= currentBudget_.periodStart) {
            report.executedProposals++;
        }
    }
    
    report.periodReceived = 0; // Would need tracking
    report.periodSpent = currentBudget_.TotalSpent();
    
    return report;
}

std::string Treasury::Report::ToString() const {
    std::ostringstream ss;
    ss << "Treasury Report {"
       << " height: " << height
       << ", balance: " << FormatAmount(totalBalance)
       << ", spent: " << FormatAmount(periodSpent)
       << ", active proposals: " << activeProposals
       << ", executed: " << executedProposals
       << " }";
    return ss.str();
}

std::vector<Byte> Treasury::Serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Byte> data;
    // Simplified serialization
    return data;
}

bool Treasury::Deserialize(const Byte* data, size_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    (void)data;
    (void)len;
    return true;
}

void Treasury::UpdateProposalStatus(TreasuryProposal& proposal, int currentHeight) {
    if (proposal.status == ProposalStatus::Voting) {
        if (currentHeight > proposal.votingEndHeight) {
            // Voting ended
            if (proposal.IsPassed()) {
                proposal.status = ProposalStatus::Approved;
            } else if (!proposal.HasQuorum()) {
                proposal.status = ProposalStatus::Expired;
            } else {
                proposal.status = ProposalStatus::Rejected;
            }
        }
    }
}

void Treasury::ExecuteApprovedProposals(int currentHeight) {
    for (auto& [id, proposal] : proposals_) {
        if (proposal.IsReadyForExecution(currentHeight)) {
            ExecuteSpending(id);
        }
    }
}

void Treasury::ProcessMilestones(int currentHeight) {
    for (auto& [id, proposal] : proposals_) {
        if (proposal.status != ProposalStatus::Executed) {
            continue;
        }
        
        for (auto& milestone : proposal.milestones) {
            if (!milestone.released && currentHeight >= milestone.releaseHeight) {
                // Release milestone funds
                // In production, this would create a transaction
                milestone.released = true;
            }
        }
    }
}

void Treasury::ProcessDeposits(int currentHeight) {
    // Refund deposits for approved/executed proposals
    // Forfeit deposits for rejected proposals
    (void)currentHeight;
}

// ============================================================================
// TreasuryOutputBuilder
// ============================================================================

std::pair<std::vector<Byte>, Amount> TreasuryOutputBuilder::BuildDepositOutput(
    const Hash160& treasuryAddress,
    Amount amount
) const {
    // Create P2PKH script
    std::vector<Byte> script;
    script.reserve(25);
    script.push_back(0x76); // OP_DUP
    script.push_back(0xa9); // OP_HASH160
    script.push_back(0x14); // Push 20 bytes
    script.insert(script.end(), treasuryAddress.begin(), treasuryAddress.end());
    script.push_back(0x88); // OP_EQUALVERIFY
    script.push_back(0xac); // OP_CHECKSIG
    
    return {script, amount};
}

std::vector<std::pair<std::vector<Byte>, Amount>> TreasuryOutputBuilder::BuildSpendingOutputs(
    const TreasuryProposal& proposal
) const {
    std::vector<std::pair<std::vector<Byte>, Amount>> outputs;
    
    // Main output to recipient
    std::vector<Byte> script;
    script.reserve(25);
    script.push_back(0x76);
    script.push_back(0xa9);
    script.push_back(0x14);
    script.insert(script.end(), proposal.recipient.begin(), proposal.recipient.end());
    script.push_back(0x88);
    script.push_back(0xac);
    
    outputs.emplace_back(script, proposal.requestedAmount);
    
    return outputs;
}

std::pair<std::vector<Byte>, Amount> TreasuryOutputBuilder::BuildRefundOutput(
    const TreasuryProposal& proposal
) const {
    // Refund deposit to proposer's address
    Hash160 proposerAddr = proposal.proposer.GetHash160();
    
    std::vector<Byte> script;
    script.reserve(25);
    script.push_back(0x76);
    script.push_back(0xa9);
    script.push_back(0x14);
    script.insert(script.end(), proposerAddr.begin(), proposerAddr.end());
    script.push_back(0x88);
    script.push_back(0xac);
    
    return {script, proposal.deposit};
}

// ============================================================================
// Utility Functions
// ============================================================================

Amount CalculateProposalDeposit(Amount proposalAmount) {
    // Deposit is 1% of proposal amount, minimum 100 NXS
    Amount deposit = proposalAmount / 100;
    return std::max(deposit, 100 * COIN);
}

bool ValidateProposal(const TreasuryProposal& proposal, Amount treasuryBalance) {
    // Check minimum amount
    if (proposal.requestedAmount < MIN_PROPOSAL_AMOUNT) {
        return false;
    }
    
    // Check maximum amount
    Amount maxAmount = (treasuryBalance * MAX_PROPOSAL_PERCENT) / 100;
    if (proposal.requestedAmount > maxAmount) {
        return false;
    }
    
    // Check title and description
    if (proposal.title.empty() || proposal.title.length() > 256) {
        return false;
    }
    
    if (proposal.description.length() > 10000) {
        return false;
    }
    
    return true;
}

uint64_t CalculateVotingPower(Amount stake) {
    // 1 voting power per 1000 NXS stake
    return stake / (1000 * COIN);
}

} // namespace economics
} // namespace shurium
