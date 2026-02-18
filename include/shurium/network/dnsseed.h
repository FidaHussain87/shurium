// SHURIUM - DNS Seeder
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// DNS-based peer discovery for initial network bootstrap.
// Resolves DNS seed hostnames to obtain initial peer addresses.
//
// Features:
// - Standard A/AAAA record resolution
// - SRV record support for service discovery
// - Retry logic with exponential backoff
// - Concurrent resolution of multiple seeds
// - Filtering of invalid/unroutable addresses
// - Timeout handling

#ifndef SHURIUM_NETWORK_DNSSEED_H
#define SHURIUM_NETWORK_DNSSEED_H

#include <shurium/network/address.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace shurium {
namespace network {

// ============================================================================
// DNS Seed Configuration
// ============================================================================

/// Configuration for a single DNS seed
struct SeedConfig {
    /// Hostname to resolve
    std::string hostname;
    
    /// Whether this seed supports SRV records
    bool supportsSrv{false};
    
    /// Custom port (0 = use default for network)
    uint16_t port{0};
    
    /// Priority (lower = higher priority)
    int priority{100};
    
    /// Whether this is a trusted/official seed
    bool trusted{false};
    
    /// Human-readable description/operator
    std::string description;
    
    SeedConfig() = default;
    explicit SeedConfig(std::string host, bool srv = false)
        : hostname(std::move(host)), supportsSrv(srv) {}
    
    SeedConfig(std::string host, std::string desc, bool trusted = false)
        : hostname(std::move(host)), trusted(trusted), description(std::move(desc)) {}
};

/// DNS Seeder configuration
struct SeederConfig {
    /// Maximum number of addresses to collect per seed
    size_t maxAddressesPerSeed{256};
    
    /// Maximum total addresses to collect
    size_t maxTotalAddresses{1000};
    
    /// Timeout for DNS resolution (seconds)
    int timeoutSeconds{30};
    
    /// Number of retry attempts per seed
    int maxRetries{3};
    
    /// Base retry delay (milliseconds), doubles each retry
    int retryDelayMs{1000};
    
    /// Maximum concurrent DNS requests
    size_t maxConcurrent{4};
    
    /// Whether to resolve IPv4 addresses
    bool resolveIPv4{true};
    
    /// Whether to resolve IPv6 addresses
    bool resolveIPv6{true};
    
    /// Minimum required successful seeds
    size_t minSuccessfulSeeds{1};
    
    /// Filter out unroutable addresses
    bool filterUnroutable{true};
    
    /// Shuffle results for privacy
    bool shuffleResults{true};
};

// ============================================================================
// DNS Resolution Result
// ============================================================================

/// Result of resolving a single seed
struct SeedResult {
    /// The seed that was resolved
    SeedConfig seed;
    
    /// Resolved addresses
    std::vector<NetService> addresses;
    
    /// Whether resolution succeeded
    bool success{false};
    
    /// Error message if failed
    std::string error;
    
    /// Time taken for resolution
    std::chrono::milliseconds duration{0};
    
    /// Number of retry attempts made
    int retries{0};
};

/// Overall DNS seeding result
struct SeederResult {
    /// All successfully resolved addresses (deduplicated)
    std::vector<NetService> addresses;
    
    /// Results per seed
    std::vector<SeedResult> seedResults;
    
    /// Total seeds attempted
    size_t seedsAttempted{0};
    
    /// Total seeds that succeeded
    size_t seedsSucceeded{0};
    
    /// Total time taken
    std::chrono::milliseconds totalDuration{0};
    
    /// Whether overall seeding was successful (met minimum requirements)
    bool success{false};
    
    /// Summary message
    std::string message;
};

// ============================================================================
// DNS Seeder Class
// ============================================================================

/**
 * DNS-based peer discovery for network bootstrap.
 * 
 * Usage:
 *   DNSSeeder seeder;
 *   seeder.AddSeed("seed1.shurium.io");
 *   seeder.AddSeed("seed2.shurium.io");
 *   auto result = seeder.Resolve();
 *   for (const auto& addr : result.addresses) {
 *       // Use discovered peer addresses
 *   }
 * 
 * Or asynchronously:
 *   seeder.ResolveAsync([](SeederResult result) {
 *       // Handle results
 *   });
 */
class DNSSeeder {
public:
    /// Callback for async resolution
    using Callback = std::function<void(SeederResult)>;
    
    /// Progress callback (seed_index, total_seeds, current_result)
    using ProgressCallback = std::function<void(size_t, size_t, const SeedResult&)>;
    
    // ========================================================================
    // Construction
    // ========================================================================
    
    DNSSeeder();
    explicit DNSSeeder(const SeederConfig& config);
    ~DNSSeeder();
    
    // Non-copyable
    DNSSeeder(const DNSSeeder&) = delete;
    DNSSeeder& operator=(const DNSSeeder&) = delete;
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /// Set seeder configuration
    void SetConfig(const SeederConfig& config);
    
    /// Get current configuration
    const SeederConfig& GetConfig() const { return config_; }
    
    /// Set default port for resolved addresses
    void SetDefaultPort(uint16_t port) { defaultPort_ = port; }
    
    /// Get default port
    uint16_t GetDefaultPort() const { return defaultPort_; }
    
    // ========================================================================
    // Seed Management
    // ========================================================================
    
    /// Add a seed hostname
    void AddSeed(const std::string& hostname);
    
    /// Add a seed with full configuration
    void AddSeed(const SeedConfig& seed);
    
    /// Add multiple seeds
    void AddSeeds(const std::vector<SeedConfig>& seeds);
    
    /// Remove a seed by hostname
    bool RemoveSeed(const std::string& hostname);
    
    /// Clear all seeds
    void ClearSeeds();
    
    /// Get all configured seeds
    const std::vector<SeedConfig>& GetSeeds() const { return seeds_; }
    
    /// Get number of configured seeds
    size_t NumSeeds() const { return seeds_.size(); }
    
    // ========================================================================
    // Resolution
    // ========================================================================
    
    /**
     * Resolve all seeds synchronously.
     * Blocks until all seeds are resolved or timeout.
     * @return Resolution result with all discovered addresses
     */
    SeederResult Resolve();
    
    /**
     * Resolve all seeds asynchronously.
     * Returns immediately, calls callback when complete.
     * @param callback Called when resolution completes
     */
    void ResolveAsync(Callback callback);
    
    /**
     * Resolve all seeds asynchronously with progress updates.
     * @param callback Called when resolution completes
     * @param progress Called after each seed is resolved
     */
    void ResolveAsync(Callback callback, ProgressCallback progress);
    
    /**
     * Cancel ongoing async resolution.
     */
    void Cancel();
    
    /**
     * Check if resolution is in progress.
     */
    bool IsResolving() const { return resolving_.load(); }
    
    /**
     * Wait for async resolution to complete.
     * @param timeout Maximum time to wait
     * @return True if completed, false if timeout
     */
    bool Wait(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    
    // ========================================================================
    // Static Utilities
    // ========================================================================
    
    /**
     * Resolve a single hostname.
     * @param hostname The hostname to resolve
     * @param port Port to use (0 = use 8333)
     * @param ipv4 Resolve IPv4 addresses
     * @param ipv6 Resolve IPv6 addresses
     * @param timeout Timeout in seconds
     * @return Vector of resolved addresses
     */
    static std::vector<NetService> ResolveHostname(
        const std::string& hostname,
        uint16_t port = 8333,
        bool ipv4 = true,
        bool ipv6 = true,
        int timeout = 30);
    
    /**
     * Check if an address is routable.
     */
    static bool IsRoutable(const NetAddress& addr);
    
    /**
     * Get default mainnet seeds.
     */
    static std::vector<SeedConfig> GetMainnetSeeds();
    
    /**
     * Get default testnet seeds.
     */
    static std::vector<SeedConfig> GetTestnetSeeds();
    
private:
    /// Resolve a single seed
    SeedResult ResolveSeed(const SeedConfig& seed);
    
    /// Resolve a single seed with retries
    SeedResult ResolveSeedWithRetry(const SeedConfig& seed);
    
    /// Resolve hostname (internal implementation)
    std::vector<NetService> ResolveHostnameInternal(
        const std::string& hostname,
        uint16_t port) const;
    
    /// Filter and deduplicate addresses
    std::vector<NetService> FilterAddresses(
        const std::vector<NetService>& addresses) const;
    
    /// Shuffle addresses
    void ShuffleAddresses(std::vector<NetService>& addresses) const;
    
    // Configuration
    SeederConfig config_;
    uint16_t defaultPort_{8333};
    std::vector<SeedConfig> seeds_;
    
    // Resolution state
    std::atomic<bool> resolving_{false};
    std::atomic<bool> cancelled_{false};
    mutable std::mutex mutex_;
    std::thread resolveThread_;
    std::promise<SeederResult> resultPromise_;
    std::shared_future<SeederResult> resultFuture_;
};

// ============================================================================
// Predefined Seed Lists
// ============================================================================

namespace Seeds {

/// Official SHURIUM mainnet DNS seeds
const std::vector<SeedConfig> MAINNET = {
    SeedConfig("seed1.shurium.io", "SHURIUM Foundation", true),
    SeedConfig("seed2.shurium.io", "SHURIUM Foundation", true),
    SeedConfig("seed3.shurium.io", "SHURIUM Foundation", true),
    SeedConfig("dnsseed.shurium.community", "Community Seed"),
    SeedConfig("seed.shurium.network", "Network Operator"),
};

/// SHURIUM testnet DNS seeds
const std::vector<SeedConfig> TESTNET = {
    SeedConfig("testnet-seed.shurium.io", "SHURIUM Foundation", true),
    SeedConfig("testnet-seed2.shurium.io", "SHURIUM Foundation", true),
    SeedConfig("testnet.seed.shurium.community", "Community Seed"),
};

/// Signet DNS seeds
const std::vector<SeedConfig> SIGNET = {
    SeedConfig("signet-seed.shurium.io", "SHURIUM Foundation", true),
};

/// Regtest has no DNS seeds (local testing only)
const std::vector<SeedConfig> REGTEST = {};

} // namespace Seeds

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get seeds for a given network.
 * @param networkId Network identifier ("main", "test", "signet", "regtest")
 * @return Vector of seed configurations
 */
std::vector<SeedConfig> GetSeedsForNetwork(const std::string& networkId);

/**
 * Quick bootstrap: resolve seeds and return addresses.
 * Convenience function for simple usage.
 * @param networkId Network to get seeds for
 * @param maxAddresses Maximum addresses to return
 * @return Vector of peer addresses
 */
std::vector<NetService> QuickBootstrap(
    const std::string& networkId = "main",
    size_t maxAddresses = 100);

} // namespace network
} // namespace shurium

#endif // SHURIUM_NETWORK_DNSSEED_H
