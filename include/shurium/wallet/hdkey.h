// SHURIUM - Hierarchical Deterministic Key Derivation (BIP32/BIP44)
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements BIP32 HD key derivation and BIP44 key paths for SHURIUM.
// Allows deriving unlimited keys from a single master seed while
// maintaining deterministic, recoverable key hierarchies.
//
// BIP44 path: m/44'/SHURIUM_COIN_TYPE'/account'/change/index
// SHURIUM_COIN_TYPE = 8888 (example - would be registered with SLIP-0044)

#ifndef SHURIUM_WALLET_HDKEY_H
#define SHURIUM_WALLET_HDKEY_H

#include <shurium/core/types.h>
#include <shurium/crypto/keys.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace wallet {

// ============================================================================
// Constants
// ============================================================================

/// SHURIUM coin type for BIP44 (would be registered with SLIP-0044)
constexpr uint32_t SHURIUM_COIN_TYPE = 8888;

/// BIP44 purpose constant
constexpr uint32_t BIP44_PURPOSE = 44;

/// Hardened key derivation threshold
constexpr uint32_t HARDENED_FLAG = 0x80000000;

/// Seed size constants
constexpr size_t BIP39_SEED_SIZE = 64;
constexpr size_t MASTER_SEED_SIZE = 32;

// ============================================================================
// Key Derivation Path
// ============================================================================

/**
 * Represents a BIP32 derivation path component.
 */
struct PathComponent {
    uint32_t index;
    bool hardened;
    
    PathComponent(uint32_t idx = 0, bool hard = false) 
        : index(idx), hardened(hard) {}
    
    /// Get the full index value (with hardened flag if applicable)
    uint32_t GetFullIndex() const {
        return hardened ? (index | HARDENED_FLAG) : index;
    }
    
    /// Parse from string (e.g., "44'" or "0")
    static std::optional<PathComponent> FromString(const std::string& str);
    
    /// Convert to string
    std::string ToString() const;
};

/**
 * A complete BIP32 derivation path.
 * 
 * Example paths:
 * - m/44'/8888'/0'/0/0  (first receiving address)
 * - m/44'/8888'/0'/1/0  (first change address)
 * - m/44'/8888'/1'/0/0  (second account, first receiving)
 */
class DerivationPath {
public:
    /// Create empty path (master key)
    DerivationPath() = default;
    
    /// Create from components
    explicit DerivationPath(std::vector<PathComponent> components)
        : components_(std::move(components)) {}
    
    /// Parse from string (e.g., "m/44'/8888'/0'/0/0")
    static std::optional<DerivationPath> FromString(const std::string& path);
    
    /// Create BIP44 path for SHURIUM
    /// @param account Account index (hardened)
    /// @param change 0 for external, 1 for internal/change
    /// @param index Address index
    static DerivationPath BIP44(uint32_t account, uint32_t change, uint32_t index);
    
    /// Create account-level path (m/44'/8888'/account')
    static DerivationPath BIP44Account(uint32_t account);
    
    /// Get path components
    const std::vector<PathComponent>& GetComponents() const { return components_; }
    
    /// Get depth (number of components)
    size_t Depth() const { return components_.size(); }
    
    /// Check if path is empty (master)
    bool IsEmpty() const { return components_.empty(); }
    
    /// Append a component
    DerivationPath Child(uint32_t index, bool hardened = false) const;
    
    /// Append hardened component
    DerivationPath HardenedChild(uint32_t index) const {
        return Child(index, true);
    }
    
    /// Get parent path
    DerivationPath Parent() const;
    
    /// Convert to string
    std::string ToString() const;
    
    /// Comparison
    bool operator==(const DerivationPath& other) const;
    bool operator!=(const DerivationPath& other) const { return !(*this == other); }
    
private:
    std::vector<PathComponent> components_;
};

// ============================================================================
// Extended Key (BIP32)
// ============================================================================

/**
 * An extended key containing both key material and chain code.
 * 
 * BIP32 extended keys allow deriving child keys deterministically.
 * Contains:
 * - Key data (32 bytes private or 33 bytes compressed public)
 * - Chain code (32 bytes) for child derivation
 * - Depth, parent fingerprint, child index for path info
 */
class ExtendedKey {
public:
    /// Size of chain code
    static constexpr size_t CHAIN_CODE_SIZE = 32;
    
    /// Size of serialized extended key (78 bytes)
    static constexpr size_t SERIALIZED_SIZE = 78;
    
    /// Version bytes for mainnet
    static constexpr uint32_t MAINNET_PRIVATE = 0x0488ADE4;  // xprv
    static constexpr uint32_t MAINNET_PUBLIC = 0x0488B21E;   // xpub
    
    /// Version bytes for testnet
    static constexpr uint32_t TESTNET_PRIVATE = 0x04358394;  // tprv
    static constexpr uint32_t TESTNET_PUBLIC = 0x043587CF;   // tpub
    
    /// Default constructor - invalid key
    ExtendedKey() = default;
    
    /// Create from private key and chain code
    ExtendedKey(const PrivateKey& key, const std::array<Byte, CHAIN_CODE_SIZE>& chainCode,
                uint8_t depth = 0, uint32_t parentFingerprint = 0, uint32_t childIndex = 0);
    
    /// Create from public key and chain code
    ExtendedKey(const PublicKey& key, const std::array<Byte, CHAIN_CODE_SIZE>& chainCode,
                uint8_t depth = 0, uint32_t parentFingerprint = 0, uint32_t childIndex = 0);
    
    /// Generate master key from seed
    static ExtendedKey FromSeed(const Byte* seed, size_t seedLen);
    
    /// Generate master key from BIP39 seed (64 bytes)
    static ExtendedKey FromBIP39Seed(const std::array<Byte, BIP39_SEED_SIZE>& seed);
    
    /// Derive child key
    /// @param index Child index (use | HARDENED_FLAG for hardened)
    std::optional<ExtendedKey> DeriveChild(uint32_t index) const;
    
    /// Derive key at path
    std::optional<ExtendedKey> DerivePath(const DerivationPath& path) const;
    
    /// Check if this is a private key
    bool IsPrivate() const { return isPrivate_; }
    
    /// Check if valid
    bool IsValid() const { return isValid_; }
    
    /// Get private key (only if IsPrivate())
    std::optional<PrivateKey> GetPrivateKey() const;
    
    /// Get public key
    PublicKey GetPublicKey() const;
    
    /// Get chain code
    const std::array<Byte, CHAIN_CODE_SIZE>& GetChainCode() const { return chainCode_; }
    
    /// Get depth
    uint8_t GetDepth() const { return depth_; }
    
    /// Get parent fingerprint
    uint32_t GetParentFingerprint() const { return parentFingerprint_; }
    
    /// Get child index
    uint32_t GetChildIndex() const { return childIndex_; }
    
    /// Get fingerprint of this key (first 4 bytes of Hash160 of public key)
    uint32_t GetFingerprint() const;
    
    /// Neuter (convert to public extended key)
    ExtendedKey Neuter() const;
    
    /// Serialize to Base58Check (xprv/xpub format)
    std::string ToBase58(bool testnet = false) const;
    
    /// Deserialize from Base58Check
    static std::optional<ExtendedKey> FromBase58(const std::string& str);
    
    /// Serialize to bytes (78 bytes)
    std::array<Byte, SERIALIZED_SIZE> ToBytes(bool testnet = false) const;
    
    /// Deserialize from bytes
    static std::optional<ExtendedKey> FromBytes(const Byte* data, size_t len);

private:
    /// Key data (private: 32 bytes, public: 33 bytes compressed)
    std::array<Byte, 33> keyData_;
    
    /// Chain code for derivation
    std::array<Byte, CHAIN_CODE_SIZE> chainCode_;
    
    /// Depth in hierarchy (0 = master)
    uint8_t depth_{0};
    
    /// Fingerprint of parent key
    uint32_t parentFingerprint_{0};
    
    /// Index of this child
    uint32_t childIndex_{0};
    
    /// Is this a private key?
    bool isPrivate_{false};
    
    /// Is this key valid?
    bool isValid_{false};
    
    /// Derive private child key
    std::optional<ExtendedKey> DerivePrivateChild(uint32_t index) const;
    
    /// Derive public child key (non-hardened only)
    std::optional<ExtendedKey> DerivePublicChild(uint32_t index) const;
};

// ============================================================================
// BIP39 Mnemonic Support
// ============================================================================

/**
 * BIP39 mnemonic word list and utilities.
 * 
 * Provides conversion between entropy and mnemonic words,
 * and derivation of seed from mnemonic + passphrase.
 */
class Mnemonic {
public:
    /// Word counts for different entropy sizes
    enum class Strength {
        Words12 = 128,  // 128 bits entropy -> 12 words
        Words15 = 160,  // 160 bits entropy -> 15 words
        Words18 = 192,  // 192 bits entropy -> 18 words
        Words21 = 224,  // 224 bits entropy -> 21 words
        Words24 = 256   // 256 bits entropy -> 24 words
    };
    
    /// Generate a new mnemonic
    /// @param strength Entropy size (determines word count)
    /// @return Space-separated mnemonic words
    static std::string Generate(Strength strength = Strength::Words24);
    
    /// Generate mnemonic from entropy
    static std::string FromEntropy(const Byte* entropy, size_t len);
    
    /// Convert mnemonic to entropy
    /// @return Entropy bytes or empty on invalid mnemonic
    static std::vector<Byte> ToEntropy(const std::string& mnemonic);
    
    /// Validate a mnemonic phrase
    static bool Validate(const std::string& mnemonic);
    
    /// Derive BIP39 seed from mnemonic and optional passphrase
    /// @param mnemonic Space-separated mnemonic words
    /// @param passphrase Optional passphrase (empty string for none)
    /// @return 64-byte seed
    static std::array<Byte, BIP39_SEED_SIZE> ToSeed(const std::string& mnemonic,
                                                     const std::string& passphrase = "");
    
    /// Get word at index from wordlist
    static std::string GetWord(uint16_t index);
    
    /// Get index of word in wordlist
    /// @return Index or -1 if not found
    static int GetWordIndex(const std::string& word);
    
    /// Get total word count in wordlist
    static constexpr size_t WordCount() { return 2048; }

private:
    /// BIP39 English wordlist
    static const char* const WORDLIST[2048];
};

// ============================================================================
// Key Manager
// ============================================================================

/**
 * Manages HD key derivation for a wallet.
 * 
 * Tracks derived keys, handles gap limits, and provides
 * key lookup by address or script.
 */
class HDKeyManager {
public:
    /// Configuration
    struct Config {
        /// Gap limit for address discovery
        uint32_t gapLimit;
        
        /// Default account
        uint32_t defaultAccount;
        
        /// Is testnet?
        bool testnet;
        
        Config() : gapLimit(20), defaultAccount(0), testnet(false) {}
    };
    
    /// Key metadata
    struct KeyInfo {
        DerivationPath path;
        PublicKey publicKey;
        Hash160 keyHash;
        uint32_t account;
        uint32_t change;  // 0 = external, 1 = internal
        uint32_t index;
        bool used{false};
    };
    
    /// Default constructor
    HDKeyManager();
    
    /// Create with master key
    explicit HDKeyManager(const ExtendedKey& masterKey, const Config& config = Config{});
    
    /// Create from mnemonic
    static HDKeyManager FromMnemonic(const std::string& mnemonic,
                                      const std::string& passphrase = "",
                                      const Config& config = Config{});
    
    /// Check if initialized
    bool IsInitialized() const { return masterKey_.IsValid(); }
    
    /// Get master extended public key (for watch-only)
    ExtendedKey GetMasterPublicKey() const;
    
    /// Get account extended key
    std::optional<ExtendedKey> GetAccountKey(uint32_t account) const;
    
    /// Derive a new receiving address
    /// @param account Account index
    /// @return New key info
    KeyInfo DeriveNextReceiving(uint32_t account = 0);
    
    /// Derive a new change address
    KeyInfo DeriveNextChange(uint32_t account = 0);
    
    /// Derive specific key
    std::optional<KeyInfo> DeriveKey(uint32_t account, uint32_t change, uint32_t index);
    
    /// Get key at path
    std::optional<KeyInfo> GetKeyAtPath(const DerivationPath& path);
    
    /// Find key by public key hash
    std::optional<KeyInfo> FindKeyByHash(const Hash160& hash) const;
    
    /// Find key by public key
    std::optional<KeyInfo> FindKey(const PublicKey& pubkey) const;
    
    /// Mark key as used
    void MarkUsed(const Hash160& keyHash);
    
    /// Get all derived keys
    std::vector<KeyInfo> GetAllKeys() const;
    
    /// Get keys for account
    std::vector<KeyInfo> GetKeysForAccount(uint32_t account) const;
    
    /// Sign with key at path
    std::optional<std::vector<uint8_t>> Sign(const DerivationPath& path,
                                              const Hash256& hash) const;
    
    /// Sign with key by hash
    std::optional<std::vector<uint8_t>> SignByKeyHash(const Hash160& keyHash,
                                                       const Hash256& hash) const;
    
    /// Get receiving address (P2WPKH)
    std::string GetReceivingAddress(uint32_t account = 0);
    
    /// Get change address
    std::string GetChangeAddress(uint32_t account = 0);
    
    /// Get next unused receiving index
    uint32_t GetNextReceivingIndex(uint32_t account = 0) const;
    
    /// Get next unused change index
    uint32_t GetNextChangeIndex(uint32_t account = 0) const;
    
    /// Get all indices (for persistence)
    const std::map<std::pair<uint32_t, uint32_t>, uint32_t>& GetAllIndices() const {
        return nextIndices_;
    }
    
    /// Set all indices (for restoration from persistence)
    /// Also regenerates the key cache for all derived keys
    void SetAllIndices(const std::map<std::pair<uint32_t, uint32_t>, uint32_t>& indices);
    
    /// Set a specific index
    void SetNextIndex(uint32_t account, uint32_t change, uint32_t index) {
        nextIndices_[{account, change}] = index;
    }

private:
    ExtendedKey masterKey_;
    Config config_;
    
    /// Derived keys cache
    std::map<Hash160, KeyInfo> keysByHash_;
    
    /// Next indices per account/change
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> nextIndices_;
    
    /// Derive and cache a key
    KeyInfo DeriveAndCache(uint32_t account, uint32_t change, uint32_t index);
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Derive HMAC-SHA512 for key derivation
std::array<Byte, 64> HMAC_SHA512(const Byte* key, size_t keyLen,
                                  const Byte* data, size_t dataLen);

/// PBKDF2-HMAC-SHA512 for BIP39 seed derivation
std::array<Byte, 64> PBKDF2_SHA512(const std::string& password,
                                    const std::string& salt,
                                    uint32_t iterations = 2048);

} // namespace wallet
} // namespace shurium

#endif // SHURIUM_WALLET_HDKEY_H
