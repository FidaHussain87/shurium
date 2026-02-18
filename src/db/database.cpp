// SHURIUM - Database Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/db/database.h"
#include "shurium/db/leveldb.h"
#include <cstring>

namespace shurium {
namespace db {

// ============================================================================
// Database Factory Functions
// ============================================================================

std::pair<Status, std::unique_ptr<Database>> OpenDatabase(
    const std::filesystem::path& path,
    const Options& options)
{
#ifdef SHURIUM_USE_LEVELDB
    // Try to open with LevelDB
    leveldb::Options lo;
    lo.create_if_missing = options.create_if_missing;
    lo.error_if_exists = options.error_if_exists;
    lo.paranoid_checks = options.paranoid_checks;
    lo.write_buffer_size = options.write_buffer_size;
    lo.max_open_files = options.max_open_files;
    lo.block_size = options.block_size;
    
    // Create cache if requested
    leveldb::Cache* cache = nullptr;
    if (options.block_cache_size > 0) {
        cache = leveldb::NewLRUCache(options.block_cache_size);
        lo.block_cache = cache;
    }
    
    // Create bloom filter if requested
    const leveldb::FilterPolicy* filter = nullptr;
    if (options.bloom_filter_bits > 0) {
        filter = leveldb::NewBloomFilterPolicy(options.bloom_filter_bits);
        lo.filter_policy = filter;
    }
    
    // Set compression
    lo.compression = options.compression ? 
        leveldb::kSnappyCompression : leveldb::kNoCompression;
    
    // Open the database
    leveldb::DB* db = nullptr;
    leveldb::Status s = leveldb::DB::Open(lo, path.string(), &db);
    
    if (!s.ok()) {
        // Clean up on failure
        delete cache;
        delete filter;
        
        if (s.IsCorruption()) {
            return {Status::Corruption(s.ToString()), nullptr};
        } else if (s.IsIOError()) {
            return {Status::IOError(s.ToString()), nullptr};
        } else if (s.IsInvalidArgument()) {
            return {Status::InvalidArgument(s.ToString()), nullptr};
        }
        return {Status::IOError(s.ToString()), nullptr};
    }
    
    return {Status::Ok(), 
            std::make_unique<LevelDBDatabase>(db, cache, filter, path)};
            
#else
    // Fallback to in-memory database
    // Create directory if it doesn't exist (for consistency)
    if (options.create_if_missing) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
    }
    
    return {Status::Ok(), std::make_unique<MemoryDatabase>()};
#endif
}

Status DestroyDatabase(const std::filesystem::path& path) {
#ifdef SHURIUM_USE_LEVELDB
    leveldb::Status s = leveldb::DestroyDB(path.string(), leveldb::Options());
    if (!s.ok()) {
        return Status::IOError(s.ToString());
    }
    return Status::Ok();
#else
    // For memory database, just try to remove the directory
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec) {
        return Status::IOError(ec.message());
    }
    return Status::Ok();
#endif
}

Status RepairDatabase(const std::filesystem::path& path) {
#ifdef SHURIUM_USE_LEVELDB
    leveldb::Status s = leveldb::RepairDB(path.string(), leveldb::Options());
    if (!s.ok()) {
        return Status::IOError(s.ToString());
    }
    return Status::Ok();
#else
    // Nothing to repair for memory database
    return Status::Ok();
#endif
}

} // namespace db
} // namespace shurium
