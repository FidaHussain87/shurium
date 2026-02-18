// SHURIUM - CPU Miner Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/miner/miner.h"
#include "shurium/chain/chainstate.h"
#include "shurium/network/message_processor.h"
#include "shurium/crypto/sha256.h"
#include "shurium/util/logging.h"
#include "shurium/util/time.h"

#include <algorithm>
#include <cstring>
#include <thread>

namespace shurium {
namespace miner {

// ============================================================================
// Mining Statistics
// ============================================================================

double MiningStats::GetHashRate() const {
    if (startTime == 0) return 0.0;
    int64_t elapsed = GetTime() - startTime;
    if (elapsed <= 0) return 0.0;
    return static_cast<double>(hashesComputed.load()) / elapsed;
}

void MiningStats::Reset() {
    hashesComputed = 0;
    blocksFound = 0;
    blocksAccepted = 0;
    startTime = GetTime();
}

// ============================================================================
// Miner Implementation
// ============================================================================

Miner::Miner(ChainStateManager& chainman,
             Mempool& mempool,
             const consensus::Params& params,
             const MinerOptions& options)
    : chainman_(chainman),
      mempool_(mempool),
      params_(params),
      options_(options) {
}

Miner::~Miner() {
    Stop();
}

bool Miner::Start() {
    if (running_.load()) {
        return true;  // Already running
    }
    
    // Validate coinbase address
    if (options_.coinbaseAddress.IsNull()) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Cannot start mining: no coinbase address set";
        return false;
    }
    
    int numThreads = GetMiningThreadCount(options_.numThreads);
    if (numThreads <= 0) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Cannot start mining: invalid thread count";
        return false;
    }
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Starting miner with " << numThreads << " thread(s)";
    
    shouldStop_.store(false);
    running_.store(true);
    stats_.Reset();
    
    // Launch mining threads
    threads_.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        threads_.emplace_back(&Miner::MiningThread, this, i);
    }
    
    return true;
}

void Miner::Stop() {
    if (!running_.load()) {
        return;
    }
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Stopping miner...";
    
    shouldStop_.store(true);
    cv_.notify_all();
    
    // Wait for all threads to finish
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
    
    running_.store(false);
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Miner stopped. Stats: "
        << stats_.blocksFound.load() << " blocks found, "
        << stats_.blocksAccepted.load() << " accepted, "
        << static_cast<uint64_t>(stats_.GetHashRate()) << " H/s average";
}

void Miner::SetCoinbaseAddress(const Hash160& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    options_.coinbaseAddress = address;
}

void Miner::SetBlockFoundCallback(BlockFoundCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    blockFoundCallback_ = std::move(callback);
}

void Miner::MiningThread(int threadId) {
    LOG_DEBUG(util::LogCategory::DEFAULT) << "Mining thread " << threadId << " started";
    
    // Get initial chain state
    ChainState& chainState = chainman_.GetActiveChainState();
    
    while (!shouldStop_.load()) {
        try {
            // Get coinbase address (thread-safe)
            Hash160 coinbaseAddr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                coinbaseAddr = options_.coinbaseAddress;
            }
            
            if (coinbaseAddr.IsNull()) {
                // No address set, wait
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait_for(lock, std::chrono::seconds(1));
                continue;
            }
            
            // Create block template
            BlockAssembler assembler(chainState, mempool_, params_);
            BlockTemplate tmpl = assembler.CreateNewBlock(coinbaseAddr);
            
            if (!tmpl.isValid) {
                LOG_DEBUG(util::LogCategory::DEFAULT) << "Thread " << threadId 
                    << ": failed to create block template: " << tmpl.error;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            
            // Try to mine this template
            if (TryMineBlock(tmpl, threadId)) {
                // Block found! Already submitted in TryMineBlock
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "Mining thread " << threadId 
                << " error: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    LOG_DEBUG(util::LogCategory::DEFAULT) << "Mining thread " << threadId << " stopped";
}

bool Miner::TryMineBlock(BlockTemplate& tmpl, int threadId) {
    Block& block = tmpl.block;
    
    // Record template height for logging
    int32_t height = tmpl.height;
    
    // Get extra nonce for this attempt (to expand nonce space)
    uint32_t myExtraNonce = 0;
    if (options_.useExtraNonce) {
        myExtraNonce = extraNonce_.fetch_add(1);
        
        // Modify coinbase to include extra nonce
        // The coinbase scriptSig has 4 bytes reserved for extra nonce at the end
        if (!block.vtx.empty()) {
            // Get the coinbase transaction
            TransactionRef& coinbaseTxRef = block.vtx[0];
            
            // Create a mutable copy
            MutableTransaction mutableCoinbase(*coinbaseTxRef);
            
            if (!mutableCoinbase.vin.empty()) {
                Script& scriptSig = mutableCoinbase.vin[0].scriptSig;
                
                // The extra nonce bytes are the last 4 bytes of the scriptSig
                // (pushed as a data element)
                if (scriptSig.size() >= 4) {
                    // Find and update the extra nonce bytes
                    // The format is: [height push] [extra nonce push (05 00 00 00 00)]
                    // We need to update the last 4 bytes that were pushed
                    size_t extraNoncePos = scriptSig.size() - 4;
                    
                    // Encode extra nonce in little-endian
                    scriptSig[extraNoncePos + 0] = static_cast<uint8_t>(myExtraNonce & 0xFF);
                    scriptSig[extraNoncePos + 1] = static_cast<uint8_t>((myExtraNonce >> 8) & 0xFF);
                    scriptSig[extraNoncePos + 2] = static_cast<uint8_t>((myExtraNonce >> 16) & 0xFF);
                    scriptSig[extraNoncePos + 3] = static_cast<uint8_t>((myExtraNonce >> 24) & 0xFF);
                    
                    // Create new transaction ref with modified coinbase
                    block.vtx[0] = MakeTransactionRef(std::move(mutableCoinbase));
                    
                    // Recompute merkle root since coinbase changed
                    block.hashMerkleRoot = block.ComputeMerkleRoot();
                }
            }
        }
    }
    
    // Start time for template refresh
    int64_t startTime = GetTime();
    
    // Mining loop
    uint32_t nonce = 0;
    uint32_t maxNonces = options_.maxNoncesPerTemplate;
    
    while (!shouldStop_.load() && nonce < maxNonces) {
        // Update nonce
        block.nNonce = nonce;
        
        // Hash the block header
        Hash256 hash = block.GetHash();
        
        // Check if we meet the target
        if (MeetsTarget(hash, tmpl.target)) {
            // Found a valid block!
            LOG_INFO(util::LogCategory::DEFAULT) << "Thread " << threadId 
                << " found block at height " << height 
                << " with hash " << hash.ToHex().substr(0, 16) << "...";
            
            stats_.blocksFound++;
            
            // Submit the block
            bool accepted = SubmitBlock(block);
            
            // Notify callback
            BlockFoundCallback callback;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                callback = blockFoundCallback_;
            }
            if (callback) {
                callback(block, accepted);
            }
            
            return accepted;
        }
        
        ++nonce;
        stats_.hashesComputed++;
        
        // Log progress periodically (every ~1M hashes)
        if ((nonce & 0xFFFFF) == 0) {
            LOG_DEBUG(util::LogCategory::DEFAULT) << "Thread " << threadId 
                << ": " << nonce << " hashes at height " << height
                << " (" << static_cast<uint64_t>(stats_.GetHashRate()) << " H/s total)";
        }
        
        // Check if we should refresh the template (new tip, timeout)
        if ((nonce & 0xFFFF) == 0) {
            // Check for new tip
            BlockIndex* currentTip = chainman_.GetActiveTip();
            if (currentTip && currentTip->GetBlockHash() != block.hashPrevBlock) {
                LOG_DEBUG(util::LogCategory::DEFAULT) << "Thread " << threadId 
                    << ": chain tip changed, getting new template";
                return false;  // Get new template
            }
            
            // Check for timeout
            int64_t elapsed = GetTime() - startTime;
            if (elapsed >= options_.templateRefreshInterval) {
                LOG_DEBUG(util::LogCategory::DEFAULT) << "Thread " << threadId 
                    << ": template refresh timeout";
                return false;  // Get new template
            }
        }
    }
    
    // Exhausted nonce space without finding a block
    return false;
}

bool Miner::SubmitBlock(Block& block) {
    LOG_INFO(util::LogCategory::DEFAULT) << "Submitting block " << block.GetHash().ToHex().substr(0, 16) << "...";
    
    // Process the new block
    bool accepted = chainman_.ProcessNewBlock(block);
    
    if (accepted) {
        stats_.blocksAccepted++;
        LOG_INFO(util::LogCategory::DEFAULT) << "Block accepted!";
        
        // Relay to network
        if (msgproc_) {
            msgproc_->RelayBlock(block.GetHash());
        }
    } else {
        LOG_WARN(util::LogCategory::DEFAULT) << "Block rejected by chain";
    }
    
    return accepted;
}

bool Miner::MeetsTarget(const Hash256& hash, const Hash256& target) {
    // Compare hash to target (both are 256-bit, little-endian internally)
    // Hash must be <= target for proof-of-work
    // Compare from most significant byte (end of array) to least significant
    for (int i = 31; i >= 0; --i) {
        if (hash[i] < target[i]) return true;
        if (hash[i] > target[i]) return false;
    }
    return true;  // Equal
}

// ============================================================================
// Utility Functions
// ============================================================================

int GetMiningThreadCount(int requestedThreads) {
    if (requestedThreads > 0) {
        return requestedThreads;
    }
    
    // Auto-detect based on hardware
    int hwThreads = static_cast<int>(std::thread::hardware_concurrency());
    if (hwThreads <= 0) {
        hwThreads = 1;  // Fallback
    }
    
    // Use half of available cores by default (leave room for other tasks)
    int threads = std::max(1, hwThreads / 2);
    
    return threads;
}

bool CheckProofOfWork(const BlockHeader& header, const Hash256& target) {
    Hash256 hash = header.GetHash();
    return Miner::MeetsTarget(hash, target);
}

} // namespace miner
} // namespace shurium
