// SHURIUM - Block Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/core/block.h"
#include "shurium/core/transaction.h"
#include "shurium/core/merkle.h"
#include "shurium/core/serialize.h"
#include "shurium/crypto/sha256.h"

using namespace shurium;

// ============================================================================
// BlockHeader Tests
// ============================================================================

class BlockHeaderTest : public ::testing::Test {
protected:
    BlockHash prevBlockHash;
    Hash256 merkleRoot;
    
    void SetUp() override {
        // Create test hashes
        for (size_t i = 0; i < 32; ++i) {
            prevBlockHash[i] = static_cast<uint8_t>(i + 1);
            merkleRoot[i] = static_cast<uint8_t>(32 - i);
        }
    }
};

TEST_F(BlockHeaderTest, DefaultConstructor) {
    BlockHeader header;
    EXPECT_TRUE(header.IsNull());
    EXPECT_EQ(header.nVersion, 0);
    EXPECT_TRUE(header.hashPrevBlock.IsNull());
    EXPECT_TRUE(header.hashMerkleRoot.IsNull());
    EXPECT_EQ(header.nTime, 0u);
    EXPECT_EQ(header.nBits, 0u);
    EXPECT_EQ(header.nNonce, 0u);
}

TEST_F(BlockHeaderTest, SetFields) {
    BlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock = prevBlockHash;
    header.hashMerkleRoot = merkleRoot;
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 42;
    
    EXPECT_FALSE(header.IsNull());
    EXPECT_EQ(header.nVersion, 1);
    EXPECT_EQ(header.hashPrevBlock, prevBlockHash);
    EXPECT_EQ(header.hashMerkleRoot, merkleRoot);
    EXPECT_EQ(header.nTime, 1234567890u);
    EXPECT_EQ(header.nBits, 0x1d00ffffu);
    EXPECT_EQ(header.nNonce, 42u);
}

TEST_F(BlockHeaderTest, SetNull) {
    BlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock = prevBlockHash;
    header.nBits = 0x1d00ffff;
    
    EXPECT_FALSE(header.IsNull());
    header.SetNull();
    EXPECT_TRUE(header.IsNull());
    EXPECT_EQ(header.nVersion, 0);
    EXPECT_TRUE(header.hashPrevBlock.IsNull());
}

TEST_F(BlockHeaderTest, GetHash) {
    BlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock = prevBlockHash;
    header.hashMerkleRoot = merkleRoot;
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 42;
    
    BlockHash hash = header.GetHash();
    EXPECT_FALSE(hash.IsNull());
    
    // Hash should be consistent
    EXPECT_EQ(hash, header.GetHash());
}

TEST_F(BlockHeaderTest, DifferentHeadersDifferentHashes) {
    BlockHeader header1;
    header1.nVersion = 1;
    header1.nBits = 0x1d00ffff;
    header1.nNonce = 1;
    
    BlockHeader header2;
    header2.nVersion = 1;
    header2.nBits = 0x1d00ffff;
    header2.nNonce = 2;
    
    EXPECT_NE(header1.GetHash(), header2.GetHash());
}

TEST_F(BlockHeaderTest, GetBlockTime) {
    BlockHeader header;
    header.nTime = 1700000000;
    header.nBits = 1;  // Make non-null
    
    EXPECT_EQ(header.GetBlockTime(), 1700000000);
}

TEST_F(BlockHeaderTest, Serialization) {
    BlockHeader header;
    header.nVersion = 2;
    header.hashPrevBlock = prevBlockHash;
    header.hashMerkleRoot = merkleRoot;
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 123456;
    
    DataStream ss;
    ss << header;
    
    BlockHeader deserialized;
    ss >> deserialized;
    
    EXPECT_EQ(header.nVersion, deserialized.nVersion);
    EXPECT_EQ(header.hashPrevBlock, deserialized.hashPrevBlock);
    EXPECT_EQ(header.hashMerkleRoot, deserialized.hashMerkleRoot);
    EXPECT_EQ(header.nTime, deserialized.nTime);
    EXPECT_EQ(header.nBits, deserialized.nBits);
    EXPECT_EQ(header.nNonce, deserialized.nNonce);
}

TEST_F(BlockHeaderTest, SerializationSize) {
    BlockHeader header;
    header.nVersion = 1;
    header.nBits = 0x1d00ffff;
    
    // Header should be exactly 80 bytes:
    // 4 (version) + 32 (prevBlock) + 32 (merkleRoot) + 4 (time) + 4 (bits) + 4 (nonce)
    EXPECT_EQ(GetSerializeSize(header), 80u);
}

TEST_F(BlockHeaderTest, ToString) {
    BlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock = prevBlockHash;
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 42;
    
    std::string str = header.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("version=1"), std::string::npos);
}

// ============================================================================
// Block Tests
// ============================================================================

class BlockTest : public ::testing::Test {
protected:
    BlockHeader testHeader;
    TransactionRef coinbaseTx;
    TransactionRef regularTx;
    
    void SetUp() override {
        // Set up test header
        testHeader.nVersion = 1;
        testHeader.nTime = 1700000000;
        testHeader.nBits = 0x1d00ffff;
        testHeader.nNonce = 12345;
        
        // Create coinbase transaction
        MutableTransaction mtxCoinbase;
        OutPoint nullOutpoint;
        Script coinbaseScript;
        coinbaseScript << std::vector<uint8_t>{0x04, 0x01, 0x00, 0x00};
        mtxCoinbase.vin.push_back(TxIn(nullOutpoint, coinbaseScript));
        
        Hash160 minerPubKeyHash;
        minerPubKeyHash[0] = 0xAA;
        Script minerScript = Script::CreateP2PKH(minerPubKeyHash);
        mtxCoinbase.vout.push_back(TxOut(50 * COIN, minerScript));
        
        coinbaseTx = MakeTransactionRef(std::move(mtxCoinbase));
        
        // Create regular transaction
        MutableTransaction mtxRegular;
        TxHash prevTxHash;
        prevTxHash[0] = 0xBB;
        mtxRegular.vin.push_back(TxIn(OutPoint(prevTxHash, 0)));
        mtxRegular.vout.push_back(TxOut(25 * COIN, minerScript));
        
        regularTx = MakeTransactionRef(std::move(mtxRegular));
    }
};

TEST_F(BlockTest, DefaultConstructor) {
    Block block;
    EXPECT_TRUE(block.IsNull());
    EXPECT_TRUE(block.vtx.empty());
}

TEST_F(BlockTest, ConstructFromHeader) {
    Block block(testHeader);
    
    EXPECT_EQ(block.nVersion, testHeader.nVersion);
    EXPECT_EQ(block.nTime, testHeader.nTime);
    EXPECT_EQ(block.nBits, testHeader.nBits);
    EXPECT_EQ(block.nNonce, testHeader.nNonce);
    EXPECT_TRUE(block.vtx.empty());
}

TEST_F(BlockTest, AddTransactions) {
    Block block;
    block.nVersion = 1;
    block.nBits = 0x1d00ffff;
    block.vtx.push_back(coinbaseTx);
    block.vtx.push_back(regularTx);
    
    EXPECT_EQ(block.vtx.size(), 2u);
    EXPECT_TRUE(block.vtx[0]->IsCoinBase());
    EXPECT_FALSE(block.vtx[1]->IsCoinBase());
}

TEST_F(BlockTest, SetNull) {
    Block block;
    block.nVersion = 1;
    block.nBits = 0x1d00ffff;
    block.vtx.push_back(coinbaseTx);
    
    EXPECT_FALSE(block.IsNull());
    block.SetNull();
    EXPECT_TRUE(block.IsNull());
    EXPECT_TRUE(block.vtx.empty());
}

TEST_F(BlockTest, GetBlockHeader) {
    Block block;
    block.nVersion = 2;
    block.hashPrevBlock = testHeader.hashPrevBlock;
    block.hashMerkleRoot = testHeader.hashMerkleRoot;
    block.nTime = 1234567890;
    block.nBits = 0x1d00ffff;
    block.nNonce = 999;
    
    BlockHeader header = block.GetBlockHeader();
    
    EXPECT_EQ(header.nVersion, block.nVersion);
    EXPECT_EQ(header.hashPrevBlock, block.hashPrevBlock);
    EXPECT_EQ(header.hashMerkleRoot, block.hashMerkleRoot);
    EXPECT_EQ(header.nTime, block.nTime);
    EXPECT_EQ(header.nBits, block.nBits);
    EXPECT_EQ(header.nNonce, block.nNonce);
}

TEST_F(BlockTest, GetHash) {
    Block block;
    block.nVersion = 1;
    block.nTime = 1234567890;
    block.nBits = 0x1d00ffff;
    block.nNonce = 42;
    block.vtx.push_back(coinbaseTx);
    
    BlockHash hash = block.GetHash();
    EXPECT_FALSE(hash.IsNull());
    
    // Hash should match header hash (block hash is header hash)
    EXPECT_EQ(hash, block.GetBlockHeader().GetHash());
}

TEST_F(BlockTest, ComputeMerkleRoot) {
    Block block;
    block.nVersion = 1;
    block.nBits = 0x1d00ffff;
    block.vtx.push_back(coinbaseTx);
    block.vtx.push_back(regularTx);
    
    Hash256 merkleRoot = block.ComputeMerkleRoot();
    EXPECT_FALSE(merkleRoot.IsNull());
    
    // Should be consistent
    EXPECT_EQ(merkleRoot, block.ComputeMerkleRoot());
}

TEST_F(BlockTest, ComputeMerkleRootSingleTx) {
    Block block;
    block.nVersion = 1;
    block.nBits = 0x1d00ffff;
    block.vtx.push_back(coinbaseTx);
    
    Hash256 merkleRoot = block.ComputeMerkleRoot();
    
    // For single transaction, merkle root should be the transaction hash
    EXPECT_EQ(merkleRoot, Hash256(coinbaseTx->GetHash().data(), 32));
}

TEST_F(BlockTest, ComputeMerkleRootEmpty) {
    Block block;
    block.nVersion = 1;
    block.nBits = 0x1d00ffff;
    
    Hash256 merkleRoot = block.ComputeMerkleRoot();
    EXPECT_TRUE(merkleRoot.IsNull());
}

TEST_F(BlockTest, Serialization) {
    Block block;
    block.nVersion = 2;
    block.nTime = 1234567890;
    block.nBits = 0x1d00ffff;
    block.nNonce = 42;
    block.vtx.push_back(coinbaseTx);
    block.vtx.push_back(regularTx);
    
    DataStream ss;
    ss << block;
    
    Block deserialized;
    ss >> deserialized;
    
    EXPECT_EQ(block.nVersion, deserialized.nVersion);
    EXPECT_EQ(block.nTime, deserialized.nTime);
    EXPECT_EQ(block.nBits, deserialized.nBits);
    EXPECT_EQ(block.nNonce, deserialized.nNonce);
    EXPECT_EQ(block.vtx.size(), deserialized.vtx.size());
    EXPECT_EQ(block.GetHash(), deserialized.GetHash());
}

TEST_F(BlockTest, ToString) {
    Block block;
    block.nVersion = 1;
    block.nTime = 1234567890;
    block.nBits = 0x1d00ffff;
    block.nNonce = 42;
    block.vtx.push_back(coinbaseTx);
    
    std::string str = block.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("Block"), std::string::npos);
    EXPECT_NE(str.find("1"), std::string::npos);  // tx count
}

// ============================================================================
// Genesis Block Tests
// ============================================================================

class GenesisBlockTest : public ::testing::Test {};

TEST_F(GenesisBlockTest, CreateGenesisBlock) {
    // Genesis block should have specific properties
    Block genesis = CreateGenesisBlock(
        1700000000,          // timestamp
        0,                   // nonce (would be mined)
        0x1d00ffff,          // bits
        1,                   // version
        50 * COIN            // subsidy
    );
    
    EXPECT_FALSE(genesis.IsNull());
    EXPECT_EQ(genesis.vtx.size(), 1u);
    EXPECT_TRUE(genesis.vtx[0]->IsCoinBase());
    EXPECT_EQ(genesis.nVersion, 1);
    EXPECT_TRUE(genesis.hashPrevBlock.IsNull());  // No previous block
}

TEST_F(GenesisBlockTest, GenesisBlockHasNullPrevHash) {
    Block genesis = CreateGenesisBlock(1700000000, 0, 0x1d00ffff, 1, 50 * COIN);
    EXPECT_TRUE(genesis.hashPrevBlock.IsNull());
}

TEST_F(GenesisBlockTest, GenesisBlockMerkleRoot) {
    Block genesis = CreateGenesisBlock(1700000000, 0, 0x1d00ffff, 1, 50 * COIN);
    
    // Merkle root should match the single coinbase transaction
    Hash256 computedRoot = genesis.ComputeMerkleRoot();
    EXPECT_EQ(genesis.hashMerkleRoot, computedRoot);
}

TEST_F(GenesisBlockTest, GenesisBlockCoinbaseValue) {
    Block genesis = CreateGenesisBlock(1700000000, 0, 0x1d00ffff, 1, 50 * COIN);
    
    EXPECT_EQ(genesis.vtx[0]->GetValueOut(), 50 * COIN);
}

// ============================================================================
// Block Validation Tests (Basic)
// ============================================================================

class BlockValidationTest : public ::testing::Test {
protected:
    Block validBlock;
    
    void SetUp() override {
        validBlock.nVersion = 1;
        validBlock.nTime = 1700000000;
        validBlock.nBits = 0x1d00ffff;
        validBlock.nNonce = 0;
        
        // Add coinbase
        MutableTransaction mtx;
        OutPoint nullOutpoint;
        mtx.vin.push_back(TxIn(nullOutpoint));
        Hash160 pubKeyHash;
        mtx.vout.push_back(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)));
        validBlock.vtx.push_back(MakeTransactionRef(std::move(mtx)));
        
        validBlock.hashMerkleRoot = validBlock.ComputeMerkleRoot();
    }
};

TEST_F(BlockValidationTest, EmptyBlockInvalid) {
    Block block;
    block.nVersion = 1;
    block.nBits = 0x1d00ffff;
    // No transactions
    
    EXPECT_TRUE(block.vtx.empty());
}

TEST_F(BlockValidationTest, FirstTxMustBeCoinbase) {
    EXPECT_TRUE(validBlock.vtx[0]->IsCoinBase());
}

TEST_F(BlockValidationTest, MerkleRootMismatch) {
    // Modify merkle root to be wrong
    Block block = validBlock;
    block.hashMerkleRoot[0] ^= 0xFF;
    
    Hash256 computed = block.ComputeMerkleRoot();
    EXPECT_NE(block.hashMerkleRoot, computed);
}

TEST_F(BlockValidationTest, MerkleRootMatch) {
    EXPECT_EQ(validBlock.hashMerkleRoot, validBlock.ComputeMerkleRoot());
}

// ============================================================================
// Block Size Tests
// ============================================================================

class BlockSizeTest : public ::testing::Test {};

TEST_F(BlockSizeTest, EmptyBlockSize) {
    Block block;
    block.nVersion = 1;
    block.nBits = 0x1d00ffff;
    
    // Empty block: 80 byte header + 1 byte (varint for 0 txs)
    size_t size = GetSerializeSize(block);
    EXPECT_EQ(size, 81u);
}

TEST_F(BlockSizeTest, GetTotalSize) {
    Block block;
    block.nVersion = 1;
    block.nBits = 0x1d00ffff;
    
    // Add a simple coinbase
    MutableTransaction mtx;
    OutPoint nullOutpoint;
    mtx.vin.push_back(TxIn(nullOutpoint));
    Hash160 pubKeyHash;
    mtx.vout.push_back(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)));
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    
    size_t totalSize = block.GetTotalSize();
    EXPECT_GT(totalSize, 80u);  // More than just header
    
    // Should match serialization size
    EXPECT_EQ(totalSize, GetSerializeSize(block));
}

// ============================================================================
// Block Locator Tests
// ============================================================================

class BlockLocatorTest : public ::testing::Test {};

TEST_F(BlockLocatorTest, DefaultConstructor) {
    BlockLocator locator;
    EXPECT_TRUE(locator.IsNull());
    EXPECT_TRUE(locator.vHave.empty());
}

TEST_F(BlockLocatorTest, ConstructWithHashes) {
    std::vector<BlockHash> hashes;
    for (int i = 0; i < 5; ++i) {
        BlockHash hash;
        hash[0] = static_cast<uint8_t>(i);
        hashes.push_back(hash);
    }
    
    BlockLocator locator(std::move(hashes));
    EXPECT_FALSE(locator.IsNull());
    EXPECT_EQ(locator.vHave.size(), 5u);
}

TEST_F(BlockLocatorTest, SetNull) {
    BlockHash hash;
    hash[0] = 0x42;
    std::vector<BlockHash> hashes = {hash};
    
    BlockLocator locator(std::move(hashes));
    EXPECT_FALSE(locator.IsNull());
    
    locator.SetNull();
    EXPECT_TRUE(locator.IsNull());
}

TEST_F(BlockLocatorTest, Serialization) {
    std::vector<BlockHash> hashes;
    for (int i = 0; i < 3; ++i) {
        BlockHash hash;
        hash[0] = static_cast<uint8_t>(i + 1);
        hashes.push_back(hash);
    }
    
    BlockLocator locator(std::move(hashes));
    
    DataStream ss;
    ss << locator;
    
    BlockLocator deserialized;
    ss >> deserialized;
    
    EXPECT_EQ(locator.vHave.size(), deserialized.vHave.size());
    for (size_t i = 0; i < locator.vHave.size(); ++i) {
        EXPECT_EQ(locator.vHave[i], deserialized.vHave[i]);
    }
}

// ============================================================================
// Multiple Block Tests
// ============================================================================

class BlockChainTest : public ::testing::Test {
protected:
    Block genesisBlock;
    
    void SetUp() override {
        genesisBlock = CreateGenesisBlock(1700000000, 0, 0x1d00ffff, 1, 50 * COIN);
    }
};

TEST_F(BlockChainTest, BlockLinksToParent) {
    Block block2;
    block2.nVersion = 1;
    block2.hashPrevBlock = genesisBlock.GetHash();
    block2.nTime = genesisBlock.nTime + 30;  // 30 second block time
    block2.nBits = 0x1d00ffff;
    block2.nNonce = 0;
    
    // Add coinbase
    MutableTransaction mtx;
    OutPoint nullOutpoint;
    mtx.vin.push_back(TxIn(nullOutpoint));
    Hash160 pubKeyHash;
    mtx.vout.push_back(TxOut(50 * COIN, Script::CreateP2PKH(pubKeyHash)));
    block2.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    block2.hashMerkleRoot = block2.ComputeMerkleRoot();
    
    EXPECT_EQ(block2.hashPrevBlock, genesisBlock.GetHash());
    EXPECT_NE(block2.GetHash(), genesisBlock.GetHash());
}

TEST_F(BlockChainTest, DifferentNoncesDifferentHashes) {
    Block block1 = genesisBlock;
    Block block2 = genesisBlock;
    
    block1.nNonce = 1;
    block2.nNonce = 2;
    
    EXPECT_NE(block1.GetHash(), block2.GetHash());
}
