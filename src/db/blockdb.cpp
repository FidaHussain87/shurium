// SHURIUM - Block Database Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/db/blockdb.h"
#include <sstream>
#include <cstdio>
#include <iomanip>

namespace shurium {
namespace db {

// ============================================================================
// BlockFileInfo Implementation
// ============================================================================

std::string BlockFileInfo::ToString() const {
    std::ostringstream ss;
    ss << "BlockFileInfo("
       << "nBlocks=" << nBlocks
       << ", nSize=" << nSize
       << ", nUndoSize=" << nUndoSize
       << ", heights=" << nHeightFirst << "-" << nHeightLast
       << ")";
    return ss.str();
}

// ============================================================================
// DiskBlockPos Implementation
// ============================================================================

std::string DiskBlockPos::ToString() const {
    std::ostringstream ss;
    ss << "DiskBlockPos(file=" << nFile << ", pos=" << nPos << ")";
    return ss.str();
}

// ============================================================================
// BlockIndexDB Implementation
// ============================================================================

BlockIndexDB::BlockIndexDB(const BlockIndex& index) {
    header.nVersion = index.nVersion;
    header.hashPrevBlock = index.pprev ? index.pprev->GetBlockHash() : BlockHash();
    header.hashMerkleRoot = index.hashMerkleRoot;
    header.nTime = index.nTime;
    header.nBits = index.nBits;
    header.nNonce = index.nNonce;
    
    nHeight = index.nHeight;
    nStatus = static_cast<uint32_t>(index.nStatus);
    nTx = index.nTx;
    
    // Block and undo positions would be set separately
    blockPos.nFile = index.nFile;
    blockPos.nPos = index.nDataPos;
    undoPos.nFile = index.nFile;
    undoPos.nPos = index.nUndoPos;
    // nChainWork would be set separately
}

void BlockIndexDB::ToBlockIndex(BlockIndex& index) const {
    index.nVersion = header.nVersion;
    // Note: pprev needs to be set by caller after loading all entries
    index.hashMerkleRoot = header.hashMerkleRoot;
    index.nTime = header.nTime;
    index.nBits = header.nBits;
    index.nNonce = header.nNonce;
    
    index.nHeight = nHeight;
    index.nStatus = static_cast<BlockStatus>(nStatus);
    index.nTx = nTx;
    
    index.nFile = blockPos.nFile;
    index.nDataPos = blockPos.nPos;
    index.nUndoPos = undoPos.nPos;
    
    // Note: pprev, pskip, nChainWork need to be set by caller
}

// ============================================================================
// BlockDB Implementation
// ============================================================================

BlockDB::BlockDB(const std::filesystem::path& dataDir, const Options& options)
    : dataDir_(dataDir)
{
    // Ensure data directory exists
    std::error_code ec;
    std::filesystem::create_directories(dataDir_, ec);
    
    // Open the block index database
    std::filesystem::path dbPath = dataDir_ / "blocks" / "index";
    std::filesystem::create_directories(dbPath, ec);
    
    auto [status, database] = OpenDatabase(dbPath, options);
    if (!status.ok()) {
        throw std::runtime_error("Failed to open block database: " + status.ToString());
    }
    db_ = std::move(database);
    
    // Create blocks directory if needed
    std::filesystem::create_directories(dataDir_ / "blocks", ec);
    
    // Load last block file number
    auto lastFile = ReadLastBlockFile();
    if (lastFile) {
        nLastBlockFile_ = *lastFile;
    }
    
    // Load block file info
    blockFileInfo_.resize(nLastBlockFile_ + 1);
    for (int i = 0; i <= nLastBlockFile_; ++i) {
        BlockFileInfo info;
        if (ReadBlockFileInfo(i, info).ok()) {
            blockFileInfo_[i] = info;
        }
    }
}

BlockDB::~BlockDB() {
    CloseAllFiles();
}

// ============================================================================
// File Path Helpers
// ============================================================================

std::filesystem::path BlockDB::GetBlockFilePath(int nFile) const {
    std::ostringstream ss;
    ss << "blk" << std::setfill('0') << std::setw(5) << nFile << ".dat";
    return dataDir_ / "blocks" / ss.str();
}

std::filesystem::path BlockDB::GetUndoFilePath(int nFile) const {
    std::ostringstream ss;
    ss << "rev" << std::setfill('0') << std::setw(5) << nFile << ".dat";
    return dataDir_ / "blocks" / ss.str();
}

FILE* BlockDB::OpenBlockFile(int nFile, bool fReadOnly) const {
    std::lock_guard<std::mutex> lock(fileMutex_);
    
    // Check cache first
    auto it = fileCache_.find(nFile);
    if (it != fileCache_.end() && it->second != nullptr) {
        // Flush and return cached file (even for reads - ensures data is written)
        std::fflush(it->second);
        return it->second;
    }
    
    std::filesystem::path path = GetBlockFilePath(nFile);
    const char* mode = fReadOnly ? "rb" : "rb+";
    
    FILE* file = std::fopen(path.c_str(), mode);
    if (!file && !fReadOnly) {
        // Try creating the file
        file = std::fopen(path.c_str(), "wb+");
    }
    
    if (file && !fReadOnly) {
        fileCache_[nFile] = file;
    }
    
    return file;
}

FILE* BlockDB::OpenUndoFile(int nFile, bool fReadOnly) const {
    std::filesystem::path path = GetUndoFilePath(nFile);
    const char* mode = fReadOnly ? "rb" : "rb+";
    
    FILE* file = std::fopen(path.c_str(), mode);
    if (!file && !fReadOnly) {
        file = std::fopen(path.c_str(), "wb+");
    }
    
    return file;
}

void BlockDB::CloseAllFiles() {
    std::lock_guard<std::mutex> lock(fileMutex_);
    for (auto& [nFile, file] : fileCache_) {
        if (file) {
            std::fclose(file);
        }
    }
    fileCache_.clear();
}

// ============================================================================
// Block File Allocation
// ============================================================================

bool BlockDB::AllocateBlockFile(uint32_t nAddSize, DiskBlockPos& pos) {
    // Check if we need a new file
    if (nLastBlockFile_ >= 0 && 
        static_cast<int>(blockFileInfo_.size()) > nLastBlockFile_) {
        uint64_t currentSize = blockFileInfo_[nLastBlockFile_].nSize;
        if (currentSize + nAddSize > MAX_BLOCKFILE_SIZE) {
            // Need new file
            ++nLastBlockFile_;
            if (static_cast<int>(blockFileInfo_.size()) <= nLastBlockFile_) {
                blockFileInfo_.resize(nLastBlockFile_ + 1);
            }
            WriteLastBlockFile(nLastBlockFile_);
        }
    } else if (nLastBlockFile_ < 0) {
        nLastBlockFile_ = 0;
        blockFileInfo_.resize(1);
        WriteLastBlockFile(0);
    }
    
    pos.nFile = nLastBlockFile_;
    pos.nPos = blockFileInfo_[nLastBlockFile_].nSize;
    blockFileInfo_[nLastBlockFile_].nSize += nAddSize;
    
    return true;
}

bool BlockDB::AllocateUndoFile(uint32_t nAddSize, DiskBlockPos& pos) {
    // Undo files correspond to block files
    if (nLastBlockFile_ < 0) {
        return false;
    }
    
    pos.nFile = nLastBlockFile_;
    pos.nPos = blockFileInfo_[nLastBlockFile_].nUndoSize;
    blockFileInfo_[nLastBlockFile_].nUndoSize += nAddSize;
    
    return true;
}

// ============================================================================
// Block Data Operations
// ============================================================================

Status BlockDB::WriteBlock(const Block& block, DiskBlockPos& pos) {
    // Serialize the block
    DataStream ss;
    Serialize(ss, block);
    
    // Allocate space
    if (!AllocateBlockFile(ss.size() + 8, pos)) {  // +8 for size prefix and magic
        return Status::IOError("Failed to allocate block file space");
    }
    
    // Open file
    FILE* file = OpenBlockFile(pos.nFile, false);
    if (!file) {
        return Status::IOError("Failed to open block file");
    }
    
    // Seek to position
    if (std::fseek(file, pos.nPos, SEEK_SET) != 0) {
        return Status::IOError("Failed to seek in block file");
    }
    
    // Write magic and size prefix (network message format)
    uint32_t nMagic = 0xD9B4BEF9;  // Mainnet magic (could be configurable)
    uint32_t nSize = ss.size();
    
    if (std::fwrite(&nMagic, 1, 4, file) != 4 ||
        std::fwrite(&nSize, 1, 4, file) != 4 ||
        std::fwrite(ss.data(), 1, ss.size(), file) != ss.size()) {
        return Status::IOError("Failed to write block to file");
    }
    
    // Update file info
    int height = 0;  // Would be set by caller
    uint64_t time = block.nTime;
    blockFileInfo_[pos.nFile].AddBlock(height, time);
    WriteBlockFileInfo(pos.nFile, blockFileInfo_[pos.nFile]);
    
    return Status::Ok();
}

Status BlockDB::ReadBlock(const DiskBlockPos& pos, Block& block) const {
    if (pos.IsNull()) {
        return Status::InvalidArgument("Invalid block position");
    }
    
    // Try to use cached file first (don't close it)
    FILE* file = nullptr;
    bool fromCache = false;
    {
        std::lock_guard<std::mutex> lock(fileMutex_);
        auto it = fileCache_.find(pos.nFile);
        if (it != fileCache_.end() && it->second != nullptr) {
            std::fflush(it->second);
            file = it->second;
            fromCache = true;
        }
    }
    
    // If not in cache, open a new file
    if (!file) {
        std::filesystem::path path = GetBlockFilePath(pos.nFile);
        file = std::fopen(path.c_str(), "rb");
        if (!file) {
            return Status::IOError("Failed to open block file");
        }
    }
    
    // Seek to position
    if (std::fseek(file, pos.nPos, SEEK_SET) != 0) {
        if (!fromCache) std::fclose(file);
        return Status::IOError("Failed to seek in block file");
    }
    
    // Read magic and size
    uint32_t nMagic = 0;
    uint32_t nSize = 0;
    
    if (std::fread(&nMagic, 1, 4, file) != 4 ||
        std::fread(&nSize, 1, 4, file) != 4) {
        if (!fromCache) std::fclose(file);
        return Status::IOError("Failed to read block header");
    }
    
    // Validate size
    if (nSize == 0 || nSize > 32 * 1024 * 1024) {  // Max 32MB
        if (!fromCache) std::fclose(file);
        return Status::Corruption("Invalid block size");
    }
    
    // Read block data
    std::vector<uint8_t> data(nSize);
    if (std::fread(data.data(), 1, nSize, file) != nSize) {
        if (!fromCache) std::fclose(file);
        return Status::IOError("Failed to read block data");
    }
    
    // Only close if we opened a new file (not from cache)
    if (!fromCache) {
        std::fclose(file);
    }
    
    // Deserialize
    try {
        DataStream ss(std::move(data));
        Unserialize(ss, block);
    } catch (const std::exception& e) {
        return Status::Corruption(std::string("Failed to deserialize block: ") + e.what());
    }
    
    return Status::Ok();
}

Status BlockDB::WriteUndo(const BlockUndo& undo, DiskBlockPos& pos) {
    // Serialize
    DataStream ss;
    Serialize(ss, undo);
    
    // Allocate space
    if (!AllocateUndoFile(ss.size() + 4, pos)) {
        return Status::IOError("Failed to allocate undo file space");
    }
    
    // Open file
    FILE* file = OpenUndoFile(pos.nFile, false);
    if (!file) {
        return Status::IOError("Failed to open undo file");
    }
    
    // Seek and write
    if (std::fseek(file, pos.nPos, SEEK_SET) != 0) {
        std::fclose(file);
        return Status::IOError("Failed to seek in undo file");
    }
    
    uint32_t nSize = ss.size();
    if (std::fwrite(&nSize, 1, 4, file) != 4 ||
        std::fwrite(ss.data(), 1, ss.size(), file) != ss.size()) {
        std::fclose(file);
        return Status::IOError("Failed to write undo data");
    }
    
    std::fclose(file);
    
    // Update file info
    WriteBlockFileInfo(pos.nFile, blockFileInfo_[pos.nFile]);
    
    return Status::Ok();
}

Status BlockDB::ReadUndo(const DiskBlockPos& pos, BlockUndo& undo) const {
    if (pos.IsNull()) {
        return Status::InvalidArgument("Invalid undo position");
    }
    
    FILE* file = OpenUndoFile(pos.nFile, true);
    if (!file) {
        return Status::IOError("Failed to open undo file");
    }
    
    if (std::fseek(file, pos.nPos, SEEK_SET) != 0) {
        std::fclose(file);
        return Status::IOError("Failed to seek in undo file");
    }
    
    uint32_t nSize = 0;
    if (std::fread(&nSize, 1, 4, file) != 4) {
        std::fclose(file);
        return Status::IOError("Failed to read undo header");
    }
    
    if (nSize == 0 || nSize > 32 * 1024 * 1024) {
        std::fclose(file);
        return Status::Corruption("Invalid undo size");
    }
    
    std::vector<uint8_t> data(nSize);
    if (std::fread(data.data(), 1, nSize, file) != nSize) {
        std::fclose(file);
        return Status::IOError("Failed to read undo data");
    }
    
    std::fclose(file);
    
    try {
        DataStream ss(std::move(data));
        Unserialize(ss, undo);
    } catch (const std::exception& e) {
        return Status::Corruption(std::string("Failed to deserialize undo: ") + e.what());
    }
    
    return Status::Ok();
}

// ============================================================================
// Block Index Operations
// ============================================================================

Status BlockDB::WriteBlockIndex(const BlockHash& hash, const BlockIndexDB& entry) {
    std::string key = MakeKey(prefix::BLOCK_INDEX, hash);
    std::string value = SerializeToString(entry);
    return db_->Put(WriteOptions(), Slice(key), Slice(value));
}

Status BlockDB::ReadBlockIndex(const BlockHash& hash, BlockIndexDB& entry) const {
    std::string key = MakeKey(prefix::BLOCK_INDEX, hash);
    std::string value;
    Status s = db_->Get(ReadOptions(), Slice(key), &value);
    if (!s.ok()) {
        return s;
    }
    
    if (!DeserializeFromString(value, entry)) {
        return Status::Corruption("Failed to deserialize block index entry");
    }
    
    return Status::Ok();
}

bool BlockDB::HaveBlockIndex(const BlockHash& hash) const {
    std::string key = MakeKey(prefix::BLOCK_INDEX, hash);
    return db_->Exists(Slice(key));
}

int BlockDB::LoadBlockIndexMap(BlockMap& blockIndex) {
    int count = 0;
    
    // First pass: Load all block index entries and track parent hashes
    std::map<BlockHash, BlockHash> parentHashes;  // block hash -> prev block hash
    
    auto iter = db_->NewIterator();
    std::string prefix(1, prefix::BLOCK_INDEX);
    iter->Seek(Slice(prefix));
    
    while (iter->Valid()) {
        Slice key = iter->key();
        if (key.size() < 1 || key[0] != prefix::BLOCK_INDEX) {
            break;
        }
        
        // Extract hash from key
        if (key.size() != 1 + 32) {  // prefix + hash size
            iter->Next();
            continue;
        }
        
        BlockHash hash;
        std::memcpy(hash.data(), key.data() + 1, 32);
        
        // Deserialize entry
        BlockIndexDB entry;
        if (!DeserializeFromString(iter->value().ToString(), entry)) {
            iter->Next();
            continue;
        }
        
        // Store the parent hash for later linking
        if (!entry.header.hashPrevBlock.IsNull()) {
            parentHashes[hash] = entry.header.hashPrevBlock;
        }
        
        // Create BlockIndex and add to map
        auto pindex = std::make_unique<BlockIndex>();
        entry.ToBlockIndex(*pindex);
        auto result = blockIndex.emplace(hash, std::move(pindex));
        result.first->second->phashBlock = &result.first->first;
        
        ++count;
        iter->Next();
    }
    
    // Second pass: Wire up parent pointers
    for (auto& [hash, indexPtr] : blockIndex) {
        auto it = parentHashes.find(hash);
        if (it != parentHashes.end()) {
            auto parentIt = blockIndex.find(it->second);
            if (parentIt != blockIndex.end()) {
                indexPtr->pprev = parentIt->second.get();
            }
        }
    }
    
    return count;
}

Status BlockDB::WriteBestChainTip(const BlockHash& hash) {
    std::string key = MakeKey(prefix::BEST_CHAIN);
    std::string value = SerializeToString(hash);
    return db_->Put(WriteOptions{true}, Slice(key), Slice(value));  // Sync write
}

std::optional<BlockHash> BlockDB::ReadBestChainTip() const {
    std::string key = MakeKey(prefix::BEST_CHAIN);
    std::string value;
    Status s = db_->Get(ReadOptions(), Slice(key), &value);
    if (!s.ok()) {
        return std::nullopt;
    }
    
    BlockHash hash;
    if (!DeserializeFromString(value, hash)) {
        return std::nullopt;
    }
    
    return hash;
}

// ============================================================================
// Block File Info Operations
// ============================================================================

Status BlockDB::WriteBlockFileInfo(int nFile, const BlockFileInfo& info) {
    std::string key = MakeKey(prefix::BLOCK_FILE, static_cast<uint32_t>(nFile));
    std::string value = SerializeToString(info);
    return db_->Put(WriteOptions(), Slice(key), Slice(value));
}

Status BlockDB::ReadBlockFileInfo(int nFile, BlockFileInfo& info) const {
    std::string key = MakeKey(prefix::BLOCK_FILE, static_cast<uint32_t>(nFile));
    std::string value;
    Status s = db_->Get(ReadOptions(), Slice(key), &value);
    if (!s.ok()) {
        return s;
    }
    
    if (!DeserializeFromString(value, info)) {
        return Status::Corruption("Failed to deserialize block file info");
    }
    
    return Status::Ok();
}

Status BlockDB::WriteLastBlockFile(int nFile) {
    std::string key = MakeKey(prefix::LAST_BLOCK_FILE);
    std::string value = SerializeToString(static_cast<uint32_t>(nFile));
    return db_->Put(WriteOptions(), Slice(key), Slice(value));
}

std::optional<int> BlockDB::ReadLastBlockFile() const {
    std::string key = MakeKey(prefix::LAST_BLOCK_FILE);
    std::string value;
    Status s = db_->Get(ReadOptions(), Slice(key), &value);
    if (!s.ok()) {
        return std::nullopt;
    }
    
    uint32_t nFile;
    if (!DeserializeFromString(value, nFile)) {
        return std::nullopt;
    }
    
    return static_cast<int>(nFile);
}

uint64_t BlockDB::GetBlockDiskUsage() const {
    uint64_t total = 0;
    
    try {
        std::filesystem::path blocksDir = dataDir_ / "blocks";
        for (const auto& entry : std::filesystem::directory_iterator(blocksDir)) {
            if (entry.is_regular_file()) {
                total += entry.file_size();
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    return total;
}

// ============================================================================
// Batch Operations
// ============================================================================

std::unique_ptr<db::WriteBatch> BlockDB::StartBatch() {
    return std::make_unique<db::WriteBatch>();
}

Status BlockDB::WriteBatch(db::WriteBatch* batch, bool sync) {
    WriteOptions opts;
    opts.sync = sync;
    return db_->Write(opts, batch);
}

Status BlockDB::Flush() {
    // Flush file handles
    {
        std::lock_guard<std::mutex> lock(fileMutex_);
        for (auto& [nFile, file] : fileCache_) {
            if (file) {
                std::fflush(file);
            }
        }
    }
    
    return db_->Sync();
}

void BlockDB::Compact() {
    db_->Compact();
}

std::string BlockDB::GetStats() const {
    return db_->GetStats();
}

// ============================================================================
// TxIndex Implementation
// ============================================================================

TxIndex::TxIndex(const std::filesystem::path& dataDir, const Options& options) {
    std::filesystem::path dbPath = dataDir / "indexes" / "txindex";
    
    std::error_code ec;
    std::filesystem::create_directories(dbPath, ec);
    
    auto [status, database] = OpenDatabase(dbPath, options);
    if (status.ok()) {
        db_ = std::move(database);
        enabled_ = true;
    }
}

Status TxIndex::IndexTx(const TxHash& txid, const TxIndexEntry& entry) {
    if (!enabled_ || !db_) {
        return Status::NotSupported("TxIndex not enabled");
    }
    
    std::string key = MakeKey(prefix::TX_INDEX, txid);
    std::string value = SerializeToString(entry);
    return db_->Put(WriteOptions(), Slice(key), Slice(value));
}

std::optional<TxIndexEntry> TxIndex::GetTx(const TxHash& txid) const {
    if (!enabled_ || !db_) {
        return std::nullopt;
    }
    
    std::string key = MakeKey(prefix::TX_INDEX, txid);
    std::string value;
    Status s = db_->Get(ReadOptions(), Slice(key), &value);
    if (!s.ok()) {
        return std::nullopt;
    }
    
    TxIndexEntry entry;
    if (!DeserializeFromString(value, entry)) {
        return std::nullopt;
    }
    
    return entry;
}

Status TxIndex::RemoveTx(const TxHash& txid) {
    if (!enabled_ || !db_) {
        return Status::NotSupported("TxIndex not enabled");
    }
    
    std::string key = MakeKey(prefix::TX_INDEX, txid);
    return db_->Delete(WriteOptions(), Slice(key));
}

Status TxIndex::IndexBlock(const Block& block, const DiskBlockPos& blockPos) {
    if (!enabled_ || !db_) {
        return Status::NotSupported("TxIndex not enabled");
    }
    
    WriteBatch batch;
    uint32_t offset = 8;  // Skip magic and size prefix
    
    for (const auto& tx : block.vtx) {
        TxIndexEntry entry(blockPos, offset);
        std::string key = MakeKey(prefix::TX_INDEX, tx->GetHash());
        std::string value = SerializeToString(entry);
        batch.Put(Slice(key), Slice(value));
        
        // Estimate transaction size for offset
        DataStream ss;
        Serialize(ss, *tx);
        offset += ss.size();
    }
    
    return db_->Write(WriteOptions(), &batch);
}

Status TxIndex::UnindexBlock(const Block& block) {
    if (!enabled_ || !db_) {
        return Status::NotSupported("TxIndex not enabled");
    }
    
    WriteBatch batch;
    for (const auto& tx : block.vtx) {
        std::string key = MakeKey(prefix::TX_INDEX, tx->GetHash());
        batch.Delete(Slice(key));
    }
    
    return db_->Write(WriteOptions(), &batch);
}

std::optional<BlockHash> TxIndex::GetBestBlock() const {
    if (!db_) {
        return std::nullopt;
    }
    
    std::string key = MakeKey(prefix::BEST_CHAIN);
    std::string value;
    Status s = db_->Get(ReadOptions(), Slice(key), &value);
    if (!s.ok()) {
        return std::nullopt;
    }
    
    BlockHash hash;
    if (!DeserializeFromString(value, hash)) {
        return std::nullopt;
    }
    
    return hash;
}

Status TxIndex::SetBestBlock(const BlockHash& hash) {
    if (!db_) {
        return Status::NotSupported("TxIndex not enabled");
    }
    
    std::string key = MakeKey(prefix::BEST_CHAIN);
    std::string value = SerializeToString(hash);
    return db_->Put(WriteOptions{true}, Slice(key), Slice(value));
}

} // namespace db
} // namespace shurium
