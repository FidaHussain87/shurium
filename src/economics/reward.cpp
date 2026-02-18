// SHURIUM - Block Reward System Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/economics/reward.h>
#include <shurium/core/types.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace shurium {
namespace economics {

// ============================================================================
// RewardDistribution
// ============================================================================

std::string RewardDistribution::ToString() const {
    std::ostringstream ss;
    ss << "RewardDistribution {"
       << " total: " << FormatAmount(total)
       << ", work: " << FormatAmount(workReward)
       << ", ubi: " << FormatAmount(ubiPool)
       << ", contrib: " << FormatAmount(contributions)
       << ", ecosystem: " << FormatAmount(ecosystem)
       << ", stability: " << FormatAmount(stability)
       << " }";
    return ss.str();
}

// ============================================================================
// RewardCalculator
// ============================================================================

RewardCalculator::RewardCalculator(const consensus::Params& params)
    : params_(params) {
}

Amount RewardCalculator::GetBlockSubsidy(int height) const {
    if (height < 0) {
        return 0;
    }
    
    // Calculate number of halvings
    int halvings = height / HALVING_INTERVAL;
    
    // After ~64 halvings, subsidy would be 0, but we have a minimum
    if (halvings >= 64) {
        return MINIMUM_BLOCK_REWARD;
    }
    
    // Calculate subsidy with halving
    Amount subsidy = INITIAL_BLOCK_REWARD >> halvings;
    
    // Enforce minimum reward
    return std::max(subsidy, MINIMUM_BLOCK_REWARD);
}

RewardDistribution RewardCalculator::GetRewardDistribution(int height) const {
    RewardDistribution dist;
    dist.total = GetBlockSubsidy(height);
    
    // Calculate each component
    dist.workReward = CalculatePercentage(dist.total, RewardPercentage::WORK_REWARD);
    dist.ubiPool = CalculatePercentage(dist.total, RewardPercentage::UBI_POOL);
    dist.contributions = CalculatePercentage(dist.total, RewardPercentage::CONTRIBUTIONS);
    dist.ecosystem = CalculatePercentage(dist.total, RewardPercentage::ECOSYSTEM);
    dist.stability = CalculatePercentage(dist.total, RewardPercentage::STABILITY);
    
    // Handle rounding - add any remainder to work reward
    Amount sum = dist.workReward + dist.ubiPool + dist.contributions + 
                 dist.ecosystem + dist.stability;
    if (sum < dist.total) {
        dist.workReward += (dist.total - sum);
    }
    
    return dist;
}

Amount RewardCalculator::GetWorkReward(int height) const {
    return CalculatePercentage(GetBlockSubsidy(height), RewardPercentage::WORK_REWARD);
}

Amount RewardCalculator::GetUBIPoolAmount(int height) const {
    return CalculatePercentage(GetBlockSubsidy(height), RewardPercentage::UBI_POOL);
}

Amount RewardCalculator::GetContributionReward(int height) const {
    return CalculatePercentage(GetBlockSubsidy(height), RewardPercentage::CONTRIBUTIONS);
}

Amount RewardCalculator::GetEcosystemReward(int height) const {
    return CalculatePercentage(GetBlockSubsidy(height), RewardPercentage::ECOSYSTEM);
}

Amount RewardCalculator::GetStabilityReward(int height) const {
    return CalculatePercentage(GetBlockSubsidy(height), RewardPercentage::STABILITY);
}

Amount RewardCalculator::GetCumulativeSupply(int height) const {
    if (height < 0) {
        return 0;
    }
    
    Amount total = 0;
    int currentHeight = 0;
    
    while (currentHeight <= height) {
        int halvings = currentHeight / HALVING_INTERVAL;
        int nextHalvingHeight = (halvings + 1) * HALVING_INTERVAL;
        
        // Blocks until next halving or until target height
        int blocksInEra = std::min(nextHalvingHeight - 1, height) - currentHeight + 1;
        
        // Subsidy for this era
        Amount subsidy = INITIAL_BLOCK_REWARD >> halvings;
        if (subsidy < MINIMUM_BLOCK_REWARD) {
            subsidy = MINIMUM_BLOCK_REWARD;
        }
        
        total += subsidy * blocksInEra;
        currentHeight += blocksInEra;
    }
    
    return total;
}

int RewardCalculator::GetMaxSupplyHeight() const {
    // Calculate when minimum reward takes effect permanently
    int halvings = 0;
    Amount subsidy = INITIAL_BLOCK_REWARD;
    
    while (subsidy > MINIMUM_BLOCK_REWARD && halvings < 64) {
        halvings++;
        subsidy = INITIAL_BLOCK_REWARD >> halvings;
    }
    
    return halvings * HALVING_INTERVAL;
}

int RewardCalculator::GetHalvingCount(int height) const {
    if (height < 0) {
        return 0;
    }
    return height / HALVING_INTERVAL;
}

int RewardCalculator::GetNextHalvingHeight(int currentHeight) const {
    if (currentHeight < 0) {
        return HALVING_INTERVAL;
    }
    int nextHalving = ((currentHeight / HALVING_INTERVAL) + 1) * HALVING_INTERVAL;
    return nextHalving;
}

int RewardCalculator::GetBlocksUntilHalving(int currentHeight) const {
    return GetNextHalvingHeight(currentHeight) - currentHeight;
}

// ============================================================================
// EpochRewardPool
// ============================================================================

void EpochRewardPool::AddBlockReward(const RewardDistribution& dist) {
    ubiPool += dist.ubiPool;
    contributionPool += dist.contributions;
    blockCount++;
}

void EpochRewardPool::Complete() {
    isComplete = true;
}

Amount EpochRewardPool::AverageUBIPerBlock() const {
    if (blockCount == 0) {
        return 0;
    }
    return ubiPool / blockCount;
}

std::string EpochRewardPool::ToString() const {
    std::ostringstream ss;
    ss << "EpochRewardPool {"
       << " epoch: " << epoch
       << ", blocks: " << blockCount
       << ", ubi: " << FormatAmount(ubiPool)
       << ", contrib: " << FormatAmount(contributionPool)
       << ", complete: " << (isComplete ? "yes" : "no")
       << " }";
    return ss.str();
}

// ============================================================================
// CoinbaseBuilder
// ============================================================================

CoinbaseBuilder::CoinbaseBuilder(const RewardCalculator& calculator)
    : calculator_(calculator) {
}

std::vector<std::pair<std::vector<Byte>, Amount>> CoinbaseBuilder::BuildCoinbase(
    int height,
    const Hash160& minerAddress,
    const Hash160& ubiPoolAddress,
    const Hash160& ecosystemAddress,
    const Hash160& stabilityAddress
) const {
    std::vector<std::pair<std::vector<Byte>, Amount>> outputs;
    
    RewardDistribution dist = calculator_.GetRewardDistribution(height);
    
    // Helper to create P2PKH script: OP_DUP OP_HASH160 <20 bytes> OP_EQUALVERIFY OP_CHECKSIG
    auto makeP2PKH = [](const Hash160& addr) -> std::vector<Byte> {
        std::vector<Byte> script;
        script.reserve(25);
        script.push_back(0x76); // OP_DUP
        script.push_back(0xa9); // OP_HASH160
        script.push_back(0x14); // Push 20 bytes
        script.insert(script.end(), addr.begin(), addr.end());
        script.push_back(0x88); // OP_EQUALVERIFY
        script.push_back(0xac); // OP_CHECKSIG
        return script;
    };
    
    // Work reward to miner
    if (dist.workReward > 0) {
        outputs.emplace_back(makeP2PKH(minerAddress), dist.workReward);
    }
    
    // UBI pool (uses same address for collection)
    if (dist.ubiPool > 0) {
        outputs.emplace_back(makeP2PKH(ubiPoolAddress), dist.ubiPool);
    }
    
    // Contributions - for now, goes to ecosystem
    // In production, this would be distributed to validators of useful work
    if (dist.contributions > 0) {
        outputs.emplace_back(makeP2PKH(ecosystemAddress), dist.contributions);
    }
    
    // Ecosystem development
    if (dist.ecosystem > 0) {
        outputs.emplace_back(makeP2PKH(ecosystemAddress), dist.ecosystem);
    }
    
    // Stability reserve
    if (dist.stability > 0) {
        outputs.emplace_back(makeP2PKH(stabilityAddress), dist.stability);
    }
    
    return outputs;
}

bool CoinbaseBuilder::VerifyCoinbase(
    int height,
    const std::vector<std::pair<std::vector<Byte>, Amount>>& outputs
) const {
    RewardDistribution expected = calculator_.GetRewardDistribution(height);
    
    // Calculate total output amount
    Amount totalOutput = 0;
    for (const auto& [script, amount] : outputs) {
        totalOutput += amount;
    }
    
    // Total should not exceed expected total
    if (totalOutput > expected.total) {
        return false;
    }
    
    // For basic validation, ensure we have expected components
    // More detailed validation would check specific addresses
    return totalOutput == expected.total;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string FormatAmount(Amount amount, int decimals) {
    // Amount is in satoshis (100,000,000 = 1 NXS)
    constexpr Amount SATOSHIS_PER_COIN = 100000000;
    
    bool negative = amount < 0;
    if (negative) {
        amount = -amount;
    }
    
    Amount wholePart = amount / SATOSHIS_PER_COIN;
    Amount fracPart = amount % SATOSHIS_PER_COIN;
    
    std::ostringstream ss;
    if (negative) {
        ss << "-";
    }
    ss << wholePart;
    
    if (decimals > 0) {
        ss << ".";
        // Format fractional part with leading zeros
        std::ostringstream fracSS;
        fracSS << std::setfill('0') << std::setw(8) << fracPart;
        std::string fracStr = fracSS.str();
        // Truncate to requested decimals
        ss << fracStr.substr(0, decimals);
    }
    
    ss << " NXS";
    return ss.str();
}

Amount ParseAmount(const std::string& str) {
    constexpr Amount SATOSHIS_PER_COIN = 100000000;
    
    std::string s = str;
    
    // Remove "SHR" suffix if present
    size_t nxsPos = s.find("SHR");
    if (nxsPos != std::string::npos) {
        s = s.substr(0, nxsPos);
    }
    
    // Trim whitespace
    while (!s.empty() && std::isspace(s.back())) {
        s.pop_back();
    }
    while (!s.empty() && std::isspace(s.front())) {
        s.erase(0, 1);
    }
    
    if (s.empty()) {
        return 0;
    }
    
    bool negative = false;
    if (s[0] == '-') {
        negative = true;
        s.erase(0, 1);
    }
    
    // Find decimal point
    size_t dotPos = s.find('.');
    
    Amount wholePart = 0;
    Amount fracPart = 0;
    
    if (dotPos != std::string::npos) {
        // Parse whole part
        if (dotPos > 0) {
            wholePart = std::stoll(s.substr(0, dotPos));
        }
        
        // Parse fractional part
        std::string fracStr = s.substr(dotPos + 1);
        // Pad or truncate to 8 digits
        while (fracStr.length() < 8) {
            fracStr += '0';
        }
        if (fracStr.length() > 8) {
            fracStr = fracStr.substr(0, 8);
        }
        fracPart = std::stoll(fracStr);
    } else {
        wholePart = std::stoll(s);
    }
    
    Amount result = wholePart * SATOSHIS_PER_COIN + fracPart;
    return negative ? -result : result;
}

} // namespace economics
} // namespace shurium
