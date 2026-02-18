// SHURIUM - Encrypted Key Storage Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/wallet/keystore.h>
#include <shurium/crypto/sha256.h>
#include <shurium/core/random.h>
#include <shurium/core/serialize.h>
#include <shurium/util/fs.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <chrono>
#include <stdexcept>

#ifdef SHURIUM_USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#endif

#ifdef __APPLE__
#include <sys/mman.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace shurium {
namespace wallet {

// ============================================================================
// CryptoEngine Implementation
// ============================================================================

std::array<Byte, AES_KEY_SIZE> CryptoEngine::DeriveKey(
    const std::string& password,
    const std::array<Byte, SALT_SIZE>& salt,
    uint32_t timeCost,
    uint32_t memoryCost,
    uint32_t parallelism) {
    
    std::array<Byte, AES_KEY_SIZE> key{};
    
#ifdef SHURIUM_USE_OPENSSL
    // Use Argon2id via OpenSSL 3.x KDF
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    if (kdf) {
        EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
        if (ctx) {
            OSSL_PARAM params[7];
            params[0] = OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_PASSWORD, 
                const_cast<char*>(password.data()), 
                password.size());
            params[1] = OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_SALT,
                const_cast<Byte*>(salt.data()),
                salt.size());
            params[2] = OSSL_PARAM_construct_uint32(
                OSSL_KDF_PARAM_ITER, &timeCost);
            params[3] = OSSL_PARAM_construct_uint32(
                OSSL_KDF_PARAM_ARGON2_LANES, &parallelism);
            params[4] = OSSL_PARAM_construct_uint32(
                OSSL_KDF_PARAM_THREADS, &parallelism);
            params[5] = OSSL_PARAM_construct_uint32(
                OSSL_KDF_PARAM_ARGON2_MEMCOST, &memoryCost);
            params[6] = OSSL_PARAM_construct_end();
            
            if (EVP_KDF_derive(ctx, key.data(), AES_KEY_SIZE, params) > 0) {
                EVP_KDF_CTX_free(ctx);
                EVP_KDF_free(kdf);
                return key;
            }
            EVP_KDF_CTX_free(ctx);
        }
        EVP_KDF_free(kdf);
    }
    
    // Fallback to PBKDF2 if Argon2id is not available
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            salt.data(), static_cast<int>(salt.size()),
            static_cast<int>(timeCost * 10000),  // More iterations for PBKDF2
            EVP_sha256(),
            AES_KEY_SIZE, key.data()) == 1) {
        return key;
    }
#endif
    
    // Software fallback: PBKDF2-like construction using SHA256
    // This is less secure than Argon2id but still reasonable
    std::vector<Byte> data;
    data.insert(data.end(), password.begin(), password.end());
    data.insert(data.end(), salt.begin(), salt.end());
    
    Hash256 hash = SHA256Hash(data.data(), data.size());
    
    // Use higher iteration count for software implementation
    uint32_t iterations = timeCost * 10000;
    for (uint32_t i = 0; i < iterations; ++i) {
        Byte counter[4] = {
            static_cast<Byte>((i >> 24) & 0xFF),
            static_cast<Byte>((i >> 16) & 0xFF),
            static_cast<Byte>((i >> 8) & 0xFF),
            static_cast<Byte>(i & 0xFF)
        };
        
        SHA256 sha;
        sha.Write(hash.data(), hash.size());
        sha.Write(counter, 4);
        sha.Write(salt.data(), salt.size());
        sha.Finalize(hash.data());
    }
    
    std::memcpy(key.data(), hash.data(), AES_KEY_SIZE);
    return key;
}

std::vector<Byte> CryptoEngine::Encrypt(
    const std::array<Byte, AES_KEY_SIZE>& key,
    const std::array<Byte, AES_NONCE_SIZE>& nonce,
    const std::vector<Byte>& plaintext,
    const std::vector<Byte>& aad) {
    
#ifdef SHURIUM_USE_OPENSSL
    // Use real AES-256-GCM via OpenSSL
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    std::vector<Byte> ciphertext(plaintext.size() + AES_TAG_SIZE);
    
    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize AES-GCM");
    }
    
    // Set IV length
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_NONCE_SIZE, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set IV length");
    }
    
    // Set key and IV
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set key/IV");
    }
    
    // Add AAD if present
    int len = 0;
    if (!aad.empty()) {
        if (EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), 
                              static_cast<int>(aad.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to add AAD");
        }
    }
    
    // Encrypt plaintext
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to encrypt");
    }
    int ciphertext_len = len;
    
    // Finalize
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize encryption");
    }
    ciphertext_len += len;
    
    // Get authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_TAG_SIZE,
                            ciphertext.data() + ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to get auth tag");
    }
    
    ciphertext.resize(ciphertext_len + AES_TAG_SIZE);
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
    
#else
    // Software fallback using CTR mode with HMAC authentication
    // This provides authenticated encryption without OpenSSL
    std::vector<Byte> ciphertext;
    ciphertext.reserve(plaintext.size() + AES_TAG_SIZE);
    
    // Generate keystream using hash-based CTR mode
    size_t keyStreamLen = plaintext.size();
    std::vector<Byte> keyStream(keyStreamLen);
    
    SHA256 sha;
    size_t generated = 0;
    uint32_t counter = 0;
    
    while (generated < keyStreamLen) {
        sha.Reset();
        sha.Write(key.data(), key.size());
        sha.Write(nonce.data(), nonce.size());
        
        Byte ctr[4] = {
            static_cast<Byte>((counter >> 24) & 0xFF),
            static_cast<Byte>((counter >> 16) & 0xFF),
            static_cast<Byte>((counter >> 8) & 0xFF),
            static_cast<Byte>(counter & 0xFF)
        };
        sha.Write(ctr, 4);
        
        Hash256 block;
        sha.Finalize(block.data());
        
        size_t toCopy = std::min(size_t(32), keyStreamLen - generated);
        std::memcpy(keyStream.data() + generated, block.data(), toCopy);
        generated += toCopy;
        ++counter;
    }
    
    // XOR plaintext with keystream
    ciphertext.resize(plaintext.size());
    for (size_t i = 0; i < plaintext.size(); ++i) {
        ciphertext[i] = plaintext[i] ^ keyStream[i];
    }
    
    // Generate HMAC-like authentication tag
    // tag = SHA256(key || nonce || len(aad) || aad || len(ct) || ct)
    SHA256 tagSha;
    tagSha.Write(key.data(), key.size());
    tagSha.Write(nonce.data(), nonce.size());
    
    uint64_t aadLen = aad.size();
    tagSha.Write(reinterpret_cast<const Byte*>(&aadLen), 8);
    if (!aad.empty()) {
        tagSha.Write(aad.data(), aad.size());
    }
    
    uint64_t ctLen = ciphertext.size();
    tagSha.Write(reinterpret_cast<const Byte*>(&ctLen), 8);
    tagSha.Write(ciphertext.data(), ciphertext.size());
    
    Hash256 tagHash;
    tagSha.Finalize(tagHash.data());
    
    // Append first 16 bytes of hash as tag
    ciphertext.insert(ciphertext.end(), tagHash.begin(), tagHash.begin() + AES_TAG_SIZE);
    
    return ciphertext;
#endif
}

std::optional<std::vector<Byte>> CryptoEngine::Decrypt(
    const std::array<Byte, AES_KEY_SIZE>& key,
    const std::array<Byte, AES_NONCE_SIZE>& nonce,
    const std::vector<Byte>& ciphertext,
    const std::vector<Byte>& aad) {
    
    if (ciphertext.size() < AES_TAG_SIZE) {
        return std::nullopt;
    }
    
    size_t dataLen = ciphertext.size() - AES_TAG_SIZE;
    
#ifdef SHURIUM_USE_OPENSSL
    // Use real AES-256-GCM via OpenSSL
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return std::nullopt;
    }
    
    std::vector<Byte> plaintext(dataLen);
    
    // Initialize decryption
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    
    // Set IV length
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_NONCE_SIZE, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    
    // Set key and IV
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    
    // Add AAD if present
    int len = 0;
    if (!aad.empty()) {
        if (EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(),
                              static_cast<int>(aad.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return std::nullopt;
        }
    }
    
    // Decrypt ciphertext
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                          static_cast<int>(dataLen)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    int plaintext_len = len;
    
    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_TAG_SIZE,
                            const_cast<Byte*>(ciphertext.data() + dataLen)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    
    // Finalize and verify tag
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        // Authentication failed
        EVP_CIPHER_CTX_free(ctx);
        SecureZero(plaintext.data(), plaintext.size());
        return std::nullopt;
    }
    plaintext_len += len;
    
    plaintext.resize(plaintext_len);
    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
    
#else
    // Software fallback: verify tag then decrypt
    
    // Verify authentication tag
    SHA256 tagSha;
    tagSha.Write(key.data(), key.size());
    tagSha.Write(nonce.data(), nonce.size());
    
    uint64_t aadLen = aad.size();
    tagSha.Write(reinterpret_cast<const Byte*>(&aadLen), 8);
    if (!aad.empty()) {
        tagSha.Write(aad.data(), aad.size());
    }
    
    uint64_t ctLen = dataLen;
    tagSha.Write(reinterpret_cast<const Byte*>(&ctLen), 8);
    tagSha.Write(ciphertext.data(), dataLen);
    
    Hash256 expectedTag;
    tagSha.Finalize(expectedTag.data());
    
    // Compare tags (constant time)
    bool tagMatch = SecureCompare(ciphertext.data() + dataLen,
                                   expectedTag.data(), AES_TAG_SIZE);
    
    if (!tagMatch) {
        return std::nullopt;
    }
    
    // Generate keystream
    std::vector<Byte> keyStream(dataLen);
    SHA256 sha;
    size_t generated = 0;
    uint32_t counter = 0;
    
    while (generated < dataLen) {
        sha.Reset();
        sha.Write(key.data(), key.size());
        sha.Write(nonce.data(), nonce.size());
        
        Byte ctr[4] = {
            static_cast<Byte>((counter >> 24) & 0xFF),
            static_cast<Byte>((counter >> 16) & 0xFF),
            static_cast<Byte>((counter >> 8) & 0xFF),
            static_cast<Byte>(counter & 0xFF)
        };
        sha.Write(ctr, 4);
        
        Hash256 block;
        sha.Finalize(block.data());
        
        size_t toCopy = std::min(size_t(32), dataLen - generated);
        std::memcpy(keyStream.data() + generated, block.data(), toCopy);
        generated += toCopy;
        ++counter;
    }
    
    // XOR ciphertext with keystream
    std::vector<Byte> plaintext(dataLen);
    for (size_t i = 0; i < dataLen; ++i) {
        plaintext[i] = ciphertext[i] ^ keyStream[i];
    }
    
    return plaintext;
#endif
}

std::array<Byte, SALT_SIZE> CryptoEngine::GenerateSalt() {
    std::array<Byte, SALT_SIZE> salt{};
    GetRandBytes(salt.data(), SALT_SIZE);
    return salt;
}

std::array<Byte, AES_NONCE_SIZE> CryptoEngine::GenerateNonce() {
    std::array<Byte, AES_NONCE_SIZE> nonce{};
    GetRandBytes(nonce.data(), AES_NONCE_SIZE);
    return nonce;
}

void CryptoEngine::SecureZero(void* ptr, size_t size) {
    volatile Byte* p = static_cast<volatile Byte*>(ptr);
    while (size--) {
        *p++ = 0;
    }
}

bool CryptoEngine::LockMemory(void* ptr, size_t size) {
#ifdef _WIN32
    return VirtualLock(ptr, size) != 0;
#else
    return mlock(ptr, size) == 0;
#endif
}

bool CryptoEngine::UnlockMemory(void* ptr, size_t size) {
#ifdef _WIN32
    return VirtualUnlock(ptr, size) != 0;
#else
    return munlock(ptr, size) == 0;
#endif
}

// ============================================================================
// MemoryKeyStore Implementation
// ============================================================================

MemoryKeyStore::MemoryKeyStore() = default;

MemoryKeyStore::MemoryKeyStore(const std::string& password) {
    SetupEncryption(password);
}

bool MemoryKeyStore::SetupEncryption(const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (encrypted_) {
        return false;  // Already encrypted
    }
    
    masterSalt_ = CryptoEngine::GenerateSalt();
    auto key = CryptoEngine::DeriveKey(password, masterSalt_);
    
    masterKey_ = std::make_unique<SecureArray<Byte, AES_KEY_SIZE>>();
    std::memcpy(masterKey_->data(), key.data(), AES_KEY_SIZE);
    
    // Create password verification token (encrypt a known string)
    static const std::string VERIFICATION_MAGIC = "SHURIUM_KEYSTORE_V2";
    std::vector<Byte> verificationPlaintext(VERIFICATION_MAGIC.begin(), VERIFICATION_MAGIC.end());
    verificationNonce_ = CryptoEngine::GenerateNonce();
    verificationToken_ = CryptoEngine::Encrypt(key, verificationNonce_, verificationPlaintext);
    
    // Encrypt existing unencrypted seed if present
    if (unencryptedSeed_) {
        auto nonce = CryptoEngine::GenerateNonce();
        std::vector<Byte> plaintext(unencryptedSeed_->begin(), unencryptedSeed_->end());
        auto ciphertext = CryptoEngine::Encrypt(key, nonce, plaintext);
        
        encryptedSeed_.salt = masterSalt_;
        encryptedSeed_.nonce = nonce;
        encryptedSeed_.ciphertext = ciphertext;
        
        // Clear unencrypted seed
        CryptoEngine::SecureZero(unencryptedSeed_->data(), unencryptedSeed_->size());
        unencryptedSeed_.reset();
    }
    
    // Encrypt existing unlocked keys
    for (auto& [hash, privKey] : unlockedKeys_) {
        auto encKey = EncryptKey(privKey, key, "", std::nullopt);
        encryptedKeys_[hash] = encKey;
    }
    
    encrypted_ = true;
    unlocked_ = true;
    
    return true;
}

bool MemoryKeyStore::IsEncrypted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return encrypted_;
}

bool MemoryKeyStore::IsLocked() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return encrypted_ && !unlocked_;
}

void MemoryKeyStore::Lock() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!encrypted_) {
        return;
    }
    
    // Clear decrypted data
    unlockedKeys_.clear();
    masterKey_.reset();
    hdKeyManager_.reset();
    unlockedIdentity_.reset();
    
    unlocked_ = false;
}

bool MemoryKeyStore::Unlock(const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!encrypted_) {
        return true;  // Nothing to unlock
    }
    
    if (unlocked_) {
        return true;  // Already unlocked
    }
    
    // Derive key from password
    auto key = CryptoEngine::DeriveKey(password, masterSalt_);
    
    // Verify password by trying to decrypt something
    bool passwordVerified = false;
    
    // First try the verification token (always present for encrypted stores)
    if (!verificationToken_.empty()) {
        static const std::string VERIFICATION_MAGIC = "SHURIUM_KEYSTORE_V2";
        auto decrypted = CryptoEngine::Decrypt(key, verificationNonce_, verificationToken_);
        if (decrypted && decrypted->size() == VERIFICATION_MAGIC.size()) {
            std::string decryptedStr(decrypted->begin(), decrypted->end());
            if (decryptedStr == VERIFICATION_MAGIC) {
                passwordVerified = true;
            }
        }
        if (!passwordVerified) {
            return false;  // Wrong password
        }
    }
    
    // Try to decrypt a key to verify password (for legacy wallets without verification token)
    if (!passwordVerified && !encryptedKeys_.empty()) {
        auto& [hash, encrypted] = *encryptedKeys_.begin();
        auto decrypted = DecryptKey(encrypted, key);
        if (!decrypted) {
            return false;  // Wrong password
        }
        passwordVerified = true;
    }
    
    // If no keys, try to verify against encrypted seed (for legacy wallets)
    if (!passwordVerified && encryptedSeed_.IsValid()) {
        auto decrypted = CryptoEngine::Decrypt(key, encryptedSeed_.nonce,
                                                encryptedSeed_.ciphertext);
        if (!decrypted || decrypted->size() != BIP39_SEED_SIZE) {
            return false;  // Wrong password
        }
        passwordVerified = true;
    }
    
    // If nothing to verify against, fail
    if (!passwordVerified) {
        return false;
    }
    
    // Store master key
    masterKey_ = std::make_unique<SecureArray<Byte, AES_KEY_SIZE>>();
    std::memcpy(masterKey_->data(), key.data(), AES_KEY_SIZE);
    
    // Decrypt all keys
    for (const auto& [hash, encrypted] : encryptedKeys_) {
        auto decrypted = DecryptKey(encrypted, key);
        if (decrypted) {
            unlockedKeys_[hash] = std::move(*decrypted);
        }
    }
    
    // Decrypt master seed and initialize HD key manager
    if (encryptedSeed_.IsValid()) {
        auto decrypted = CryptoEngine::Decrypt(key, encryptedSeed_.nonce,
                                                encryptedSeed_.ciphertext);
        if (decrypted && decrypted->size() == BIP39_SEED_SIZE) {
            std::array<Byte, BIP39_SEED_SIZE> seed;
            std::memcpy(seed.data(), decrypted->data(), BIP39_SEED_SIZE);
            
            auto masterKey = ExtendedKey::FromBIP39Seed(seed);
            HDKeyManager::Config hdConfig;
            hdConfig.testnet = testnet_;
            hdKeyManager_ = std::make_unique<HDKeyManager>(masterKey, hdConfig);
            
            // Restore HD key indices from persisted storage
            if (!hdKeyIndices_.empty()) {
                hdKeyManager_->SetAllIndices(hdKeyIndices_);
            }
            
            // Secure clear
            CryptoEngine::SecureZero(seed.data(), seed.size());
            CryptoEngine::SecureZero(decrypted->data(), decrypted->size());
        }
    }
    
    unlocked_ = true;
    return true;
}

bool MemoryKeyStore::CheckPassword(const std::string& password) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!encrypted_) {
        return true;
    }
    
    auto key = CryptoEngine::DeriveKey(password, masterSalt_);
    
    // Try to decrypt a key to verify
    if (!encryptedKeys_.empty()) {
        auto& [hash, encrypted] = *encryptedKeys_.begin();
        return DecryptKey(encrypted, key).has_value();
    }
    
    // If we have a seed, try that
    if (encryptedSeed_.IsValid()) {
        auto decrypted = CryptoEngine::Decrypt(key, encryptedSeed_.nonce,
                                                encryptedSeed_.ciphertext);
        return decrypted.has_value();
    }
    
    return true;  // No keys to verify against
}

bool MemoryKeyStore::ChangePassword(const std::string& oldPassword,
                                     const std::string& newPassword) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!encrypted_) {
        return SetupEncryption(newPassword);
    }
    
    // Verify old password using verification token if present
    auto oldKey = CryptoEngine::DeriveKey(oldPassword, masterSalt_);
    bool verified = false;
    
    if (!verificationToken_.empty()) {
        static const std::string VERIFICATION_MAGIC = "SHURIUM_KEYSTORE_V2";
        auto decrypted = CryptoEngine::Decrypt(oldKey, verificationNonce_, verificationToken_);
        if (decrypted && decrypted->size() == VERIFICATION_MAGIC.size()) {
            std::string decryptedStr(decrypted->begin(), decrypted->end());
            if (decryptedStr == VERIFICATION_MAGIC) {
                verified = true;
            }
        }
    }
    
    // Fallback to key verification for legacy wallets
    if (!verified && !encryptedKeys_.empty()) {
        auto& [hash, encrypted] = *encryptedKeys_.begin();
        if (DecryptKey(encrypted, oldKey)) {
            verified = true;
        }
    }
    
    if (!verified) {
        return false;  // Wrong old password
    }
    
    // Generate new salt and key
    auto newSalt = CryptoEngine::GenerateSalt();
    auto newKey = CryptoEngine::DeriveKey(newPassword, newSalt);
    
    // Re-encrypt verification token with new key
    if (!verificationToken_.empty()) {
        static const std::string VERIFICATION_MAGIC = "SHURIUM_KEYSTORE_V2";
        std::vector<Byte> verificationPlaintext(VERIFICATION_MAGIC.begin(), VERIFICATION_MAGIC.end());
        verificationNonce_ = CryptoEngine::GenerateNonce();
        verificationToken_ = CryptoEngine::Encrypt(newKey, verificationNonce_, verificationPlaintext);
    }
    
    // Re-encrypt all keys
    if (!ReencryptAll(oldKey, newKey)) {
        return false;
    }
    
    masterSalt_ = newSalt;
    
    // Update master key
    masterKey_ = std::make_unique<SecureArray<Byte, AES_KEY_SIZE>>();
    std::memcpy(masterKey_->data(), newKey.data(), AES_KEY_SIZE);
    
    return true;
}

bool MemoryKeyStore::AddKey(const PrivateKey& key, const std::string& label) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!key.IsValid()) {
        return false;
    }
    
    auto pubKey = key.GetPublicKey();
    auto keyHash = pubKey.GetHash160();
    
    // Check if key already exists
    if (encryptedKeys_.count(keyHash) > 0) {
        return true;  // Already have this key
    }
    
    if (encrypted_ && masterKey_) {
        // Encrypt and store
        std::array<Byte, AES_KEY_SIZE> encKey;
        std::memcpy(encKey.data(), masterKey_->data(), AES_KEY_SIZE);
        
        auto encrypted = EncryptKey(key, encKey, label);
        encryptedKeys_[keyHash] = encrypted;
        
        if (unlocked_) {
            unlockedKeys_[keyHash] = key;
        }
    } else if (!encrypted_) {
        // Store unencrypted (not recommended)
        unlockedKeys_[keyHash] = key;
    } else {
        return false;  // Encrypted but locked
    }
    
    publicKeys_[keyHash] = pubKey;
    return true;
}

bool MemoryKeyStore::AddWatchOnly(const PublicKey& pubkey, const std::string& label) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!pubkey.IsValid()) {
        return false;
    }
    
    auto keyHash = pubkey.GetHash160();
    watchOnlyKeys_.insert(keyHash);
    publicKeys_[keyHash] = pubkey;
    
    return true;
}

std::optional<PrivateKey> MemoryKeyStore::GetKey(const Hash160& keyHash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if watch-only
    if (watchOnlyKeys_.count(keyHash) > 0) {
        return std::nullopt;
    }
    
    // Check unlocked keys
    auto it = unlockedKeys_.find(keyHash);
    if (it != unlockedKeys_.end()) {
        return it->second;
    }
    
    // Try HD key manager
    if (hdKeyManager_) {
        auto info = hdKeyManager_->FindKeyByHash(keyHash);
        if (info) {
            // Key exists in HD manager - derive it using the stored path info
            // The path stored in info contains the full BIP44 path (e.g., m/44'/60'/0'/0/0)
            // We derive from the account key using change/index
            auto accountKey = hdKeyManager_->GetAccountKey(info->account);
            if (accountKey) {
                // From account key (m/44'/60'/account'), derive change/index
                auto changeKey = accountKey->DeriveChild(info->change);
                if (changeKey) {
                    auto indexKey = changeKey->DeriveChild(info->index);
                    if (indexKey) {
                        return indexKey->GetPrivateKey();
                    }
                }
            }
        }
    }
    
    return std::nullopt;
}

std::optional<PublicKey> MemoryKeyStore::GetPublicKey(const Hash160& keyHash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = publicKeys_.find(keyHash);
    if (it != publicKeys_.end()) {
        return it->second;
    }
    
    // Check HD key manager
    if (hdKeyManager_) {
        auto info = hdKeyManager_->FindKeyByHash(keyHash);
        if (info) {
            return info->publicKey;
        }
    }
    
    return std::nullopt;
}

bool MemoryKeyStore::HaveKey(const Hash160& keyHash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check encrypted keys
    if (encryptedKeys_.count(keyHash) > 0) {
        return true;
    }
    
    // Check unlocked keys
    if (unlockedKeys_.count(keyHash) > 0) {
        return true;
    }
    
    // Check watch-only
    if (watchOnlyKeys_.count(keyHash) > 0) {
        return true;
    }
    
    // Check HD key manager
    if (hdKeyManager_) {
        return hdKeyManager_->FindKeyByHash(keyHash).has_value();
    }
    
    return false;
}

bool MemoryKeyStore::IsWatchOnly(const Hash160& keyHash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return watchOnlyKeys_.count(keyHash) > 0;
}

std::vector<Hash160> MemoryKeyStore::GetKeyHashes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::set<Hash160> hashSet;  // Use set to avoid duplicates
    
    // Add encrypted keys
    for (const auto& [hash, _] : encryptedKeys_) {
        hashSet.insert(hash);
    }
    
    // Add unlocked keys (that aren't already in encrypted)
    for (const auto& [hash, _] : unlockedKeys_) {
        hashSet.insert(hash);
    }
    
    // Add watch-only keys
    for (const auto& hash : watchOnlyKeys_) {
        hashSet.insert(hash);
    }
    
    // Add HD-derived keys from the key manager
    if (hdKeyManager_) {
        for (const auto& keyInfo : hdKeyManager_->GetAllKeys()) {
            hashSet.insert(keyInfo.keyHash);
        }
    }
    
    return std::vector<Hash160>(hashSet.begin(), hashSet.end());
}

std::optional<std::vector<Byte>> MemoryKeyStore::Sign(const Hash160& keyHash,
                                                       const Hash256& hash) const {
    auto key = GetKey(keyHash);
    if (!key) {
        return std::nullopt;
    }
    return key->Sign(hash);
}

void MemoryKeyStore::SetTestnet(bool testnet) {
    std::lock_guard<std::mutex> lock(mutex_);
    testnet_ = testnet;
    
    // Check if we need deferred HD initialization (unencrypted wallet loaded from file)
    if (needsDeferredHDInit_ && unencryptedSeed_) {
        auto masterKey = ExtendedKey::FromBIP39Seed(*unencryptedSeed_);
        HDKeyManager::Config hdConfig;
        hdConfig.testnet = testnet_;
        hdKeyManager_ = std::make_unique<HDKeyManager>(masterKey, hdConfig);
        
        // Restore HD key indices from persisted storage
        if (!hdKeyIndices_.empty()) {
            hdKeyManager_->SetAllIndices(hdKeyIndices_);
        }
        
        unlocked_ = true;
        needsDeferredHDInit_ = false;
    }
}

bool MemoryKeyStore::SetMasterSeed(const std::array<Byte, BIP39_SEED_SIZE>& seed) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Initialize HD key manager from seed with testnet config
    auto masterKey = ExtendedKey::FromBIP39Seed(seed);
    HDKeyManager::Config hdConfig;
    hdConfig.testnet = testnet_;
    hdKeyManager_ = std::make_unique<HDKeyManager>(masterKey, hdConfig);
    
    // Restore HD key indices if any were persisted
    if (!hdKeyIndices_.empty()) {
        hdKeyManager_->SetAllIndices(hdKeyIndices_);
    }
    
    // Compute mnemonic checksum
    encryptedSeed_.mnemonicChecksum = SHA256Hash(seed.data(), seed.size());
    encryptedSeed_.masterPublicKey = masterKey.Neuter();
    encryptedSeed_.created = std::chrono::system_clock::now().time_since_epoch().count();
    
    if (encrypted_ && masterKey_) {
        // Encrypt seed for encrypted wallets
        std::array<Byte, AES_KEY_SIZE> key;
        std::memcpy(key.data(), masterKey_->data(), AES_KEY_SIZE);
        
        auto nonce = CryptoEngine::GenerateNonce();
        std::vector<Byte> plaintext(seed.begin(), seed.end());
        auto ciphertext = CryptoEngine::Encrypt(key, nonce, plaintext);
        
        encryptedSeed_.salt = masterSalt_;
        encryptedSeed_.nonce = nonce;
        encryptedSeed_.ciphertext = ciphertext;
    } else {
        // For unencrypted wallets, store seed as plaintext in ciphertext field
        // This allows wallet functionality without a password
        // WARNING: Not recommended for production - use encryptwallet to secure!
        encryptedSeed_.salt = masterSalt_;
        encryptedSeed_.nonce = {};  // Zero nonce indicates unencrypted
        encryptedSeed_.ciphertext = std::vector<Byte>(seed.begin(), seed.end());
        unencryptedSeed_ = seed;
    }
    
    return true;
}

bool MemoryKeyStore::SetFromMnemonic(const std::string& mnemonic,
                                      const std::string& passphrase) {
    auto seed = Mnemonic::ToSeed(mnemonic, passphrase);
    return SetMasterSeed(seed);
}

HDKeyManager* MemoryKeyStore::GetHDKeyManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    return hdKeyManager_.get();
}

const HDKeyManager* MemoryKeyStore::GetHDKeyManager() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hdKeyManager_.get();
}

std::optional<PublicKey> MemoryKeyStore::DeriveNextReceiving(uint32_t account) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!hdKeyManager_) {
        return std::nullopt;
    }
    
    auto info = hdKeyManager_->DeriveNextReceiving(account);
    publicKeys_[info.keyHash] = info.publicKey;
    
    // Update persisted indices
    hdKeyIndices_ = hdKeyManager_->GetAllIndices();
    
    return info.publicKey;
}

std::optional<PublicKey> MemoryKeyStore::DeriveNextChange(uint32_t account) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!hdKeyManager_) {
        return std::nullopt;
    }
    
    auto info = hdKeyManager_->DeriveNextChange(account);
    publicKeys_[info.keyHash] = info.publicKey;
    
    // Update persisted indices
    hdKeyIndices_ = hdKeyManager_->GetAllIndices();
    
    return info.publicKey;
}

bool MemoryKeyStore::SetIdentitySecrets(const identity::IdentitySecrets& secrets) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!encrypted_ || !masterKey_) {
        return false;
    }
    
    // Serialize identity secrets
    std::vector<Byte> plaintext;
    // ... serialize secrets to plaintext
    // For now, just store the commitment hash
    plaintext.resize(32);
    auto commitment = secrets.GetCommitment();
    const auto& commitHash = commitment.GetHash();
    std::memcpy(plaintext.data(), commitHash.data(), 32);
    
    // Encrypt
    std::array<Byte, AES_KEY_SIZE> key;
    std::memcpy(key.data(), masterKey_->data(), AES_KEY_SIZE);
    
    auto nonce = CryptoEngine::GenerateNonce();
    auto ciphertext = CryptoEngine::Encrypt(key, nonce, plaintext);
    
    encryptedIdentity_.salt = masterSalt_;
    encryptedIdentity_.nonce = nonce;
    encryptedIdentity_.ciphertext = ciphertext;
    // Convert IdentityCommitment to Hash256 for storage
    encryptedIdentity_.commitment = Hash256(commitHash.data(), 32);
    encryptedIdentity_.created = std::chrono::system_clock::now().time_since_epoch().count();
    
    unlockedIdentity_ = secrets;
    
    return true;
}

std::optional<identity::IdentitySecrets> MemoryKeyStore::GetIdentitySecrets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return unlockedIdentity_;
}

Hash256 MemoryKeyStore::GetIdentityCommitment() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return encryptedIdentity_.commitment;
}

size_t MemoryKeyStore::KeyCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return encryptedKeys_.size() + unlockedKeys_.size();
}

size_t MemoryKeyStore::WatchOnlyCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return watchOnlyKeys_.size();
}

EncryptedKey MemoryKeyStore::EncryptKey(const PrivateKey& key,
                                         const std::array<Byte, AES_KEY_SIZE>& encKey,
                                         const std::string& label,
                                         const std::optional<DerivationPath>& path) {
    EncryptedKey result;
    result.salt = masterSalt_;
    result.nonce = CryptoEngine::GenerateNonce();
    result.publicKey = key.GetPublicKey();
    result.path = path;
    result.created = std::chrono::system_clock::now().time_since_epoch().count();
    result.label = label;
    
    std::vector<Byte> plaintext(key.data(), key.data() + 32);
    result.ciphertext = CryptoEngine::Encrypt(encKey, result.nonce, plaintext);
    
    // Secure clear
    CryptoEngine::SecureZero(plaintext.data(), plaintext.size());
    
    return result;
}

std::optional<PrivateKey> MemoryKeyStore::DecryptKey(const EncryptedKey& encrypted,
                                                      const std::array<Byte, AES_KEY_SIZE>& encKey) const {
    auto plaintext = CryptoEngine::Decrypt(encKey, encrypted.nonce, encrypted.ciphertext);
    if (!plaintext || plaintext->size() != 32) {
        return std::nullopt;
    }
    
    PrivateKey key(plaintext->data(), true);
    
    // Secure clear
    CryptoEngine::SecureZero(plaintext->data(), plaintext->size());
    
    if (!key.IsValid()) {
        return std::nullopt;
    }
    
    return key;
}

bool MemoryKeyStore::ReencryptAll(const std::array<Byte, AES_KEY_SIZE>& oldKey,
                                   const std::array<Byte, AES_KEY_SIZE>& newKey) {
    // Re-encrypt all keys
    std::map<Hash160, EncryptedKey> newEncryptedKeys;
    
    for (auto& [hash, encrypted] : encryptedKeys_) {
        auto decrypted = DecryptKey(encrypted, oldKey);
        if (!decrypted) {
            return false;
        }
        
        auto newEncrypted = EncryptKey(*decrypted, newKey, encrypted.label, encrypted.path);
        newEncryptedKeys[hash] = newEncrypted;
    }
    
    // Re-encrypt seed
    EncryptedSeed newEncryptedSeed;
    if (encryptedSeed_.IsValid()) {
        auto decrypted = CryptoEngine::Decrypt(oldKey, encryptedSeed_.nonce,
                                                encryptedSeed_.ciphertext);
        if (!decrypted) {
            return false;
        }
        
        auto nonce = CryptoEngine::GenerateNonce();
        newEncryptedSeed.salt = masterSalt_;
        newEncryptedSeed.nonce = nonce;
        newEncryptedSeed.ciphertext = CryptoEngine::Encrypt(newKey, nonce, *decrypted);
        newEncryptedSeed.masterPublicKey = encryptedSeed_.masterPublicKey;
        newEncryptedSeed.created = encryptedSeed_.created;
        newEncryptedSeed.mnemonicChecksum = encryptedSeed_.mnemonicChecksum;
    }
    
    // Re-encrypt identity
    EncryptedIdentity newEncryptedIdentity;
    if (encryptedIdentity_.IsValid()) {
        auto decrypted = CryptoEngine::Decrypt(oldKey, encryptedIdentity_.nonce,
                                                encryptedIdentity_.ciphertext);
        if (!decrypted) {
            return false;
        }
        
        auto nonce = CryptoEngine::GenerateNonce();
        newEncryptedIdentity.salt = masterSalt_;
        newEncryptedIdentity.nonce = nonce;
        newEncryptedIdentity.ciphertext = CryptoEngine::Encrypt(newKey, nonce, *decrypted);
        newEncryptedIdentity.commitment = encryptedIdentity_.commitment;
        newEncryptedIdentity.created = encryptedIdentity_.created;
    }
    
    // Commit changes
    encryptedKeys_ = std::move(newEncryptedKeys);
    if (newEncryptedSeed.IsValid()) {
        encryptedSeed_ = newEncryptedSeed;
    }
    if (newEncryptedIdentity.IsValid()) {
        encryptedIdentity_ = newEncryptedIdentity;
    }
    
    return true;
}

// ============================================================================
// FileKeyStore Implementation
// ============================================================================

FileKeyStore::FileKeyStore() = default;

FileKeyStore::FileKeyStore(const std::string& path) {
    Load(path);
}

bool FileKeyStore::Load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Read file contents
    std::vector<Byte> data((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    
    if (!Deserialize(data)) {
        return false;
    }
    
    path_ = path;
    return true;
}

bool FileKeyStore::Save(const std::string& path) const {
    auto data = Serialize();
    
    // Use SecureWriteFile to set restrictive permissions (0600)
    util::fs::Path fsPath(path);
    return util::fs::SecureWriteFile(fsPath, data.data(), data.size());
}

bool FileKeyStore::Save() const {
    if (path_.empty()) {
        return false;
    }
    return Save(path_);
}

std::vector<Byte> FileKeyStore::Serialize() const {
    std::vector<Byte> data;
    
    // Header
    data.push_back((FILE_MAGIC >> 24) & 0xFF);
    data.push_back((FILE_MAGIC >> 16) & 0xFF);
    data.push_back((FILE_MAGIC >> 8) & 0xFF);
    data.push_back(FILE_MAGIC & 0xFF);
    
    data.push_back((FILE_VERSION >> 24) & 0xFF);
    data.push_back((FILE_VERSION >> 16) & 0xFF);
    data.push_back((FILE_VERSION >> 8) & 0xFF);
    data.push_back(FILE_VERSION & 0xFF);
    
    // Flags
    uint32_t flags = 0;
    if (encrypted_) flags |= 0x01;
    if (encryptedSeed_.IsValid()) flags |= 0x02;
    if (encryptedIdentity_.IsValid()) flags |= 0x04;
    if (!verificationToken_.empty()) flags |= 0x08;  // Has verification token
    
    data.push_back((flags >> 24) & 0xFF);
    data.push_back((flags >> 16) & 0xFF);
    data.push_back((flags >> 8) & 0xFF);
    data.push_back(flags & 0xFF);
    
    // Master salt
    data.insert(data.end(), masterSalt_.begin(), masterSalt_.end());
    
    // Key count
    uint32_t keyCount = static_cast<uint32_t>(encryptedKeys_.size());
    data.push_back((keyCount >> 24) & 0xFF);
    data.push_back((keyCount >> 16) & 0xFF);
    data.push_back((keyCount >> 8) & 0xFF);
    data.push_back(keyCount & 0xFF);
    
    // Encrypted keys
    for (const auto& [hash, key] : encryptedKeys_) {
        // Hash
        data.insert(data.end(), hash.begin(), hash.end());
        
        // Nonce
        data.insert(data.end(), key.nonce.begin(), key.nonce.end());
        
        // Ciphertext length and data
        uint32_t ctLen = static_cast<uint32_t>(key.ciphertext.size());
        data.push_back((ctLen >> 24) & 0xFF);
        data.push_back((ctLen >> 16) & 0xFF);
        data.push_back((ctLen >> 8) & 0xFF);
        data.push_back(ctLen & 0xFF);
        data.insert(data.end(), key.ciphertext.begin(), key.ciphertext.end());
        
        // Public key (33 bytes compressed)
        auto pubKeyData = key.publicKey.GetCompressed();
        data.insert(data.end(), pubKeyData.begin(), pubKeyData.end());
    }
    
    // === Serialize encrypted seed (if present) ===
    if (encryptedSeed_.IsValid()) {
        // Salt
        data.insert(data.end(), encryptedSeed_.salt.begin(), encryptedSeed_.salt.end());
        
        // Nonce
        data.insert(data.end(), encryptedSeed_.nonce.begin(), encryptedSeed_.nonce.end());
        
        // Ciphertext length and data
        uint32_t ctLen = static_cast<uint32_t>(encryptedSeed_.ciphertext.size());
        data.push_back((ctLen >> 24) & 0xFF);
        data.push_back((ctLen >> 16) & 0xFF);
        data.push_back((ctLen >> 8) & 0xFF);
        data.push_back(ctLen & 0xFF);
        data.insert(data.end(), encryptedSeed_.ciphertext.begin(), encryptedSeed_.ciphertext.end());
        
        // Master public key (serialized as Base58 xpub)
        std::string xpub = encryptedSeed_.masterPublicKey.ToBase58(false);
        uint32_t xpubLen = static_cast<uint32_t>(xpub.size());
        data.push_back((xpubLen >> 24) & 0xFF);
        data.push_back((xpubLen >> 16) & 0xFF);
        data.push_back((xpubLen >> 8) & 0xFF);
        data.push_back(xpubLen & 0xFF);
        data.insert(data.end(), xpub.begin(), xpub.end());
        
        // Creation timestamp (8 bytes, big-endian)
        int64_t created = encryptedSeed_.created;
        data.push_back((created >> 56) & 0xFF);
        data.push_back((created >> 48) & 0xFF);
        data.push_back((created >> 40) & 0xFF);
        data.push_back((created >> 32) & 0xFF);
        data.push_back((created >> 24) & 0xFF);
        data.push_back((created >> 16) & 0xFF);
        data.push_back((created >> 8) & 0xFF);
        data.push_back(created & 0xFF);
        
        // Mnemonic checksum (32 bytes)
        data.insert(data.end(), encryptedSeed_.mnemonicChecksum.begin(), 
                    encryptedSeed_.mnemonicChecksum.end());
    }
    
    // === Serialize encrypted identity (if present) ===
    if (encryptedIdentity_.IsValid()) {
        // Salt
        data.insert(data.end(), encryptedIdentity_.salt.begin(), encryptedIdentity_.salt.end());
        
        // Nonce
        data.insert(data.end(), encryptedIdentity_.nonce.begin(), encryptedIdentity_.nonce.end());
        
        // Ciphertext length and data
        uint32_t ctLen = static_cast<uint32_t>(encryptedIdentity_.ciphertext.size());
        data.push_back((ctLen >> 24) & 0xFF);
        data.push_back((ctLen >> 16) & 0xFF);
        data.push_back((ctLen >> 8) & 0xFF);
        data.push_back(ctLen & 0xFF);
        data.insert(data.end(), encryptedIdentity_.ciphertext.begin(), encryptedIdentity_.ciphertext.end());
        
        // Commitment (32 bytes)
        data.insert(data.end(), encryptedIdentity_.commitment.begin(), 
                    encryptedIdentity_.commitment.end());
        
        // Creation timestamp (8 bytes, big-endian)
        int64_t created = encryptedIdentity_.created;
        data.push_back((created >> 56) & 0xFF);
        data.push_back((created >> 48) & 0xFF);
        data.push_back((created >> 40) & 0xFF);
        data.push_back((created >> 32) & 0xFF);
        data.push_back((created >> 24) & 0xFF);
        data.push_back((created >> 16) & 0xFF);
        data.push_back((created >> 8) & 0xFF);
        data.push_back(created & 0xFF);
    }
    
    // === Serialize HD key indices (added in version 2) ===
    uint32_t indexCount = static_cast<uint32_t>(hdKeyIndices_.size());
    data.push_back((indexCount >> 24) & 0xFF);
    data.push_back((indexCount >> 16) & 0xFF);
    data.push_back((indexCount >> 8) & 0xFF);
    data.push_back(indexCount & 0xFF);
    
    for (const auto& [key, index] : hdKeyIndices_) {
        // Account (4 bytes)
        uint32_t account = key.first;
        data.push_back((account >> 24) & 0xFF);
        data.push_back((account >> 16) & 0xFF);
        data.push_back((account >> 8) & 0xFF);
        data.push_back(account & 0xFF);
        
        // Change (4 bytes)
        uint32_t change = key.second;
        data.push_back((change >> 24) & 0xFF);
        data.push_back((change >> 16) & 0xFF);
        data.push_back((change >> 8) & 0xFF);
        data.push_back(change & 0xFF);
        
        // Index (4 bytes)
        data.push_back((index >> 24) & 0xFF);
        data.push_back((index >> 16) & 0xFF);
        data.push_back((index >> 8) & 0xFF);
        data.push_back(index & 0xFF);
    }
    
    // === Serialize verification token (added in version 3) ===
    if (!verificationToken_.empty()) {
        // Nonce (12 bytes)
        data.insert(data.end(), verificationNonce_.begin(), verificationNonce_.end());
        
        // Token length and data
        uint32_t tokenLen = static_cast<uint32_t>(verificationToken_.size());
        data.push_back((tokenLen >> 24) & 0xFF);
        data.push_back((tokenLen >> 16) & 0xFF);
        data.push_back((tokenLen >> 8) & 0xFF);
        data.push_back(tokenLen & 0xFF);
        data.insert(data.end(), verificationToken_.begin(), verificationToken_.end());
    }
    
    return data;
}

bool FileKeyStore::Deserialize(const std::vector<Byte>& data) {
    // Minimum size: header (12) + salt (32) + key count (4) = 48 bytes
    if (data.size() < 48) {
        return false;
    }
    
    size_t offset = 0;
    
    // Helper lambda to read 4-byte big-endian uint32
    auto readUint32 = [&data, &offset]() -> uint32_t {
        if (offset + 4 > data.size()) {
            throw std::runtime_error("Buffer underflow reading uint32");
        }
        uint32_t value = (static_cast<uint32_t>(data[offset]) << 24) |
                         (static_cast<uint32_t>(data[offset + 1]) << 16) |
                         (static_cast<uint32_t>(data[offset + 2]) << 8) |
                         static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
        return value;
    };
    
    // Helper lambda to read 8-byte big-endian int64
    auto readInt64 = [&data, &offset]() -> int64_t {
        if (offset + 8 > data.size()) {
            throw std::runtime_error("Buffer underflow reading int64");
        }
        int64_t value = (static_cast<int64_t>(data[offset]) << 56) |
                        (static_cast<int64_t>(data[offset + 1]) << 48) |
                        (static_cast<int64_t>(data[offset + 2]) << 40) |
                        (static_cast<int64_t>(data[offset + 3]) << 32) |
                        (static_cast<int64_t>(data[offset + 4]) << 24) |
                        (static_cast<int64_t>(data[offset + 5]) << 16) |
                        (static_cast<int64_t>(data[offset + 6]) << 8) |
                        static_cast<int64_t>(data[offset + 7]);
        offset += 8;
        return value;
    };
    
    // Helper lambda to read bytes into an array
    auto readBytes = [&data, &offset](void* dest, size_t count) {
        if (offset + count > data.size()) {
            throw std::runtime_error("Buffer underflow reading bytes");
        }
        std::memcpy(dest, data.data() + offset, count);
        offset += count;
    };
    
    // Helper lambda to read a variable-length byte vector
    auto readVector = [&data, &offset, &readUint32]() -> std::vector<Byte> {
        uint32_t len = readUint32();
        if (offset + len > data.size()) {
            throw std::runtime_error("Buffer underflow reading vector");
        }
        std::vector<Byte> result(data.begin() + offset, data.begin() + offset + len);
        offset += len;
        return result;
    };
    
    try {
        // === Read header ===
        uint32_t magic = readUint32();
        if (magic != FILE_MAGIC) {
            return false;
        }
        
        uint32_t version = readUint32();
        if (version > FILE_VERSION) {
            return false;  // Unsupported version
        }
        
        uint32_t flags = readUint32();
        encrypted_ = (flags & 0x01) != 0;
        bool hasSeed = (flags & 0x02) != 0;
        bool hasIdentity = (flags & 0x04) != 0;
        
        // === Read master salt (32 bytes) ===
        readBytes(masterSalt_.data(), SALT_SIZE);
        
        // === Read encrypted keys ===
        uint32_t keyCount = readUint32();
        
        // Sanity check: prevent excessive memory allocation
        if (keyCount > 1000000) {
            return false;
        }
        
        encryptedKeys_.clear();
        publicKeys_.clear();  // Also clear public keys map
        for (uint32_t i = 0; i < keyCount; ++i) {
            EncryptedKey key;
            
            // The keys share the master salt
            key.salt = masterSalt_;
            
            // Read key hash (20 bytes)
            Hash160 keyHash;
            readBytes(keyHash.data(), 20);
            
            // Read nonce (12 bytes)
            readBytes(key.nonce.data(), AES_NONCE_SIZE);
            
            // Read ciphertext
            key.ciphertext = readVector();
            
            // Read public key (33 bytes compressed)
            std::array<Byte, 33> pubKeyData;
            readBytes(pubKeyData.data(), 33);
            PublicKey pubKey(pubKeyData);
            if (pubKey.IsValid()) {
                key.publicKey = pubKey;
                // Also store in publicKeys_ map for GetPublicKey()
                publicKeys_[keyHash] = pubKey;
            }
            
            encryptedKeys_[keyHash] = std::move(key);
        }
        
        // === Read encrypted seed (if present) ===
        if (hasSeed) {
            EncryptedSeed seed;
            
            // Salt (32 bytes)
            readBytes(seed.salt.data(), SALT_SIZE);
            
            // Nonce (12 bytes)
            readBytes(seed.nonce.data(), AES_NONCE_SIZE);
            
            // Ciphertext
            seed.ciphertext = readVector();
            
            // Master public key (xpub as string)
            uint32_t xpubLen = readUint32();
            if (offset + xpubLen > data.size()) {
                return false;
            }
            std::string xpub(reinterpret_cast<const char*>(data.data() + offset), xpubLen);
            offset += xpubLen;
            
            auto masterPubKey = ExtendedKey::FromBase58(xpub);
            if (!masterPubKey) {
                return false;  // Invalid xpub
            }
            seed.masterPublicKey = *masterPubKey;
            
            // Creation timestamp (8 bytes)
            seed.created = readInt64();
            
            // Mnemonic checksum (32 bytes)
            readBytes(seed.mnemonicChecksum.data(), 32);
            
            encryptedSeed_ = std::move(seed);
        } else {
            encryptedSeed_ = EncryptedSeed{};
        }
        
        // === Read encrypted identity (if present) ===
        if (hasIdentity) {
            EncryptedIdentity identity;
            
            // Salt (32 bytes)
            readBytes(identity.salt.data(), SALT_SIZE);
            
            // Nonce (12 bytes)
            readBytes(identity.nonce.data(), AES_NONCE_SIZE);
            
            // Ciphertext
            identity.ciphertext = readVector();
            
            // Commitment (32 bytes)
            readBytes(identity.commitment.data(), 32);
            
            // Creation timestamp (8 bytes)
            identity.created = readInt64();
            
            encryptedIdentity_ = std::move(identity);
        } else {
            encryptedIdentity_ = EncryptedIdentity{};
        }
        
        // === Read HD key indices (version 2+) ===
        hdKeyIndices_.clear();
        if (version >= 2 && offset < data.size()) {
            uint32_t indexCount = readUint32();
            
            // Sanity check
            if (indexCount > 10000) {
                return false;
            }
            
            for (uint32_t i = 0; i < indexCount; ++i) {
                uint32_t account = readUint32();
                uint32_t change = readUint32();
                uint32_t index = readUint32();
                hdKeyIndices_[{account, change}] = index;
            }
        }
        
        // === Read verification token (if present, flag 0x08) ===
        bool hasVerificationToken = (flags & 0x08) != 0;
        verificationToken_.clear();
        verificationNonce_ = {};
        if (hasVerificationToken && offset < data.size()) {
            // Nonce (12 bytes)
            readBytes(verificationNonce_.data(), AES_NONCE_SIZE);
            
            // Token
            verificationToken_ = readVector();
        }
        
        // Reset unlocked state
        unlocked_ = false;
        masterKey_.reset();
        unlockedKeys_.clear();
        hdKeyManager_.reset();
        unlockedIdentity_.reset();
        
        // For unencrypted wallets with plaintext seed, mark as needing deferred HD init
        // The actual HDKeyManager creation is deferred to SetTestnet() call
        // since testnet_ isn't set correctly until then
        if (!encrypted_ && encryptedSeed_.IsValid()) {
            // Check if nonce is all zeros (indicates plaintext seed storage)
            bool isPlaintext = std::all_of(encryptedSeed_.nonce.begin(), 
                                           encryptedSeed_.nonce.end(),
                                           [](Byte b) { return b == 0; });
            
            if (isPlaintext && encryptedSeed_.ciphertext.size() == BIP39_SEED_SIZE) {
                // Store in unencryptedSeed_ for later initialization
                std::array<Byte, BIP39_SEED_SIZE> seed;
                std::memcpy(seed.data(), encryptedSeed_.ciphertext.data(), BIP39_SEED_SIZE);
                unencryptedSeed_ = seed;
                needsDeferredHDInit_ = true;
            }
        }
        
        return true;
        
    } catch (const std::exception&) {
        // Buffer underflow or other parsing error
        return false;
    }
}

// ============================================================================
// InteractiveKeyStore Implementation
// ============================================================================

InteractiveKeyStore::InteractiveKeyStore() = default;

bool InteractiveKeyStore::UnlockInteractive() {
    if (!passwordCallback_) {
        return false;
    }
    
    std::string password = passwordCallback_();
    return Unlock(password);
}

bool InteractiveKeyStore::AddKeyNotify(const PrivateKey& key, const std::string& label) {
    if (!AddKey(key, label)) {
        return false;
    }
    
    if (keyAddedCallback_) {
        keyAddedCallback_(key.GetPublicKey().GetHash160(), KeyType::Single);
    }
    
    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

PasswordStrength CheckPasswordStrength(const std::string& password) {
    PasswordStrength result{};
    
    result.hasMinLength = password.size() >= 8;
    result.hasUppercase = std::any_of(password.begin(), password.end(), ::isupper);
    result.hasLowercase = std::any_of(password.begin(), password.end(), ::islower);
    result.hasDigit = std::any_of(password.begin(), password.end(), ::isdigit);
    result.hasSpecial = std::any_of(password.begin(), password.end(), 
                                     [](char c) { return !std::isalnum(c); });
    
    result.score = 0;
    if (result.hasMinLength) ++result.score;
    if (result.hasUppercase) ++result.score;
    if (result.hasLowercase) ++result.score;
    if (result.hasDigit) ++result.score;
    if (result.hasSpecial) ++result.score;
    
    return result;
}

std::string PasswordStrength::GetFeedback() const {
    if (score == 5) {
        return "Strong password";
    }
    
    std::string feedback = "Password could be stronger: ";
    if (!hasMinLength) feedback += "use at least 8 characters; ";
    if (!hasUppercase) feedback += "add uppercase letters; ";
    if (!hasLowercase) feedback += "add lowercase letters; ";
    if (!hasDigit) feedback += "add digits; ";
    if (!hasSpecial) feedback += "add special characters; ";
    
    return feedback;
}

std::string GenerateRandomPassword(size_t length) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*()_+-=[]{}|;:,.<>?";
    
    std::string password;
    password.reserve(length);
    
    std::vector<Byte> randomBytes(length);
    GetRandBytes(randomBytes.data(), length);
    
    for (size_t i = 0; i < length; ++i) {
        password += charset[randomBytes[i] % (sizeof(charset) - 1)];
    }
    
    return password;
}

bool SecureCompare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    return SecureCompare(reinterpret_cast<const Byte*>(a.data()),
                         reinterpret_cast<const Byte*>(b.data()),
                         a.size());
}

bool SecureCompare(const Byte* a, const Byte* b, size_t len) {
    Byte diff = 0;
    for (size_t i = 0; i < len; ++i) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

} // namespace wallet
} // namespace shurium
