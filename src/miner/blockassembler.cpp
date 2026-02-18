// SHURIUM - Block Assembler Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/miner/blockassembler.h"
#include "shurium/consensus/params.h"
#include "shurium/chain/blockindex.h"
#include "shurium/core/serialize.h"
#include "shurium/script/interpreter.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace shurium {
namespace miner {

// ============================================================================
// Block Assembler Implementation
// ============================================================================

BlockAssembler::BlockAssembler(ChainState& chainState,
                               Mempool& mempool,
                               const consensus::Params& params,
                               const BlockAssemblerOptions& options)
    : m_chainState(chainState),
      m_mempool(mempool),
      m_params(params),
      m_options(options) {
}

BlockTemplate BlockAssembler::CreateNewBlock(const Script& coinbaseScript) {
    // Reset state
    m_template = std::make_unique<BlockTemplate>();
    m_blockWeight = 0;
    m_blockSigops = 0;
    m_txCount = 0;
    
    // Get the tip
    BlockIndex* pindexPrev = m_chainState.GetTip();
    int32_t height = pindexPrev ? pindexPrev->nHeight + 1 : 0;
    m_template->height = height;
    
    // Set up block header
    Block& block = m_template->block;
    block.nVersion = 1;
    block.hashPrevBlock = pindexPrev ? pindexPrev->GetBlockHash() : BlockHash();
    
    // Calculate difficulty target using proper difficulty adjustment
    m_template->nBits = consensus::GetNextWorkRequired(pindexPrev, m_params);
    block.nBits = m_template->nBits;
    
    // Calculate target hash
    m_template->target = GetTargetFromBits(m_template->nBits);
    
    // Set timestamps
    int64_t now = GetTime();
    m_template->curTime = now;
    
    // Minimum time is MTP + 1 (median time past)
    if (pindexPrev && m_options.enforceMinTime) {
        m_template->minTime = pindexPrev->GetMedianTimePast() + 1;
    } else {
        m_template->minTime = 0;
    }
    
    block.nTime = static_cast<uint32_t>(std::max(m_template->minTime, now));
    block.nNonce = 0;  // Miner will find this
    
    // Reserve space for coinbase (we'll add it after selecting transactions)
    m_blockWeight += 4000;  // Estimated coinbase weight
    m_blockSigops += 1;     // Coinbase sigop
    
    // Select transactions from mempool
    AddTransactionsFromMempool();
    
    // Calculate coinbase value
    Amount subsidy = consensus::GetBlockSubsidy(height, m_params);
    m_template->coinbaseValue = subsidy + m_template->totalFees;
    
    // Create coinbase transaction
    TransactionRef coinbaseTx = CreateCoinbase(coinbaseScript);
    
    // Insert coinbase at the front
    block.vtx.insert(block.vtx.begin(), coinbaseTx);
    
    // Finalize the block
    FinalizeBlock();
    
    m_template->isValid = true;
    return std::move(*m_template);
}

BlockTemplate BlockAssembler::CreateNewBlock(const Hash160& coinbaseDest) {
    Script coinbaseScript = Script::CreateP2PKH(coinbaseDest);
    return CreateNewBlock(coinbaseScript);
}

void BlockAssembler::AddTransactionsFromMempool() {
    // Get transactions sorted by fee rate
    std::vector<TxMempoolInfo> mempoolTxs = m_mempool.GetAllTxInfo();
    
    // Sort by fee rate (highest first)
    std::sort(mempoolTxs.begin(), mempoolTxs.end(),
              [](const TxMempoolInfo& a, const TxMempoolInfo& b) {
                  // Compare fee rates (fee / vsize)
                  return (a.fee * b.vsize) > (b.fee * a.vsize);
              });
    
    // Add transactions greedily by fee rate
    for (const auto& txInfo : mempoolTxs) {
        if (!txInfo.tx) continue;
        
        // Count sigops accurately for each output script
        // This counts OP_CHECKSIG, OP_CHECKSIGVERIFY, and multisig operations
        size_t sigops = 0;
        for (const auto& txout : txInfo.tx->vout) {
            sigops += txout.scriptPubKey.GetSigOpCount(false);  // Non-accurate count for raw script
        }
        // For P2SH, accurate counting requires the input scripts to be evaluated
        // We use a conservative estimate: max 15 sigops per P2SH input (standard multisig limit)
        for (const auto& txin : txInfo.tx->vin) {
            // Check if this is spending a P2SH output (scriptSig contains data pushes)
            // Use accurate counting if we have the scriptSig
            sigops += txin.scriptSig.GetSigOpCount(true);
        }
        
        // Check if we can add this transaction
        if (!CanAddTransaction(*txInfo.tx, txInfo.fee)) {
            continue;
        }
        
        // Add to block
        AddTransaction(txInfo.tx, txInfo.fee, sigops);
    }
}

TransactionRef BlockAssembler::CreateCoinbase(const Script& coinbaseScript) {
    MutableTransaction coinbase;
    coinbase.version = 1;
    coinbase.nLockTime = 0;
    
    // Coinbase input
    OutPoint nullOutpoint;  // null hash and index -1
    
    // Create coinbase script with block height (BIP34)
    Script inputScript;
    
    // Push block height
    int32_t height = m_template->height;
    if (height < 17) {
        inputScript << static_cast<uint8_t>(OP_1 + height - 1);
    } else {
        std::vector<uint8_t> heightBytes;
        int32_t h = height;
        while (h > 0) {
            heightBytes.push_back(static_cast<uint8_t>(h & 0xFF));
            h >>= 8;
        }
        inputScript << heightBytes;
    }
    
    // Add extra nonce space (for pool mining)
    inputScript << std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00};
    
    coinbase.vin.push_back(TxIn(nullOutpoint, inputScript, 0xFFFFFFFF));
    
    // Calculate reward distribution (SHURIUM-specific)
    // Total reward is subsidy + fees
    Amount totalReward = m_template->coinbaseValue;
    
    // SHURIUM distribution:
    // - 40% to miner (useful work)
    // - 30% to UBI pool
    // - 15% to human contributions
    // - 10% to ecosystem development
    // - 5% to stability reserve
    
    // Calculate each component
    Amount minerReward = consensus::CalculateWorkReward(totalReward, m_params);
    Amount ubiReward = consensus::CalculateUBIReward(totalReward, m_params);
    Amount contributionReward = consensus::CalculateContributionReward(totalReward, m_params);
    Amount ecosystemReward = consensus::CalculateEcosystemReward(totalReward, m_params);
    Amount stabilityReward = consensus::CalculateStabilityReserve(totalReward, m_params);
    
    // Handle rounding - any remainder goes to the miner
    Amount summed = minerReward + ubiReward + contributionReward + ecosystemReward + stabilityReward;
    if (summed < totalReward) {
        minerReward += (totalReward - summed);
    }
    
    // Output 1: Miner reward (40%)
    coinbase.vout.push_back(TxOut(minerReward, coinbaseScript));
    
    // Output 2: UBI Pool (30%)
    if (ubiReward > 0) {
        Script ubiScript = Script::CreateP2PKH(m_params.ubiPoolAddress);
        coinbase.vout.push_back(TxOut(ubiReward, ubiScript));
    }
    
    // Output 3: Contribution rewards (15%)
    if (contributionReward > 0) {
        Script contribScript = Script::CreateP2PKH(m_params.contributionAddress);
        coinbase.vout.push_back(TxOut(contributionReward, contribScript));
    }
    
    // Output 4: Ecosystem development (10%)
    if (ecosystemReward > 0) {
        Script ecoScript = Script::CreateP2PKH(m_params.ecosystemAddress);
        coinbase.vout.push_back(TxOut(ecosystemReward, ecoScript));
    }
    
    // Output 5: Stability reserve (5%)
    if (stabilityReward > 0) {
        Script stabilityScript = Script::CreateP2PKH(m_params.stabilityAddress);
        coinbase.vout.push_back(TxOut(stabilityReward, stabilityScript));
    }
    
    return MakeTransactionRef(std::move(coinbase));
}

Amount BlockAssembler::CalculateCoinbaseValue() const {
    Amount subsidy = consensus::GetBlockSubsidy(m_template->height, m_params);
    return subsidy + m_template->totalFees;
}

bool BlockAssembler::CanAddTransaction(const Transaction& tx, Amount fee) const {
    // Calculate transaction weight (using virtual size for SegWit compatibility)
    // For non-witness transactions: weight = size * 4
    // For witness transactions: weight = base_size * 3 + total_size
    size_t txWeight = tx.GetTotalSize() * 4;  // Conservative: assume non-witness
    
    // Check weight limit
    if (m_blockWeight + txWeight > m_options.maxBlockWeight) {
        return false;
    }
    
    // Count sigops accurately
    size_t sigops = 0;
    for (const auto& txout : tx.vout) {
        sigops += txout.scriptPubKey.GetSigOpCount(false);
    }
    for (const auto& txin : tx.vin) {
        sigops += txin.scriptSig.GetSigOpCount(true);
    }
    
    // Check sigops limit
    if (m_blockSigops + sigops > m_options.maxBlockSigops) {
        return false;
    }
    
    // Check minimum fee rate
    if (fee < m_options.minFeeRate.GetFee(tx.GetTotalSize())) {
        return false;
    }
    
    return true;
}

void BlockAssembler::AddTransaction(const TransactionRef& tx, Amount fee, size_t sigops) {
    // Add to block
    m_template->block.vtx.push_back(tx);
    
    // Record info
    m_template->txInfo.push_back({tx, fee, sigops});
    
    // Update totals
    m_template->totalFees += fee;
    m_blockWeight += tx->GetTotalSize() * 4;
    m_blockSigops += sigops;
    ++m_txCount;
}

void BlockAssembler::FinalizeBlock() {
    // Compute merkle root
    m_template->block.hashMerkleRoot = m_template->block.ComputeMerkleRoot();
}

// ============================================================================
// Utility Functions
// ============================================================================

TransactionRef CreateCoinbaseTransaction(int32_t height,
                                          const Script& coinbaseScript,
                                          Amount reward,
                                          const consensus::Params& params) {
    MutableTransaction coinbase;
    coinbase.version = 1;
    coinbase.nLockTime = 0;
    
    // Coinbase input with null outpoint
    OutPoint nullOutpoint;
    
    // Create coinbase script with height
    Script inputScript;
    if (height < 17) {
        inputScript << static_cast<uint8_t>(OP_1 + height - 1);
    } else {
        std::vector<uint8_t> heightBytes;
        int32_t h = height;
        while (h > 0) {
            heightBytes.push_back(static_cast<uint8_t>(h & 0xFF));
            h >>= 8;
        }
        inputScript << heightBytes;
    }
    
    coinbase.vin.push_back(TxIn(nullOutpoint, inputScript, 0xFFFFFFFF));
    
    // SHURIUM reward distribution
    Amount minerReward = consensus::CalculateWorkReward(reward, params);
    Amount ubiReward = consensus::CalculateUBIReward(reward, params);
    Amount contributionReward = consensus::CalculateContributionReward(reward, params);
    Amount ecosystemReward = consensus::CalculateEcosystemReward(reward, params);
    Amount stabilityReward = consensus::CalculateStabilityReserve(reward, params);
    
    // Handle rounding
    Amount summed = minerReward + ubiReward + contributionReward + ecosystemReward + stabilityReward;
    if (summed < reward) {
        minerReward += (reward - summed);
    }
    
    // Output 1: Miner reward (40%)
    coinbase.vout.push_back(TxOut(minerReward, coinbaseScript));
    
    // Output 2: UBI Pool (30%)
    if (ubiReward > 0) {
        Script ubiScript = Script::CreateP2PKH(params.ubiPoolAddress);
        coinbase.vout.push_back(TxOut(ubiReward, ubiScript));
    }
    
    // Output 3: Contribution rewards (15%)
    if (contributionReward > 0) {
        Script contribScript = Script::CreateP2PKH(params.contributionAddress);
        coinbase.vout.push_back(TxOut(contributionReward, contribScript));
    }
    
    // Output 4: Ecosystem development (10%)
    if (ecosystemReward > 0) {
        Script ecoScript = Script::CreateP2PKH(params.ecosystemAddress);
        coinbase.vout.push_back(TxOut(ecosystemReward, ecoScript));
    }
    
    // Output 5: Stability reserve (5%)
    if (stabilityReward > 0) {
        Script stabilityScript = Script::CreateP2PKH(params.stabilityAddress);
        coinbase.vout.push_back(TxOut(stabilityReward, stabilityScript));
    }
    
    return MakeTransactionRef(std::move(coinbase));
}

std::string BlockToHex(const Block& block) {
    DataStream ss;
    Serialize(ss, block);
    
    // Rewind to read from beginning
    ss.Rewind();
    
    std::ostringstream oss;
    for (auto it = ss.begin(); it != ss.end(); ++it) {
        oss << std::hex << std::setfill('0') << std::setw(2) 
            << static_cast<unsigned int>(*it);
    }
    return oss.str();
}

std::optional<Block> HexToBlock(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }
    
    std::vector<uint8_t> data;
    data.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        try {
            std::string byteStr = hex.substr(i, 2);
            data.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
        } catch (...) {
            return std::nullopt;
        }
    }
    
    try {
        DataStream ss(data);
        Block block;
        Unserialize(ss, block);
        return block;
    } catch (...) {
        return std::nullopt;
    }
}

Hash256 GetTargetFromBits(uint32_t nBits) {
    return consensus::CompactToBig(nBits);
}

std::string TargetToHex(const Hash256& target) {
    std::ostringstream oss;
    // Big-endian display (most significant byte first)
    for (int i = 31; i >= 0; --i) {
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<unsigned int>(target[i]);
    }
    return oss.str();
}

} // namespace miner
} // namespace shurium
