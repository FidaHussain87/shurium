// SHURIUM - Database Abstraction Layer
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the abstract database interface for SHURIUM.
// Implementations can use LevelDB, RocksDB, or other key-value stores.

#ifndef SHURIUM_DB_DATABASE_H
#define SHURIUM_DB_DATABASE_H

#include "shurium/core/types.h"
#include "shurium/core/serialize.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <filesystem>

namespace shurium {
namespace db {

// ============================================================================
// Database Status - Result of database operations
// ============================================================================

/**
 * Status returned by database operations.
 */
class Status {
public:
    enum Code {
        OK = 0,
        NOT_FOUND = 1,
        CORRUPTION = 2,
        NOT_SUPPORTED = 3,
        INVALID_ARGUMENT = 4,
        IO_ERROR = 5,
    };
    
private:
    Code code_;
    std::string message_;
    
public:
    Status() : code_(OK) {}
    Status(Code code, const std::string& msg = "") : code_(code), message_(msg) {}
    
    static Status Ok() { return Status(); }
    static Status NotFound(const std::string& msg = "") { return Status(NOT_FOUND, msg); }
    static Status Corruption(const std::string& msg = "") { return Status(CORRUPTION, msg); }
    static Status NotSupported(const std::string& msg = "") { return Status(NOT_SUPPORTED, msg); }
    static Status InvalidArgument(const std::string& msg = "") { return Status(INVALID_ARGUMENT, msg); }
    static Status IOError(const std::string& msg = "") { return Status(IO_ERROR, msg); }
    
    bool ok() const { return code_ == OK; }
    bool IsNotFound() const { return code_ == NOT_FOUND; }
    bool IsCorruption() const { return code_ == CORRUPTION; }
    bool IsIOError() const { return code_ == IO_ERROR; }
    
    Code code() const { return code_; }
    const std::string& message() const { return message_; }
    
    std::string ToString() const {
        if (ok()) return "OK";
        std::string result;
        switch (code_) {
            case NOT_FOUND: result = "NotFound: "; break;
            case CORRUPTION: result = "Corruption: "; break;
            case NOT_SUPPORTED: result = "NotSupported: "; break;
            case INVALID_ARGUMENT: result = "InvalidArgument: "; break;
            case IO_ERROR: result = "IOError: "; break;
            default: result = "Unknown: "; break;
        }
        return result + message_;
    }
};

// ============================================================================
// Slice - A reference to a byte range (like std::string_view for bytes)
// ============================================================================

/**
 * A lightweight reference to a contiguous range of bytes.
 * Does not own the data - the underlying buffer must outlive the Slice.
 */
class Slice {
private:
    const char* data_;
    size_t size_;
    
public:
    Slice() : data_(nullptr), size_(0) {}
    Slice(const char* d, size_t n) : data_(d), size_(n) {}
    Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}
    Slice(const std::vector<uint8_t>& v) 
        : data_(reinterpret_cast<const char*>(v.data())), size_(v.size()) {}
    Slice(const char* s) : data_(s), size_(strlen(s)) {}
    
    const char* data() const { return data_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    
    char operator[](size_t n) const { return data_[n]; }
    
    void clear() { data_ = nullptr; size_ = 0; }
    
    std::string ToString() const { return std::string(data_, size_); }
    std::vector<uint8_t> ToVector() const {
        return std::vector<uint8_t>(data_, data_ + size_);
    }
    
    int compare(const Slice& b) const {
        size_t min_len = std::min(size_, b.size_);
        int r = memcmp(data_, b.data_, min_len);
        if (r == 0) {
            if (size_ < b.size_) r = -1;
            else if (size_ > b.size_) r = +1;
        }
        return r;
    }
    
    bool operator==(const Slice& b) const {
        return size_ == b.size_ && memcmp(data_, b.data_, size_) == 0;
    }
    
    bool operator!=(const Slice& b) const { return !(*this == b); }
    bool operator<(const Slice& b) const { return compare(b) < 0; }
};

// ============================================================================
// Database Options
// ============================================================================

/**
 * Options for opening a database.
 */
struct Options {
    /// Create the database if it doesn't exist
    bool create_if_missing = true;
    
    /// Throw error if database already exists
    bool error_if_exists = false;
    
    /// Enable paranoid checks
    bool paranoid_checks = false;
    
    /// Write buffer size (default 4MB)
    size_t write_buffer_size = 4 * 1024 * 1024;
    
    /// Maximum number of open files
    int max_open_files = 1000;
    
    /// Block size for data blocks (default 4KB)
    size_t block_size = 4 * 1024;
    
    /// LRU cache size for blocks (default 8MB)
    size_t block_cache_size = 8 * 1024 * 1024;
    
    /// Compression enabled
    bool compression = true;
    
    /// Bloom filter bits per key (0 to disable)
    int bloom_filter_bits = 10;
};

/**
 * Options for read operations.
 */
struct ReadOptions {
    /// Verify checksums on reads
    bool verify_checksums = false;
    
    /// Fill the cache on reads
    bool fill_cache = true;
    
    /// Read from a specific snapshot (nullptr for current)
    const void* snapshot = nullptr;
};

/**
 * Options for write operations.
 */
struct WriteOptions {
    /// Sync write to disk before returning
    bool sync = false;
};

// ============================================================================
// WriteBatch - Atomic batch of write operations
// ============================================================================

/**
 * A batch of write operations to be applied atomically.
 */
class WriteBatch {
private:
    std::vector<std::pair<std::string, std::optional<std::string>>> operations_;
    
public:
    WriteBatch() = default;
    
    /// Put a key-value pair
    void Put(const Slice& key, const Slice& value) {
        operations_.emplace_back(key.ToString(), value.ToString());
    }
    
    /// Delete a key
    void Delete(const Slice& key) {
        operations_.emplace_back(key.ToString(), std::nullopt);
    }
    
    /// Clear all operations
    void Clear() { operations_.clear(); }
    
    /// Get number of operations
    size_t Count() const { return operations_.size(); }
    
    /// Check if empty
    bool Empty() const { return operations_.empty(); }
    
    /// Iterate over operations
    template<typename Func>
    void Iterate(Func&& func) const {
        for (const auto& [key, value] : operations_) {
            func(key, value);
        }
    }
    
    /// Get approximate size of batch
    size_t ApproximateSize() const {
        size_t size = 0;
        for (const auto& [key, value] : operations_) {
            size += key.size();
            if (value) size += value->size();
        }
        return size;
    }
};

// ============================================================================
// Iterator - Database iterator interface
// ============================================================================

/**
 * Iterator for traversing database contents.
 */
class Iterator {
public:
    virtual ~Iterator() = default;
    
    /// Check if iterator is valid
    virtual bool Valid() const = 0;
    
    /// Seek to the first key
    virtual void SeekToFirst() = 0;
    
    /// Seek to the last key
    virtual void SeekToLast() = 0;
    
    /// Seek to the first key >= target
    virtual void Seek(const Slice& target) = 0;
    
    /// Move to the next key
    virtual void Next() = 0;
    
    /// Move to the previous key
    virtual void Prev() = 0;
    
    /// Get the current key
    virtual Slice key() const = 0;
    
    /// Get the current value
    virtual Slice value() const = 0;
    
    /// Get the status of the iterator
    virtual Status status() const = 0;
};

// ============================================================================
// Database - Abstract database interface
// ============================================================================

/**
 * Abstract interface for a key-value database.
 */
class Database {
public:
    virtual ~Database() = default;
    
    /// Get a value by key
    virtual Status Get(const ReadOptions& options, const Slice& key, std::string* value) = 0;
    
    /// Convenience get with default options
    Status Get(const Slice& key, std::string* value) {
        return Get(ReadOptions(), key, value);
    }
    
    /// Put a key-value pair
    virtual Status Put(const WriteOptions& options, const Slice& key, const Slice& value) = 0;
    
    /// Convenience put with default options
    Status Put(const Slice& key, const Slice& value) {
        return Put(WriteOptions(), key, value);
    }
    
    /// Delete a key
    virtual Status Delete(const WriteOptions& options, const Slice& key) = 0;
    
    /// Convenience delete with default options
    Status Delete(const Slice& key) {
        return Delete(WriteOptions(), key);
    }
    
    /// Apply a batch of writes atomically
    virtual Status Write(const WriteOptions& options, WriteBatch* batch) = 0;
    
    /// Convenience write with default options
    Status Write(WriteBatch* batch) {
        return Write(WriteOptions(), batch);
    }
    
    /// Create an iterator
    virtual std::unique_ptr<Iterator> NewIterator(const ReadOptions& options) = 0;
    
    /// Convenience iterator with default options
    std::unique_ptr<Iterator> NewIterator() {
        return NewIterator(ReadOptions());
    }
    
    /// Check if a key exists
    virtual bool Exists(const Slice& key) {
        std::string value;
        Status s = Get(key, &value);
        return s.ok();
    }
    
    /// Compact the database
    virtual void Compact() {}
    
    /// Sync to disk
    virtual Status Sync() { return Status::Ok(); }
    
    /// Get approximate disk usage
    virtual uint64_t GetDiskUsage() const { return 0; }
    
    /// Get database statistics
    virtual std::string GetStats() const { return ""; }
};

// ============================================================================
// Database Factory Functions
// ============================================================================

/**
 * Open a database at the specified path.
 * @param path Path to the database directory
 * @param options Database options
 * @return Pair of (status, database pointer)
 */
std::pair<Status, std::unique_ptr<Database>> OpenDatabase(
    const std::filesystem::path& path,
    const Options& options = Options());

/**
 * Destroy a database (delete all data).
 * @param path Path to the database directory
 * @return Status of the operation
 */
Status DestroyDatabase(const std::filesystem::path& path);

/**
 * Repair a damaged database.
 * @param path Path to the database directory
 * @return Status of the operation
 */
Status RepairDatabase(const std::filesystem::path& path);

// ============================================================================
// Serialization Helpers
// ============================================================================

/**
 * Serialize an object to a byte string using our serialization framework.
 */
template<typename T>
std::string SerializeToString(const T& obj) {
    DataStream ss;
    Serialize(ss, obj);
    // Convert DataStream to string
    return std::string(reinterpret_cast<const char*>(ss.data()), ss.size());
}

/**
 * Deserialize an object from a byte string.
 */
template<typename T>
bool DeserializeFromString(const std::string& data, T& obj) {
    try {
        DataStream ss(reinterpret_cast<const uint8_t*>(data.data()), data.size());
        Unserialize(ss, obj);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Key Prefixes for Database Namespacing
// ============================================================================

namespace prefix {
    // Block storage
    constexpr char BLOCK = 'b';           // block hash -> block data
    constexpr char BLOCK_INDEX = 'B';     // block hash -> block index entry
    constexpr char BLOCK_FILE = 'f';      // file number -> file info
    constexpr char LAST_BLOCK_FILE = 'l'; // -> last block file number
    
    // UTXO set
    constexpr char COIN = 'C';            // outpoint -> coin
    constexpr char COINS_TIP = 'c';       // -> best block hash for coins
    
    // Transaction index
    constexpr char TX_INDEX = 't';        // txid -> block location
    
    // Chain state
    constexpr char BEST_CHAIN = 'H';      // -> hash of best chain tip
    constexpr char FLAG = 'F';            // name -> flag value
    constexpr char REINDEX = 'R';         // -> reindexing flag
    
    // Address index (optional)
    constexpr char ADDRESS = 'a';         // address -> tx list
    
    // Spent index (optional)  
    constexpr char SPENT = 's';           // outpoint -> spending tx info
}

/**
 * Create a prefixed database key.
 */
inline std::string MakeKey(char prefix, const Slice& key) {
    std::string result;
    result.reserve(1 + key.size());
    result.push_back(prefix);
    result.append(key.data(), key.size());
    return result;
}

inline std::string MakeKey(char prefix) {
    return std::string(1, prefix);
}

template<typename T>
std::string MakeKey(char prefix, const T& obj) {
    std::string result;
    result.push_back(prefix);
    DataStream ss;
    Serialize(ss, obj);
    result.append(reinterpret_cast<const char*>(ss.data()), ss.size());
    return result;
}

} // namespace db
} // namespace shurium

#endif // SHURIUM_DB_DATABASE_H
