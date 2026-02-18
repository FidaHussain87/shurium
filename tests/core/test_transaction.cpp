// SHURIUM - Transaction Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/core/transaction.h"
#include "shurium/core/serialize.h"
#include "shurium/crypto/sha256.h"

using namespace shurium;

// ============================================================================
// OutPoint Tests
// ============================================================================

class OutPointTest : public ::testing::Test {
protected:
    TxHash testHash;
    
    void SetUp() override {
        // Create a known test hash
        std::array<uint8_t, 32> hashBytes;
        for (size_t i = 0; i < 32; ++i) {
            hashBytes[i] = static_cast<uint8_t>(i + 1);
        }
        testHash = TxHash(hashBytes);
    }
};

TEST_F(OutPointTest, DefaultConstructor) {
    OutPoint op;
    EXPECT_TRUE(op.IsNull());
    EXPECT_TRUE(op.hash.IsNull());
    EXPECT_EQ(op.n, OutPoint::NULL_INDEX);
}

TEST_F(OutPointTest, ParameterizedConstructor) {
    OutPoint op(testHash, 5);
    EXPECT_FALSE(op.IsNull());
    EXPECT_EQ(op.hash, testHash);
    EXPECT_EQ(op.n, 5u);
}

TEST_F(OutPointTest, SetNull) {
    OutPoint op(testHash, 5);
    EXPECT_FALSE(op.IsNull());
    op.SetNull();
    EXPECT_TRUE(op.IsNull());
}

TEST_F(OutPointTest, Equality) {
    OutPoint op1(testHash, 5);
    OutPoint op2(testHash, 5);
    OutPoint op3(testHash, 6);
    
    EXPECT_EQ(op1, op2);
    EXPECT_NE(op1, op3);
}

TEST_F(OutPointTest, LessThan) {
    TxHash hash1, hash2;
    hash1[0] = 1;
    hash2[0] = 2;
    
    OutPoint op1(hash1, 5);
    OutPoint op2(hash2, 5);
    OutPoint op3(hash1, 6);
    
    EXPECT_TRUE(op1 < op2);  // Different hash
    EXPECT_TRUE(op1 < op3);  // Same hash, different index
}

TEST_F(OutPointTest, Serialization) {
    OutPoint op(testHash, 42);
    
    DataStream ss;
    ss << op;
    
    OutPoint deserialized;
    ss >> deserialized;
    
    EXPECT_EQ(op, deserialized);
}

TEST_F(OutPointTest, ToString) {
    OutPoint op(testHash, 5);
    std::string str = op.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("5"), std::string::npos);  // Should contain index
}

// ============================================================================
// TxIn Tests
// ============================================================================

class TxInTest : public ::testing::Test {
protected:
    OutPoint testOutpoint;
    Script testScriptSig;
    
    void SetUp() override {
        TxHash hash;
        hash[0] = 0xAB;
        hash[1] = 0xCD;
        testOutpoint = OutPoint(hash, 0);
        
        // Create a simple scriptSig (push some data)
        std::vector<uint8_t> sig = {0x30, 0x45, 0x02, 0x21};  // DER signature prefix
        testScriptSig << sig;
    }
};

TEST_F(TxInTest, DefaultConstructor) {
    TxIn txin;
    EXPECT_TRUE(txin.prevout.IsNull());
    EXPECT_TRUE(txin.scriptSig.empty());
    EXPECT_EQ(txin.nSequence, TxIn::SEQUENCE_FINAL);
}

TEST_F(TxInTest, ConstructWithOutpoint) {
    TxIn txin(testOutpoint);
    EXPECT_EQ(txin.prevout, testOutpoint);
    EXPECT_TRUE(txin.scriptSig.empty());
    EXPECT_EQ(txin.nSequence, TxIn::SEQUENCE_FINAL);
}

TEST_F(TxInTest, ConstructWithScript) {
    TxIn txin(testOutpoint, testScriptSig);
    EXPECT_EQ(txin.prevout, testOutpoint);
    EXPECT_EQ(txin.scriptSig, testScriptSig);
    EXPECT_EQ(txin.nSequence, TxIn::SEQUENCE_FINAL);
}

TEST_F(TxInTest, ConstructWithSequence) {
    TxIn txin(testOutpoint, testScriptSig, 0x12345678);
    EXPECT_EQ(txin.prevout, testOutpoint);
    EXPECT_EQ(txin.scriptSig, testScriptSig);
    EXPECT_EQ(txin.nSequence, 0x12345678u);
}

TEST_F(TxInTest, Equality) {
    TxIn txin1(testOutpoint, testScriptSig, 100);
    TxIn txin2(testOutpoint, testScriptSig, 100);
    TxIn txin3(testOutpoint, testScriptSig, 200);
    
    EXPECT_EQ(txin1, txin2);
    EXPECT_NE(txin1, txin3);
}

TEST_F(TxInTest, Serialization) {
    TxIn txin(testOutpoint, testScriptSig, 0xABCDEF01);
    
    DataStream ss;
    ss << txin;
    
    TxIn deserialized;
    ss >> deserialized;
    
    EXPECT_EQ(txin, deserialized);
}

TEST_F(TxInTest, SequenceConstants) {
    EXPECT_EQ(TxIn::SEQUENCE_FINAL, 0xFFFFFFFFu);
    EXPECT_EQ(TxIn::MAX_SEQUENCE_NONFINAL, 0xFFFFFFFEu);
    EXPECT_EQ(TxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG, 0x80000000u);
}

TEST_F(TxInTest, ToString) {
    TxIn txin(testOutpoint, testScriptSig, 0xFFFFFFFF);
    std::string str = txin.ToString();
    EXPECT_FALSE(str.empty());
}

// ============================================================================
// TxOut Tests
// ============================================================================

class TxOutTest : public ::testing::Test {
protected:
    Script testScriptPubKey;
    
    void SetUp() override {
        // Create a P2PKH scriptPubKey
        Hash160 pubKeyHash;
        pubKeyHash[0] = 0x12;
        pubKeyHash[1] = 0x34;
        testScriptPubKey = Script::CreateP2PKH(pubKeyHash);
    }
};

TEST_F(TxOutTest, DefaultConstructor) {
    TxOut txout;
    EXPECT_TRUE(txout.IsNull());
    EXPECT_EQ(txout.nValue, -1);
    EXPECT_TRUE(txout.scriptPubKey.empty());
}

TEST_F(TxOutTest, ParameterizedConstructor) {
    TxOut txout(50 * COIN, testScriptPubKey);
    EXPECT_FALSE(txout.IsNull());
    EXPECT_EQ(txout.nValue, 50 * COIN);
    EXPECT_EQ(txout.scriptPubKey, testScriptPubKey);
}

TEST_F(TxOutTest, SetNull) {
    TxOut txout(50 * COIN, testScriptPubKey);
    EXPECT_FALSE(txout.IsNull());
    txout.SetNull();
    EXPECT_TRUE(txout.IsNull());
}

TEST_F(TxOutTest, Equality) {
    TxOut txout1(50 * COIN, testScriptPubKey);
    TxOut txout2(50 * COIN, testScriptPubKey);
    TxOut txout3(100 * COIN, testScriptPubKey);
    
    EXPECT_EQ(txout1, txout2);
    EXPECT_NE(txout1, txout3);
}

TEST_F(TxOutTest, Serialization) {
    TxOut txout(123456789LL, testScriptPubKey);
    
    DataStream ss;
    ss << txout;
    
    TxOut deserialized;
    ss >> deserialized;
    
    EXPECT_EQ(txout, deserialized);
}

TEST_F(TxOutTest, ZeroValue) {
    TxOut txout(0, testScriptPubKey);
    EXPECT_FALSE(txout.IsNull());
    EXPECT_EQ(txout.nValue, 0);
}

TEST_F(TxOutTest, ToString) {
    TxOut txout(50 * COIN, testScriptPubKey);
    std::string str = txout.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("50"), std::string::npos);  // Should show amount
}

// ============================================================================
// MutableTransaction Tests
// ============================================================================

class MutableTransactionTest : public ::testing::Test {
protected:
    TxIn testInput;
    TxOut testOutput;
    
    void SetUp() override {
        // Create test input
        TxHash prevHash;
        prevHash[0] = 0xAA;
        OutPoint prevOut(prevHash, 0);
        testInput = TxIn(prevOut);
        
        // Create test output
        Hash160 pubKeyHash;
        pubKeyHash[0] = 0x12;
        Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
        testOutput = TxOut(50 * COIN, scriptPubKey);
    }
};

TEST_F(MutableTransactionTest, DefaultConstructor) {
    MutableTransaction tx;
    EXPECT_EQ(tx.version, MutableTransaction::CURRENT_VERSION);
    EXPECT_TRUE(tx.vin.empty());
    EXPECT_TRUE(tx.vout.empty());
    EXPECT_EQ(tx.nLockTime, 0u);
}

TEST_F(MutableTransactionTest, IsNull) {
    MutableTransaction tx;
    EXPECT_TRUE(tx.IsNull());
    
    tx.vin.push_back(testInput);
    EXPECT_FALSE(tx.IsNull());
}

TEST_F(MutableTransactionTest, AddInputsAndOutputs) {
    MutableTransaction tx;
    tx.vin.push_back(testInput);
    tx.vout.push_back(testOutput);
    
    EXPECT_EQ(tx.vin.size(), 1u);
    EXPECT_EQ(tx.vout.size(), 1u);
}

TEST_F(MutableTransactionTest, GetHash) {
    MutableTransaction tx;
    tx.vin.push_back(testInput);
    tx.vout.push_back(testOutput);
    
    TxHash hash = tx.GetHash();
    EXPECT_FALSE(hash.IsNull());
    
    // Same transaction should produce same hash
    TxHash hash2 = tx.GetHash();
    EXPECT_EQ(hash, hash2);
}

TEST_F(MutableTransactionTest, DifferentTransactionsDifferentHashes) {
    MutableTransaction tx1;
    tx1.vin.push_back(testInput);
    tx1.vout.push_back(testOutput);
    
    MutableTransaction tx2;
    tx2.vin.push_back(testInput);
    TxOut differentOutput(100 * COIN, testOutput.scriptPubKey);
    tx2.vout.push_back(differentOutput);
    
    EXPECT_NE(tx1.GetHash(), tx2.GetHash());
}

TEST_F(MutableTransactionTest, Serialization) {
    MutableTransaction tx;
    tx.version = 2;
    tx.vin.push_back(testInput);
    tx.vout.push_back(testOutput);
    tx.nLockTime = 500000;
    
    DataStream ss;
    ss << tx;
    
    MutableTransaction deserialized;
    ss >> deserialized;
    
    EXPECT_EQ(tx.version, deserialized.version);
    EXPECT_EQ(tx.nLockTime, deserialized.nLockTime);
    EXPECT_EQ(tx.vin.size(), deserialized.vin.size());
    EXPECT_EQ(tx.vout.size(), deserialized.vout.size());
    EXPECT_EQ(tx.GetHash(), deserialized.GetHash());
}

TEST_F(MutableTransactionTest, GetValueOut) {
    MutableTransaction tx;
    tx.vout.push_back(TxOut(50 * COIN, testOutput.scriptPubKey));
    tx.vout.push_back(TxOut(30 * COIN, testOutput.scriptPubKey));
    tx.vout.push_back(TxOut(20 * COIN, testOutput.scriptPubKey));
    
    EXPECT_EQ(tx.GetValueOut(), 100 * COIN);
}

TEST_F(MutableTransactionTest, GetTotalSize) {
    MutableTransaction tx;
    tx.vin.push_back(testInput);
    tx.vout.push_back(testOutput);
    
    size_t size = tx.GetTotalSize();
    EXPECT_GT(size, 0u);
    
    // Size should match serialized size
    DataStream ss;
    ss << tx;
    EXPECT_EQ(size, ss.TotalSize());
}

// ============================================================================
// Transaction (Immutable) Tests
// ============================================================================

class TransactionTest : public ::testing::Test {
protected:
    MutableTransaction mutableTx;
    
    void SetUp() override {
        // Create a complete transaction
        TxHash prevHash;
        prevHash[0] = 0xAA;
        OutPoint prevOut(prevHash, 0);
        TxIn input(prevOut);
        
        Hash160 pubKeyHash;
        pubKeyHash[0] = 0x12;
        Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
        TxOut output(50 * COIN, scriptPubKey);
        
        mutableTx.version = 2;
        mutableTx.vin.push_back(input);
        mutableTx.vout.push_back(output);
        mutableTx.nLockTime = 0;
    }
};

TEST_F(TransactionTest, ConstructFromMutable) {
    Transaction tx(mutableTx);
    
    EXPECT_EQ(tx.version, mutableTx.version);
    EXPECT_EQ(tx.nLockTime, mutableTx.nLockTime);
    EXPECT_EQ(tx.vin.size(), mutableTx.vin.size());
    EXPECT_EQ(tx.vout.size(), mutableTx.vout.size());
}

TEST_F(TransactionTest, GetHash) {
    Transaction tx(mutableTx);
    
    // Hash should be cached and consistent
    const TxHash& hash1 = tx.GetHash();
    const TxHash& hash2 = tx.GetHash();
    EXPECT_EQ(hash1, hash2);
    EXPECT_FALSE(hash1.IsNull());
    
    // Should match mutable transaction hash
    EXPECT_EQ(hash1, mutableTx.GetHash());
}

TEST_F(TransactionTest, IsCoinBase) {
    // Regular transaction
    Transaction tx(mutableTx);
    EXPECT_FALSE(tx.IsCoinBase());
    
    // Coinbase transaction
    MutableTransaction coinbaseTx;
    OutPoint nullOutpoint;  // Null outpoint
    TxIn coinbaseInput(nullOutpoint);
    coinbaseTx.vin.push_back(coinbaseInput);
    coinbaseTx.vout.push_back(mutableTx.vout[0]);
    
    Transaction coinbase(coinbaseTx);
    EXPECT_TRUE(coinbase.IsCoinBase());
}

TEST_F(TransactionTest, GetValueOut) {
    Transaction tx(mutableTx);
    EXPECT_EQ(tx.GetValueOut(), 50 * COIN);
}

TEST_F(TransactionTest, GetTotalSize) {
    Transaction tx(mutableTx);
    
    size_t size = tx.GetTotalSize();
    EXPECT_GT(size, 0u);
}

TEST_F(TransactionTest, Equality) {
    Transaction tx1(mutableTx);
    Transaction tx2(mutableTx);
    
    EXPECT_EQ(tx1, tx2);
    
    // Different transaction
    MutableTransaction differentTx = mutableTx;
    differentTx.nLockTime = 12345;
    Transaction tx3(differentTx);
    
    EXPECT_NE(tx1, tx3);
}

TEST_F(TransactionTest, ToString) {
    Transaction tx(mutableTx);
    std::string str = tx.ToString();
    EXPECT_FALSE(str.empty());
}

TEST_F(TransactionTest, Serialization) {
    Transaction tx(mutableTx);
    
    DataStream ss;
    ss << tx;
    
    // Deserialize back
    MutableTransaction deserialized;
    ss >> deserialized;
    
    EXPECT_EQ(tx.GetHash(), deserialized.GetHash());
}

// ============================================================================
// Coinbase Transaction Tests
// ============================================================================

class CoinbaseTest : public ::testing::Test {};

TEST_F(CoinbaseTest, CreateCoinbase) {
    MutableTransaction coinbaseTx;
    coinbaseTx.version = 2;
    
    // Coinbase input: null outpoint
    OutPoint nullOutpoint;
    EXPECT_TRUE(nullOutpoint.IsNull());
    
    // Coinbase script (can contain arbitrary data up to 100 bytes)
    Script coinbaseScript;
    coinbaseScript << std::vector<uint8_t>{0x04, 0xFF, 0xFF, 0x00, 0x1D};  // Block height
    coinbaseScript << std::vector<uint8_t>{'N', 'E', 'X', 'U', 'S'};  // Extra nonce/message
    
    TxIn coinbaseInput(nullOutpoint, coinbaseScript);
    coinbaseTx.vin.push_back(coinbaseInput);
    
    // Block reward output
    Hash160 minerPubKeyHash;
    minerPubKeyHash[0] = 0xAA;
    Script minerScript = Script::CreateP2PKH(minerPubKeyHash);
    TxOut rewardOutput(50 * COIN, minerScript);
    coinbaseTx.vout.push_back(rewardOutput);
    
    Transaction coinbase(coinbaseTx);
    EXPECT_TRUE(coinbase.IsCoinBase());
    EXPECT_EQ(coinbase.GetValueOut(), 50 * COIN);
}

TEST_F(CoinbaseTest, NonCoinbaseWithMultipleInputs) {
    MutableTransaction tx;
    
    // Add two inputs - cannot be coinbase
    OutPoint nullOutpoint;
    TxIn input1(nullOutpoint);
    TxIn input2(nullOutpoint);
    tx.vin.push_back(input1);
    tx.vin.push_back(input2);
    
    Hash160 pubKeyHash;
    Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
    tx.vout.push_back(TxOut(50 * COIN, scriptPubKey));
    
    Transaction transaction(tx);
    EXPECT_FALSE(transaction.IsCoinBase());  // Multiple inputs
}

// ============================================================================
// Transaction Validation Tests (Basic)
// ============================================================================

class TransactionValidationTest : public ::testing::Test {
protected:
    MutableTransaction validTx;
    
    void SetUp() override {
        TxHash prevHash;
        prevHash[0] = 0xAA;
        OutPoint prevOut(prevHash, 0);
        TxIn input(prevOut);
        
        Hash160 pubKeyHash;
        Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
        TxOut output(50 * COIN, scriptPubKey);
        
        validTx.vin.push_back(input);
        validTx.vout.push_back(output);
    }
};

TEST_F(TransactionValidationTest, EmptyInputsInvalid) {
    MutableTransaction tx;
    tx.vout.push_back(validTx.vout[0]);
    
    // Should be considered invalid (empty inputs for non-coinbase)
    EXPECT_TRUE(tx.vin.empty());
}

TEST_F(TransactionValidationTest, EmptyOutputsInvalid) {
    MutableTransaction tx;
    tx.vin.push_back(validTx.vin[0]);
    
    // Should be considered invalid (empty outputs)
    EXPECT_TRUE(tx.vout.empty());
}

TEST_F(TransactionValidationTest, NegativeOutputInvalid) {
    MutableTransaction tx = validTx;
    tx.vout[0].nValue = -1;
    
    // Negative values should be caught by MoneyRange
    EXPECT_FALSE(MoneyRange(tx.vout[0].nValue));
}

TEST_F(TransactionValidationTest, OverflowOutputInvalid) {
    MutableTransaction tx = validTx;
    tx.vout[0].nValue = MAX_MONEY + 1;
    
    EXPECT_FALSE(MoneyRange(tx.vout[0].nValue));
}

TEST_F(TransactionValidationTest, ValidOutput) {
    EXPECT_TRUE(MoneyRange(validTx.vout[0].nValue));
}

// ============================================================================
// Multiple Input/Output Tests
// ============================================================================

class MultipleIOTest : public ::testing::Test {};

TEST_F(MultipleIOTest, MultipleInputs) {
    MutableTransaction tx;
    
    // Add 3 inputs
    for (int i = 0; i < 3; ++i) {
        TxHash prevHash;
        prevHash[0] = static_cast<uint8_t>(i);
        OutPoint prevOut(prevHash, i);
        tx.vin.push_back(TxIn(prevOut));
    }
    
    Hash160 pubKeyHash;
    Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
    tx.vout.push_back(TxOut(150 * COIN, scriptPubKey));
    
    Transaction transaction(tx);
    EXPECT_EQ(transaction.vin.size(), 3u);
    EXPECT_EQ(transaction.vout.size(), 1u);
    EXPECT_FALSE(transaction.IsCoinBase());
}

TEST_F(MultipleIOTest, MultipleOutputs) {
    MutableTransaction tx;
    
    TxHash prevHash;
    prevHash[0] = 0xAA;
    tx.vin.push_back(TxIn(OutPoint(prevHash, 0)));
    
    // Add 5 outputs
    Hash160 pubKeyHash;
    Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
    for (int i = 0; i < 5; ++i) {
        tx.vout.push_back(TxOut((i + 1) * COIN, scriptPubKey));
    }
    
    Transaction transaction(tx);
    EXPECT_EQ(transaction.vin.size(), 1u);
    EXPECT_EQ(transaction.vout.size(), 5u);
    
    // Value out: 1 + 2 + 3 + 4 + 5 = 15 COIN
    EXPECT_EQ(transaction.GetValueOut(), 15 * COIN);
}

// ============================================================================
// Serialization Round-Trip Tests
// ============================================================================

class SerializationTest : public ::testing::Test {};

TEST_F(SerializationTest, ComplexTransaction) {
    MutableTransaction tx;
    tx.version = 2;
    tx.nLockTime = 500000;
    
    // Multiple inputs with scripts
    for (int i = 0; i < 3; ++i) {
        TxHash prevHash;
        prevHash[0] = static_cast<uint8_t>(i * 10);
        OutPoint prevOut(prevHash, i);
        
        Script scriptSig;
        scriptSig << std::vector<uint8_t>(72, static_cast<uint8_t>(i));  // Fake signature
        scriptSig << std::vector<uint8_t>(33, static_cast<uint8_t>(i + 1));  // Fake pubkey
        
        tx.vin.push_back(TxIn(prevOut, scriptSig, 0xFFFFFFFE - i));
    }
    
    // Multiple outputs
    for (int i = 0; i < 4; ++i) {
        Hash160 pubKeyHash;
        pubKeyHash[0] = static_cast<uint8_t>(i * 20);
        Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
        tx.vout.push_back(TxOut((i + 1) * 10 * COIN, scriptPubKey));
    }
    
    // Serialize
    DataStream ss;
    ss << tx;
    
    // Deserialize
    MutableTransaction deserialized;
    ss >> deserialized;
    
    // Verify all fields
    EXPECT_EQ(tx.version, deserialized.version);
    EXPECT_EQ(tx.nLockTime, deserialized.nLockTime);
    EXPECT_EQ(tx.vin.size(), deserialized.vin.size());
    EXPECT_EQ(tx.vout.size(), deserialized.vout.size());
    
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        EXPECT_EQ(tx.vin[i], deserialized.vin[i]);
    }
    
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        EXPECT_EQ(tx.vout[i], deserialized.vout[i]);
    }
    
    EXPECT_EQ(tx.GetHash(), deserialized.GetHash());
}

TEST_F(SerializationTest, EmptyScripts) {
    MutableTransaction tx;
    
    TxHash prevHash;
    prevHash[0] = 0xAA;
    tx.vin.push_back(TxIn(OutPoint(prevHash, 0)));  // Empty scriptSig
    tx.vout.push_back(TxOut(50 * COIN, Script()));  // Empty scriptPubKey
    
    DataStream ss;
    ss << tx;
    
    MutableTransaction deserialized;
    ss >> deserialized;
    
    EXPECT_TRUE(deserialized.vin[0].scriptSig.empty());
    EXPECT_TRUE(deserialized.vout[0].scriptPubKey.empty());
}

// ============================================================================
// GetSerializeSize Tests
// ============================================================================

TEST_F(SerializationTest, GetSerializeSize) {
    MutableTransaction tx;
    tx.version = 1;
    tx.nLockTime = 0;
    
    TxHash prevHash;
    tx.vin.push_back(TxIn(OutPoint(prevHash, 0)));
    
    Hash160 pubKeyHash;
    Script scriptPubKey = Script::CreateP2PKH(pubKeyHash);
    tx.vout.push_back(TxOut(50 * COIN, scriptPubKey));
    
    size_t expectedSize = GetSerializeSize(tx);
    
    DataStream ss;
    ss << tx;
    
    EXPECT_EQ(expectedSize, ss.TotalSize());
}
