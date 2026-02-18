// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#include <gtest/gtest.h>
#include <shurium/crypto/keys.h>
#include <shurium/core/hex.h>

namespace shurium {
namespace {

// ============================================================================
// Base58 Tests
// ============================================================================

TEST(Base58Test, EncodeEmpty) {
    std::vector<uint8_t> empty;
    EXPECT_EQ(EncodeBase58(empty), "");
}

TEST(Base58Test, EncodeZeros) {
    std::vector<uint8_t> zeros = {0, 0, 0};
    std::string encoded = EncodeBase58(zeros);
    EXPECT_EQ(encoded, "111");  // Leading zeros become '1's
}

TEST(Base58Test, EncodeSimple) {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03};
    std::string encoded = EncodeBase58(data);
    EXPECT_FALSE(encoded.empty());
    
    // Decode and verify roundtrip
    auto decoded = DecodeBase58(encoded);
    EXPECT_EQ(decoded, data);
}

TEST(Base58Test, DecodeInvalidChars) {
    // '0', 'O', 'I', 'l' are not in Base58 alphabet
    EXPECT_TRUE(DecodeBase58("0invalid").empty());
    EXPECT_TRUE(DecodeBase58("Oinvalid").empty());
    EXPECT_TRUE(DecodeBase58("Iinvalid").empty());
    EXPECT_TRUE(DecodeBase58("linvalid").empty());
}

TEST(Base58Test, RoundTrip) {
    std::vector<uint8_t> data = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
    };
    
    std::string encoded = EncodeBase58(data);
    auto decoded = DecodeBase58(encoded);
    EXPECT_EQ(decoded, data);
}

// ============================================================================
// Base58Check Tests
// ============================================================================

TEST(Base58CheckTest, EncodeWithVersion) {
    std::vector<uint8_t> data = {0x00};  // Version byte
    data.insert(data.end(), 20, 0x00);   // 20 zero bytes
    
    std::string encoded = EncodeBase58Check(data);
    EXPECT_FALSE(encoded.empty());
    EXPECT_TRUE(encoded.length() > 4);  // Has checksum
}

TEST(Base58CheckTest, DecodeValid) {
    // Create valid data
    std::vector<uint8_t> original = {0x00, 0x01, 0x02, 0x03, 0x04};
    std::string encoded = EncodeBase58Check(original);
    
    auto decoded = DecodeBase58Check(encoded);
    EXPECT_EQ(decoded, original);
}

TEST(Base58CheckTest, DecodeInvalidChecksum) {
    // Encode valid data
    std::vector<uint8_t> original = {0x00, 0x01, 0x02, 0x03, 0x04};
    std::string encoded = EncodeBase58Check(original);
    
    // Corrupt the last character
    if (!encoded.empty()) {
        encoded.back() = (encoded.back() == '1') ? '2' : '1';
    }
    
    auto decoded = DecodeBase58Check(encoded);
    EXPECT_TRUE(decoded.empty());  // Should fail checksum
}

TEST(Base58CheckTest, DecodeTooShort) {
    EXPECT_TRUE(DecodeBase58Check("1").empty());
    EXPECT_TRUE(DecodeBase58Check("11").empty());
    EXPECT_TRUE(DecodeBase58Check("111").empty());
}

// ============================================================================
// Bech32 Tests
// ============================================================================

TEST(Bech32Test, EncodeP2WPKH) {
    std::vector<uint8_t> program(20, 0x00);  // 20 zero bytes
    std::string address = EncodeBech32("nx", 0, program);
    
    EXPECT_FALSE(address.empty());
    EXPECT_EQ(address.substr(0, 3), "nx1");  // HRP + separator
}

TEST(Bech32Test, EncodeP2TR) {
    std::vector<uint8_t> program(32, 0xAB);  // 32 bytes
    std::string address = EncodeBech32m("nx", 1, program);
    
    EXPECT_FALSE(address.empty());
    EXPECT_EQ(address.substr(0, 3), "nx1");
}

TEST(Bech32Test, DecodeValid) {
    // Encode first
    std::vector<uint8_t> program(20, 0x42);
    std::string encoded = EncodeBech32("nx", 0, program);
    
    // Decode
    auto result = DecodeBech32(encoded);
    ASSERT_TRUE(result.has_value());
    
    auto [hrp, version, decoded] = *result;
    EXPECT_EQ(hrp, "nx");
    EXPECT_EQ(version, 0);
    EXPECT_EQ(decoded, program);
}

TEST(Bech32Test, DecodeBech32m) {
    std::vector<uint8_t> program(32, 0x11);
    std::string encoded = EncodeBech32m("nx", 1, program);
    
    auto result = DecodeBech32(encoded);
    ASSERT_TRUE(result.has_value());
    
    auto [hrp, version, decoded] = *result;
    EXPECT_EQ(hrp, "nx");
    EXPECT_EQ(version, 1);
    EXPECT_EQ(decoded, program);
}

TEST(Bech32Test, DecodeInvalid) {
    EXPECT_FALSE(DecodeBech32("").has_value());
    EXPECT_FALSE(DecodeBech32("nx").has_value());  // No separator
    EXPECT_FALSE(DecodeBech32("1invalid").has_value());  // Bad format
}

// ============================================================================
// Hash160 Tests
// ============================================================================

TEST(Hash160Test, ComputeFromData) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    Hash160 hash = ComputeHash160(data);
    
    EXPECT_EQ(hash.size(), 20u);
    EXPECT_FALSE(hash.IsNull());
}

TEST(Hash160Test, DifferentInputsDifferentHashes) {
    std::vector<uint8_t> data1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> data2 = {0x01, 0x02, 0x04};
    
    Hash160 hash1 = ComputeHash160(data1);
    Hash160 hash2 = ComputeHash160(data2);
    
    EXPECT_NE(hash1, hash2);
}

TEST(Hash160Test, SameInputSameHash) {
    std::vector<uint8_t> data = {0xAB, 0xCD, 0xEF};
    
    Hash160 hash1 = ComputeHash160(data);
    Hash160 hash2 = ComputeHash160(data);
    
    EXPECT_EQ(hash1, hash2);
}

// ============================================================================
// PublicKey Tests
// ============================================================================

TEST(PublicKeyTest, DefaultConstructor) {
    PublicKey key;
    EXPECT_FALSE(key.IsValid());
    EXPECT_EQ(key.size(), 0u);
}

TEST(PublicKeyTest, ConstructFromCompressed) {
    // Valid compressed public key format (0x02 or 0x03 prefix)
    std::array<uint8_t, 33> compressed;
    compressed[0] = 0x02;
    for (size_t i = 1; i < 33; ++i) {
        compressed[i] = static_cast<uint8_t>(i);
    }
    
    PublicKey key(compressed);
    EXPECT_TRUE(key.IsValid());
    EXPECT_TRUE(key.IsCompressed());
    EXPECT_EQ(key.size(), 33u);
}

TEST(PublicKeyTest, ConstructFromUncompressed) {
    // Valid uncompressed public key format (0x04 prefix)
    std::vector<uint8_t> uncompressed(65);
    uncompressed[0] = 0x04;
    for (size_t i = 1; i < 65; ++i) {
        uncompressed[i] = static_cast<uint8_t>(i);
    }
    
    PublicKey key(uncompressed);
    EXPECT_TRUE(key.IsValid());
    EXPECT_FALSE(key.IsCompressed());
    EXPECT_EQ(key.size(), 65u);
}

TEST(PublicKeyTest, InvalidPrefix) {
    std::vector<uint8_t> invalid(33);
    invalid[0] = 0x05;  // Invalid prefix
    
    PublicKey key(invalid);
    EXPECT_FALSE(key.IsValid());
}

TEST(PublicKeyTest, InvalidSize) {
    std::vector<uint8_t> invalid(30);
    invalid[0] = 0x02;
    
    PublicKey key(invalid);
    EXPECT_FALSE(key.IsValid());
}

TEST(PublicKeyTest, GetHash160) {
    std::array<uint8_t, 33> compressed;
    compressed[0] = 0x02;
    for (size_t i = 1; i < 33; ++i) {
        compressed[i] = static_cast<uint8_t>(i);
    }
    
    PublicKey key(compressed);
    Hash160 hash = key.GetHash160();
    
    EXPECT_FALSE(hash.IsNull());
}

TEST(PublicKeyTest, ToHex) {
    std::array<uint8_t, 33> compressed;
    compressed[0] = 0x02;
    for (size_t i = 1; i < 33; ++i) {
        compressed[i] = static_cast<uint8_t>(i);
    }
    
    PublicKey key(compressed);
    std::string hex = key.ToHex();
    
    EXPECT_EQ(hex.length(), 66u);  // 33 bytes * 2
    EXPECT_EQ(hex.substr(0, 2), "02");
}

TEST(PublicKeyTest, FromHex) {
    std::string hex = "02" + std::string(64, '1');  // 02 + 32 bytes
    
    auto key = PublicKey::FromHex(hex);
    ASSERT_TRUE(key.has_value());
    EXPECT_TRUE(key->IsValid());
    EXPECT_TRUE(key->IsCompressed());
}

TEST(PublicKeyTest, Comparison) {
    std::array<uint8_t, 33> data1, data2;
    data1.fill(0x01);
    data2.fill(0x02);
    data1[0] = data2[0] = 0x02;
    
    PublicKey key1(data1);
    PublicKey key2(data2);
    PublicKey key3(data1);
    
    EXPECT_NE(key1, key2);
    EXPECT_EQ(key1, key3);
    EXPECT_LT(key1, key2);
}

TEST(PublicKeyTest, Serialization) {
    std::array<uint8_t, 33> compressed;
    compressed[0] = 0x03;
    for (size_t i = 1; i < 33; ++i) {
        compressed[i] = static_cast<uint8_t>(i * 2);
    }
    
    PublicKey key(compressed);
    
    DataStream stream;
    key.Serialize(stream);
    
    PublicKey key2;
    key2.Unserialize(stream);
    
    EXPECT_EQ(key, key2);
}

TEST(PublicKeyTest, GetCompressed) {
    // Create uncompressed key
    std::vector<uint8_t> uncompressed(65);
    uncompressed[0] = 0x04;
    for (size_t i = 1; i < 33; ++i) {
        uncompressed[i] = static_cast<uint8_t>(i);
    }
    for (size_t i = 33; i < 65; ++i) {
        uncompressed[i] = static_cast<uint8_t>(i - 32);
    }
    uncompressed[64] = 0x01;  // Make Y odd -> 0x03 prefix
    
    PublicKey key(uncompressed);
    ASSERT_TRUE(key.IsValid());
    EXPECT_FALSE(key.IsCompressed());
    
    PublicKey compressed = key.GetCompressed();
    EXPECT_TRUE(compressed.IsCompressed());
    EXPECT_EQ(compressed.size(), 33u);
    EXPECT_EQ(compressed.data()[0], 0x03);  // Y was odd
}

// ============================================================================
// PrivateKey Tests
// ============================================================================

TEST(PrivateKeyTest, DefaultConstructor) {
    PrivateKey key;
    EXPECT_FALSE(key.IsValid());
}

TEST(PrivateKeyTest, Generate) {
    PrivateKey key = PrivateKey::Generate();
    EXPECT_TRUE(key.IsValid());
    EXPECT_TRUE(key.IsCompressed());
}

TEST(PrivateKeyTest, GenerateUncompressed) {
    PrivateKey key = PrivateKey::Generate(false);
    EXPECT_TRUE(key.IsValid());
    EXPECT_FALSE(key.IsCompressed());
}

TEST(PrivateKeyTest, GenerateUnique) {
    PrivateKey key1 = PrivateKey::Generate();
    PrivateKey key2 = PrivateKey::Generate();
    
    EXPECT_TRUE(key1.IsValid());
    EXPECT_TRUE(key2.IsValid());
    EXPECT_NE(key1, key2);
}

TEST(PrivateKeyTest, ConstructFromBytes) {
    std::array<uint8_t, 32> data;
    data.fill(0x42);
    
    PrivateKey key(data);
    EXPECT_TRUE(key.IsValid());
}

TEST(PrivateKeyTest, ZeroKeyInvalid) {
    std::array<uint8_t, 32> zero;
    zero.fill(0x00);
    
    PrivateKey key(zero);
    EXPECT_FALSE(key.IsValid());
}

TEST(PrivateKeyTest, MaxKeyInvalid) {
    // Key >= curve order should be invalid
    std::array<uint8_t, 32> maxKey;
    maxKey.fill(0xFF);
    
    PrivateKey key(maxKey);
    EXPECT_FALSE(key.IsValid());
}

TEST(PrivateKeyTest, ToHex) {
    std::array<uint8_t, 32> data;
    for (size_t i = 0; i < 32; ++i) {
        data[i] = static_cast<uint8_t>(i + 1);
    }
    
    PrivateKey key(data);
    std::string hex = key.ToHex();
    
    EXPECT_EQ(hex.length(), 64u);
}

TEST(PrivateKeyTest, FromHex) {
    std::string hex = "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20";
    
    auto key = PrivateKey::FromHex(hex);
    ASSERT_TRUE(key.has_value());
    EXPECT_TRUE(key->IsValid());
    EXPECT_EQ(key->ToHex(), hex);
}

TEST(PrivateKeyTest, FromHexInvalidLength) {
    EXPECT_FALSE(PrivateKey::FromHex("0102030405").has_value());
    EXPECT_FALSE(PrivateKey::FromHex("").has_value());
}

TEST(PrivateKeyTest, ToWIF) {
    std::array<uint8_t, 32> data;
    data.fill(0x42);
    
    PrivateKey key(data, true);  // Compressed
    std::string wif = key.ToWIF();
    
    EXPECT_FALSE(wif.empty());
    // WIF for compressed keys typically starts with K or L on Bitcoin mainnet
    // Our SHURIUM version byte is different
}

TEST(PrivateKeyTest, FromWIF) {
    std::array<uint8_t, 32> data;
    data.fill(0x42);
    
    PrivateKey original(data, true);
    std::string wif = original.ToWIF();
    
    auto restored = PrivateKey::FromWIF(wif);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
    EXPECT_EQ(original.IsCompressed(), restored->IsCompressed());
}

TEST(PrivateKeyTest, FromWIFUncompressed) {
    std::array<uint8_t, 32> data;
    data.fill(0x42);
    
    PrivateKey original(data, false);  // Uncompressed
    std::string wif = original.ToWIF();
    
    auto restored = PrivateKey::FromWIF(wif);
    ASSERT_TRUE(restored.has_value());
    EXPECT_FALSE(restored->IsCompressed());
}

TEST(PrivateKeyTest, Clear) {
    PrivateKey key = PrivateKey::Generate();
    EXPECT_TRUE(key.IsValid());
    
    key.Clear();
    EXPECT_FALSE(key.IsValid());
}

TEST(PrivateKeyTest, MoveConstructor) {
    PrivateKey key1 = PrivateKey::Generate();
    EXPECT_TRUE(key1.IsValid());
    
    PrivateKey key2(std::move(key1));
    EXPECT_TRUE(key2.IsValid());
    EXPECT_FALSE(key1.IsValid());  // Moved from
}

TEST(PrivateKeyTest, MoveAssignment) {
    PrivateKey key1 = PrivateKey::Generate();
    PrivateKey key2;
    
    key2 = std::move(key1);
    EXPECT_TRUE(key2.IsValid());
    EXPECT_FALSE(key1.IsValid());
}

TEST(PrivateKeyTest, Negate) {
    std::array<uint8_t, 32> data;
    data.fill(0x42);
    
    PrivateKey key(data);
    PrivateKey negated = key.Negate();
    
    EXPECT_TRUE(negated.IsValid());
    EXPECT_NE(key, negated);
    
    // Double negation should give original
    PrivateKey doubleNegated = negated.Negate();
    EXPECT_EQ(key, doubleNegated);
}

TEST(PrivateKeyTest, TweakAdd) {
    std::array<uint8_t, 32> data;
    data.fill(0x42);
    
    PrivateKey key(data);
    
    Hash256 tweak;
    for (size_t i = 0; i < 32; ++i) {
        tweak[i] = static_cast<uint8_t>(i);
    }
    
    auto tweaked = key.TweakAdd(tweak);
    ASSERT_TRUE(tweaked.has_value());
    EXPECT_TRUE(tweaked->IsValid());
    EXPECT_NE(key, *tweaked);
}

// ============================================================================
// KeyPair Tests
// ============================================================================

TEST(KeyPairTest, Generate) {
    KeyPair kp = KeyPair::Generate();
    EXPECT_TRUE(kp.IsValid());
    EXPECT_TRUE(kp.GetPrivateKey().IsValid());
    // PublicKey generation not fully implemented yet
}

TEST(KeyPairTest, ConstructFromPrivateKey) {
    PrivateKey priv = PrivateKey::Generate();
    KeyPair kp(priv);
    
    EXPECT_TRUE(kp.IsValid());
    EXPECT_EQ(kp.GetPrivateKey(), priv);
}

// ============================================================================
// Address Encoding Tests
// ============================================================================

TEST(AddressTest, EncodeP2PKHFromHash) {
    Hash160 hash;
    for (size_t i = 0; i < 20; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    std::string address = EncodeP2PKH(hash, false);  // Mainnet
    EXPECT_FALSE(address.empty());
    // Just verify it's a valid Base58Check address
    EXPECT_GT(address.length(), 25u);
}

TEST(AddressTest, EncodeP2PKHTestnet) {
    Hash160 hash;
    hash.SetNull();
    
    std::string address = EncodeP2PKH(hash, true);  // Testnet
    EXPECT_FALSE(address.empty());
    // Testnet has different version byte
}

TEST(AddressTest, EncodeP2WPKH) {
    Hash160 hash;
    for (size_t i = 0; i < 20; ++i) {
        hash[i] = static_cast<uint8_t>(i + 10);
    }
    
    std::string address = EncodeP2WPKH(hash, false);
    EXPECT_FALSE(address.empty());
    EXPECT_EQ(address.substr(0, 3), "nx1");  // SHURIUM bech32 prefix
}

TEST(AddressTest, GetAddressTypeP2PKH) {
    Hash160 hash;
    hash.SetNull();
    
    std::string address = EncodeP2PKH(hash, false);
    AddressType type = GetAddressType(address);
    
    EXPECT_EQ(type, AddressType::P2PKH);
}

TEST(AddressTest, GetAddressTypeP2WPKH) {
    Hash160 hash;
    hash.SetNull();
    
    std::string address = EncodeP2WPKH(hash, false);
    AddressType type = GetAddressType(address);
    
    EXPECT_EQ(type, AddressType::P2WPKH);
}

TEST(AddressTest, GetAddressTypeInvalid) {
    EXPECT_EQ(GetAddressType(""), AddressType::INVALID);
    EXPECT_EQ(GetAddressType("invalid"), AddressType::INVALID);
}

TEST(AddressTest, DecodeP2PKH) {
    Hash160 hash;
    for (size_t i = 0; i < 20; ++i) {
        hash[i] = static_cast<uint8_t>(i + 1);
    }
    
    std::string address = EncodeP2PKH(hash, false);
    auto script = DecodeAddress(address);
    
    EXPECT_FALSE(script.empty());
    // P2PKH script: OP_DUP OP_HASH160 <20 bytes> OP_EQUALVERIFY OP_CHECKSIG
    EXPECT_EQ(script.size(), 25u);
    EXPECT_EQ(script[0], 0x76);  // OP_DUP
    EXPECT_EQ(script[1], 0xa9);  // OP_HASH160
    EXPECT_EQ(script[2], 0x14);  // Push 20 bytes
    EXPECT_EQ(script[23], 0x88); // OP_EQUALVERIFY
    EXPECT_EQ(script[24], 0xac); // OP_CHECKSIG
}

TEST(AddressTest, DecodeP2WPKH) {
    Hash160 hash;
    for (size_t i = 0; i < 20; ++i) {
        hash[i] = static_cast<uint8_t>(i + 5);
    }
    
    std::string address = EncodeP2WPKH(hash, false);
    auto script = DecodeAddress(address);
    
    EXPECT_FALSE(script.empty());
    // P2WPKH script: OP_0 <20 bytes>
    EXPECT_EQ(script.size(), 22u);
    EXPECT_EQ(script[0], 0x00);  // OP_0
    EXPECT_EQ(script[1], 0x14);  // Push 20 bytes
}

// ============================================================================
// Curve Constants Tests
// ============================================================================

TEST(Secp256k1Test, CurveOrderNonZero) {
    bool allZero = true;
    for (uint8_t byte : secp256k1::CURVE_ORDER) {
        if (byte != 0) {
            allZero = false;
            break;
        }
    }
    EXPECT_FALSE(allZero);
}

TEST(Secp256k1Test, PrivateKeyValidation) {
    // Valid key (all 0x01s should be valid)
    std::array<uint8_t, 32> validKey;
    validKey.fill(0x01);
    PrivateKey key1(validKey);
    EXPECT_TRUE(key1.IsValid());
    
    // Invalid key (all zeros)
    std::array<uint8_t, 32> zeroKey;
    zeroKey.fill(0x00);
    PrivateKey key2(zeroKey);
    EXPECT_FALSE(key2.IsValid());
}

// ============================================================================
// WIF Roundtrip Tests
// ============================================================================

TEST(WIFTest, RoundTripCompressed) {
    for (int i = 0; i < 10; ++i) {
        PrivateKey original = PrivateKey::Generate(true);
        std::string wif = original.ToWIF();
        auto restored = PrivateKey::FromWIF(wif);
        
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(original, *restored);
        EXPECT_TRUE(restored->IsCompressed());
    }
}

TEST(WIFTest, RoundTripUncompressed) {
    for (int i = 0; i < 10; ++i) {
        PrivateKey original = PrivateKey::Generate(false);
        std::string wif = original.ToWIF();
        auto restored = PrivateKey::FromWIF(wif);
        
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(original, *restored);
        EXPECT_FALSE(restored->IsCompressed());
    }
}

// ============================================================================
// Address Roundtrip Tests
// ============================================================================

TEST(AddressRoundtripTest, P2PKHMainnet) {
    for (int i = 0; i < 5; ++i) {
        Hash160 hash;
        for (size_t j = 0; j < 20; ++j) {
            hash[j] = static_cast<uint8_t>(i * 20 + j);
        }
        
        std::string address = EncodeP2PKH(hash, false);
        EXPECT_EQ(GetAddressType(address), AddressType::P2PKH);
        
        auto script = DecodeAddress(address);
        EXPECT_FALSE(script.empty());
    }
}

TEST(AddressRoundtripTest, P2WPKHMainnet) {
    for (int i = 0; i < 5; ++i) {
        Hash160 hash;
        for (size_t j = 0; j < 20; ++j) {
            hash[j] = static_cast<uint8_t>(i * 10 + j);
        }
        
        std::string address = EncodeP2WPKH(hash, false);
        EXPECT_EQ(GetAddressType(address), AddressType::P2WPKH);
        
        auto script = DecodeAddress(address);
        EXPECT_FALSE(script.empty());
    }
}

// ============================================================================
// ECDSA Signing and Verification Tests
// ============================================================================

TEST(ECDSATest, SignAndVerify) {
    // Generate a key pair
    PrivateKey privKey = PrivateKey::Generate();
    ASSERT_TRUE(privKey.IsValid());
    
    PublicKey pubKey = privKey.GetPublicKey();
    ASSERT_TRUE(pubKey.IsValid());
    
    // Create a message hash
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    // Sign the hash
    std::vector<uint8_t> signature = privKey.Sign(hash);
    EXPECT_FALSE(signature.empty()) << "Signature should not be empty";
    
    // Verify the signature
    bool verified = pubKey.Verify(hash, signature);
    EXPECT_TRUE(verified) << "Signature verification should succeed";
}

TEST(ECDSATest, SignAndVerifyMultiple) {
    // Test multiple sign/verify operations
    for (int i = 0; i < 5; ++i) {
        PrivateKey privKey = PrivateKey::Generate();
        ASSERT_TRUE(privKey.IsValid());
        
        PublicKey pubKey = privKey.GetPublicKey();
        ASSERT_TRUE(pubKey.IsValid());
        
        // Create unique message hash
        Hash256 hash;
        for (size_t j = 0; j < 32; ++j) {
            hash[j] = static_cast<uint8_t>(i * 32 + j);
        }
        
        std::vector<uint8_t> signature = privKey.Sign(hash);
        EXPECT_FALSE(signature.empty());
        
        EXPECT_TRUE(pubKey.Verify(hash, signature));
    }
}

TEST(ECDSATest, VerifyWrongMessage) {
    PrivateKey privKey = PrivateKey::Generate();
    PublicKey pubKey = privKey.GetPublicKey();
    
    Hash256 hash1, hash2;
    for (size_t i = 0; i < 32; ++i) {
        hash1[i] = static_cast<uint8_t>(i);
        hash2[i] = static_cast<uint8_t>(i + 100);
    }
    
    std::vector<uint8_t> signature = privKey.Sign(hash1);
    ASSERT_FALSE(signature.empty());
    
    // Verify against wrong hash should fail
    EXPECT_FALSE(pubKey.Verify(hash2, signature));
    
    // Verify against correct hash should succeed
    EXPECT_TRUE(pubKey.Verify(hash1, signature));
}

TEST(ECDSATest, VerifyWrongKey) {
    PrivateKey privKey1 = PrivateKey::Generate();
    PrivateKey privKey2 = PrivateKey::Generate();
    
    PublicKey pubKey1 = privKey1.GetPublicKey();
    PublicKey pubKey2 = privKey2.GetPublicKey();
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    std::vector<uint8_t> signature = privKey1.Sign(hash);
    ASSERT_FALSE(signature.empty());
    
    // Verify with correct key should succeed
    EXPECT_TRUE(pubKey1.Verify(hash, signature));
    
    // Verify with wrong key should fail
    EXPECT_FALSE(pubKey2.Verify(hash, signature));
}

TEST(ECDSATest, VerifyInvalidSignature) {
    PrivateKey privKey = PrivateKey::Generate();
    PublicKey pubKey = privKey.GetPublicKey();
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    // Empty signature should fail
    std::vector<uint8_t> emptySig;
    EXPECT_FALSE(pubKey.Verify(hash, emptySig));
    
    // Garbage signature should fail
    std::vector<uint8_t> garbageSig(72, 0xFF);
    EXPECT_FALSE(pubKey.Verify(hash, garbageSig));
}

TEST(ECDSATest, SignatureFormat) {
    PrivateKey privKey = PrivateKey::Generate();
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    std::vector<uint8_t> signature = privKey.Sign(hash);
    ASSERT_FALSE(signature.empty());
    
    // DER signature should start with 0x30 (SEQUENCE tag)
    EXPECT_EQ(signature[0], 0x30);
    
    // DER signature length should be reasonable (typically 70-72 bytes)
    EXPECT_GE(signature.size(), 68);
    EXPECT_LE(signature.size(), 73);
}

TEST(ECDSATest, CompressedVsUncompressedKey) {
    // Generate compressed key
    PrivateKey privKeyCompressed = PrivateKey::Generate(true);
    PublicKey pubKeyCompressed = privKeyCompressed.GetPublicKey();
    EXPECT_TRUE(pubKeyCompressed.IsCompressed());
    EXPECT_EQ(pubKeyCompressed.size(), 33);
    
    // Generate uncompressed key
    PrivateKey privKeyUncompressed = PrivateKey::Generate(false);
    PublicKey pubKeyUncompressed = privKeyUncompressed.GetPublicKey();
    EXPECT_FALSE(pubKeyUncompressed.IsCompressed());
    EXPECT_EQ(pubKeyUncompressed.size(), 65);
    
    // Both should be able to sign and verify
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    auto sig1 = privKeyCompressed.Sign(hash);
    auto sig2 = privKeyUncompressed.Sign(hash);
    
    EXPECT_FALSE(sig1.empty());
    EXPECT_FALSE(sig2.empty());
    
    EXPECT_TRUE(pubKeyCompressed.Verify(hash, sig1));
    EXPECT_TRUE(pubKeyUncompressed.Verify(hash, sig2));
}

TEST(ECDSATest, DecompressPublicKey) {
    // Generate a compressed key
    PrivateKey privKey = PrivateKey::Generate(true);
    PublicKey compressedKey = privKey.GetPublicKey();
    
    ASSERT_TRUE(compressedKey.IsValid());
    ASSERT_TRUE(compressedKey.IsCompressed());
    EXPECT_EQ(compressedKey.size(), 33);
    
    // Decompress it
    PublicKey uncompressedKey = compressedKey.GetUncompressed();
    
    // Should now be valid and uncompressed
    EXPECT_TRUE(uncompressedKey.IsValid()) << "Decompressed key should be valid";
    EXPECT_FALSE(uncompressedKey.IsCompressed()) << "Decompressed key should not be compressed";
    EXPECT_EQ(uncompressedKey.size(), 65) << "Uncompressed key should be 65 bytes";
    
    // Note: Hash160 of compressed vs uncompressed key WILL be different!
    // This is expected - they produce different addresses in Bitcoin.
    // The important thing is both are valid and represent the same EC point.
    EXPECT_NE(compressedKey.GetHash160(), uncompressedKey.GetHash160());
}

TEST(ECDSATest, DecompressAndVerify) {
    // Generate compressed key, sign, then verify with both forms
    PrivateKey privKey = PrivateKey::Generate(true);
    PublicKey compressedKey = privKey.GetPublicKey();
    PublicKey uncompressedKey = compressedKey.GetUncompressed();
    
    ASSERT_TRUE(uncompressedKey.IsValid());
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i * 3);
    }
    
    // Sign with compressed private key
    auto signature = privKey.Sign(hash);
    ASSERT_FALSE(signature.empty());
    
    // Both compressed and uncompressed public keys should verify
    EXPECT_TRUE(compressedKey.Verify(hash, signature));
    EXPECT_TRUE(uncompressedKey.Verify(hash, signature));
}

// ============================================================================
// BIP340 Schnorr Signature Tests
// ============================================================================

TEST(SchnorrTest, SignAndVerify) {
    // Generate a key pair
    PrivateKey privKey = PrivateKey::Generate();
    ASSERT_TRUE(privKey.IsValid());
    
    PublicKey pubKey = privKey.GetPublicKey();
    ASSERT_TRUE(pubKey.IsValid());
    
    // Create a message hash
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    // Sign the hash with Schnorr
    std::array<uint8_t, 64> signature = privKey.SignSchnorr(hash);
    
    // Check signature is not all zeros
    bool allZero = true;
    for (auto b : signature) {
        if (b != 0) {
            allZero = false;
            break;
        }
    }
    EXPECT_FALSE(allZero) << "Schnorr signature should not be all zeros";
    
    // Verify the signature
    bool verified = pubKey.VerifySchnorr(hash, signature);
    EXPECT_TRUE(verified) << "Schnorr signature verification should succeed";
}

TEST(SchnorrTest, SignAndVerifyMultiple) {
    // Test multiple sign/verify operations with different keys and messages
    for (int i = 0; i < 5; ++i) {
        PrivateKey privKey = PrivateKey::Generate();
        ASSERT_TRUE(privKey.IsValid());
        
        PublicKey pubKey = privKey.GetPublicKey();
        ASSERT_TRUE(pubKey.IsValid());
        
        // Create unique message hash
        Hash256 hash;
        for (size_t j = 0; j < 32; ++j) {
            hash[j] = static_cast<uint8_t>(i * 32 + j);
        }
        
        auto signature = privKey.SignSchnorr(hash);
        
        // Check signature is not all zeros
        bool allZero = true;
        for (auto b : signature) {
            if (b != 0) { allZero = false; break; }
        }
        EXPECT_FALSE(allZero);
        
        EXPECT_TRUE(pubKey.VerifySchnorr(hash, signature))
            << "Failed on iteration " << i;
    }
}

TEST(SchnorrTest, VerifyWrongMessage) {
    PrivateKey privKey = PrivateKey::Generate();
    PublicKey pubKey = privKey.GetPublicKey();
    
    Hash256 hash1, hash2;
    for (size_t i = 0; i < 32; ++i) {
        hash1[i] = static_cast<uint8_t>(i);
        hash2[i] = static_cast<uint8_t>(i + 100);
    }
    
    auto signature = privKey.SignSchnorr(hash1);
    
    // Verify against wrong hash should fail
    EXPECT_FALSE(pubKey.VerifySchnorr(hash2, signature))
        << "Verification with wrong message should fail";
    
    // Verify against correct hash should succeed
    EXPECT_TRUE(pubKey.VerifySchnorr(hash1, signature))
        << "Verification with correct message should succeed";
}

TEST(SchnorrTest, VerifyWrongKey) {
    PrivateKey privKey1 = PrivateKey::Generate();
    PrivateKey privKey2 = PrivateKey::Generate();
    
    PublicKey pubKey1 = privKey1.GetPublicKey();
    PublicKey pubKey2 = privKey2.GetPublicKey();
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    auto signature = privKey1.SignSchnorr(hash);
    
    // Verify with correct key should succeed
    EXPECT_TRUE(pubKey1.VerifySchnorr(hash, signature))
        << "Verification with correct key should succeed";
    
    // Verify with wrong key should fail
    EXPECT_FALSE(pubKey2.VerifySchnorr(hash, signature))
        << "Verification with wrong key should fail";
}

TEST(SchnorrTest, VerifyInvalidSignature) {
    PrivateKey privKey = PrivateKey::Generate();
    PublicKey pubKey = privKey.GetPublicKey();
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    // All zeros signature should fail
    std::array<uint8_t, 64> zeroSig{};
    EXPECT_FALSE(pubKey.VerifySchnorr(hash, zeroSig));
    
    // Garbage signature should fail
    std::array<uint8_t, 64> garbageSig;
    garbageSig.fill(0xFF);
    EXPECT_FALSE(pubKey.VerifySchnorr(hash, garbageSig));
}

TEST(SchnorrTest, SignatureFormat) {
    PrivateKey privKey = PrivateKey::Generate();
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    auto signature = privKey.SignSchnorr(hash);
    
    // BIP340 Schnorr signature is exactly 64 bytes (32 bytes R.x + 32 bytes s)
    EXPECT_EQ(signature.size(), 64);
}

TEST(SchnorrTest, DeterministicSignatures) {
    // BIP340 signatures should be deterministic
    PrivateKey privKey = PrivateKey::Generate();
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    auto sig1 = privKey.SignSchnorr(hash);
    auto sig2 = privKey.SignSchnorr(hash);
    
    // Same key + same message = same signature
    EXPECT_EQ(sig1, sig2) << "BIP340 signatures should be deterministic";
}

TEST(SchnorrTest, DifferentMessagesProduceDifferentSignatures) {
    PrivateKey privKey = PrivateKey::Generate();
    
    Hash256 hash1, hash2;
    for (size_t i = 0; i < 32; ++i) {
        hash1[i] = static_cast<uint8_t>(i);
        hash2[i] = static_cast<uint8_t>(i + 1);
    }
    
    auto sig1 = privKey.SignSchnorr(hash1);
    auto sig2 = privKey.SignSchnorr(hash2);
    
    EXPECT_NE(sig1, sig2) << "Different messages should produce different signatures";
}

TEST(SchnorrTest, GetXOnlyPublicKey) {
    PrivateKey privKey = PrivateKey::Generate();
    PublicKey pubKey = privKey.GetPublicKey();
    
    ASSERT_TRUE(pubKey.IsValid());
    
    // Get x-only public key
    auto xonly = pubKey.GetXOnly();
    
    // Should be exactly 32 bytes
    EXPECT_EQ(xonly.size(), 32);
    
    // Should not be all zeros (unless we're extremely unlucky)
    bool allZero = true;
    for (auto b : xonly) {
        if (b != 0) { allZero = false; break; }
    }
    EXPECT_FALSE(allZero) << "X-only public key should not be all zeros";
}

TEST(SchnorrTest, XOnlyFromCompressedVsUncompressed) {
    PrivateKey privKey = PrivateKey::Generate(true);
    PublicKey compressedKey = privKey.GetPublicKey();
    PublicKey uncompressedKey = compressedKey.GetUncompressed();
    
    ASSERT_TRUE(uncompressedKey.IsValid());
    
    // Both should produce the same x-only key
    auto xonly1 = compressedKey.GetXOnly();
    auto xonly2 = uncompressedKey.GetXOnly();
    
    EXPECT_EQ(xonly1, xonly2) 
        << "Compressed and uncompressed keys should have same X coordinate";
}

TEST(SchnorrTest, SignWithUncompressedKeyVerifyWorks) {
    // Schnorr signing should work with both compressed and uncompressed keys
    PrivateKey privKeyUncompressed = PrivateKey::Generate(false);
    PublicKey pubKeyUncompressed = privKeyUncompressed.GetPublicKey();
    
    ASSERT_TRUE(pubKeyUncompressed.IsValid());
    EXPECT_FALSE(pubKeyUncompressed.IsCompressed());
    
    Hash256 hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i * 2);
    }
    
    auto signature = privKeyUncompressed.SignSchnorr(hash);
    
    // Should verify with the uncompressed key
    EXPECT_TRUE(pubKeyUncompressed.VerifySchnorr(hash, signature));
    
    // Should also verify with the compressed version
    PublicKey pubKeyCompressed = pubKeyUncompressed.GetCompressed();
    EXPECT_TRUE(pubKeyCompressed.VerifySchnorr(hash, signature));
}

TEST(SchnorrTest, InvalidPublicKeyVerifyFails) {
    PublicKey invalidKey;  // Default constructor creates invalid key
    EXPECT_FALSE(invalidKey.IsValid());
    
    Hash256 hash;
    hash.SetNull();
    
    std::array<uint8_t, 64> signature{};
    
    // Verify with invalid public key should fail gracefully
    EXPECT_FALSE(invalidKey.VerifySchnorr(hash, signature));
}

TEST(SchnorrTest, InvalidPrivateKeySignFails) {
    PrivateKey invalidKey;  // Default constructor creates invalid key
    EXPECT_FALSE(invalidKey.IsValid());
    
    Hash256 hash;
    hash.SetNull();
    
    // Sign with invalid private key should return all zeros
    auto signature = invalidKey.SignSchnorr(hash);
    
    bool allZero = true;
    for (auto b : signature) {
        if (b != 0) { allZero = false; break; }
    }
    EXPECT_TRUE(allZero) << "Invalid private key should produce zero signature";
}

} // namespace
} // namespace shurium
