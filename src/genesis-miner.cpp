// SHURIUM Genesis Block Miner
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This utility mines genesis blocks by finding nonces that produce
// block hashes satisfying the specified difficulty target.

#include "shurium/core/block.h"
#include "shurium/consensus/params.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstring>

using namespace shurium;
using namespace shurium::consensus;

// Check if hash is below target (meets difficulty)
bool HashMeetsDifficulty(const BlockHash& hash, const Hash256& target) {
    return static_cast<const Hash256&>(hash) < target;
}

// Print hash in hex (using ToHex for proper big-endian display)
void PrintHash(const char* label, const Hash256& hash) {
    std::cout << label << ": " << hash.ToHex() << std::endl;
}

// Mine a genesis block
void MineGenesisBlock(const char* networkName, uint32_t nTime, uint32_t nBits, 
                       int32_t nVersion, Amount reward) {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "Mining " << networkName << " Genesis Block" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    // Convert difficulty bits to target
    Hash256 target = CompactToBig(nBits);
    PrintHash("Target", target);
    
    std::cout << "Timestamp: " << nTime << std::endl;
    std::cout << "Difficulty bits: 0x" << std::hex << nBits << std::dec << std::endl;
    std::cout << "Version: " << nVersion << std::endl;
    std::cout << "Reward: " << reward << " satoshis" << std::endl;
    std::cout << "\nStarting mining..." << std::endl;
    
    auto startTime = std::chrono::steady_clock::now();
    uint64_t hashesComputed = 0;
    uint32_t lastProgressNonce = 0;
    
    // Create genesis block with nonce = 0 initially
    Block genesis = CreateGenesisBlock(nTime, 0, nBits, nVersion, reward);
    
    // Print merkle root (stays constant during mining)
    PrintHash("Merkle Root", genesis.hashMerkleRoot);
    
    // Mine by incrementing nonce
    for (uint32_t nonce = 0; ; ++nonce) {
        genesis.nNonce = nonce;
        BlockHash hash = genesis.GetHash();
        ++hashesComputed;
        
        // Check if hash meets difficulty
        if (HashMeetsDifficulty(hash, target)) {
            auto endTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - startTime).count();
            
            std::cout << "\n*** FOUND VALID GENESIS BLOCK! ***\n" << std::endl;
            std::cout << "Nonce: " << nonce << " (0x" << std::hex << nonce << std::dec << ")" << std::endl;
            PrintHash("Block Hash", static_cast<const Hash256&>(hash));
            PrintHash("Merkle Root", genesis.hashMerkleRoot);
            std::cout << "\nMining took " << elapsed << " ms" << std::endl;
            std::cout << "Hash rate: " << (hashesComputed * 1000.0 / elapsed) << " H/s" << std::endl;
            
            // Print code snippet for params.cpp
            std::cout << "\n// Code for params.cpp:" << std::endl;
            std::cout << "Block genesis = CreateGenesisBlock(" << std::endl;
            std::cout << "    " << nTime << ",    // Timestamp" << std::endl;
            std::cout << "    " << nonce << ",    // Nonce (mined)" << std::endl;
            std::cout << "    0x" << std::hex << nBits << std::dec << ",  // Difficulty" << std::endl;
            std::cout << "    " << nVersion << ",             // Version" << std::endl;
            std::cout << "    params.nInitialBlockReward" << std::endl;
            std::cout << ");" << std::endl;
            
            // Print expected hash in hex (for use in params)
            std::cout << "\n// Expected genesis hash: " << hash.ToHex() << std::endl;
            
            return;
        }
        
        // Progress report every million hashes
        if (nonce - lastProgressNonce >= 1000000) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                currentTime - startTime).count();
            
            double hashRate = elapsed > 0 ? hashesComputed / static_cast<double>(elapsed) : 0;
            
            std::cout << "Progress: " << nonce << " nonces tested, "
                      << std::fixed << std::setprecision(2) << hashRate << " H/s"
                      << std::endl;
            
            lastProgressNonce = nonce;
        }
        
        // Handle nonce overflow (shouldn't happen with reasonable difficulty)
        if (nonce == 0xFFFFFFFF) {
            std::cout << "ERROR: Nonce space exhausted without finding valid hash!" << std::endl;
            std::cout << "Consider using lower difficulty or different timestamp." << std::endl;
            return;
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "SHURIUM Genesis Block Miner" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Get initial block reward from params
    auto mainParams = Params::Main();
    Amount initialReward = mainParams.nInitialBlockReward;
    
    std::cout << "Initial block reward: " << initialReward << " satoshis" << std::endl;
    std::cout << "                    = " << (initialReward / COIN) << " NXS" << std::endl;
    
    // Parse command line arguments
    std::string network = "all";
    if (argc > 1) {
        network = argv[1];
    }
    
    // Timestamps for each network (using current approximate time)
    const uint32_t MAINNET_TIME = 1700000000;   // Nov 14, 2023 (example)
    const uint32_t TESTNET_TIME = 1700000001;
    const uint32_t REGTEST_TIME = 1700000002;
    
    // Difficulty targets (nBits format)
    // For development, use easier targets that can be mined in seconds:
    // 0x1e0fffff = target starts with 0x0fffff... (needs 1 leading zero byte)
    // 0x1d00ffff = Bitcoin's original (needs ~4 leading zero bytes) - TOO HARD for dev
    // 0x207fffff = regtest (extremely easy, target = 0x7fffff...)
    const uint32_t MAINNET_BITS = 0x1e0fffff;   // Easy enough for development mining
    const uint32_t TESTNET_BITS = 0x1e0fffff;   // Same as mainnet
    const uint32_t REGTEST_BITS = 0x207fffff;   // Very easy for instant mining
    
    if (network == "regtest" || network == "all") {
        // RegTest should be instant (difficulty is extremely easy)
        MineGenesisBlock("RegTest", REGTEST_TIME, REGTEST_BITS, 1, initialReward);
    }
    
    if (network == "testnet" || network == "all") {
        MineGenesisBlock("TestNet", TESTNET_TIME, TESTNET_BITS, 1, initialReward);
    }
    
    if (network == "mainnet" || network == "all") {
        MineGenesisBlock("MainNet", MAINNET_TIME, MAINNET_BITS, 1, initialReward);
    }
    
    if (network != "regtest" && network != "testnet" && network != "mainnet" && network != "all") {
        std::cout << "Usage: " << argv[0] << " [regtest|testnet|mainnet|all]" << std::endl;
        std::cout << "Default: all" << std::endl;
        return 1;
    }
    
    return 0;
}
