// SHURIUM - UTXO Database
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the persistent UTXO database for SHURIUM.
// Provides a CoinsView backed by the key-value database.

#ifndef SHURIUM_DB_UTXODB_H
#define SHURIUM_DB_UTXODB_H

#include "shurium/db/database.h"
#include "shurium/chain/coins.h"
#include <memory>
#include <filesystem>
#include <atomic>

namespace shurium {
namespace db {

// ============================================================================
// CoinsViewDB - Persistent UTXO database
// ============================================================================

/**
 * CoinsView backed by LevelDB for persistent UTXO storage.
 * 
 * This is the primary implementation for storing the UTXO set on disk.
 * It implements the CoinsView interface and can be used as the base
 * for CoinsViewCache.
 */
class CoinsViewDB : public CoinsView {
private:
    /// The underlying database
    std::unique_ptr<Database> db_;
    
    /// Path to the database
    std::filesystem::path dbPath_;
    
    /// Best block hash (cached)
    mutable BlockHash cachedBestBlock_;
    mutable bool cachedBestBlockValid_{false};
    
    /// Statistics
    mutable std::atomic<uint64_t> nReads_{0};
    mutable std::atomic<uint64_t> nWrites_{0};
    mutable std::atomic<uint64_t> nReadBytes_{0};
    mutable std::atomic<uint64_t> nWriteBytes_{0};
    
public:
    /**
     * Open or create the UTXO database.
     * @param dbPath Path to the database directory
     * @param options Database options
     * @param wipe If true, delete existing data
     */
    CoinsViewDB(const std::filesystem::path& dbPath, 
                const Options& options = Options(),
                bool wipe = false);
    
    ~CoinsViewDB() override = default;
    
    // Prevent copies
    CoinsViewDB(const CoinsViewDB&) = delete;
    CoinsViewDB& operator=(const CoinsViewDB&) = delete;
    
    // ========================================================================
    // CoinsView Interface
    // ========================================================================
    
    std::optional<Coin> GetCoin(const OutPoint& outpoint) const override;
    bool HaveCoin(const OutPoint& outpoint) const override;
    BlockHash GetBestBlock() const override;
    size_t EstimateSize() const override;
    
    // ========================================================================
    // Write Operations
    // ========================================================================
    
    /**
     * Write a batch of coin changes to the database.
     * This is the primary method for updating the UTXO set.
     * 
     * @param mapCoins Map of outpoints to coin cache entries
     * @param hashBlock The block hash this state corresponds to
     * @return true if successful
     */
    bool BatchWrite(CoinsMap& mapCoins, const BlockHash& hashBlock);
    
    /**
     * Add a single coin to the database.
     */
    Status AddCoin(const OutPoint& outpoint, const Coin& coin);
    
    /**
     * Remove a coin from the database.
     */
    Status RemoveCoin(const OutPoint& outpoint);
    
    /**
     * Set the best block hash.
     */
    Status SetBestBlock(const BlockHash& hash);
    
    // ========================================================================
    // Iteration
    // ========================================================================
    
    /**
     * Create an iterator over all coins.
     */
    std::unique_ptr<Iterator> NewIterator() const;
    
    /**
     * Iterate over all coins with a callback.
     * @param func Callback function (outpoint, coin) -> bool (continue?)
     * @return Number of coins iterated
     */
    template<typename Func>
    size_t ForEachCoin(Func&& func) const {
        size_t count = 0;
        auto iter = NewIterator();
        std::string prefix(1, prefix::COIN);
        iter->Seek(Slice(prefix));
        
        while (iter->Valid()) {
            Slice key = iter->key();
            if (key.size() < 1 || key[0] != prefix::COIN) {
                break;
            }
            
            OutPoint outpoint;
            if (!DeserializeFromString(key.ToString().substr(1), outpoint)) {
                continue;
            }
            
            Coin coin;
            if (!DeserializeFromString(iter->value().ToString(), coin)) {
                continue;
            }
            
            ++count;
            if (!func(outpoint, coin)) {
                break;
            }
            
            iter->Next();
        }
        
        return count;
    }
    
    // ========================================================================
    // Statistics and Maintenance
    // ========================================================================
    
    /**
     * Get read statistics.
     */
    uint64_t GetReadCount() const { return nReads_; }
    uint64_t GetReadBytes() const { return nReadBytes_; }
    
    /**
     * Get write statistics.
     */
    uint64_t GetWriteCount() const { return nWrites_; }
    uint64_t GetWriteBytes() const { return nWriteBytes_; }
    
    /**
     * Reset statistics.
     */
    void ResetStats() {
        nReads_ = 0;
        nWrites_ = 0;
        nReadBytes_ = 0;
        nWriteBytes_ = 0;
    }
    
    /**
     * Compact the database.
     */
    void Compact() { if (db_) db_->Compact(); }
    
    /**
     * Get database disk usage.
     */
    uint64_t GetDiskUsage() const { return db_ ? db_->GetDiskUsage() : 0; }
    
    /**
     * Get database statistics string.
     */
    std::string GetStats() const { return db_ ? db_->GetStats() : ""; }
    
    /**
     * Check if database is open.
     */
    bool IsOpen() const { return db_ != nullptr; }
};

// ============================================================================
// UTXO Set Hash Calculation
// ============================================================================

/**
 * Calculate a hash of the entire UTXO set for verification.
 * This iterates over all coins and computes a cumulative hash.
 * 
 * @param coinsView The coins view to hash
 * @return Hash of the UTXO set
 */
Hash256 CalculateUTXOSetHash(const CoinsViewDB& coinsView);

/**
 * Get comprehensive UTXO statistics.
 */
UTXOStats GetUTXOStats(const CoinsViewDB& coinsView);

} // namespace db
} // namespace shurium

#endif // SHURIUM_DB_UTXODB_H
