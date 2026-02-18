// SHURIUM - Governance Module Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/governance/governance.h>
#include <shurium/crypto/sha256.h>
#include <shurium/core/serialize.h>

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace shurium {
namespace governance {

// ============================================================================
// String Conversion Functions
// ============================================================================

const char* ProposalTypeToString(ProposalType type) {
    switch (type) {
        case ProposalType::Parameter: return "Parameter";
        case ProposalType::Protocol: return "Protocol";
        case ProposalType::Constitutional: return "Constitutional";
        case ProposalType::Emergency: return "Emergency";
        case ProposalType::Signal: return "Signal";
        default: return "Unknown";
    }
}

std::optional<ProposalType> ParseProposalType(const std::string& str) {
    if (str == "Parameter" || str == "parameter") return ProposalType::Parameter;
    if (str == "Protocol" || str == "protocol") return ProposalType::Protocol;
    if (str == "Constitutional" || str == "constitutional") return ProposalType::Constitutional;
    if (str == "Emergency" || str == "emergency") return ProposalType::Emergency;
    if (str == "Signal" || str == "signal") return ProposalType::Signal;
    return std::nullopt;
}

const char* GovernanceStatusToString(GovernanceStatus status) {
    switch (status) {
        case GovernanceStatus::Draft: return "Draft";
        case GovernanceStatus::Pending: return "Pending";
        case GovernanceStatus::Active: return "Active";
        case GovernanceStatus::Approved: return "Approved";
        case GovernanceStatus::Rejected: return "Rejected";
        case GovernanceStatus::QuorumFailed: return "QuorumFailed";
        case GovernanceStatus::Executed: return "Executed";
        case GovernanceStatus::ExecutionFailed: return "ExecutionFailed";
        case GovernanceStatus::Cancelled: return "Cancelled";
        case GovernanceStatus::Vetoed: return "Vetoed";
        case GovernanceStatus::Expired: return "Expired";
        default: return "Unknown";
    }
}

const char* VoteChoiceToString(VoteChoice choice) {
    switch (choice) {
        case VoteChoice::Yes: return "Yes";
        case VoteChoice::No: return "No";
        case VoteChoice::Abstain: return "Abstain";
        case VoteChoice::NoWithVeto: return "NoWithVeto";
        default: return "Unknown";
    }
}

const char* GovernableParameterToString(GovernableParameter param) {
    switch (param) {
        case GovernableParameter::TransactionFeeMultiplier: return "TransactionFeeMultiplier";
        case GovernableParameter::BlockSizeLimit: return "BlockSizeLimit";
        case GovernableParameter::MinTransactionFee: return "MinTransactionFee";
        case GovernableParameter::BlockRewardAdjustment: return "BlockRewardAdjustment";
        case GovernableParameter::UBIDistributionRate: return "UBIDistributionRate";
        case GovernableParameter::OracleMinStake: return "OracleMinStake";
        case GovernableParameter::OracleSlashingRate: return "OracleSlashingRate";
        case GovernableParameter::TreasuryAllocationDev: return "TreasuryAllocationDev";
        case GovernableParameter::TreasuryAllocationSecurity: return "TreasuryAllocationSecurity";
        case GovernableParameter::TreasuryAllocationMarketing: return "TreasuryAllocationMarketing";
        case GovernableParameter::StabilityFeeRate: return "StabilityFeeRate";
        case GovernableParameter::PriceDeviationThreshold: return "PriceDeviationThreshold";
        case GovernableParameter::ProposalDepositAmount: return "ProposalDepositAmount";
        case GovernableParameter::VotingPeriodBlocks: return "VotingPeriodBlocks";
        default: return "Unknown";
    }
}

std::optional<GovernableParameter> ParseGovernableParameter(const std::string& str) {
    if (str == "TransactionFeeMultiplier") return GovernableParameter::TransactionFeeMultiplier;
    if (str == "BlockSizeLimit") return GovernableParameter::BlockSizeLimit;
    if (str == "MinTransactionFee") return GovernableParameter::MinTransactionFee;
    if (str == "BlockRewardAdjustment") return GovernableParameter::BlockRewardAdjustment;
    if (str == "UBIDistributionRate") return GovernableParameter::UBIDistributionRate;
    if (str == "OracleMinStake") return GovernableParameter::OracleMinStake;
    if (str == "OracleSlashingRate") return GovernableParameter::OracleSlashingRate;
    if (str == "TreasuryAllocationDev") return GovernableParameter::TreasuryAllocationDev;
    if (str == "TreasuryAllocationSecurity") return GovernableParameter::TreasuryAllocationSecurity;
    if (str == "TreasuryAllocationMarketing") return GovernableParameter::TreasuryAllocationMarketing;
    if (str == "StabilityFeeRate") return GovernableParameter::StabilityFeeRate;
    if (str == "PriceDeviationThreshold") return GovernableParameter::PriceDeviationThreshold;
    if (str == "ProposalDepositAmount") return GovernableParameter::ProposalDepositAmount;
    if (str == "VotingPeriodBlocks") return GovernableParameter::VotingPeriodBlocks;
    return std::nullopt;
}

const char* ConstitutionalArticleToString(ConstitutionalArticle article) {
    switch (article) {
        case ConstitutionalArticle::GovernanceProcess: return "GovernanceProcess";
        case ConstitutionalArticle::EconomicPolicy: return "EconomicPolicy";
        case ConstitutionalArticle::PrivacyRights: return "PrivacyRights";
        case ConstitutionalArticle::SecurityRequirements: return "SecurityRequirements";
        case ConstitutionalArticle::UpgradeProcedures: return "UpgradeProcedures";
        case ConstitutionalArticle::EmergencyPowers: return "EmergencyPowers";
        case ConstitutionalArticle::FundamentalLimits: return "FundamentalLimits";
        default: return "Unknown";
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string FormatGovernanceAmount(Amount amount) {
    std::ostringstream ss;
    Amount whole = amount / COIN;
    Amount frac = amount % COIN;
    ss << whole;
    if (frac > 0) {
        ss << "." << std::setfill('0') << std::setw(8) << frac;
        // Trim trailing zeros
        std::string s = ss.str();
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
        return s + " NXS";
    }
    return ss.str() + " NXS";
}

uint64_t CalculateVotingPower(Amount stake) {
    if (stake < MIN_VOTING_STAKE) return 0;
    // Square root voting power to prevent plutocracy
    // 1 NXS = 1 base voting power, but diminishing returns
    double stakeInCoins = static_cast<double>(stake) / COIN;
    return static_cast<uint64_t>(std::sqrt(stakeInCoins) * 1000);
}

ParameterValue GetParameterDefault(GovernableParameter param) {
    switch (param) {
        case GovernableParameter::TransactionFeeMultiplier: return int64_t(100); // 100 = 1x
        case GovernableParameter::BlockSizeLimit: return int64_t(4 * 1024 * 1024); // 4MB
        case GovernableParameter::MinTransactionFee: return int64_t(1000); // 1000 base units
        case GovernableParameter::BlockRewardAdjustment: return int64_t(100); // 100 bps = 1%
        case GovernableParameter::UBIDistributionRate: return int64_t(1000); // 10%
        case GovernableParameter::OracleMinStake: return int64_t(10000 * COIN);
        case GovernableParameter::OracleSlashingRate: return int64_t(500); // 5%
        case GovernableParameter::TreasuryAllocationDev: return int64_t(30); // 30%
        case GovernableParameter::TreasuryAllocationSecurity: return int64_t(15); // 15%
        case GovernableParameter::TreasuryAllocationMarketing: return int64_t(10); // 10%
        case GovernableParameter::StabilityFeeRate: return int64_t(50); // 0.5%
        case GovernableParameter::PriceDeviationThreshold: return int64_t(500); // 5%
        case GovernableParameter::ProposalDepositAmount: return int64_t(1000 * COIN);
        case GovernableParameter::VotingPeriodBlocks: return int64_t(PARAMETER_VOTING_PERIOD);
        default: return int64_t(0);
    }
}

std::optional<int64_t> GetParameterMin(GovernableParameter param) {
    switch (param) {
        case GovernableParameter::TransactionFeeMultiplier: return 10; // 0.1x min
        case GovernableParameter::BlockSizeLimit: return 1024 * 1024; // 1MB min
        case GovernableParameter::MinTransactionFee: return 100;
        case GovernableParameter::BlockRewardAdjustment: return 0;
        case GovernableParameter::UBIDistributionRate: return 0;
        case GovernableParameter::OracleMinStake: return 1000 * COIN;
        case GovernableParameter::OracleSlashingRate: return 0;
        case GovernableParameter::TreasuryAllocationDev: return 0;
        case GovernableParameter::TreasuryAllocationSecurity: return 0;
        case GovernableParameter::TreasuryAllocationMarketing: return 0;
        case GovernableParameter::StabilityFeeRate: return 0;
        case GovernableParameter::PriceDeviationThreshold: return 100; // 1% min
        case GovernableParameter::ProposalDepositAmount: return 100 * COIN;
        case GovernableParameter::VotingPeriodBlocks: return 1440; // ~12 hours min
        default: return std::nullopt;
    }
}

std::optional<int64_t> GetParameterMax(GovernableParameter param) {
    switch (param) {
        case GovernableParameter::TransactionFeeMultiplier: return 10000; // 100x max
        case GovernableParameter::BlockSizeLimit: return 32 * 1024 * 1024; // 32MB max
        case GovernableParameter::MinTransactionFee: return 1000000;
        case GovernableParameter::BlockRewardAdjustment: return 1000; // 10% max
        case GovernableParameter::UBIDistributionRate: return 5000; // 50% max
        case GovernableParameter::OracleMinStake: return 1000000 * COIN;
        case GovernableParameter::OracleSlashingRate: return 5000; // 50% max
        case GovernableParameter::TreasuryAllocationDev: return 100;
        case GovernableParameter::TreasuryAllocationSecurity: return 100;
        case GovernableParameter::TreasuryAllocationMarketing: return 100;
        case GovernableParameter::StabilityFeeRate: return 1000; // 10% max
        case GovernableParameter::PriceDeviationThreshold: return 5000; // 50% max
        case GovernableParameter::ProposalDepositAmount: return 100000 * COIN;
        case GovernableParameter::VotingPeriodBlocks: return 172800; // ~60 days max
        default: return std::nullopt;
    }
}

bool ValidateParameterBounds(GovernableParameter param, const ParameterValue& value) {
    if (!std::holds_alternative<int64_t>(value)) {
        return true; // String values don't have bounds
    }
    
    int64_t intVal = std::get<int64_t>(value);
    auto minVal = GetParameterMin(param);
    auto maxVal = GetParameterMax(param);
    
    if (minVal && intVal < *minVal) return false;
    if (maxVal && intVal > *maxVal) return false;
    
    return true;
}

// ============================================================================
// ParameterChange Implementation
// ============================================================================

bool ParameterChange::IsValid() const {
    return ValidateParameterBounds(parameter, newValue);
}

std::string ParameterChange::ToString() const {
    std::ostringstream ss;
    ss << GovernableParameterToString(parameter) << ": ";
    
    if (std::holds_alternative<int64_t>(currentValue)) {
        ss << std::get<int64_t>(currentValue);
    } else {
        ss << std::get<std::string>(currentValue);
    }
    
    ss << " -> ";
    
    if (std::holds_alternative<int64_t>(newValue)) {
        ss << std::get<int64_t>(newValue);
    } else {
        ss << std::get<std::string>(newValue);
    }
    
    return ss.str();
}

// ============================================================================
// ProtocolUpgrade Implementation
// ============================================================================

bool ProtocolUpgrade::IsBackwardCompatible() const {
    // Backward compatible if no features are deprecated
    return deprecatedFeatures == 0;
}

std::string ProtocolUpgrade::FormatVersion(uint32_t version) {
    uint32_t major = (version >> 16) & 0xFF;
    uint32_t minor = (version >> 8) & 0xFF;
    uint32_t patch = version & 0xFF;
    
    std::ostringstream ss;
    ss << major << "." << minor << "." << patch;
    return ss.str();
}

std::optional<uint32_t> ProtocolUpgrade::ParseVersion(const std::string& str) {
    int major, minor, patch;
    if (sscanf(str.c_str(), "%d.%d.%d", &major, &minor, &patch) != 3) {
        return std::nullopt;
    }
    if (major < 0 || major > 255 || minor < 0 || minor > 255 || patch < 0 || patch > 255) {
        return std::nullopt;
    }
    return (static_cast<uint32_t>(major) << 16) | 
           (static_cast<uint32_t>(minor) << 8) | 
           static_cast<uint32_t>(patch);
}

std::string ProtocolUpgrade::ToString() const {
    std::ostringstream ss;
    ss << "ProtocolUpgrade { version: " << FormatVersion(newVersion)
       << ", minClient: " << FormatVersion(minClientVersion)
       << ", activation: " << activationHeight
       << ", deadline: " << deadlineHeight
       << " }";
    return ss.str();
}

// ============================================================================
// ConstitutionalChange Implementation
// ============================================================================

Hash256 ConstitutionalChange::GetHash() const {
    std::string data = std::to_string(static_cast<int>(article)) + 
                       currentText + newText + rationale;
    
    SHA256 hasher;
    hasher.Write(reinterpret_cast<const Byte*>(data.data()), data.size());
    
    std::array<Byte, 32> result;
    hasher.Finalize(result.data());
    return Hash256(result);
}

std::string ConstitutionalChange::ToString() const {
    std::ostringstream ss;
    ss << "ConstitutionalChange { article: " << ConstitutionalArticleToString(article)
       << ", rationale: " << rationale.substr(0, 50) << "... }";
    return ss.str();
}


// ============================================================================
// Vote Implementation
// ============================================================================

Hash256 Vote::GetHash() const {
    std::ostringstream ss;
    ss << proposalId.ToHex() << voter.ToHex() << static_cast<int>(choice)
       << votingPower << voteHeight << reason;
    
    std::string data = ss.str();
    SHA256 hasher;
    hasher.Write(reinterpret_cast<const Byte*>(data.data()), data.size());
    
    std::array<Byte, 32> result;
    hasher.Finalize(result.data());
    return Hash256(result);
}

bool Vote::VerifySignature(const PublicKey& pubKey) const {
    if (signature.empty()) return false;
    
    Hash256 hash = GetHash();
    return pubKey.Verify(hash, signature);
}

bool Vote::Sign(const std::vector<Byte>& privateKey) {
    if (privateKey.size() != 32) return false;
    
    Hash256 hash = GetHash();
    
    // Use the crypto signing function
    PrivateKey privKey(privateKey.data(), privateKey.size());
    auto sig = privKey.Sign(hash);
    if (sig.empty()) return false;
    
    signature = sig;
    return true;
}

std::string Vote::ToString() const {
    std::ostringstream ss;
    ss << "Vote { proposal: " << proposalId.ToHex().substr(0, 16) << "..."
       << ", voter: " << voter.ToHex().substr(0, 12) << "..."
       << ", choice: " << VoteChoiceToString(choice)
       << ", power: " << votingPower
       << " }";
    return ss.str();
}

// ============================================================================
// Delegation Implementation
// ============================================================================

Hash256 Delegation::GetHash() const {
    std::ostringstream ss;
    ss << delegator.ToHex() << delegate.ToHex();
    if (scope) ss << static_cast<int>(*scope);
    ss << expirationHeight << creationHeight;
    
    std::string data = ss.str();
    SHA256 hasher;
    hasher.Write(reinterpret_cast<const Byte*>(data.data()), data.size());
    
    std::array<Byte, 32> result;
    hasher.Finalize(result.data());
    return Hash256(result);
}

bool Delegation::IsValidAt(int height) const {
    if (!isActive) return false;
    if (expirationHeight > 0 && height >= expirationHeight) return false;
    return height >= creationHeight;
}

std::string Delegation::ToString() const {
    std::ostringstream ss;
    ss << "Delegation { from: " << delegator.ToHex().substr(0, 12) << "..."
       << ", to: " << delegate.ToHex().substr(0, 12) << "..."
       << ", active: " << (isActive ? "yes" : "no")
       << " }";
    return ss.str();
}

// ============================================================================
// GovernanceProposal Implementation
// ============================================================================

Hash256 GovernanceProposal::CalculateHash() const {
    std::ostringstream ss;
    ss << static_cast<int>(type) << title << description
       << proposer.ToHex() << deposit << submitHeight;
    
    std::string data = ss.str();
    SHA256 hasher;
    hasher.Write(reinterpret_cast<const Byte*>(data.data()), data.size());
    
    std::array<Byte, 32> result;
    hasher.Finalize(result.data());
    return Hash256(result);
}

int GovernanceProposal::GetVotingPeriod() const {
    switch (type) {
        case ProposalType::Parameter: return PARAMETER_VOTING_PERIOD;
        case ProposalType::Protocol: return PROTOCOL_VOTING_PERIOD;
        case ProposalType::Constitutional: return CONSTITUTIONAL_VOTING_PERIOD;
        case ProposalType::Emergency: return PARAMETER_VOTING_PERIOD; // Fast track
        case ProposalType::Signal: return PARAMETER_VOTING_PERIOD;
        default: return PARAMETER_VOTING_PERIOD;
    }
}

int GovernanceProposal::GetExecutionDelay() const {
    switch (type) {
        case ProposalType::Parameter: return PARAMETER_EXECUTION_DELAY;
        case ProposalType::Protocol: return PROTOCOL_EXECUTION_DELAY;
        case ProposalType::Constitutional: return CONSTITUTIONAL_EXECUTION_DELAY;
        case ProposalType::Emergency: return 0; // Immediate
        case ProposalType::Signal: return 0; // No execution
        default: return PARAMETER_EXECUTION_DELAY;
    }
}

int GovernanceProposal::GetApprovalThreshold() const {
    switch (type) {
        case ProposalType::Parameter: return PARAMETER_APPROVAL_THRESHOLD;
        case ProposalType::Protocol: return PROTOCOL_APPROVAL_THRESHOLD;
        case ProposalType::Constitutional: return CONSTITUTIONAL_APPROVAL_THRESHOLD;
        case ProposalType::Emergency: return PROTOCOL_APPROVAL_THRESHOLD;
        case ProposalType::Signal: return PARAMETER_APPROVAL_THRESHOLD;
        default: return PARAMETER_APPROVAL_THRESHOLD;
    }
}

int GovernanceProposal::GetQuorumRequirement() const {
    switch (type) {
        case ProposalType::Parameter: return PARAMETER_QUORUM;
        case ProposalType::Protocol: return PROTOCOL_QUORUM;
        case ProposalType::Constitutional: return CONSTITUTIONAL_QUORUM;
        case ProposalType::Emergency: return PROTOCOL_QUORUM;
        case ProposalType::Signal: return PARAMETER_QUORUM;
        default: return PARAMETER_QUORUM;
    }
}

double GovernanceProposal::GetApprovalPercent() const {
    uint64_t totalVoted = votesYes + votesNo + votesNoWithVeto;
    if (totalVoted == 0) return 0.0;
    return (static_cast<double>(votesYes) / totalVoted) * 100.0;
}

double GovernanceProposal::GetParticipationPercent() const {
    if (totalVotingPower == 0) return 0.0;
    uint64_t totalVoted = votesYes + votesNo + votesAbstain + votesNoWithVeto;
    return (static_cast<double>(totalVoted) / totalVotingPower) * 100.0;
}

bool GovernanceProposal::HasQuorum() const {
    return GetParticipationPercent() >= GetQuorumRequirement();
}

bool GovernanceProposal::HasApproval() const {
    return GetApprovalPercent() >= GetApprovalThreshold();
}

bool GovernanceProposal::IsVetoed() const {
    uint64_t totalVoted = votesYes + votesNo + votesNoWithVeto;
    if (totalVoted == 0) return false;
    // Vetoed if more than 33% vote NoWithVeto
    return (static_cast<double>(votesNoWithVeto) / totalVoted) * 100.0 > 33.0;
}

bool GovernanceProposal::IsVotingActive(int currentHeight) const {
    return status == GovernanceStatus::Active &&
           currentHeight >= votingStartHeight &&
           currentHeight < votingEndHeight;
}

bool GovernanceProposal::IsReadyForExecution(int currentHeight) const {
    return status == GovernanceStatus::Approved &&
           currentHeight >= executionHeight;
}

uint64_t GovernanceProposal::GetTotalVotes() const {
    return votesYes + votesNo + votesAbstain + votesNoWithVeto;
}

std::vector<Byte> GovernanceProposal::Serialize() const {
    DataStream ss;
    
    // Serialize proposal ID (32 bytes)
    ss.Write(id.data(), id.size());
    
    // Serialize type (1 byte)
    ser_writedata8(ss, static_cast<uint8_t>(type));
    
    // Serialize title (compact size + string)
    WriteCompactSize(ss, title.size());
    ss.Write(reinterpret_cast<const uint8_t*>(title.data()), title.size());
    
    // Serialize description (compact size + string)
    WriteCompactSize(ss, description.size());
    ss.Write(reinterpret_cast<const uint8_t*>(description.data()), description.size());
    
    // Serialize proposer public key
    auto pubkeyVec = proposer.ToVector();
    WriteCompactSize(ss, pubkeyVec.size());
    ss.Write(pubkeyVec.data(), pubkeyVec.size());
    
    // Serialize deposit (8 bytes)
    ser_writedata64(ss, static_cast<uint64_t>(deposit));
    
    // Serialize status (1 byte)
    ser_writedata8(ss, static_cast<uint8_t>(status));
    
    // Serialize heights (4 bytes each, signed as unsigned)
    ser_writedata32(ss, static_cast<uint32_t>(submitHeight));
    ser_writedata32(ss, static_cast<uint32_t>(votingStartHeight));
    ser_writedata32(ss, static_cast<uint32_t>(votingEndHeight));
    ser_writedata32(ss, static_cast<uint32_t>(executionHeight));
    
    // Serialize votes (8 bytes each)
    ser_writedata64(ss, votesYes);
    ser_writedata64(ss, votesNo);
    ser_writedata64(ss, votesAbstain);
    ser_writedata64(ss, votesNoWithVeto);
    
    // Serialize total voting power (8 bytes)
    ser_writedata64(ss, totalVotingPower);
    
    // Serialize discussion URL
    WriteCompactSize(ss, discussionUrl.size());
    ss.Write(reinterpret_cast<const uint8_t*>(discussionUrl.data()), discussionUrl.size());
    
    // Serialize payload variant
    ser_writedata8(ss, static_cast<uint8_t>(payload.index()));
    
    if (std::holds_alternative<std::vector<ParameterChange>>(payload)) {
        const auto& changes = std::get<std::vector<ParameterChange>>(payload);
        WriteCompactSize(ss, changes.size());
        for (const auto& change : changes) {
            ser_writedata32(ss, static_cast<uint32_t>(change.parameter));
            
            // Serialize current value (variant<int64_t, string>)
            ser_writedata8(ss, static_cast<uint8_t>(change.currentValue.index()));
            if (change.currentValue.index() == 0) {
                ser_writedata64(ss, static_cast<uint64_t>(std::get<int64_t>(change.currentValue)));
            } else {
                const auto& str = std::get<std::string>(change.currentValue);
                WriteCompactSize(ss, str.size());
                ss.Write(reinterpret_cast<const uint8_t*>(str.data()), str.size());
            }
            
            // Serialize new value
            ser_writedata8(ss, static_cast<uint8_t>(change.newValue.index()));
            if (change.newValue.index() == 0) {
                ser_writedata64(ss, static_cast<uint64_t>(std::get<int64_t>(change.newValue)));
            } else {
                const auto& str = std::get<std::string>(change.newValue);
                WriteCompactSize(ss, str.size());
                ss.Write(reinterpret_cast<const uint8_t*>(str.data()), str.size());
            }
        }
    } else if (std::holds_alternative<ProtocolUpgrade>(payload)) {
        const auto& upgrade = std::get<ProtocolUpgrade>(payload);
        ser_writedata32(ss, upgrade.newVersion);
        ser_writedata32(ss, upgrade.minClientVersion);
        ser_writedata32(ss, upgrade.activatedFeatures);
        ser_writedata32(ss, upgrade.deprecatedFeatures);
        ser_writedata32(ss, static_cast<uint32_t>(upgrade.activationHeight));
        ser_writedata32(ss, static_cast<uint32_t>(upgrade.deadlineHeight));
        WriteCompactSize(ss, upgrade.codeReference.size());
        ss.Write(reinterpret_cast<const uint8_t*>(upgrade.codeReference.data()), upgrade.codeReference.size());
        WriteCompactSize(ss, upgrade.changelogUrl.size());
        ss.Write(reinterpret_cast<const uint8_t*>(upgrade.changelogUrl.data()), upgrade.changelogUrl.size());
    } else if (std::holds_alternative<ConstitutionalChange>(payload)) {
        const auto& change = std::get<ConstitutionalChange>(payload);
        ser_writedata8(ss, static_cast<uint8_t>(change.article));
        WriteCompactSize(ss, change.currentText.size());
        ss.Write(reinterpret_cast<const uint8_t*>(change.currentText.data()), change.currentText.size());
        WriteCompactSize(ss, change.newText.size());
        ss.Write(reinterpret_cast<const uint8_t*>(change.newText.data()), change.newText.size());
        WriteCompactSize(ss, change.rationale.size());
        ss.Write(reinterpret_cast<const uint8_t*>(change.rationale.data()), change.rationale.size());
    } else {
        // Signal/Emergency - just text
        const auto& text = std::get<std::string>(payload);
        WriteCompactSize(ss, text.size());
        ss.Write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    }
    
    return std::vector<Byte>(ss.begin(), ss.end());
}

std::optional<GovernanceProposal> GovernanceProposal::Deserialize(const Byte* data, size_t len) {
    if (!data || len == 0) {
        return std::nullopt;
    }
    
    try {
        DataStream ss(data, len);
        GovernanceProposal proposal;
        
        // Deserialize proposal ID (32 bytes)
        if (ss.size() < 32) return std::nullopt;
        ss.Read(proposal.id.data(), proposal.id.size());
        
        // Deserialize type (1 byte)
        proposal.type = static_cast<ProposalType>(ser_readdata8(ss));
        
        // Deserialize title
        uint64_t titleLen = ReadCompactSize(ss);
        if (titleLen > MAX_SIZE) return std::nullopt;
        proposal.title.resize(titleLen);
        if (titleLen > 0) {
            ss.Read(reinterpret_cast<uint8_t*>(proposal.title.data()), titleLen);
        }
        
        // Deserialize description
        uint64_t descLen = ReadCompactSize(ss);
        if (descLen > MAX_SIZE) return std::nullopt;
        proposal.description.resize(descLen);
        if (descLen > 0) {
            ss.Read(reinterpret_cast<uint8_t*>(proposal.description.data()), descLen);
        }
        
        // Deserialize proposer public key
        uint64_t pubkeyLen = ReadCompactSize(ss);
        if (pubkeyLen > 65) return std::nullopt;  // Max pubkey size
        std::vector<uint8_t> pubkeyData(pubkeyLen);
        if (pubkeyLen > 0) {
            ss.Read(pubkeyData.data(), pubkeyLen);
            proposal.proposer = PublicKey(pubkeyData);
        }
        
        // Deserialize deposit
        proposal.deposit = static_cast<Amount>(ser_readdata64(ss));
        
        // Deserialize status
        proposal.status = static_cast<GovernanceStatus>(ser_readdata8(ss));
        
        // Deserialize heights
        proposal.submitHeight = static_cast<int>(ser_readdata32(ss));
        proposal.votingStartHeight = static_cast<int>(ser_readdata32(ss));
        proposal.votingEndHeight = static_cast<int>(ser_readdata32(ss));
        proposal.executionHeight = static_cast<int>(ser_readdata32(ss));
        
        // Deserialize votes
        proposal.votesYes = ser_readdata64(ss);
        proposal.votesNo = ser_readdata64(ss);
        proposal.votesAbstain = ser_readdata64(ss);
        proposal.votesNoWithVeto = ser_readdata64(ss);
        
        // Deserialize total voting power
        proposal.totalVotingPower = ser_readdata64(ss);
        
        // Deserialize discussion URL
        uint64_t urlLen = ReadCompactSize(ss);
        if (urlLen > MAX_SIZE) return std::nullopt;
        proposal.discussionUrl.resize(urlLen);
        if (urlLen > 0) {
            ss.Read(reinterpret_cast<uint8_t*>(proposal.discussionUrl.data()), urlLen);
        }
        
        // Deserialize payload variant
        uint8_t payloadType = ser_readdata8(ss);
        
        switch (payloadType) {
            case 0: {
                // Parameter changes
                uint64_t numChanges = ReadCompactSize(ss);
                std::vector<ParameterChange> changes;
                for (uint64_t i = 0; i < numChanges; ++i) {
                    ParameterChange change;
                    change.parameter = static_cast<GovernableParameter>(ser_readdata32(ss));
                    
                    // Read current value (variant<int64_t, string>)
                    uint8_t cvType = ser_readdata8(ss);
                    if (cvType == 0) {
                        change.currentValue = static_cast<int64_t>(ser_readdata64(ss));
                    } else {
                        uint64_t cvLen = ReadCompactSize(ss);
                        std::string cvStr(cvLen, '\0');
                        if (cvLen > 0) ss.Read(reinterpret_cast<uint8_t*>(cvStr.data()), cvLen);
                        change.currentValue = cvStr;
                    }
                    
                    // Read new value
                    uint8_t nvType = ser_readdata8(ss);
                    if (nvType == 0) {
                        change.newValue = static_cast<int64_t>(ser_readdata64(ss));
                    } else {
                        uint64_t nvLen = ReadCompactSize(ss);
                        std::string nvStr(nvLen, '\0');
                        if (nvLen > 0) ss.Read(reinterpret_cast<uint8_t*>(nvStr.data()), nvLen);
                        change.newValue = nvStr;
                    }
                    
                    changes.push_back(change);
                }
                proposal.payload = changes;
                break;
            }
            case 1: {
                // Protocol upgrade
                ProtocolUpgrade upgrade;
                upgrade.newVersion = ser_readdata32(ss);
                upgrade.minClientVersion = ser_readdata32(ss);
                upgrade.activatedFeatures = ser_readdata32(ss);
                upgrade.deprecatedFeatures = ser_readdata32(ss);
                upgrade.activationHeight = static_cast<int>(ser_readdata32(ss));
                upgrade.deadlineHeight = static_cast<int>(ser_readdata32(ss));
                
                uint64_t codeRefLen = ReadCompactSize(ss);
                upgrade.codeReference.resize(codeRefLen);
                if (codeRefLen > 0) ss.Read(reinterpret_cast<uint8_t*>(upgrade.codeReference.data()), codeRefLen);
                
                uint64_t changelogLen = ReadCompactSize(ss);
                upgrade.changelogUrl.resize(changelogLen);
                if (changelogLen > 0) ss.Read(reinterpret_cast<uint8_t*>(upgrade.changelogUrl.data()), changelogLen);
                
                proposal.payload = upgrade;
                break;
            }
            case 2: {
                // Constitutional change
                ConstitutionalChange change;
                change.article = static_cast<ConstitutionalArticle>(ser_readdata8(ss));
                
                uint64_t ctLen = ReadCompactSize(ss);
                change.currentText.resize(ctLen);
                if (ctLen > 0) ss.Read(reinterpret_cast<uint8_t*>(change.currentText.data()), ctLen);
                
                uint64_t ntLen = ReadCompactSize(ss);
                change.newText.resize(ntLen);
                if (ntLen > 0) ss.Read(reinterpret_cast<uint8_t*>(change.newText.data()), ntLen);
                
                uint64_t ratLen = ReadCompactSize(ss);
                change.rationale.resize(ratLen);
                if (ratLen > 0) ss.Read(reinterpret_cast<uint8_t*>(change.rationale.data()), ratLen);
                
                proposal.payload = change;
                break;
            }
            default: {
                // Signal/Emergency - text
                uint64_t textLen = ReadCompactSize(ss);
                std::string text(textLen, '\0');
                if (textLen > 0) ss.Read(reinterpret_cast<uint8_t*>(text.data()), textLen);
                proposal.payload = text;
                break;
            }
        }
        
        return proposal;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string GovernanceProposal::ToString() const {
    std::ostringstream ss;
    ss << "GovernanceProposal {"
       << " id: " << id.ToHex().substr(0, 16) << "..."
       << ", type: " << ProposalTypeToString(type)
       << ", title: " << title.substr(0, 30) << (title.length() > 30 ? "..." : "")
       << ", status: " << GovernanceStatusToString(status)
       << ", approval: " << std::fixed << std::setprecision(1) << GetApprovalPercent() << "%"
       << ", quorum: " << std::fixed << std::setprecision(1) << GetParticipationPercent() << "%"
       << " }";
    return ss.str();
}


// ============================================================================
// VotingPowerTracker Implementation
// ============================================================================

VotingPowerTracker::VotingPowerTracker() = default;
VotingPowerTracker::~VotingPowerTracker() = default;

void VotingPowerTracker::UpdateVotingPower(const VoterId& voter, uint64_t power) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = votingPower_.find(voter);
    if (it != votingPower_.end()) {
        totalPower_ -= it->second;
        if (power == 0) {
            votingPower_.erase(it);
        } else {
            it->second = power;
            totalPower_ += power;
        }
    } else if (power > 0) {
        votingPower_[voter] = power;
        totalPower_ += power;
    }
}

uint64_t VotingPowerTracker::GetVotingPower(const VoterId& voter) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = votingPower_.find(voter);
    return it != votingPower_.end() ? it->second : 0;
}

uint64_t VotingPowerTracker::GetTotalVotingPower() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalPower_;
}

std::map<VoterId, uint64_t> VotingPowerTracker::TakeSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return votingPower_;
}

size_t VotingPowerTracker::GetVoterCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return votingPower_.size();
}

void VotingPowerTracker::RemoveVoter(const VoterId& voter) {
    UpdateVotingPower(voter, 0);
}

void VotingPowerTracker::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    votingPower_.clear();
    totalPower_ = 0;
}

std::vector<Byte> VotingPowerTracker::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool VotingPowerTracker::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

// ============================================================================
// DelegationRegistry Implementation
// ============================================================================

DelegationRegistry::DelegationRegistry() = default;
DelegationRegistry::~DelegationRegistry() = default;

bool DelegationRegistry::AddDelegation(const Delegation& delegation) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check for cycles
    if (HasCycle(delegation.delegator, delegation.delegate)) {
        return false;
    }
    
    // Check delegation depth
    if (GetDelegationDepth(delegation.delegate) >= MAX_DELEGATION_DEPTH - 1) {
        return false;
    }
    
    // Remove existing delegation if any
    auto existingIt = delegations_.find(delegation.delegator);
    if (existingIt != delegations_.end()) {
        auto& oldDelegate = existingIt->second.delegate;
        reverseLookup_[oldDelegate].erase(delegation.delegator);
    }
    
    delegations_[delegation.delegator] = delegation;
    reverseLookup_[delegation.delegate].insert(delegation.delegator);
    
    return true;
}

bool DelegationRegistry::RemoveDelegation(const VoterId& delegator) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = delegations_.find(delegator);
    if (it == delegations_.end()) {
        return false;
    }
    
    auto& delegate = it->second.delegate;
    reverseLookup_[delegate].erase(delegator);
    delegations_.erase(it);
    
    return true;
}

std::optional<Delegation> DelegationRegistry::GetDelegation(const VoterId& delegator) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = delegations_.find(delegator);
    if (it == delegations_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<VoterId> DelegationRegistry::GetDelegators(const VoterId& delegate) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = reverseLookup_.find(delegate);
    if (it == reverseLookup_.end()) {
        return {};
    }
    
    return std::vector<VoterId>(it->second.begin(), it->second.end());
}

uint64_t DelegationRegistry::GetEffectiveVotingPower(
    const VoterId& voter,
    const VotingPowerTracker& tracker,
    ProposalType proposalType,
    int currentHeight) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t power = tracker.GetVotingPower(voter);
    
    // Add delegated power
    auto it = reverseLookup_.find(voter);
    if (it != reverseLookup_.end()) {
        for (const auto& delegator : it->second) {
            auto delIt = delegations_.find(delegator);
            if (delIt != delegations_.end()) {
                const auto& delegation = delIt->second;
                if (delegation.IsValidAt(currentHeight)) {
                    // Check scope
                    if (!delegation.scope || *delegation.scope == proposalType) {
                        power += tracker.GetVotingPower(delegator);
                    }
                }
            }
        }
    }
    
    return power;
}

bool DelegationRegistry::HasCycle(const VoterId& delegator, const VoterId& delegate) const {
    // Check if delegating to this delegate would create a cycle
    VoterId current = delegate;
    int depth = 0;
    
    while (depth < MAX_DELEGATION_DEPTH + 1) {
        if (current == delegator) {
            return true; // Cycle detected
        }
        
        auto it = delegations_.find(current);
        if (it == delegations_.end()) {
            break; // End of chain
        }
        
        current = it->second.delegate;
        depth++;
    }
    
    return false;
}

int DelegationRegistry::GetDelegationDepth(const VoterId& voter) const {
    VoterId current = voter;
    int depth = 0;
    
    while (depth < MAX_DELEGATION_DEPTH + 1) {
        auto it = delegations_.find(current);
        if (it == delegations_.end()) {
            break;
        }
        current = it->second.delegate;
        depth++;
    }
    
    return depth;
}

void DelegationRegistry::ExpireDelegations(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<VoterId> toRemove;
    for (auto& [delegator, delegation] : delegations_) {
        if (delegation.expirationHeight > 0 && height >= delegation.expirationHeight) {
            delegation.isActive = false;
            toRemove.push_back(delegator);
        }
    }
    
    for (const auto& delegator : toRemove) {
        auto it = delegations_.find(delegator);
        if (it != delegations_.end()) {
            reverseLookup_[it->second.delegate].erase(delegator);
            delegations_.erase(it);
        }
    }
}

size_t DelegationRegistry::GetActiveDelegationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return delegations_.size();
}

void DelegationRegistry::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    delegations_.clear();
    reverseLookup_.clear();
}

std::vector<Byte> DelegationRegistry::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool DelegationRegistry::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}


// ============================================================================
// ParameterRegistry Implementation
// ============================================================================

ParameterRegistry::ParameterRegistry() {
    InitializeDefaults();
}

ParameterRegistry::~ParameterRegistry() = default;

void ParameterRegistry::InitializeDefaults() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (int i = 0; i < static_cast<int>(GovernableParameter::MaxParameterCount); ++i) {
        auto param = static_cast<GovernableParameter>(i);
        parameters_[param] = GetParameterDefault(param);
    }
}

ParameterValue ParameterRegistry::GetParameter(GovernableParameter param) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = parameters_.find(param);
    if (it != parameters_.end()) {
        return it->second;
    }
    return GetParameterDefault(param);
}

int64_t ParameterRegistry::GetParameterInt(GovernableParameter param) const {
    auto value = GetParameter(param);
    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value);
    }
    return 0;
}

std::string ParameterRegistry::GetParameterString(GovernableParameter param) const {
    auto value = GetParameter(param);
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    return "";
}

bool ParameterRegistry::SetParameter(GovernableParameter param, const ParameterValue& value) {
    if (!ValidateParameterBounds(param, value)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    parameters_[param] = value;
    return true;
}

bool ParameterRegistry::ApplyChanges(const std::vector<ParameterChange>& changes) {
    // Validate all changes first
    for (const auto& change : changes) {
        if (!ValidateChange(change)) {
            return false;
        }
    }
    
    // Apply all changes
    for (const auto& change : changes) {
        SetParameter(change.parameter, change.newValue);
    }
    
    return true;
}

bool ParameterRegistry::ValidateChange(const ParameterChange& change) const {
    return change.IsValid();
}

std::map<GovernableParameter, ParameterValue> ParameterRegistry::GetAllParameters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return parameters_;
}

std::vector<Byte> ParameterRegistry::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool ParameterRegistry::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

// ============================================================================
// GuardianRegistry Implementation
// ============================================================================

GuardianRegistry::GuardianRegistry() = default;
GuardianRegistry::~GuardianRegistry() = default;

bool GuardianRegistry::AddGuardian(const Guardian& guardian) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (guardians_.count(guardian.id) > 0) {
        return false; // Already exists
    }
    
    guardians_[guardian.id] = guardian;
    return true;
}

bool GuardianRegistry::RemoveGuardian(const VoterId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return guardians_.erase(id) > 0;
}

std::optional<Guardian> GuardianRegistry::GetGuardian(const VoterId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = guardians_.find(id);
    if (it == guardians_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool GuardianRegistry::IsVetoed(const GovernanceProposalId& proposalId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetVetoCount(proposalId) >= GetRequiredVetoCount();
}

bool GuardianRegistry::RecordVeto(const VoterId& guardianId, const GovernanceProposalId& proposalId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = guardians_.find(guardianId);
    if (it == guardians_.end() || !it->second.isActive) {
        return false;
    }
    
    if (it->second.vetosUsed >= Guardian::MAX_VETOS_PER_PERIOD) {
        return false; // Exceeded veto limit
    }
    
    proposalVetoes_[proposalId].insert(guardianId);
    it->second.vetosUsed++;
    
    return true;
}

int GuardianRegistry::GetVetoCount(const GovernanceProposalId& proposalId) const {
    auto it = proposalVetoes_.find(proposalId);
    if (it == proposalVetoes_.end()) {
        return 0;
    }
    return static_cast<int>(it->second.size());
}

int GuardianRegistry::GetRequiredVetoCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Majority of guardians required to veto
    size_t activeCount = 0;
    for (const auto& [id, guardian] : guardians_) {
        if (guardian.isActive) activeCount++;
    }
    return static_cast<int>((activeCount / 2) + 1);
}

size_t GuardianRegistry::GetActiveGuardianCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [id, guardian] : guardians_) {
        if (guardian.isActive) count++;
    }
    return count;
}

void GuardianRegistry::ResetVetoCounts() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, guardian] : guardians_) {
        guardian.vetosUsed = 0;
    }
    proposalVetoes_.clear();
}


// ============================================================================
// GovernanceEngine Implementation
// ============================================================================

GovernanceEngine::GovernanceEngine()
    : params_(std::make_shared<ParameterRegistry>()) {}

GovernanceEngine::GovernanceEngine(std::shared_ptr<ParameterRegistry> params)
    : params_(params ? params : std::make_shared<ParameterRegistry>()) {}

GovernanceEngine::~GovernanceEngine() = default;

std::optional<GovernanceProposalId> GovernanceEngine::SubmitProposal(
    const GovernanceProposal& proposal,
    const std::vector<Byte>& signature) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate proposal
    if (!ValidateProposal(proposal)) {
        return std::nullopt;
    }
    
    // Verify signature
    if (!VerifyProposalSignature(proposal, signature)) {
        return std::nullopt;
    }
    
    // Check proposer's active proposal count
    int activeCount = 0;
    for (const auto& [id, p] : proposals_) {
        if (p.proposer == proposal.proposer &&
            (p.status == GovernanceStatus::Pending || 
             p.status == GovernanceStatus::Active)) {
            activeCount++;
        }
    }
    
    if (activeCount >= MAX_ACTIVE_PROPOSALS_PER_USER) {
        return std::nullopt;
    }
    
    // Create proposal with calculated ID
    GovernanceProposal newProposal = proposal;
    newProposal.id = proposal.CalculateHash();
    newProposal.status = GovernanceStatus::Pending;
    newProposal.submitHeight = currentHeight_;
    newProposal.votingStartHeight = currentHeight_ + 1; // Start voting next block
    newProposal.votingEndHeight = newProposal.votingStartHeight + newProposal.GetVotingPeriod();
    
    proposals_[newProposal.id] = newProposal;
    
    return newProposal.id;
}

std::optional<GovernanceProposal> GovernanceEngine::GetProposal(const GovernanceProposalId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = proposals_.find(id);
    if (it == proposals_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<GovernanceProposal> GovernanceEngine::GetProposalsByStatus(GovernanceStatus status) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<GovernanceProposal> result;
    for (const auto& [id, proposal] : proposals_) {
        if (proposal.status == status) {
            result.push_back(proposal);
        }
    }
    return result;
}

std::vector<GovernanceProposal> GovernanceEngine::GetProposalsByProposer(const PublicKey& proposer) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<GovernanceProposal> result;
    for (const auto& [id, proposal] : proposals_) {
        if (proposal.proposer == proposer) {
            result.push_back(proposal);
        }
    }
    return result;
}

size_t GovernanceEngine::GetActiveProposalCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [id, proposal] : proposals_) {
        if (proposal.status == GovernanceStatus::Active ||
            proposal.status == GovernanceStatus::Pending) {
            count++;
        }
    }
    return count;
}

size_t GovernanceEngine::GetTotalProposalCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return proposals_.size();
}

bool GovernanceEngine::CancelProposal(const GovernanceProposalId& id, const std::vector<Byte>& signature) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = proposals_.find(id);
    if (it == proposals_.end()) {
        return false;
    }
    
    auto& proposal = it->second;
    
    // Can only cancel pending proposals
    if (proposal.status != GovernanceStatus::Pending) {
        return false;
    }
    
    // Verify signature matches proposer
    if (!proposal.proposer.Verify(proposal.id, signature)) {
        return false;
    }
    
    proposal.status = GovernanceStatus::Cancelled;
    return true;
}

bool GovernanceEngine::CastVote(const Vote& vote) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = proposals_.find(vote.proposalId);
    if (it == proposals_.end()) {
        return false;
    }
    
    auto& proposal = it->second;
    
    // Check voting is active
    if (!proposal.IsVotingActive(currentHeight_)) {
        return false;
    }
    
    // Check if already voted
    if (votes_[vote.proposalId].count(vote.voter) > 0) {
        return false;
    }
    
    // Verify vote signature
    // Note: In production, would verify against voter's public key
    if (vote.signature.empty()) {
        return false;
    }
    
    // Record vote
    votes_[vote.proposalId][vote.voter] = vote;
    
    // Update proposal vote counts
    switch (vote.choice) {
        case VoteChoice::Yes:
            proposal.votesYes += vote.votingPower;
            break;
        case VoteChoice::No:
            proposal.votesNo += vote.votingPower;
            break;
        case VoteChoice::Abstain:
            proposal.votesAbstain += vote.votingPower;
            break;
        case VoteChoice::NoWithVeto:
            proposal.votesNoWithVeto += vote.votingPower;
            break;
    }
    
    return true;
}

bool GovernanceEngine::ChangeVote(const Vote& newVote) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto propIt = proposals_.find(newVote.proposalId);
    if (propIt == proposals_.end()) {
        return false;
    }
    
    auto& proposal = propIt->second;
    
    // Check voting is active
    if (!proposal.IsVotingActive(currentHeight_)) {
        return false;
    }
    
    // Check if has previous vote
    auto& proposalVotes = votes_[newVote.proposalId];
    auto voteIt = proposalVotes.find(newVote.voter);
    if (voteIt == proposalVotes.end()) {
        return false; // No previous vote
    }
    
    auto& oldVote = voteIt->second;
    
    // Check cooldown
    if (currentHeight_ - oldVote.voteHeight < VOTE_CHANGE_COOLDOWN) {
        return false;
    }
    
    // Remove old vote from counts
    switch (oldVote.choice) {
        case VoteChoice::Yes:
            proposal.votesYes -= oldVote.votingPower;
            break;
        case VoteChoice::No:
            proposal.votesNo -= oldVote.votingPower;
            break;
        case VoteChoice::Abstain:
            proposal.votesAbstain -= oldVote.votingPower;
            break;
        case VoteChoice::NoWithVeto:
            proposal.votesNoWithVeto -= oldVote.votingPower;
            break;
    }
    
    // Add new vote
    switch (newVote.choice) {
        case VoteChoice::Yes:
            proposal.votesYes += newVote.votingPower;
            break;
        case VoteChoice::No:
            proposal.votesNo += newVote.votingPower;
            break;
        case VoteChoice::Abstain:
            proposal.votesAbstain += newVote.votingPower;
            break;
        case VoteChoice::NoWithVeto:
            proposal.votesNoWithVeto += newVote.votingPower;
            break;
    }
    
    proposalVotes[newVote.voter] = newVote;
    return true;
}

std::optional<Vote> GovernanceEngine::GetVote(const GovernanceProposalId& proposalId, 
                                              const VoterId& voter) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto propIt = votes_.find(proposalId);
    if (propIt == votes_.end()) {
        return std::nullopt;
    }
    
    auto voteIt = propIt->second.find(voter);
    if (voteIt == propIt->second.end()) {
        return std::nullopt;
    }
    
    return voteIt->second;
}

std::vector<Vote> GovernanceEngine::GetVotes(const GovernanceProposalId& proposalId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Vote> result;
    auto propIt = votes_.find(proposalId);
    if (propIt != votes_.end()) {
        for (const auto& [voter, vote] : propIt->second) {
            result.push_back(vote);
        }
    }
    return result;
}

bool GovernanceEngine::HasVoted(const GovernanceProposalId& proposalId, const VoterId& voter) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto propIt = votes_.find(proposalId);
    if (propIt == votes_.end()) {
        return false;
    }
    return propIt->second.count(voter) > 0;
}

bool GovernanceEngine::Delegate(const Delegation& delegation, const std::vector<Byte>& signature) {
    (void)signature; // Would verify in production
    return delegations_.AddDelegation(delegation);
}

bool GovernanceEngine::RevokeDelegation(const VoterId& delegator, const std::vector<Byte>& signature) {
    (void)signature; // Would verify in production
    return delegations_.RemoveDelegation(delegator);
}

void GovernanceEngine::UpdateVotingPower(const VoterId& voter, uint64_t power) {
    votingPower_.UpdateVotingPower(voter, power);
}

uint64_t GovernanceEngine::GetVotingPower(const VoterId& voter) const {
    return votingPower_.GetVotingPower(voter);
}

uint64_t GovernanceEngine::GetEffectiveVotingPower(const VoterId& voter, ProposalType type) const {
    return delegations_.GetEffectiveVotingPower(voter, votingPower_, type, currentHeight_);
}

ParameterValue GovernanceEngine::GetParameter(GovernableParameter param) const {
    return params_->GetParameter(param);
}

bool GovernanceEngine::VetoProposal(const GovernanceProposalId& proposalId,
                                     const VoterId& guardianId,
                                     const std::vector<Byte>& signature) {
    (void)signature; // Would verify in production
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = proposals_.find(proposalId);
    if (it == proposals_.end()) {
        return false;
    }
    
    if (!guardians_.RecordVeto(guardianId, proposalId)) {
        return false;
    }
    
    // Check if proposal is now vetoed
    if (guardians_.IsVetoed(proposalId)) {
        it->second.status = GovernanceStatus::Vetoed;
    }
    
    return true;
}

void GovernanceEngine::ProcessBlock(int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    currentHeight_ = height;
    
    // Expire old delegations
    delegations_.ExpireDelegations(height);
    
    // Process proposals
    for (auto& [id, proposal] : proposals_) {
        // Start voting
        if (proposal.status == GovernanceStatus::Pending && 
            height >= proposal.votingStartHeight) {
            proposal.status = GovernanceStatus::Active;
            SnapshotVotingPower(proposal);
        }
        
        // End voting
        if (proposal.status == GovernanceStatus::Active &&
            height >= proposal.votingEndHeight) {
            EndVoting(const_cast<GovernanceProposalId&>(id), height);
        }
        
        // Execute approved proposals
        if (proposal.status == GovernanceStatus::Approved &&
            proposal.IsReadyForExecution(height)) {
            ExecuteProposal(proposal);
        }
    }
}

void GovernanceEngine::SetParameterChangeCallback(ParameterChangeCallback callback) {
    paramChangeCallback_ = callback;
}

void GovernanceEngine::SetProtocolUpgradeCallback(ProtocolUpgradeCallback callback) {
    upgradeCallback_ = callback;
}

std::vector<Byte> GovernanceEngine::Serialize() const {
    std::vector<Byte> data;
    return data;
}

bool GovernanceEngine::Deserialize(const Byte* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

void GovernanceEngine::StartVoting(GovernanceProposalId& id, int height) {
    auto it = proposals_.find(id);
    if (it == proposals_.end()) return;
    
    it->second.status = GovernanceStatus::Active;
    it->second.votingStartHeight = height;
    SnapshotVotingPower(it->second);
}

void GovernanceEngine::EndVoting(GovernanceProposalId& id, int height) {
    auto it = proposals_.find(id);
    if (it == proposals_.end()) return;
    
    auto& proposal = it->second;
    
    if (proposal.IsVetoed()) {
        proposal.status = GovernanceStatus::Vetoed;
    } else if (!proposal.HasQuorum()) {
        proposal.status = GovernanceStatus::QuorumFailed;
    } else if (!proposal.HasApproval()) {
        proposal.status = GovernanceStatus::Rejected;
    } else {
        proposal.status = GovernanceStatus::Approved;
        proposal.executionHeight = height + proposal.GetExecutionDelay();
    }
}

bool GovernanceEngine::ExecuteProposal(GovernanceProposal& proposal) {
    bool success = false;
    
    if (std::holds_alternative<std::vector<ParameterChange>>(proposal.payload)) {
        success = ExecuteParameterChanges(std::get<std::vector<ParameterChange>>(proposal.payload));
    } else if (std::holds_alternative<ProtocolUpgrade>(proposal.payload)) {
        success = ExecuteProtocolUpgrade(std::get<ProtocolUpgrade>(proposal.payload));
    } else if (std::holds_alternative<ConstitutionalChange>(proposal.payload)) {
        success = ExecuteConstitutionalChange(std::get<ConstitutionalChange>(proposal.payload));
    } else {
        // Signal proposals don't need execution
        success = (proposal.type == ProposalType::Signal);
    }
    
    proposal.status = success ? GovernanceStatus::Executed : GovernanceStatus::ExecutionFailed;
    return success;
}

bool GovernanceEngine::ExecuteParameterChanges(const std::vector<ParameterChange>& changes) {
    if (!params_->ApplyChanges(changes)) {
        return false;
    }
    
    // Notify callbacks
    if (paramChangeCallback_) {
        for (const auto& change : changes) {
            paramChangeCallback_(change.parameter, change.newValue);
        }
    }
    
    return true;
}

bool GovernanceEngine::ExecuteProtocolUpgrade(const ProtocolUpgrade& upgrade) {
    if (upgradeCallback_) {
        upgradeCallback_(upgrade);
    }
    return true;
}

bool GovernanceEngine::ExecuteConstitutionalChange(const ConstitutionalChange& change) {
    (void)change;
    // Constitutional changes are recorded but implementation is external
    return true;
}

bool GovernanceEngine::VerifyProposalSignature(const GovernanceProposal& proposal,
                                                const std::vector<Byte>& signature) const {
    if (signature.empty()) {
        return false;
    }
    
    Hash256 hash = proposal.CalculateHash();
    return proposal.proposer.Verify(hash, signature);
}

bool GovernanceEngine::ValidateProposal(const GovernanceProposal& proposal) const {
    // Check title and description
    if (proposal.title.empty() || proposal.title.length() > 200) {
        return false;
    }
    
    if (proposal.description.empty() || proposal.description.length() > 10000) {
        return false;
    }
    
    // Validate payload based on type
    if (proposal.type == ProposalType::Parameter) {
        if (!std::holds_alternative<std::vector<ParameterChange>>(proposal.payload)) {
            return false;
        }
        const auto& changes = std::get<std::vector<ParameterChange>>(proposal.payload);
        for (const auto& change : changes) {
            if (!change.IsValid()) {
                return false;
            }
        }
    }
    
    return true;
}

void GovernanceEngine::SnapshotVotingPower(GovernanceProposal& proposal) {
    proposal.totalVotingPower = votingPower_.GetTotalVotingPower();
}

} // namespace governance
} // namespace shurium
