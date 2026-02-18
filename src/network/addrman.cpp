// SHURIUM - Address Manager Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/network/addrman.h>
#include <shurium/util/logging.h>
#include <shurium/core/serialize.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <random>

// Include system headers for DNS resolution
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace shurium {

// ============================================================================
// AddressManager Implementation
// ============================================================================

AddressManager::AddressManager(const std::string& networkId)
    : networkId_(networkId)
    , rng_(std::random_device{}()) {
    
    // Set default seeds based on network
    if (networkId == "main") {
        seeds_ = MAINNET_SEEDS;
        defaultPort_ = 8333;
    } else if (networkId == "test") {
        seeds_ = TESTNET_SEEDS;
        defaultPort_ = 18333;
    } else {
        seeds_ = REGTEST_SEEDS;
        defaultPort_ = 18444;
    }
}

AddressManager::~AddressManager() {
    Stop();
}

void AddressManager::Start() {
    running_ = true;
}

void AddressManager::Stop() {
    running_ = false;
    
    if (resolveThread_.joinable()) {
        resolveThread_.join();
    }
}

bool AddressManager::Load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        LOG_DEBUG(util::LogCategory::NET) << "No peers file found at " << path;
        return true;  // Not an error - file may not exist yet
    }
    
    // Read file contents
    std::vector<Byte> data((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    
    if (data.size() < 16) {
        LOG_WARN(util::LogCategory::NET) << "Peers file too small: " << data.size() << " bytes";
        return false;
    }
    
    try {
        DataStream stream(data);
        
        // === Read header ===
        // Magic: "NXPR" (SHURIUM PeeRs)
        uint32_t magic;
        Unserialize(stream, magic);
        if (magic != 0x4E585052) {
            LOG_WARN(util::LogCategory::NET) << "Invalid peers file magic";
            return false;
        }
        
        // Version
        uint32_t version;
        Unserialize(stream, version);
        if (version > 1) {
            LOG_WARN(util::LogCategory::NET) << "Unsupported peers file version: " << version;
            return false;
        }
        
        // Network ID length and string
        std::string fileNetworkId;
        Unserialize(stream, fileNetworkId);
        
        // Verify network matches
        if (fileNetworkId != networkId_) {
            LOG_WARN(util::LogCategory::NET) << "Peers file network mismatch: expected " 
                                                << networkId_ << ", got " << fileNetworkId;
            return false;
        }
        
        // === Read address entries ===
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Clear existing data
        mapInfo_.clear();
        vNew_.clear();
        vTried_.clear();
        
        uint32_t count;
        Unserialize(stream, count);
        
        // Sanity check
        if (count > 100000) {
            LOG_WARN(util::LogCategory::NET) << "Peers file has too many entries: " << count;
            return false;
        }
        
        for (uint32_t i = 0; i < count; ++i) {
            AddressInfo info;
            
            // Read PeerAddress (using member Unserialize method)
            info.addr.Unserialize(stream);
            
            // Read source NetService (using member Unserialize method)
            info.source.Unserialize(stream);
            
            // Read metadata
            Unserialize(stream, info.nTime);
            Unserialize(stream, info.nLastSuccess);
            Unserialize(stream, info.nLastTry);
            Unserialize(stream, info.nAttempts);
            Unserialize(stream, info.nRefCount);
            
            uint8_t flags;
            Unserialize(stream, flags);
            info.fInTried = (flags & 0x01) != 0;
            
            // Generate key and store
            std::string key = info.GetKey();
            mapInfo_[key] = std::move(info);
            
            // Add to appropriate bucket
            if (mapInfo_[key].fInTried) {
                vTried_.push_back(key);
            } else {
                vNew_.push_back(key);
            }
        }
        
        LOG_INFO(util::LogCategory::NET) << "Loaded " << count << " peer addresses from " << path
                                         << " (" << vTried_.size() << " tried, " 
                                         << vNew_.size() << " new)";
        return true;
        
    } catch (const std::exception& e) {
        LOG_WARN(util::LogCategory::NET) << "Error loading peers file: " << e.what();
        return false;
    }
}

bool AddressManager::Save(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    DataStream stream;
    
    // === Write header ===
    // Magic: "NXPR" (SHURIUM PeeRs)
    uint32_t magic = 0x4E585052;
    Serialize(stream, magic);
    
    // Version
    uint32_t version = 1;
    Serialize(stream, version);
    
    // Network ID
    Serialize(stream, networkId_);
    
    // === Write address entries ===
    uint32_t count = static_cast<uint32_t>(mapInfo_.size());
    Serialize(stream, count);
    
    for (const auto& [key, info] : mapInfo_) {
        // Write PeerAddress (using member Serialize method)
        info.addr.Serialize(stream);
        
        // Write source NetService (using member Serialize method)
        info.source.Serialize(stream);
        
        // Write metadata
        Serialize(stream, info.nTime);
        Serialize(stream, info.nLastSuccess);
        Serialize(stream, info.nLastTry);
        Serialize(stream, info.nAttempts);
        Serialize(stream, info.nRefCount);
        
        // Flags
        uint8_t flags = 0;
        if (info.fInTried) flags |= 0x01;
        Serialize(stream, flags);
    }
    
    // === Write to file ===
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        LOG_WARN(util::LogCategory::NET) << "Failed to open peers file for writing: " << path;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(stream.data()), stream.size());
    if (!file.good()) {
        LOG_WARN(util::LogCategory::NET) << "Failed to write peers file: " << path;
        return false;
    }
    
    LOG_INFO(util::LogCategory::NET) << "Saved " << count << " peer addresses to " << path;
    return true;
}

// ============================================================================
// Address Management
// ============================================================================

bool AddressManager::Add(const PeerAddress& addr, const NetService& source, int64_t penalty) {
    if (!IsValidForStorage(addr)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // PeerAddress inherits from NetService which inherits from NetAddress
    // Use NetAddress::ToString() for the address and GetPort() for the port
    std::string key = static_cast<const NetAddress&>(addr).ToString() + ":" + std::to_string(addr.GetPort());
    
    auto it = mapInfo_.find(key);
    if (it != mapInfo_.end()) {
        // Address already known - update reference count
        it->second.nRefCount++;
        return false;
    }
    
    // Add new address
    AddressInfo info = MakeInfo(addr, source, penalty);
    mapInfo_[key] = std::move(info);
    vNew_.push_back(key);
    
    return true;
}

size_t AddressManager::Add(const std::vector<PeerAddress>& addrs, const NetService& source,
                           int64_t penalty) {
    size_t added = 0;
    for (const auto& addr : addrs) {
        if (Add(addr, source, penalty)) {
            ++added;
        }
    }
    return added;
}

void AddressManager::Attempt(const NetService& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = addr.ToString();
    auto it = mapInfo_.find(key);
    if (it != mapInfo_.end()) {
        it->second.nLastTry = GetAdjustedTime();
        it->second.nAttempts++;
    }
}

void AddressManager::Good(const NetService& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = addr.ToString();
    auto it = mapInfo_.find(key);
    if (it == mapInfo_.end()) {
        return;
    }
    
    int64_t now = GetAdjustedTime();
    it->second.nLastSuccess = now;
    it->second.nLastTry = now;
    it->second.nAttempts = 0;
    
    // Move from new to tried if not already there
    if (!it->second.fInTried) {
        it->second.fInTried = true;
        
        // Remove from vNew_ and add to vTried_
        auto newIt = std::find(vNew_.begin(), vNew_.end(), key);
        if (newIt != vNew_.end()) {
            vNew_.erase(newIt);
        }
        vTried_.push_back(key);
    }
}

void AddressManager::Connected(const NetService& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = addr.ToString();
    auto it = mapInfo_.find(key);
    if (it != mapInfo_.end()) {
        it->second.nTime = GetAdjustedTime();
    }
}

std::optional<PeerAddress> AddressManager::Select(bool newOnly) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (mapInfo_.empty()) {
        return std::nullopt;
    }
    
    int64_t now = GetAdjustedTime();
    
    // Build list of candidates
    std::vector<std::pair<double, std::string>> candidates;
    
    const auto& pool = newOnly ? vNew_ : (vTried_.empty() ? vNew_ : 
                       (rng_() % 2 == 0 ? vTried_ : vNew_));
    
    for (const auto& key : pool) {
        auto it = mapInfo_.find(key);
        if (it == mapInfo_.end()) continue;
        
        if (it->second.IsTerrible(now)) continue;
        
        double chance = it->second.GetChance(now);
        if (chance > 0) {
            candidates.emplace_back(chance, key);
        }
    }
    
    if (candidates.empty()) {
        return std::nullopt;
    }
    
    // Weighted random selection
    double total = 0;
    for (const auto& [chance, _] : candidates) {
        total += chance;
    }
    
    std::uniform_real_distribution<double> dist(0, total);
    double target = dist(rng_);
    
    double sum = 0;
    for (const auto& [chance, key] : candidates) {
        sum += chance;
        if (sum >= target) {
            auto it = mapInfo_.find(key);
            if (it != mapInfo_.end()) {
                return it->second.addr;
            }
        }
    }
    
    // Fallback to first candidate
    auto it = mapInfo_.find(candidates.front().second);
    if (it != mapInfo_.end()) {
        return it->second.addr;
    }
    
    return std::nullopt;
}

std::vector<PeerAddress> AddressManager::SelectMany(size_t count, bool newOnly) {
    std::vector<PeerAddress> result;
    result.reserve(count);
    
    std::set<std::string> selected;
    
    for (size_t i = 0; i < count * 2 && result.size() < count; ++i) {
        auto addr = Select(newOnly);
        if (addr) {
            std::string key = static_cast<const NetAddress&>(*addr).ToString() + ":" + std::to_string(addr->GetPort());
            if (selected.find(key) == selected.end()) {
                result.push_back(*addr);
                selected.insert(key);
            }
        }
    }
    
    return result;
}

std::vector<PeerAddress> AddressManager::GetAddr(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<PeerAddress> result;
    int64_t now = GetAdjustedTime();
    
    // Collect all non-terrible addresses
    std::vector<std::string> candidates;
    for (const auto& [key, info] : mapInfo_) {
        if (!info.IsTerrible(now)) {
            candidates.push_back(key);
        }
    }
    
    // Shuffle and take up to count
    std::shuffle(candidates.begin(), candidates.end(), 
                 std::mt19937(std::random_device{}()));
    
    for (size_t i = 0; i < std::min(count, candidates.size()); ++i) {
        auto it = mapInfo_.find(candidates[i]);
        if (it != mapInfo_.end()) {
            result.push_back(it->second.addr);
        }
    }
    
    return result;
}

// ============================================================================
// DNS Seeds
// ============================================================================

void AddressManager::SetSeeds(const std::vector<DNSSeed>& seeds) {
    seeds_ = seeds;
}

void AddressManager::ResolveSeeds(ResolveCallback callback) {
    if (seeds_.empty()) {
        if (callback) {
            callback({});
        }
        return;
    }
    
    resolveThread_ = std::thread([this, callback]() {
        auto addresses = ResolveSeedsSync();
        if (callback) {
            callback(addresses);
        }
    });
}

std::vector<NetService> AddressManager::ResolveSeedsSync() {
    std::vector<NetService> result;
    
    for (const auto& seed : seeds_) {
        if (!running_) break;
        
        LOG_INFO(util::LogCategory::NET) << "Resolving DNS seed: " << seed.host;
        
        auto addresses = ResolveHost(seed.host);
        for (const auto& addr : addresses) {
            result.push_back(addr);
            
            // Also add to our address pool
            // PeerAddress constructor: PeerAddress(NetService, time, services)
            PeerAddress peerAddr(addr, GetAdjustedTime(), ServiceFlags::NETWORK);
            
            Add(peerAddr, NetService(), 0);
        }
        
        LOG_INFO(util::LogCategory::NET) << "Resolved " << addresses.size() 
                                         << " addresses from " << seed.host;
    }
    
    return result;
}

std::vector<NetService> AddressManager::ResolveHost(const std::string& host) const {
    std::vector<NetService> result;
    
    struct addrinfo hints{};
    struct addrinfo* res = nullptr;
    
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_ADDRCONFIG;   // Only return addresses we can use
    
    std::string portStr = std::to_string(defaultPort_);
    
    int status = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (status != 0) {
        LOG_DEBUG(util::LogCategory::NET) << "DNS resolution failed for " << host
                                          << ": " << gai_strerror(status);
        return result;
    }
    
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        uint16_t port = defaultPort_;
        
        if (p->ai_family == AF_INET) {
            // IPv4
            struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
            const uint8_t* ipBytes = reinterpret_cast<const uint8_t*>(&ipv4->sin_addr);
            std::array<uint8_t, 4> ipv4Arr;
            std::memcpy(ipv4Arr.data(), ipBytes, 4);
            NetAddress addr(ipv4Arr);
            port = ntohs(ipv4->sin_port);
            if (port == 0) port = defaultPort_;
            result.emplace_back(addr, port);
        } else if (p->ai_family == AF_INET6) {
            // IPv6
            struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(p->ai_addr);
            const uint8_t* ipBytes = reinterpret_cast<const uint8_t*>(&ipv6->sin6_addr);
            std::array<uint8_t, 16> ipv6Arr;
            std::memcpy(ipv6Arr.data(), ipBytes, 16);
            NetAddress addr(ipv6Arr);
            port = ntohs(ipv6->sin6_port);
            if (port == 0) port = defaultPort_;
            result.emplace_back(addr, port);
        }
    }
    
    freeaddrinfo(res);
    return result;
}

// ============================================================================
// Statistics
// ============================================================================

size_t AddressManager::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mapInfo_.size();
}

size_t AddressManager::NumTried() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return vTried_.size();
}

size_t AddressManager::NumNew() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return vNew_.size();
}

void AddressManager::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    mapInfo_.clear();
    vNew_.clear();
    vTried_.clear();
}

// ============================================================================
// Private Helpers
// ============================================================================

AddressInfo AddressManager::MakeInfo(const PeerAddress& addr, const NetService& source, 
                                     int64_t penalty) {
    AddressInfo info;
    info.addr = addr;
    info.source = source;
    info.nTime = addr.GetTime() > 0 ? addr.GetTime() - penalty : GetAdjustedTime() - penalty;
    info.nLastSuccess = 0;
    info.nLastTry = 0;
    info.nAttempts = 0;
    info.nRefCount = 1;
    info.fInTried = false;
    return info;
}

AddressInfo* AddressManager::Find(const std::string& key) {
    auto it = mapInfo_.find(key);
    return it != mapInfo_.end() ? &it->second : nullptr;
}

const AddressInfo* AddressManager::Find(const std::string& key) const {
    auto it = mapInfo_.find(key);
    return it != mapInfo_.end() ? &it->second : nullptr;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool IsRoutable(const NetAddress& addr) {
    // Check for local/private addresses
    auto bytes = addr.GetBytes();
    
    // Check IPv4 (bytes are stored directly, not as IPv4-mapped IPv6)
    if (addr.IsIPv4() && bytes.size() >= 4) {
        // 10.x.x.x
        if (bytes[0] == 10) return false;
        // 172.16-31.x.x
        if (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) return false;
        // 192.168.x.x
        if (bytes[0] == 192 && bytes[1] == 168) return false;
        // 127.x.x.x (loopback)
        if (bytes[0] == 127) return false;
        // 0.x.x.x
        if (bytes[0] == 0) return false;
        // 169.254.x.x (link-local)
        if (bytes[0] == 169 && bytes[1] == 254) return false;
    }
    
    // Check for all zeros
    bool allZeros = true;
    for (auto b : bytes) {
        if (b != 0) {
            allZeros = false;
            break;
        }
    }
    if (allZeros) return false;
    
    return true;
}

bool IsValidForStorage(const PeerAddress& addr) {
    // Port must be non-zero (PeerAddress inherits GetPort() from NetService)
    if (addr.GetPort() == 0) return false;
    
    // Address must be routable (PeerAddress IS-A NetAddress through inheritance)
    if (!IsRoutable(static_cast<const NetAddress&>(addr))) return false;
    
    return true;
}

} // namespace shurium
