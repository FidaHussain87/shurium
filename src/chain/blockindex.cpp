// SHURIUM - Block Index Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/chain/blockindex.h"
#include <sstream>
#include <iomanip>

namespace shurium {

// ============================================================================
// BlockIndex Implementation
// ============================================================================

void BlockIndex::BuildSkip() {
    if (!pprev) {
        pskip = nullptr;
        return;
    }
    
    // Skip pointer optimization for ancestor lookup
    // pskip points to an ancestor that is a power of 2 blocks back
    // This allows O(log n) ancestor lookups
    
    // Calculate the skip height
    // Using the same algorithm as Bitcoin: skip to the highest
    // ancestor whose height has more trailing zeros than this height
    int skipHeight = nHeight & (nHeight - 1);  // Turn off lowest set bit
    
    if (skipHeight == 0) {
        // Power of 2 height - skip to genesis or halfway point
        pskip = pprev->GetAncestor(nHeight / 2);
    } else {
        // Non-power of 2 - find appropriate skip target
        pskip = pprev->GetAncestor(skipHeight);
    }
    
    if (!pskip) {
        pskip = pprev;  // Fallback to previous
    }
}

BlockIndex* BlockIndex::GetAncestor(int height) {
    if (height > nHeight || height < 0) {
        return nullptr;
    }
    
    BlockIndex* pindex = this;
    
    // Use skip pointers for efficient traversal
    while (pindex && pindex->nHeight != height) {
        if (pindex->pskip && pindex->pskip->nHeight >= height) {
            // Can use skip pointer
            pindex = pindex->pskip;
        } else if (pindex->pprev) {
            // Walk back one step
            pindex = pindex->pprev;
        } else {
            // No more ancestors
            break;
        }
    }
    
    return (pindex && pindex->nHeight == height) ? pindex : nullptr;
}

const BlockIndex* BlockIndex::GetAncestor(int height) const {
    return const_cast<BlockIndex*>(this)->GetAncestor(height);
}

std::string BlockIndex::ToString() const {
    std::ostringstream ss;
    ss << "BlockIndex(hash=";
    if (phashBlock) {
        ss << phashBlock->ToHex().substr(0, 16) << "...";
    } else {
        ss << "null";
    }
    ss << ", height=" << nHeight
       << ", version=" << nVersion
       << ", time=" << nTime
       << ", bits=" << std::hex << nBits << std::dec
       << ", nTx=" << nTx
       << ", status=" << static_cast<uint32_t>(nStatus)
       << ")";
    return ss.str();
}

// ============================================================================
// Chain Implementation
// ============================================================================

void Chain::SetTip(BlockIndex* pindex) {
    if (!pindex) {
        vChain.clear();
        return;
    }
    
    // Build the chain from genesis to pindex
    vChain.resize(pindex->nHeight + 1);
    
    while (pindex) {
        vChain[pindex->nHeight] = pindex;
        pindex = pindex->pprev;
    }
}

const BlockIndex* Chain::FindFork(const BlockIndex* pindex) const {
    if (!pindex) return nullptr;
    
    // Walk back until we find a block in our chain
    while (pindex && !Contains(pindex)) {
        pindex = pindex->pprev;
    }
    
    return pindex;
}

BlockIndex* Chain::FindEarliestAtLeast(int64_t nTime, int height) const {
    // Binary search for the first block at or after the given time and height
    int lo = height;
    int hi = Height();
    
    if (lo > hi) return nullptr;
    
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        BlockIndex* pindex = vChain[mid];
        if (pindex->GetBlockTime() < nTime) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    
    if (lo <= Height()) {
        BlockIndex* pindex = vChain[lo];
        if (pindex->GetBlockTime() >= nTime && pindex->nHeight >= height) {
            return pindex;
        }
    }
    
    return nullptr;
}

// ============================================================================
// Utility Functions
// ============================================================================

const BlockIndex* LastCommonAncestor(const BlockIndex* pa, const BlockIndex* pb) {
    if (!pa || !pb) return nullptr;
    
    // Walk back to the same height
    while (pa->nHeight > pb->nHeight) {
        pa = pa->pprev;
    }
    while (pb->nHeight > pa->nHeight) {
        pb = pb->pprev;
    }
    
    // Walk back together until we find common ancestor
    while (pa != pb && pa && pb) {
        pa = pa->pprev;
        pb = pb->pprev;
    }
    
    return pa;
}

uint64_t GetBlockProof(uint32_t nBits) {
    // Simplified proof calculation
    // In production, this would use arith_uint256 for full precision
    
    // Extract mantissa and exponent from compact format
    uint32_t nSize = nBits >> 24;
    uint32_t nWord = nBits & 0x007fffff;
    
    if (nWord == 0) return 0;
    
    // For very easy difficulty (regtest), just return a minimal work value
    // that still allows proper chain selection
    if (nSize >= 29) {
        // Very easy difficulty - return 1 so chains can still be compared
        return 1;
    }
    
    if (nSize <= 3) {
        nWord >>= 8 * (3 - nSize);
    }
    
    if (nWord == 0) return 0;
    
    // Work = 2^256 / (target + 1)
    // Simplified approximation for moderate difficulties
    uint64_t work = 0x00000000FFFFFFFFULL / nWord;
    
    // Adjust for size - shift work to approximate 2^256 / target
    if (nSize < 29) {
        // Shift work right for larger targets
        int shift = 8 * (nSize - 3);
        if (shift > 0 && shift < 64) {
            work >>= shift;
        }
    }
    
    // Ensure we return at least 1 for valid blocks
    return work > 0 ? work : 1;
}

BlockLocator GetLocator(const BlockIndex* pindex) {
    BlockLocator locator;
    
    if (!pindex) {
        return locator;
    }
    
    int step = 1;
    while (pindex) {
        locator.vHave.push_back(pindex->GetBlockHash());
        
        // Exponentially larger steps as we go back
        for (int i = 0; pindex && i < step; ++i) {
            pindex = pindex->pprev;
        }
        if (locator.vHave.size() > 10) {
            step *= 2;
        }
    }
    
    return locator;
}

} // namespace shurium
