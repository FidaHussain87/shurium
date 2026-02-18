// SHURIUM - Mempool Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the transaction memory pool for SHURIUM.
// The mempool holds unconfirmed transactions waiting to be included in blocks.

#ifndef SHURIUM_MEMPOOL_MEMPOOL_H
#define SHURIUM_MEMPOOL_MEMPOOL_H

#include "shurium/core/types.h"
#include "shurium/core/transaction.h"
#include "shurium/chain/coins.h"
#include <cstdint>
#include <chrono>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <optional>
#include <functional>

namespace shurium {

// Forward declarations
class Mempool;

// ============================================================================
// Fee Rate - Fee per virtual byte
// ============================================================================

/**
 * Fee rate in base units per virtual byte.
 * Used for transaction prioritization and minimum fee enforcement.
 */
class FeeRate {
private:
    Amount nSatoshisPerK;  // Fee per 1000 virtual bytes
    
public:
    /// Default constructor - zero fee rate
    FeeRate() : nSatoshisPerK(0) {}
    
    /// Construct from fee per 1000 bytes
    explicit FeeRate(Amount satoshisPerK) : nSatoshisPerK(satoshisPerK) {}
    
    /// Construct from fee and size
    FeeRate(Amount fee, size_t bytes) {
        if (bytes > 0) {
            nSatoshisPerK = (fee * 1000) / static_cast<Amount>(bytes);
        } else {
            nSatoshisPerK = 0;
        }
    }
    
    /// Get fee for a given size
    Amount GetFee(size_t bytes) const {
        Amount fee = (nSatoshisPerK * static_cast<Amount>(bytes)) / 1000;
        // Round up
        if ((nSatoshisPerK * static_cast<Amount>(bytes)) % 1000 > 0) {
            fee++;
        }
        return fee;
    }
    
    /// Get fee per 1000 bytes
    Amount GetFeePerK() const { return nSatoshisPerK; }
    
    /// Comparison operators
    bool operator<(const FeeRate& other) const { return nSatoshisPerK < other.nSatoshisPerK; }
    bool operator>(const FeeRate& other) const { return nSatoshisPerK > other.nSatoshisPerK; }
    bool operator<=(const FeeRate& other) const { return nSatoshisPerK <= other.nSatoshisPerK; }
    bool operator>=(const FeeRate& other) const { return nSatoshisPerK >= other.nSatoshisPerK; }
    bool operator==(const FeeRate& other) const { return nSatoshisPerK == other.nSatoshisPerK; }
    bool operator!=(const FeeRate& other) const { return nSatoshisPerK != other.nSatoshisPerK; }
    
    /// Arithmetic
    FeeRate& operator+=(const FeeRate& other) {
        nSatoshisPerK += other.nSatoshisPerK;
        return *this;
    }
    
    /// String representation
    std::string ToString() const;
};

// ============================================================================
// Mempool Entry - A transaction in the mempool
// ============================================================================

/**
 * Stores data about a transaction in the mempool.
 * Includes the transaction itself plus metadata for prioritization.
 */
class MempoolEntry {
private:
    /// The transaction
    TransactionRef tx;
    
    /// Fee paid by this transaction
    Amount nFee;
    
    /// Transaction size (virtual bytes)
    size_t nTxSize;
    
    /// Time when transaction entered mempool
    int64_t nTime;
    
    /// Chain height when entering mempool
    uint32_t entryHeight;
    
    /// Whether this transaction spends a coinbase
    bool spendsCoinbase;
    
    /// Modified fee (for fee bumping)
    mutable Amount nModifiedFee;
    
    /// Fee rate (cached)
    FeeRate feeRate;
    
    // Ancestor/descendant tracking for CPFP (Child Pays For Parent)
    mutable uint64_t nCountWithAncestors;
    mutable uint64_t nSizeWithAncestors;
    mutable Amount nModFeesWithAncestors;
    
    mutable uint64_t nCountWithDescendants;
    mutable uint64_t nSizeWithDescendants;
    mutable Amount nModFeesWithDescendants;
    
public:
    /// Construct a mempool entry
    MempoolEntry(const TransactionRef& txIn, Amount feeIn, int64_t timeIn,
                 uint32_t heightIn, bool spendsCoinbaseIn);
    
    // Accessors
    const Transaction& GetTx() const { return *tx; }
    const TransactionRef& GetSharedTx() const { return tx; }
    TxHash GetTxHash() const { return tx->GetHash(); }
    
    Amount GetFee() const { return nFee; }
    Amount GetModifiedFee() const { return nModifiedFee; }
    size_t GetTxSize() const { return nTxSize; }
    int64_t GetTime() const { return nTime; }
    uint32_t GetHeight() const { return entryHeight; }
    bool SpendsCoinbase() const { return spendsCoinbase; }
    
    FeeRate GetFeeRate() const { return feeRate; }
    FeeRate GetModifiedFeeRate() const { return FeeRate(nModifiedFee, nTxSize); }
    
    // Ancestor stats
    uint64_t GetCountWithAncestors() const { return nCountWithAncestors; }
    uint64_t GetSizeWithAncestors() const { return nSizeWithAncestors; }
    Amount GetModFeesWithAncestors() const { return nModFeesWithAncestors; }
    FeeRate GetAncestorFeeRate() const {
        return FeeRate(nModFeesWithAncestors, nSizeWithAncestors);
    }
    
    // Descendant stats
    uint64_t GetCountWithDescendants() const { return nCountWithDescendants; }
    uint64_t GetSizeWithDescendants() const { return nSizeWithDescendants; }
    Amount GetModFeesWithDescendants() const { return nModFeesWithDescendants; }
    
    // Modifiers (for mempool management)
    void UpdateModifiedFee(Amount fee) const { nModifiedFee = fee; }
    void UpdateAncestorState(int64_t countDelta, int64_t sizeDelta, Amount feeDelta) const;
    void UpdateDescendantState(int64_t countDelta, int64_t sizeDelta, Amount feeDelta) const;
    
    /// Get memory usage estimate
    size_t DynamicMemoryUsage() const;
};

// ============================================================================
// Mempool Entry Comparators
// ============================================================================

/// Compare by descendant fee rate (for mining prioritization)
struct CompareMempoolEntryByDescendantScore {
    bool operator()(const MempoolEntry& a, const MempoolEntry& b) const {
        // Higher fee rate = higher priority
        FeeRate aRate = a.GetModFeesWithDescendants() > 0 ?
            FeeRate(a.GetModFeesWithDescendants(), a.GetSizeWithDescendants()) :
            a.GetModifiedFeeRate();
        FeeRate bRate = b.GetModFeesWithDescendants() > 0 ?
            FeeRate(b.GetModFeesWithDescendants(), b.GetSizeWithDescendants()) :
            b.GetModifiedFeeRate();
        
        if (aRate == bRate) {
            return a.GetTime() < b.GetTime();  // Earlier = higher priority
        }
        return aRate > bRate;
    }
};

/// Compare by ancestor fee rate (for eviction)
struct CompareMempoolEntryByAncestorFee {
    bool operator()(const MempoolEntry& a, const MempoolEntry& b) const {
        FeeRate aRate = a.GetAncestorFeeRate();
        FeeRate bRate = b.GetAncestorFeeRate();
        
        if (aRate == bRate) {
            return a.GetTxHash() < b.GetTxHash();
        }
        return aRate < bRate;  // Lower fee rate = evict first
    }
};

/// Compare by entry time
struct CompareMempoolEntryByEntryTime {
    bool operator()(const MempoolEntry& a, const MempoolEntry& b) const {
        return a.GetTime() < b.GetTime();
    }
};

// ============================================================================
// Mempool Removal Reason
// ============================================================================

enum class MempoolRemovalReason {
    UNKNOWN = 0,
    EXPIRY,           // Transaction exceeded maximum age
    SIZELIMIT,        // Mempool size limit exceeded
    REORG,            // Chain reorganization
    BLOCK,            // Included in a block
    CONFLICT,         // Conflicts with another transaction
    REPLACED,         // Replaced by higher-fee transaction (RBF)
};

std::string RemovalReasonToString(MempoolRemovalReason reason);

// ============================================================================
// Mempool Limits
// ============================================================================

/**
 * Mempool size and policy limits.
 */
struct MempoolLimits {
    /// Maximum mempool size in bytes
    size_t maxSize = 300 * 1000 * 1000;  // 300 MB default
    
    /// Maximum transaction age in seconds
    int64_t maxAge = 14 * 24 * 60 * 60;  // 14 days default
    
    /// Maximum ancestor count
    uint64_t maxAncestorCount = 25;
    
    /// Maximum ancestor size (bytes)
    uint64_t maxAncestorSize = 101000;
    
    /// Maximum descendant count
    uint64_t maxDescendantCount = 25;
    
    /// Maximum descendant size (bytes)
    uint64_t maxDescendantSize = 101000;
    
    /// Minimum fee rate to enter mempool
    FeeRate minFeeRate{1000};  // 1 sat/vB default
    
    /// Incremental relay fee (for RBF)
    FeeRate incrementalRelayFee{1000};
};

// ============================================================================
// Mempool Transaction Info
// ============================================================================

/**
 * Summary information about a mempool transaction.
 * Used for RPC and notification interfaces.
 */
struct TxMempoolInfo {
    TransactionRef tx;
    int64_t time;
    Amount fee;
    size_t vsize;
    FeeRate feeRate;
};

// ============================================================================
// Mempool - The transaction memory pool
// ============================================================================

/**
 * The transaction memory pool.
 * 
 * Stores valid unconfirmed transactions that may be included in future blocks.
 * Provides efficient lookup by txid and prioritization for mining.
 */
class Mempool {
public:
    struct TxHasher {
        size_t operator()(const TxHash& hash) const {
            size_t seed = 0;
            for (size_t i = 0; i < 8; ++i) {
                seed ^= static_cast<size_t>(hash[i]) << (i * 8);
            }
            return seed;
        }
    };
    
    /// Type for mempool entries indexed by txid
    using TxMap = std::unordered_map<TxHash, MempoolEntry, TxHasher>;
    using txiter = TxMap::iterator;
    using const_txiter = TxMap::const_iterator;
    
private:
    /// Main transaction map
    TxMap mapTx;
    
    /// Index by outpoint (for conflict detection)
    std::unordered_map<OutPoint, TxHash, OutPointHasher> mapNextTx;
    
    /// Set of transactions ordered by ancestor fee rate (for eviction)
    std::set<std::reference_wrapper<const MempoolEntry>, CompareMempoolEntryByAncestorFee> mapAncestorFee;
    
    /// Limits
    MempoolLimits limits;
    
    /// Total size of all transactions
    size_t totalTxSize{0};
    
    /// Total fees
    Amount totalFees{0};
    
    /// Sequence number for ordering (reserved for future use)
    [[maybe_unused]] uint64_t nSequence{0};
    
    /// Mutex for thread safety
    mutable std::mutex cs;
    
    /// Notification callback for transaction removal
    std::function<void(const TransactionRef&, MempoolRemovalReason)> notifyRemoved;
    
    // Internal helpers
    void UpdateAncestorsOf(txiter it, bool add);
    void UpdateDescendantsOf(txiter it, bool add);
    void RemoveUnchecked(txiter it, MempoolRemovalReason reason);
    bool CheckAncestorLimits(const MempoolEntry& entry, std::string& errString) const;
    bool CheckDescendantLimits(txiter ancestor, std::string& errString) const;
    void TrimToSize(size_t targetSize);
    void ExpireOld(int64_t currentTime);
    
    /// Get ancestors of a transaction
    std::set<TxHash> GetAncestors(const TxHash& txid) const;
    
    /// Get descendants of a transaction
    std::set<TxHash> GetDescendants(const TxHash& txid) const;
    
public:
    Mempool();
    explicit Mempool(const MempoolLimits& limitsIn);
    ~Mempool() = default;
    
    // Prevent copies
    Mempool(const Mempool&) = delete;
    Mempool& operator=(const Mempool&) = delete;
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /// Set limits
    void SetLimits(const MempoolLimits& limitsIn) {
        std::lock_guard<std::mutex> lock(cs);
        limits = limitsIn;
    }
    
    /// Get limits
    MempoolLimits GetLimits() const {
        std::lock_guard<std::mutex> lock(cs);
        return limits;
    }
    
    /// Set removal notification callback
    void SetNotifyRemoved(std::function<void(const TransactionRef&, MempoolRemovalReason)> callback) {
        std::lock_guard<std::mutex> lock(cs);
        notifyRemoved = std::move(callback);
    }
    
    // ========================================================================
    // Adding Transactions
    // ========================================================================
    
    /**
     * Add a transaction to the mempool.
     * 
     * @param tx The transaction to add
     * @param fee The fee paid by this transaction
     * @param height Current chain height
     * @param spendsCoinbase Whether any inputs are coinbase outputs
     * @param errString Output: error message if failed
     * @return True if added successfully
     */
    bool AddTx(const TransactionRef& tx, Amount fee, uint32_t height,
               bool spendsCoinbase, std::string& errString);
    
    /**
     * Check if a transaction can be added (without actually adding).
     */
    bool CheckTx(const TransactionRef& tx, Amount fee, std::string& errString) const;
    
    // ========================================================================
    // Removing Transactions
    // ========================================================================
    
    /**
     * Remove a transaction and all its descendants.
     */
    void RemoveTxAndDescendants(const TxHash& txid, MempoolRemovalReason reason);
    
    /**
     * Remove transactions that conflict with the given transaction.
     */
    void RemoveConflicts(const Transaction& tx);
    
    /**
     * Remove transactions confirmed in a block.
     */
    void RemoveForBlock(const std::vector<TransactionRef>& vtx);
    
    /**
     * Clear all transactions.
     */
    void Clear();
    
    // ========================================================================
    // Querying
    // ========================================================================
    
    /**
     * Check if a transaction is in the mempool.
     */
    bool Exists(const TxHash& txid) const;
    
    /**
     * Get a transaction from the mempool.
     */
    TransactionRef Get(const TxHash& txid) const;
    
    /**
     * Get info about a mempool transaction.
     */
    std::optional<TxMempoolInfo> GetInfo(const TxHash& txid) const;
    
    /**
     * Get the transaction that spends an output.
     */
    TransactionRef GetSpender(const OutPoint& outpoint) const;
    
    /**
     * Check if an outpoint is spent by a mempool transaction.
     */
    bool IsSpent(const OutPoint& outpoint) const;
    
    /**
     * Check if adding a transaction would conflict with mempool.
     */
    bool HasConflicts(const Transaction& tx) const;
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Number of transactions in mempool
    size_t Size() const {
        std::lock_guard<std::mutex> lock(cs);
        return mapTx.size();
    }
    
    /// Total size of transactions in bytes
    size_t GetTotalSize() const {
        std::lock_guard<std::mutex> lock(cs);
        return totalTxSize;
    }
    
    /// Total fees in mempool
    Amount GetTotalFees() const {
        std::lock_guard<std::mutex> lock(cs);
        return totalFees;
    }
    
    /// Check if mempool is empty
    bool IsEmpty() const {
        std::lock_guard<std::mutex> lock(cs);
        return mapTx.empty();
    }
    
    /// Get minimum fee rate to enter mempool
    FeeRate GetMinFee() const;
    
    // ========================================================================
    // Mining Support
    // ========================================================================
    
    /**
     * Get transactions sorted by priority for mining.
     * 
     * @param maxSize Maximum total size to return
     * @param minFeeRate Minimum fee rate
     * @return Vector of transactions in priority order
     */
    std::vector<TransactionRef> GetTransactionsForBlock(
        size_t maxSize, FeeRate minFeeRate) const;
    
    /**
     * Get info about all transactions (for RPC).
     */
    std::vector<TxMempoolInfo> GetAllTxInfo() const;
    
    // ========================================================================
    // Maintenance
    // ========================================================================
    
    /**
     * Expire old transactions and trim to size limit.
     */
    void LimitSize(int64_t currentTime);
    
    /**
     * Check consistency of internal data structures.
     * Used for testing.
     */
    bool CheckConsistency() const;
    
    // ========================================================================
    // UTXO Integration
    // ========================================================================
    
    /**
     * Get a coin from the mempool (for transaction validation).
     * Mempool coins have height MEMPOOL_HEIGHT.
     */
    std::optional<Coin> GetCoin(const OutPoint& outpoint) const;
    
    /// Height value for mempool coins
    static constexpr uint32_t MEMPOOL_HEIGHT = 0x7FFFFFFF;
};

// ============================================================================
// Mempool Coins View - UTXO view that includes mempool transactions
// ============================================================================

/**
 * A CoinsView that layers mempool transactions on top of a base view.
 * Used for validating chains of unconfirmed transactions.
 */
class MempoolCoinsView : public CoinsViewBacked {
private:
    const Mempool& mempool;
    
public:
    MempoolCoinsView(CoinsView* baseIn, const Mempool& mempoolIn)
        : CoinsViewBacked(baseIn), mempool(mempoolIn) {}
    
    std::optional<Coin> GetCoin(const OutPoint& outpoint) const override;
    bool HaveCoin(const OutPoint& outpoint) const override;
};

// ============================================================================
// Transaction Acceptance
// ============================================================================

/**
 * Result of AcceptToMempool.
 */
struct MempoolAcceptResult {
    enum class ResultType {
        VALID,              // Transaction was accepted
        INVALID,            // Transaction is invalid
        MEMPOOL_ERROR,      // Mempool policy error (fee too low, etc.)
    };
    
    ResultType result;
    std::string rejectReason;
    TxHash txid;
    Amount fee{0};
    
    static MempoolAcceptResult Success(const TxHash& id, Amount f) {
        MempoolAcceptResult r;
        r.result = ResultType::VALID;
        r.txid = id;
        r.fee = f;
        return r;
    }
    
    static MempoolAcceptResult Invalid(const std::string& reason) {
        MempoolAcceptResult r;
        r.result = ResultType::INVALID;
        r.rejectReason = reason;
        return r;
    }
    
    static MempoolAcceptResult MempoolPolicy(const std::string& reason) {
        MempoolAcceptResult r;
        r.result = ResultType::MEMPOOL_ERROR;
        r.rejectReason = reason;
        return r;
    }
    
    bool IsValid() const { return result == ResultType::VALID; }
};

/**
 * Accept a transaction into the mempool.
 * 
 * Performs full validation including:
 * - Basic transaction validity (CheckTransaction)
 * - Input availability (UTXO or mempool)
 * - Script verification
 * - Fee calculation and policy checks
 * - Mempool ancestor/descendant limits
 * 
 * @param tx Transaction to validate and add
 * @param mempool Mempool to add to
 * @param coins UTXO view for input validation
 * @param chainHeight Current chain height (for coinbase maturity)
 * @param bypassLimits If true, bypass mempool policy checks (for reorg)
 * @return Result indicating success or failure reason
 */
MempoolAcceptResult AcceptToMempool(
    const TransactionRef& tx,
    Mempool& mempool,
    CoinsView& coins,
    int32_t chainHeight,
    bool bypassLimits = false);

} // namespace shurium

#endif // SHURIUM_MEMPOOL_MEMPOOL_H
