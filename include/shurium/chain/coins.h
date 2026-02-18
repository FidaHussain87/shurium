// SHURIUM - Coins (UTXO) Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the UTXO (Unspent Transaction Output) management
// system for SHURIUM, inspired by Bitcoin's coins.h

#ifndef SHURIUM_CHAIN_COINS_H
#define SHURIUM_CHAIN_COINS_H

#include "shurium/core/types.h"
#include "shurium/core/transaction.h"
#include "shurium/core/serialize.h"
#include <cstdint>
#include <unordered_map>
#include <optional>
#include <memory>
#include <functional>

namespace shurium {

// ============================================================================
// Coin - A single unspent transaction output
// ============================================================================

/**
 * A UTXO entry representing an unspent transaction output.
 * 
 * Stores the output itself along with metadata about when it was created
 * (block height) and whether it came from a coinbase transaction.
 */
class Coin {
public:
    /// The unspent transaction output
    TxOut out;
    
    /// Whether this output is from a coinbase transaction
    bool fCoinBase;
    
    /// Block height at which this output was created
    uint32_t nHeight;
    
    /// Default constructor - creates an empty/spent coin
    Coin() : fCoinBase(false), nHeight(0) {}
    
    /// Construct from TxOut with metadata
    Coin(const TxOut& outIn, uint32_t heightIn, bool coinbaseIn)
        : out(outIn), fCoinBase(coinbaseIn), nHeight(heightIn) {}
    
    /// Move construct from TxOut
    Coin(TxOut&& outIn, uint32_t heightIn, bool coinbaseIn)
        : out(std::move(outIn)), fCoinBase(coinbaseIn), nHeight(heightIn) {}
    
    /// Check if this is a coinbase output
    bool IsCoinBase() const { return fCoinBase; }
    
    /// Check if this coin has been spent (null output)
    bool IsSpent() const { return out.IsNull(); }
    
    /// Clear this coin (mark as spent)
    void Clear() {
        out.SetNull();
        fCoinBase = false;
        nHeight = 0;
    }
    
    /// Get the value of this output
    Amount GetAmount() const { return out.nValue; }
    
    /// Get the scriptPubKey
    const Script& GetScriptPubKey() const { return out.scriptPubKey; }
    
    /// Check if coinbase output is mature (can be spent)
    /// Coinbase outputs require COINBASE_MATURITY confirmations
    bool IsMature(uint32_t currentHeight) const {
        if (!fCoinBase) return true;
        // Coinbase maturity: 100 blocks for mainnet
        static constexpr uint32_t COINBASE_MATURITY = 100;
        return currentHeight >= nHeight + COINBASE_MATURITY;
    }
    
    /// Estimate dynamic memory usage
    size_t DynamicMemoryUsage() const {
        return out.scriptPubKey.size();
    }
    
    /// Comparison
    bool operator==(const Coin& other) const {
        return out == other.out && 
               fCoinBase == other.fCoinBase && 
               nHeight == other.nHeight;
    }
    
    bool operator!=(const Coin& other) const {
        return !(*this == other);
    }
};

/// Serialization for Coin
template<typename Stream>
void Serialize(Stream& s, const Coin& coin) {
    // Encode height and coinbase flag together
    uint32_t code = (coin.nHeight << 1) | (coin.fCoinBase ? 1 : 0);
    WriteCompactSize(s, code);
    Serialize(s, coin.out);
}

template<typename Stream>
void Unserialize(Stream& s, Coin& coin) {
    uint32_t code = static_cast<uint32_t>(ReadCompactSize(s));
    coin.nHeight = code >> 1;
    coin.fCoinBase = code & 1;
    Unserialize(s, coin.out);
}

// ============================================================================
// OutPointHasher - Hash function for OutPoint keys
// ============================================================================

/// Hash function for OutPoint to use in unordered_map
struct OutPointHasher {
    size_t operator()(const OutPoint& outpoint) const {
        // Combine hash of the txid with the output index
        size_t seed = 0;
        // Simple hash combining
        for (size_t i = 0; i < 8; ++i) {
            seed ^= static_cast<size_t>(outpoint.hash[i]) << (i * 8);
        }
        seed ^= std::hash<uint32_t>{}(outpoint.n);
        return seed;
    }
};

// ============================================================================
// CoinsCacheEntry - Entry in the coins cache with dirty/fresh flags
// ============================================================================

/// Cache entry flags
enum class CoinsCacheFlags : uint8_t {
    NONE = 0,
    DIRTY = 1 << 0,  // Modified, needs to be written to parent
    FRESH = 1 << 1,  // Not in parent cache, can be deleted without writing
};

inline CoinsCacheFlags operator|(CoinsCacheFlags a, CoinsCacheFlags b) {
    return static_cast<CoinsCacheFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline CoinsCacheFlags operator&(CoinsCacheFlags a, CoinsCacheFlags b) {
    return static_cast<CoinsCacheFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool HasFlag(CoinsCacheFlags flags, CoinsCacheFlags flag) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

/**
 * A coin entry in the cache with tracking flags.
 */
struct CoinsCacheEntry {
    Coin coin;
    CoinsCacheFlags flags;
    
    CoinsCacheEntry() : flags(CoinsCacheFlags::NONE) {}
    explicit CoinsCacheEntry(Coin&& c) : coin(std::move(c)), flags(CoinsCacheFlags::NONE) {}
    explicit CoinsCacheEntry(const Coin& c) : coin(c), flags(CoinsCacheFlags::NONE) {}
    
    bool IsDirty() const { return HasFlag(flags, CoinsCacheFlags::DIRTY); }
    bool IsFresh() const { return HasFlag(flags, CoinsCacheFlags::FRESH); }
    
    void SetDirty() { flags = flags | CoinsCacheFlags::DIRTY; }
    void SetFresh() { flags = flags | CoinsCacheFlags::FRESH; }
    void ClearFlags() { flags = CoinsCacheFlags::NONE; }
};

/// Type alias for the coins cache map
using CoinsMap = std::unordered_map<OutPoint, CoinsCacheEntry, OutPointHasher>;

// ============================================================================
// CoinsView - Abstract interface for UTXO database views
// ============================================================================

/**
 * Abstract view on the UTXO set.
 * 
 * This provides an interface for accessing unspent transaction outputs.
 * Implementations can back this with in-memory storage, database, or
 * caching layers.
 */
class CoinsView {
public:
    virtual ~CoinsView() = default;
    
    /// Retrieve a coin by its outpoint
    virtual std::optional<Coin> GetCoin(const OutPoint& outpoint) const = 0;
    
    /// Check if a coin exists (is unspent)
    virtual bool HaveCoin(const OutPoint& outpoint) const = 0;
    
    /// Get the hash of the best block for this view's state
    virtual BlockHash GetBestBlock() const = 0;
    
    /// Get estimated size of the UTXO set
    virtual size_t EstimateSize() const { return 0; }
};

// ============================================================================
// CoinsViewBacked - CoinsView with a backing parent view
// ============================================================================

/**
 * A CoinsView that is backed by another CoinsView.
 * Lookups fall through to the parent if not found locally.
 */
class CoinsViewBacked : public CoinsView {
protected:
    CoinsView* base;
    
public:
    explicit CoinsViewBacked(CoinsView* baseIn) : base(baseIn) {}
    
    std::optional<Coin> GetCoin(const OutPoint& outpoint) const override {
        return base ? base->GetCoin(outpoint) : std::nullopt;
    }
    
    bool HaveCoin(const OutPoint& outpoint) const override {
        return base ? base->HaveCoin(outpoint) : false;
    }
    
    BlockHash GetBestBlock() const override {
        return base ? base->GetBestBlock() : BlockHash();
    }
    
    size_t EstimateSize() const override {
        return base ? base->EstimateSize() : 0;
    }
    
    void SetBackend(CoinsView* viewIn) { base = viewIn; }
    CoinsView* GetBackend() const { return base; }
};

// ============================================================================
// CoinsViewCache - In-memory caching layer for UTXO lookups
// ============================================================================

/**
 * A CoinsView that adds an in-memory cache on top of another view.
 * 
 * This is the primary interface for accessing and modifying the UTXO set
 * during block validation. Changes are accumulated in memory and can be
 * flushed to the backing view.
 */
class CoinsViewCache : public CoinsViewBacked {
private:
    mutable CoinsMap cacheCoins;
    mutable BlockHash hashBlock;
    mutable size_t cachedCoinsUsage{0};
    
    /// Fetch a coin into the cache if not already present
    CoinsMap::iterator FetchCoin(const OutPoint& outpoint) const;
    
public:
    explicit CoinsViewCache(CoinsView* baseIn);
    
    // Prevent accidental copies
    CoinsViewCache(const CoinsViewCache&) = delete;
    CoinsViewCache& operator=(const CoinsViewCache&) = delete;
    
    // CoinsView interface
    std::optional<Coin> GetCoin(const OutPoint& outpoint) const override;
    bool HaveCoin(const OutPoint& outpoint) const override;
    BlockHash GetBestBlock() const override;
    size_t EstimateSize() const override;
    
    /// Get a reference to a coin in the cache (more efficient than GetCoin)
    const Coin& AccessCoin(const OutPoint& outpoint) const;
    
    /// Add a new coin to the cache
    void AddCoin(const OutPoint& outpoint, Coin&& coin, bool possibleOverwrite);
    
    /// Spend a coin (mark it as spent)
    bool SpendCoin(const OutPoint& outpoint, Coin* moveTo = nullptr);
    
    /// Set the best block hash
    void SetBestBlock(const BlockHash& block);
    
    /// Check if a coin is in the cache (not checking parent)
    bool HaveCoinInCache(const OutPoint& outpoint) const;
    
    /// Get the cached dynamic memory usage
    size_t GetCacheSize() const { return cacheCoins.size(); }
    size_t GetCacheUsage() const { return cachedCoinsUsage; }
    
    /// Flush changes to the backing view
    bool Flush();
    
    /// Clear the cache without flushing
    void Reset();
    
    /// Calculate the hash of all UTXOs for verification
    Hash256 GetUTXOSetHash() const;
    
    /// Batch-add all outputs from a transaction
    void AddTransaction(const Transaction& tx, uint32_t height);
    
    /// Check if all inputs of a transaction exist and are unspent
    bool HaveInputs(const Transaction& tx) const;
    
    /// Get the total value of inputs for a transaction
    Amount GetValueIn(const Transaction& tx) const;
};

// ============================================================================
// CoinsViewMemory - Simple in-memory CoinsView for testing
// ============================================================================

/**
 * A simple in-memory implementation of CoinsView.
 * Useful for testing and as a backing store for CoinsViewCache.
 */
class CoinsViewMemory : public CoinsView {
private:
    CoinsMap coins;
    BlockHash bestBlock;
    
public:
    CoinsViewMemory() = default;
    
    std::optional<Coin> GetCoin(const OutPoint& outpoint) const override;
    bool HaveCoin(const OutPoint& outpoint) const override;
    BlockHash GetBestBlock() const override;
    size_t EstimateSize() const override;
    
    /// Direct manipulation for testing
    void AddCoin(const OutPoint& outpoint, const Coin& coin);
    void RemoveCoin(const OutPoint& outpoint);
    void SetBestBlock(const BlockHash& block);
    void Clear();
    
    /// Receive a batch write from a cache
    void BatchWrite(CoinsMap& mapCoins, const BlockHash& block);
};

// ============================================================================
// UTXO Statistics
// ============================================================================

/**
 * Statistics about the UTXO set.
 */
struct UTXOStats {
    uint64_t nTransactions{0};     // Number of transactions with unspent outputs
    uint64_t nTransactionOutputs{0}; // Total number of UTXOs
    uint64_t nBogoSize{0};         // Estimate of serialized size
    Amount nTotalAmount{0};        // Total value of all UTXOs
    Hash256 hashSerialized;        // Hash of the entire UTXO set
    uint64_t nDiskSize{0};         // Size on disk
    
    void Reset() {
        nTransactions = 0;
        nTransactionOutputs = 0;
        nBogoSize = 0;
        nTotalAmount = 0;
        hashSerialized.SetNull();
        nDiskSize = 0;
    }
};

/// Calculate UTXO set statistics
UTXOStats GetUTXOStats(const CoinsView& view);

} // namespace shurium

#endif // SHURIUM_CHAIN_COINS_H
