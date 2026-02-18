// SHURIUM - Encrypted Key Storage
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements secure key storage with AES-256-GCM encryption.
// Features:
// - Encrypted private key storage
// - Watch-only wallet support
// - Memory locking for sensitive data
// - Key derivation from password (Argon2id)

#ifndef SHURIUM_WALLET_KEYSTORE_H
#define SHURIUM_WALLET_KEYSTORE_H

#include <shurium/core/types.h>
#include <shurium/crypto/keys.h>
#include <shurium/wallet/hdkey.h>
#include <shurium/identity/identity.h>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace shurium {
namespace wallet {

// ============================================================================
// Constants
// ============================================================================

/// AES-256 key size
constexpr size_t AES_KEY_SIZE = 32;

/// AES-GCM nonce size
constexpr size_t AES_NONCE_SIZE = 12;

/// AES-GCM tag size
constexpr size_t AES_TAG_SIZE = 16;

/// Salt size for key derivation
constexpr size_t SALT_SIZE = 32;

/// Default Argon2 parameters
constexpr uint32_t ARGON2_TIME_COST = 3;
constexpr uint32_t ARGON2_MEMORY_COST = 65536;  // 64 MB
constexpr uint32_t ARGON2_PARALLELISM = 4;

// ============================================================================
// Encrypted Data Structures
// ============================================================================

/// Encrypted private key data
struct EncryptedKey {
    /// Salt for key derivation
    std::array<Byte, SALT_SIZE> salt;
    
    /// Nonce for AES-GCM
    std::array<Byte, AES_NONCE_SIZE> nonce;
    
    /// Encrypted key data (32 bytes + tag)
    std::vector<Byte> ciphertext;
    
    /// Public key (for identification)
    PublicKey publicKey;
    
    /// Key derivation path (if HD)
    std::optional<DerivationPath> path;
    
    /// Creation timestamp
    int64_t created{0};
    
    /// Key label/name
    std::string label;
    
    /// Is this key valid?
    bool IsValid() const {
        return !ciphertext.empty() && publicKey.IsValid();
    }
};

/// Encrypted master seed data
struct EncryptedSeed {
    /// Salt for key derivation
    std::array<Byte, SALT_SIZE> salt;
    
    /// Nonce for AES-GCM
    std::array<Byte, AES_NONCE_SIZE> nonce;
    
    /// Encrypted seed (64 bytes for BIP39 seed + tag)
    std::vector<Byte> ciphertext;
    
    /// Master public key (for watch-only recovery)
    ExtendedKey masterPublicKey;
    
    /// Creation timestamp
    int64_t created{0};
    
    /// Mnemonic checksum (for verification without decryption)
    Hash256 mnemonicChecksum;
    
    bool IsValid() const {
        return !ciphertext.empty() && masterPublicKey.IsValid();
    }
};

/// Encrypted identity secrets
struct EncryptedIdentity {
    /// Salt for key derivation
    std::array<Byte, SALT_SIZE> salt;
    
    /// Nonce for AES-GCM
    std::array<Byte, AES_NONCE_SIZE> nonce;
    
    /// Encrypted identity secrets
    std::vector<Byte> ciphertext;
    
    /// Identity commitment (for identification)
    Hash256 commitment;
    
    /// Creation timestamp
    int64_t created{0};
    
    bool IsValid() const {
        return !ciphertext.empty() && !commitment.IsNull();
    }
};

// ============================================================================
// Encryption Engine
// ============================================================================

/**
 * Cryptographic operations for key storage.
 * 
 * Uses:
 * - Argon2id for password-based key derivation
 * - AES-256-GCM for authenticated encryption
 */
class CryptoEngine {
public:
    /// Derive encryption key from password using Argon2id
    static std::array<Byte, AES_KEY_SIZE> DeriveKey(
        const std::string& password,
        const std::array<Byte, SALT_SIZE>& salt,
        uint32_t timeCost = ARGON2_TIME_COST,
        uint32_t memoryCost = ARGON2_MEMORY_COST,
        uint32_t parallelism = ARGON2_PARALLELISM);
    
    /// Encrypt data using AES-256-GCM
    /// @param key Encryption key (32 bytes)
    /// @param nonce Nonce (12 bytes, must be unique per encryption)
    /// @param plaintext Data to encrypt
    /// @param aad Additional authenticated data (optional)
    /// @return Ciphertext with appended authentication tag
    static std::vector<Byte> Encrypt(
        const std::array<Byte, AES_KEY_SIZE>& key,
        const std::array<Byte, AES_NONCE_SIZE>& nonce,
        const std::vector<Byte>& plaintext,
        const std::vector<Byte>& aad = {});
    
    /// Decrypt data using AES-256-GCM
    /// @return Plaintext or empty on authentication failure
    static std::optional<std::vector<Byte>> Decrypt(
        const std::array<Byte, AES_KEY_SIZE>& key,
        const std::array<Byte, AES_NONCE_SIZE>& nonce,
        const std::vector<Byte>& ciphertext,
        const std::vector<Byte>& aad = {});
    
    /// Generate random salt
    static std::array<Byte, SALT_SIZE> GenerateSalt();
    
    /// Generate random nonce
    static std::array<Byte, AES_NONCE_SIZE> GenerateNonce();
    
    /// Securely zero memory
    static void SecureZero(void* ptr, size_t size);
    
    /// Lock memory pages (prevent swapping)
    static bool LockMemory(void* ptr, size_t size);
    
    /// Unlock memory pages
    static bool UnlockMemory(void* ptr, size_t size);
};

// ============================================================================
// Secure Memory Container
// ============================================================================

/**
 * RAII container for sensitive data that:
 * - Locks memory to prevent swapping
 * - Securely zeros memory on destruction
 */
template<typename T, size_t N>
class SecureArray {
public:
    SecureArray() {
        CryptoEngine::LockMemory(data_.data(), sizeof(data_));
        data_.fill(0);
    }
    
    ~SecureArray() {
        CryptoEngine::SecureZero(data_.data(), sizeof(data_));
        CryptoEngine::UnlockMemory(data_.data(), sizeof(data_));
    }
    
    // Non-copyable
    SecureArray(const SecureArray&) = delete;
    SecureArray& operator=(const SecureArray&) = delete;
    
    // Move only
    SecureArray(SecureArray&& other) noexcept : data_(std::move(other.data_)) {
        other.data_.fill(0);
    }
    
    SecureArray& operator=(SecureArray&& other) noexcept {
        if (this != &other) {
            CryptoEngine::SecureZero(data_.data(), sizeof(data_));
            data_ = std::move(other.data_);
            other.data_.fill(0);
        }
        return *this;
    }
    
    T* data() { return data_.data(); }
    const T* data() const { return data_.data(); }
    constexpr size_t size() const { return N; }
    
    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }
    
    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }

private:
    std::array<T, N> data_;
};

// ============================================================================
// Key Store Interface
// ============================================================================

/// Key types
enum class KeyType {
    Single,      // Single private key
    HD,          // HD derived key
    WatchOnly,   // Public key only
    Identity     // Identity secrets
};

/// Key status
enum class KeyStatus {
    Locked,     // Encrypted, needs password
    Unlocked,   // Decrypted, ready for signing
    WatchOnly   // No private key available
};

/**
 * Abstract interface for key storage backends.
 */
class IKeyStore {
public:
    virtual ~IKeyStore() = default;
    
    /// Check if store is encrypted
    virtual bool IsEncrypted() const = 0;
    
    /// Check if store is locked
    virtual bool IsLocked() const = 0;
    
    /// Lock the keystore (clear decrypted keys)
    virtual void Lock() = 0;
    
    /// Unlock with password
    virtual bool Unlock(const std::string& password) = 0;
    
    /// Check if password is correct
    virtual bool CheckPassword(const std::string& password) const = 0;
    
    /// Change password
    virtual bool ChangePassword(const std::string& oldPassword,
                                const std::string& newPassword) = 0;
    
    /// Add a private key
    virtual bool AddKey(const PrivateKey& key, const std::string& label = "") = 0;
    
    /// Add watch-only public key
    virtual bool AddWatchOnly(const PublicKey& pubkey, const std::string& label = "") = 0;
    
    /// Get private key for signing
    virtual std::optional<PrivateKey> GetKey(const Hash160& keyHash) const = 0;
    
    /// Get public key
    virtual std::optional<PublicKey> GetPublicKey(const Hash160& keyHash) const = 0;
    
    /// Check if we have a key
    virtual bool HaveKey(const Hash160& keyHash) const = 0;
    
    /// Check if key is watch-only
    virtual bool IsWatchOnly(const Hash160& keyHash) const = 0;
    
    /// Get all key hashes
    virtual std::vector<Hash160> GetKeyHashes() const = 0;
    
    /// Sign a hash with key
    virtual std::optional<std::vector<Byte>> Sign(const Hash160& keyHash,
                                                   const Hash256& hash) const = 0;
};

// ============================================================================
// In-Memory Key Store
// ============================================================================

/**
 * In-memory key storage with optional encryption.
 * 
 * Keys are stored encrypted and decrypted on demand.
 * Supports both HD and individual keys.
 */
class MemoryKeyStore : public IKeyStore {
public:
    /// Create empty keystore (will need SetupEncryption)
    MemoryKeyStore();
    
    /// Create with password (encrypted from start)
    explicit MemoryKeyStore(const std::string& password);
    
    /// Setup encryption for unencrypted store
    bool SetupEncryption(const std::string& password);
    
    // IKeyStore implementation
    bool IsEncrypted() const override;
    bool IsLocked() const override;
    void Lock() override;
    bool Unlock(const std::string& password) override;
    bool CheckPassword(const std::string& password) const override;
    bool ChangePassword(const std::string& oldPassword,
                        const std::string& newPassword) override;
    
    bool AddKey(const PrivateKey& key, const std::string& label = "") override;
    bool AddWatchOnly(const PublicKey& pubkey, const std::string& label = "") override;
    std::optional<PrivateKey> GetKey(const Hash160& keyHash) const override;
    std::optional<PublicKey> GetPublicKey(const Hash160& keyHash) const override;
    bool HaveKey(const Hash160& keyHash) const override;
    bool IsWatchOnly(const Hash160& keyHash) const override;
    std::vector<Hash160> GetKeyHashes() const override;
    std::optional<std::vector<Byte>> Sign(const Hash160& keyHash,
                                           const Hash256& hash) const override;
    
    // HD wallet support
    
    /// Set master seed (will encrypt)
    bool SetMasterSeed(const std::array<Byte, BIP39_SEED_SIZE>& seed);
    
    /// Set from mnemonic
    bool SetFromMnemonic(const std::string& mnemonic,
                         const std::string& passphrase = "");
    
    /// Check if HD wallet is initialized
    bool HasMasterSeed() const { return encryptedSeed_.IsValid(); }
    
    /// Get HD key manager (unlocked only)
    HDKeyManager* GetHDKeyManager();
    const HDKeyManager* GetHDKeyManager() const;
    
    /// Derive next receiving key
    std::optional<PublicKey> DeriveNextReceiving(uint32_t account = 0);
    
    /// Derive next change key
    std::optional<PublicKey> DeriveNextChange(uint32_t account = 0);
    
    // Identity support
    
    /// Store identity secrets
    bool SetIdentitySecrets(const identity::IdentitySecrets& secrets);
    
    /// Check if identity is stored
    bool HasIdentity() const { return encryptedIdentity_.IsValid(); }
    
    /// Get identity secrets (unlocked only)
    std::optional<identity::IdentitySecrets> GetIdentitySecrets() const;
    
    /// Get identity commitment
    Hash256 GetIdentityCommitment() const;
    
    // Statistics
    
    /// Get key count
    size_t KeyCount() const;
    
    /// Get watch-only count
    size_t WatchOnlyCount() const;
    
    /// Set testnet mode (must be called before SetMasterSeed)
    void SetTestnet(bool testnet);
    
    /// Check if testnet mode
    bool IsTestnet() const { return testnet_; }

protected:
    /// Thread safety
    mutable std::mutex mutex_;
    
    /// Network mode
    bool testnet_{false};
    
    /// Encryption parameters
    bool encrypted_{false};
    std::array<Byte, SALT_SIZE> masterSalt_;
    
    /// Password verification token (encrypted known value for password verification)
    std::array<Byte, AES_NONCE_SIZE> verificationNonce_{};
    std::vector<Byte> verificationToken_;  // Encrypted "SHURIUM_KEYSTORE_V2" string
    
    /// Encrypted storage
    std::map<Hash160, EncryptedKey> encryptedKeys_;
    EncryptedSeed encryptedSeed_;
    EncryptedIdentity encryptedIdentity_;
    
    /// Watch-only keys (not encrypted)
    std::set<Hash160> watchOnlyKeys_;
    std::map<Hash160, PublicKey> publicKeys_;
    
    /// Stored HD key indices (persisted - account/change -> next index)
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> hdKeyIndices_;
    
    /// Unlocked state (in memory, cleared on lock)
    bool unlocked_{false};
    std::unique_ptr<SecureArray<Byte, AES_KEY_SIZE>> masterKey_;
    std::map<Hash160, PrivateKey> unlockedKeys_;
    std::unique_ptr<HDKeyManager> hdKeyManager_;
    std::optional<identity::IdentitySecrets> unlockedIdentity_;
    
    /// Unencrypted seed storage (used when no password set - NOT recommended for production)
    std::optional<std::array<Byte, BIP39_SEED_SIZE>> unencryptedSeed_;
    
    /// Flag indicating deferred HD init is needed (after SetTestnet call)
    bool needsDeferredHDInit_{false};

private:
    /// Encrypt a private key
    EncryptedKey EncryptKey(const PrivateKey& key,
                            const std::array<Byte, AES_KEY_SIZE>& encKey,
                            const std::string& label,
                            const std::optional<DerivationPath>& path = std::nullopt);
    
    /// Decrypt a private key
    std::optional<PrivateKey> DecryptKey(const EncryptedKey& encrypted,
                                          const std::array<Byte, AES_KEY_SIZE>& encKey) const;
    
    /// Re-encrypt all keys with new key
    bool ReencryptAll(const std::array<Byte, AES_KEY_SIZE>& oldKey,
                      const std::array<Byte, AES_KEY_SIZE>& newKey);
};

// ============================================================================
// File-Based Key Store
// ============================================================================

/**
 * Persistent key storage that saves to disk.
 * 
 * File format:
 * - Header: magic, version, flags
 * - Master salt
 * - Encrypted seed (if HD)
 * - Encrypted keys
 * - Watch-only keys
 */
class FileKeyStore : public MemoryKeyStore {
public:
    /// File magic number
    static constexpr uint32_t FILE_MAGIC = 0x4E584B53;  // "NXKS"
    
    /// Current file version
    static constexpr uint32_t FILE_VERSION = 3;
    
    /// Create new file keystore
    FileKeyStore();
    
    /// Load from file
    explicit FileKeyStore(const std::string& path);
    
    /// Load keystore from file
    bool Load(const std::string& path);
    
    /// Save keystore to file
    bool Save(const std::string& path) const;
    
    /// Save to current path
    bool Save() const;
    
    /// Get current file path
    const std::string& GetPath() const { return path_; }
    
    /// Check if loaded from file
    bool IsFromFile() const { return !path_.empty(); }
    
    /// Auto-save on changes?
    void SetAutoSave(bool autoSave) { autoSave_ = autoSave; }

private:
    std::string path_;
    bool autoSave_{false};
    
    /// Serialize to bytes
    std::vector<Byte> Serialize() const;
    
    /// Deserialize from bytes
    bool Deserialize(const std::vector<Byte>& data);
};

// ============================================================================
// Key Store Callbacks
// ============================================================================

/// Callback for password requests
using PasswordCallback = std::function<std::string()>;

/// Callback for key operations
using KeyCallback = std::function<void(const Hash160& keyHash, KeyType type)>;

/**
 * Key store with callbacks for UI integration.
 */
class InteractiveKeyStore : public FileKeyStore {
public:
    InteractiveKeyStore();
    
    /// Set callback to request password
    void SetPasswordCallback(PasswordCallback callback) {
        passwordCallback_ = std::move(callback);
    }
    
    /// Set callback for key added
    void SetKeyAddedCallback(KeyCallback callback) {
        keyAddedCallback_ = std::move(callback);
    }
    
    /// Unlock with callback
    bool UnlockInteractive();
    
    /// Add key with callback notification
    bool AddKeyNotify(const PrivateKey& key, const std::string& label = "");

private:
    PasswordCallback passwordCallback_;
    KeyCallback keyAddedCallback_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Verify password strength
struct PasswordStrength {
    bool hasMinLength;      // At least 8 characters
    bool hasUppercase;      // At least one uppercase
    bool hasLowercase;      // At least one lowercase
    bool hasDigit;          // At least one digit
    bool hasSpecial;        // At least one special character
    int score;              // 0-5 based on above
    
    bool IsAcceptable() const { return score >= 3; }
    bool IsStrong() const { return score >= 4; }
    std::string GetFeedback() const;
};

/// Check password strength
PasswordStrength CheckPasswordStrength(const std::string& password);

/// Generate a random password
std::string GenerateRandomPassword(size_t length = 16);

/// Securely compare two strings (constant time)
bool SecureCompare(const std::string& a, const std::string& b);

/// Securely compare two byte arrays (constant time)
bool SecureCompare(const Byte* a, const Byte* b, size_t len);

} // namespace wallet
} // namespace shurium

#endif // SHURIUM_WALLET_KEYSTORE_H
