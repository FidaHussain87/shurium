// SHURIUM - Script Interpreter Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/script/interpreter.h"
#include "shurium/core/script.h"
#include "shurium/core/transaction.h"
#include "shurium/crypto/keys.h"
#include "shurium/crypto/sha256.h"
#include "shurium/crypto/ripemd160.h"
#include <vector>

using namespace shurium;

// ============================================================================
// Helper Functions
// ============================================================================

// Create a simple test transaction
static MutableTransaction CreateTestTransaction() {
    MutableTransaction tx;
    tx.version = 2;
    tx.nLockTime = 0;
    
    // Add a dummy input
    tx.vin.emplace_back(OutPoint(TxHash(), 0));
    
    // Add a dummy output
    tx.vout.emplace_back(Amount(1000), Script());
    
    return tx;
}

// ============================================================================
// CastToBool Tests
// ============================================================================

TEST(InterpreterTest, CastToBool) {
    // Empty vector is false
    EXPECT_FALSE(CastToBool(std::vector<uint8_t>{}));
    
    // Zero bytes are false
    EXPECT_FALSE(CastToBool(std::vector<uint8_t>{0x00}));
    EXPECT_FALSE(CastToBool(std::vector<uint8_t>{0x00, 0x00}));
    
    // Negative zero (0x80) is false
    EXPECT_FALSE(CastToBool(std::vector<uint8_t>{0x80}));
    
    // Non-zero is true
    EXPECT_TRUE(CastToBool(std::vector<uint8_t>{0x01}));
    EXPECT_TRUE(CastToBool(std::vector<uint8_t>{0xFF}));
    EXPECT_TRUE(CastToBool(std::vector<uint8_t>{0x00, 0x01}));
}

// ============================================================================
// ScriptError Tests
// ============================================================================

TEST(InterpreterTest, ScriptErrorString) {
    EXPECT_EQ(ScriptErrorString(ScriptError::OK), "No error");
    EXPECT_EQ(ScriptErrorString(ScriptError::EVAL_FALSE), 
              "Script evaluated without error but finished with a false/empty top stack element");
    EXPECT_EQ(ScriptErrorString(ScriptError::OP_RETURN), "OP_RETURN was encountered");
    EXPECT_EQ(ScriptErrorString(ScriptError::BAD_OPCODE), "Bad opcode");
}

// ============================================================================
// Basic Script Evaluation Tests
// ============================================================================

TEST(InterpreterTest, EvalEmptyScript) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_TRUE(result);
    EXPECT_EQ(error, ScriptError::OK);
    EXPECT_TRUE(stack.empty());
}

TEST(InterpreterTest, EvalPushData) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF};
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack[0], (std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}));
}

TEST(InterpreterTest, EvalOP_1Through16) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_2 << OP_16;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 3);
    
    // OP_1 pushes 0x01, OP_2 pushes 0x02, etc.
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 1);
    EXPECT_EQ(ScriptNum(stack[1], false).GetInt64(), 2);
    EXPECT_EQ(ScriptNum(stack[2], false).GetInt64(), 16);
}

TEST(InterpreterTest, EvalOP_0) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_0;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_TRUE(stack[0].empty());  // OP_0 pushes empty vector
}

// ============================================================================
// Stack Operation Tests
// ============================================================================

TEST(InterpreterTest, EvalOP_DUP) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_DUP;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack[0], stack[1]);
}

TEST(InterpreterTest, EvalOP_DROP) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_2 << OP_DROP;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 1);
}

TEST(InterpreterTest, EvalOP_SWAP) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_2 << OP_SWAP;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 2);
    EXPECT_EQ(ScriptNum(stack[1], false).GetInt64(), 1);
}

TEST(InterpreterTest, EvalOP_OVER) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_2 << OP_OVER;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(ScriptNum(stack[2], false).GetInt64(), 1);  // Copy of first element
}

TEST(InterpreterTest, EvalOP_ROT) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_2 << OP_3 << OP_ROT;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 3);
    // ROT: [1,2,3] -> [2,3,1]
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 2);
    EXPECT_EQ(ScriptNum(stack[1], false).GetInt64(), 3);
    EXPECT_EQ(ScriptNum(stack[2], false).GetInt64(), 1);
}

TEST(InterpreterTest, EvalOP_DEPTH) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_2 << OP_3 << OP_DEPTH;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 4);
    EXPECT_EQ(ScriptNum(stack[3], false).GetInt64(), 3);  // Depth was 3 before DEPTH
}

// ============================================================================
// Arithmetic Operation Tests
// ============================================================================

TEST(InterpreterTest, EvalOP_ADD) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_3 << OP_ADD;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 8);
}

TEST(InterpreterTest, EvalOP_SUB) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_3 << OP_SUB;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 2);
}

TEST(InterpreterTest, EvalOP_1ADD) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_1ADD;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 6);
}

TEST(InterpreterTest, EvalOP_NEGATE) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_NEGATE;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), -5);
}

TEST(InterpreterTest, EvalOP_ABS) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_NEGATE << OP_ABS;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 5);
}

// ============================================================================
// Comparison Operation Tests  
// ============================================================================

TEST(InterpreterTest, EvalOP_EQUAL) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_5 << OP_EQUAL;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_TRUE(CastToBool(stack[0]));  // True
}

TEST(InterpreterTest, EvalOP_EQUAL_False) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_3 << OP_EQUAL;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_FALSE(CastToBool(stack[0]));  // False
}

TEST(InterpreterTest, EvalOP_LESSTHAN) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_3 << OP_5 << OP_LESSTHAN;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_TRUE(CastToBool(stack[0]));  // 3 < 5 is true
}

// ============================================================================
// Control Flow Tests
// ============================================================================

TEST(InterpreterTest, EvalOP_IF_True) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_IF << OP_2 << OP_ENDIF;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 2);
}

TEST(InterpreterTest, EvalOP_IF_False) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_0 << OP_IF << OP_2 << OP_ENDIF;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_TRUE(stack.empty());  // OP_2 was not executed
}

TEST(InterpreterTest, EvalOP_IF_ELSE) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_0 << OP_IF << OP_2 << OP_ELSE << OP_3 << OP_ENDIF;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 3);  // ELSE branch executed
}

TEST(InterpreterTest, EvalOP_NOTIF) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_0 << OP_NOTIF << OP_2 << OP_ENDIF;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(ScriptNum(stack[0], false).GetInt64(), 2);
}

// ============================================================================
// OP_VERIFY Tests
// ============================================================================

TEST(InterpreterTest, EvalOP_VERIFY_Success) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_VERIFY;
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_TRUE(result);
    EXPECT_TRUE(stack.empty());  // VERIFY consumes the stack element
}

TEST(InterpreterTest, EvalOP_VERIFY_Fail) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_0 << OP_VERIFY;
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_FALSE(result);
    EXPECT_EQ(error, ScriptError::VERIFY);
}

TEST(InterpreterTest, EvalOP_EQUALVERIFY_Success) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_5 << OP_EQUALVERIFY << OP_1;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
}

TEST(InterpreterTest, EvalOP_EQUALVERIFY_Fail) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_5 << OP_3 << OP_EQUALVERIFY;
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_FALSE(result);
    EXPECT_EQ(error, ScriptError::EQUALVERIFY);
}

// ============================================================================
// Hash Operation Tests
// ============================================================================

TEST(InterpreterTest, EvalOP_SHA256) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << std::vector<uint8_t>{'t', 'e', 's', 't'} << OP_SHA256;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack[0].size(), 32);  // SHA256 produces 32 bytes
    
    // Verify against known hash
    Hash256 expected = SHA256Hash(reinterpret_cast<const uint8_t*>("test"), 4);
    EXPECT_EQ(stack[0], std::vector<uint8_t>(expected.begin(), expected.end()));
}

TEST(InterpreterTest, EvalOP_HASH256) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << std::vector<uint8_t>{'t', 'e', 's', 't'} << OP_HASH256;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack[0].size(), 32);  // Double SHA256 produces 32 bytes
}

TEST(InterpreterTest, EvalOP_HASH160) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << std::vector<uint8_t>{'t', 'e', 's', 't'} << OP_HASH160;
    
    DummySignatureChecker checker;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker);
    EXPECT_TRUE(result);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack[0].size(), 20);  // HASH160 produces 20 bytes
}

// ============================================================================
// OP_RETURN Test
// ============================================================================

TEST(InterpreterTest, EvalOP_RETURN) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_RETURN;
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_FALSE(result);
    EXPECT_EQ(error, ScriptError::OP_RETURN);
}

// ============================================================================
// Error Condition Tests
// ============================================================================

TEST(InterpreterTest, StackUnderflow) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_DUP;  // DUP with empty stack
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_FALSE(result);
    EXPECT_EQ(error, ScriptError::INVALID_STACK_OPERATION);
}

TEST(InterpreterTest, UnbalancedConditional) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_IF << OP_2;  // Missing ENDIF
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_FALSE(result);
    EXPECT_EQ(error, ScriptError::UNBALANCED_CONDITIONAL);
}

TEST(InterpreterTest, DisabledOpcode) {
    std::vector<std::vector<uint8_t>> stack;
    Script script;
    script << OP_1 << OP_2 << OP_CAT;  // CAT is disabled
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = EvalScript(stack, script, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_FALSE(result);
    EXPECT_EQ(error, ScriptError::DISABLED_OPCODE);
}

// ============================================================================
// VerifyScript Tests
// ============================================================================

TEST(InterpreterTest, VerifyScript_SimpleTrue) {
    Script scriptSig;
    scriptSig << OP_1;
    
    Script scriptPubKey;
    // Empty scriptPubKey that just leaves stack as-is
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = VerifyScript(scriptSig, scriptPubKey, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_TRUE(result);
}

TEST(InterpreterTest, VerifyScript_SimpleFalse) {
    Script scriptSig;
    scriptSig << OP_0;
    
    Script scriptPubKey;
    // Empty scriptPubKey
    
    DummySignatureChecker checker;
    ScriptError error;
    
    bool result = VerifyScript(scriptSig, scriptPubKey, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_FALSE(result);
    EXPECT_EQ(error, ScriptError::EVAL_FALSE);
}

TEST(InterpreterTest, VerifyScript_P2PKH_Pattern) {
    // This tests the P2PKH pattern without actual signature verification
    // Since DummySignatureChecker returns false for CheckSig
    
    // Create a P2PKH scriptPubKey
    Hash160 pubKeyHash;
    for (int i = 0; i < 20; i++) pubKeyHash[i] = i;
    
    Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
    
    // Create a matching scriptSig that would push the same pubkeyhash
    // Note: This won't actually verify because we're using a dummy checker
    // and would need real keys, but we can verify the pattern recognition
    
    EXPECT_TRUE(scriptPubKey.IsPayToPublicKeyHash());
}

// ============================================================================
// Signature Verification Tests (with real keys)
// ============================================================================

TEST(InterpreterTest, CheckSig_WithRealKeys) {
    // Generate a key pair
    KeyPair keyPair = KeyPair::Generate(true);
    const PublicKey& pubKey = keyPair.GetPublicKey();
    const PrivateKey& privKey = keyPair.GetPrivateKey();
    
    // Create a simple transaction
    MutableTransaction mtx = CreateTestTransaction();
    
    // Create scriptPubKey: P2PKH
    Hash160 pubKeyHash = pubKey.GetHash160();
    Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
    
    // Set up the transaction's output
    mtx.vout[0].scriptPubKey = scriptPubKey;
    
    // Create immutable transaction for signing
    Transaction tx(mtx);
    
    // Create signature
    Hash256 sighash = SignatureHash(tx, 0, scriptPubKey, SIGHASH_ALL);
    std::vector<uint8_t> signature = privKey.Sign(sighash);
    signature.push_back(SIGHASH_ALL);  // Append hash type
    
    // Create scriptSig
    Script scriptSig;
    scriptSig << signature;
    scriptSig << pubKey.ToVector();
    
    // Now verify the script
    TransactionSignatureChecker checker(&tx, 0, 1000);
    ScriptError error;
    
    bool result = VerifyScript(scriptSig, scriptPubKey, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_TRUE(result) << "Script verification failed with error: " << ScriptErrorString(error);
}

TEST(InterpreterTest, CheckSig_WrongKey) {
    // Generate two key pairs
    KeyPair keyPair1 = KeyPair::Generate(true);
    KeyPair keyPair2 = KeyPair::Generate(true);
    
    const PublicKey& pubKey1 = keyPair1.GetPublicKey();
    const PrivateKey& privKey2 = keyPair2.GetPrivateKey();  // Use different key
    
    // Create transaction
    MutableTransaction mtx = CreateTestTransaction();
    
    // Create scriptPubKey for pubKey1
    Hash160 pubKeyHash = pubKey1.GetHash160();
    Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
    mtx.vout[0].scriptPubKey = scriptPubKey;
    
    Transaction tx(mtx);
    
    // Create signature with wrong key
    Hash256 sighash = SignatureHash(tx, 0, scriptPubKey, SIGHASH_ALL);
    std::vector<uint8_t> signature = privKey2.Sign(sighash);  // Wrong key!
    signature.push_back(SIGHASH_ALL);
    
    // Create scriptSig with pubKey1 but signature from privKey2
    Script scriptSig;
    scriptSig << signature;
    scriptSig << pubKey1.ToVector();
    
    // Verification should fail
    TransactionSignatureChecker checker(&tx, 0, 1000);
    ScriptError error;
    
    bool result = VerifyScript(scriptSig, scriptPubKey, ScriptFlags::VERIFY_NONE, checker, &error);
    EXPECT_FALSE(result);
}

// ============================================================================
// IsValidPubKey Tests
// ============================================================================

TEST(InterpreterTest, IsValidPubKey) {
    // Valid compressed pubkey (02 or 03 prefix, 33 bytes)
    std::vector<uint8_t> compressed(33, 0);
    compressed[0] = 0x02;
    EXPECT_TRUE(IsValidPubKey(compressed));
    
    compressed[0] = 0x03;
    EXPECT_TRUE(IsValidPubKey(compressed));
    
    // Valid uncompressed pubkey (04 prefix, 65 bytes)
    std::vector<uint8_t> uncompressed(65, 0);
    uncompressed[0] = 0x04;
    EXPECT_TRUE(IsValidPubKey(uncompressed));
    
    // Invalid: wrong size
    std::vector<uint8_t> invalid(20, 0);
    invalid[0] = 0x02;
    EXPECT_FALSE(IsValidPubKey(invalid));
    
    // Invalid: wrong prefix
    std::vector<uint8_t> wrongPrefix(33, 0);
    wrongPrefix[0] = 0x01;
    EXPECT_FALSE(IsValidPubKey(wrongPrefix));
}

TEST(InterpreterTest, IsCompressedPubKey) {
    std::vector<uint8_t> compressed(33, 0);
    compressed[0] = 0x02;
    EXPECT_TRUE(IsCompressedPubKey(compressed));
    
    compressed[0] = 0x03;
    EXPECT_TRUE(IsCompressedPubKey(compressed));
    
    std::vector<uint8_t> uncompressed(65, 0);
    uncompressed[0] = 0x04;
    EXPECT_FALSE(IsCompressedPubKey(uncompressed));
}
