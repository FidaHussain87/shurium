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
static bool g_fundManagerInitialized = false;

FundManager& GetFundManager() {
    if (!g_fundManager) {
        g_fundManager = std::make_unique<FundManager>();
    }
    return *g_fundManager;
}

bool InitializeFundManager(const std::string& network) {
    // Only initialize once - don't replace existing fund manager
    if (g_fundManagerInitialized && g_fundManager) {
        return true;  // Already initialized
    }
    
    g_fundManager = std::make_unique<FundManager>();
    g_fundManagerInitialized = g_fundManager->Initialize(network);
    return g_fundManagerInitialized;
}

bool IsFundManagerInitialized() {
    return g_fundManagerInitialized && g_fundManager != nullptr;
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
    // If custom address is set, return it
    if (!customAddress.empty()) {
        return customAddress;
    }
    
    // Otherwise, generate from multisig script hash
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
        config.addressSource = FundAddressSource::Default;  // Default until configured
        
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
    // WARNING: These are demo/testing keys only!
    // In production, use SetFundAddress() with real addresses you control.
    
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

// ============================================================================
// Address Configuration
// ============================================================================

std::optional<Hash160> FundManager::ParseAddress(const std::string& address) {
    // For addresses in the format "shr1q" + hex (like our default multisig)
    // Try to parse as hex first
    
    size_t hexStart = address.find("1q");
    if (hexStart == std::string::npos) {
        return std::nullopt;
    }
    hexStart += 2;  // Skip "1q"
    
    std::string hexPart = address.substr(hexStart);
    
    // Check if this is a hex-encoded address (40 chars = 20 bytes)
    if (hexPart.length() == 40) {
        // Validate all chars are hex
        bool isHex = true;
        for (char c : hexPart) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                isHex = false;
                break;
            }
        }
        
        if (isHex) {
            Hash160 hash;
            for (size_t i = 0; i < 20; ++i) {
                std::string byteStr = hexPart.substr(i * 2, 2);
                try {
                    hash[i] = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
                } catch (...) {
                    return std::nullopt;
                }
            }
            return hash;
        }
    }
    
    // For bech32 addresses, we can't parse to Hash160 directly
    // but we can still validate format and return a placeholder
    // The actual address string will be stored in customAddress field
    
    // Basic bech32 format validation: must contain valid characters
    // Bech32 uses: 0-9, a-z except 1, b, i, o (case-insensitive)
    // For now, just return empty hash - the string address will be used directly
    
    // Return a deterministic hash based on the address string for mapping purposes
    Hash256 addrHash = SHA256Hash(
        reinterpret_cast<const uint8_t*>(address.data()),
        address.size()
    );
    
    Hash160 hash;
    std::copy(addrHash.begin(), addrHash.begin() + 20, hash.begin());
    return hash;
}

bool FundManager::SetFundAddress(FundType type, const std::string& address, FundAddressSource source) {
    // Don't allow overriding genesis or governance addresses with lower-priority sources
    for (auto& config : m_funds) {
        if (config.type == type) {
            // Check priority - higher priority sources cannot be overridden by lower ones
            if (config.addressSource == FundAddressSource::GenesisBlock && 
                source != FundAddressSource::Governance) {
                return false;  // Only governance can override genesis
            }
            if (config.addressSource == FundAddressSource::Governance &&
                source != FundAddressSource::Governance) {
                return false;  // Only governance can override governance
            }
            
            // Parse and validate the address
            auto parsedHash = ParseAddress(address);
            if (!parsedHash) {
                return false;  // Invalid address format
            }
            
            // Remove old address mapping
            if (!config.customAddress.empty()) {
                auto oldHash = ParseAddress(config.customAddress);
                if (oldHash) {
                    m_addressToFund.erase(*oldHash);
                }
            } else {
                m_addressToFund.erase(config.scriptHash);
            }
            
            // Set custom address
            config.customAddress = address;
            config.addressSource = source;
            
            // Add new address mapping
            m_addressToFund[*parsedHash] = type;
            
            return true;
        }
    }
    return false;
}

void FundManager::ClearCustomAddress(FundType type) {
    for (auto& config : m_funds) {
        if (config.type == type) {
            if (!config.customAddress.empty()) {
                // Remove custom address mapping
                auto oldHash = ParseAddress(config.customAddress);
                if (oldHash) {
                    m_addressToFund.erase(*oldHash);
                }
                
                config.customAddress.clear();
                config.addressSource = FundAddressSource::Default;
                
                // Restore default address mapping
                m_addressToFund[config.scriptHash] = type;
            }
            break;
        }
    }
}

bool FundManager::LoadAddressesFromConfig(const std::map<std::string, std::string>& config) {
    // Map config keys to fund types
    std::map<std::string, FundType> keyToType = {
        {"ubiaddress", FundType::UBI},
        {"ubi_address", FundType::UBI},
        {"contributionaddress", FundType::Contribution},
        {"contribution_address", FundType::Contribution},
        {"ecosystemaddress", FundType::Ecosystem},
        {"ecosystem_address", FundType::Ecosystem},
        {"stabilityaddress", FundType::Stability},
        {"stability_address", FundType::Stability}
    };
    
    bool anySet = false;
    
    for (const auto& [key, value] : config) {
        // Convert key to lowercase for comparison
        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        
        auto it = keyToType.find(lowerKey);
        if (it != keyToType.end() && !value.empty()) {
            if (SetFundAddress(it->second, value, FundAddressSource::ConfigFile)) {
                anySet = true;
            }
        }
    }
    
    return anySet;
}

FundAddressSource FundManager::GetAddressSource(FundType type) const {
    return GetFundConfig(type).addressSource;
}

// ============================================================================
// Key Management
// ============================================================================

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
