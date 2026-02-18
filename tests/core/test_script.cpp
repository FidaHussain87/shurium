// SHURIUM - Script Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/core/script.h"
#include "shurium/core/types.h"
#include "shurium/core/serialize.h"
#include "shurium/crypto/sha256.h"
#include <vector>

using namespace shurium;

// ============================================================================
// Opcode Tests
// ============================================================================

TEST(OpcodeTest, GetOpName) {
    EXPECT_EQ(GetOpName(OP_0), "OP_0");
    EXPECT_EQ(GetOpName(OP_1), "OP_1");
    EXPECT_EQ(GetOpName(OP_16), "OP_16");
    EXPECT_EQ(GetOpName(OP_DUP), "OP_DUP");
    EXPECT_EQ(GetOpName(OP_HASH160), "OP_HASH160");
    EXPECT_EQ(GetOpName(OP_CHECKSIG), "OP_CHECKSIG");
    EXPECT_EQ(GetOpName(OP_RETURN), "OP_RETURN");
    EXPECT_EQ(GetOpName(OP_EQUAL), "OP_EQUAL");
    EXPECT_EQ(GetOpName(OP_EQUALVERIFY), "OP_EQUALVERIFY");
}

TEST(OpcodeTest, DecodeOP_N) {
    EXPECT_EQ(Script::DecodeOP_N(OP_0), 0);
    EXPECT_EQ(Script::DecodeOP_N(OP_1), 1);
    EXPECT_EQ(Script::DecodeOP_N(OP_16), 16);
}

TEST(OpcodeTest, EncodeOP_N) {
    EXPECT_EQ(Script::EncodeOP_N(0), OP_0);
    EXPECT_EQ(Script::EncodeOP_N(1), OP_1);
    EXPECT_EQ(Script::EncodeOP_N(16), OP_16);
}

// ============================================================================
// ScriptNum Tests (Script integer encoding)
// ============================================================================

TEST(ScriptNumTest, Zero) {
    ScriptNum n(0);
    EXPECT_EQ(n.GetInt64(), 0);
    auto bytes = n.GetBytes();
    EXPECT_TRUE(bytes.empty());
}

TEST(ScriptNumTest, PositiveSmall) {
    ScriptNum n(127);
    EXPECT_EQ(n.GetInt64(), 127);
    auto bytes = n.GetBytes();
    EXPECT_EQ(bytes.size(), 1);
    EXPECT_EQ(bytes[0], 127);
}

TEST(ScriptNumTest, PositiveLarge) {
    ScriptNum n(255);
    EXPECT_EQ(n.GetInt64(), 255);
    auto bytes = n.GetBytes();
    // 255 = 0xFF needs sign byte since 0xFF has high bit set
    EXPECT_EQ(bytes.size(), 2);
    EXPECT_EQ(bytes[0], 0xFF);
    EXPECT_EQ(bytes[1], 0x00);
}

TEST(ScriptNumTest, Negative) {
    ScriptNum n(-1);
    EXPECT_EQ(n.GetInt64(), -1);
    auto bytes = n.GetBytes();
    EXPECT_EQ(bytes.size(), 1);
    EXPECT_EQ(bytes[0], 0x81);  // -1 encoded as 0x81
}

TEST(ScriptNumTest, FromBytes) {
    std::vector<uint8_t> bytes = {0x81};  // -1
    ScriptNum n(bytes);
    EXPECT_EQ(n.GetInt64(), -1);
}

TEST(ScriptNumTest, Arithmetic) {
    ScriptNum a(10);
    ScriptNum b(3);
    
    EXPECT_EQ((a + b).GetInt64(), 13);
    EXPECT_EQ((a - b).GetInt64(), 7);
    EXPECT_EQ((-a).GetInt64(), -10);
}

// ============================================================================
// Script Construction Tests
// ============================================================================

TEST(ScriptTest, DefaultConstructor) {
    Script script;
    EXPECT_TRUE(script.empty());
    EXPECT_EQ(script.size(), 0);
}

TEST(ScriptTest, PushOpcode) {
    Script script;
    script << OP_DUP << OP_HASH160;
    
    EXPECT_EQ(script.size(), 2);
    EXPECT_EQ(script[0], OP_DUP);
    EXPECT_EQ(script[1], OP_HASH160);
}

TEST(ScriptTest, PushSmallInt) {
    Script script;
    script << 0 << 1 << 16;
    
    EXPECT_EQ(script.size(), 3);
    EXPECT_EQ(script[0], OP_0);
    EXPECT_EQ(script[1], OP_1);
    EXPECT_EQ(script[2], OP_16);
}

TEST(ScriptTest, PushData) {
    Script script;
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    script << data;
    
    // Data push: 1 byte for size (4) + 4 bytes data = 5 bytes
    EXPECT_EQ(script.size(), 5);
    EXPECT_EQ(script[0], 4);  // Size
    EXPECT_EQ(script[1], 0xDE);
    EXPECT_EQ(script[2], 0xAD);
    EXPECT_EQ(script[3], 0xBE);
    EXPECT_EQ(script[4], 0xEF);
}

TEST(ScriptTest, PushLargeData) {
    Script script;
    std::vector<uint8_t> data(100, 0xAB);
    script << data;
    
    // Data > 75 bytes uses OP_PUSHDATA1: 1 + 1 + 100 = 102 bytes
    EXPECT_EQ(script.size(), 102);
    EXPECT_EQ(script[0], OP_PUSHDATA1);
    EXPECT_EQ(script[1], 100);
}

TEST(ScriptTest, PushHash160) {
    Script script;
    Hash160 hash;
    for (int i = 0; i < 20; i++) hash[i] = i;
    script << hash;
    
    // 20 bytes data: 1 byte size + 20 bytes = 21 bytes
    EXPECT_EQ(script.size(), 21);
    EXPECT_EQ(script[0], 20);
}

TEST(ScriptTest, PushHash256) {
    Script script;
    Hash256 hash;
    for (int i = 0; i < 32; i++) hash[i] = i;
    script << hash;
    
    // 32 bytes data: 1 byte size + 32 bytes = 33 bytes
    EXPECT_EQ(script.size(), 33);
    EXPECT_EQ(script[0], 32);
}

// ============================================================================
// Script Parsing Tests
// ============================================================================

TEST(ScriptTest, GetOp) {
    Script script;
    script << OP_DUP << OP_HASH160;
    
    Script::const_iterator it = script.cbegin();
    Opcode op;
    std::vector<uint8_t> data;
    
    EXPECT_TRUE(script.GetOp(it, op, data));
    EXPECT_EQ(op, OP_DUP);
    EXPECT_TRUE(data.empty());
    
    EXPECT_TRUE(script.GetOp(it, op, data));
    EXPECT_EQ(op, OP_HASH160);
    
    EXPECT_FALSE(script.GetOp(it, op, data));  // End of script
}

TEST(ScriptTest, GetOpWithData) {
    Script script;
    std::vector<uint8_t> testData = {0x01, 0x02, 0x03};
    script << testData;
    
    Script::const_iterator it = script.cbegin();
    Opcode op;
    std::vector<uint8_t> data;
    
    EXPECT_TRUE(script.GetOp(it, op, data));
    EXPECT_EQ(op, static_cast<Opcode>(3));  // Size as opcode
    EXPECT_EQ(data, testData);
}

// ============================================================================
// Standard Script Pattern Tests
// ============================================================================

TEST(ScriptTest, P2PKH) {
    // Pay-to-Public-Key-Hash: OP_DUP OP_HASH160 <20-byte hash> OP_EQUALVERIFY OP_CHECKSIG
    Hash160 pubKeyHash;
    for (int i = 0; i < 20; i++) pubKeyHash[i] = i;
    
    Script script = Script::CreateP2PKH(pubKeyHash);
    
    EXPECT_TRUE(script.IsPayToPublicKeyHash());
    EXPECT_FALSE(script.IsPayToScriptHash());
    
    Hash160 extracted;
    EXPECT_TRUE(script.ExtractPubKeyHash(extracted));
    EXPECT_EQ(extracted, pubKeyHash);
}

TEST(ScriptTest, P2SH) {
    // Pay-to-Script-Hash: OP_HASH160 <20-byte hash> OP_EQUAL
    Hash160 scriptHash;
    for (int i = 0; i < 20; i++) scriptHash[i] = i + 100;
    
    Script script = Script::CreateP2SH(scriptHash);
    
    EXPECT_FALSE(script.IsPayToPublicKeyHash());
    EXPECT_TRUE(script.IsPayToScriptHash());
    
    Hash160 extracted;
    EXPECT_TRUE(script.ExtractScriptHash(extracted));
    EXPECT_EQ(extracted, scriptHash);
}

TEST(ScriptTest, OpReturn) {
    // OP_RETURN with data (provably unspendable output for data embedding)
    std::vector<uint8_t> data = {'N', 'E', 'X', 'U', 'S'};
    Script script = Script::CreateOpReturn(data);
    
    EXPECT_TRUE(script.IsUnspendable());
    EXPECT_FALSE(script.IsPayToPublicKeyHash());
    EXPECT_FALSE(script.IsPayToScriptHash());
}

TEST(ScriptTest, IsUnspendable) {
    Script script;
    script << OP_RETURN;
    EXPECT_TRUE(script.IsUnspendable());
    
    Script script2;
    script2 << OP_DUP;
    EXPECT_FALSE(script2.IsUnspendable());
}

// ============================================================================
// Script Serialization Tests
// ============================================================================

TEST(ScriptTest, Serialization) {
    Script original;
    original << OP_DUP << OP_HASH160;
    
    Hash160 hash;
    for (int i = 0; i < 20; i++) hash[i] = i;
    original << hash;
    original << OP_EQUALVERIFY << OP_CHECKSIG;
    
    // Serialize
    DataStream ds;
    ds << original;
    
    // Deserialize
    Script restored;
    ds >> restored;
    
    EXPECT_EQ(original.size(), restored.size());
    EXPECT_EQ(original, restored);
}

TEST(ScriptTest, EmptySerialization) {
    Script original;
    
    DataStream ds;
    ds << original;
    
    Script restored;
    ds >> restored;
    
    EXPECT_TRUE(restored.empty());
}

// ============================================================================
// Script Signature Operations Count
// ============================================================================

TEST(ScriptTest, SigOpCount) {
    Script script;
    script << OP_CHECKSIG;
    EXPECT_EQ(script.GetSigOpCount(), 1);
    
    Script script2;
    script2 << OP_CHECKSIG << OP_CHECKSIGVERIFY << OP_CHECKSIG;
    EXPECT_EQ(script2.GetSigOpCount(), 3);
}

TEST(ScriptTest, MultiSigOpCount) {
    Script script;
    script << OP_2 << OP_CHECKMULTISIG;
    // Without accurate counting, CHECKMULTISIG counts as 20
    EXPECT_EQ(script.GetSigOpCount(false), 20);
    // With accurate counting, should count the actual keys
    EXPECT_EQ(script.GetSigOpCount(true), 2);
}

// ============================================================================
// IsPushOnly Tests
// ============================================================================

TEST(ScriptTest, IsPushOnly) {
    Script script;
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    script << data << 5 << 16;
    
    EXPECT_TRUE(script.IsPushOnly());
}

TEST(ScriptTest, NotPushOnly) {
    Script script;
    script << OP_DUP << OP_HASH160;
    
    EXPECT_FALSE(script.IsPushOnly());
}

// ============================================================================
// HasValidOps Tests
// ============================================================================

TEST(ScriptTest, HasValidOps) {
    Script script;
    script << OP_DUP << OP_HASH160 << OP_EQUALVERIFY << OP_CHECKSIG;
    EXPECT_TRUE(script.HasValidOps());
}

// ============================================================================
// Script Comparison Tests
// ============================================================================

TEST(ScriptTest, Equality) {
    Script s1, s2;
    s1 << OP_DUP << OP_HASH160;
    s2 << OP_DUP << OP_HASH160;
    
    EXPECT_EQ(s1, s2);
}

TEST(ScriptTest, Inequality) {
    Script s1, s2;
    s1 << OP_DUP;
    s2 << OP_HASH160;
    
    EXPECT_NE(s1, s2);
}

// ============================================================================
// Clear and Size Tests
// ============================================================================

TEST(ScriptTest, Clear) {
    Script script;
    script << OP_DUP << OP_HASH160;
    EXPECT_FALSE(script.empty());
    
    script.clear();
    EXPECT_TRUE(script.empty());
    EXPECT_EQ(script.size(), 0);
}

// ============================================================================
// Script to String
// ============================================================================

TEST(ScriptTest, ToString) {
    Script script;
    script << OP_DUP << OP_HASH160;
    
    std::string str = script.ToString();
    EXPECT_TRUE(str.find("OP_DUP") != std::string::npos);
    EXPECT_TRUE(str.find("OP_HASH160") != std::string::npos);
}
