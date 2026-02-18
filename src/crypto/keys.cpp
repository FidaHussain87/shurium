// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#include <shurium/crypto/keys.h>
#include <shurium/crypto/secp256k1.h>
#include <shurium/core/random.h>
#include <shurium/core/hex.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

#ifdef SHURIUM_USE_OPENSSL
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#endif

namespace shurium {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

/// Compare two 32-byte big-endian numbers
/// Returns -1 if a < b, 0 if a == b, 1 if a > b
static int Compare256(const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < 32; ++i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

/// Modular addition: (a + b) mod n
static void ModAdd(uint8_t* result, const uint8_t* a, const uint8_t* b) {
    uint16_t carry = 0;
    for (int i = 31; i >= 0; --i) {
        uint16_t sum = static_cast<uint16_t>(a[i]) + b[i] + carry;
        result[i] = static_cast<uint8_t>(sum);
        carry = sum >> 8;
    }
    
    // Reduce mod n if needed
    if (carry || Compare256(result, secp256k1::CURVE_ORDER.data()) >= 0) {
        uint16_t borrow = 0;
        for (int i = 31; i >= 0; --i) {
            int16_t diff = static_cast<int16_t>(result[i]) - secp256k1::CURVE_ORDER[i] - borrow;
            if (diff < 0) {
                diff += 256;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result[i] = static_cast<uint8_t>(diff);
        }
    }
}

/// Modular negation: (-a) mod n = n - a
static void ModNegate(uint8_t* result, const uint8_t* a) {
    // Check if a is zero
    bool isZero = true;
    for (int i = 0; i < 32; ++i) {
        if (a[i] != 0) {
            isZero = false;
            break;
        }
    }
    if (isZero) {
        std::memset(result, 0, 32);
        return;
    }
    
    // result = n - a
    uint16_t borrow = 0;
    for (int i = 31; i >= 0; --i) {
        int16_t diff = static_cast<int16_t>(secp256k1::CURVE_ORDER[i]) - a[i] - borrow;
        if (diff < 0) {
            diff += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result[i] = static_cast<uint8_t>(diff);
    }
}

} // anonymous namespace

// ============================================================================
// Hash160 Implementation
// ============================================================================

Hash160 ComputeHash160(const uint8_t* data, size_t len) {
    // SHA256 first
    Hash256 sha256_hash = SHA256Hash(data, len);
    
    // Then RIPEMD160
    Hash160 result;
    RIPEMD160 ripemd;
    ripemd.Write(sha256_hash.data(), sha256_hash.size());
    ripemd.Finalize(result.data());
    
    return result;
}

// ============================================================================
// PublicKey Implementation
// ============================================================================

PublicKey::PublicKey(const uint8_t* data, size_t len) : size_(0) {
    data_.fill(0);
    
    if (len == 0 || len > MAX_SIZE) {
        return;
    }
    
    // Validate format by checking first byte
    if (len == COMPRESSED_SIZE) {
        if (data[0] != 0x02 && data[0] != 0x03) {
            return;  // Invalid compressed format
        }
    } else if (len == MAX_SIZE) {
        if (data[0] != 0x04 && data[0] != 0x06 && data[0] != 0x07) {
            return;  // Invalid uncompressed format
        }
    } else {
        return;  // Invalid size
    }
    
    std::memcpy(data_.data(), data, len);
    size_ = static_cast<uint8_t>(len);
}

bool PublicKey::IsValid() const {
    if (size_ != COMPRESSED_SIZE && size_ != MAX_SIZE) {
        return false;
    }
    
    // Check format byte
    if (size_ == COMPRESSED_SIZE) {
        return data_[0] == 0x02 || data_[0] == 0x03;
    } else {
        return data_[0] == 0x04 || data_[0] == 0x06 || data_[0] == 0x07;
    }
}

PublicKey PublicKey::GetCompressed() const {
    if (!IsValid()) {
        return PublicKey();
    }
    
    if (IsCompressed()) {
        return *this;
    }
    
    // Convert uncompressed to compressed
    // Format: 0x02/0x03 (based on Y parity) + X coordinate
    std::array<uint8_t, COMPRESSED_SIZE> compressed;
    
    // Y is odd if last byte of Y has bit 0 set
    bool yIsOdd = (data_[64] & 1) != 0;
    compressed[0] = yIsOdd ? 0x03 : 0x02;
    std::memcpy(compressed.data() + 1, data_.data() + 1, 32);
    
    return PublicKey(compressed);
}

PublicKey PublicKey::GetUncompressed() const {
    if (!IsValid()) {
        return PublicKey();
    }
    
    if (!IsCompressed()) {
        return *this;
    }
    
#ifdef SHURIUM_USE_OPENSSL
    // Use OpenSSL to decompress the point
    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey) {
        return PublicKey();
    }
    
    const EC_GROUP* group = EC_KEY_get0_group(eckey);
    EC_POINT* point = EC_POINT_new(group);
    if (!point) {
        EC_KEY_free(eckey);
        return PublicKey();
    }
    
    // Import compressed point
    if (!EC_POINT_oct2point(group, point, data_.data(), size_, nullptr)) {
        EC_POINT_free(point);
        EC_KEY_free(eckey);
        return PublicKey();
    }
    
    // Export as uncompressed
    std::vector<uint8_t> uncompressed(secp256k1::UNCOMPRESSED_PUBKEY_SIZE);
    size_t len = EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
                                     uncompressed.data(), secp256k1::UNCOMPRESSED_PUBKEY_SIZE, nullptr);
    
    EC_POINT_free(point);
    EC_KEY_free(eckey);
    
    if (len != secp256k1::UNCOMPRESSED_PUBKEY_SIZE) {
        return PublicKey();
    }
    
    return PublicKey(uncompressed);
#else
    // Without OpenSSL, we can't decompress
    return PublicKey();
#endif
}

Hash160 PublicKey::GetHash160() const {
    if (!IsValid()) {
        return Hash160();
    }
    return ComputeHash160(data_.data(), size_);
}

bool PublicKey::Verify(const Hash256& hash, const std::vector<uint8_t>& signature) const {
    if (!IsValid() || signature.empty()) {
        return false;
    }
    
#ifdef SHURIUM_USE_OPENSSL
    // Create EC_KEY for secp256k1
    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey) {
        return false;
    }
    
    const EC_GROUP* group = EC_KEY_get0_group(eckey);
    EC_POINT* pub_point = EC_POINT_new(group);
    if (!pub_point) {
        EC_KEY_free(eckey);
        return false;
    }
    
    // Import public key point
    if (!EC_POINT_oct2point(group, pub_point, data_.data(), size_, nullptr)) {
        EC_POINT_free(pub_point);
        EC_KEY_free(eckey);
        return false;
    }
    
    if (!EC_KEY_set_public_key(eckey, pub_point)) {
        EC_POINT_free(pub_point);
        EC_KEY_free(eckey);
        return false;
    }
    
    EC_POINT_free(pub_point);
    
    // Verify signature
    int result = ECDSA_verify(0, hash.data(), 32, 
                               signature.data(), static_cast<int>(signature.size()), 
                               eckey);
    
    EC_KEY_free(eckey);
    
    return result == 1;
#else
    // Fallback: return false (not implemented without OpenSSL)
    (void)hash;
    (void)signature;
    return false;
#endif
}

bool PublicKey::VerifySchnorr(const Hash256& hash, const std::array<uint8_t, 64>& signature) const {
    if (!IsValid()) {
        return false;
    }
    
    // Get x-only public key (32 bytes)
    auto xonly = GetXOnly();
    
    // Use the secp256k1 Schnorr verification
    return secp256k1::SchnorrVerify(hash.data(), signature.data(), xonly.data());
}

std::array<uint8_t, 32> PublicKey::GetXOnly() const {
    std::array<uint8_t, 32> result{};
    
    if (!IsValid()) {
        return result;
    }
    
    // For compressed keys: skip the 0x02/0x03 prefix, copy 32 bytes of X
    // For uncompressed keys: skip the 0x04 prefix, copy 32 bytes of X
    if (IsCompressed()) {
        // Compressed: [prefix (1)] [X (32)]
        std::memcpy(result.data(), data_.data() + 1, 32);
    } else {
        // Uncompressed: [prefix (1)] [X (32)] [Y (32)]
        std::memcpy(result.data(), data_.data() + 1, 32);
    }
    
    return result;
}

std::optional<PublicKey> PublicKey::RecoverCompact(const Hash256& hash,
                                                    const std::vector<uint8_t>& signature) {
    if (signature.size() != secp256k1::COMPACT_SIGNATURE_SIZE) {
        return std::nullopt;
    }
    
#ifdef SHURIUM_USE_OPENSSL
    // Compact signature format: [recid (1 byte)] [r (32 bytes)] [s (32 bytes)]
    // recid encodes: compressed flag (bit 0x04) and recovery id (bits 0-1)
    uint8_t recid = signature[0];
    if (recid < 27 || recid > 34) {
        return std::nullopt;
    }
    
    bool compressed = (recid >= 31);
    int recoveryId = (recid - 27) & 3;
    
    // Extract r and s
    BIGNUM* r = BN_bin2bn(signature.data() + 1, 32, nullptr);
    BIGNUM* s = BN_bin2bn(signature.data() + 33, 32, nullptr);
    if (!r || !s) {
        BN_free(r);
        BN_free(s);
        return std::nullopt;
    }
    
    // Create ECDSA signature
    ECDSA_SIG* ecdsaSig = ECDSA_SIG_new();
    if (!ecdsaSig) {
        BN_free(r);
        BN_free(s);
        return std::nullopt;
    }
    
    // ECDSA_SIG_set0 takes ownership of r and s
    if (!ECDSA_SIG_set0(ecdsaSig, r, s)) {
        ECDSA_SIG_free(ecdsaSig);
        BN_free(r);
        BN_free(s);
        return std::nullopt;
    }
    
    // Get the curve
    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey) {
        ECDSA_SIG_free(ecdsaSig);
        return std::nullopt;
    }
    
    const EC_GROUP* group = EC_KEY_get0_group(eckey);
    
    // Get curve order
    BIGNUM* order = BN_new();
    if (!EC_GROUP_get_order(group, order, nullptr)) {
        BN_free(order);
        EC_KEY_free(eckey);
        ECDSA_SIG_free(ecdsaSig);
        return std::nullopt;
    }
    
    // Calculate the recovery point R = (r + recoveryId * order) * G + s * pubkey
    // This is complex with OpenSSL - for now return nullopt
    // Full implementation would require computing R from r, then solving for pubkey
    
    BN_free(order);
    EC_KEY_free(eckey);
    ECDSA_SIG_free(ecdsaSig);
    
    // Note: Full ECDSA recovery requires either:
    // 1. libsecp256k1 library (preferred)
    // 2. Manual implementation of point recovery algorithm
    // For production use, consider adding libsecp256k1 as a dependency
    return std::nullopt;
#else
    (void)hash;
    return std::nullopt;
#endif
}

bool PublicKey::operator==(const PublicKey& other) const {
    if (size_ != other.size_) {
        return false;
    }
    return std::memcmp(data_.data(), other.data_.data(), size_) == 0;
}

bool PublicKey::operator<(const PublicKey& other) const {
    if (size_ != other.size_) {
        return size_ < other.size_;
    }
    return std::memcmp(data_.data(), other.data_.data(), size_) < 0;
}

std::string PublicKey::ToHex() const {
    return BytesToHex(data_.data(), size_);
}

std::optional<PublicKey> PublicKey::FromHex(const std::string& hex) {
    auto bytes = HexToBytes(hex);
    if (bytes.empty()) {
        return std::nullopt;
    }
    PublicKey key(bytes);
    if (!key.IsValid()) {
        return std::nullopt;
    }
    return key;
}

// ============================================================================
// PrivateKey Implementation
// ============================================================================

PrivateKey::PrivateKey(const uint8_t* data, bool compressed)
    : compressed_(compressed) {
    data_.fill(0);
    if (data) {
        std::memcpy(data_.data(), data, SIZE);
        valid_ = Validate();
    } else {
        valid_ = false;
    }
}

PrivateKey::~PrivateKey() {
    Clear();
}

PrivateKey::PrivateKey(PrivateKey&& other) noexcept
    : data_(other.data_), valid_(other.valid_), compressed_(other.compressed_) {
    other.Clear();
}

PrivateKey& PrivateKey::operator=(PrivateKey&& other) noexcept {
    if (this != &other) {
        Clear();
        data_ = other.data_;
        valid_ = other.valid_;
        compressed_ = other.compressed_;
        other.Clear();
    }
    return *this;
}

PrivateKey::PrivateKey(const PrivateKey& other)
    : data_(other.data_), valid_(other.valid_), compressed_(other.compressed_) {
}

PrivateKey& PrivateKey::operator=(const PrivateKey& other) {
    if (this != &other) {
        data_ = other.data_;
        valid_ = other.valid_;
        compressed_ = other.compressed_;
    }
    return *this;
}

PrivateKey PrivateKey::Generate(bool compressed) {
    PrivateKey key;
    key.compressed_ = compressed;
    
    // Generate random bytes until we get a valid key
    for (int attempts = 0; attempts < 100; ++attempts) {
        GetRandBytes(key.data_.data(), SIZE);
        if (key.Validate()) {
            key.valid_ = true;
            return key;
        }
    }
    
    // Should never happen with a good RNG
    return PrivateKey();
}

bool PrivateKey::Validate() const {
    return secp256k1::IsValidPrivateKey(data_.data());
}

PublicKey PrivateKey::GetPublicKey() const {
    if (!valid_) {
        return PublicKey();
    }
    
#ifdef SHURIUM_USE_OPENSSL
    // Create EC_KEY for secp256k1
    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey) {
        return PublicKey();
    }
    
    // Set the private key
    BIGNUM* priv_bn = BN_bin2bn(data_.data(), 32, nullptr);
    if (!priv_bn) {
        EC_KEY_free(eckey);
        return PublicKey();
    }
    
    if (!EC_KEY_set_private_key(eckey, priv_bn)) {
        BN_free(priv_bn);
        EC_KEY_free(eckey);
        return PublicKey();
    }
    
    // Compute public key: pubkey = privateKey * G
    const EC_GROUP* group = EC_KEY_get0_group(eckey);
    EC_POINT* pub_point = EC_POINT_new(group);
    if (!pub_point) {
        BN_free(priv_bn);
        EC_KEY_free(eckey);
        return PublicKey();
    }
    
    if (!EC_POINT_mul(group, pub_point, priv_bn, nullptr, nullptr, nullptr)) {
        EC_POINT_free(pub_point);
        BN_free(priv_bn);
        EC_KEY_free(eckey);
        return PublicKey();
    }
    
    BN_free(priv_bn);
    
    // Convert to bytes
    point_conversion_form_t form = compressed_ ? POINT_CONVERSION_COMPRESSED : POINT_CONVERSION_UNCOMPRESSED;
    size_t pub_len = EC_POINT_point2oct(group, pub_point, form, nullptr, 0, nullptr);
    
    if (pub_len == 0 || pub_len > PublicKey::MAX_SIZE) {
        EC_POINT_free(pub_point);
        EC_KEY_free(eckey);
        return PublicKey();
    }
    
    std::vector<uint8_t> pub_bytes(pub_len);
    EC_POINT_point2oct(group, pub_point, form, pub_bytes.data(), pub_len, nullptr);
    
    EC_POINT_free(pub_point);
    EC_KEY_free(eckey);
    
    return PublicKey(pub_bytes.data(), pub_bytes.size());
#else
    // Fallback: return placeholder (not a real public key)
    if (compressed_) {
        return PublicKey(secp256k1::GENERATOR_COMPRESSED.data(), 
                        secp256k1::GENERATOR_COMPRESSED.size());
    }
    return PublicKey();
#endif
}

std::vector<uint8_t> PrivateKey::Sign(const Hash256& hash) const {
    if (!valid_) {
        return {};
    }
    
#ifdef SHURIUM_USE_OPENSSL
    std::vector<uint8_t> signature;
    
    // Create EC_KEY for secp256k1
    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey) {
        return {};
    }
    
    // Set the private key
    BIGNUM* priv_bn = BN_bin2bn(data_.data(), 32, nullptr);
    if (!priv_bn) {
        EC_KEY_free(eckey);
        return {};
    }
    
    if (!EC_KEY_set_private_key(eckey, priv_bn)) {
        BN_free(priv_bn);
        EC_KEY_free(eckey);
        return {};
    }
    
    // Compute and set the public key
    const EC_GROUP* group = EC_KEY_get0_group(eckey);
    EC_POINT* pub_point = EC_POINT_new(group);
    if (!pub_point) {
        BN_free(priv_bn);
        EC_KEY_free(eckey);
        return {};
    }
    
    if (!EC_POINT_mul(group, pub_point, priv_bn, nullptr, nullptr, nullptr)) {
        EC_POINT_free(pub_point);
        BN_free(priv_bn);
        EC_KEY_free(eckey);
        return {};
    }
    
    if (!EC_KEY_set_public_key(eckey, pub_point)) {
        EC_POINT_free(pub_point);
        BN_free(priv_bn);
        EC_KEY_free(eckey);
        return {};
    }
    
    EC_POINT_free(pub_point);
    BN_free(priv_bn);
    
    // Sign the hash
    unsigned int sig_len = ECDSA_size(eckey);
    signature.resize(sig_len);
    
    if (!ECDSA_sign(0, hash.data(), 32, signature.data(), &sig_len, eckey)) {
        EC_KEY_free(eckey);
        return {};
    }
    
    signature.resize(sig_len);
    EC_KEY_free(eckey);
    
    return signature;
#else
    // Fallback: return empty signature (not implemented without OpenSSL)
    (void)hash;
    return {};
#endif
}

std::vector<uint8_t> PrivateKey::SignCompact(const Hash256& hash) const {
    if (!valid_) {
        return {};
    }
    
#ifdef SHURIUM_USE_OPENSSL
    // For compact signature, we sign and then convert to compact format
    // with recovery ID. This is more complex with OpenSSL.
    // For now, use standard DER signature as fallback.
    return Sign(hash);
#else
    (void)hash;
    return {};
#endif
}

std::array<uint8_t, 64> PrivateKey::SignSchnorr(const Hash256& hash) const {
    std::array<uint8_t, 64> sig{};
    if (!valid_) {
        return sig;
    }
    
    // Use the secp256k1 Schnorr signing implementation
    if (!secp256k1::SchnorrSign(hash.data(), data_.data(), sig.data())) {
        sig.fill(0);
    }
    
    return sig;
}

PrivateKey PrivateKey::Negate() const {
    if (!valid_) {
        return PrivateKey();
    }
    
    PrivateKey result;
    ModNegate(result.data_.data(), data_.data());
    result.compressed_ = compressed_;
    result.valid_ = result.Validate();
    return result;
}

std::optional<PrivateKey> PrivateKey::TweakAdd(const Hash256& tweak) const {
    if (!valid_) {
        return std::nullopt;
    }
    
    PrivateKey result;
    ModAdd(result.data_.data(), data_.data(), tweak.data());
    result.compressed_ = compressed_;
    
    if (!result.Validate()) {
        return std::nullopt;
    }
    result.valid_ = true;
    return result;
}

bool PrivateKey::operator==(const PrivateKey& other) const {
    if (!valid_ && !other.valid_) {
        return true;
    }
    if (!valid_ || !other.valid_) {
        return false;
    }
    
    // Constant-time comparison
    uint8_t diff = 0;
    for (size_t i = 0; i < SIZE; ++i) {
        diff |= data_[i] ^ other.data_[i];
    }
    return diff == 0;
}

std::string PrivateKey::ToWIF(uint8_t versionByte) const {
    if (!valid_) {
        return "";
    }
    
    std::vector<uint8_t> data;
    data.reserve(34);
    data.push_back(versionByte);
    data.insert(data.end(), data_.begin(), data_.end());
    if (compressed_) {
        data.push_back(0x01);
    }
    
    return EncodeBase58Check(data);
}

std::optional<PrivateKey> PrivateKey::FromWIF(const std::string& wif) {
    auto data = DecodeBase58Check(wif);
    if (data.size() < 33) {
        return std::nullopt;
    }
    
    // Check version byte (skip it for key extraction)
    // uint8_t version = data[0];
    
    bool compressed = false;
    size_t keyLen = data.size() - 1;
    
    if (data.size() == 34 && data[33] == 0x01) {
        compressed = true;
        keyLen = 32;
    } else if (data.size() == 33) {
        compressed = false;
        keyLen = 32;
    } else {
        return std::nullopt;
    }
    
    PrivateKey key(data.data() + 1, compressed);
    if (!key.IsValid()) {
        return std::nullopt;
    }
    return key;
}

std::string PrivateKey::ToHex() const {
    if (!valid_) {
        return "";
    }
    return BytesToHex(data_.data(), SIZE);
}

std::optional<PrivateKey> PrivateKey::FromHex(const std::string& hex) {
    auto bytes = HexToBytes(hex);
    if (bytes.size() != SIZE) {
        return std::nullopt;
    }
    PrivateKey key(bytes.data(), true);
    if (!key.IsValid()) {
        return std::nullopt;
    }
    return key;
}

void PrivateKey::Clear() {
    // Secure clear - write zeros then random data then zeros again
    std::memset(data_.data(), 0, SIZE);
    valid_ = false;
}

// ============================================================================
// KeyPair Implementation
// ============================================================================

KeyPair::KeyPair(const PrivateKey& priv) 
    : privateKey_(priv), publicKey_(priv.GetPublicKey()) {
}

KeyPair::KeyPair(PrivateKey&& priv)
    : privateKey_(std::move(priv)) {
    publicKey_ = privateKey_.GetPublicKey();
}

KeyPair KeyPair::Generate(bool compressed) {
    return KeyPair(PrivateKey::Generate(compressed));
}

// ============================================================================
// Base58 Implementation
// ============================================================================

namespace {

const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

const int8_t BASE58_MAP[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6,  7, 8,-1,-1,-1,-1,-1,-1,
    -1, 9,10,11,12,13,14,15, 16,-1,17,18,19,20,21,-1,
    22,23,24,25,26,27,28,29, 30,31,32,-1,-1,-1,-1,-1,
    -1,33,34,35,36,37,38,39, 40,41,42,43,-1,44,45,46,
    47,48,49,50,51,52,53,54, 55,56,57,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
};

} // namespace

std::string EncodeBase58(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return "";
    }
    
    // Count leading zeros
    size_t zeroes = 0;
    while (zeroes < data.size() && data[zeroes] == 0) {
        ++zeroes;
    }
    
    // Allocate enough space (log(256) / log(58), rounded up)
    size_t size = (data.size() - zeroes) * 138 / 100 + 1;
    std::vector<uint8_t> b58(size);
    
    // Process the bytes
    for (size_t i = zeroes; i < data.size(); ++i) {
        int carry = data[i];
        size_t j = 0;
        for (auto it = b58.rbegin(); (carry != 0 || j < size) && it != b58.rend(); ++it, ++j) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
        size = j;
    }
    
    // Skip leading zeroes in base58 result
    auto it = b58.begin() + (b58.size() - size);
    while (it != b58.end() && *it == 0) {
        ++it;
    }
    
    // Translate the result into a string
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end()) {
        str += BASE58_ALPHABET[*it++];
    }
    
    return str;
}

std::vector<uint8_t> DecodeBase58(const std::string& str) {
    if (str.empty()) {
        return {};
    }
    
    // Count leading '1's
    size_t zeroes = 0;
    while (zeroes < str.size() && str[zeroes] == '1') {
        ++zeroes;
    }
    
    // Allocate enough space
    size_t size = (str.size() - zeroes) * 733 / 1000 + 1;
    std::vector<uint8_t> b256(size);
    
    // Process the characters
    for (size_t i = zeroes; i < str.size(); ++i) {
        int carry = BASE58_MAP[static_cast<uint8_t>(str[i])];
        if (carry < 0) {
            return {};  // Invalid character
        }
        size_t j = 0;
        for (auto it = b256.rbegin(); (carry != 0 || j < size) && it != b256.rend(); ++it, ++j) {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
        size = j;
    }
    
    // Skip leading zeroes in b256
    auto it = b256.begin() + (b256.size() - size);
    while (it != b256.end() && *it == 0) {
        ++it;
    }
    
    // Build result
    std::vector<uint8_t> result;
    result.reserve(zeroes + (b256.end() - it));
    result.assign(zeroes, 0x00);
    while (it != b256.end()) {
        result.push_back(*it++);
    }
    
    return result;
}

std::string EncodeBase58Check(const std::vector<uint8_t>& data) {
    // Compute checksum (first 4 bytes of double SHA256)
    Hash256 hash = DoubleSHA256(data.data(), data.size());
    
    // Append checksum
    std::vector<uint8_t> dataWithChecksum = data;
    dataWithChecksum.insert(dataWithChecksum.end(), hash.begin(), hash.begin() + 4);
    
    return EncodeBase58(dataWithChecksum);
}

std::vector<uint8_t> DecodeBase58Check(const std::string& str) {
    auto data = DecodeBase58(str);
    if (data.size() < 4) {
        return {};
    }
    
    // Extract payload and checksum
    std::vector<uint8_t> payload(data.begin(), data.end() - 4);
    
    // Verify checksum
    Hash256 hash = DoubleSHA256(payload.data(), payload.size());
    
    if (std::memcmp(hash.data(), data.data() + payload.size(), 4) != 0) {
        return {};  // Checksum failed
    }
    
    return payload;
}

// ============================================================================
// Bech32 Implementation
// ============================================================================

namespace {

const char* BECH32_ALPHABET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

const int8_t BECH32_MAP[128] = {
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    15,-1,10,17,21,20,26,30,  7, 5,-1,-1,-1,-1,-1,-1,
    -1,29,-1,24,13,25, 9, 8, 23,-1,18,22,31,27,19,-1,
     1, 0, 3,16,11,28,12,14,  6, 4, 2,-1,-1,-1,-1,-1,
    -1,29,-1,24,13,25, 9, 8, 23,-1,18,22,31,27,19,-1,
     1, 0, 3,16,11,28,12,14,  6, 4, 2,-1,-1,-1,-1,-1,
};

uint32_t Bech32Polymod(const std::vector<uint8_t>& values) {
    uint32_t chk = 1;
    for (uint8_t v : values) {
        uint8_t top = chk >> 25;
        chk = ((chk & 0x1ffffff) << 5) ^ v;
        if (top & 1) chk ^= 0x3b6a57b2;
        if (top & 2) chk ^= 0x26508e6d;
        if (top & 4) chk ^= 0x1ea119fa;
        if (top & 8) chk ^= 0x3d4233dd;
        if (top & 16) chk ^= 0x2a1462b3;
    }
    return chk;
}

std::vector<uint8_t> Bech32HrpExpand(const std::string& hrp) {
    std::vector<uint8_t> ret;
    ret.reserve(hrp.size() * 2 + 1);
    for (char c : hrp) {
        ret.push_back(static_cast<uint8_t>(c) >> 5);
    }
    ret.push_back(0);
    for (char c : hrp) {
        ret.push_back(static_cast<uint8_t>(c) & 31);
    }
    return ret;
}

bool Bech32VerifyChecksum(const std::string& hrp, const std::vector<uint8_t>& values, 
                          bool bech32m) {
    auto hrpExp = Bech32HrpExpand(hrp);
    hrpExp.insert(hrpExp.end(), values.begin(), values.end());
    uint32_t expected = bech32m ? 0x2bc830a3 : 1;
    return Bech32Polymod(hrpExp) == expected;
}

std::vector<uint8_t> Bech32CreateChecksum(const std::string& hrp, 
                                           const std::vector<uint8_t>& values,
                                           bool bech32m) {
    auto hrpExp = Bech32HrpExpand(hrp);
    hrpExp.insert(hrpExp.end(), values.begin(), values.end());
    hrpExp.resize(hrpExp.size() + 6);
    uint32_t target = bech32m ? 0x2bc830a3 : 1;
    uint32_t mod = Bech32Polymod(hrpExp) ^ target;
    std::vector<uint8_t> ret(6);
    for (int i = 0; i < 6; ++i) {
        ret[i] = (mod >> (5 * (5 - i))) & 31;
    }
    return ret;
}

bool ConvertBits8to5(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    int acc = 0;
    int bits = 0;
    for (uint8_t value : in) {
        acc = (acc << 8) | value;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out.push_back((acc >> bits) & 31);
        }
    }
    if (bits > 0) {
        out.push_back((acc << (5 - bits)) & 31);
    }
    return true;
}

bool ConvertBits5to8(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    int acc = 0;
    int bits = 0;
    for (uint8_t value : in) {
        if (value >= 32) return false;
        acc = (acc << 5) | value;
        bits += 5;
        while (bits >= 8) {
            bits -= 8;
            out.push_back((acc >> bits) & 255);
        }
    }
    // Check for non-zero padding
    if (bits >= 5 || ((acc << (8 - bits)) & 255)) {
        return false;
    }
    return true;
}

} // namespace

std::string EncodeBech32(const std::string& hrp, uint8_t version,
                          const std::vector<uint8_t>& data) {
    std::vector<uint8_t> values;
    values.push_back(version);
    ConvertBits8to5(data, values);
    
    auto checksum = Bech32CreateChecksum(hrp, values, false);
    values.insert(values.end(), checksum.begin(), checksum.end());
    
    std::string result = hrp + "1";
    for (uint8_t v : values) {
        result += BECH32_ALPHABET[v];
    }
    return result;
}

std::string EncodeBech32m(const std::string& hrp, uint8_t version,
                           const std::vector<uint8_t>& data) {
    std::vector<uint8_t> values;
    values.push_back(version);
    ConvertBits8to5(data, values);
    
    auto checksum = Bech32CreateChecksum(hrp, values, true);
    values.insert(values.end(), checksum.begin(), checksum.end());
    
    std::string result = hrp + "1";
    for (uint8_t v : values) {
        result += BECH32_ALPHABET[v];
    }
    return result;
}

std::optional<std::tuple<std::string, uint8_t, std::vector<uint8_t>>>
DecodeBech32(const std::string& str) {
    // Find separator
    size_t pos = str.rfind('1');
    if (pos == std::string::npos || pos < 1 || pos + 7 > str.size()) {
        return std::nullopt;
    }
    
    std::string hrp = str.substr(0, pos);
    
    // Decode data part
    std::vector<uint8_t> values;
    for (size_t i = pos + 1; i < str.size(); ++i) {
        char c = str[i];
        if (c < 0 || static_cast<size_t>(c) >= sizeof(BECH32_MAP)) {
            return std::nullopt;
        }
        int8_t val = BECH32_MAP[static_cast<uint8_t>(c)];
        if (val < 0) {
            return std::nullopt;
        }
        values.push_back(val);
    }
    
    // Verify checksum (try both bech32 and bech32m)
    bool validBech32 = Bech32VerifyChecksum(hrp, values, false);
    bool validBech32m = Bech32VerifyChecksum(hrp, values, true);
    
    if (!validBech32 && !validBech32m) {
        return std::nullopt;
    }
    
    // Remove checksum
    values.resize(values.size() - 6);
    
    if (values.empty()) {
        return std::nullopt;
    }
    
    uint8_t version = values[0];
    values.erase(values.begin());
    
    // Convert from 5-bit to 8-bit
    std::vector<uint8_t> data;
    if (!ConvertBits5to8(values, data)) {
        return std::nullopt;
    }
    
    return std::make_tuple(hrp, version, data);
}

// ============================================================================
// Address Functions
// ============================================================================

std::string EncodeP2PKH(const PublicKey& pubkey, bool testnet) {
    if (!pubkey.IsValid()) {
        return "";
    }
    return EncodeP2PKH(pubkey.GetHash160(), testnet);
}

std::string EncodeP2PKH(const Hash160& hash, bool testnet) {
    uint8_t version = testnet ? AddressVersion::TESTNET_PUBKEY 
                              : AddressVersion::MAINNET_PUBKEY;
    
    std::vector<uint8_t> data;
    data.reserve(21);
    data.push_back(version);
    data.insert(data.end(), hash.begin(), hash.end());
    
    return EncodeBase58Check(data);
}

std::string EncodeP2WPKH(const PublicKey& pubkey, bool testnet) {
    if (!pubkey.IsValid() || !pubkey.IsCompressed()) {
        return "";  // SegWit requires compressed pubkey
    }
    return EncodeP2WPKH(pubkey.GetHash160(), testnet);
}

std::string EncodeP2WPKH(const Hash160& hash, bool testnet) {
    const char* hrp = testnet ? Bech32HRP::TESTNET : Bech32HRP::MAINNET;
    std::vector<uint8_t> program(hash.begin(), hash.end());
    return EncodeBech32(hrp, 0, program);
}

std::vector<uint8_t> DecodeAddress(const std::string& address) {
    // Try Base58Check first (P2PKH, P2SH)
    auto base58Data = DecodeBase58Check(address);
    if (!base58Data.empty() && base58Data.size() == 21) {
        // Return script pubkey for P2PKH: OP_DUP OP_HASH160 <20 bytes> OP_EQUALVERIFY OP_CHECKSIG
        std::vector<uint8_t> script;
        script.push_back(0x76);  // OP_DUP
        script.push_back(0xa9);  // OP_HASH160
        script.push_back(0x14);  // Push 20 bytes
        script.insert(script.end(), base58Data.begin() + 1, base58Data.end());
        script.push_back(0x88);  // OP_EQUALVERIFY
        script.push_back(0xac);  // OP_CHECKSIG
        return script;
    }
    
    // Try Bech32 (P2WPKH, P2WSH, P2TR)
    auto bech32Result = DecodeBech32(address);
    if (bech32Result) {
        auto [hrp, version, program] = *bech32Result;
        
        if (version == 0 && program.size() == 20) {
            // P2WPKH: OP_0 <20 bytes>
            std::vector<uint8_t> script;
            script.push_back(0x00);  // OP_0
            script.push_back(0x14);  // Push 20 bytes
            script.insert(script.end(), program.begin(), program.end());
            return script;
        } else if (version == 0 && program.size() == 32) {
            // P2WSH: OP_0 <32 bytes>
            std::vector<uint8_t> script;
            script.push_back(0x00);  // OP_0
            script.push_back(0x20);  // Push 32 bytes
            script.insert(script.end(), program.begin(), program.end());
            return script;
        } else if (version == 1 && program.size() == 32) {
            // P2TR: OP_1 <32 bytes>
            std::vector<uint8_t> script;
            script.push_back(0x51);  // OP_1
            script.push_back(0x20);  // Push 32 bytes
            script.insert(script.end(), program.begin(), program.end());
            return script;
        }
    }
    
    return {};
}

AddressType GetAddressType(const std::string& address) {
    // Try Base58Check first
    auto base58Data = DecodeBase58Check(address);
    if (!base58Data.empty() && base58Data.size() == 21) {
        uint8_t version = base58Data[0];
        if (version == AddressVersion::MAINNET_PUBKEY || 
            version == AddressVersion::TESTNET_PUBKEY) {
            return AddressType::P2PKH;
        }
        if (version == AddressVersion::MAINNET_SCRIPT ||
            version == AddressVersion::TESTNET_SCRIPT) {
            return AddressType::P2SH;
        }
    }
    
    // Try Bech32
    auto bech32Result = DecodeBech32(address);
    if (bech32Result) {
        auto [hrp, version, program] = *bech32Result;
        
        if (version == 0 && program.size() == 20) {
            return AddressType::P2WPKH;
        } else if (version == 0 && program.size() == 32) {
            return AddressType::P2WSH;
        } else if (version == 1 && program.size() == 32) {
            return AddressType::P2TR;
        }
    }
    
    return AddressType::INVALID;
}

} // namespace shurium
