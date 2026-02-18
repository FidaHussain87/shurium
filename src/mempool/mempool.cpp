// SHURIUM - Mempool Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/mempool/mempool.h"
#include "shurium/consensus/validation.h"
#include "shurium/consensus/params.h"
#include "shurium/script/interpreter.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <queue>

namespace shurium {

// ============================================================================
// FeeRate Implementation
// ============================================================================

std::string FeeRate::ToString() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    double rate = static_cast<double>(nSatoshisPerK) / 1000.0;
    ss << rate << " sat/vB";
    return ss.str();
}

// ============================================================================
// MempoolEntry Implementation
// ============================================================================

MempoolEntry::MempoolEntry(const TransactionRef& txIn, Amount feeIn, int64_t timeIn,
                           uint32_t heightIn, bool spendsCoinbaseIn)
    : tx(txIn)
    , nFee(feeIn)
    , nTxSize(txIn->GetTotalSize())
    , nTime(timeIn)
    , entryHeight(heightIn)
    , spendsCoinbase(spendsCoinbaseIn)
    , nModifiedFee(feeIn)
    , feeRate(feeIn, nTxSize)
    , nCountWithAncestors(1)
    , nSizeWithAncestors(nTxSize)
    , nModFeesWithAncestors(feeIn)
    , nCountWithDescendants(1)
    , nSizeWithDescendants(nTxSize)
    , nModFeesWithDescendants(feeIn) {
}

void MempoolEntry::UpdateAncestorState(int64_t countDelta, int64_t sizeDelta, Amount feeDelta) const {
    nCountWithAncestors = static_cast<uint64_t>(
        std::max<int64_t>(1, static_cast<int64_t>(nCountWithAncestors) + countDelta));
    nSizeWithAncestors = static_cast<uint64_t>(
        std::max<int64_t>(static_cast<int64_t>(nTxSize), static_cast<int64_t>(nSizeWithAncestors) + sizeDelta));
    nModFeesWithAncestors = std::max<Amount>(nModifiedFee, nModFeesWithAncestors + feeDelta);
}

void MempoolEntry::UpdateDescendantState(int64_t countDelta, int64_t sizeDelta, Amount feeDelta) const {
    nCountWithDescendants = static_cast<uint64_t>(
        std::max<int64_t>(1, static_cast<int64_t>(nCountWithDescendants) + countDelta));
    nSizeWithDescendants = static_cast<uint64_t>(
        std::max<int64_t>(static_cast<int64_t>(nTxSize), static_cast<int64_t>(nSizeWithDescendants) + sizeDelta));
    nModFeesWithDescendants = std::max<Amount>(nModifiedFee, nModFeesWithDescendants + feeDelta);
}

size_t MempoolEntry::DynamicMemoryUsage() const {
    size_t usage = sizeof(MempoolEntry);
    // Add transaction size
    usage += tx->GetTotalSize();
    return usage;
}

// ============================================================================
// Removal Reason String
// ============================================================================

std::string RemovalReasonToString(MempoolRemovalReason reason) {
    switch (reason) {
        case MempoolRemovalReason::UNKNOWN:   return "unknown";
        case MempoolRemovalReason::EXPIRY:    return "expiry";
        case MempoolRemovalReason::SIZELIMIT: return "sizelimit";
        case MempoolRemovalReason::REORG:     return "reorg";
        case MempoolRemovalReason::BLOCK:     return "block";
        case MempoolRemovalReason::CONFLICT:  return "conflict";
        case MempoolRemovalReason::REPLACED:  return "replaced";
        default: return "unknown";
    }
}

// ============================================================================
// Mempool Implementation
// ============================================================================

Mempool::Mempool() : limits() {}

Mempool::Mempool(const MempoolLimits& limitsIn) : limits(limitsIn) {}

bool Mempool::AddTx(const TransactionRef& tx, Amount fee, uint32_t height,
                    bool spendsCoinbase, std::string& errString) {
    std::lock_guard<std::mutex> lock(cs);
    
    TxHash txid = tx->GetHash();
    
    // Check if already in mempool
    if (mapTx.find(txid) != mapTx.end()) {
        errString = "txn-already-in-mempool";
        return false;
    }
    
    // Check minimum fee rate
    FeeRate txFeeRate(fee, tx->GetTotalSize());
    if (txFeeRate < limits.minFeeRate) {
        errString = "mempool min fee not met";
        return false;
    }
    
    // Check for conflicts
    for (const auto& txin : tx->vin) {
        auto it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            // There's a conflict - for now just reject
            // RBF would check if replacement is valid
            errString = "txn-mempool-conflict";
            return false;
        }
    }
    
    // Create entry
    MempoolEntry entry(tx, fee, GetTime(), height, spendsCoinbase);
    
    // Check ancestor limits
    if (!CheckAncestorLimits(entry, errString)) {
        return false;
    }
    
    // Add to main map
    auto [it, inserted] = mapTx.emplace(txid, std::move(entry));
    if (!inserted) {
        errString = "failed to insert into mempool";
        return false;
    }
    
    // Add to mapNextTx
    for (const auto& txin : tx->vin) {
        mapNextTx[txin.prevout] = txid;
    }
    
    // Add to fee-sorted index
    mapAncestorFee.insert(std::cref(it->second));
    
    // Update totals
    totalTxSize += it->second.GetTxSize();
    totalFees += fee;
    
    // Update ancestors of this transaction
    UpdateAncestorsOf(it, true);
    
    // Check descendant limits for ancestors and update their descendant stats
    for (const auto& txin : tx->vin) {
        auto spenderIt = mapTx.find(TxHash(txin.prevout.hash));
        if (spenderIt != mapTx.end()) {
            // This input spends a mempool tx - update its descendants
            if (!CheckDescendantLimits(spenderIt, errString)) {
                // Rollback
                RemoveUnchecked(it, MempoolRemovalReason::UNKNOWN);
                return false;
            }
            UpdateDescendantsOf(spenderIt, true);
        }
    }
    
    // Enforce size limit
    if (totalTxSize > limits.maxSize) {
        TrimToSize(limits.maxSize);
    }
    
    return true;
}

bool Mempool::CheckTx(const TransactionRef& tx, Amount fee, std::string& errString) const {
    std::lock_guard<std::mutex> lock(cs);
    
    TxHash txid = tx->GetHash();
    
    if (mapTx.find(txid) != mapTx.end()) {
        errString = "txn-already-in-mempool";
        return false;
    }
    
    FeeRate txFeeRate(fee, tx->GetTotalSize());
    if (txFeeRate < limits.minFeeRate) {
        errString = "mempool min fee not met";
        return false;
    }
    
    for (const auto& txin : tx->vin) {
        if (mapNextTx.find(txin.prevout) != mapNextTx.end()) {
            errString = "txn-mempool-conflict";
            return false;
        }
    }
    
    return true;
}

void Mempool::RemoveTxAndDescendants(const TxHash& txid, MempoolRemovalReason reason) {
    std::lock_guard<std::mutex> lock(cs);
    
    auto it = mapTx.find(txid);
    if (it == mapTx.end()) return;
    
    // Get all descendants
    std::set<TxHash> descendants = GetDescendants(txid);
    descendants.insert(txid);
    
    // Remove in reverse topological order (descendants first)
    std::vector<txiter> toRemove;
    for (const auto& hash : descendants) {
        auto descIt = mapTx.find(hash);
        if (descIt != mapTx.end()) {
            toRemove.push_back(descIt);
        }
    }
    
    // Sort by descendant count (most descendants first)
    std::sort(toRemove.begin(), toRemove.end(),
        [](const txiter& a, const txiter& b) {
            return a->second.GetCountWithDescendants() > b->second.GetCountWithDescendants();
        });
    
    for (auto& removeIt : toRemove) {
        RemoveUnchecked(removeIt, reason);
    }
}

void Mempool::RemoveConflicts(const Transaction& tx) {
    std::lock_guard<std::mutex> lock(cs);
    
    for (const auto& txin : tx.vin) {
        auto it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            auto txIt = mapTx.find(it->second);
            if (txIt != mapTx.end()) {
                // Remove the conflicting tx and its descendants
                std::set<TxHash> toRemove = GetDescendants(it->second);
                toRemove.insert(it->second);
                
                for (const auto& hash : toRemove) {
                    auto removeIt = mapTx.find(hash);
                    if (removeIt != mapTx.end()) {
                        RemoveUnchecked(removeIt, MempoolRemovalReason::CONFLICT);
                    }
                }
            }
        }
    }
}

void Mempool::RemoveForBlock(const std::vector<TransactionRef>& vtx) {
    std::lock_guard<std::mutex> lock(cs);
    
    for (const auto& tx : vtx) {
        auto it = mapTx.find(tx->GetHash());
        if (it != mapTx.end()) {
            RemoveUnchecked(it, MempoolRemovalReason::BLOCK);
        }
        
        // Also remove any conflicts
        for (const auto& txin : tx->vin) {
            auto nextIt = mapNextTx.find(txin.prevout);
            if (nextIt != mapNextTx.end()) {
                auto conflictIt = mapTx.find(nextIt->second);
                if (conflictIt != mapTx.end()) {
                    RemoveUnchecked(conflictIt, MempoolRemovalReason::CONFLICT);
                }
            }
        }
    }
}

void Mempool::Clear() {
    std::lock_guard<std::mutex> lock(cs);
    
    mapTx.clear();
    mapNextTx.clear();
    mapAncestorFee.clear();
    totalTxSize = 0;
    totalFees = 0;
}

bool Mempool::Exists(const TxHash& txid) const {
    std::lock_guard<std::mutex> lock(cs);
    return mapTx.find(txid) != mapTx.end();
}

TransactionRef Mempool::Get(const TxHash& txid) const {
    std::lock_guard<std::mutex> lock(cs);
    auto it = mapTx.find(txid);
    if (it != mapTx.end()) {
        return it->second.GetSharedTx();
    }
    return nullptr;
}

std::optional<TxMempoolInfo> Mempool::GetInfo(const TxHash& txid) const {
    std::lock_guard<std::mutex> lock(cs);
    auto it = mapTx.find(txid);
    if (it == mapTx.end()) {
        return std::nullopt;
    }
    
    TxMempoolInfo info;
    info.tx = it->second.GetSharedTx();
    info.time = it->second.GetTime();
    info.fee = it->second.GetFee();
    info.vsize = it->second.GetTxSize();
    info.feeRate = it->second.GetFeeRate();
    return info;
}

TransactionRef Mempool::GetSpender(const OutPoint& outpoint) const {
    std::lock_guard<std::mutex> lock(cs);
    auto it = mapNextTx.find(outpoint);
    if (it != mapNextTx.end()) {
        auto txIt = mapTx.find(it->second);
        if (txIt != mapTx.end()) {
            return txIt->second.GetSharedTx();
        }
    }
    return nullptr;
}

bool Mempool::IsSpent(const OutPoint& outpoint) const {
    std::lock_guard<std::mutex> lock(cs);
    return mapNextTx.find(outpoint) != mapNextTx.end();
}

bool Mempool::HasConflicts(const Transaction& tx) const {
    std::lock_guard<std::mutex> lock(cs);
    for (const auto& txin : tx.vin) {
        if (mapNextTx.find(txin.prevout) != mapNextTx.end()) {
            return true;
        }
    }
    return false;
}

FeeRate Mempool::GetMinFee() const {
    std::lock_guard<std::mutex> lock(cs);
    
    // If mempool is below half full, use configured minimum
    if (totalTxSize < limits.maxSize / 2) {
        return limits.minFeeRate;
    }
    
    // Otherwise, use the lowest fee in mempool as minimum
    if (!mapAncestorFee.empty()) {
        return mapAncestorFee.begin()->get().GetAncestorFeeRate();
    }
    
    return limits.minFeeRate;
}

std::vector<TransactionRef> Mempool::GetTransactionsForBlock(
    size_t maxSize, FeeRate minFeeRate) const {
    
    std::lock_guard<std::mutex> lock(cs);
    
    std::vector<TransactionRef> result;
    size_t currentSize = 0;
    
    // Collect all entries and sort by descendant score
    std::vector<std::reference_wrapper<const MempoolEntry>> sorted;
    sorted.reserve(mapTx.size());
    for (const auto& [txid, entry] : mapTx) {
        if (entry.GetModifiedFeeRate() >= minFeeRate) {
            sorted.push_back(std::cref(entry));
        }
    }
    
    std::sort(sorted.begin(), sorted.end(), CompareMempoolEntryByDescendantScore());
    
    // Track which transactions we've added (for parent checking)
    std::unordered_set<TxHash, TxHasher> added;
    
    // Add transactions in priority order
    for (const auto& entryRef : sorted) {
        const MempoolEntry& entry = entryRef.get();
        
        // Check size limit
        if (currentSize + entry.GetTxSize() > maxSize) {
            continue;
        }
        
        // Check that all parents are either confirmed or already added
        bool parentsOk = true;
        for (const auto& txin : entry.GetTx().vin) {
            // Check if this input spends a mempool tx
            auto parentIt = mapTx.find(TxHash(txin.prevout.hash));
            if (parentIt != mapTx.end()) {
                // Parent is in mempool - must be in our result
                if (added.find(parentIt->first) == added.end()) {
                    parentsOk = false;
                    break;
                }
            }
        }
        
        if (!parentsOk) continue;
        
        result.push_back(entry.GetSharedTx());
        added.insert(entry.GetTxHash());
        currentSize += entry.GetTxSize();
    }
    
    return result;
}

std::vector<TxMempoolInfo> Mempool::GetAllTxInfo() const {
    std::lock_guard<std::mutex> lock(cs);
    
    std::vector<TxMempoolInfo> result;
    result.reserve(mapTx.size());
    
    for (const auto& [txid, entry] : mapTx) {
        TxMempoolInfo info;
        info.tx = entry.GetSharedTx();
        info.time = entry.GetTime();
        info.fee = entry.GetFee();
        info.vsize = entry.GetTxSize();
        info.feeRate = entry.GetFeeRate();
        result.push_back(std::move(info));
    }
    
    return result;
}

void Mempool::LimitSize(int64_t currentTime) {
    std::lock_guard<std::mutex> lock(cs);
    
    // First expire old transactions
    ExpireOld(currentTime);
    
    // Then trim to size limit
    if (totalTxSize > limits.maxSize) {
        TrimToSize(limits.maxSize);
    }
}

bool Mempool::CheckConsistency() const {
    std::lock_guard<std::mutex> lock(cs);
    
    // Check mapNextTx consistency
    for (const auto& [txid, entry] : mapTx) {
        for (const auto& txin : entry.GetTx().vin) {
            auto it = mapNextTx.find(txin.prevout);
            if (it == mapNextTx.end() || it->second != txid) {
                return false;
            }
        }
    }
    
    // Check size consistency
    size_t computedSize = 0;
    for (const auto& [txid, entry] : mapTx) {
        computedSize += entry.GetTxSize();
    }
    if (computedSize != totalTxSize) {
        return false;
    }
    
    return true;
}

std::optional<Coin> Mempool::GetCoin(const OutPoint& outpoint) const {
    std::lock_guard<std::mutex> lock(cs);
    
    // Find the transaction that created this output
    auto txIt = mapTx.find(TxHash(outpoint.hash));
    if (txIt == mapTx.end()) {
        return std::nullopt;
    }
    
    const Transaction& tx = txIt->second.GetTx();
    if (outpoint.n >= tx.vout.size()) {
        return std::nullopt;
    }
    
    // Check if this output is spent by another mempool tx
    if (mapNextTx.find(outpoint) != mapNextTx.end()) {
        return std::nullopt;  // Already spent
    }
    
    return Coin(tx.vout[outpoint.n], MEMPOOL_HEIGHT, tx.IsCoinBase());
}

// ============================================================================
// Internal Helpers
// ============================================================================

void Mempool::RemoveUnchecked(txiter it, MempoolRemovalReason reason) {
    // Notify callback
    if (notifyRemoved) {
        notifyRemoved(it->second.GetSharedTx(), reason);
    }
    
    // Update ancestors' descendant stats
    UpdateAncestorsOf(it, false);
    
    // Remove from mapAncestorFee
    mapAncestorFee.erase(std::cref(it->second));
    
    // Remove from mapNextTx
    for (const auto& txin : it->second.GetTx().vin) {
        mapNextTx.erase(txin.prevout);
    }
    
    // Update totals
    totalTxSize -= it->second.GetTxSize();
    totalFees -= it->second.GetFee();
    
    // Remove from main map
    mapTx.erase(it);
}

void Mempool::UpdateAncestorsOf(txiter it, bool add) {
    // Get parents (transactions whose outputs this tx spends)
    std::set<TxHash> parents;
    for (const auto& txin : it->second.GetTx().vin) {
        TxHash parentHash(txin.prevout.hash);
        if (mapTx.find(parentHash) != mapTx.end()) {
            parents.insert(parentHash);
        }
    }
    
    // Update this entry's ancestor stats
    int64_t countDelta = add ? 1 : -1;
    size_t totalParentSize = 0;
    Amount totalParentFees = 0;
    
    for (const auto& parentHash : parents) {
        auto parentIt = mapTx.find(parentHash);
        if (parentIt != mapTx.end()) {
            totalParentSize += parentIt->second.GetSizeWithAncestors();
            totalParentFees += parentIt->second.GetModFeesWithAncestors();
        }
    }
    
    if (add) {
        it->second.UpdateAncestorState(
            static_cast<int64_t>(parents.size()),
            static_cast<int64_t>(totalParentSize),
            totalParentFees - it->second.GetModifiedFee() * static_cast<int64_t>(parents.size())
        );
    }
}

void Mempool::UpdateDescendantsOf(txiter it, bool add) {
    // This would update descendant stats for all descendants
    // Simplified implementation - would need full graph traversal for production
    int64_t countDelta = add ? 1 : -1;
    int64_t sizeDelta = add ? static_cast<int64_t>(it->second.GetTxSize()) 
                            : -static_cast<int64_t>(it->second.GetTxSize());
    Amount feeDelta = add ? it->second.GetModifiedFee() : -it->second.GetModifiedFee();
    
    it->second.UpdateDescendantState(countDelta, sizeDelta, feeDelta);
}

bool Mempool::CheckAncestorLimits(const MempoolEntry& entry, std::string& errString) const {
    // Count ancestors
    std::set<TxHash> ancestors;
    std::queue<TxHash> toProcess;
    
    for (const auto& txin : entry.GetTx().vin) {
        TxHash parentHash(txin.prevout.hash);
        if (mapTx.find(parentHash) != mapTx.end()) {
            toProcess.push(parentHash);
        }
    }
    
    while (!toProcess.empty()) {
        TxHash current = toProcess.front();
        toProcess.pop();
        
        if (ancestors.count(current)) continue;
        ancestors.insert(current);
        
        auto it = mapTx.find(current);
        if (it != mapTx.end()) {
            for (const auto& txin : it->second.GetTx().vin) {
                TxHash parentHash(txin.prevout.hash);
                if (mapTx.find(parentHash) != mapTx.end()) {
                    toProcess.push(parentHash);
                }
            }
        }
    }
    
    if (ancestors.size() + 1 > limits.maxAncestorCount) {
        errString = "too many unconfirmed ancestors";
        return false;
    }
    
    size_t ancestorSize = entry.GetTxSize();
    for (const auto& hash : ancestors) {
        auto it = mapTx.find(hash);
        if (it != mapTx.end()) {
            ancestorSize += it->second.GetTxSize();
        }
    }
    
    if (ancestorSize > limits.maxAncestorSize) {
        errString = "exceeds ancestor size limit";
        return false;
    }
    
    return true;
}

bool Mempool::CheckDescendantLimits(txiter ancestor, std::string& errString) const {
    // Get all descendants
    std::set<TxHash> descendants = GetDescendants(ancestor->first);
    
    if (descendants.size() + 1 > limits.maxDescendantCount) {
        errString = "too many descendants";
        return false;
    }
    
    size_t descendantSize = ancestor->second.GetTxSize();
    for (const auto& hash : descendants) {
        auto it = mapTx.find(hash);
        if (it != mapTx.end()) {
            descendantSize += it->second.GetTxSize();
        }
    }
    
    if (descendantSize > limits.maxDescendantSize) {
        errString = "exceeds descendant size limit";
        return false;
    }
    
    return true;
}

std::set<TxHash> Mempool::GetAncestors(const TxHash& txid) const {
    std::set<TxHash> result;
    
    auto it = mapTx.find(txid);
    if (it == mapTx.end()) return result;
    
    std::queue<TxHash> toProcess;
    for (const auto& txin : it->second.GetTx().vin) {
        TxHash parentHash(txin.prevout.hash);
        if (mapTx.find(parentHash) != mapTx.end()) {
            toProcess.push(parentHash);
        }
    }
    
    while (!toProcess.empty()) {
        TxHash current = toProcess.front();
        toProcess.pop();
        
        if (result.count(current)) continue;
        result.insert(current);
        
        auto parentIt = mapTx.find(current);
        if (parentIt != mapTx.end()) {
            for (const auto& txin : parentIt->second.GetTx().vin) {
                TxHash grandparentHash(txin.prevout.hash);
                if (mapTx.find(grandparentHash) != mapTx.end()) {
                    toProcess.push(grandparentHash);
                }
            }
        }
    }
    
    return result;
}

std::set<TxHash> Mempool::GetDescendants(const TxHash& txid) const {
    std::set<TxHash> result;
    
    auto it = mapTx.find(txid);
    if (it == mapTx.end()) return result;
    
    // Find all outputs of this transaction
    std::queue<TxHash> toProcess;
    
    const Transaction& tx = it->second.GetTx();
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        OutPoint outpoint(it->first, static_cast<uint32_t>(i));
        auto nextIt = mapNextTx.find(outpoint);
        if (nextIt != mapNextTx.end()) {
            toProcess.push(nextIt->second);
        }
    }
    
    while (!toProcess.empty()) {
        TxHash current = toProcess.front();
        toProcess.pop();
        
        if (result.count(current)) continue;
        result.insert(current);
        
        auto childIt = mapTx.find(current);
        if (childIt != mapTx.end()) {
            const Transaction& childTx = childIt->second.GetTx();
            for (size_t i = 0; i < childTx.vout.size(); ++i) {
                OutPoint outpoint(current, static_cast<uint32_t>(i));
                auto nextIt = mapNextTx.find(outpoint);
                if (nextIt != mapNextTx.end()) {
                    toProcess.push(nextIt->second);
                }
            }
        }
    }
    
    return result;
}

void Mempool::TrimToSize(size_t targetSize) {
    // Remove lowest fee-rate transactions until under target
    while (totalTxSize > targetSize && !mapAncestorFee.empty()) {
        // Get lowest fee-rate entry
        auto it = mapAncestorFee.begin();
        TxHash txid = it->get().GetTxHash();
        
        // Remove it and its descendants
        std::set<TxHash> toRemove = GetDescendants(txid);
        toRemove.insert(txid);
        
        for (const auto& hash : toRemove) {
            auto removeIt = mapTx.find(hash);
            if (removeIt != mapTx.end()) {
                RemoveUnchecked(removeIt, MempoolRemovalReason::SIZELIMIT);
            }
        }
    }
}

void Mempool::ExpireOld(int64_t currentTime) {
    std::vector<TxHash> expired;
    
    for (const auto& [txid, entry] : mapTx) {
        if (currentTime - entry.GetTime() > limits.maxAge) {
            expired.push_back(txid);
        }
    }
    
    for (const auto& txid : expired) {
        auto it = mapTx.find(txid);
        if (it != mapTx.end()) {
            RemoveUnchecked(it, MempoolRemovalReason::EXPIRY);
        }
    }
}

// ============================================================================
// MempoolCoinsView Implementation
// ============================================================================

std::optional<Coin> MempoolCoinsView::GetCoin(const OutPoint& outpoint) const {
    // First check mempool
    auto mempoolCoin = mempool.GetCoin(outpoint);
    if (mempoolCoin) {
        return mempoolCoin;
    }
    
    // Fall back to base view
    return CoinsViewBacked::GetCoin(outpoint);
}

bool MempoolCoinsView::HaveCoin(const OutPoint& outpoint) const {
    // Check if spent by mempool
    if (mempool.IsSpent(outpoint)) {
        return false;
    }
    
    // Check mempool for output
    if (mempool.GetCoin(outpoint)) {
        return true;
    }
    
    // Fall back to base view
    return CoinsViewBacked::HaveCoin(outpoint);
}

// ============================================================================
// AcceptToMempool Implementation
// ============================================================================

MempoolAcceptResult AcceptToMempool(
    const TransactionRef& tx,
    Mempool& mempool,
    CoinsView& coins,
    int32_t chainHeight,
    bool bypassLimits) {
    
    const Transaction& txRef = *tx;
    TxHash txid = txRef.GetHash();
    
    // 1. Check if already in mempool
    if (mempool.Exists(txid)) {
        return MempoolAcceptResult::Invalid("txn-already-in-mempool");
    }
    
    // 2. Basic transaction validation (context-free)
    consensus::ValidationState state;
    if (!consensus::CheckTransaction(txRef, state)) {
        return MempoolAcceptResult::Invalid(state.GetRejectReason());
    }
    
    // 3. Coinbase transactions cannot be in mempool
    if (txRef.IsCoinBase()) {
        return MempoolAcceptResult::Invalid("coinbase");
    }
    
    // 4. Create a view that includes mempool for unconfirmed chain validation
    MempoolCoinsView mempoolView(&coins, mempool);
    
    // 5. Check all inputs exist
    Amount inputValue = 0;
    bool spendsCoinbase = false;
    
    for (const auto& txin : txRef.vin) {
        auto coin = mempoolView.GetCoin(txin.prevout);
        if (!coin) {
            return MempoolAcceptResult::Invalid("missing-inputs");
        }
        
        // Check coinbase maturity (100 blocks)
        if (coin->IsCoinBase()) {
            spendsCoinbase = true;
            int32_t coinHeight = static_cast<int32_t>(coin->nHeight);
            // Mempool coins have MEMPOOL_HEIGHT
            if (coinHeight != static_cast<int32_t>(Mempool::MEMPOOL_HEIGHT)) {
                int32_t nSpendDepth = chainHeight - coinHeight;
                if (nSpendDepth < 100) {  // COINBASE_MATURITY
                    return MempoolAcceptResult::Invalid("bad-txns-premature-spend-of-coinbase");
                }
            }
        }
        
        inputValue += coin->GetAmount();
    }
    
    // 6. Calculate output value and fee
    Amount outputValue = 0;
    for (const auto& txout : txRef.vout) {
        outputValue += txout.nValue;
    }
    
    if (inputValue < outputValue) {
        return MempoolAcceptResult::Invalid("bad-txns-in-belowout");
    }
    
    Amount fee = inputValue - outputValue;
    
    // 7. Check minimum fee rate (unless bypassing limits)
    if (!bypassLimits) {
        size_t txSize = txRef.GetTotalSize();
        FeeRate txFeeRate(fee, txSize);
        FeeRate minFee = mempool.GetMinFee();
        
        if (txFeeRate < minFee) {
            std::ostringstream ss;
            ss << "min relay fee not met, " << txFeeRate.ToString() 
               << " < " << minFee.ToString();
            return MempoolAcceptResult::MempoolPolicy(ss.str());
        }
    }
    
    // 8. Verify scripts
    // Use standard script verification flags
    ScriptFlags flags = ScriptFlags::VERIFY_P2SH | 
                        ScriptFlags::VERIFY_STRICTENC |
                        ScriptFlags::VERIFY_LOW_S;
    
    for (size_t i = 0; i < txRef.vin.size(); ++i) {
        const auto& txin = txRef.vin[i];
        auto coin = mempoolView.GetCoin(txin.prevout);
        if (!coin) {
            // Should not happen - we checked above
            return MempoolAcceptResult::Invalid("missing-inputs");
        }
        
        // Create signature checker for this input
        TransactionSignatureChecker checker(&txRef, static_cast<unsigned int>(i), coin->GetAmount());
        
        ScriptError error;
        if (!VerifyScript(txin.scriptSig, coin->GetScriptPubKey(), flags, checker, &error)) {
            std::string errMsg = "mandatory-script-verify-flag-failed (";
            errMsg += ScriptErrorString(error);
            errMsg += ")";
            return MempoolAcceptResult::Invalid(errMsg);
        }
    }
    
    // 9. Check for conflicts (double spends)
    if (mempool.HasConflicts(txRef)) {
        // For now, reject conflicting transactions
        // A full implementation would support RBF (Replace By Fee)
        return MempoolAcceptResult::Invalid("txn-mempool-conflict");
    }
    
    // 10. Add to mempool
    std::string errString;
    if (!mempool.AddTx(tx, fee, static_cast<uint32_t>(chainHeight), spendsCoinbase, errString)) {
        return MempoolAcceptResult::MempoolPolicy(errString);
    }
    
    // Success!
    return MempoolAcceptResult::Success(txid, fee);
}

} // namespace shurium
