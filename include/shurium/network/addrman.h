// SHURIUM - Address Manager
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Manages known peer addresses for network connectivity.
// Handles DNS seed resolution, address storage, and peer selection.

#ifndef SHURIUM_NETWORK_ADDRMAN_H
#define SHURIUM_NETWORK_ADDRMAN_H

#include <shurium/network/address.h>
#include <shurium/core/types.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace shurium {

// ============================================================================
// Address Info
// ============================================================================

/// Extended address information for tracking
struct AddressInfo {
    /// The network address
    PeerAddress addr;
    
    /// Source of this address (who told us about it)
    NetService source;
    
    /// When we first learned about this address
    int64_t nTime{0};
    
    /// Last successful connection time
    int64_t nLastSuccess{0};
    
    /// Last attempt to connect
    int64_t nLastTry{0};
    
    /// Number of connection attempts
    int32_t nAttempts{0};
    
    /// Reference count (how many sources reported this)
    int32_t nRefCount{0};
    
    /// Is this address in the "tried" bucket (we've successfully connected)?
    bool fInTried{false};
    
    /// Random position in bucket
    int nRandomPos{-1};
    
    /// Get the address key for lookups
    std::string GetKey() const {
        // PeerAddress inherits from NetService which inherits from NetAddress
        return static_cast<const NetAddress&>(addr).ToString() + ":" + std::to_string(addr.GetPort());
    }
    
    /// Check if address is considered "terrible" (many failed attempts)
    bool IsTerrible(int64_t now) const {
        // More than 10 failed attempts with no recent success
        if (nAttempts >= 10 && nLastSuccess < nLastTry - 3600) {
            return true;
        }
        // Not seen in over a month
        if (nTime < now - 30 * 24 * 60 * 60) {
            return true;
        }
        return false;
    }
    
    /// Calculate selection chance (higher = more likely to be selected)
    double GetChance(int64_t now) const {
        double chance = 1.0;
        
        // Reduce chance based on failed attempts
        int64_t sinceLastTry = std::max(int64_t(0), now - nLastTry);
        if (sinceLastTry < 60 * 10) {
            chance *= 0.01;  // Recently tried
        }
        chance *= std::pow(0.66, std::min(nAttempts, 8));
        
        return chance;
    }
};

// ============================================================================
// DNS Seed Configuration
// ============================================================================

/// DNS seed entry
struct DNSSeed {
    std::string host;
    bool supportsSrv{false};  // SRV record support
    
    DNSSeed() = default;
    explicit DNSSeed(const std::string& h, bool srv = false)
        : host(h), supportsSrv(srv) {}
};

/// Default DNS seeds for mainnet
const std::vector<DNSSeed> MAINNET_SEEDS = {
    DNSSeed("seed1.shurium.io"),
    DNSSeed("seed2.shurium.io"),
    DNSSeed("seed3.shurium.io"),
    DNSSeed("dnsseed.shurium.community"),
};

/// Default DNS seeds for testnet
const std::vector<DNSSeed> TESTNET_SEEDS = {
    DNSSeed("testnet-seed.shurium.io"),
    DNSSeed("testnet-seed2.shurium.io"),
};

/// Default DNS seeds for regtest (none - manual connections only)
const std::vector<DNSSeed> REGTEST_SEEDS = {};

// ============================================================================
// Address Manager
// ============================================================================

/**
 * Manages known peer addresses for network connectivity.
 * 
 * Features:
 * - DNS seed resolution on startup
 * - Address storage and persistence
 * - Random selection of addresses for new connections
 * - Tracking of connection success/failure
 * - Handling of addr messages from peers
 */
class AddressManager {
public:
    /// Callback for resolved DNS seeds
    using ResolveCallback = std::function<void(const std::vector<NetService>&)>;
    
    explicit AddressManager(const std::string& networkId = "main");
    ~AddressManager();
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    /// Start the address manager
    void Start();
    
    /// Stop the address manager
    void Stop();
    
    /// Load addresses from persistent storage
    bool Load(const std::string& path);
    
    /// Save addresses to persistent storage
    bool Save(const std::string& path) const;
    
    // ========================================================================
    // Address Management
    // ========================================================================
    
    /**
     * Add a new address (e.g., from addr message).
     * @param addr The address to add
     * @param source Who told us about this address
     * @param penalty Time penalty for this address (0 for direct knowledge)
     * @return True if address was new or updated
     */
    bool Add(const PeerAddress& addr, const NetService& source, int64_t penalty = 0);
    
    /**
     * Add multiple addresses.
     * @param addrs Addresses to add
     * @param source Who told us about these addresses
     * @param penalty Time penalty
     * @return Number of new/updated addresses
     */
    size_t Add(const std::vector<PeerAddress>& addrs, const NetService& source,
               int64_t penalty = 0);
    
    /**
     * Mark a connection attempt.
     * @param addr Address that was tried
     */
    void Attempt(const NetService& addr);
    
    /**
     * Mark a successful connection.
     * @param addr Address that connected successfully
     */
    void Good(const NetService& addr);
    
    /**
     * Mark address as connected (for tracking).
     * @param addr The connected address
     */
    void Connected(const NetService& addr);
    
    /**
     * Select an address to connect to.
     * @param newOnly Only return addresses we haven't tried
     * @return Selected address, or empty optional if none available
     */
    std::optional<PeerAddress> Select(bool newOnly = false);
    
    /**
     * Select multiple addresses for connection.
     * @param count Maximum number to return
     * @param newOnly Only return addresses we haven't tried
     * @return Vector of selected addresses
     */
    std::vector<PeerAddress> SelectMany(size_t count, bool newOnly = false);
    
    /**
     * Get addresses to send in an addr message.
     * @param count Maximum number to return
     * @return Vector of addresses suitable for relay
     */
    std::vector<PeerAddress> GetAddr(size_t count = 1000) const;
    
    // ========================================================================
    // DNS Seeds
    // ========================================================================
    
    /**
     * Set DNS seeds to use.
     * @param seeds Vector of DNS seed entries
     */
    void SetSeeds(const std::vector<DNSSeed>& seeds);
    
    /**
     * Resolve DNS seeds asynchronously.
     * @param callback Called when resolution completes
     */
    void ResolveSeeds(ResolveCallback callback = nullptr);
    
    /**
     * Resolve DNS seeds synchronously.
     * @return Vector of resolved addresses
     */
    std::vector<NetService> ResolveSeedsSync();
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get total number of known addresses
    size_t Size() const;
    
    /// Get number of "tried" addresses (successfully connected)
    size_t NumTried() const;
    
    /// Get number of "new" addresses (never connected)
    size_t NumNew() const;
    
    /// Check if we have any addresses
    bool empty() const { return Size() == 0; }
    
    /// Clear all addresses
    void Clear();

private:
    /// Create AddressInfo for a new address
    AddressInfo MakeInfo(const PeerAddress& addr, const NetService& source, int64_t penalty);
    
    /// Find address info by key
    AddressInfo* Find(const std::string& key);
    const AddressInfo* Find(const std::string& key) const;
    
    /// DNS resolution helper
    std::vector<NetService> ResolveHost(const std::string& host) const;
    
    // Configuration
    std::string networkId_;
    std::vector<DNSSeed> seeds_;
    uint16_t defaultPort_{8333};
    
    // Address storage
    mutable std::mutex mutex_;
    std::map<std::string, AddressInfo> mapInfo_;
    std::vector<std::string> vNew_;   // Addresses never tried
    std::vector<std::string> vTried_; // Successfully connected addresses
    
    // Random number generator
    mutable std::mt19937 rng_;
    
    // DNS resolution thread
    std::thread resolveThread_;
    std::atomic<bool> running_{false};
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Get current Unix timestamp
inline int64_t GetAdjustedTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/// Check if an address is routable (not local/private)
bool IsRoutable(const NetAddress& addr);

/// Check if an address is valid for storing
bool IsValidForStorage(const PeerAddress& addr);

} // namespace shurium

#endif // SHURIUM_NETWORK_ADDRMAN_H
