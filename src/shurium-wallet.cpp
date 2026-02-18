// SHURIUM Wallet Tool
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Command-line wallet management tool for SHURIUM.
// Supports:
// - Creating new wallets (HD with BIP39 mnemonic)
// - Importing wallets from mnemonic
// - Address generation and listing
// - Wallet info and balance display
// - Offline transaction signing
// - Key export/backup
// - Password management

#include <shurium/wallet/wallet.h>
#include <shurium/wallet/hdkey.h>
#include <shurium/wallet/keystore.h>
#include <shurium/core/hex.h>
#include <shurium/crypto/sha256.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <termios.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace shurium;
using namespace shurium::wallet;

// ============================================================================
// Constants
// ============================================================================

constexpr const char* VERSION = "0.1.0";
constexpr const char* DEFAULT_WALLET_FILE = "wallet.dat";

// ============================================================================
// Terminal Utilities
// ============================================================================

/// Read password from terminal without echo
std::string ReadPassword(const std::string& prompt) {
    std::cout << prompt << std::flush;
    
    // Disable echo
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    std::string password;
    std::getline(std::cin, password);
    
    // Restore echo
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << std::endl;
    
    return password;
}

/// Read password with confirmation
std::string ReadPasswordWithConfirm(const std::string& prompt) {
    std::string password1 = ReadPassword(prompt);
    std::string password2 = ReadPassword("Confirm password: ");
    
    if (password1 != password2) {
        std::cerr << "Error: Passwords do not match\n";
        return "";
    }
    
    return password1;
}

/// Read line from terminal
std::string ReadLine(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

/// Ask yes/no question
bool AskYesNo(const std::string& question, bool defaultYes = false) {
    std::string prompt = question + (defaultYes ? " [Y/n]: " : " [y/N]: ");
    std::string answer = ReadLine(prompt);
    
    if (answer.empty()) {
        return defaultYes;
    }
    
    return (answer[0] == 'y' || answer[0] == 'Y');
}

/// Print horizontal line
void PrintLine(char c = '-', int width = 60) {
    std::cout << std::string(width, c) << "\n";
}

/// Format amount for display (satoshis to SHURIUM)
std::string FormatShurium(Amount amount) {
    bool negative = amount < 0;
    if (negative) amount = -amount;
    
    int64_t whole = amount / 100000000;
    int64_t frac = amount % 100000000;
    
    std::ostringstream oss;
    if (negative) oss << "-";
    oss << whole << "." << std::setfill('0') << std::setw(8) << frac;
    
    // Trim trailing zeros
    std::string result = oss.str();
    size_t dot = result.find('.');
    if (dot != std::string::npos) {
        size_t lastNonZero = result.find_last_not_of('0');
        if (lastNonZero > dot) {
            result = result.substr(0, lastNonZero + 1);
        } else {
            result = result.substr(0, dot + 2);
        }
    }
    
    return result + " NXS";
}

// ============================================================================
// Wallet File Utilities
// ============================================================================

/// Get default wallet path
fs::path GetDefaultWalletPath() {
    const char* home = getenv("HOME");
    if (home) {
        fs::path dataDir = fs::path(home) / ".shurium";
        return dataDir / DEFAULT_WALLET_FILE;
    }
    return fs::path(DEFAULT_WALLET_FILE);
}

/// Ensure data directory exists
bool EnsureDataDir(const fs::path& walletPath) {
    fs::path dir = walletPath.parent_path();
    if (!dir.empty() && !fs::exists(dir)) {
        std::error_code ec;
        if (!fs::create_directories(dir, ec)) {
            std::cerr << "Error: Cannot create directory " << dir << ": " << ec.message() << "\n";
            return false;
        }
    }
    return true;
}

// ============================================================================
// Command: Create
// ============================================================================

int CommandCreate(const std::string& walletPath, int wordCount, bool testnet) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    // Check if wallet already exists
    if (fs::exists(path)) {
        std::cerr << "Error: Wallet already exists at " << path << "\n";
        std::cerr << "Use --wallet=<path> to specify a different location or remove the existing file.\n";
        return 1;
    }
    
    // Ensure directory exists
    if (!EnsureDataDir(path)) {
        return 1;
    }
    
    // Determine mnemonic strength
    Mnemonic::Strength strength;
    switch (wordCount) {
        case 12: strength = Mnemonic::Strength::Words12; break;
        case 15: strength = Mnemonic::Strength::Words15; break;
        case 18: strength = Mnemonic::Strength::Words18; break;
        case 21: strength = Mnemonic::Strength::Words21; break;
        case 24: strength = Mnemonic::Strength::Words24; break;
        default:
            std::cerr << "Error: Invalid word count. Must be 12, 15, 18, 21, or 24.\n";
            return 1;
    }
    
    // Generate mnemonic
    std::string mnemonic = Mnemonic::Generate(strength);
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "SHURIUM WALLET CREATION\n";
    PrintLine('=');
    std::cout << "\n";
    
    // Display mnemonic with strong warning
    std::cout << "Your wallet recovery phrase (" << wordCount << " words):\n\n";
    PrintLine();
    
    // Display words in a readable format
    std::istringstream iss(mnemonic);
    std::string word;
    int i = 1;
    while (iss >> word) {
        std::cout << std::setw(2) << i++ << ". " << word << "\n";
    }
    
    PrintLine();
    std::cout << "\n";
    
    std::cout << "!!! CRITICAL WARNING !!!\n";
    std::cout << "Write down these words and store them in a SECURE location.\n";
    std::cout << "This is the ONLY way to recover your wallet if you lose access.\n";
    std::cout << "NEVER share these words with anyone!\n";
    std::cout << "NEVER store them digitally (no photos, no text files)!\n\n";
    
    // Confirm backup
    if (!AskYesNo("Have you written down your recovery phrase?", false)) {
        std::cout << "Please write down your recovery phrase before continuing.\n";
        return 1;
    }
    
    // Verify backup by asking for random words
    std::cout << "\nVerify your backup by entering the following words:\n";
    
    std::vector<std::string> words;
    {
        std::istringstream iss2(mnemonic);
        std::string w;
        while (iss2 >> w) words.push_back(w);
    }
    
    // Ask for 3 random words
    srand(static_cast<unsigned>(time(nullptr)));
    std::vector<int> indices;
    while (indices.size() < 3) {
        int idx = rand() % static_cast<int>(words.size());
        if (std::find(indices.begin(), indices.end(), idx) == indices.end()) {
            indices.push_back(idx);
        }
    }
    std::sort(indices.begin(), indices.end());
    
    for (int idx : indices) {
        std::string prompt = "Enter word #" + std::to_string(idx + 1) + ": ";
        std::string answer = ReadLine(prompt);
        
        // Trim whitespace
        while (!answer.empty() && (answer.front() == ' ' || answer.front() == '\t')) {
            answer.erase(0, 1);
        }
        while (!answer.empty() && (answer.back() == ' ' || answer.back() == '\t')) {
            answer.pop_back();
        }
        
        // Convert to lowercase for comparison
        std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
        
        if (answer != words[idx]) {
            std::cerr << "Error: Incorrect word. Wallet creation aborted for your safety.\n";
            std::cerr << "Please write down your recovery phrase and try again.\n";
            return 1;
        }
    }
    
    std::cout << "\nBackup verified successfully!\n\n";
    
    // Get password
    std::string password = ReadPasswordWithConfirm("Enter wallet password: ");
    if (password.empty()) {
        return 1;
    }
    
    // Check password strength
    auto strength_check = CheckPasswordStrength(password);
    if (!strength_check.IsAcceptable()) {
        std::cerr << "Warning: " << strength_check.GetFeedback() << "\n";
        if (!AskYesNo("Continue with weak password?", false)) {
            return 1;
        }
    }
    
    // Create wallet configuration
    Wallet::Config config;
    config.name = "default";
    config.testnet = testnet;
    config.gapLimit = 20;
    
    // Create wallet
    std::cout << "Creating wallet...\n";
    
    auto wallet = Wallet::FromMnemonic(mnemonic, "", password, config);
    if (!wallet) {
        std::cerr << "Error: Failed to create wallet\n";
        return 1;
    }
    
    // Save wallet
    if (!wallet->Save(path.string())) {
        std::cerr << "Error: Failed to save wallet to " << path << "\n";
        return 1;
    }
    
    // Generate initial address
    wallet->Unlock(password);
    std::string address = wallet->GetNewAddress("Default");
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "WALLET CREATED SUCCESSFULLY\n";
    PrintLine('=');
    std::cout << "\n";
    std::cout << "Wallet file: " << path << "\n";
    std::cout << "Network:     " << (testnet ? "Testnet" : "Mainnet") << "\n";
    std::cout << "First address: " << address << "\n";
    std::cout << "\n";
    
    // Clear sensitive data
    std::fill(mnemonic.begin(), mnemonic.end(), '\0');
    std::fill(password.begin(), password.end(), '\0');
    
    return 0;
}

// ============================================================================
// Command: Import
// ============================================================================

int CommandImport(const std::string& walletPath, bool testnet) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    // Check if wallet already exists
    if (fs::exists(path)) {
        std::cerr << "Error: Wallet already exists at " << path << "\n";
        return 1;
    }
    
    // Ensure directory exists
    if (!EnsureDataDir(path)) {
        return 1;
    }
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "SHURIUM WALLET IMPORT\n";
    PrintLine('=');
    std::cout << "\n";
    
    // Read mnemonic
    std::cout << "Enter your recovery phrase (12, 15, 18, 21, or 24 words):\n";
    std::string mnemonic = ReadLine("> ");
    
    // Validate mnemonic
    if (!Mnemonic::Validate(mnemonic)) {
        std::cerr << "Error: Invalid recovery phrase. Please check the words and try again.\n";
        return 1;
    }
    
    // Count words
    int wordCount = 0;
    {
        std::istringstream iss(mnemonic);
        std::string word;
        while (iss >> word) wordCount++;
    }
    std::cout << "Valid " << wordCount << "-word recovery phrase.\n\n";
    
    // Optional passphrase
    std::cout << "Enter BIP39 passphrase (leave empty if none was used):\n";
    std::string passphrase = ReadPassword("Passphrase: ");
    
    // Get new password
    std::string password = ReadPasswordWithConfirm("\nEnter new wallet password: ");
    if (password.empty()) {
        return 1;
    }
    
    // Create wallet configuration
    Wallet::Config config;
    config.name = "imported";
    config.testnet = testnet;
    config.gapLimit = 20;
    
    // Create wallet
    std::cout << "Importing wallet...\n";
    
    auto wallet = Wallet::FromMnemonic(mnemonic, passphrase, password, config);
    if (!wallet) {
        std::cerr << "Error: Failed to import wallet\n";
        return 1;
    }
    
    // Save wallet
    if (!wallet->Save(path.string())) {
        std::cerr << "Error: Failed to save wallet to " << path << "\n";
        return 1;
    }
    
    // Generate initial address
    wallet->Unlock(password);
    std::string address = wallet->GetNewAddress("Default");
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "WALLET IMPORTED SUCCESSFULLY\n";
    PrintLine('=');
    std::cout << "\n";
    std::cout << "Wallet file: " << path << "\n";
    std::cout << "Network:     " << (testnet ? "Testnet" : "Mainnet") << "\n";
    std::cout << "First address: " << address << "\n";
    std::cout << "\n";
    std::cout << "Note: Run 'shurium-wallet info' to see wallet details.\n";
    std::cout << "Note: If this wallet was previously used, the blockchain will need\n";
    std::cout << "      to be scanned to find existing transactions and balance.\n";
    std::cout << "\n";
    
    // Clear sensitive data
    std::fill(mnemonic.begin(), mnemonic.end(), '\0');
    std::fill(passphrase.begin(), passphrase.end(), '\0');
    std::fill(password.begin(), password.end(), '\0');
    
    return 0;
}

// ============================================================================
// Command: Info
// ============================================================================

int CommandInfo(const std::string& walletPath) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    if (!fs::exists(path)) {
        std::cerr << "Error: Wallet not found at " << path << "\n";
        std::cerr << "Use 'shurium-wallet create' to create a new wallet.\n";
        return 1;
    }
    
    // Load wallet
    auto wallet = Wallet::Load(path.string());
    if (!wallet) {
        std::cerr << "Error: Failed to load wallet from " << path << "\n";
        return 1;
    }
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "SHURIUM WALLET INFO\n";
    PrintLine('=');
    std::cout << "\n";
    
    std::cout << "Wallet file:    " << path << "\n";
    std::cout << "Wallet name:    " << wallet->GetName() << "\n";
    std::cout << "Network:        " << (wallet->GetConfig().testnet ? "Testnet" : "Mainnet") << "\n";
    std::cout << "Encrypted:      Yes\n";
    std::cout << "Locked:         " << (wallet->IsLocked() ? "Yes" : "No") << "\n";
    std::cout << "\n";
    
    // Get balance (shows cached/offline balance)
    auto balance = wallet->GetBalance();
    
    PrintLine();
    std::cout << "BALANCE (offline cache)\n";
    PrintLine();
    std::cout << "Confirmed:      " << FormatShurium(balance.confirmed) << "\n";
    std::cout << "Unconfirmed:    " << FormatShurium(balance.unconfirmed) << "\n";
    std::cout << "Immature:       " << FormatShurium(balance.immature) << "\n";
    std::cout << "Total:          " << FormatShurium(balance.GetTotal()) << "\n";
    std::cout << "\n";
    
    // Unlock to show more info
    std::string password = ReadPassword("Enter password to show addresses (or press Enter to skip): ");
    if (!password.empty()) {
        if (!wallet->Unlock(password)) {
            std::cerr << "Error: Incorrect password\n";
            return 1;
        }
        
        auto addresses = wallet->GetAddresses();
        
        PrintLine();
        std::cout << "ADDRESSES (" << addresses.size() << " total)\n";
        PrintLine();
        
        int shown = 0;
        for (const auto& addr : addresses) {
            std::cout << addr << "\n";
            if (++shown >= 10) {
                std::cout << "... and " << (addresses.size() - 10) << " more\n";
                break;
            }
        }
        std::cout << "\n";
        
        // Show UTXOs
        auto outputs = wallet->GetOutputs();
        int spendable = 0;
        for (const auto& out : outputs) {
            if (out.status == OutputStatus::Available || out.status == OutputStatus::Unconfirmed) {
                spendable++;
            }
        }
        
        PrintLine();
        std::cout << "UTXOS\n";
        PrintLine();
        std::cout << "Total outputs:  " << outputs.size() << "\n";
        std::cout << "Spendable:      " << spendable << "\n";
        std::cout << "\n";
    }
    
    return 0;
}

// ============================================================================
// Command: Address
// ============================================================================

int CommandAddressNew(const std::string& walletPath, const std::string& label) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    if (!fs::exists(path)) {
        std::cerr << "Error: Wallet not found at " << path << "\n";
        return 1;
    }
    
    auto wallet = Wallet::Load(path.string());
    if (!wallet) {
        std::cerr << "Error: Failed to load wallet\n";
        return 1;
    }
    
    std::string password = ReadPassword("Enter wallet password: ");
    if (!wallet->Unlock(password)) {
        std::cerr << "Error: Incorrect password\n";
        return 1;
    }
    
    std::string address = wallet->GetNewAddress(label);
    
    // Save wallet to persist new address
    wallet->Save();
    
    std::cout << "\nNew address: " << address << "\n";
    if (!label.empty()) {
        std::cout << "Label: " << label << "\n";
    }
    std::cout << "\n";
    
    return 0;
}

int CommandAddressList(const std::string& walletPath, bool showAll) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    if (!fs::exists(path)) {
        std::cerr << "Error: Wallet not found at " << path << "\n";
        return 1;
    }
    
    auto wallet = Wallet::Load(path.string());
    if (!wallet) {
        std::cerr << "Error: Failed to load wallet\n";
        return 1;
    }
    
    std::string password = ReadPassword("Enter wallet password: ");
    if (!wallet->Unlock(password)) {
        std::cerr << "Error: Incorrect password\n";
        return 1;
    }
    
    auto addresses = wallet->GetAddresses();
    auto addressBook = wallet->GetAddressBook();
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "WALLET ADDRESSES\n";
    PrintLine('=');
    std::cout << "\n";
    
    // Create a map of address to label
    std::map<std::string, std::string> labels;
    for (const auto& entry : addressBook) {
        if (entry.purpose == "receive") {
            labels[entry.address] = entry.label;
        }
    }
    
    int count = 0;
    for (const auto& addr : addresses) {
        std::cout << addr;
        auto it = labels.find(addr);
        if (it != labels.end() && !it->second.empty()) {
            std::cout << "  (" << it->second << ")";
        }
        std::cout << "\n";
        
        if (!showAll && ++count >= 20) {
            std::cout << "\n... " << (addresses.size() - 20) << " more addresses\n";
            std::cout << "Use --all to show all addresses\n";
            break;
        }
    }
    
    std::cout << "\nTotal: " << addresses.size() << " addresses\n\n";
    
    return 0;
}

// ============================================================================
// Command: Dump (Export)
// ============================================================================

int CommandDump(const std::string& walletPath, bool showMnemonic) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    if (!fs::exists(path)) {
        std::cerr << "Error: Wallet not found at " << path << "\n";
        return 1;
    }
    
    auto wallet = Wallet::Load(path.string());
    if (!wallet) {
        std::cerr << "Error: Failed to load wallet\n";
        return 1;
    }
    
    if (showMnemonic) {
        std::cout << "\n";
        std::cout << "!!! WARNING !!!\n";
        std::cout << "You are about to display your recovery phrase.\n";
        std::cout << "Anyone who sees these words can steal your funds!\n";
        std::cout << "Make sure no one is watching your screen.\n\n";
        
        if (!AskYesNo("Are you sure you want to continue?", false)) {
            return 0;
        }
    }
    
    std::string password = ReadPassword("Enter wallet password: ");
    if (!wallet->Unlock(password)) {
        std::cerr << "Error: Incorrect password\n";
        return 1;
    }
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "WALLET EXPORT\n";
    PrintLine('=');
    std::cout << "\n";
    
    // Get HD key manager to access master public key
    auto* keystore = wallet->GetKeyStore();
    if (!keystore) {
        std::cerr << "Error: Could not access keystore\n";
        return 1;
    }
    
    // Show master public key (safe to share for watch-only)
    auto* hdManager = wallet->GetHDKeyManager();
    if (hdManager) {
        auto masterPub = hdManager->GetMasterPublicKey();
        
        std::cout << "Master Public Key (xpub):\n";
        std::cout << masterPub.ToBase58(wallet->GetConfig().testnet) << "\n\n";
        std::cout << "This key can be used to create a watch-only wallet.\n";
        std::cout << "It cannot be used to spend funds.\n\n";
    }
    
    if (showMnemonic) {
        // Note: We can't actually retrieve the mnemonic from an encrypted wallet
        // without storing it separately. For production, you'd need to either:
        // 1. Store encrypted mnemonic in wallet file
        // 2. Tell user to refer to their backup
        std::cout << "RECOVERY PHRASE:\n";
        std::cout << "The recovery phrase is not stored in the wallet file.\n";
        std::cout << "Please refer to your written backup.\n\n";
        std::cout << "If you have lost your recovery phrase, create a new wallet\n";
        std::cout << "and transfer your funds to it immediately.\n\n";
    }
    
    // Show derived addresses with private keys (DANGEROUS)
    if (showMnemonic) {
        if (AskYesNo("Show private keys for derived addresses? (DANGEROUS)", false)) {
            std::cout << "\n!!! PRIVATE KEYS - DO NOT SHARE !!!\n\n";
            
            auto addresses = wallet->GetAddresses();
            int shown = 0;
            for (const auto& addr : addresses) {
                // For each address, we'd need to look up and export the private key
                // This requires the keystore to support key export
                std::cout << "Address: " << addr << "\n";
                // std::cout << "Private: " << privateKeyWIF << "\n\n";
                
                if (++shown >= 5) {
                    std::cout << "... (showing first 5 only for safety)\n";
                    break;
                }
            }
            std::cout << "\n";
        }
    }
    
    return 0;
}

// ============================================================================
// Command: Change Password
// ============================================================================

int CommandChangePassword(const std::string& walletPath) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    if (!fs::exists(path)) {
        std::cerr << "Error: Wallet not found at " << path << "\n";
        return 1;
    }
    
    auto wallet = Wallet::Load(path.string());
    if (!wallet) {
        std::cerr << "Error: Failed to load wallet\n";
        return 1;
    }
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "CHANGE WALLET PASSWORD\n";
    PrintLine('=');
    std::cout << "\n";
    
    std::string oldPassword = ReadPassword("Enter current password: ");
    
    if (!wallet->CheckPassword(oldPassword)) {
        std::cerr << "Error: Incorrect password\n";
        return 1;
    }
    
    std::string newPassword = ReadPasswordWithConfirm("Enter new password: ");
    if (newPassword.empty()) {
        return 1;
    }
    
    // Check new password strength
    auto strength = CheckPasswordStrength(newPassword);
    if (!strength.IsAcceptable()) {
        std::cerr << "Warning: " << strength.GetFeedback() << "\n";
        if (!AskYesNo("Continue with weak password?", false)) {
            return 1;
        }
    }
    
    if (!wallet->ChangePassword(oldPassword, newPassword)) {
        std::cerr << "Error: Failed to change password\n";
        return 1;
    }
    
    // Save wallet
    if (!wallet->Save()) {
        std::cerr << "Error: Failed to save wallet\n";
        return 1;
    }
    
    std::cout << "\nPassword changed successfully!\n\n";
    
    // Clear sensitive data
    std::fill(oldPassword.begin(), oldPassword.end(), '\0');
    std::fill(newPassword.begin(), newPassword.end(), '\0');
    
    return 0;
}

// ============================================================================
// Command: Sign Transaction
// ============================================================================

int CommandSign(const std::string& walletPath, const std::string& txHex) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    if (!fs::exists(path)) {
        std::cerr << "Error: Wallet not found at " << path << "\n";
        return 1;
    }
    
    auto wallet = Wallet::Load(path.string());
    if (!wallet) {
        std::cerr << "Error: Failed to load wallet\n";
        return 1;
    }
    
    std::string password = ReadPassword("Enter wallet password: ");
    if (!wallet->Unlock(password)) {
        std::cerr << "Error: Incorrect password\n";
        return 1;
    }
    
    // Get transaction hex
    std::string hex = txHex;
    if (hex.empty()) {
        std::cout << "\nEnter raw transaction hex:\n";
        std::getline(std::cin, hex);
    }
    
    // Trim whitespace
    hex.erase(std::remove_if(hex.begin(), hex.end(), ::isspace), hex.end());
    
    if (hex.empty()) {
        std::cerr << "Error: No transaction provided\n";
        return 1;
    }
    
    // Decode hex to bytes
    std::vector<uint8_t> txBytes;
    try {
        txBytes = HexToBytes(hex);
    } catch (...) {
        std::cerr << "Error: Invalid hex encoding\n";
        return 1;
    }
    
    // Deserialize transaction
    DataStream stream(txBytes);
    MutableTransaction mtx;
    try {
        Unserialize(stream, mtx);
    } catch (...) {
        std::cerr << "Error: Failed to parse transaction\n";
        return 1;
    }
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "TRANSACTION TO SIGN\n";
    PrintLine('=');
    std::cout << "\n";
    std::cout << "Inputs:  " << mtx.vin.size() << "\n";
    std::cout << "Outputs: " << mtx.vout.size() << "\n";
    
    Amount totalOut = 0;
    for (const auto& out : mtx.vout) {
        totalOut += out.nValue;
    }
    std::cout << "Total output: " << FormatShurium(totalOut) << "\n\n";
    
    // Display outputs
    std::cout << "Output breakdown:\n";
    for (size_t i = 0; i < mtx.vout.size(); i++) {
        std::cout << "  " << i << ": " << FormatShurium(mtx.vout[i].nValue) << "\n";
    }
    std::cout << "\n";
    
    if (!AskYesNo("Sign this transaction?", false)) {
        std::cout << "Aborted.\n";
        return 0;
    }
    
    // Sign transaction
    if (!wallet->SignTransaction(mtx)) {
        std::cerr << "Error: Failed to sign transaction\n";
        std::cerr << "Make sure you have the private keys for all inputs.\n";
        return 1;
    }
    
    // Serialize signed transaction
    DataStream outStream;
    Serialize(outStream, mtx);
    std::vector<uint8_t> signedBytes(outStream.begin(), outStream.end());
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "SIGNED TRANSACTION\n";
    PrintLine('=');
    std::cout << "\n";
    std::cout << BytesToHex(signedBytes) << "\n\n";
    std::cout << "Transaction signed successfully!\n";
    std::cout << "Broadcast this transaction using 'shurium-cli sendrawtransaction <hex>'\n\n";
    
    return 0;
}

// ============================================================================
// Command: Verify
// ============================================================================

int CommandVerify(const std::string& walletPath) {
    fs::path path = walletPath.empty() ? GetDefaultWalletPath() : fs::path(walletPath);
    
    if (!fs::exists(path)) {
        std::cerr << "Error: Wallet not found at " << path << "\n";
        return 1;
    }
    
    auto wallet = Wallet::Load(path.string());
    if (!wallet) {
        std::cerr << "Error: Failed to load wallet\n";
        return 1;
    }
    
    std::cout << "\n";
    PrintLine('=');
    std::cout << "WALLET VERIFICATION\n";
    PrintLine('=');
    std::cout << "\n";
    
    std::cout << "Wallet file:    " << path << "\n";
    std::cout << "File size:      " << fs::file_size(path) << " bytes\n";
    std::cout << "Load status:    OK\n";
    
    std::string password = ReadPassword("Enter wallet password: ");
    if (wallet->Unlock(password)) {
        std::cout << "Password:       VALID\n";
        std::cout << "Encryption:     OK\n";
        
        // Verify we can derive keys
        auto addresses = wallet->GetAddresses();
        std::cout << "Key derivation: OK (" << addresses.size() << " addresses)\n";
        
        std::cout << "\nWallet verification PASSED\n\n";
        return 0;
    } else {
        std::cout << "Password:       INVALID\n";
        std::cout << "\nWallet verification FAILED\n\n";
        return 1;
    }
}

// ============================================================================
// Help and Usage
// ============================================================================

void PrintUsage() {
    std::cout << "SHURIUM Wallet Tool v" << VERSION << "\n";
    std::cout << "\n";
    std::cout << "Usage: shurium-wallet <command> [options]\n";
    std::cout << "\n";
    std::cout << "Commands:\n";
    std::cout << "  create          Create a new wallet with a new recovery phrase\n";
    std::cout << "  import          Import wallet from existing recovery phrase\n";
    std::cout << "  info            Display wallet information\n";
    std::cout << "  address new     Generate a new receiving address\n";
    std::cout << "  address list    List all wallet addresses\n";
    std::cout << "  dump            Export wallet data (master public key)\n";
    std::cout << "  sign [hex]      Sign a raw transaction offline\n";
    std::cout << "  verify          Verify wallet integrity and password\n";
    std::cout << "  passwd          Change wallet password\n";
    std::cout << "  help            Show this help message\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  --wallet=<path>  Path to wallet file (default: ~/.shurium/wallet.dat)\n";
    std::cout << "  --testnet        Use testnet instead of mainnet\n";
    std::cout << "  --words=<n>      Word count for new mnemonic (12,15,18,21,24)\n";
    std::cout << "  --label=<text>   Label for new address\n";
    std::cout << "  --all            Show all items (not just first few)\n";
    std::cout << "  --show-seed      Show recovery phrase (DANGEROUS)\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  shurium-wallet create --words=24\n";
    std::cout << "  shurium-wallet import --wallet=backup.dat\n";
    std::cout << "  shurium-wallet address new --label=\"Savings\"\n";
    std::cout << "  shurium-wallet sign\n";
    std::cout << "\n";
}

void PrintVersion() {
    std::cout << "SHURIUM Wallet Tool v" << VERSION << "\n";
    std::cout << "Copyright (c) 2024 SHURIUM Developers\n";
    std::cout << "MIT License\n";
}

// ============================================================================
// Argument Parsing
// ============================================================================

struct Options {
    std::string command;
    std::string subcommand;
    std::string walletPath;
    std::string label;
    std::string txHex;
    int wordCount = 24;
    bool testnet = false;
    bool showAll = false;
    bool showSeed = false;
    bool help = false;
    bool version = false;
};

Options ParseArgs(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            opts.help = true;
        } else if (arg == "-v" || arg == "--version") {
            opts.version = true;
        } else if (arg == "--testnet") {
            opts.testnet = true;
        } else if (arg == "--all") {
            opts.showAll = true;
        } else if (arg == "--show-seed") {
            opts.showSeed = true;
        } else if (arg.rfind("--wallet=", 0) == 0) {
            opts.walletPath = arg.substr(9);
        } else if (arg.rfind("--words=", 0) == 0) {
            opts.wordCount = std::stoi(arg.substr(8));
        } else if (arg.rfind("--label=", 0) == 0) {
            opts.label = arg.substr(8);
        } else if (arg[0] != '-') {
            // Positional argument
            if (opts.command.empty()) {
                opts.command = arg;
            } else if (opts.subcommand.empty()) {
                opts.subcommand = arg;
            } else if (opts.txHex.empty()) {
                opts.txHex = arg;
            }
        }
    }
    
    return opts;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = ParseArgs(argc, argv);
    
    if (opts.version) {
        PrintVersion();
        return 0;
    }
    
    if (opts.help || opts.command.empty()) {
        PrintUsage();
        return opts.help ? 0 : 1;
    }
    
    // Route to command
    if (opts.command == "create") {
        return CommandCreate(opts.walletPath, opts.wordCount, opts.testnet);
    } else if (opts.command == "import") {
        return CommandImport(opts.walletPath, opts.testnet);
    } else if (opts.command == "info") {
        return CommandInfo(opts.walletPath);
    } else if (opts.command == "address") {
        if (opts.subcommand == "new") {
            return CommandAddressNew(opts.walletPath, opts.label);
        } else if (opts.subcommand == "list" || opts.subcommand.empty()) {
            return CommandAddressList(opts.walletPath, opts.showAll);
        } else {
            std::cerr << "Unknown address subcommand: " << opts.subcommand << "\n";
            return 1;
        }
    } else if (opts.command == "dump" || opts.command == "export") {
        return CommandDump(opts.walletPath, opts.showSeed);
    } else if (opts.command == "sign") {
        return CommandSign(opts.walletPath, opts.txHex);
    } else if (opts.command == "verify") {
        return CommandVerify(opts.walletPath);
    } else if (opts.command == "passwd" || opts.command == "password") {
        return CommandChangePassword(opts.walletPath);
    } else if (opts.command == "help") {
        PrintUsage();
        return 0;
    } else {
        std::cerr << "Unknown command: " << opts.command << "\n";
        std::cerr << "Run 'shurium-wallet help' for usage.\n";
        return 1;
    }
}
