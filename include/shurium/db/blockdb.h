// SHURIUM - Block Database
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the block storage database for SHURIUM.
// Handles storage and retrieval of blocks and block metadata.

#ifndef SHURIUM_DB_BLOCKDB_H
#define SHURIUM_DB_BLOCKDB_H

#include "shurium/db/database.h"
#include "shurium/core/block.h"
#include "shurium/chain/blockindex.h"
#include "shurium/chain/chainstate.h"  // For BlockUndo
#include <memory>
#include <optional>
#include <filesystem>
#include <map>
#include <mutex>

namespace shurium {
namespace db {

// ============================================================================
// Block File Info - Metadata about block storage files
// ============================================================================

/**
 * Information about a block file (blk?????.dat).
 * Blocks are stored in sequential files, with each file having a maximum size.
 */
struct BlockFileInfo {
    /// Number of blocks stored in this file
    uint32_t nBlocks{0};
    
    /// File size in bytes (current)
    uint32_t nSize{0};
    
    /// Number of undo blocks stored in the corresponding rev?????.dat
    uint32_t nUndoSize{0};
    
    /// Lowest height of a block in this file
    int32_t nHeightFirst{0};
    
    /// Highest height of a block in this file
    int32_t nHeightLast{0};
    
    /// Timestamp of earliest block in this file
    uint64_t nTimeFirst{0};
    
    /// Timestamp of latest block in this file
    uint64_t nTimeLast{0};
    
    void AddBlock(int height, uint64_t time) {
        if (nBlocks == 0 || height < nHeightFirst) {
            nHeightFirst = height;
            nTimeFirst = time;
        }
        if (height > nHeightLast) {
            nHeightLast = height;
            nTimeLast = time;
        }
        ++nBlocks;
    }
    
    std::string ToString() const;
};

/// Serialization for BlockFileInfo
template<typename Stream>
void Serialize(Stream& s, const BlockFileInfo& info) {
    Serialize(s, info.nBlocks);
    Serialize(s, info.nSize);
    Serialize(s, info.nUndoSize);
    Serialize(s, info.nHeightFirst);
    Serialize(s, info.nHeightLast);
    Serialize(s, info.nTimeFirst);
    Serialize(s, info.nTimeLast);
}

template<typename Stream>
void Unserialize(Stream& s, BlockFileInfo& info) {
    Unserialize(s, info.nBlocks);
    Unserialize(s, info.nSize);
    Unserialize(s, info.nUndoSize);
    Unserialize(s, info.nHeightFirst);
    Unserialize(s, info.nHeightLast);
    Unserialize(s, info.nTimeFirst);
    Unserialize(s, info.nTimeLast);
}

// ============================================================================
// Block Disk Position - Location of a block on disk
// ============================================================================

/**
 * Position of a block (or undo data) on disk.
 */
struct DiskBlockPos {
    /// File number (blk?????.dat where ????? is the file number)
    int32_t nFile{-1};
    
    /// Byte offset within the file
    uint32_t nPos{0};
    
    DiskBlockPos() = default;
    DiskBlockPos(int32_t file, uint32_t pos) : nFile(file), nPos(pos) {}
    
    bool IsNull() const { return nFile < 0; }
    void SetNull() { nFile = -1; nPos = 0; }
    
    bool operator==(const DiskBlockPos& other) const {
        return nFile == other.nFile && nPos == other.nPos;
    }
    
    bool operator!=(const DiskBlockPos& other) const {
        return !(*this == other);
    }
    
    std::string ToString() const;
};

/// Serialization for DiskBlockPos
template<typename Stream>
void Serialize(Stream& s, const DiskBlockPos& pos) {
    Serialize(s, pos.nFile);
    Serialize(s, pos.nPos);
}

template<typename Stream>
void Unserialize(Stream& s, DiskBlockPos& pos) {
    Unserialize(s, pos.nFile);
    Unserialize(s, pos.nPos);
}

// ============================================================================
// Block Index Database Entry
// ============================================================================

/**
 * Database entry for a block index node.
 * This stores the on-disk metadata for each block we know about.
 */
struct BlockIndexDB {
    /// Block header
    BlockHeader header;
    
    /// Block height
    int32_t nHeight{0};
    
    /// Block status flags
    uint32_t nStatus{0};
    
    /// Number of transactions in the block
    uint32_t nTx{0};
    
    /// Position of block data on disk
    DiskBlockPos blockPos;
    
    /// Position of undo data on disk
    DiskBlockPos undoPos;
    
    /// Cumulative chain work to this point
    Hash256 nChainWork;
    
    BlockIndexDB() = default;
    
    explicit BlockIndexDB(const BlockIndex& index);
    
    /// Populate a BlockIndex from this entry
    void ToBlockIndex(BlockIndex& index) const;
};

/// Serialization for BlockIndexDB
template<typename Stream>
void Serialize(Stream& s, const BlockIndexDB& entry) {
    Serialize(s, entry.header);
    Serialize(s, entry.nHeight);
    Serialize(s, entry.nStatus);
    Serialize(s, entry.nTx);
    Serialize(s, entry.blockPos);
    Serialize(s, entry.undoPos);
    Serialize(s, entry.nChainWork);
}

template<typename Stream>
void Unserialize(Stream& s, BlockIndexDB& entry) {
    Unserialize(s, entry.header);
    Unserialize(s, entry.nHeight);
    Unserialize(s, entry.nStatus);
    Unserialize(s, entry.nTx);
    Unserialize(s, entry.blockPos);
    Unserialize(s, entry.undoPos);
    Unserialize(s, entry.nChainWork);
}

// ============================================================================
// Block Database - Main block storage interface
// ============================================================================

/**
 * Database for storing blocks and block index entries.
 * 
 * Block data is stored in flat files (blk?????.dat), while the index
 * and metadata are stored in the key-value database.
 */
class BlockDB {
private:
    /// Key-value database for index entries
    std::unique_ptr<Database> db_;
    
    /// Data directory for block files
    std::filesystem::path dataDir_;
    
    /// Current block file number
    int32_t nLastBlockFile_{0};
    
    /// Information about each block file
    std::vector<BlockFileInfo> blockFileInfo_;
    
    /// Maximum size of a block file (default 128MB)
    static constexpr uint64_t MAX_BLOCKFILE_SIZE = 128 * 1024 * 1024;
    
    /// File handle cache
    mutable std::map<int, FILE*> fileCache_;
    mutable std::mutex fileMutex_;
    
    // Helper functions
    std::filesystem::path GetBlockFilePath(int nFile) const;
    std::filesystem::path GetUndoFilePath(int nFile) const;
    FILE* OpenBlockFile(int nFile, bool fReadOnly) const;
    FILE* OpenUndoFile(int nFile, bool fReadOnly) const;
    void CloseAllFiles();
    
    // Allocate space in a block file
    bool AllocateBlockFile(uint32_t nAddSize, DiskBlockPos& pos);
    bool AllocateUndoFile(uint32_t nAddSize, DiskBlockPos& pos);
    
public:
    /**
     * Open or create a block database.
     * @param dataDir Data directory path
     * @param options Database options
     */
    BlockDB(const std::filesystem::path& dataDir, const Options& options = Options());
    
    ~BlockDB();
    
    // Prevent copies
    BlockDB(const BlockDB&) = delete;
    BlockDB& operator=(const BlockDB&) = delete;
    
    /// Check if database is open
    bool IsOpen() const { return db_ != nullptr; }
    
    // ========================================================================
    // Block Data Operations
    // ========================================================================
    
    /**
     * Write a block to disk.
     * @param block Block to write
     * @param pos Output: position where block was written
     * @return Status of the operation
     */
    Status WriteBlock(const Block& block, DiskBlockPos& pos);
    
    /**
     * Read a block from disk.
     * @param pos Position of the block
     * @param block Output: the block data
     * @return Status of the operation
     */
    Status ReadBlock(const DiskBlockPos& pos, Block& block) const;
    
    /**
     * Write undo data for a block.
     * @param undo Undo data
     * @param pos Output: position where undo was written
     * @return Status of the operation
     */
    Status WriteUndo(const BlockUndo& undo, DiskBlockPos& pos);
    
    /**
     * Read undo data for a block.
     * @param pos Position of the undo data
     * @param undo Output: the undo data
     * @return Status of the operation
     */
    Status ReadUndo(const DiskBlockPos& pos, BlockUndo& undo) const;
    
    // ========================================================================
    // Block Index Operations
    // ========================================================================
    
    /**
     * Write a block index entry.
     * @param hash Block hash
     * @param entry Index entry to write
     * @return Status of the operation
     */
    Status WriteBlockIndex(const BlockHash& hash, const BlockIndexDB& entry);
    
    /**
     * Read a block index entry.
     * @param hash Block hash
     * @param entry Output: index entry
     * @return Status of the operation
     */
    Status ReadBlockIndex(const BlockHash& hash, BlockIndexDB& entry) const;
    
    /**
     * Check if a block index entry exists.
     */
    bool HaveBlockIndex(const BlockHash& hash) const;
    
    /**
     * Load all block index entries into the given map.
     * @param blockIndex Map to populate
     * @return Number of entries loaded, or -1 on error
     */
    int LoadBlockIndexMap(BlockMap& blockIndex);
    
    /**
     * Write the best chain tip hash.
     */
    Status WriteBestChainTip(const BlockHash& hash);
    
    /**
     * Read the best chain tip hash.
     */
    std::optional<BlockHash> ReadBestChainTip() const;
    
    // ========================================================================
    // Block File Management
    // ========================================================================
    
    /**
     * Write block file info.
     */
    Status WriteBlockFileInfo(int nFile, const BlockFileInfo& info);
    
    /**
     * Read block file info.
     */
    Status ReadBlockFileInfo(int nFile, BlockFileInfo& info) const;
    
    /**
     * Write the last block file number.
     */
    Status WriteLastBlockFile(int nFile);
    
    /**
     * Read the last block file number.
     */
    std::optional<int> ReadLastBlockFile() const;
    
    /**
     * Get current block file info.
     */
    const std::vector<BlockFileInfo>& GetBlockFileInfo() const { return blockFileInfo_; }
    
    /**
     * Get total disk usage of block files.
     */
    uint64_t GetBlockDiskUsage() const;
    
    // ========================================================================
    // Batch Operations
    // ========================================================================
    
    /**
     * Start a batch write operation.
     */
    std::unique_ptr<db::WriteBatch> StartBatch();
    
    /**
     * Write a batch to the database.
     */
    Status WriteBatch(db::WriteBatch* batch, bool sync = false);
    
    // ========================================================================
    // Maintenance
    // ========================================================================
    
    /**
     * Flush pending writes to disk.
     */
    Status Flush();
    
    /**
     * Compact the database.
     */
    void Compact();
    
    /**
     * Get database statistics.
     */
    std::string GetStats() const;
};

// ============================================================================
// Transaction Index Database Entry
// ============================================================================

/**
 * Location of a transaction within a block.
 */
struct TxIndexEntry {
    /// Block position containing this transaction
    DiskBlockPos blockPos;
    
    /// Offset of transaction within the block
    uint32_t nTxOffset{0};
    
    TxIndexEntry() = default;
    TxIndexEntry(const DiskBlockPos& pos, uint32_t offset)
        : blockPos(pos), nTxOffset(offset) {}
};

/// Serialization for TxIndexEntry
template<typename Stream>
void Serialize(Stream& s, const TxIndexEntry& entry) {
    Serialize(s, entry.blockPos);
    Serialize(s, entry.nTxOffset);
}

template<typename Stream>
void Unserialize(Stream& s, TxIndexEntry& entry) {
    Unserialize(s, entry.blockPos);
    Unserialize(s, entry.nTxOffset);
}

// ============================================================================
// Transaction Index - Optional transaction lookup index
// ============================================================================

/**
 * Index for looking up transactions by txid.
 * This is optional and can be enabled for nodes that need transaction lookup.
 */
class TxIndex {
private:
    std::unique_ptr<Database> db_;
    bool enabled_{false};
    
public:
    /**
     * Open or create the transaction index.
     */
    TxIndex(const std::filesystem::path& dataDir, const Options& options = Options());
    
    ~TxIndex() = default;
    
    /// Check if index is enabled
    bool IsEnabled() const { return enabled_; }
    
    /// Enable/disable the index
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    
    /**
     * Index a transaction.
     */
    Status IndexTx(const TxHash& txid, const TxIndexEntry& entry);
    
    /**
     * Look up a transaction.
     */
    std::optional<TxIndexEntry> GetTx(const TxHash& txid) const;
    
    /**
     * Remove a transaction from the index.
     */
    Status RemoveTx(const TxHash& txid);
    
    /**
     * Index all transactions in a block.
     */
    Status IndexBlock(const Block& block, const DiskBlockPos& blockPos);
    
    /**
     * Remove all transactions in a block from the index.
     */
    Status UnindexBlock(const Block& block);
    
    /**
     * Get best indexed block hash.
     */
    std::optional<BlockHash> GetBestBlock() const;
    
    /**
     * Set best indexed block hash.
     */
    Status SetBestBlock(const BlockHash& hash);
};

} // namespace db
} // namespace shurium

#endif // SHURIUM_DB_BLOCKDB_H
