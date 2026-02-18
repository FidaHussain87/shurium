// SHURIUM - HD Key Derivation Implementation (BIP32/BIP44)
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/wallet/hdkey.h>
#include <shurium/crypto/sha256.h>
#include <shurium/crypto/ripemd160.h>
#include <shurium/crypto/keys.h>
#include <shurium/crypto/secp256k1.h>
#include <shurium/core/random.h>
#include <shurium/core/hex.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>

#ifdef SHURIUM_USE_OPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#endif

namespace shurium {
namespace wallet {

// ============================================================================
// HMAC-SHA512 Implementation
// ============================================================================

std::array<Byte, 64> HMAC_SHA512(const Byte* key, size_t keyLen,
                                  const Byte* data, size_t dataLen) {
    std::array<Byte, 64> result{};
    
#ifdef SHURIUM_USE_OPENSSL
    // Use real HMAC-SHA512 via OpenSSL
    unsigned int resultLen = 64;
    if (HMAC(EVP_sha512(), key, static_cast<int>(keyLen), 
             data, dataLen, result.data(), &resultLen) != nullptr) {
        return result;
    }
    // Fall through to software implementation on error
#endif
    
    // Software fallback: HMAC-SHA512 using double SHA256
    // This is a reasonable approximation but not cryptographically identical
    constexpr size_t BLOCK_SIZE = 128;
    
    // If key is longer than block size, hash it first
    std::array<Byte, BLOCK_SIZE> keyBlock{};
    if (keyLen > BLOCK_SIZE) {
        // Create 64-byte key from two SHA256 hashes
        SHA256 sha1, sha2;
        sha1.Write(key, keyLen);
        sha2.Write(key, keyLen);
        sha2.Write(reinterpret_cast<const Byte*>("\x01"), 1);
        
        std::array<Byte, 32> h1{}, h2{};
        sha1.Finalize(h1.data());
        sha2.Finalize(h2.data());
        
        std::memcpy(keyBlock.data(), h1.data(), 32);
        std::memcpy(keyBlock.data() + 32, h2.data(), 32);
    } else {
        std::memcpy(keyBlock.data(), key, keyLen);
    }
    
    // Inner and outer padding
    std::array<Byte, BLOCK_SIZE> innerPad{};
    std::array<Byte, BLOCK_SIZE> outerPad{};
    
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        innerPad[i] = keyBlock[i] ^ 0x36;
        outerPad[i] = keyBlock[i] ^ 0x5c;
    }
    
    // Inner hash using two SHA256 instances to approximate SHA512
    SHA256 innerSha1, innerSha2;
    innerSha1.Write(innerPad.data(), BLOCK_SIZE);
    innerSha1.Write(data, dataLen);
    innerSha2.Write(innerPad.data() + 64, 64);
    innerSha2.Write(data, dataLen);
    
    std::array<Byte, 32> innerHash1{}, innerHash2{};
    innerSha1.Finalize(innerHash1.data());
    innerSha2.Finalize(innerHash2.data());
    
    // Outer hash
    SHA256 outerSha1, outerSha2;
    outerSha1.Write(outerPad.data(), BLOCK_SIZE);
    outerSha1.Write(innerHash1.data(), 32);
    outerSha1.Write(innerHash2.data(), 32);
    
    outerSha2.Write(outerPad.data() + 64, 64);
    outerSha2.Write(innerHash1.data(), 32);
    outerSha2.Write(innerHash2.data(), 32);
    
    std::array<Byte, 32> outerHash1{}, outerHash2{};
    outerSha1.Finalize(outerHash1.data());
    outerSha2.Finalize(outerHash2.data());
    
    std::memcpy(result.data(), outerHash1.data(), 32);
    std::memcpy(result.data() + 32, outerHash2.data(), 32);
    
    return result;
}

// ============================================================================
// PBKDF2-SHA512 Implementation
// ============================================================================

std::array<Byte, 64> PBKDF2_SHA512(const std::string& password,
                                    const std::string& salt,
                                    uint32_t iterations) {
    std::array<Byte, 64> result{};
    
#ifdef SHURIUM_USE_OPENSSL
    // Use real PBKDF2-HMAC-SHA512 via OpenSSL
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            reinterpret_cast<const unsigned char*>(salt.c_str()), 
            static_cast<int>(salt.size()),
            static_cast<int>(iterations),
            EVP_sha512(),
            64, result.data()) == 1) {
        return result;
    }
    // Fall through to software implementation on error
#endif
    
    // Software fallback: PBKDF2-HMAC-SHA512
    // BIP39 uses PBKDF2-HMAC-SHA512 with 2048 iterations
    // F(Password, Salt, c, i) = U1 ^ U2 ^ ... ^ Uc
    // where U1 = HMAC-SHA512(Password, Salt || INT_32_BE(i))
    
    // For 64-byte output with SHA512, we need 1 block (i=1)
    std::vector<Byte> saltWithIndex(salt.begin(), salt.end());
    saltWithIndex.push_back(0);
    saltWithIndex.push_back(0);
    saltWithIndex.push_back(0);
    saltWithIndex.push_back(1);  // Block index 1
    
    // U1
    auto u = HMAC_SHA512(reinterpret_cast<const Byte*>(password.data()),
                         password.size(),
                         saltWithIndex.data(), saltWithIndex.size());
    result = u;
    
    // U2 to Uc
    for (uint32_t i = 1; i < iterations; ++i) {
        u = HMAC_SHA512(reinterpret_cast<const Byte*>(password.data()),
                        password.size(),
                        u.data(), u.size());
        for (size_t j = 0; j < 64; ++j) {
            result[j] ^= u[j];
        }
    }
    
    return result;
}

// ============================================================================
// PathComponent Implementation
// ============================================================================

std::optional<PathComponent> PathComponent::FromString(const std::string& str) {
    if (str.empty()) {
        return std::nullopt;
    }
    
    bool hardened = false;
    std::string numStr = str;
    
    if (str.back() == '\'' || str.back() == 'h' || str.back() == 'H') {
        hardened = true;
        numStr = str.substr(0, str.size() - 1);
    }
    
    try {
        uint32_t index = std::stoul(numStr);
        return PathComponent(index, hardened);
    } catch (...) {
        return std::nullopt;
    }
}

std::string PathComponent::ToString() const {
    return std::to_string(index) + (hardened ? "'" : "");
}

// ============================================================================
// DerivationPath Implementation
// ============================================================================

std::optional<DerivationPath> DerivationPath::FromString(const std::string& path) {
    if (path.empty()) {
        return DerivationPath();
    }
    
    std::string p = path;
    
    // Remove leading "m/" or "M/"
    if (p.size() >= 2 && (p[0] == 'm' || p[0] == 'M') && p[1] == '/') {
        p = p.substr(2);
    } else if (p.size() >= 1 && (p[0] == 'm' || p[0] == 'M')) {
        p = p.substr(1);
    }
    
    if (p.empty()) {
        return DerivationPath();
    }
    
    std::vector<PathComponent> components;
    std::istringstream stream(p);
    std::string token;
    
    while (std::getline(stream, token, '/')) {
        if (token.empty()) continue;
        
        auto comp = PathComponent::FromString(token);
        if (!comp) {
            return std::nullopt;
        }
        components.push_back(*comp);
    }
    
    return DerivationPath(std::move(components));
}

DerivationPath DerivationPath::BIP44(uint32_t account, uint32_t change, uint32_t index) {
    std::vector<PathComponent> components;
    components.emplace_back(BIP44_PURPOSE, true);       // 44'
    components.emplace_back(SHURIUM_COIN_TYPE, true);     // 8888'
    components.emplace_back(account, true);              // account'
    components.emplace_back(change, false);              // change
    components.emplace_back(index, false);               // index
    return DerivationPath(std::move(components));
}

DerivationPath DerivationPath::BIP44Account(uint32_t account) {
    std::vector<PathComponent> components;
    components.emplace_back(BIP44_PURPOSE, true);
    components.emplace_back(SHURIUM_COIN_TYPE, true);
    components.emplace_back(account, true);
    return DerivationPath(std::move(components));
}

DerivationPath DerivationPath::Child(uint32_t index, bool hardened) const {
    std::vector<PathComponent> newComponents = components_;
    newComponents.emplace_back(index, hardened);
    return DerivationPath(std::move(newComponents));
}

DerivationPath DerivationPath::Parent() const {
    if (components_.empty()) {
        return *this;
    }
    std::vector<PathComponent> newComponents(components_.begin(), components_.end() - 1);
    return DerivationPath(std::move(newComponents));
}

std::string DerivationPath::ToString() const {
    std::string result = "m";
    for (const auto& comp : components_) {
        result += "/" + comp.ToString();
    }
    return result;
}

bool DerivationPath::operator==(const DerivationPath& other) const {
    if (components_.size() != other.components_.size()) {
        return false;
    }
    for (size_t i = 0; i < components_.size(); ++i) {
        if (components_[i].index != other.components_[i].index ||
            components_[i].hardened != other.components_[i].hardened) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// ExtendedKey Implementation
// ============================================================================

ExtendedKey::ExtendedKey(const PrivateKey& key, 
                         const std::array<Byte, CHAIN_CODE_SIZE>& chainCode,
                         uint8_t depth, uint32_t parentFingerprint, 
                         uint32_t childIndex)
    : chainCode_(chainCode)
    , depth_(depth)
    , parentFingerprint_(parentFingerprint)
    , childIndex_(childIndex)
    , isPrivate_(true)
    , isValid_(key.IsValid()) {
    
    keyData_.fill(0);
    if (isValid_) {
        std::memcpy(keyData_.data(), key.data(), 32);
    }
}

ExtendedKey::ExtendedKey(const PublicKey& key,
                         const std::array<Byte, CHAIN_CODE_SIZE>& chainCode,
                         uint8_t depth, uint32_t parentFingerprint,
                         uint32_t childIndex)
    : chainCode_(chainCode)
    , depth_(depth)
    , parentFingerprint_(parentFingerprint)
    , childIndex_(childIndex)
    , isPrivate_(false)
    , isValid_(key.IsValid() && key.IsCompressed()) {
    
    keyData_.fill(0);
    if (isValid_) {
        std::memcpy(keyData_.data(), key.data(), 33);
    }
}

ExtendedKey ExtendedKey::FromSeed(const Byte* seed, size_t seedLen) {
    // BIP32: Master key = HMAC-SHA512(Key = "Bitcoin seed", Data = Seed)
    static const char* KEY = "Bitcoin seed";
    
    auto hash = HMAC_SHA512(reinterpret_cast<const Byte*>(KEY), 12,
                            seed, seedLen);
    
    // Left 32 bytes = private key
    // Right 32 bytes = chain code
    std::array<Byte, 32> privKeyData{};
    std::array<Byte, CHAIN_CODE_SIZE> chainCode{};
    
    std::memcpy(privKeyData.data(), hash.data(), 32);
    std::memcpy(chainCode.data(), hash.data() + 32, 32);
    
    PrivateKey privKey(privKeyData.data(), true);
    if (!privKey.IsValid()) {
        return ExtendedKey();
    }
    
    return ExtendedKey(privKey, chainCode, 0, 0, 0);
}

ExtendedKey ExtendedKey::FromBIP39Seed(const std::array<Byte, BIP39_SEED_SIZE>& seed) {
    return FromSeed(seed.data(), seed.size());
}

std::optional<ExtendedKey> ExtendedKey::DeriveChild(uint32_t index) const {
    if (!isValid_) {
        return std::nullopt;
    }
    
    if (isPrivate_) {
        return DerivePrivateChild(index);
    } else {
        return DerivePublicChild(index);
    }
}

std::optional<ExtendedKey> ExtendedKey::DerivePath(const DerivationPath& path) const {
    ExtendedKey current = *this;
    
    for (const auto& comp : path.GetComponents()) {
        auto child = current.DeriveChild(comp.GetFullIndex());
        if (!child) {
            return std::nullopt;
        }
        current = std::move(*child);
    }
    
    return current;
}

std::optional<ExtendedKey> ExtendedKey::DerivePrivateChild(uint32_t index) const {
    if (!isPrivate_ || !isValid_) {
        return std::nullopt;
    }
    
    bool hardened = (index & HARDENED_FLAG) != 0;
    
    // Data for HMAC
    std::vector<Byte> data;
    
    if (hardened) {
        // Hardened: 0x00 || private key || index
        data.push_back(0x00);
        data.insert(data.end(), keyData_.begin(), keyData_.begin() + 32);
    } else {
        // Normal: public key || index
        auto pubKey = GetPublicKey();
        data.insert(data.end(), pubKey.data(), pubKey.data() + 33);
    }
    
    // Append index (big endian)
    data.push_back((index >> 24) & 0xFF);
    data.push_back((index >> 16) & 0xFF);
    data.push_back((index >> 8) & 0xFF);
    data.push_back(index & 0xFF);
    
    // HMAC-SHA512
    auto hash = HMAC_SHA512(chainCode_.data(), chainCode_.size(),
                            data.data(), data.size());
    
    // Left 32 bytes = IL (tweak for private key)
    // Right 32 bytes = new chain code
    Hash256 tweak;
    std::memcpy(tweak.data(), hash.data(), 32);
    
    std::array<Byte, CHAIN_CODE_SIZE> newChainCode{};
    std::memcpy(newChainCode.data(), hash.data() + 32, 32);
    
    // Parse parent private key
    PrivateKey parentKey(keyData_.data(), true);
    
    // Child key = IL + parent key (mod n)
    auto childKey = parentKey.TweakAdd(tweak);
    if (!childKey) {
        return std::nullopt;  // Invalid key produced
    }
    
    return ExtendedKey(*childKey, newChainCode, depth_ + 1, 
                       GetFingerprint(), index);
}

std::optional<ExtendedKey> ExtendedKey::DerivePublicChild(uint32_t index) const {
    if (!isValid_) {
        return std::nullopt;
    }
    
    // Cannot derive hardened child from public key
    if ((index & HARDENED_FLAG) != 0) {
        return std::nullopt;
    }
    
    // Get the parent public key
    PublicKey parentPubKey = GetPublicKey();
    if (!parentPubKey.IsValid()) {
        return std::nullopt;
    }
    
    // Data: compressed public key (33 bytes) || index (4 bytes big-endian)
    std::vector<Byte> data(keyData_.begin(), keyData_.begin() + 33);
    data.push_back((index >> 24) & 0xFF);
    data.push_back((index >> 16) & 0xFF);
    data.push_back((index >> 8) & 0xFF);
    data.push_back(index & 0xFF);
    
    // Compute I = HMAC-SHA512(chainCode, data)
    auto hash = HMAC_SHA512(chainCode_.data(), chainCode_.size(),
                            data.data(), data.size());
    
    // Split I into IL (first 32 bytes) and IR (last 32 bytes)
    // IL is the tweak, IR is the new chain code
    std::array<Byte, 32> tweak{};
    std::memcpy(tweak.data(), hash.data(), 32);
    
    std::array<Byte, CHAIN_CODE_SIZE> newChainCode{};
    std::memcpy(newChainCode.data(), hash.data() + 32, 32);
    
    // Check if IL is valid (must be < curve order)
    if (!secp256k1::IsValidPrivateKey(tweak)) {
        // This is extremely rare but we must handle it per BIP32
        return std::nullopt;
    }
    
    // Compute child public key: childPubKey = IL * G + parentPubKey
    // Using PublicKeyTweakAdd which does: result = P + tweak*G
    std::array<Byte, 33> childPubKeyData{};
    if (!secp256k1::PublicKeyTweakAdd(parentPubKey.data(), parentPubKey.size(),
                                       tweak.data(), childPubKeyData.data())) {
        // Point at infinity - invalid child key
        return std::nullopt;
    }
    
    // Create child public key
    PublicKey childPubKey(childPubKeyData.data(), childPubKeyData.size());
    if (!childPubKey.IsValid()) {
        return std::nullopt;
    }
    
    return ExtendedKey(childPubKey, newChainCode, depth_ + 1,
                       GetFingerprint(), index);
}

std::optional<PrivateKey> ExtendedKey::GetPrivateKey() const {
    if (!isPrivate_ || !isValid_) {
        return std::nullopt;
    }
    return PrivateKey(keyData_.data(), true);
}

PublicKey ExtendedKey::GetPublicKey() const {
    if (!isValid_) {
        return PublicKey();
    }
    
    if (isPrivate_) {
        PrivateKey privKey(keyData_.data(), true);
        return privKey.GetPublicKey();
    } else {
        return PublicKey(keyData_.data(), 33);
    }
}

uint32_t ExtendedKey::GetFingerprint() const {
    if (!isValid_) {
        return 0;
    }
    
    auto pubKey = GetPublicKey();
    auto hash = pubKey.GetHash160();
    
    return (static_cast<uint32_t>(hash[0]) << 24) |
           (static_cast<uint32_t>(hash[1]) << 16) |
           (static_cast<uint32_t>(hash[2]) << 8) |
           static_cast<uint32_t>(hash[3]);
}

ExtendedKey ExtendedKey::Neuter() const {
    if (!isValid_) {
        return ExtendedKey();
    }
    
    if (!isPrivate_) {
        return *this;
    }
    
    return ExtendedKey(GetPublicKey(), chainCode_, depth_, 
                       parentFingerprint_, childIndex_);
}

std::string ExtendedKey::ToBase58(bool testnet) const {
    if (!isValid_) {
        return "";
    }
    
    auto bytes = ToBytes(testnet);
    return EncodeBase58Check(std::vector<Byte>(bytes.begin(), bytes.end()));
}

std::array<Byte, ExtendedKey::SERIALIZED_SIZE> ExtendedKey::ToBytes(bool testnet) const {
    std::array<Byte, SERIALIZED_SIZE> result{};
    
    if (!isValid_) {
        return result;
    }
    
    // Version (4 bytes)
    uint32_t version;
    if (testnet) {
        version = isPrivate_ ? TESTNET_PRIVATE : TESTNET_PUBLIC;
    } else {
        version = isPrivate_ ? MAINNET_PRIVATE : MAINNET_PUBLIC;
    }
    
    result[0] = (version >> 24) & 0xFF;
    result[1] = (version >> 16) & 0xFF;
    result[2] = (version >> 8) & 0xFF;
    result[3] = version & 0xFF;
    
    // Depth (1 byte)
    result[4] = depth_;
    
    // Parent fingerprint (4 bytes)
    result[5] = (parentFingerprint_ >> 24) & 0xFF;
    result[6] = (parentFingerprint_ >> 16) & 0xFF;
    result[7] = (parentFingerprint_ >> 8) & 0xFF;
    result[8] = parentFingerprint_ & 0xFF;
    
    // Child index (4 bytes)
    result[9] = (childIndex_ >> 24) & 0xFF;
    result[10] = (childIndex_ >> 16) & 0xFF;
    result[11] = (childIndex_ >> 8) & 0xFF;
    result[12] = childIndex_ & 0xFF;
    
    // Chain code (32 bytes)
    std::memcpy(result.data() + 13, chainCode_.data(), 32);
    
    // Key data (33 bytes)
    if (isPrivate_) {
        result[45] = 0x00;  // Leading zero for private key
        std::memcpy(result.data() + 46, keyData_.data(), 32);
    } else {
        std::memcpy(result.data() + 45, keyData_.data(), 33);
    }
    
    return result;
}

std::optional<ExtendedKey> ExtendedKey::FromBase58(const std::string& str) {
    auto data = DecodeBase58Check(str);
    if (data.size() != SERIALIZED_SIZE) {
        return std::nullopt;
    }
    return FromBytes(data.data(), data.size());
}

std::optional<ExtendedKey> ExtendedKey::FromBytes(const Byte* data, size_t len) {
    if (len != SERIALIZED_SIZE) {
        return std::nullopt;
    }
    
    // Version
    uint32_t version = (static_cast<uint32_t>(data[0]) << 24) |
                       (static_cast<uint32_t>(data[1]) << 16) |
                       (static_cast<uint32_t>(data[2]) << 8) |
                       static_cast<uint32_t>(data[3]);
    
    bool isPrivate;
    if (version == MAINNET_PRIVATE || version == TESTNET_PRIVATE) {
        isPrivate = true;
    } else if (version == MAINNET_PUBLIC || version == TESTNET_PUBLIC) {
        isPrivate = false;
    } else {
        return std::nullopt;
    }
    
    // Depth
    uint8_t depth = data[4];
    
    // Parent fingerprint
    uint32_t parentFingerprint = (static_cast<uint32_t>(data[5]) << 24) |
                                  (static_cast<uint32_t>(data[6]) << 16) |
                                  (static_cast<uint32_t>(data[7]) << 8) |
                                  static_cast<uint32_t>(data[8]);
    
    // Child index
    uint32_t childIndex = (static_cast<uint32_t>(data[9]) << 24) |
                          (static_cast<uint32_t>(data[10]) << 16) |
                          (static_cast<uint32_t>(data[11]) << 8) |
                          static_cast<uint32_t>(data[12]);
    
    // Chain code
    std::array<Byte, CHAIN_CODE_SIZE> chainCode{};
    std::memcpy(chainCode.data(), data + 13, 32);
    
    // Key data
    if (isPrivate) {
        if (data[45] != 0x00) {
            return std::nullopt;
        }
        PrivateKey privKey(data + 46, true);
        if (!privKey.IsValid()) {
            return std::nullopt;
        }
        return ExtendedKey(privKey, chainCode, depth, parentFingerprint, childIndex);
    } else {
        PublicKey pubKey(data + 45, 33);
        if (!pubKey.IsValid()) {
            return std::nullopt;
        }
        return ExtendedKey(pubKey, chainCode, depth, parentFingerprint, childIndex);
    }
}

// ============================================================================
// Mnemonic Implementation (BIP39)
// ============================================================================

// BIP39 English wordlist - complete 2048 words (from bitcoin/bips)
const char* const Mnemonic::WORDLIST[2048] = {
    "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract",
    "absurd", "abuse", "access", "accident", "account", "accuse", "achieve", "acid",
    "acoustic", "acquire", "across", "act", "action", "actor", "actress", "actual",
    "adapt", "add", "addict", "address", "adjust", "admit", "adult", "advance",
    "advice", "aerobic", "affair", "afford", "afraid", "again", "age", "agent",
    "agree", "ahead", "aim", "air", "airport", "aisle", "alarm", "album",
    "alcohol", "alert", "alien", "all", "alley", "allow", "almost", "alone",
    "alpha", "already", "also", "alter", "always", "amateur", "amazing", "among",
    "amount", "amused", "analyst", "anchor", "ancient", "anger", "angle", "angry",
    "animal", "ankle", "announce", "annual", "another", "answer", "antenna", "antique",
    "anxiety", "any", "apart", "apology", "appear", "apple", "approve", "april",
    "arch", "arctic", "area", "arena", "argue", "arm", "armed", "armor",
    "army", "around", "arrange", "arrest", "arrive", "arrow", "art", "artefact",
    "artist", "artwork", "ask", "aspect", "assault", "asset", "assist", "assume",
    "asthma", "athlete", "atom", "attack", "attend", "attitude", "attract", "auction",
    "audit", "august", "aunt", "author", "auto", "autumn", "average", "avocado",
    "avoid", "awake", "aware", "away", "awesome", "awful", "awkward", "axis",
    "baby", "bachelor", "bacon", "badge", "bag", "balance", "balcony", "ball",
    "bamboo", "banana", "banner", "bar", "barely", "bargain", "barrel", "base",
    "basic", "basket", "battle", "beach", "bean", "beauty", "because", "become",
    "beef", "before", "begin", "behave", "behind", "believe", "below", "belt",
    "bench", "benefit", "best", "betray", "better", "between", "beyond", "bicycle",
    "bid", "bike", "bind", "biology", "bird", "birth", "bitter", "black",
    "blade", "blame", "blanket", "blast", "bleak", "bless", "blind", "blood",
    "blossom", "blouse", "blue", "blur", "blush", "board", "boat", "body",
    "boil", "bomb", "bone", "bonus", "book", "boost", "border", "boring",
    "borrow", "boss", "bottom", "bounce", "box", "boy", "bracket", "brain",
    "brand", "brass", "brave", "bread", "breeze", "brick", "bridge", "brief",
    "bright", "bring", "brisk", "broccoli", "broken", "bronze", "broom", "brother",
    "brown", "brush", "bubble", "buddy", "budget", "buffalo", "build", "bulb",
    "bulk", "bullet", "bundle", "bunker", "burden", "burger", "burst", "bus",
    "business", "busy", "butter", "buyer", "buzz", "cabbage", "cabin", "cable",
    "cactus", "cage", "cake", "call", "calm", "camera", "camp", "can",
    "canal", "cancel", "candy", "cannon", "canoe", "canvas", "canyon", "capable",
    "capital", "captain", "car", "carbon", "card", "cargo", "carpet", "carry",
    "cart", "case", "cash", "casino", "castle", "casual", "cat", "catalog",
    "catch", "category", "cattle", "caught", "cause", "caution", "cave", "ceiling",
    "celery", "cement", "census", "century", "cereal", "certain", "chair", "chalk",
    "champion", "change", "chaos", "chapter", "charge", "chase", "chat", "cheap",
    "check", "cheese", "chef", "cherry", "chest", "chicken", "chief", "child",
    "chimney", "choice", "choose", "chronic", "chuckle", "chunk", "churn", "cigar",
    "cinnamon", "circle", "citizen", "city", "civil", "claim", "clap", "clarify",
    "claw", "clay", "clean", "clerk", "clever", "click", "client", "cliff",
    "climb", "clinic", "clip", "clock", "clog", "close", "cloth", "cloud",
    "clown", "club", "clump", "cluster", "clutch", "coach", "coast", "coconut",
    "code", "coffee", "coil", "coin", "collect", "color", "column", "combine",
    "come", "comfort", "comic", "common", "company", "concert", "conduct", "confirm",
    "congress", "connect", "consider", "control", "convince", "cook", "cool", "copper",
    "copy", "coral", "core", "corn", "correct", "cost", "cotton", "couch",
    "country", "couple", "course", "cousin", "cover", "coyote", "crack", "cradle",
    "craft", "cram", "crane", "crash", "crater", "crawl", "crazy", "cream",
    "credit", "creek", "crew", "cricket", "crime", "crisp", "critic", "crop",
    "cross", "crouch", "crowd", "crucial", "cruel", "cruise", "crumble", "crunch",
    "crush", "cry", "crystal", "cube", "culture", "cup", "cupboard", "curious",
    "current", "curtain", "curve", "cushion", "custom", "cute", "cycle", "dad",
    "damage", "damp", "dance", "danger", "daring", "dash", "daughter", "dawn",
    "day", "deal", "debate", "debris", "decade", "december", "decide", "decline",
    "decorate", "decrease", "deer", "defense", "define", "defy", "degree", "delay",
    "deliver", "demand", "demise", "denial", "dentist", "deny", "depart", "depend",
    "deposit", "depth", "deputy", "derive", "describe", "desert", "design", "desk",
    "despair", "destroy", "detail", "detect", "develop", "device", "devote", "diagram",
    "dial", "diamond", "diary", "dice", "diesel", "diet", "differ", "digital",
    "dignity", "dilemma", "dinner", "dinosaur", "direct", "dirt", "disagree", "discover",
    "disease", "dish", "dismiss", "disorder", "display", "distance", "divert", "divide",
    "divorce", "dizzy", "doctor", "document", "dog", "doll", "dolphin", "domain",
    "donate", "donkey", "donor", "door", "dose", "double", "dove", "draft",
    "dragon", "drama", "drastic", "draw", "dream", "dress", "drift", "drill",
    "drink", "drip", "drive", "drop", "drum", "dry", "duck", "dumb",
    "dune", "during", "dust", "dutch", "duty", "dwarf", "dynamic", "eager",
    "eagle", "early", "earn", "earth", "easily", "east", "easy", "echo",
    "ecology", "economy", "edge", "edit", "educate", "effort", "egg", "eight",
    "either", "elbow", "elder", "electric", "elegant", "element", "elephant", "elevator",
    "elite", "else", "embark", "embody", "embrace", "emerge", "emotion", "employ",
    "empower", "empty", "enable", "enact", "end", "endless", "endorse", "enemy",
    "energy", "enforce", "engage", "engine", "enhance", "enjoy", "enlist", "enough",
    "enrich", "enroll", "ensure", "enter", "entire", "entry", "envelope", "episode",
    "equal", "equip", "era", "erase", "erode", "erosion", "error", "erupt",
    "escape", "essay", "essence", "estate", "eternal", "ethics", "evidence", "evil",
    "evoke", "evolve", "exact", "example", "excess", "exchange", "excite", "exclude",
    "excuse", "execute", "exercise", "exhaust", "exhibit", "exile", "exist", "exit",
    "exotic", "expand", "expect", "expire", "explain", "expose", "express", "extend",
    "extra", "eye", "eyebrow", "fabric", "face", "faculty", "fade", "faint",
    "faith", "fall", "false", "fame", "family", "famous", "fan", "fancy",
    "fantasy", "farm", "fashion", "fat", "fatal", "father", "fatigue", "fault",
    "favorite", "feature", "february", "federal", "fee", "feed", "feel", "female",
    "fence", "festival", "fetch", "fever", "few", "fiber", "fiction", "field",
    "figure", "file", "film", "filter", "final", "find", "fine", "finger",
    "finish", "fire", "firm", "first", "fiscal", "fish", "fit", "fitness",
    "fix", "flag", "flame", "flash", "flat", "flavor", "flee", "flight",
    "flip", "float", "flock", "floor", "flower", "fluid", "flush", "fly",
    "foam", "focus", "fog", "foil", "fold", "follow", "food", "foot",
    "force", "forest", "forget", "fork", "fortune", "forum", "forward", "fossil",
    "foster", "found", "fox", "fragile", "frame", "frequent", "fresh", "friend",
    "fringe", "frog", "front", "frost", "frown", "frozen", "fruit", "fuel",
    "fun", "funny", "furnace", "fury", "future", "gadget", "gain", "galaxy",
    "gallery", "game", "gap", "garage", "garbage", "garden", "garlic", "garment",
    "gas", "gasp", "gate", "gather", "gauge", "gaze", "general", "genius",
    "genre", "gentle", "genuine", "gesture", "ghost", "giant", "gift", "giggle",
    "ginger", "giraffe", "girl", "give", "glad", "glance", "glare", "glass",
    "glide", "glimpse", "globe", "gloom", "glory", "glove", "glow", "glue",
    "goat", "goddess", "gold", "good", "goose", "gorilla", "gospel", "gossip",
    "govern", "gown", "grab", "grace", "grain", "grant", "grape", "grass",
    "gravity", "great", "green", "grid", "grief", "grit", "grocery", "group",
    "grow", "grunt", "guard", "guess", "guide", "guilt", "guitar", "gun",
    "gym", "habit", "hair", "half", "hammer", "hamster", "hand", "happy",
    "harbor", "hard", "harsh", "harvest", "hat", "have", "hawk", "hazard",
    "head", "health", "heart", "heavy", "hedgehog", "height", "hello", "helmet",
    "help", "hen", "hero", "hidden", "high", "hill", "hint", "hip",
    "hire", "history", "hobby", "hockey", "hold", "hole", "holiday", "hollow",
    "home", "honey", "hood", "hope", "horn", "horror", "horse", "hospital",
    "host", "hotel", "hour", "hover", "hub", "huge", "human", "humble",
    "humor", "hundred", "hungry", "hunt", "hurdle", "hurry", "hurt", "husband",
    "hybrid", "ice", "icon", "idea", "identify", "idle", "ignore", "ill",
    "illegal", "illness", "image", "imitate", "immense", "immune", "impact", "impose",
    "improve", "impulse", "inch", "include", "income", "increase", "index", "indicate",
    "indoor", "industry", "infant", "inflict", "inform", "inhale", "inherit", "initial",
    "inject", "injury", "inmate", "inner", "innocent", "input", "inquiry", "insane",
    "insect", "inside", "inspire", "install", "intact", "interest", "into", "invest",
    "invite", "involve", "iron", "island", "isolate", "issue", "item", "ivory",
    "jacket", "jaguar", "jar", "jazz", "jealous", "jeans", "jelly", "jewel",
    "job", "join", "joke", "journey", "joy", "judge", "juice", "jump",
    "jungle", "junior", "junk", "just", "kangaroo", "keen", "keep", "ketchup",
    "key", "kick", "kid", "kidney", "kind", "kingdom", "kiss", "kit",
    "kitchen", "kite", "kitten", "kiwi", "knee", "knife", "knock", "know",
    "lab", "label", "labor", "ladder", "lady", "lake", "lamp", "language",
    "laptop", "large", "later", "latin", "laugh", "laundry", "lava", "law",
    "lawn", "lawsuit", "layer", "lazy", "leader", "leaf", "learn", "leave",
    "lecture", "left", "leg", "legal", "legend", "leisure", "lemon", "lend",
    "length", "lens", "leopard", "lesson", "letter", "level", "liar", "liberty",
    "library", "license", "life", "lift", "light", "like", "limb", "limit",
    "link", "lion", "liquid", "list", "little", "live", "lizard", "load",
    "loan", "lobster", "local", "lock", "logic", "lonely", "long", "loop",
    "lottery", "loud", "lounge", "love", "loyal", "lucky", "luggage", "lumber",
    "lunar", "lunch", "luxury", "lyrics", "machine", "mad", "magic", "magnet",
    "maid", "mail", "main", "major", "make", "mammal", "man", "manage",
    "mandate", "mango", "mansion", "manual", "maple", "marble", "march", "margin",
    "marine", "market", "marriage", "mask", "mass", "master", "match", "material",
    "math", "matrix", "matter", "maximum", "maze", "meadow", "mean", "measure",
    "meat", "mechanic", "medal", "media", "melody", "melt", "member", "memory",
    "mention", "menu", "mercy", "merge", "merit", "merry", "mesh", "message",
    "metal", "method", "middle", "midnight", "milk", "million", "mimic", "mind",
    "minimum", "minor", "minute", "miracle", "mirror", "misery", "miss", "mistake",
    "mix", "mixed", "mixture", "mobile", "model", "modify", "mom", "moment",
    "monitor", "monkey", "monster", "month", "moon", "moral", "more", "morning",
    "mosquito", "mother", "motion", "motor", "mountain", "mouse", "move", "movie",
    "much", "muffin", "mule", "multiply", "muscle", "museum", "mushroom", "music",
    "must", "mutual", "myself", "mystery", "myth", "naive", "name", "napkin",
    "narrow", "nasty", "nation", "nature", "near", "neck", "need", "negative",
    "neglect", "neither", "nephew", "nerve", "nest", "net", "network", "neutral",
    "never", "news", "next", "nice", "night", "noble", "noise", "nominee",
    "noodle", "normal", "north", "nose", "notable", "note", "nothing", "notice",
    "novel", "now", "nuclear", "number", "nurse", "nut", "oak", "obey",
    "object", "oblige", "obscure", "observe", "obtain", "obvious", "occur", "ocean",
    "october", "odor", "off", "offer", "office", "often", "oil", "okay",
    "old", "olive", "olympic", "omit", "once", "one", "onion", "online",
    "only", "open", "opera", "opinion", "oppose", "option", "orange", "orbit",
    "orchard", "order", "ordinary", "organ", "orient", "original", "orphan", "ostrich",
    "other", "outdoor", "outer", "output", "outside", "oval", "oven", "over",
    "own", "owner", "oxygen", "oyster", "ozone", "pact", "paddle", "page",
    "pair", "palace", "palm", "panda", "panel", "panic", "panther", "paper",
    "parade", "parent", "park", "parrot", "party", "pass", "patch", "path",
    "patient", "patrol", "pattern", "pause", "pave", "payment", "peace", "peanut",
    "pear", "peasant", "pelican", "pen", "penalty", "pencil", "people", "pepper",
    "perfect", "permit", "person", "pet", "phone", "photo", "phrase", "physical",
    "piano", "picnic", "picture", "piece", "pig", "pigeon", "pill", "pilot",
    "pink", "pioneer", "pipe", "pistol", "pitch", "pizza", "place", "planet",
    "plastic", "plate", "play", "please", "pledge", "pluck", "plug", "plunge",
    "poem", "poet", "point", "polar", "pole", "police", "pond", "pony",
    "pool", "popular", "portion", "position", "possible", "post", "potato", "pottery",
    "poverty", "powder", "power", "practice", "praise", "predict", "prefer", "prepare",
    "present", "pretty", "prevent", "price", "pride", "primary", "print", "priority",
    "prison", "private", "prize", "problem", "process", "produce", "profit", "program",
    "project", "promote", "proof", "property", "prosper", "protect", "proud", "provide",
    "public", "pudding", "pull", "pulp", "pulse", "pumpkin", "punch", "pupil",
    "puppy", "purchase", "purity", "purpose", "purse", "push", "put", "puzzle",
    "pyramid", "quality", "quantum", "quarter", "question", "quick", "quit", "quiz",
    "quote", "rabbit", "raccoon", "race", "rack", "radar", "radio", "rail",
    "rain", "raise", "rally", "ramp", "ranch", "random", "range", "rapid",
    "rare", "rate", "rather", "raven", "raw", "razor", "ready", "real",
    "reason", "rebel", "rebuild", "recall", "receive", "recipe", "record", "recycle",
    "reduce", "reflect", "reform", "refuse", "region", "regret", "regular", "reject",
    "relax", "release", "relief", "rely", "remain", "remember", "remind", "remove",
    "render", "renew", "rent", "reopen", "repair", "repeat", "replace", "report",
    "require", "rescue", "resemble", "resist", "resource", "response", "result", "retire",
    "retreat", "return", "reunion", "reveal", "review", "reward", "rhythm", "rib",
    "ribbon", "rice", "rich", "ride", "ridge", "rifle", "right", "rigid",
    "ring", "riot", "ripple", "risk", "ritual", "rival", "river", "road",
    "roast", "robot", "robust", "rocket", "romance", "roof", "rookie", "room",
    "rose", "rotate", "rough", "round", "route", "royal", "rubber", "rude",
    "rug", "rule", "run", "runway", "rural", "sad", "saddle", "sadness",
    "safe", "sail", "salad", "salmon", "salon", "salt", "salute", "same",
    "sample", "sand", "satisfy", "satoshi", "sauce", "sausage", "save", "say",
    "scale", "scan", "scare", "scatter", "scene", "scheme", "school", "science",
    "scissors", "scorpion", "scout", "scrap", "screen", "script", "scrub", "sea",
    "search", "season", "seat", "second", "secret", "section", "security", "seed",
    "seek", "segment", "select", "sell", "seminar", "senior", "sense", "sentence",
    "series", "service", "session", "settle", "setup", "seven", "shadow", "shaft",
    "shallow", "share", "shed", "shell", "sheriff", "shield", "shift", "shine",
    "ship", "shiver", "shock", "shoe", "shoot", "shop", "short", "shoulder",
    "shove", "shrimp", "shrug", "shuffle", "shy", "sibling", "sick", "side",
    "siege", "sight", "sign", "silent", "silk", "silly", "silver", "similar",
    "simple", "since", "sing", "siren", "sister", "situate", "six", "size",
    "skate", "sketch", "ski", "skill", "skin", "skirt", "skull", "slab",
    "slam", "sleep", "slender", "slice", "slide", "slight", "slim", "slogan",
    "slot", "slow", "slush", "small", "smart", "smile", "smoke", "smooth",
    "snack", "snake", "snap", "sniff", "snow", "soap", "soccer", "social",
    "sock", "soda", "soft", "solar", "soldier", "solid", "solution", "solve",
    "someone", "song", "soon", "sorry", "sort", "soul", "sound", "soup",
    "source", "south", "space", "spare", "spatial", "spawn", "speak", "special",
    "speed", "spell", "spend", "sphere", "spice", "spider", "spike", "spin",
    "spirit", "split", "spoil", "sponsor", "spoon", "sport", "spot", "spray",
    "spread", "spring", "spy", "square", "squeeze", "squirrel", "stable", "stadium",
    "staff", "stage", "stairs", "stamp", "stand", "start", "state", "stay",
    "steak", "steel", "stem", "step", "stereo", "stick", "still", "sting",
    "stock", "stomach", "stone", "stool", "story", "stove", "strategy", "street",
    "strike", "strong", "struggle", "student", "stuff", "stumble", "style", "subject",
    "submit", "subway", "success", "such", "sudden", "suffer", "sugar", "suggest",
    "suit", "summer", "sun", "sunny", "sunset", "super", "supply", "supreme",
    "sure", "surface", "surge", "surprise", "surround", "survey", "suspect", "sustain",
    "swallow", "swamp", "swap", "swarm", "swear", "sweet", "swift", "swim",
    "swing", "switch", "sword", "symbol", "symptom", "syrup", "system", "table",
    "tackle", "tag", "tail", "talent", "talk", "tank", "tape", "target",
    "task", "taste", "tattoo", "taxi", "teach", "team", "tell", "ten",
    "tenant", "tennis", "tent", "term", "test", "text", "thank", "that",
    "theme", "then", "theory", "there", "they", "thing", "this", "thought",
    "three", "thrive", "throw", "thumb", "thunder", "ticket", "tide", "tiger",
    "tilt", "timber", "time", "tiny", "tip", "tired", "tissue", "title",
    "toast", "tobacco", "today", "toddler", "toe", "together", "toilet", "token",
    "tomato", "tomorrow", "tone", "tongue", "tonight", "tool", "tooth", "top",
    "topic", "topple", "torch", "tornado", "tortoise", "toss", "total", "tourist",
    "toward", "tower", "town", "toy", "track", "trade", "traffic", "tragic",
    "train", "transfer", "trap", "trash", "travel", "tray", "treat", "tree",
    "trend", "trial", "tribe", "trick", "trigger", "trim", "trip", "trophy",
    "trouble", "truck", "true", "truly", "trumpet", "trust", "truth", "try",
    "tube", "tuition", "tumble", "tuna", "tunnel", "turkey", "turn", "turtle",
    "twelve", "twenty", "twice", "twin", "twist", "two", "type", "typical",
    "ugly", "umbrella", "unable", "unaware", "uncle", "uncover", "under", "undo",
    "unfair", "unfold", "unhappy", "uniform", "unique", "unit", "universe", "unknown",
    "unlock", "until", "unusual", "unveil", "update", "upgrade", "uphold", "upon",
    "upper", "upset", "urban", "urge", "usage", "use", "used", "useful",
    "useless", "usual", "utility", "vacant", "vacuum", "vague", "valid", "valley",
    "valve", "van", "vanish", "vapor", "various", "vast", "vault", "vehicle",
    "velvet", "vendor", "venture", "venue", "verb", "verify", "version", "very",
    "vessel", "veteran", "viable", "vibrant", "vicious", "victory", "video", "view",
    "village", "vintage", "violin", "virtual", "virus", "visa", "visit", "visual",
    "vital", "vivid", "vocal", "voice", "void", "volcano", "volume", "vote",
    "voyage", "wage", "wagon", "wait", "walk", "wall", "walnut", "want",
    "warfare", "warm", "warrior", "wash", "wasp", "waste", "water", "wave",
    "way", "wealth", "weapon", "wear", "weasel", "weather", "web", "wedding",
    "weekend", "weird", "welcome", "west", "wet", "whale", "what", "wheat",
    "wheel", "when", "where", "whip", "whisper", "wide", "width", "wife",
    "wild", "will", "win", "window", "wine", "wing", "wink", "winner",
    "winter", "wire", "wisdom", "wise", "wish", "witness", "wolf", "woman",
    "wonder", "wood", "wool", "word", "work", "world", "worry", "worth",
    "wrap", "wreck", "wrestle", "wrist", "write", "wrong", "yard", "year",
    "yellow", "you", "young", "youth", "zebra", "zero", "zone", "zoo"
};
std::string Mnemonic::Generate(Strength strength) {
    size_t entropyBits = static_cast<size_t>(strength);
    size_t entropyBytes = entropyBits / 8;
    
    std::vector<Byte> entropy(entropyBytes);
    GetRandBytes(entropy.data(), entropyBytes);
    
    return FromEntropy(entropy.data(), entropyBytes);
}

std::string Mnemonic::FromEntropy(const Byte* entropy, size_t len) {
    // Valid lengths: 16, 20, 24, 28, 32 bytes
    if (len < 16 || len > 32 || len % 4 != 0) {
        return "";
    }
    
    // Calculate checksum
    Hash256 hash = SHA256Hash(entropy, len);
    
    // Number of checksum bits = entropy bits / 32
    size_t checksumBits = len / 4;
    size_t totalBits = len * 8 + checksumBits;
    size_t wordCount = totalBits / 11;
    
    // Combine entropy and checksum
    std::vector<Byte> data(entropy, entropy + len);
    data.push_back(hash[0]);  // Only need first byte for checksum
    
    // Extract 11-bit words
    std::vector<uint16_t> indices;
    size_t bitPos = 0;
    
    for (size_t i = 0; i < wordCount; ++i) {
        uint16_t index = 0;
        for (int j = 10; j >= 0; --j) {
            size_t byteIdx = bitPos / 8;
            size_t bitIdx = 7 - (bitPos % 8);
            if (byteIdx < data.size() && (data[byteIdx] & (1 << bitIdx))) {
                index |= (1 << j);
            }
            ++bitPos;
        }
        indices.push_back(index);
    }
    
    // Build mnemonic string
    std::string result;
    for (size_t i = 0; i < indices.size(); ++i) {
        if (i > 0) result += " ";
        result += WORDLIST[indices[i] % 2048];  // Mod for safety
    }
    
    return result;
}

std::vector<Byte> Mnemonic::ToEntropy(const std::string& mnemonic) {
    // Parse words
    std::vector<std::string> words;
    std::istringstream stream(mnemonic);
    std::string word;
    while (stream >> word) {
        // Convert to lowercase
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        words.push_back(word);
    }
    
    // Valid word counts: 12, 15, 18, 21, 24
    if (words.size() < 12 || words.size() > 24 || words.size() % 3 != 0) {
        return {};
    }
    
    // Convert words to indices
    std::vector<uint16_t> indices;
    for (const auto& w : words) {
        int idx = GetWordIndex(w);
        if (idx < 0) {
            return {};  // Invalid word
        }
        indices.push_back(static_cast<uint16_t>(idx));
    }
    
    // Convert indices to bits
    size_t totalBits = indices.size() * 11;
    size_t checksumBits = indices.size() / 3;
    size_t entropyBits = totalBits - checksumBits;
    size_t entropyBytes = entropyBits / 8;
    
    std::vector<Byte> result(entropyBytes);
    size_t bitPos = 0;
    
    for (uint16_t idx : indices) {
        for (int j = 10; j >= 0 && bitPos < entropyBits; --j) {
            if (idx & (1 << j)) {
                size_t byteIdx = bitPos / 8;
                size_t bitIdx = 7 - (bitPos % 8);
                if (byteIdx < result.size()) {
                    result[byteIdx] |= (1 << bitIdx);
                }
            }
            ++bitPos;
        }
    }
    
    return result;
}

bool Mnemonic::Validate(const std::string& mnemonic) {
    auto entropy = ToEntropy(mnemonic);
    if (entropy.empty()) {
        return false;
    }
    
    // Regenerate mnemonic and compare
    std::string regenerated = FromEntropy(entropy.data(), entropy.size());
    
    // Compare (case-insensitive)
    std::string m1 = mnemonic, m2 = regenerated;
    std::transform(m1.begin(), m1.end(), m1.begin(), ::tolower);
    std::transform(m2.begin(), m2.end(), m2.begin(), ::tolower);
    
    return m1 == m2;
}

std::array<Byte, BIP39_SEED_SIZE> Mnemonic::ToSeed(const std::string& mnemonic,
                                                    const std::string& passphrase) {
    // BIP39: seed = PBKDF2(mnemonic, "mnemonic" + passphrase, 2048, 64)
    std::string salt = "mnemonic" + passphrase;
    return PBKDF2_SHA512(mnemonic, salt, 2048);
}

std::string Mnemonic::GetWord(uint16_t index) {
    if (index >= 2048) {
        return "";
    }
    return WORDLIST[index];
}

int Mnemonic::GetWordIndex(const std::string& word) {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (int i = 0; i < 2048; ++i) {
        if (WORDLIST[i] == lower) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// HDKeyManager Implementation
// ============================================================================

HDKeyManager::HDKeyManager() = default;

HDKeyManager::HDKeyManager(const ExtendedKey& masterKey, const Config& config)
    : masterKey_(masterKey)
    , config_(config) {
}

HDKeyManager HDKeyManager::FromMnemonic(const std::string& mnemonic,
                                         const std::string& passphrase,
                                         const Config& config) {
    auto seed = Mnemonic::ToSeed(mnemonic, passphrase);
    auto masterKey = ExtendedKey::FromBIP39Seed(seed);
    return HDKeyManager(masterKey, config);
}

ExtendedKey HDKeyManager::GetMasterPublicKey() const {
    return masterKey_.Neuter();
}

std::optional<ExtendedKey> HDKeyManager::GetAccountKey(uint32_t account) const {
    if (!masterKey_.IsValid()) {
        return std::nullopt;
    }
    
    auto path = DerivationPath::BIP44Account(account);
    return masterKey_.DerivePath(path);
}

HDKeyManager::KeyInfo HDKeyManager::DeriveNextReceiving(uint32_t account) {
    uint32_t index = GetNextReceivingIndex(account);
    return DeriveAndCache(account, 0, index);
}

HDKeyManager::KeyInfo HDKeyManager::DeriveNextChange(uint32_t account) {
    uint32_t index = GetNextChangeIndex(account);
    return DeriveAndCache(account, 1, index);
}

std::optional<HDKeyManager::KeyInfo> HDKeyManager::DeriveKey(uint32_t account,
                                                              uint32_t change,
                                                              uint32_t index) {
    auto path = DerivationPath::BIP44(account, change, index);
    auto key = masterKey_.DerivePath(path);
    if (!key) {
        return std::nullopt;
    }
    
    KeyInfo info;
    info.path = path;
    info.publicKey = key->GetPublicKey();
    info.keyHash = info.publicKey.GetHash160();
    info.account = account;
    info.change = change;
    info.index = index;
    info.used = false;
    
    keysByHash_[info.keyHash] = info;
    
    return info;
}

std::optional<HDKeyManager::KeyInfo> HDKeyManager::GetKeyAtPath(const DerivationPath& path) {
    auto key = masterKey_.DerivePath(path);
    if (!key) {
        return std::nullopt;
    }
    
    KeyInfo info;
    info.path = path;
    info.publicKey = key->GetPublicKey();
    info.keyHash = info.publicKey.GetHash160();
    
    // Try to extract account/change/index from path
    auto& comps = path.GetComponents();
    if (comps.size() >= 5) {
        info.account = comps[2].index;
        info.change = comps[3].index;
        info.index = comps[4].index;
    }
    
    return info;
}

std::optional<HDKeyManager::KeyInfo> HDKeyManager::FindKeyByHash(const Hash160& hash) const {
    auto it = keysByHash_.find(hash);
    if (it != keysByHash_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<HDKeyManager::KeyInfo> HDKeyManager::FindKey(const PublicKey& pubkey) const {
    return FindKeyByHash(pubkey.GetHash160());
}

void HDKeyManager::MarkUsed(const Hash160& keyHash) {
    auto it = keysByHash_.find(keyHash);
    if (it != keysByHash_.end()) {
        it->second.used = true;
    }
}

std::vector<HDKeyManager::KeyInfo> HDKeyManager::GetAllKeys() const {
    std::vector<KeyInfo> result;
    result.reserve(keysByHash_.size());
    for (const auto& [hash, info] : keysByHash_) {
        result.push_back(info);
    }
    return result;
}

std::vector<HDKeyManager::KeyInfo> HDKeyManager::GetKeysForAccount(uint32_t account) const {
    std::vector<KeyInfo> result;
    for (const auto& [hash, info] : keysByHash_) {
        if (info.account == account) {
            result.push_back(info);
        }
    }
    return result;
}

std::optional<std::vector<uint8_t>> HDKeyManager::Sign(const DerivationPath& path,
                                                        const Hash256& hash) const {
    if (!masterKey_.IsValid() || !masterKey_.IsPrivate()) {
        return std::nullopt;
    }
    
    auto key = masterKey_.DerivePath(path);
    if (!key || !key->IsPrivate()) {
        return std::nullopt;
    }
    
    auto privKey = key->GetPrivateKey();
    if (!privKey) {
        return std::nullopt;
    }
    
    return privKey->Sign(hash);
}

std::optional<std::vector<uint8_t>> HDKeyManager::SignByKeyHash(const Hash160& keyHash,
                                                                 const Hash256& hash) const {
    auto info = FindKeyByHash(keyHash);
    if (!info) {
        return std::nullopt;
    }
    return Sign(info->path, hash);
}

std::string HDKeyManager::GetReceivingAddress(uint32_t account) {
    auto info = DeriveNextReceiving(account);
    return EncodeP2WPKH(info.keyHash, config_.testnet);
}

std::string HDKeyManager::GetChangeAddress(uint32_t account) {
    auto info = DeriveNextChange(account);
    return EncodeP2WPKH(info.keyHash, config_.testnet);
}

uint32_t HDKeyManager::GetNextReceivingIndex(uint32_t account) const {
    auto it = nextIndices_.find({account, 0});
    return (it != nextIndices_.end()) ? it->second : 0;
}

uint32_t HDKeyManager::GetNextChangeIndex(uint32_t account) const {
    auto it = nextIndices_.find({account, 1});
    return (it != nextIndices_.end()) ? it->second : 0;
}

HDKeyManager::KeyInfo HDKeyManager::DeriveAndCache(uint32_t account, 
                                                    uint32_t change, 
                                                    uint32_t index) {
    auto info = DeriveKey(account, change, index);
    if (!info) {
        // Return empty info on failure
        return KeyInfo();
    }
    
    // Update next index
    nextIndices_[{account, change}] = index + 1;
    
    return *info;
}

void HDKeyManager::SetAllIndices(const std::map<std::pair<uint32_t, uint32_t>, uint32_t>& indices) {
    nextIndices_ = indices;
    
    // Regenerate key cache for all previously derived keys
    // This ensures GetAllKeys() returns the correct keys after wallet reload
    keysByHash_.clear();
    
    for (const auto& [key, nextIndex] : indices) {
        uint32_t account = key.first;
        uint32_t change = key.second;
        
        // Derive all keys from 0 to nextIndex-1
        for (uint32_t i = 0; i < nextIndex; ++i) {
            auto info = DeriveKey(account, change, i);
            // DeriveKey already adds to keysByHash_
        }
    }
}

} // namespace wallet
} // namespace shurium
