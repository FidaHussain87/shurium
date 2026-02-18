// SHURIUM - Database Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include "shurium/db/database.h"
#include "shurium/db/leveldb.h"
#include "shurium/db/blockdb.h"
#include "shurium/db/utxodb.h"
#include "shurium/core/block.h"
#include "shurium/consensus/params.h"
#include <filesystem>
#include <random>

using namespace shurium;
using namespace shurium::db;

// ============================================================================
// Test Utilities
// ============================================================================

class DatabaseTest : public ::testing::Test {
protected:
    std::filesystem::path testDir_;
    
    void SetUp() override {
        // Create a unique test directory
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 999999);
        
        testDir_ = std::filesystem::temp_directory_path() / 
                   ("shurium_db_test_" + std::to_string(dis(gen)));
        std::filesystem::create_directories(testDir_);
    }
    
    void TearDown() override {
        // Clean up test directory
        std::error_code ec;
        std::filesystem::remove_all(testDir_, ec);
    }
    
    Block CreateTestBlock(uint32_t nonce = 0) {
        auto params = consensus::Params::RegTest();
        return CreateGenesisBlock(
            1700000000 + nonce,  // Different timestamp for different blocks
            nonce,
            0x207fffff,
            1,
            params.nInitialBlockReward
        );
    }
};

// ============================================================================
// Basic Database Tests
// ============================================================================

TEST_F(DatabaseTest, OpenAndClose) {
    Options opts;
    opts.create_if_missing = true;
    
    auto [status, db] = OpenDatabase(testDir_ / "test_db", opts);
    ASSERT_TRUE(status.ok()) << status.ToString();
    ASSERT_NE(db, nullptr);
}

TEST_F(DatabaseTest, PutAndGet) {
    Options opts;
    opts.create_if_missing = true;
    
    auto [status, db] = OpenDatabase(testDir_ / "test_db", opts);
    ASSERT_TRUE(status.ok());
    
    // Put a value
    Status s = db->Put(Slice("key1"), Slice("value1"));
    ASSERT_TRUE(s.ok());
    
    // Get it back
    std::string value;
    s = db->Get(Slice("key1"), &value);
    ASSERT_TRUE(s.ok());
    EXPECT_EQ(value, "value1");
}

TEST_F(DatabaseTest, GetNotFound) {
    Options opts;
    opts.create_if_missing = true;
    
    auto [status, db] = OpenDatabase(testDir_ / "test_db", opts);
    ASSERT_TRUE(status.ok());
    
    std::string value;
    Status s = db->Get(Slice("nonexistent"), &value);
    EXPECT_TRUE(s.IsNotFound());
}

TEST_F(DatabaseTest, Delete) {
    Options opts;
    opts.create_if_missing = true;
    
    auto [status, db] = OpenDatabase(testDir_ / "test_db", opts);
    ASSERT_TRUE(status.ok());
    
    // Put then delete
    db->Put(Slice("key1"), Slice("value1"));
    Status s = db->Delete(Slice("key1"));
    ASSERT_TRUE(s.ok());
    
    // Verify deleted
    std::string value;
    s = db->Get(Slice("key1"), &value);
    EXPECT_TRUE(s.IsNotFound());
}

TEST_F(DatabaseTest, WriteBatch) {
    Options opts;
    opts.create_if_missing = true;
    
    auto [status, db] = OpenDatabase(testDir_ / "test_db", opts);
    ASSERT_TRUE(status.ok());
    
    // Create batch
    db::WriteBatch batch;
    batch.Put(Slice("key1"), Slice("value1"));
    batch.Put(Slice("key2"), Slice("value2"));
    batch.Put(Slice("key3"), Slice("value3"));
    batch.Delete(Slice("key2"));  // Delete in same batch
    
    Status s = db->Write(&batch);
    ASSERT_TRUE(s.ok());
    
    // Verify
    std::string value;
    s = db->Get(Slice("key1"), &value);
    ASSERT_TRUE(s.ok());
    EXPECT_EQ(value, "value1");
    
    s = db->Get(Slice("key2"), &value);
    EXPECT_TRUE(s.IsNotFound());
    
    s = db->Get(Slice("key3"), &value);
    ASSERT_TRUE(s.ok());
    EXPECT_EQ(value, "value3");
}

TEST_F(DatabaseTest, Iterator) {
    Options opts;
    opts.create_if_missing = true;
    
    auto [status, db] = OpenDatabase(testDir_ / "test_db", opts);
    ASSERT_TRUE(status.ok());
    
    // Put several values
    db->Put(Slice("a"), Slice("1"));
    db->Put(Slice("b"), Slice("2"));
    db->Put(Slice("c"), Slice("3"));
    
    // Iterate
    auto iter = db->NewIterator();
    int count = 0;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        ++count;
    }
    EXPECT_EQ(count, 3);
}

TEST_F(DatabaseTest, Exists) {
    Options opts;
    opts.create_if_missing = true;
    
    auto [status, db] = OpenDatabase(testDir_ / "test_db", opts);
    ASSERT_TRUE(status.ok());
    
    db->Put(Slice("key1"), Slice("value1"));
    
    EXPECT_TRUE(db->Exists(Slice("key1")));
    EXPECT_FALSE(db->Exists(Slice("key2")));
}

// ============================================================================
// Memory Database Tests
// ============================================================================

TEST_F(DatabaseTest, MemoryDatabaseBasic) {
    MemoryDatabase db;
    
    // Put and get
    Status s = db.Put(WriteOptions(), Slice("key"), Slice("value"));
    ASSERT_TRUE(s.ok());
    
    std::string value;
    s = db.Get(ReadOptions(), Slice("key"), &value);
    ASSERT_TRUE(s.ok());
    EXPECT_EQ(value, "value");
}

// ============================================================================
// Serialization Tests
// ============================================================================

TEST_F(DatabaseTest, SerializeDeserialize) {
    // Test serialization helpers with a simple struct
    Hash256 original;
    for (size_t i = 0; i < 32; ++i) {
        original[i] = static_cast<uint8_t>(i);
    }
    
    std::string data = SerializeToString(original);
    EXPECT_EQ(data.size(), 32);
    
    Hash256 restored;
    EXPECT_TRUE(DeserializeFromString(data, restored));
    EXPECT_EQ(original, restored);
}

TEST_F(DatabaseTest, MakeKey) {
    // Test key creation
    std::string key1 = MakeKey(prefix::COIN);
    EXPECT_EQ(key1.size(), 1);
    EXPECT_EQ(key1[0], prefix::COIN);
    
    // Test with data
    std::string key2 = MakeKey(prefix::BLOCK_INDEX, Slice("test"));
    EXPECT_EQ(key2[0], prefix::BLOCK_INDEX);
    EXPECT_EQ(key2.substr(1), "test");
}

// ============================================================================
// Block Database Tests
// ============================================================================

TEST_F(DatabaseTest, BlockDBOpen) {
    BlockDB db(testDir_);
    EXPECT_TRUE(db.IsOpen());
}

TEST_F(DatabaseTest, BlockDBWriteAndReadBlock) {
    BlockDB db(testDir_);
    
    // Create a test block
    Block block = CreateTestBlock(1);
    
    // Write it
    DiskBlockPos pos;
    Status s = db.WriteBlock(block, pos);
    ASSERT_TRUE(s.ok()) << s.ToString();
    EXPECT_FALSE(pos.IsNull());
    
    // Read it back
    Block readBlock;
    s = db.ReadBlock(pos, readBlock);
    ASSERT_TRUE(s.ok()) << s.ToString();
    
    // Verify
    EXPECT_EQ(readBlock.GetHash(), block.GetHash());
    EXPECT_EQ(readBlock.nTime, block.nTime);
    EXPECT_EQ(readBlock.nNonce, block.nNonce);
}

TEST_F(DatabaseTest, BlockDBMultipleBlocks) {
    BlockDB db(testDir_);
    
    std::vector<Block> blocks;
    std::vector<DiskBlockPos> positions;
    
    // Write multiple blocks
    for (int i = 0; i < 10; ++i) {
        Block block = CreateTestBlock(i);
        DiskBlockPos pos;
        Status s = db.WriteBlock(block, pos);
        ASSERT_TRUE(s.ok()) << "Write failed at i=" << i << ": " << s.ToString();
        
        blocks.push_back(block);
        positions.push_back(pos);
    }
    
    // Flush before reading
    db.Flush();
    
    // Read them back
    for (size_t i = 0; i < blocks.size(); ++i) {
        Block readBlock;
        Status s = db.ReadBlock(positions[i], readBlock);
        ASSERT_TRUE(s.ok()) << "Read failed at i=" << i << ": " << s.ToString() 
                            << " pos=(" << positions[i].nFile << "," << positions[i].nPos << ")";
        EXPECT_EQ(readBlock.GetHash(), blocks[i].GetHash());
    }
}

TEST_F(DatabaseTest, BlockDBBestChainTip) {
    BlockDB db(testDir_);
    
    // Initially no best chain
    auto tip = db.ReadBestChainTip();
    EXPECT_FALSE(tip.has_value());
    
    // Set best chain
    Block block = CreateTestBlock(0);
    BlockHash hash = block.GetHash();
    
    Status s = db.WriteBestChainTip(hash);
    ASSERT_TRUE(s.ok());
    
    // Read it back
    tip = db.ReadBestChainTip();
    ASSERT_TRUE(tip.has_value());
    EXPECT_EQ(*tip, hash);
}

// ============================================================================
// UTXO Database Tests
// ============================================================================

TEST_F(DatabaseTest, UTXODBOpen) {
    CoinsViewDB db(testDir_ / "utxo");
    EXPECT_TRUE(db.IsOpen());
}

TEST_F(DatabaseTest, UTXODBAddAndGetCoin) {
    CoinsViewDB db(testDir_ / "utxo");
    
    // Create a test outpoint and coin
    TxHash txHash;
    for (size_t i = 0; i < 32; ++i) {
        txHash[i] = static_cast<uint8_t>(i);
    }
    OutPoint outpoint(txHash, 0);
    
    TxOut txout(1000000, Script());  // 0.01 NXS
    Coin coin(txout, 100, false);
    
    // Add coin
    Status s = db.AddCoin(outpoint, coin);
    ASSERT_TRUE(s.ok());
    
    // Check existence
    EXPECT_TRUE(db.HaveCoin(outpoint));
    
    // Get coin
    auto retrieved = db.GetCoin(outpoint);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->GetAmount(), 1000000);
    EXPECT_EQ(retrieved->nHeight, 100);
    EXPECT_FALSE(retrieved->IsCoinBase());
}

TEST_F(DatabaseTest, UTXODBRemoveCoin) {
    CoinsViewDB db(testDir_ / "utxo");
    
    TxHash txHash;
    txHash.SetNull();
    OutPoint outpoint(txHash, 0);
    
    TxOut txout(1000000, Script());
    Coin coin(txout, 100, false);
    
    // Add and remove
    db.AddCoin(outpoint, coin);
    EXPECT_TRUE(db.HaveCoin(outpoint));
    
    Status s = db.RemoveCoin(outpoint);
    ASSERT_TRUE(s.ok());
    
    EXPECT_FALSE(db.HaveCoin(outpoint));
}

TEST_F(DatabaseTest, UTXODBBestBlock) {
    CoinsViewDB db(testDir_ / "utxo");
    
    // Initially empty
    BlockHash initial = db.GetBestBlock();
    EXPECT_TRUE(initial.IsNull());
    
    // Set best block
    BlockHash hash;
    for (size_t i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i + 100);
    }
    
    Status s = db.SetBestBlock(hash);
    ASSERT_TRUE(s.ok());
    
    // Read back
    BlockHash read = db.GetBestBlock();
    EXPECT_EQ(read, hash);
}

TEST_F(DatabaseTest, UTXODBBatchWrite) {
    CoinsViewDB db(testDir_ / "utxo");
    
    // Create a batch of coins
    CoinsMap coins;
    for (int i = 0; i < 100; ++i) {
        TxHash txHash;
        for (size_t j = 0; j < 32; ++j) {
            txHash[j] = static_cast<uint8_t>((i + j) % 256);
        }
        OutPoint outpoint(txHash, 0);
        
        TxOut txout(1000000 * (i + 1), Script());
        Coin coin(txout, i, i % 2 == 0);
        
        CoinsCacheEntry entry(std::move(coin));
        entry.SetDirty();
        coins[outpoint] = std::move(entry);
    }
    
    // Write batch
    BlockHash bestBlock;
    for (size_t i = 0; i < 32; ++i) {
        bestBlock[i] = static_cast<uint8_t>(255 - i);
    }
    
    bool success = db.BatchWrite(coins, bestBlock);
    ASSERT_TRUE(success);
    
    // Verify coins were added (coins map is cleared after batch write)
    EXPECT_EQ(db.GetBestBlock(), bestBlock);
    
    // Verify some coins exist
    TxHash testHash;
    for (size_t j = 0; j < 32; ++j) {
        testHash[j] = static_cast<uint8_t>(j % 256);
    }
    EXPECT_TRUE(db.HaveCoin(OutPoint(testHash, 0)));
}

TEST_F(DatabaseTest, UTXODBStatistics) {
    CoinsViewDB db(testDir_ / "utxo");
    
    // Add some coins
    for (int i = 0; i < 10; ++i) {
        TxHash txHash;
        for (size_t j = 0; j < 32; ++j) {
            txHash[j] = static_cast<uint8_t>((i * 10 + j) % 256);
        }
        OutPoint outpoint(txHash, 0);
        
        TxOut txout(1000000, Script());
        Coin coin(txout, i, false);
        
        db.AddCoin(outpoint, coin);
    }
    
    // Check stats
    EXPECT_GT(db.GetWriteCount(), 0);
}

// ============================================================================
// Status Tests
// ============================================================================

TEST_F(DatabaseTest, StatusOk) {
    Status s = Status::Ok();
    EXPECT_TRUE(s.ok());
    EXPECT_FALSE(s.IsNotFound());
    EXPECT_FALSE(s.IsCorruption());
    EXPECT_FALSE(s.IsIOError());
}

TEST_F(DatabaseTest, StatusNotFound) {
    Status s = Status::NotFound("key not found");
    EXPECT_FALSE(s.ok());
    EXPECT_TRUE(s.IsNotFound());
    EXPECT_EQ(s.message(), "key not found");
}

TEST_F(DatabaseTest, StatusCorruption) {
    Status s = Status::Corruption("data corrupted");
    EXPECT_FALSE(s.ok());
    EXPECT_TRUE(s.IsCorruption());
}

TEST_F(DatabaseTest, StatusIOError) {
    Status s = Status::IOError("disk full");
    EXPECT_FALSE(s.ok());
    EXPECT_TRUE(s.IsIOError());
}

TEST_F(DatabaseTest, StatusToString) {
    Status s = Status::NotFound("test message");
    std::string str = s.ToString();
    EXPECT_NE(str.find("NotFound"), std::string::npos);
    EXPECT_NE(str.find("test message"), std::string::npos);
}

// ============================================================================
// Slice Tests
// ============================================================================

TEST_F(DatabaseTest, SliceBasic) {
    std::string data = "hello world";
    Slice slice(data);
    
    EXPECT_EQ(slice.size(), data.size());
    EXPECT_EQ(slice.ToString(), data);
    EXPECT_FALSE(slice.empty());
}

TEST_F(DatabaseTest, SliceCompare) {
    Slice a("abc");
    Slice b("abd");
    Slice c("abc");
    
    EXPECT_LT(a.compare(b), 0);
    EXPECT_GT(b.compare(a), 0);
    EXPECT_EQ(a.compare(c), 0);
    
    EXPECT_TRUE(a == c);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < b);
}

TEST_F(DatabaseTest, SliceEmpty) {
    Slice empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0);
}
