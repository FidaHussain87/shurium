// SHURIUM - DNS Seeder Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/network/dnsseed.h>
#include <shurium/util/logging.h>

#include <algorithm>
#include <cstring>
#include <random>
#include <set>
#include <sstream>

// Platform-specific includes for DNS resolution
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windns.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dnsapi.lib")
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <resolv.h>
#include <arpa/nameser.h>
#endif

namespace shurium {
namespace network {

// ============================================================================
// DNSSeeder Implementation
// ============================================================================

DNSSeeder::DNSSeeder() = default;

DNSSeeder::DNSSeeder(const SeederConfig& config)
    : config_(config) {}

DNSSeeder::~DNSSeeder() {
    Cancel();
    if (resolveThread_.joinable()) {
        resolveThread_.join();
    }
}

void DNSSeeder::SetConfig(const SeederConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void DNSSeeder::AddSeed(const std::string& hostname) {
    AddSeed(SeedConfig(hostname));
}

void DNSSeeder::AddSeed(const SeedConfig& seed) {
    std::lock_guard<std::mutex> lock(mutex_);
    seeds_.push_back(seed);
}

void DNSSeeder::AddSeeds(const std::vector<SeedConfig>& seeds) {
    std::lock_guard<std::mutex> lock(mutex_);
    seeds_.insert(seeds_.end(), seeds.begin(), seeds.end());
}

bool DNSSeeder::RemoveSeed(const std::string& hostname) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(seeds_.begin(), seeds_.end(),
        [&hostname](const SeedConfig& s) { return s.hostname == hostname; });
    if (it != seeds_.end()) {
        seeds_.erase(it);
        return true;
    }
    return false;
}

void DNSSeeder::ClearSeeds() {
    std::lock_guard<std::mutex> lock(mutex_);
    seeds_.clear();
}

// ============================================================================
// Resolution
// ============================================================================

SeederResult DNSSeeder::Resolve() {
    auto startTime = std::chrono::steady_clock::now();
    
    SeederResult result;
    std::vector<SeedConfig> seedsCopy;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        seedsCopy = seeds_;
    }
    
    if (seedsCopy.empty()) {
        result.message = "No DNS seeds configured";
        return result;
    }
    
    // Sort seeds by priority
    std::sort(seedsCopy.begin(), seedsCopy.end(),
        [](const SeedConfig& a, const SeedConfig& b) {
            return a.priority < b.priority;
        });
    
    resolving_.store(true);
    cancelled_.store(false);
    
    result.seedsAttempted = seedsCopy.size();
    
    // Collect all addresses
    std::set<std::string> seenAddresses;
    
    for (const auto& seed : seedsCopy) {
        if (cancelled_.load()) {
            break;
        }
        
        // Check if we have enough addresses
        if (result.addresses.size() >= config_.maxTotalAddresses) {
            break;
        }
        
        LOG_INFO(util::LogCategory::NET) << "Resolving DNS seed: " << seed.hostname;
        
        SeedResult seedResult = ResolveSeedWithRetry(seed);
        result.seedResults.push_back(seedResult);
        
        if (seedResult.success) {
            result.seedsSucceeded++;
            
            // Add unique addresses
            for (const auto& addr : seedResult.addresses) {
                std::string key = addr.ToString();
                if (seenAddresses.find(key) == seenAddresses.end()) {
                    seenAddresses.insert(key);
                    result.addresses.push_back(addr);
                    
                    if (result.addresses.size() >= config_.maxTotalAddresses) {
                        break;
                    }
                }
            }
            
            LOG_INFO(util::LogCategory::NET) << "Resolved " << seedResult.addresses.size()
                                             << " addresses from " << seed.hostname;
        } else {
            LOG_WARN(util::LogCategory::NET) << "Failed to resolve " << seed.hostname
                                              << ": " << seedResult.error;
        }
    }
    
    resolving_.store(false);
    
    // Filter addresses if configured
    if (config_.filterUnroutable) {
        result.addresses = FilterAddresses(result.addresses);
    }
    
    // Shuffle results if configured
    if (config_.shuffleResults) {
        ShuffleAddresses(result.addresses);
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    
    // Determine success
    result.success = result.seedsSucceeded >= config_.minSuccessfulSeeds &&
                     !result.addresses.empty();
    
    // Build summary message
    std::ostringstream msg;
    msg << "DNS seeding complete: " << result.seedsSucceeded << "/" 
        << result.seedsAttempted << " seeds succeeded, "
        << result.addresses.size() << " addresses discovered in "
        << result.totalDuration.count() << "ms";
    result.message = msg.str();
    
    LOG_INFO(util::LogCategory::NET) << result.message;
    
    return result;
}

void DNSSeeder::ResolveAsync(Callback callback) {
    ResolveAsync(callback, nullptr);
}

void DNSSeeder::ResolveAsync(Callback callback, ProgressCallback progress) {
    // Cancel any existing resolution
    if (resolving_.load()) {
        Cancel();
        if (resolveThread_.joinable()) {
            resolveThread_.join();
        }
    }
    
    // Create promise/future for result
    resultPromise_ = std::promise<SeederResult>();
    resultFuture_ = resultPromise_.get_future().share();
    
    resolveThread_ = std::thread([this, callback, progress]() {
        SeederResult result = Resolve();
        
        try {
            resultPromise_.set_value(result);
        } catch (const std::future_error&) {
            // Promise already satisfied
        }
        
        if (callback) {
            callback(result);
        }
    });
}

void DNSSeeder::Cancel() {
    cancelled_.store(true);
}

bool DNSSeeder::Wait(std::chrono::milliseconds timeout) {
    if (!resultFuture_.valid()) {
        return true;
    }
    
    if (timeout == std::chrono::milliseconds::max()) {
        resultFuture_.wait();
        return true;
    }
    
    return resultFuture_.wait_for(timeout) == std::future_status::ready;
}

// ============================================================================
// Internal Resolution
// ============================================================================

SeedResult DNSSeeder::ResolveSeedWithRetry(const SeedConfig& seed) {
    SeedResult result;
    result.seed = seed;
    
    auto startTime = std::chrono::steady_clock::now();
    
    int delayMs = config_.retryDelayMs;
    
    for (int attempt = 0; attempt <= config_.maxRetries; ++attempt) {
        if (cancelled_.load()) {
            result.error = "Cancelled";
            break;
        }
        
        if (attempt > 0) {
            // Wait before retry with exponential backoff
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            delayMs *= 2;  // Exponential backoff
            result.retries = attempt;
        }
        
        result = ResolveSeed(seed);
        
        if (result.success) {
            break;
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    
    return result;
}

SeedResult DNSSeeder::ResolveSeed(const SeedConfig& seed) {
    SeedResult result;
    result.seed = seed;
    
    uint16_t port = seed.port > 0 ? seed.port : defaultPort_;
    
    try {
        auto addresses = ResolveHostnameInternal(seed.hostname, port);
        
        // Limit addresses per seed
        if (addresses.size() > config_.maxAddressesPerSeed) {
            // Shuffle and truncate
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(addresses.begin(), addresses.end(), g);
            addresses.resize(config_.maxAddressesPerSeed);
        }
        
        result.addresses = std::move(addresses);
        result.success = !result.addresses.empty();
        
        if (!result.success) {
            result.error = "No addresses resolved";
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
    }
    
    return result;
}

std::vector<NetService> DNSSeeder::ResolveHostnameInternal(
    const std::string& hostname,
    uint16_t port) const {
    
    std::vector<NetService> result;
    
    struct addrinfo hints{};
    struct addrinfo* res = nullptr;
    
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_ADDRCONFIG;   // Only return addresses we can use
    
    // Set address family based on config
    if (config_.resolveIPv4 && config_.resolveIPv6) {
        hints.ai_family = AF_UNSPEC;
    } else if (config_.resolveIPv4) {
        hints.ai_family = AF_INET;
    } else if (config_.resolveIPv6) {
        hints.ai_family = AF_INET6;
    } else {
        return result;  // Nothing to resolve
    }
    
    std::string portStr = std::to_string(port);
    
    int status = getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &res);
    if (status != 0) {
        throw std::runtime_error(std::string("DNS resolution failed: ") + 
                                 gai_strerror(status));
    }
    
    // Process all results
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        uint16_t resolvedPort = port;
        
        if (p->ai_family == AF_INET) {
            // IPv4
            struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
            const uint8_t* ipBytes = reinterpret_cast<const uint8_t*>(&ipv4->sin_addr);
            
            std::array<uint8_t, 4> ipv4Arr;
            std::memcpy(ipv4Arr.data(), ipBytes, 4);
            
            NetAddress addr(ipv4Arr);
            
            // Use resolved port if available, otherwise default
            uint16_t sockPort = ntohs(ipv4->sin_port);
            if (sockPort > 0) resolvedPort = sockPort;
            
            result.emplace_back(addr, resolvedPort);
            
        } else if (p->ai_family == AF_INET6) {
            // IPv6
            struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(p->ai_addr);
            const uint8_t* ipBytes = reinterpret_cast<const uint8_t*>(&ipv6->sin6_addr);
            
            std::array<uint8_t, 16> ipv6Arr;
            std::memcpy(ipv6Arr.data(), ipBytes, 16);
            
            NetAddress addr(ipv6Arr);
            
            // Use resolved port if available, otherwise default
            uint16_t sockPort = ntohs(ipv6->sin6_port);
            if (sockPort > 0) resolvedPort = sockPort;
            
            result.emplace_back(addr, resolvedPort);
        }
    }
    
    freeaddrinfo(res);
    
    return result;
}

std::vector<NetService> DNSSeeder::FilterAddresses(
    const std::vector<NetService>& addresses) const {
    
    std::vector<NetService> filtered;
    filtered.reserve(addresses.size());
    
    for (const auto& addr : addresses) {
        if (IsRoutable(addr)) {
            filtered.push_back(addr);
        }
    }
    
    return filtered;
}

void DNSSeeder::ShuffleAddresses(std::vector<NetService>& addresses) const {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(addresses.begin(), addresses.end(), g);
}

// ============================================================================
// Static Utilities
// ============================================================================

std::vector<NetService> DNSSeeder::ResolveHostname(
    const std::string& hostname,
    uint16_t port,
    bool ipv4,
    bool ipv6,
    int timeout) {
    
    DNSSeeder seeder;
    SeederConfig config;
    config.resolveIPv4 = ipv4;
    config.resolveIPv6 = ipv6;
    config.timeoutSeconds = timeout;
    seeder.SetConfig(config);
    seeder.SetDefaultPort(port);
    
    try {
        return seeder.ResolveHostnameInternal(hostname, port);
    } catch (const std::exception&) {
        return {};
    }
}

bool DNSSeeder::IsRoutable(const NetAddress& addr) {
    // Check for invalid/unroutable addresses
    auto bytes = addr.GetBytes();
    
    if (addr.IsIPv4() && bytes.size() >= 4) {
        // 0.0.0.0/8 - Current network
        if (bytes[0] == 0) return false;
        
        // 10.0.0.0/8 - Private
        if (bytes[0] == 10) return false;
        
        // 127.0.0.0/8 - Loopback
        if (bytes[0] == 127) return false;
        
        // 169.254.0.0/16 - Link-local
        if (bytes[0] == 169 && bytes[1] == 254) return false;
        
        // 172.16.0.0/12 - Private
        if (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) return false;
        
        // 192.168.0.0/16 - Private
        if (bytes[0] == 192 && bytes[1] == 168) return false;
        
        // 224.0.0.0/4 - Multicast
        if (bytes[0] >= 224 && bytes[0] <= 239) return false;
        
        // 240.0.0.0/4 - Reserved
        if (bytes[0] >= 240) return false;
        
        // 255.255.255.255 - Broadcast
        if (bytes[0] == 255 && bytes[1] == 255 && 
            bytes[2] == 255 && bytes[3] == 255) return false;
    }
    
    if (addr.IsIPv6() && bytes.size() >= 16) {
        // ::1 - Loopback
        bool isLoopback = true;
        for (size_t i = 0; i < 15; ++i) {
            if (bytes[i] != 0) { isLoopback = false; break; }
        }
        if (isLoopback && bytes[15] == 1) return false;
        
        // :: - Unspecified
        bool isUnspecified = true;
        for (size_t i = 0; i < 16; ++i) {
            if (bytes[i] != 0) { isUnspecified = false; break; }
        }
        if (isUnspecified) return false;
        
        // fe80::/10 - Link-local
        if (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80) return false;
        
        // fc00::/7 - Unique local
        if ((bytes[0] & 0xfe) == 0xfc) return false;
        
        // ff00::/8 - Multicast
        if (bytes[0] == 0xff) return false;
    }
    
    // Check for all zeros
    bool allZeros = true;
    for (auto b : bytes) {
        if (b != 0) { allZeros = false; break; }
    }
    if (allZeros) return false;
    
    return true;
}

std::vector<SeedConfig> DNSSeeder::GetMainnetSeeds() {
    return Seeds::MAINNET;
}

std::vector<SeedConfig> DNSSeeder::GetTestnetSeeds() {
    return Seeds::TESTNET;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::vector<SeedConfig> GetSeedsForNetwork(const std::string& networkId) {
    if (networkId == "main" || networkId == "mainnet") {
        return Seeds::MAINNET;
    } else if (networkId == "test" || networkId == "testnet") {
        return Seeds::TESTNET;
    } else if (networkId == "signet") {
        return Seeds::SIGNET;
    } else {
        return Seeds::REGTEST;  // Empty for regtest
    }
}

std::vector<NetService> QuickBootstrap(const std::string& networkId, size_t maxAddresses) {
    DNSSeeder seeder;
    
    auto seeds = GetSeedsForNetwork(networkId);
    if (seeds.empty()) {
        return {};
    }
    
    seeder.AddSeeds(seeds);
    
    SeederConfig config;
    config.maxTotalAddresses = maxAddresses;
    config.shuffleResults = true;
    config.filterUnroutable = true;
    seeder.SetConfig(config);
    
    // Set appropriate default port
    if (networkId == "test" || networkId == "testnet") {
        seeder.SetDefaultPort(18333);
    } else if (networkId == "signet") {
        seeder.SetDefaultPort(38333);
    } else if (networkId == "regtest") {
        seeder.SetDefaultPort(18444);
    } else {
        seeder.SetDefaultPort(8333);
    }
    
    auto result = seeder.Resolve();
    return result.addresses;
}

} // namespace network
} // namespace shurium
