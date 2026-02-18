// SHURIUM - UTXO Database Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/db/utxodb.h"
#include "shurium/crypto/sha256.h"
#include <map>

namespace shurium {
namespace db {

// ============================================================================
// CoinsViewDB Implementation
// ============================================================================

CoinsViewDB::CoinsViewDB(const std::filesystem::path& dbPath, 
                          const Options& options,
                          bool wipe)
    : dbPath_(dbPath)
{
    // Create directory if needed
    std::error_code ec;
    std::filesystem::create_directories(dbPath_, ec);
    
    // Wipe if requested
    if (wipe) {
        std::filesystem::remove_all(dbPath_, ec);
        std::filesystem::create_directories(dbPath_, ec);
    }
    
    // Open database
    auto [status, database] = OpenDatabase(dbPath_, options);
    if (!status.ok()) {
        throw std::runtime_error("Failed to open UTXO database: " + status.ToString());
    }
    db_ = std::move(database);
}

std::optional<Coin> CoinsViewDB::GetCoin(const OutPoint& outpoint) const {
    if (!db_) {
        return std::nullopt;
    }
    
    ++nReads_;
    
    std::string key = MakeKey(prefix::COIN, outpoint);
    std::string value;
    Status s = db_->Get(ReadOptions(), Slice(key), &value);
    
    if (!s.ok()) {
        return std::nullopt;
    }
    
    nReadBytes_ += value.size();
    
    Coin coin;
    if (!DeserializeFromString(value, coin)) {
        return std::nullopt;
    }
    
    return coin;
}

bool CoinsViewDB::HaveCoin(const OutPoint& outpoint) const {
    if (!db_) {
        return false;
    }
    
    std::string key = MakeKey(prefix::COIN, outpoint);
    return db_->Exists(Slice(key));
}

BlockHash CoinsViewDB::GetBestBlock() const {
    if (cachedBestBlockValid_) {
        return cachedBestBlock_;
    }
    
    if (!db_) {
        return BlockHash();
    }
    
    std::string key = MakeKey(prefix::COINS_TIP);
    std::string value;
    Status s = db_->Get(ReadOptions(), Slice(key), &value);
    
    if (!s.ok()) {
        return BlockHash();
    }
    
    BlockHash hash;
    if (DeserializeFromString(value, hash)) {
        cachedBestBlock_ = hash;
        cachedBestBlockValid_ = true;
        return hash;
    }
    
    return BlockHash();
}

size_t CoinsViewDB::EstimateSize() const {
    // This is an estimate based on disk usage
    if (!db_) {
        return 0;
    }
    
    // Rough estimate: one UTXO ~= 50 bytes on average
    // Disk usage / 50 gives approximate count
    uint64_t diskUsage = db_->GetDiskUsage();
    return diskUsage / 50;
}

bool CoinsViewDB::BatchWrite(CoinsMap& mapCoins, const BlockHash& hashBlock) {
    if (!db_) {
        return false;
    }
    
    db::WriteBatch batch;
    size_t writeBytes = 0;
    size_t writeCount = 0;
    
    for (auto it = mapCoins.begin(); it != mapCoins.end(); ) {
        const OutPoint& outpoint = it->first;
        CoinsCacheEntry& entry = it->second;
        
        if (entry.IsDirty()) {
            std::string key = MakeKey(prefix::COIN, outpoint);
            
            if (entry.coin.IsSpent()) {
                // Delete spent coin
                batch.Delete(Slice(key));
            } else {
                // Write unspent coin
                std::string value = SerializeToString(entry.coin);
                batch.Put(Slice(key), Slice(value));
                writeBytes += value.size();
            }
            ++writeCount;
        }
        
        // Erase from cache after processing
        it = mapCoins.erase(it);
    }
    
    // Update best block
    if (!hashBlock.IsNull()) {
        std::string key = MakeKey(prefix::COINS_TIP);
        std::string value = SerializeToString(hashBlock);
        batch.Put(Slice(key), Slice(value));
        
        cachedBestBlock_ = hashBlock;
        cachedBestBlockValid_ = true;
    }
    
    // Write batch
    WriteOptions opts;
    opts.sync = true;  // Sync for safety
    Status s = db_->Write(opts, &batch);
    
    if (s.ok()) {
        nWrites_ += writeCount;
        nWriteBytes_ += writeBytes;
    }
    
    return s.ok();
}

Status CoinsViewDB::AddCoin(const OutPoint& outpoint, const Coin& coin) {
    if (!db_) {
        return Status::NotSupported("Database not open");
    }
    
    std::string key = MakeKey(prefix::COIN, outpoint);
    std::string value = SerializeToString(coin);
    
    ++nWrites_;
    nWriteBytes_ += value.size();
    
    return db_->Put(WriteOptions(), Slice(key), Slice(value));
}

Status CoinsViewDB::RemoveCoin(const OutPoint& outpoint) {
    if (!db_) {
        return Status::NotSupported("Database not open");
    }
    
    std::string key = MakeKey(prefix::COIN, outpoint);
    ++nWrites_;
    return db_->Delete(WriteOptions(), Slice(key));
}

Status CoinsViewDB::SetBestBlock(const BlockHash& hash) {
    if (!db_) {
        return Status::NotSupported("Database not open");
    }
    
    std::string key = MakeKey(prefix::COINS_TIP);
    std::string value = SerializeToString(hash);
    
    WriteOptions opts;
    opts.sync = true;
    Status s = db_->Put(opts, Slice(key), Slice(value));
    
    if (s.ok()) {
        cachedBestBlock_ = hash;
        cachedBestBlockValid_ = true;
    }
    
    return s;
}

std::unique_ptr<Iterator> CoinsViewDB::NewIterator() const {
    if (!db_) {
        return nullptr;
    }
    return db_->NewIterator();
}

// ============================================================================
// UTXO Statistics
// ============================================================================

Hash256 CalculateUTXOSetHash(const CoinsViewDB& coinsView) {
    SHA256 hasher;
    
    coinsView.ForEachCoin([&hasher](const OutPoint& outpoint, const Coin& coin) {
        // Hash the outpoint
        DataStream ss;
        Serialize(ss, outpoint);
        hasher.Write(ss.data(), ss.size());
        
        // Hash the coin
        ss.clear();
        Serialize(ss, coin);
        hasher.Write(ss.data(), ss.size());
        
        return true;  // Continue iteration
    });
    
    Hash256 result;
    hasher.Finalize(result.data());
    return result;
}

UTXOStats GetUTXOStats(const CoinsViewDB& coinsView) {
    UTXOStats stats;
    stats.Reset();
    
    SHA256 hasher;
    
    std::map<TxHash, uint32_t> txCount;  // Count outputs per transaction
    
    coinsView.ForEachCoin([&](const OutPoint& outpoint, const Coin& coin) {
        ++stats.nTransactionOutputs;
        stats.nTotalAmount += coin.GetAmount();
        
        // Track unique transactions
        txCount[TxHash(outpoint.hash)]++;
        
        // Estimate serialized size
        DataStream ss;
        Serialize(ss, outpoint);
        Serialize(ss, coin);
        stats.nBogoSize += ss.size();
        
        // Update hash
        hasher.Write(ss.data(), ss.size());
        
        return true;
    });
    
    stats.nTransactions = txCount.size();
    stats.nDiskSize = coinsView.GetDiskUsage();
    hasher.Finalize(stats.hashSerialized.data());
    
    return stats;
}

} // namespace db
} // namespace shurium
