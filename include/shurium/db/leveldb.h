// SHURIUM - LevelDB Wrapper
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file provides the LevelDB implementation of the database interface.

#ifndef SHURIUM_DB_LEVELDB_H
#define SHURIUM_DB_LEVELDB_H

#include "shurium/db/database.h"
#include <map>
#include <mutex>

#ifdef SHURIUM_USE_LEVELDB
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>
#endif

namespace shurium {
namespace db {

// ============================================================================
// LevelDB Iterator Wrapper
// ============================================================================

#ifdef SHURIUM_USE_LEVELDB

class LevelDBIterator : public Iterator {
private:
    std::unique_ptr<leveldb::Iterator> iter_;
    
public:
    explicit LevelDBIterator(leveldb::Iterator* iter) : iter_(iter) {}
    
    bool Valid() const override { return iter_->Valid(); }
    
    void SeekToFirst() override { iter_->SeekToFirst(); }
    void SeekToLast() override { iter_->SeekToLast(); }
    void Seek(const Slice& target) override {
        iter_->Seek(leveldb::Slice(target.data(), target.size()));
    }
    
    void Next() override { iter_->Next(); }
    void Prev() override { iter_->Prev(); }
    
    Slice key() const override {
        leveldb::Slice k = iter_->key();
        return Slice(k.data(), k.size());
    }
    
    Slice value() const override {
        leveldb::Slice v = iter_->value();
        return Slice(v.data(), v.size());
    }
    
    Status status() const override {
        leveldb::Status s = iter_->status();
        if (s.ok()) return Status::Ok();
        if (s.IsNotFound()) return Status::NotFound(s.ToString());
        if (s.IsCorruption()) return Status::Corruption(s.ToString());
        if (s.IsIOError()) return Status::IOError(s.ToString());
        return Status::IOError(s.ToString());
    }
};

// ============================================================================
// LevelDB Database Implementation
// ============================================================================

class LevelDBDatabase : public Database {
private:
    std::unique_ptr<leveldb::DB> db_;
    std::unique_ptr<leveldb::Cache> cache_;
    std::unique_ptr<const leveldb::FilterPolicy> filter_policy_;
    leveldb::ReadOptions default_read_options_;
    leveldb::WriteOptions default_write_options_;
    std::filesystem::path path_;
    
    static leveldb::Status ConvertStatus(const Status& s) {
        // Not needed - we convert the other direction
        return leveldb::Status::OK();
    }
    
    static Status ConvertStatus(const leveldb::Status& s) {
        if (s.ok()) return Status::Ok();
        if (s.IsNotFound()) return Status::NotFound(s.ToString());
        if (s.IsCorruption()) return Status::Corruption(s.ToString());
        if (s.IsIOError()) return Status::IOError(s.ToString());
        if (s.IsNotSupportedError()) return Status::NotSupported(s.ToString());
        if (s.IsInvalidArgument()) return Status::InvalidArgument(s.ToString());
        return Status::IOError(s.ToString());
    }
    
    leveldb::ReadOptions MakeReadOptions(const ReadOptions& opts) const {
        leveldb::ReadOptions lo;
        lo.verify_checksums = opts.verify_checksums;
        lo.fill_cache = opts.fill_cache;
        lo.snapshot = static_cast<const leveldb::Snapshot*>(opts.snapshot);
        return lo;
    }
    
    leveldb::WriteOptions MakeWriteOptions(const WriteOptions& opts) const {
        leveldb::WriteOptions lo;
        lo.sync = opts.sync;
        return lo;
    }
    
public:
    LevelDBDatabase(leveldb::DB* db, leveldb::Cache* cache, 
                    const leveldb::FilterPolicy* filter,
                    const std::filesystem::path& path)
        : db_(db), cache_(cache), filter_policy_(filter), path_(path) {}
    
    ~LevelDBDatabase() override {
        // Close in correct order
        db_.reset();
        cache_.reset();
        filter_policy_.reset();
    }
    
    Status Get(const ReadOptions& options, const Slice& key, std::string* value) override {
        leveldb::ReadOptions lo = MakeReadOptions(options);
        leveldb::Slice lkey(key.data(), key.size());
        return ConvertStatus(db_->Get(lo, lkey, value));
    }
    
    Status Put(const WriteOptions& options, const Slice& key, const Slice& value) override {
        leveldb::WriteOptions lo = MakeWriteOptions(options);
        leveldb::Slice lkey(key.data(), key.size());
        leveldb::Slice lval(value.data(), value.size());
        return ConvertStatus(db_->Put(lo, lkey, lval));
    }
    
    Status Delete(const WriteOptions& options, const Slice& key) override {
        leveldb::WriteOptions lo = MakeWriteOptions(options);
        leveldb::Slice lkey(key.data(), key.size());
        return ConvertStatus(db_->Delete(lo, lkey));
    }
    
    Status Write(const WriteOptions& options, WriteBatch* batch) override {
        leveldb::WriteBatch lb;
        batch->Iterate([&lb](const std::string& key, const std::optional<std::string>& value) {
            if (value) {
                lb.Put(key, *value);
            } else {
                lb.Delete(key);
            }
        });
        leveldb::WriteOptions lo = MakeWriteOptions(options);
        return ConvertStatus(db_->Write(lo, &lb));
    }
    
    std::unique_ptr<Iterator> NewIterator(const ReadOptions& options) override {
        leveldb::ReadOptions lo = MakeReadOptions(options);
        return std::make_unique<LevelDBIterator>(db_->NewIterator(lo));
    }
    
    void Compact() override {
        db_->CompactRange(nullptr, nullptr);
    }
    
    uint64_t GetDiskUsage() const override {
        uint64_t size = 0;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path_)) {
                if (entry.is_regular_file()) {
                    size += entry.file_size();
                }
            }
        } catch (...) {
            // Ignore errors
        }
        return size;
    }
    
    std::string GetStats() const override {
        std::string stats;
        db_->GetProperty("leveldb.stats", &stats);
        return stats;
    }
};

#endif // SHURIUM_USE_LEVELDB

// ============================================================================
// In-Memory Database (Fallback when LevelDB not available)
// ============================================================================

/**
 * Simple in-memory database for testing or when LevelDB is not available.
 */
class MemoryDatabase : public Database {
private:
    std::map<std::string, std::string> data_;
    mutable std::mutex mutex_;
    
public:
    MemoryDatabase() = default;
    
    Status Get(const ReadOptions& options, const Slice& key, std::string* value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = data_.find(key.ToString());
        if (it == data_.end()) {
            return Status::NotFound();
        }
        *value = it->second;
        return Status::Ok();
    }
    
    Status Put(const WriteOptions& options, const Slice& key, const Slice& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key.ToString()] = value.ToString();
        return Status::Ok();
    }
    
    Status Delete(const WriteOptions& options, const Slice& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.erase(key.ToString());
        return Status::Ok();
    }
    
    Status Write(const WriteOptions& options, WriteBatch* batch) override {
        std::lock_guard<std::mutex> lock(mutex_);
        batch->Iterate([this](const std::string& key, const std::optional<std::string>& value) {
            if (value) {
                data_[key] = *value;
            } else {
                data_.erase(key);
            }
        });
        return Status::Ok();
    }
    
    std::unique_ptr<Iterator> NewIterator(const ReadOptions& options) override;
    
    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.size();
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.clear();
    }
    
    // For iterator access
    const std::map<std::string, std::string>& GetData() const { return data_; }
};

/**
 * Iterator for MemoryDatabase.
 */
class MemoryIterator : public Iterator {
private:
    const std::map<std::string, std::string>& data_;
    std::map<std::string, std::string>::const_iterator iter_;
    bool valid_;
    
public:
    explicit MemoryIterator(const std::map<std::string, std::string>& data)
        : data_(data), iter_(data_.end()), valid_(false) {}
    
    bool Valid() const override { return valid_ && iter_ != data_.end(); }
    
    void SeekToFirst() override {
        iter_ = data_.begin();
        valid_ = (iter_ != data_.end());
    }
    
    void SeekToLast() override {
        if (data_.empty()) {
            iter_ = data_.end();
            valid_ = false;
        } else {
            iter_ = std::prev(data_.end());
            valid_ = true;
        }
    }
    
    void Seek(const Slice& target) override {
        iter_ = data_.lower_bound(target.ToString());
        valid_ = (iter_ != data_.end());
    }
    
    void Next() override {
        if (valid_ && iter_ != data_.end()) {
            ++iter_;
            valid_ = (iter_ != data_.end());
        }
    }
    
    void Prev() override {
        if (iter_ == data_.begin()) {
            valid_ = false;
        } else if (iter_ != data_.end()) {
            --iter_;
            valid_ = true;
        }
    }
    
    Slice key() const override {
        return Slice(iter_->first);
    }
    
    Slice value() const override {
        return Slice(iter_->second);
    }
    
    Status status() const override {
        return Status::Ok();
    }
};

inline std::unique_ptr<Iterator> MemoryDatabase::NewIterator(const ReadOptions& options) {
    return std::make_unique<MemoryIterator>(data_);
}

} // namespace db
} // namespace shurium

#endif // SHURIUM_DB_LEVELDB_H
