// SHURIUM - Wallet Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/wallet/wallet.h>
#include <shurium/crypto/sha256.h>
#include <shurium/core/random.h>
#include <shurium/core/serialize.h>
#include <shurium/economics/ubi.h>
#include <shurium/util/fs.h>
#include <shurium/script/interpreter.h>

#include <fstream>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace shurium {
namespace wallet {

namespace {

// Helper to get wallet data file path from keystore path
// wallet.dat -> wallet_data.dat
std::string GetWalletDataPath(const std::string& keystorePath) {
    size_t dotPos = keystorePath.rfind('.');
    if (dotPos != std::string::npos) {
        return keystorePath.substr(0, dotPos) + "_data" + keystorePath.substr(dotPos);
    }
    return keystorePath + "_data";
}

} // anonymous namespace

// ============================================================================
// WalletOutput Implementation
// ============================================================================

bool WalletOutput::IsSpendable(int32_t currentHeight, int32_t maturity) const {
    if (status == OutputStatus::Locked || status == OutputStatus::Frozen ||
        status == OutputStatus::Spent) {
        return false;
    }
    
    if (coinbase && !IsMature(currentHeight, maturity)) {
        return false;
    }
    
    return true;
}

int32_t WalletOutput::GetDepth(int32_t currentHeight) const {
    if (height < 0) {
        return 0;  // Unconfirmed
    }
    return currentHeight - height + 1;
}

bool WalletOutput::IsMature(int32_t currentHeight, int32_t maturity) const {
    if (!coinbase) {
        return true;
    }
    return GetDepth(currentHeight) >= maturity;
}

OutputGroup WalletOutput::ToOutputGroup(FeeRate feeRate, int32_t currentHeight) const {
    return OutputGroup(outpoint, txout, feeRate, GetDepth(currentHeight), 
                       coinbase, timeReceived);
}

// ============================================================================
// TxConfirmation Implementation
// ============================================================================

int32_t TxConfirmation::GetDepth(int32_t currentHeight) const {
    if (blockHeight < 0) {
        return 0;
    }
    return currentHeight - blockHeight + 1;
}

// ============================================================================
// WalletTransaction Implementation
// ============================================================================

int32_t WalletTransaction::GetDepth(int32_t currentHeight) const {
    return confirmation.GetDepth(currentHeight);
}

Amount WalletTransaction::GetNetAmount() const {
    return GetCredit() - GetDebit();
}

Amount WalletTransaction::GetDebit() const {
    Amount debit = 0;
    for (size_t idx : ourInputs) {
        // Note: We need the actual input values, which would require
        // looking up the previous outputs. For now, return fee if available.
    }
    return fee > 0 ? fee : 0;
}

Amount WalletTransaction::GetCredit() const {
    Amount credit = 0;
    for (size_t idx : ourOutputs) {
        if (idx < tx->vout.size()) {
            credit += tx->vout[idx].nValue;
        }
    }
    return credit;
}

bool WalletTransaction::IsTrusted() const {
    // Confirmed transactions are trusted
    if (IsConfirmed()) {
        return true;
    }
    
    // Our own unconfirmed transactions are trusted
    if (fromMe) {
        return true;
    }
    
    return false;
}

// ============================================================================
// TransactionBuilder Implementation
// ============================================================================

TransactionBuilder::TransactionBuilder(Wallet& wallet) : wallet_(wallet) {}

TransactionBuilder& TransactionBuilder::AddRecipient(const Recipient& recipient) {
    recipients_.push_back(recipient);
    return *this;
}

TransactionBuilder& TransactionBuilder::AddRecipient(const Script& script, Amount amount) {
    recipients_.emplace_back(script, amount);
    return *this;
}

TransactionBuilder& TransactionBuilder::AddRecipient(const std::string& address, 
                                                      Amount amount) {
    auto recipient = Recipient::FromAddress(address, amount);
    if (recipient) {
        recipients_.push_back(*recipient);
    }
    return *this;
}

TransactionBuilder& TransactionBuilder::SetFeeRate(FeeRate rate) {
    feeRate_ = rate;
    absoluteFee_.reset();
    return *this;
}

TransactionBuilder& TransactionBuilder::SetAbsoluteFee(Amount fee) {
    absoluteFee_ = fee;
    return *this;
}

TransactionBuilder& TransactionBuilder::SetStrategy(SelectionStrategy strategy) {
    strategy_ = strategy;
    return *this;
}

TransactionBuilder& TransactionBuilder::EnableChange(bool enable) {
    enableChange_ = enable;
    return *this;
}

TransactionBuilder& TransactionBuilder::SetChangeAddress(const Script& changeScript) {
    changeScript_ = changeScript;
    return *this;
}

TransactionBuilder& TransactionBuilder::AllowUnconfirmed(bool allow) {
    allowUnconfirmed_ = allow;
    return *this;
}

TransactionBuilder& TransactionBuilder::SetMinConfirmations(int32_t minConf) {
    minConfirmations_ = minConf;
    return *this;
}

TransactionBuilder& TransactionBuilder::SetLockTime(uint32_t lockTime) {
    lockTime_ = lockTime;
    return *this;
}

TransactionBuilder& TransactionBuilder::SetRBF(bool enable) {
    rbfEnabled_ = enable;
    return *this;
}

BuildTxResult TransactionBuilder::Build() {
    BuildTxResult result;
    
    // Calculate total to send
    Amount totalToSend = 0;
    for (const auto& recipient : recipients_) {
        totalToSend += recipient.amount;
    }
    
    if (totalToSend <= 0) {
        result.error = "No recipients or invalid amounts";
        return result;
    }
    
    // Get available outputs
    auto availableOutputs = GetAvailableOutputs();
    if (availableOutputs.empty()) {
        result.error = "No spendable outputs available";
        return result;
    }
    
    // Setup coin selection parameters
    SelectionParams params;
    params.targetValue = totalToSend;
    params.feeRate = feeRate_;
    params.outputCount = recipients_.size();
    params.includeUnconfirmed = allowUnconfirmed_;
    params.minConfirmations = minConfirmations_;
    
    // Select coins
    CoinSelector selector(params);
    auto selection = selector.Select(availableOutputs, strategy_);
    
    if (!selection.success) {
        result.error = "Insufficient funds";
        return result;
    }
    
    result.algorithm = selection.algorithm;
    
    // Build transaction
    result.tx.version = 2;
    result.tx.nLockTime = lockTime_;
    
    // Add inputs
    for (const auto& group : selection.selected) {
        TxIn input(group.outpoint);
        if (rbfEnabled_) {
            input.nSequence = TxIn::MAX_SEQUENCE_NONFINAL;
        }
        result.tx.vin.push_back(input);
    }
    
    // Add outputs
    for (const auto& recipient : recipients_) {
        result.tx.vout.emplace_back(recipient.amount, recipient.scriptPubKey);
    }
    
    // Calculate fee
    result.fee = absoluteFee_.value_or(selector.CalculateFee(selection, recipients_.size()));
    
    // Calculate change
    result.change = selection.totalEffectiveValue - totalToSend - result.fee;
    
    // Add change output if needed
    if (result.change > wallet_.GetConfig().minChange && enableChange_) {
        Script changeScript;
        if (changeScript_) {
            changeScript = *changeScript_;
        } else {
            changeScript = CreateChangeOutput(result.change).scriptPubKey;
        }
        
        result.tx.vout.emplace_back(result.change, changeScript);
        result.changeIndex = static_cast<int32_t>(result.tx.vout.size() - 1);
    } else {
        // Add to fee if change is dust
        result.fee += result.change;
        result.change = 0;
        result.changeIndex = -1;
    }
    
    // Store selected inputs
    for (const auto& group : selection.selected) {
        WalletOutput wo(group.outpoint, group.output);
        wo.height = group.depth;  // Use height as depth approximation
        result.inputs.push_back(wo);
    }
    
    result.success = true;
    return result;
}

BuildTxResult TransactionBuilder::BuildAndSign() {
    auto result = Build();
    if (!result.success) {
        return result;
    }
    
    if (!wallet_.SignTransaction(result.tx)) {
        result.success = false;
        result.error = "Failed to sign transaction";
    }
    
    return result;
}

Amount TransactionBuilder::EstimateFee() const {
    Amount totalToSend = 0;
    for (const auto& recipient : recipients_) {
        totalToSend += recipient.amount;
    }
    
    // Estimate with average input count
    return EstimateTransactionFee(2, recipients_.size() + 1, feeRate_, true);
}

void TransactionBuilder::Clear() {
    recipients_.clear();
    feeRate_ = 1;
    absoluteFee_.reset();
    strategy_ = SelectionStrategy::Auto;
    enableChange_ = true;
    changeScript_.reset();
    allowUnconfirmed_ = false;
    minConfirmations_ = 0;
    lockTime_ = 0;
    rbfEnabled_ = false;
}

std::vector<OutputGroup> TransactionBuilder::GetAvailableOutputs() const {
    std::vector<OutputGroup> outputs;
    
    auto walletOutputs = wallet_.GetSpendableOutputs();
    int32_t currentHeight = wallet_.GetChainHeight();
    
    for (const auto& wo : walletOutputs) {
        outputs.push_back(wo.ToOutputGroup(feeRate_, currentHeight));
    }
    
    return outputs;
}

TxOut TransactionBuilder::CreateChangeOutput(Amount amount) {
    // Generate new change address
    std::string changeAddr = wallet_.GetChangeAddress();
    auto scriptData = DecodeAddress(changeAddr);
    Script changeScript(scriptData.begin(), scriptData.end());
    return TxOut(amount, changeScript);
}

// ============================================================================
// Recipient Implementation
// ============================================================================

std::optional<Recipient> Recipient::FromAddress(const std::string& address, Amount amount) {
    auto scriptData = DecodeAddress(address);
    if (scriptData.empty()) {
        return std::nullopt;
    }
    
    Script script(scriptData.begin(), scriptData.end());
    return Recipient(script, amount);
}

// ============================================================================
// Wallet Implementation
// ============================================================================

Wallet::Wallet() : config_() {}

Wallet::Wallet(const Config& config) : config_(config) {}

std::unique_ptr<Wallet> Wallet::FromMnemonic(const std::string& mnemonic,
                                               const std::string& passphrase,
                                               const std::string& password,
                                               const Config& config) {
    auto wallet = std::make_unique<Wallet>(config);
    if (!wallet->Initialize(mnemonic, passphrase, password)) {
        return nullptr;
    }
    return wallet;
}

std::unique_ptr<Wallet> Wallet::Generate(const std::string& password,
                                          Mnemonic::Strength strength,
                                          const Config& config) {
    std::string mnemonic = Mnemonic::Generate(strength);
    return FromMnemonic(mnemonic, "", password, config);
}

std::unique_ptr<Wallet> Wallet::Load(const std::string& path) {
    // Load with default config
    return Load(path, Config{});
}

std::unique_ptr<Wallet> Wallet::Load(const std::string& path, const Config& config) {
    auto wallet = std::make_unique<Wallet>(config);
    wallet->keystore_ = std::make_unique<FileKeyStore>(path);
    
    // Set testnet mode on keystore (needed for address generation)
    wallet->keystore_->SetTestnet(config.testnet);
    
    if (!wallet->keystore_->HasMasterSeed()) {
        return nullptr;
    }
    
    wallet->path_ = path;
    
    // Load wallet data (address book, transactions, outputs, etc.)
    std::string dataPath = GetWalletDataPath(path);
    std::ifstream file(dataPath, std::ios::binary);
    if (file) {
        std::vector<Byte> walletData((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
        if (!walletData.empty()) {
            wallet->DeserializeWalletData(walletData);
        }
    }
    // Note: It's OK if wallet data file doesn't exist (new wallet or migration)
    
    return wallet;
}

Wallet::~Wallet() = default;

bool Wallet::IsInitialized() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // Wallet is initialized if keystore exists and has HD key manager
    return keystore_ && keystore_->GetHDKeyManager() != nullptr;
}

bool Wallet::Initialize(const std::string& mnemonic,
                         const std::string& passphrase,
                         const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Create keystore
    keystore_ = std::make_unique<FileKeyStore>();
    
    // Set testnet mode before initializing HD manager
    keystore_->SetTestnet(config_.testnet);
    
    if (!password.empty()) {
        keystore_->SetupEncryption(password);
    }
    
    // Set from mnemonic
    if (!keystore_->SetFromMnemonic(mnemonic, passphrase)) {
        return false;
    }
    
    return true;
}

bool Wallet::Initialize(std::unique_ptr<FileKeyStore> keystore) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    keystore_ = std::move(keystore);
    return keystore_ != nullptr;
}

bool Wallet::IsLocked() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return keystore_ && keystore_->IsLocked();
}

void Wallet::Lock() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (keystore_) {
        keystore_->Lock();
        EmitEvent(WalletEvent::Locked);
    }
}

bool Wallet::Unlock(const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (keystore_ && keystore_->Unlock(password)) {
        EmitEvent(WalletEvent::Unlocked);
        return true;
    }
    return false;
}

bool Wallet::CheckPassword(const std::string& password) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return keystore_ && keystore_->CheckPassword(password);
}

bool Wallet::ChangePassword(const std::string& oldPassword,
                             const std::string& newPassword) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return keystore_ && keystore_->ChangePassword(oldPassword, newPassword);
}

bool Wallet::EncryptWallet(const std::string& passphrase) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (!keystore_) {
        return false;
    }
    
    // Check if already encrypted
    if (keystore_->IsEncrypted()) {
        return false;
    }
    
    // Cast to FileKeyStore to access SetupEncryption
    auto* fileKeystore = dynamic_cast<FileKeyStore*>(keystore_.get());
    if (!fileKeystore) {
        return false;
    }
    
    // Setup encryption
    if (!fileKeystore->SetupEncryption(passphrase)) {
        return false;
    }
    
    return true;
}

bool Wallet::IsEncrypted() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return keystore_ && keystore_->IsEncrypted();
}

IKeyStore* Wallet::GetKeyStore() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return keystore_.get();
}

const IKeyStore* Wallet::GetKeyStore() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return keystore_.get();
}

HDKeyManager* Wallet::GetHDKeyManager() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (keystore_) {
        return keystore_->GetHDKeyManager();
    }
    return nullptr;
}

bool Wallet::IsMine(const Script& script) const {
    auto keyHash = GetKeyHashFromScript(script);
    if (!keyHash) {
        return false;
    }
    return keystore_ && keystore_->HaveKey(*keyHash);
}

bool Wallet::IsMine(const TxOut& txout) const {
    return IsMine(txout.scriptPubKey);
}

std::optional<Hash160> Wallet::GetKeyHashFromScript(const Script& script) const {
    // Try P2WPKH
    auto hash = ExtractP2WPKHKeyHash(script);
    if (hash) {
        return hash;
    }
    
    // Try P2PKH
    return ExtractP2PKHKeyHash(script);
}

std::string Wallet::GetNewAddress(const std::string& label) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (!keystore_) {
        return "";
    }
    
    // Use keystore's DeriveNextReceiving to properly track indices for persistence
    auto pubKey = keystore_->DeriveNextReceiving();
    if (!pubKey) {
        return "";
    }
    
    std::string address = EncodeP2WPKH(pubKey->GetHash160(), config_.testnet);
    
    if (!label.empty() && !address.empty()) {
        AddAddressBookEntry(address, label, "receive");
    }
    
    // Auto-save wallet to persist the new HD index
    if (!path_.empty()) {
        Save(path_);
    }
    
    return address;
}

std::string Wallet::GetChangeAddress() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (!keystore_) {
        return "";
    }
    
    // Use keystore's DeriveNextChange to properly track indices for persistence
    auto pubKey = keystore_->DeriveNextChange();
    if (!pubKey) {
        return "";
    }
    
    std::string address = EncodeP2WPKH(pubKey->GetHash160(), config_.testnet);
    
    // Auto-save wallet to persist the new HD index
    if (!path_.empty()) {
        Save(path_);
    }
    
    return address;
}

std::string Wallet::GetAddress(const Hash160& keyHash) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return EncodeP2WPKH(keyHash, config_.testnet);
}

std::vector<std::string> Wallet::GetAddresses() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::vector<std::string> addresses;
    
    if (keystore_) {
        for (const auto& hash : keystore_->GetKeyHashes()) {
            addresses.push_back(GetAddress(hash));
        }
    }
    
    return addresses;
}

void Wallet::AddAddressBookEntry(const std::string& address,
                                  const std::string& label,
                                  const std::string& purpose) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    addressBook_[address] = AddressBookEntry(address, label, purpose);
}

std::vector<AddressBookEntry> Wallet::GetAddressBook() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::vector<AddressBookEntry> entries;
    entries.reserve(addressBook_.size());
    
    for (const auto& [addr, entry] : addressBook_) {
        entries.push_back(entry);
    }
    
    return entries;
}

std::optional<AddressBookEntry> Wallet::LookupAddress(const std::string& address) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = addressBook_.find(address);
    if (it != addressBook_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

WalletBalance Wallet::GetBalance() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    WalletBalance balance;
    int32_t currentHeight = chainHeight_.load();
    
    for (const auto& [outpoint, output] : outputs_) {
        if (output.status == OutputStatus::Spent) {
            continue;
        }
        
        Amount value = output.GetValue();
        bool isWatchOnly = keystore_ && keystore_->IsWatchOnly(output.keyHash);
        
        if (output.status == OutputStatus::Locked) {
            balance.locked += value;
        } else if (!output.IsMature(currentHeight)) {
            balance.immature += value;
        } else if (output.height < 0) {
            if (isWatchOnly) {
                balance.watchOnlyUnconfirmed += value;
            } else {
                balance.unconfirmed += value;
            }
        } else {
            if (isWatchOnly) {
                balance.watchOnlyConfirmed += value;
            } else {
                balance.confirmed += value;
            }
        }
    }
    
    return balance;
}

WalletBalance Wallet::GetBalanceAtHeight(int32_t height) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    WalletBalance balance;
    
    for (const auto& [outpoint, output] : outputs_) {
        if (output.height > height) {
            continue;  // Created after target height
        }
        
        if (output.status == OutputStatus::Spent) {
            continue;
        }
        
        Amount value = output.GetValue();
        
        if (output.height < 0) {
            balance.unconfirmed += value;
        } else {
            balance.confirmed += value;
        }
    }
    
    return balance;
}

std::vector<WalletOutput> Wallet::GetOutputs() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::vector<WalletOutput> result;
    result.reserve(outputs_.size());
    
    for (const auto& [outpoint, output] : outputs_) {
        result.push_back(output);
    }
    
    return result;
}

std::vector<WalletOutput> Wallet::GetSpendableOutputs() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::vector<WalletOutput> result;
    int32_t currentHeight = chainHeight_.load();
    
    for (const auto& [outpoint, output] : outputs_) {
        if (output.IsSpendable(currentHeight)) {
            if (!IsLockedOutput(outpoint)) {
                result.push_back(output);
            }
        }
    }
    
    return result;
}

std::vector<WalletOutput> Wallet::GetUnconfirmedOutputs() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::vector<WalletOutput> result;
    
    for (const auto& [outpoint, output] : outputs_) {
        if (output.height < 0 && output.status != OutputStatus::Spent) {
            result.push_back(output);
        }
    }
    
    return result;
}

bool Wallet::LockOutput(const OutPoint& outpoint) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    lockedOutputs_.insert(outpoint);
    return true;
}

bool Wallet::UnlockOutput(const OutPoint& outpoint) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return lockedOutputs_.erase(outpoint) > 0;
}

std::vector<OutPoint> Wallet::GetLockedOutputs() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return std::vector<OutPoint>(lockedOutputs_.begin(), lockedOutputs_.end());
}

bool Wallet::IsLockedOutput(const OutPoint& outpoint) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return lockedOutputs_.count(outpoint) > 0;
}

TransactionBuilder Wallet::CreateTransaction() {
    return TransactionBuilder(*this);
}

BuildTxResult Wallet::SendToRecipients(const std::vector<Recipient>& recipients,
                                        FeeRate feeRate) {
    auto builder = CreateTransaction();
    builder.SetFeeRate(feeRate);
    
    for (const auto& recipient : recipients) {
        builder.AddRecipient(recipient);
    }
    
    return builder.BuildAndSign();
}

BuildTxResult Wallet::SendToAddress(const std::string& address, Amount amount,
                                     FeeRate feeRate) {
    auto recipient = Recipient::FromAddress(address, amount);
    if (!recipient) {
        BuildTxResult result;
        result.error = "Invalid address";
        return result;
    }
    
    return SendToRecipients({*recipient}, feeRate);
}

bool Wallet::SignTransaction(MutableTransaction& tx) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (!keystore_) {
        return false;
    }
    
    return SignTransaction(tx, *keystore_);
}

bool Wallet::SignTransaction(MutableTransaction& tx, const IKeyStore& keystore) {
    // For each input, find the key and sign with proper SIGHASH handling
    
    // First, convert to Transaction for signature hash computation
    Transaction txCopy(tx);
    
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        // Get the previous output
        auto it = outputs_.find(tx.vin[i].prevout);
        if (it == outputs_.end()) {
            continue;  // Not our output
        }
        
        const auto& prevOut = it->second;
        
        // Get the key
        auto key = keystore.GetKey(prevOut.keyHash);
        if (!key) {
            return false;  // Can't sign without key
        }
        
        // Use the previous output's scriptPubKey for signature hash
        // For P2PKH: OP_DUP OP_HASH160 <pubKeyHash> OP_EQUALVERIFY OP_CHECKSIG
        const Script& scriptCode = prevOut.txout.scriptPubKey;
        
        // Compute proper signature hash using SIGHASH_ALL
        constexpr uint8_t nHashType = SIGHASH_ALL;
        Hash256 sigHash = SignatureHash(txCopy, static_cast<unsigned int>(i), scriptCode, nHashType);
        
        // Sign the hash
        auto signature = key->Sign(sigHash);
        if (signature.empty()) {
            return false;
        }
        
        // Append SIGHASH type to signature
        signature.push_back(nHashType);
        
        // Build scriptSig for P2PKH: <sig> <pubkey>
        Script scriptSig;
        scriptSig << signature;
        
        // Get public key bytes
        std::vector<uint8_t> pubkeyData = key->GetPublicKey().ToVector();
        scriptSig << pubkeyData;
        
        tx.vin[i].scriptSig = scriptSig;
    }
    
    return true;
}

std::optional<TxHash> Wallet::BroadcastTransaction(const Transaction& tx) {
    // This would need network connection
    // For now, just return the hash as if broadcast succeeded
    return tx.GetHash();
}

std::vector<WalletTransaction> Wallet::GetTransactions() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::vector<WalletTransaction> result;
    result.reserve(transactions_.size());
    
    for (const auto& [hash, tx] : transactions_) {
        result.push_back(tx);
    }
    
    // Sort by time (newest first)
    std::sort(result.begin(), result.end(),
              [](const WalletTransaction& a, const WalletTransaction& b) {
                  return a.timeReceived > b.timeReceived;
              });
    
    return result;
}

std::optional<WalletTransaction> Wallet::GetTransaction(const TxHash& hash) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = transactions_.find(hash);
    if (it != transactions_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::vector<WalletTransaction> Wallet::GetRecentTransactions(size_t count) const {
    auto txs = GetTransactions();
    if (txs.size() > count) {
        txs.resize(count);
    }
    return txs;
}

void Wallet::ProcessBlock(const Block& block, int32_t height) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    chainHeight_.store(height);
    
    for (const auto& tx : block.vtx) {
        ProcessWalletTransaction(tx, height);
    }
}

void Wallet::ProcessTransaction(const TransactionRef& tx, int32_t height) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ProcessWalletTransaction(tx, height);
}

void Wallet::DisconnectBlock(const Block& block, int32_t height) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Remove outputs created in this block
    for (const auto& tx : block.vtx) {
        const auto& hash = tx->GetHash();
        
        for (uint32_t i = 0; i < tx->vout.size(); ++i) {
            OutPoint outpoint(hash, i);
            outputs_.erase(outpoint);
        }
        
        // Mark transaction as unconfirmed
        auto it = transactions_.find(hash);
        if (it != transactions_.end()) {
            it->second.confirmation.blockHeight = -1;
        }
    }
    
    chainHeight_.store(height - 1);
}

void Wallet::SetChainHeight(int32_t height) {
    chainHeight_.store(height);
}

int32_t Wallet::GetChainHeight() const {
    return chainHeight_.load();
}

void Wallet::RescanFrom(int32_t height) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Clear outputs created after height
    for (auto it = outputs_.begin(); it != outputs_.end(); ) {
        if (it->second.height > height) {
            it = outputs_.erase(it);
        } else {
            ++it;
        }
    }
}

bool Wallet::HasIdentity() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return keystore_ && keystore_->HasIdentity();
}

bool Wallet::RegisterIdentity(const identity::IdentitySecrets& secrets) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (!keystore_) {
        return false;
    }
    
    return keystore_->SetIdentitySecrets(secrets);
}

std::pair<std::optional<economics::UBIClaim>, std::string> Wallet::CreateUBIClaim(
    identity::EpochId epoch,
    const Hash160& recipient) {
    
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Check if wallet has identity secrets
    if (!keystore_) {
        return {std::nullopt, "Wallet not initialized"};
    }
    
    auto secretsOpt = keystore_->GetIdentitySecrets();
    if (!secretsOpt) {
        return {std::nullopt, "No identity registered in wallet"};
    }
    
    const auto& secrets = *secretsOpt;
    
    // Determine recipient address
    Hash160 actualRecipient = recipient;
    if (actualRecipient == Hash160()) {
        // Use wallet's first address
        auto addresses = GetAddresses();
        if (addresses.empty()) {
            return {std::nullopt, "No recipient address available"};
        }
        
        // Decode first address to get the hash
        // For simplicity, we get the key hash from keystore
        auto keyHashes = keystore_->GetKeyHashes();
        if (keyHashes.empty()) {
            return {std::nullopt, "No keys available for recipient"};
        }
        actualRecipient = keyHashes[0];
    }
    
    // Create the UBI claim
    // Note: In production, we would need a membership proof from the identity tree
    // For now, we create a claim without the full proof (placeholder)
    identity::VectorCommitment::MerkleProof emptyProof;
    
    economics::UBIClaim claim = economics::UBIClaim::Create(
        epoch,
        secrets,
        actualRecipient,
        emptyProof
    );
    
    return {claim, ""};
}

Hash256 Wallet::GetIdentityCommitment() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (keystore_) {
        return keystore_->GetIdentityCommitment();
    }
    
    return Hash256();
}

bool Wallet::Save(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Save keystore (keys, HD seed, etc.)
    if (keystore_) {
        if (!keystore_->Save(path)) {
            return false;
        }
    }
    
    // Save wallet data (address book, transactions, outputs, etc.)
    std::string dataPath = GetWalletDataPath(path);
    auto walletData = SerializeWalletData();
    
    std::ofstream file(dataPath, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(walletData.data()), walletData.size());
    if (!file.good()) {
        return false;
    }
    
    path_ = path;
    return true;
}

bool Wallet::Save() {
    if (path_.empty()) {
        return false;
    }
    return Save(path_);
}

void Wallet::SetDefaultFeeRate(FeeRate rate) {
    config_.defaultFeeRate = rate;
}

void Wallet::OnEvent(WalletCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

void Wallet::EmitEvent(WalletEvent event, const std::string& data) {
    for (const auto& callback : callbacks_) {
        callback(event, data);
    }
}

void Wallet::ProcessWalletTransaction(const TransactionRef& tx, int32_t height) {
    const auto& hash = tx->GetHash();
    
    WalletTransaction wtx(tx);
    wtx.timeReceived = std::chrono::system_clock::now().time_since_epoch().count();
    
    if (height >= 0) {
        wtx.confirmation.blockHeight = height;
    }
    
    bool isRelevant = false;
    
    // Check inputs (spending our outputs)
    for (size_t i = 0; i < tx->vin.size(); ++i) {
        const auto& input = tx->vin[i];
        auto it = outputs_.find(input.prevout);
        
        if (it != outputs_.end()) {
            wtx.ourInputs.push_back(i);
            wtx.fromMe = true;
            isRelevant = true;
            
            // Mark as spent
            SpendOutput(input.prevout, hash);
        }
    }
    
    // Check outputs (receiving to our addresses)
    for (size_t i = 0; i < tx->vout.size(); ++i) {
        const auto& output = tx->vout[i];
        
        if (IsMine(output)) {
            wtx.ourOutputs.push_back(i);
            isRelevant = true;
            
            OutPoint outpoint(hash, static_cast<uint32_t>(i));
            AddOutput(outpoint, output, height);
        }
    }
    
    if (isRelevant) {
        transactions_[hash] = wtx;
        EmitEvent(WalletEvent::NewTransaction, BytesToHex(hash.data(), hash.size()));
        EmitEvent(WalletEvent::BalanceChanged);
    }
}

void Wallet::AddOutput(const OutPoint& outpoint, const TxOut& txout, int32_t height) {
    WalletOutput wo(outpoint, txout, height);
    wo.timeReceived = std::chrono::system_clock::now().time_since_epoch().count();
    wo.status = (height >= 0) ? OutputStatus::Available : OutputStatus::Unconfirmed;
    
    // Get key hash
    auto keyHash = GetKeyHashFromScript(txout.scriptPubKey);
    if (keyHash) {
        wo.keyHash = *keyHash;
    }
    
    outputs_[outpoint] = wo;
    EmitEvent(WalletEvent::OutputReceived, outpoint.ToString());
}

void Wallet::SpendOutput(const OutPoint& outpoint, const TxHash& spendingTx) {
    auto it = outputs_.find(outpoint);
    if (it != outputs_.end()) {
        it->second.status = OutputStatus::Spent;
        EmitEvent(WalletEvent::OutputSpent, outpoint.ToString());
    }
}

void Wallet::UpdateBalance() {
    // Balance is calculated on demand in GetBalance()
}

Amount Wallet::CalculateFee(const MutableTransaction& tx) const {
    // Sum inputs
    Amount inputSum = 0;
    for (const auto& input : tx.vin) {
        auto it = outputs_.find(input.prevout);
        if (it != outputs_.end()) {
            inputSum += it->second.GetValue();
        }
    }
    
    // Sum outputs
    Amount outputSum = 0;
    for (const auto& output : tx.vout) {
        outputSum += output.nValue;
    }
    
    return inputSum - outputSum;
}

std::vector<Byte> Wallet::SerializeWalletData() const {
    DataStream stream;
    
    // Wallet data format version
    constexpr uint32_t WALLET_DATA_VERSION = 1;
    Serialize(stream, WALLET_DATA_VERSION);
    
    // Config
    Serialize(stream, config_.name);
    Serialize(stream, config_.gapLimit);
    Serialize(stream, static_cast<int64_t>(config_.defaultFeeRate));
    Serialize(stream, config_.minChange);
    Serialize(stream, config_.testnet);
    Serialize(stream, config_.autoLockTimeout);
    
    // Chain height
    Serialize(stream, chainHeight_.load());
    
    // === Serialize Outputs (UTXOs) ===
    uint32_t outputCount = static_cast<uint32_t>(outputs_.size());
    Serialize(stream, outputCount);
    
    for (const auto& [outpoint, output] : outputs_) {
        // OutPoint
        Serialize(stream, outpoint);
        
        // TxOut
        Serialize(stream, output.txout);
        
        // Metadata
        Serialize(stream, output.height);
        Serialize(stream, output.coinbase);
        Serialize(stream, output.timeReceived);
        Serialize(stream, static_cast<uint8_t>(output.status));
        Serialize(stream, output.keyHash);
        
        // Optional derivation path
        bool hasPath = output.keyPath.has_value();
        Serialize(stream, hasPath);
        if (hasPath) {
            Serialize(stream, output.keyPath->ToString());
        }
        
        Serialize(stream, output.label);
    }
    
    // === Serialize Transactions ===
    uint32_t txCount = static_cast<uint32_t>(transactions_.size());
    Serialize(stream, txCount);
    
    for (const auto& [hash, wtx] : transactions_) {
        // Transaction hash (key)
        Serialize(stream, hash);
        
        // Serialize the transaction if we have it
        bool hasTx = (wtx.tx != nullptr);
        Serialize(stream, hasTx);
        if (hasTx) {
            // Serialize as MutableTransaction then convert
            MutableTransaction mtx(*wtx.tx);
            Serialize(stream, mtx);
        }
        
        // Confirmation info
        Serialize(stream, wtx.confirmation.blockHeight);
        Serialize(stream, wtx.confirmation.blockHash);
        Serialize(stream, wtx.confirmation.txIndex);
        Serialize(stream, wtx.confirmation.blockTime);
        
        // Timestamps
        Serialize(stream, wtx.timeReceived);
        Serialize(stream, wtx.timeCreated);
        
        // Flags
        Serialize(stream, wtx.fromMe);
        
        // Our inputs/outputs indices - serialize as uint32_t for cross-platform compatibility
        uint32_t inputCount = static_cast<uint32_t>(wtx.ourInputs.size());
        Serialize(stream, inputCount);
        for (size_t idx : wtx.ourInputs) {
            uint32_t idx32 = static_cast<uint32_t>(idx);
            Serialize(stream, idx32);
        }
        
        uint32_t outputCount = static_cast<uint32_t>(wtx.ourOutputs.size());
        Serialize(stream, outputCount);
        for (size_t idx : wtx.ourOutputs) {
            uint32_t idx32 = static_cast<uint32_t>(idx);
            Serialize(stream, idx32);
        }
        
        // Fee and label
        Serialize(stream, wtx.fee);
        Serialize(stream, wtx.label);
    }
    
    // === Serialize Locked Outputs ===
    uint32_t lockedCount = static_cast<uint32_t>(lockedOutputs_.size());
    Serialize(stream, lockedCount);
    
    for (const auto& outpoint : lockedOutputs_) {
        Serialize(stream, outpoint);
    }
    
    // === Serialize Address Book ===
    uint32_t addressBookCount = static_cast<uint32_t>(addressBook_.size());
    Serialize(stream, addressBookCount);
    
    for (const auto& [address, entry] : addressBook_) {
        Serialize(stream, entry.address);
        Serialize(stream, entry.label);
        Serialize(stream, entry.purpose);
        Serialize(stream, entry.created);
    }
    
    // Return the serialized data
    return std::vector<Byte>(stream.begin(), stream.end());
}

bool Wallet::DeserializeWalletData(const std::vector<Byte>& data) {
    if (data.empty()) {
        return true;  // Empty data is valid (new wallet)
    }
    
    try {
        DataStream stream(data);
        
        // Check version
        uint32_t version;
        Unserialize(stream, version);
        if (version > 1) {
            return false;  // Unsupported version
        }
        
        // Config
        Unserialize(stream, config_.name);
        Unserialize(stream, config_.gapLimit);
        int64_t feeRate;
        Unserialize(stream, feeRate);
        config_.defaultFeeRate = static_cast<FeeRate>(feeRate);
        Unserialize(stream, config_.minChange);
        Unserialize(stream, config_.testnet);
        Unserialize(stream, config_.autoLockTimeout);
        
        // Chain height
        int32_t height;
        Unserialize(stream, height);
        chainHeight_.store(height);
        
        // === Deserialize Outputs ===
        outputs_.clear();
        uint32_t outputCount;
        Unserialize(stream, outputCount);
        
        for (uint32_t i = 0; i < outputCount; ++i) {
            OutPoint outpoint;
            Unserialize(stream, outpoint);
            
            WalletOutput output;
            output.outpoint = outpoint;
            
            Unserialize(stream, output.txout);
            Unserialize(stream, output.height);
            Unserialize(stream, output.coinbase);
            Unserialize(stream, output.timeReceived);
            
            uint8_t status;
            Unserialize(stream, status);
            output.status = static_cast<OutputStatus>(status);
            
            Unserialize(stream, output.keyHash);
            
            bool hasPath;
            Unserialize(stream, hasPath);
            if (hasPath) {
                std::string pathStr;
                Unserialize(stream, pathStr);
                output.keyPath = DerivationPath::FromString(pathStr);
            }
            
            Unserialize(stream, output.label);
            
            outputs_[outpoint] = std::move(output);
        }
        
        // === Deserialize Transactions ===
        transactions_.clear();
        uint32_t txCount;
        Unserialize(stream, txCount);
        
        for (uint32_t i = 0; i < txCount; ++i) {
            TxHash hash;
            Unserialize(stream, hash);
            
            WalletTransaction wtx;
            
            bool hasTx;
            Unserialize(stream, hasTx);
            if (hasTx) {
                MutableTransaction mtx;
                Unserialize(stream, mtx);
                wtx.tx = std::make_shared<Transaction>(mtx);
            }
            
            Unserialize(stream, wtx.confirmation.blockHeight);
            Unserialize(stream, wtx.confirmation.blockHash);
            Unserialize(stream, wtx.confirmation.txIndex);
            Unserialize(stream, wtx.confirmation.blockTime);
            
            Unserialize(stream, wtx.timeReceived);
            Unserialize(stream, wtx.timeCreated);
            Unserialize(stream, wtx.fromMe);
            
            // Deserialize our inputs/outputs indices
            uint32_t inputCount;
            Unserialize(stream, inputCount);
            wtx.ourInputs.clear();
            wtx.ourInputs.reserve(inputCount);
            for (uint32_t j = 0; j < inputCount; ++j) {
                uint32_t idx;
                Unserialize(stream, idx);
                wtx.ourInputs.push_back(static_cast<size_t>(idx));
            }
            
            uint32_t outputIndicesCount;
            Unserialize(stream, outputIndicesCount);
            wtx.ourOutputs.clear();
            wtx.ourOutputs.reserve(outputIndicesCount);
            for (uint32_t j = 0; j < outputIndicesCount; ++j) {
                uint32_t idx;
                Unserialize(stream, idx);
                wtx.ourOutputs.push_back(static_cast<size_t>(idx));
            }
            
            Unserialize(stream, wtx.fee);
            Unserialize(stream, wtx.label);
            
            transactions_[hash] = std::move(wtx);
        }
        
        // === Deserialize Locked Outputs ===
        lockedOutputs_.clear();
        uint32_t lockedCount;
        Unserialize(stream, lockedCount);
        
        for (uint32_t i = 0; i < lockedCount; ++i) {
            OutPoint outpoint;
            Unserialize(stream, outpoint);
            lockedOutputs_.insert(outpoint);
        }
        
        // === Deserialize Address Book ===
        addressBook_.clear();
        uint32_t addressBookCount;
        Unserialize(stream, addressBookCount);
        
        for (uint32_t i = 0; i < addressBookCount; ++i) {
            AddressBookEntry entry;
            Unserialize(stream, entry.address);
            Unserialize(stream, entry.label);
            Unserialize(stream, entry.purpose);
            Unserialize(stream, entry.created);
            
            addressBook_[entry.address] = std::move(entry);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        // Deserialization failed
        return false;
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

Script CreateP2PKHScript(const Hash160& keyHash) {
    Script script;
    script.push_back(0x76);  // OP_DUP
    script.push_back(0xa9);  // OP_HASH160
    script.push_back(0x14);  // Push 20 bytes
    script.insert(script.end(), keyHash.begin(), keyHash.end());
    script.push_back(0x88);  // OP_EQUALVERIFY
    script.push_back(0xac);  // OP_CHECKSIG
    return script;
}

Script CreateP2WPKHScript(const Hash160& keyHash) {
    Script script;
    script.push_back(0x00);  // OP_0 (witness version)
    script.push_back(0x14);  // Push 20 bytes
    script.insert(script.end(), keyHash.begin(), keyHash.end());
    return script;
}

Script CreateP2SHScript(const Hash160& scriptHash) {
    Script script;
    script.push_back(0xa9);  // OP_HASH160
    script.push_back(0x14);  // Push 20 bytes
    script.insert(script.end(), scriptHash.begin(), scriptHash.end());
    script.push_back(0x87);  // OP_EQUAL
    return script;
}

std::optional<Hash160> ExtractP2PKHKeyHash(const Script& script) {
    // P2PKH: OP_DUP OP_HASH160 <20 bytes> OP_EQUALVERIFY OP_CHECKSIG
    if (script.size() == 25 && 
        script[0] == 0x76 && script[1] == 0xa9 && script[2] == 0x14 &&
        script[23] == 0x88 && script[24] == 0xac) {
        Hash160 hash;
        std::memcpy(hash.data(), script.data() + 3, 20);
        return hash;
    }
    return std::nullopt;
}

std::optional<Hash160> ExtractP2WPKHKeyHash(const Script& script) {
    // P2WPKH: OP_0 <20 bytes>
    if (script.size() == 22 && script[0] == 0x00 && script[1] == 0x14) {
        Hash160 hash;
        std::memcpy(hash.data(), script.data() + 2, 20);
        return hash;
    }
    return std::nullopt;
}

ScriptType GetScriptType(const Script& script) {
    if (script.empty()) {
        return ScriptType::Unknown;
    }
    
    // P2PKH
    if (script.size() == 25 && script[0] == 0x76 && script[1] == 0xa9) {
        return ScriptType::P2PKH;
    }
    
    // P2SH
    if (script.size() == 23 && script[0] == 0xa9) {
        return ScriptType::P2SH;
    }
    
    // P2WPKH
    if (script.size() == 22 && script[0] == 0x00 && script[1] == 0x14) {
        return ScriptType::P2WPKH;
    }
    
    // P2WSH
    if (script.size() == 34 && script[0] == 0x00 && script[1] == 0x20) {
        return ScriptType::P2WSH;
    }
    
    // OP_RETURN
    if (!script.empty() && script[0] == 0x6a) {
        return ScriptType::NullData;
    }
    
    return ScriptType::Unknown;
}

size_t EstimateVirtualSize(size_t numInputs, size_t numOutputs, bool segwit) {
    if (segwit) {
        // SegWit: weight = base_size * 3 + total_size
        // vsize = weight / 4
        size_t baseSize = 10 + numInputs * 41 + numOutputs * 31;
        size_t witnessSize = numInputs * 107;  // Average signature
        return baseSize + (witnessSize + 3) / 4;
    } else {
        return 10 + numInputs * 148 + numOutputs * 34;
    }
}

Amount EstimateTransactionFee(size_t numInputs, size_t numOutputs,
                               FeeRate feeRate, bool segwit) {
    size_t vsize = EstimateVirtualSize(numInputs, numOutputs, segwit);
    return static_cast<Amount>(vsize * feeRate);
}

std::string FormatAmount(Amount amount, int decimals) {
    bool negative = amount < 0;
    if (negative) amount = -amount;
    
    std::ostringstream oss;
    
    int64_t whole = amount / 100000000;
    int64_t frac = amount % 100000000;
    
    oss << (negative ? "-" : "") << whole << ".";
    oss << std::setfill('0') << std::setw(8) << frac;
    
    std::string result = oss.str();
    
    // Trim trailing zeros
    if (decimals < 8) {
        size_t dotPos = result.find('.');
        if (dotPos != std::string::npos) {
            result = result.substr(0, dotPos + decimals + 1);
        }
    }
    
    return result;
}

std::optional<Amount> ParseAmount(const std::string& str) {
    std::string s = str;
    
    // Remove commas
    s.erase(std::remove(s.begin(), s.end(), ','), s.end());
    
    // Handle negative
    bool negative = false;
    if (!s.empty() && s[0] == '-') {
        negative = true;
        s = s.substr(1);
    }
    
    // Split by decimal point
    size_t dotPos = s.find('.');
    std::string wholePart = (dotPos != std::string::npos) ? s.substr(0, dotPos) : s;
    std::string fracPart = (dotPos != std::string::npos) ? s.substr(dotPos + 1) : "";
    
    // Pad or truncate fractional part
    if (fracPart.size() > 8) {
        fracPart = fracPart.substr(0, 8);
    }
    while (fracPart.size() < 8) {
        fracPart += '0';
    }
    
    try {
        int64_t whole = wholePart.empty() ? 0 : std::stoll(wholePart);
        int64_t frac = std::stoll(fracPart);
        
        Amount amount = whole * 100000000 + frac;
        return negative ? -amount : amount;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace wallet
} // namespace shurium
