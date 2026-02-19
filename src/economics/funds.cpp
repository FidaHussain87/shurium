// SHURIUM - Fund Management Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/economics/funds.h"
#include "shurium/core/script.h"
#include "shurium/crypto/sha256.h"
#include "shurium/crypto/ripemd160.h"
#include "shurium/crypto/keys.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace shurium {
namespace economics {

// ============================================================================
// Global Fund Manager
// ============================================================================

static std::unique_ptr<FundManager> g_fundManager;

FundManager& GetFundManager() {
    if (!g_fundManager) {
        g_fundManager = std::make_unique<FundManager>();
    }
    return *g_fundManager;
}

bool InitializeFundManager(const std::string& network) {
    g_fundManager = std::make_unique<FundManager>();
    return g_fundManager->Initialize(network);
}

// ============================================================================
// FundConfig Implementation
// ============================================================================

Script FundConfig::GenerateRedeemScript() const {
    // Create m-of-n multisig: OP_2 <pubkey1> <pubkey2> <pubkey3> OP_3 OP_CHECKMULTISIG
    Script script;
    
    // Push required signatures (2)
    script << static_cast<Opcode>(OP_1 + FUND_MULTISIG_REQUIRED - 1);
    
    // Push all public keys (sorted for determinism)
    std::vector<std::vector<uint8_t>> sortedPubkeys;
    for (const auto& keyInfo : keys) {
        if (keyInfo.pubkey.IsValid()) {
            sortedPubkeys.push_back(keyInfo.pubkey.ToVector());
        }
    }
    std::sort(sortedPubkeys.begin(), sortedPubkeys.end());
    
    for (const auto& pubkey : sortedPubkeys) {
        script << pubkey;
    }
    
    // Push total keys (3)
    script << static_cast<Opcode>(OP_1 + FUND_MULTISIG_TOTAL - 1);
    
    // Add OP_CHECKMULTISIG
    script << OP_CHECKMULTISIG;
    
    return script;
}

Hash160 FundConfig::GetScriptHash() const {
    // P2SH: HASH160(redeemScript)
    Script redeem = GenerateRedeemScript();
    
    // Script inherits from std::vector<uint8_t>, so we can use it directly
    return ComputeHash160(redeem.data(), redeem.size());
}

std::string FundConfig::GetAddress(const std::string& hrp) const {
    // For now, return hex representation with prefix
    // TODO: Implement proper bech32 encoding for P2WSH
    Hash160 hash = GetScriptHash();
    std::ostringstream oss;
    oss << hrp << "1q";  // Simulated bech32 prefix
    for (size_t i = 0; i < hash.size(); ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

// ============================================================================
// FundManager Implementation
// ============================================================================

FundManager::FundManager() = default;
FundManager::~FundManager() = default;

bool FundManager::Initialize(const std::string& network) {
    m_funds.clear();
    m_addressToFund.clear();
    
    // Initialize each fund type
    std::vector<FundType> types = {
        FundType::UBI,
        FundType::Contribution,
        FundType::Ecosystem,
        FundType::Stability
    };
    
    for (FundType type : types) {
        FundConfig config;
        config.type = type;
        config.percentageBasisPoints = GetFundPercentageBasisPoints(type);
        
        switch (type) {
            case FundType::UBI:
                config.name = "UBI Pool";
                config.description = "Universal Basic Income distribution fund - 30% of block rewards";
                config.requiresGovernanceVote = true;
                config.maxSpendWithoutVote = 0;  // All UBI spending requires governance
                break;
                
            case FundType::Contribution:
                config.name = "Contribution Fund";
                config.description = "Human contribution rewards - 15% of block rewards";
                config.requiresGovernanceVote = false;
                config.maxSpendWithoutVote = 1000 * COIN;  // Up to 1000 SHR without vote
                break;
                
            case FundType::Ecosystem:
                config.name = "Ecosystem Fund";
                config.description = "Development and ecosystem growth - 10% of block rewards";
                config.requiresGovernanceVote = false;
                config.maxSpendWithoutVote = 5000 * COIN;  // Up to 5000 SHR without vote
                break;
                
            case FundType::Stability:
                config.name = "Stability Reserve";
                config.description = "Price stability mechanism - 5% of block rewards";
                config.requiresGovernanceVote = true;
                config.maxSpendWithoutVote = 0;  // All stability spending requires governance
                break;
        }
        
        // Generate initial keys for the fund
        config.keys = GenerateInitialKeys(type, network);
        
        // Calculate derived values
        config.redeemScript = config.GenerateRedeemScript();
        config.scriptHash = config.GetScriptHash();
        
        // Map address to fund type
        m_addressToFund[config.scriptHash] = type;
        
        m_funds.push_back(config);
    }
    
    return true;
}

std::array<FundKeyInfo, FUND_MULTISIG_TOTAL> FundManager::GenerateInitialKeys(
    FundType type,
    const std::string& network
) {
    std::array<FundKeyInfo, FUND_MULTISIG_TOTAL> keys;
    
    // Generate deterministic keys based on fund type and network
    // In production, these would be real keys held by different parties
    // For now, we generate deterministic keys from seed phrases
    
    const char* fundNames[] = {"UBI", "CONTRIBUTION", "ECOSYSTEM", "STABILITY"};
    const char* roleNames[] = {"GOVERNANCE", "FOUNDATION", "COMMUNITY"};
    
    int fundIndex = static_cast<int>(type);
    
    for (size_t i = 0; i < FUND_MULTISIG_TOTAL; ++i) {
        // Create a deterministic seed
        std::string seed = std::string("SHURIUM_") + network + "_" + 
                          fundNames[fundIndex] + "_KEY_" + roleNames[i] + "_V1";
        
        // Hash the seed to get 32 bytes for private key
        Hash256 seedHash = SHA256Hash(
            reinterpret_cast<const uint8_t*>(seed.data()), 
            seed.size()
        );
        
        // Create private key from hash
        PrivateKey privKey(seedHash.data());
        
        // Derive public key
        keys[i].pubkey = privKey.GetPublicKey();
        keys[i].role = static_cast<FundKeyRole>(i);
        keys[i].description = std::string(roleNames[i]) + " key for " + fundNames[fundIndex] + " fund";
        keys[i].activeFrom = 0;
        keys[i].activeTo = 0;  // No expiry
    }
    
    return keys;
}

const FundConfig& FundManager::GetFundConfig(FundType type) const {
    for (const auto& config : m_funds) {
        if (config.type == type) {
            return config;
        }
    }
    // Return first fund as fallback (shouldn't happen)
    return m_funds[0];
}

const std::vector<FundConfig>& FundManager::GetAllFunds() const {
    return m_funds;
}

Hash160 FundManager::GetFundAddress(FundType type) const {
    return GetFundConfig(type).scriptHash;
}

bool FundManager::IsFundAddress(const Hash160& address) const {
    return m_addressToFund.find(address) != m_addressToFund.end();
}

std::optional<FundType> FundManager::GetFundTypeForAddress(const Hash160& address) const {
    auto it = m_addressToFund.find(address);
    if (it != m_addressToFund.end()) {
        return it->second;
    }
    return std::nullopt;
}

int FundManager::GetTotalFundPercentage() const {
    int total = 0;
    for (const auto& config : m_funds) {
        total += config.percentageBasisPoints;
    }
    return total / 100;  // Convert basis points to percentage
}

bool FundManager::RotateKey(FundType type, size_t keyIndex, const FundKeyInfo& newKey) {
    if (keyIndex >= FUND_MULTISIG_TOTAL) {
        return false;
    }
    
    // Find the fund config (non-const)
    for (auto& config : m_funds) {
        if (config.type == type) {
            // Remove old address mapping
            m_addressToFund.erase(config.scriptHash);
            
            // Update key
            config.keys[keyIndex] = newKey;
            
            // Regenerate scripts and address
            config.redeemScript = config.GenerateRedeemScript();
            config.scriptHash = config.GetScriptHash();
            
            // Add new address mapping
            m_addressToFund[config.scriptHash] = type;
            
            return true;
        }
    }
    
    return false;
}

bool FundManager::HasPendingKeyRotation(FundType /*type*/) const {
    // TODO: Implement pending key rotation tracking
    return false;
}

Amount FundManager::GetFundBalance(FundType /*type*/) const {
    // TODO: Implement UTXO scanning for fund balance
    return 0;
}

Amount FundManager::GetTotalReceived(FundType /*type*/) const {
    // TODO: Implement historical tracking
    return 0;
}

Amount FundManager::GetTotalSpent(FundType /*type*/) const {
    // TODO: Implement historical tracking
    return 0;
}

std::optional<FundManager::UnsignedFundTx> FundManager::CreateFundSpendingTx(
    FundType /*type*/,
    const std::vector<std::pair<Script, Amount>>& /*outputs*/,
    Amount /*fee*/
) const {
    // TODO: Implement fund spending transaction creation
    return std::nullopt;
}

bool FundManager::AddSignature(
    Transaction& /*tx*/,
    size_t /*inputIndex*/,
    const PublicKey& /*pubkey*/,
    const std::vector<uint8_t>& /*signature*/
) const {
    // TODO: Implement signature addition
    return false;
}

// ============================================================================
// Fund Statistics
// ============================================================================

std::vector<FundStats> GetAllFundStats() {
    std::vector<FundStats> stats;
    
    const auto& funds = GetFundManager().GetAllFunds();
    for (const auto& config : funds) {
        FundStats s;
        s.type = config.type;
        s.name = config.name;
        s.balance = GetFundManager().GetFundBalance(config.type);
        s.totalReceived = GetFundManager().GetTotalReceived(config.type);
        s.totalSpent = GetFundManager().GetTotalSpent(config.type);
        s.transactionCount = 0;  // TODO
        s.lastActivityHeight = 0;  // TODO
        stats.push_back(s);
    }
    
    return stats;
}

} // namespace economics
} // namespace shurium
