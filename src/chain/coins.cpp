// SHURIUM - Coins (UTXO) Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/chain/coins.h"
#include "shurium/crypto/sha256.h"
#include <algorithm>
#include <cassert>

namespace shurium {

// ============================================================================
// Empty Coin Singleton
// ============================================================================

/// Static empty coin for returning references to non-existent coins
static const Coin coinEmpty;

// ============================================================================
// CoinsViewCache Implementation
// ============================================================================

CoinsViewCache::CoinsViewCache(CoinsView* baseIn) 
    : CoinsViewBacked(baseIn) {
    if (baseIn) {
        hashBlock = baseIn->GetBestBlock();
    }
}

CoinsMap::iterator CoinsViewCache::FetchCoin(const OutPoint& outpoint) const {
    auto it = cacheCoins.find(outpoint);
    if (it != cacheCoins.end()) {
        return it;
    }
    
    // Not in cache, try to fetch from parent
    auto coinOpt = base ? base->GetCoin(outpoint) : std::nullopt;
    if (!coinOpt) {
        return cacheCoins.end();
    }
    
    // Insert into cache
    auto [insertIt, inserted] = cacheCoins.emplace(
        outpoint, 
        CoinsCacheEntry(std::move(*coinOpt))
    );
    
    if (inserted && !insertIt->second.coin.IsSpent()) {
        cachedCoinsUsage += insertIt->second.coin.DynamicMemoryUsage();
    }
    
    return insertIt;
}

std::optional<Coin> CoinsViewCache::GetCoin(const OutPoint& outpoint) const {
    auto it = FetchCoin(outpoint);
    if (it == cacheCoins.end() || it->second.coin.IsSpent()) {
        return std::nullopt;
    }
    return it->second.coin;
}

bool CoinsViewCache::HaveCoin(const OutPoint& outpoint) const {
    auto it = FetchCoin(outpoint);
    return it != cacheCoins.end() && !it->second.coin.IsSpent();
}

bool CoinsViewCache::HaveCoinInCache(const OutPoint& outpoint) const {
    auto it = cacheCoins.find(outpoint);
    return it != cacheCoins.end() && !it->second.coin.IsSpent();
}

BlockHash CoinsViewCache::GetBestBlock() const {
    if (hashBlock.IsNull() && base) {
        hashBlock = base->GetBestBlock();
    }
    return hashBlock;
}

void CoinsViewCache::SetBestBlock(const BlockHash& block) {
    hashBlock = block;
}

size_t CoinsViewCache::EstimateSize() const {
    return cacheCoins.size();
}

const Coin& CoinsViewCache::AccessCoin(const OutPoint& outpoint) const {
    auto it = FetchCoin(outpoint);
    if (it == cacheCoins.end()) {
        return coinEmpty;
    }
    return it->second.coin;
}

void CoinsViewCache::AddCoin(const OutPoint& outpoint, Coin&& coin, bool possibleOverwrite) {
    assert(!coin.IsSpent());
    
    auto [it, inserted] = cacheCoins.try_emplace(outpoint);
    
    bool fresh = false;
    if (!inserted) {
        // Entry already exists
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
        
        if (!possibleOverwrite && !it->second.coin.IsSpent()) {
            // Should not happen - overwriting an existing unspent coin
            throw std::logic_error("Overwriting existing unspent coin");
        }
        
        // If the existing entry is fresh, the new one is also fresh
        fresh = it->second.IsFresh();
    } else {
        // New entry - it's fresh if we're adding a new coin
        fresh = true;
    }
    
    it->second.coin = std::move(coin);
    it->second.SetDirty();
    if (fresh) {
        it->second.SetFresh();
    }
    
    cachedCoinsUsage += it->second.coin.DynamicMemoryUsage();
}

bool CoinsViewCache::SpendCoin(const OutPoint& outpoint, Coin* moveTo) {
    auto it = FetchCoin(outpoint);
    if (it == cacheCoins.end() || it->second.coin.IsSpent()) {
        return false;
    }
    
    cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
    
    if (moveTo) {
        *moveTo = std::move(it->second.coin);
    }
    
    // If the coin was fresh (not in parent), we can just delete it
    if (it->second.IsFresh()) {
        cacheCoins.erase(it);
    } else {
        // Mark as spent but keep for flushing
        it->second.coin.Clear();
        it->second.SetDirty();
    }
    
    return true;
}

bool CoinsViewCache::Flush() {
    // For now, we just clear the cache
    // In production, this would write to the backing storage
    bool success = true;
    
    // Clear all non-dirty entries and reset dirty flags
    for (auto it = cacheCoins.begin(); it != cacheCoins.end(); ) {
        if (it->second.IsDirty()) {
            // In production: write to backing store
            it->second.ClearFlags();
            if (it->second.coin.IsSpent()) {
                // Remove spent entries
                it = cacheCoins.erase(it);
                continue;
            }
        }
        ++it;
    }
    
    return success;
}

void CoinsViewCache::Reset() {
    cacheCoins.clear();
    cachedCoinsUsage = 0;
    if (base) {
        hashBlock = base->GetBestBlock();
    } else {
        hashBlock.SetNull();
    }
}

Hash256 CoinsViewCache::GetUTXOSetHash() const {
    // Create a hash of all UTXOs for verification
    // This is a simplified version - production would use a more efficient method
    SHA256 hasher;
    
    // Sort keys for deterministic ordering
    std::vector<OutPoint> keys;
    keys.reserve(cacheCoins.size());
    for (const auto& [outpoint, entry] : cacheCoins) {
        if (!entry.coin.IsSpent()) {
            keys.push_back(outpoint);
        }
    }
    std::sort(keys.begin(), keys.end());
    
    for (const auto& outpoint : keys) {
        const auto& entry = cacheCoins.at(outpoint);
        
        // Hash the outpoint
        hasher.Write(outpoint.hash.data(), 32);
        uint32_t n = outpoint.n;
        hasher.Write(reinterpret_cast<const uint8_t*>(&n), sizeof(n));
        
        // Hash the coin
        Amount value = entry.coin.GetAmount();
        hasher.Write(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
        hasher.Write(entry.coin.GetScriptPubKey().data(), 
                     entry.coin.GetScriptPubKey().size());
    }
    
    Hash256 result;
    hasher.Finalize(result.data());
    return result;
}

void CoinsViewCache::AddTransaction(const Transaction& tx, uint32_t height) {
    bool isCoinbase = tx.IsCoinBase();
    TxHash txhash = tx.GetHash();
    
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        const TxOut& out = tx.vout[i];
        if (!out.IsNull()) {
            AddCoin(OutPoint(txhash, static_cast<uint32_t>(i)), 
                    Coin(out, height, isCoinbase), 
                    false);
        }
    }
}

bool CoinsViewCache::HaveInputs(const Transaction& tx) const {
    if (tx.IsCoinBase()) return true;
    
    for (const auto& txin : tx.vin) {
        if (!HaveCoin(txin.prevout)) {
            return false;
        }
    }
    return true;
}

Amount CoinsViewCache::GetValueIn(const Transaction& tx) const {
    if (tx.IsCoinBase()) return 0;
    
    Amount total = 0;
    for (const auto& txin : tx.vin) {
        const Coin& coin = AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            return 0;  // Missing input
        }
        total += coin.GetAmount();
    }
    return total;
}

// ============================================================================
// CoinsViewMemory Implementation
// ============================================================================

std::optional<Coin> CoinsViewMemory::GetCoin(const OutPoint& outpoint) const {
    auto it = coins.find(outpoint);
    if (it == coins.end() || it->second.coin.IsSpent()) {
        return std::nullopt;
    }
    return it->second.coin;
}

bool CoinsViewMemory::HaveCoin(const OutPoint& outpoint) const {
    auto it = coins.find(outpoint);
    return it != coins.end() && !it->second.coin.IsSpent();
}

BlockHash CoinsViewMemory::GetBestBlock() const {
    return bestBlock;
}

size_t CoinsViewMemory::EstimateSize() const {
    return coins.size();
}

void CoinsViewMemory::AddCoin(const OutPoint& outpoint, const Coin& coin) {
    coins[outpoint] = CoinsCacheEntry(coin);
}

void CoinsViewMemory::RemoveCoin(const OutPoint& outpoint) {
    coins.erase(outpoint);
}

void CoinsViewMemory::SetBestBlock(const BlockHash& block) {
    bestBlock = block;
}

void CoinsViewMemory::Clear() {
    coins.clear();
    bestBlock.SetNull();
}

void CoinsViewMemory::BatchWrite(CoinsMap& mapCoins, const BlockHash& block) {
    for (auto& [outpoint, entry] : mapCoins) {
        if (entry.coin.IsSpent()) {
            coins.erase(outpoint);
        } else {
            coins[outpoint] = std::move(entry);
        }
    }
    bestBlock = block;
}

// ============================================================================
// UTXO Statistics
// ============================================================================

UTXOStats GetUTXOStats(const CoinsView& view) {
    UTXOStats stats;
    // This is a simplified implementation
    // Production would iterate through all UTXOs
    stats.hashSerialized.SetNull();
    return stats;
}

} // namespace shurium
