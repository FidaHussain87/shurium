// SHURIUM - Wallet Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Main wallet class providing:
// - UTXO tracking and balance calculation
// - Transaction building and signing
// - Address generation and management
// - UBI claim creation
// - Watch-only wallet support

#ifndef SHURIUM_WALLET_WALLET_H
#define SHURIUM_WALLET_WALLET_H

#include <shurium/core/types.h>
#include <shurium/core/transaction.h>
#include <shurium/core/block.h>
#include <shurium/chain/coins.h>
#include <shurium/wallet/keystore.h>
#include <shurium/wallet/coinselection.h>
#include <shurium/wallet/hdkey.h>
#include <shurium/identity/identity.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace shurium {

// Forward declaration for UBIClaim
namespace economics { struct UBIClaim; }

namespace wallet {

// ============================================================================
// Forward Declarations
// ============================================================================

class Wallet;
class WalletTransaction;
class WalletOutput;

// ============================================================================
// Wallet Output (UTXO)
// ============================================================================

/// Status of a wallet output
enum class OutputStatus {
    Available,      // Unspent and confirmed
    Unconfirmed,    // Unspent but unconfirmed
    Immature,       // Coinbase not yet mature
    Spent,          // Already spent
    Locked,         // Manually locked by user
    Frozen          // Frozen (cannot be spent)
};

/**
 * Represents a wallet-owned output (UTXO).
 */
class WalletOutput {
public:
    /// The outpoint
    OutPoint outpoint;
    
    /// The output data
    TxOut txout;
    
    /// Block height where this was confirmed (-1 if unconfirmed)
    int32_t height{-1};
    
    /// Whether this is from a coinbase
    bool coinbase{false};
    
    /// Time when we received this output
    int64_t timeReceived{0};
    
    /// Status
    OutputStatus status{OutputStatus::Unconfirmed};
    
    /// Key hash that owns this output
    Hash160 keyHash;
    
    /// Derivation path (if HD)
    std::optional<DerivationPath> keyPath;
    
    /// Label/memo
    std::string label;
    
    /// Constructor
    WalletOutput() = default;
    WalletOutput(const OutPoint& op, const TxOut& out, int32_t h = -1)
        : outpoint(op), txout(out), height(h) {}
    
    /// Get the value
    Amount GetValue() const { return txout.nValue; }
    
    /// Check if spendable (available and mature)
    bool IsSpendable(int32_t currentHeight, int32_t maturity = 100) const;
    
    /// Get depth (confirmations)
    int32_t GetDepth(int32_t currentHeight) const;
    
    /// Check if mature (for coinbase outputs)
    bool IsMature(int32_t currentHeight, int32_t maturity = 100) const;
    
    /// Convert to OutputGroup for coin selection
    OutputGroup ToOutputGroup(FeeRate feeRate, int32_t currentHeight) const;
};

// ============================================================================
// Wallet Transaction
// ============================================================================

/// Transaction confirmation status
struct TxConfirmation {
    int32_t blockHeight{-1};
    Hash256 blockHash;
    int32_t txIndex{-1};  // Position in block
    int64_t blockTime{0};
    
    bool IsConfirmed() const { return blockHeight >= 0; }
    int32_t GetDepth(int32_t currentHeight) const;
};

/**
 * A transaction relevant to this wallet.
 */
class WalletTransaction {
public:
    /// The transaction
    TransactionRef tx;
    
    /// Confirmation info
    TxConfirmation confirmation;
    
    /// Time when transaction was added to wallet
    int64_t timeReceived{0};
    
    /// Time when transaction was created (if by us)
    int64_t timeCreated{0};
    
    /// Is this our transaction (created by this wallet)?
    bool fromMe{false};
    
    /// Inputs that belong to us (indices into tx->vin)
    std::vector<size_t> ourInputs;
    
    /// Outputs that belong to us (indices into tx->vout)
    std::vector<size_t> ourOutputs;
    
    /// Fee (if we created this tx)
    Amount fee{0};
    
    /// Label/memo
    std::string label;
    
    /// Constructor
    WalletTransaction() = default;
    explicit WalletTransaction(TransactionRef txIn) : tx(std::move(txIn)) {}
    
    /// Get transaction hash
    const TxHash& GetHash() const { return tx->GetHash(); }
    
    /// Check if confirmed
    bool IsConfirmed() const { return confirmation.IsConfirmed(); }
    
    /// Get confirmation depth
    int32_t GetDepth(int32_t currentHeight) const;
    
    /// Calculate net amount (outputs - inputs that belong to us)
    Amount GetNetAmount() const;
    
    /// Get debit (sum of our inputs)
    Amount GetDebit() const;
    
    /// Get credit (sum of our outputs)
    Amount GetCredit() const;
    
    /// Check if transaction is trusted (for unconfirmed)
    bool IsTrusted() const;
};

// ============================================================================
// Address Book
// ============================================================================

/// Address book entry
struct AddressBookEntry {
    std::string address;
    std::string label;
    std::string purpose;  // "send", "receive", "refund"
    int64_t created{0};
    
    AddressBookEntry() = default;
    AddressBookEntry(std::string addr, std::string lbl, std::string purp = "")
        : address(std::move(addr)), label(std::move(lbl)), purpose(std::move(purp)) {}
};

// ============================================================================
// Transaction Builder
// ============================================================================

/// Result of building a transaction
struct BuildTxResult {
    /// The built transaction (mutable for signing)
    MutableTransaction tx;
    
    /// Selected inputs
    std::vector<WalletOutput> inputs;
    
    /// Fee paid
    Amount fee{0};
    
    /// Change amount
    Amount change{0};
    
    /// Change output index (-1 if no change)
    int32_t changeIndex{-1};
    
    /// Was build successful?
    bool success{false};
    
    /// Error message (if failed)
    std::string error;
    
    /// Selection algorithm used
    std::string algorithm;
};

/// Recipient of a transaction
struct Recipient {
    /// Destination script
    Script scriptPubKey;
    
    /// Amount to send
    Amount amount;
    
    /// Label
    std::string label;
    
    /// Subtract fee from this output?
    bool subtractFee{false};
    
    Recipient() : amount(0) {}
    Recipient(const Script& script, Amount amt, std::string lbl = "")
        : scriptPubKey(script), amount(amt), label(std::move(lbl)) {}
    
    /// Create from address string
    static std::optional<Recipient> FromAddress(const std::string& address, Amount amount);
};

/**
 * Builder for creating transactions.
 */
class TransactionBuilder {
public:
    /// Create builder for wallet
    explicit TransactionBuilder(Wallet& wallet);
    
    /// Add a recipient
    TransactionBuilder& AddRecipient(const Recipient& recipient);
    TransactionBuilder& AddRecipient(const Script& script, Amount amount);
    TransactionBuilder& AddRecipient(const std::string& address, Amount amount);
    
    /// Set fee rate
    TransactionBuilder& SetFeeRate(FeeRate rate);
    
    /// Set absolute fee
    TransactionBuilder& SetAbsoluteFee(Amount fee);
    
    /// Set coin selection strategy
    TransactionBuilder& SetStrategy(SelectionStrategy strategy);
    
    /// Enable/disable change output
    TransactionBuilder& EnableChange(bool enable = true);
    
    /// Set change address (if not set, generates new)
    TransactionBuilder& SetChangeAddress(const Script& changeScript);
    
    /// Allow unconfirmed inputs?
    TransactionBuilder& AllowUnconfirmed(bool allow = true);
    
    /// Set minimum confirmations
    TransactionBuilder& SetMinConfirmations(int32_t minConf);
    
    /// Lock time
    TransactionBuilder& SetLockTime(uint32_t lockTime);
    
    /// Set RBF (replace-by-fee)
    TransactionBuilder& SetRBF(bool enable = true);
    
    /// Build the transaction
    BuildTxResult Build();
    
    /// Build and sign
    BuildTxResult BuildAndSign();
    
    /// Get estimated fee for current recipients
    Amount EstimateFee() const;
    
    /// Clear builder for reuse
    void Clear();

private:
    Wallet& wallet_;
    
    std::vector<Recipient> recipients_;
    FeeRate feeRate_{1};
    std::optional<Amount> absoluteFee_;
    SelectionStrategy strategy_{SelectionStrategy::Auto};
    bool enableChange_{true};
    std::optional<Script> changeScript_;
    bool allowUnconfirmed_{false};
    int32_t minConfirmations_{0};
    uint32_t lockTime_{0};
    bool rbfEnabled_{false};
    
    /// Get available outputs
    std::vector<OutputGroup> GetAvailableOutputs() const;
    
    /// Create change output
    TxOut CreateChangeOutput(Amount amount);
};

// ============================================================================
// Wallet Events
// ============================================================================

/// Wallet event types
enum class WalletEvent {
    NewTransaction,     // New transaction added
    ConfirmedTransaction,  // Transaction confirmed
    OutputSpent,        // Our output was spent
    OutputReceived,     // New output received
    BalanceChanged,     // Balance changed
    AddressUsed,        // Address was used
    Locked,             // Wallet locked
    Unlocked            // Wallet unlocked
};

/// Wallet event callback
using WalletCallback = std::function<void(WalletEvent event, const std::string& data)>;

// ============================================================================
// Wallet Balance
// ============================================================================

/// Balance breakdown
struct WalletBalance {
    /// Confirmed balance
    Amount confirmed{0};
    
    /// Unconfirmed balance (trusted)
    Amount unconfirmed{0};
    
    /// Immature (coinbase not mature)
    Amount immature{0};
    
    /// Locked (user-locked outputs)
    Amount locked{0};
    
    /// Watch-only confirmed
    Amount watchOnlyConfirmed{0};
    
    /// Watch-only unconfirmed
    Amount watchOnlyUnconfirmed{0};
    
    /// Get total available (confirmed + unconfirmed)
    Amount GetAvailable() const { return confirmed + unconfirmed; }
    
    /// Get total balance
    Amount GetTotal() const { return confirmed + unconfirmed + immature; }
    
    /// Get spendable balance
    Amount GetSpendable() const { return confirmed + unconfirmed - locked; }
};

// ============================================================================
// Main Wallet Class
// ============================================================================

/**
 * Main wallet implementation for SHURIUM.
 * 
 * Features:
 * - HD key derivation (BIP32/BIP44)
 * - UTXO management
 * - Transaction building and signing
 * - Address book
 * - UBI claim creation
 */
class Wallet {
public:
    /// Wallet configuration
    struct Config {
        /// Wallet name
        std::string name;
        
        /// Gap limit for HD
        uint32_t gapLimit;
        
        /// Default fee rate
        FeeRate defaultFeeRate;
        
        /// Minimum change amount
        Amount minChange;
        
        /// Is testnet?
        bool testnet;
        
        /// Auto-lock timeout (seconds, 0 = disabled)
        uint32_t autoLockTimeout;
        
        /// Default constructor
        Config() : name("default"), gapLimit(20), defaultFeeRate(1), 
                   minChange(546), testnet(false), autoLockTimeout(300) {}
    };
    
    /// Create empty wallet
    Wallet();
    
    /// Create with config
    explicit Wallet(const Config& config);
    
    /// Create from mnemonic
    static std::unique_ptr<Wallet> FromMnemonic(const std::string& mnemonic,
                                                 const std::string& passphrase = "",
                                                 const std::string& password = "",
                                                 const Config& config = Config{});
    
    /// Generate new wallet
    static std::unique_ptr<Wallet> Generate(const std::string& password,
                                             Mnemonic::Strength strength = Mnemonic::Strength::Words24,
                                             const Config& config = {});
    
    /// Load from file
    static std::unique_ptr<Wallet> Load(const std::string& path);
    
    /// Load from file with config
    static std::unique_ptr<Wallet> Load(const std::string& path, const Config& config);
    
    /// Destructor
    ~Wallet();
    
    // === Initialization ===
    
    /// Check if wallet is initialized
    bool IsInitialized() const;
    
    /// Initialize from mnemonic
    bool Initialize(const std::string& mnemonic,
                    const std::string& passphrase = "",
                    const std::string& password = "");
    
    /// Initialize with existing keystore
    bool Initialize(std::unique_ptr<FileKeyStore> keystore);
    
    // === Lock/Unlock ===
    
    /// Check if wallet is locked
    bool IsLocked() const;
    
    /// Lock the wallet
    void Lock();
    
    /// Unlock with password
    bool Unlock(const std::string& password);
    
    /// Check password
    bool CheckPassword(const std::string& password) const;
    
    /// Change password
    bool ChangePassword(const std::string& oldPassword,
                        const std::string& newPassword);
    
    /// Encrypt wallet with password (first-time encryption)
    bool EncryptWallet(const std::string& passphrase);
    
    /// Check if wallet is encrypted
    bool IsEncrypted() const;
    
    // === Key Management ===
    
    /// Get keystore
    IKeyStore* GetKeyStore();
    const IKeyStore* GetKeyStore() const;
    
    /// Get HD key manager (requires unlock)
    HDKeyManager* GetHDKeyManager();
    
    /// Check if we have a key for script
    bool IsMine(const Script& script) const;
    
    /// Check if we have a key for output
    bool IsMine(const TxOut& txout) const;
    
    /// Get key hash from script (if P2PKH or P2WPKH)
    std::optional<Hash160> GetKeyHashFromScript(const Script& script) const;
    
    // === Address Management ===
    
    /// Get new receiving address
    std::string GetNewAddress(const std::string& label = "");
    
    /// Get new change address
    std::string GetChangeAddress();
    
    /// Get address for key hash
    std::string GetAddress(const Hash160& keyHash) const;
    
    /// Get all addresses
    std::vector<std::string> GetAddresses() const;
    
    /// Add to address book
    void AddAddressBookEntry(const std::string& address,
                             const std::string& label,
                             const std::string& purpose = "send");
    
    /// Get address book entries
    std::vector<AddressBookEntry> GetAddressBook() const;
    
    /// Look up address book entry
    std::optional<AddressBookEntry> LookupAddress(const std::string& address) const;
    
    // === Balance ===
    
    /// Get wallet balance
    WalletBalance GetBalance() const;
    
    /// Get balance at specific height
    WalletBalance GetBalanceAtHeight(int32_t height) const;
    
    // === UTXOs ===
    
    /// Get all wallet outputs
    std::vector<WalletOutput> GetOutputs() const;
    
    /// Get spendable outputs
    std::vector<WalletOutput> GetSpendableOutputs() const;
    
    /// Get unconfirmed outputs
    std::vector<WalletOutput> GetUnconfirmedOutputs() const;
    
    /// Lock an output (prevent spending)
    bool LockOutput(const OutPoint& outpoint);
    
    /// Unlock an output
    bool UnlockOutput(const OutPoint& outpoint);
    
    /// Get locked outputs
    std::vector<OutPoint> GetLockedOutputs() const;
    
    /// Check if output is locked
    bool IsLockedOutput(const OutPoint& outpoint) const;
    
    // === Transactions ===
    
    /// Get transaction builder
    TransactionBuilder CreateTransaction();
    
    /// Send to recipients
    BuildTxResult SendToRecipients(const std::vector<Recipient>& recipients,
                                    FeeRate feeRate = 1);
    
    /// Send to single address
    BuildTxResult SendToAddress(const std::string& address, Amount amount,
                                 FeeRate feeRate = 1);
    
    /// Sign a transaction
    bool SignTransaction(MutableTransaction& tx);
    
    /// Sign with specific signing provider
    bool SignTransaction(MutableTransaction& tx, const IKeyStore& keystore);
    
    /// Broadcast transaction (requires external connection)
    /// Returns txid on success
    std::optional<TxHash> BroadcastTransaction(const Transaction& tx);
    
    /// Get wallet transactions
    std::vector<WalletTransaction> GetTransactions() const;
    
    /// Get transaction by hash
    std::optional<WalletTransaction> GetTransaction(const TxHash& hash) const;
    
    /// Get recent transactions
    std::vector<WalletTransaction> GetRecentTransactions(size_t count = 10) const;
    
    // === Chain Sync ===
    
    /// Process a new block
    void ProcessBlock(const Block& block, int32_t height);
    
    /// Process a new transaction
    void ProcessTransaction(const TransactionRef& tx, int32_t height = -1);
    
    /// Handle block disconnection (reorg)
    void DisconnectBlock(const Block& block, int32_t height);
    
    /// Set current chain height
    void SetChainHeight(int32_t height);
    
    /// Get current chain height
    int32_t GetChainHeight() const;
    
    /// Rescan from height
    void RescanFrom(int32_t height);
    
    // === UBI Claims ===
    
    /// Check if identity is registered
    bool HasIdentity() const;
    
    /// Register identity
    bool RegisterIdentity(const identity::IdentitySecrets& secrets);
    
    /// Create UBI claim for the given epoch
    /// @param epoch The epoch to claim UBI for
    /// @param recipient The address to receive the UBI (empty = use wallet address)
    /// @return The UBI claim if successful, empty optional with error message otherwise
    std::pair<std::optional<economics::UBIClaim>, std::string> CreateUBIClaim(
        identity::EpochId epoch,
        const Hash160& recipient = Hash160());
    
    /// Get identity commitment
    Hash256 GetIdentityCommitment() const;
    
    // === Persistence ===
    
    /// Save wallet to file
    bool Save(const std::string& path);
    
    /// Save to current path
    bool Save();
    
    /// Get wallet path
    const std::string& GetPath() const { return path_; }
    
    // === Configuration ===
    
    /// Get config
    const Config& GetConfig() const { return config_; }
    
    /// Set default fee rate
    void SetDefaultFeeRate(FeeRate rate);
    
    // === Events ===
    
    /// Register event callback
    void OnEvent(WalletCallback callback);
    
    /// Get wallet name
    const std::string& GetName() const { return config_.name; }

private:
    /// Configuration
    Config config_;
    
    /// File path
    std::string path_;
    
    /// Thread safety
    mutable std::recursive_mutex mutex_;
    
    /// Key storage
    std::unique_ptr<FileKeyStore> keystore_;
    
    /// Wallet outputs (UTXOs)
    std::map<OutPoint, WalletOutput> outputs_;
    
    /// Wallet transactions
    std::map<TxHash, WalletTransaction> transactions_;
    
    /// Locked outputs
    std::set<OutPoint> lockedOutputs_;
    
    /// Address book
    std::map<std::string, AddressBookEntry> addressBook_;
    
    /// Current chain height
    std::atomic<int32_t> chainHeight_{0};
    
    /// Event callbacks
    std::vector<WalletCallback> callbacks_;
    
    /// Emit event
    void EmitEvent(WalletEvent event, const std::string& data = "");
    
    /// Process transaction for wallet relevance
    void ProcessWalletTransaction(const TransactionRef& tx, int32_t height);
    
    /// Add output to wallet
    void AddOutput(const OutPoint& outpoint, const TxOut& txout, int32_t height);
    
    /// Mark output as spent
    void SpendOutput(const OutPoint& outpoint, const TxHash& spendingTx);
    
    /// Update balance cache
    void UpdateBalance();
    
    /// Calculate fee for transaction
    Amount CalculateFee(const MutableTransaction& tx) const;
    
    /// Serialize wallet data
    std::vector<Byte> SerializeWalletData() const;
    
    /// Deserialize wallet data
    bool DeserializeWalletData(const std::vector<Byte>& data);
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Create P2PKH script from key hash
Script CreateP2PKHScript(const Hash160& keyHash);

/// Create P2WPKH script from key hash (SegWit)
Script CreateP2WPKHScript(const Hash160& keyHash);

/// Create P2SH script from script hash
Script CreateP2SHScript(const Hash160& scriptHash);

/// Extract key hash from P2PKH script
std::optional<Hash160> ExtractP2PKHKeyHash(const Script& script);

/// Extract key hash from P2WPKH script
std::optional<Hash160> ExtractP2WPKHKeyHash(const Script& script);

/// Get script type
enum class ScriptType {
    Unknown,
    P2PKH,      // Pay to public key hash
    P2PK,       // Pay to public key
    P2SH,       // Pay to script hash
    P2WPKH,     // Pay to witness public key hash
    P2WSH,      // Pay to witness script hash
    Multisig,   // Bare multisig
    NullData    // OP_RETURN data
};

ScriptType GetScriptType(const Script& script);

/// Estimate virtual size of transaction
size_t EstimateVirtualSize(size_t numInputs, size_t numOutputs,
                           bool segwit = true);

/// Estimate transaction fee
Amount EstimateTransactionFee(size_t numInputs, size_t numOutputs,
                               FeeRate feeRate, bool segwit = true);

/// Format amount for display
std::string FormatAmount(Amount amount, int decimals = 8);

/// Parse amount from string
std::optional<Amount> ParseAmount(const std::string& str);

} // namespace wallet
} // namespace shurium

#endif // SHURIUM_WALLET_WALLET_H
