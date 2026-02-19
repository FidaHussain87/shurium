// SHURIUM - Fund Management System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Manages the SHURIUM protocol funds:
// - UBI Pool (30% of block rewards) - Universal Basic Income distribution
// - Contribution Fund (15%) - Rewards for human contributions
// - Ecosystem Fund (10%) - Development and ecosystem growth
// - Stability Reserve (5%) - Price stability mechanism
//
// Each fund uses a 2-of-3 multisig for security and governance.
//
// CONFIGURATION:
// Fund addresses can be configured in multiple ways (in order of priority):
// 1. Genesis block (immutable after chain starts)
// 2. Governance vote (requires supermajority)
// 3. Configuration file (shurium.conf)
// 4. RPC command (runtime, non-persistent)
// 5. Default (deterministic addresses for demo/testing)

#ifndef SHURIUM_ECONOMICS_FUNDS_H
#define SHURIUM_ECONOMICS_FUNDS_H

#include <shurium/core/types.h>
#include <shurium/core/script.h>
#include <shurium/core/transaction.h>
#include <shurium/crypto/keys.h>
#include <shurium/crypto/sha256.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace economics {

// ============================================================================
// Fund Types
// ============================================================================

/// Enumeration of protocol funds
enum class FundType {
    UBI,            // Universal Basic Income pool (30%)
    Contribution,   // Human contribution rewards (15%)
    Ecosystem,      // Ecosystem development (10%)
    Stability       // Price stability reserve (5%)
};

/// Convert fund type to string
inline const char* FundTypeToString(FundType type) {
    switch (type) {
        case FundType::UBI: return "UBI Pool";
        case FundType::Contribution: return "Contribution Fund";
        case FundType::Ecosystem: return "Ecosystem Fund";
        case FundType::Stability: return "Stability Reserve";
        default: return "Unknown";
    }
}

/// Get fund percentage (basis points - 1/100th of a percent)
inline int GetFundPercentageBasisPoints(FundType type) {
    switch (type) {
        case FundType::UBI: return 3000;          // 30%
        case FundType::Contribution: return 1500;  // 15%
        case FundType::Ecosystem: return 1000;     // 10%
        case FundType::Stability: return 500;      // 5%
        default: return 0;
    }
}

// ============================================================================
// Address Source - Where the fund address came from
// ============================================================================

/// Source of fund address configuration
enum class FundAddressSource {
    Default,        // Deterministic address (demo/testing only)
    ConfigFile,     // From shurium.conf
    RpcCommand,     // Set via setfundaddress RPC
    GenesisBlock,   // Defined in genesis block (immutable)
    Governance      // Changed via governance vote
};

inline const char* FundAddressSourceToString(FundAddressSource source) {
    switch (source) {
        case FundAddressSource::Default: return "default (demo)";
        case FundAddressSource::ConfigFile: return "configuration file";
        case FundAddressSource::RpcCommand: return "RPC command";
        case FundAddressSource::GenesisBlock: return "genesis block";
        case FundAddressSource::Governance: return "governance vote";
        default: return "unknown";
    }
}

// ============================================================================
// Fund Key Configuration
// ============================================================================

/// Number of keys in multisig (n of m)
constexpr size_t FUND_MULTISIG_REQUIRED = 2;  // 2 signatures required
constexpr size_t FUND_MULTISIG_TOTAL = 3;     // out of 3 possible signers

/// Roles for fund key holders
enum class FundKeyRole {
    Governance,     // Elected governance council member
    Foundation,     // SHURIUM foundation key
    Community       // Community-elected guardian
};

/// Fund key information
struct FundKeyInfo {
    PublicKey pubkey;
    FundKeyRole role;
    std::string description;
    int64_t activeFrom;     // Block height when key became active
    int64_t activeTo;       // Block height when key expires (0 = no expiry)
};

// ============================================================================
// Fund Configuration
// ============================================================================

/// Configuration for a single fund
struct FundConfig {
    FundType type;
    std::string name;
    std::string description;
    int percentageBasisPoints;  // In basis points (3000 = 30%)
    
    // Multisig configuration
    std::array<FundKeyInfo, FUND_MULTISIG_TOTAL> keys;
    
    // Derived addresses
    Hash160 scriptHash;         // P2SH address hash
    Script redeemScript;        // Multisig redeem script
    
    // Custom address (if set via config/RPC)
    std::string customAddress;  // User-configured address (overrides multisig)
    FundAddressSource addressSource;  // Where the address came from
    
    // Governance
    bool requiresGovernanceVote;  // Whether spending requires on-chain vote
    Amount maxSpendWithoutVote;   // Max amount spendable without governance vote
    
    /// Generate the multisig redeem script from keys
    Script GenerateRedeemScript() const;
    
    /// Get the P2SH script hash for this fund
    Hash160 GetScriptHash() const;
    
    /// Get the bech32 address for this fund
    std::string GetAddress(const std::string& hrp = "shr") const;
    
    /// Check if using custom address (not default multisig)
    bool HasCustomAddress() const { return !customAddress.empty(); }
};

// ============================================================================
// Fund Manager
// ============================================================================

/// Manages all protocol funds
class FundManager {
public:
    FundManager();
    ~FundManager();
    
    /// Initialize fund manager with network parameters
    bool Initialize(const std::string& network);
    
    /// Get configuration for a specific fund
    const FundConfig& GetFundConfig(FundType type) const;
    
    /// Get all fund configurations
    const std::vector<FundConfig>& GetAllFunds() const;
    
    /// Get the script hash (address) for a fund
    Hash160 GetFundAddress(FundType type) const;
    
    /// Check if an address belongs to a protocol fund
    bool IsFundAddress(const Hash160& address) const;
    
    /// Get fund type for an address (if it's a fund address)
    std::optional<FundType> GetFundTypeForAddress(const Hash160& address) const;
    
    /// Get total percentage allocated to funds (should be 60%)
    int GetTotalFundPercentage() const;
    
    // ========================================================================
    // Address Configuration
    // ========================================================================
    
    /// Set a custom address for a fund (from config file or RPC)
    /// Returns true if address was valid and set successfully
    bool SetFundAddress(FundType type, const std::string& address, FundAddressSource source);
    
    /// Clear custom address, revert to default multisig
    void ClearCustomAddress(FundType type);
    
    /// Load fund addresses from configuration file
    /// Expects keys like: ubiaddress=shr1q..., ecosystemaddress=shr1q...
    bool LoadAddressesFromConfig(const std::map<std::string, std::string>& config);
    
    /// Get the address source for a fund
    FundAddressSource GetAddressSource(FundType type) const;
    
    // ========================================================================
    // Key Management
    // ========================================================================
    
    /// Rotate a key for a fund (requires governance vote)
    bool RotateKey(FundType type, size_t keyIndex, const FundKeyInfo& newKey);
    
    /// Check if a key rotation is pending
    bool HasPendingKeyRotation(FundType type) const;
    
    // ========================================================================
    // Balance Tracking
    // ========================================================================
    
    /// Get current balance for a fund (requires UTXO database access)
    Amount GetFundBalance(FundType type) const;
    
    /// Get total value received by a fund since genesis
    Amount GetTotalReceived(FundType type) const;
    
    /// Get total value spent from a fund
    Amount GetTotalSpent(FundType type) const;
    
    // ========================================================================
    // Transaction Building
    // ========================================================================
    
    /// Create an unsigned spending transaction from a fund
    /// Returns the transaction and required signatures
    struct UnsignedFundTx {
        Transaction tx;
        std::vector<Hash256> sigHashes;  // Hashes to sign for each input
        Script redeemScript;
    };
    
    std::optional<UnsignedFundTx> CreateFundSpendingTx(
        FundType type,
        const std::vector<std::pair<Script, Amount>>& outputs,
        Amount fee
    ) const;
    
    /// Add a signature to a fund spending transaction
    bool AddSignature(
        Transaction& tx,
        size_t inputIndex,
        const PublicKey& pubkey,
        const std::vector<uint8_t>& signature
    ) const;
    
private:
    std::vector<FundConfig> m_funds;
    std::map<Hash160, FundType> m_addressToFund;
    
    /// Generate deterministic keys for a fund (for initial setup)
    static std::array<FundKeyInfo, FUND_MULTISIG_TOTAL> GenerateInitialKeys(
        FundType type,
        const std::string& network
    );
    
    /// Parse address string to Hash160
    static std::optional<Hash160> ParseAddress(const std::string& address);
};

// ============================================================================
// Fund Statistics
// ============================================================================

/// Statistics for a single fund
struct FundStats {
    FundType type;
    std::string name;
    Amount balance;
    Amount totalReceived;
    Amount totalSpent;
    size_t transactionCount;
    int64_t lastActivityHeight;
};

/// Get statistics for all funds
std::vector<FundStats> GetAllFundStats();

// ============================================================================
// Global Fund Manager Access
// ============================================================================

/// Get the global fund manager instance
FundManager& GetFundManager();

/// Initialize the global fund manager (only initializes once, safe to call multiple times)
bool InitializeFundManager(const std::string& network);

/// Check if fund manager has been initialized
bool IsFundManagerInitialized();

} // namespace economics
} // namespace shurium

#endif // SHURIUM_ECONOMICS_FUNDS_H
