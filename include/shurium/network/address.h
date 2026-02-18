// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#pragma once

#include <shurium/core/serialize.h>
#include <shurium/core/types.h>

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace shurium {

/// Network types
enum class Network : uint8_t {
    UNROUTABLE = 0,  ///< Unroutable addresses
    IPV4 = 1,        ///< IPv4
    IPV6 = 2,        ///< IPv6
    ONION = 3,       ///< Tor (v3)
    I2P = 4,         ///< I2P
    INTERNAL = 5,    ///< Internal (for tracking only)
    MAX = 6          ///< Sentinel
};

/// Address byte sizes by network type
constexpr size_t ADDR_IPV4_SIZE = 4;
constexpr size_t ADDR_IPV6_SIZE = 16;
constexpr size_t ADDR_TORV3_SIZE = 32;
constexpr size_t ADDR_I2P_SIZE = 32;
constexpr size_t ADDR_INTERNAL_SIZE = 10;

/// BIP155 network IDs for address serialization
namespace BIP155 {
    constexpr uint8_t IPV4 = 1;
    constexpr uint8_t IPV6 = 2;
    constexpr uint8_t TORV3 = 4;
    constexpr uint8_t I2P = 5;
}

/**
 * A network address (IP address without port).
 * 
 * Supports IPv4, IPv6, Tor v3, and I2P addresses.
 */
class NetAddress {
public:
    /// Default constructor creates an unroutable address
    NetAddress();
    
    /// Construct from IPv4 bytes (4 bytes)
    explicit NetAddress(const std::array<uint8_t, 4>& ipv4);
    
    /// Construct from IPv6 bytes (16 bytes)
    explicit NetAddress(const std::array<uint8_t, 16>& ipv6);
    
    /// Construct from raw bytes with network type
    NetAddress(const std::vector<uint8_t>& addr, Network net);
    
    /// Parse from string (IP address or hostname)
    static std::optional<NetAddress> FromString(const std::string& str);
    
    /// Get the network type
    Network GetNetwork() const { return network_; }
    
    /// Get raw address bytes
    const std::vector<uint8_t>& GetBytes() const { return addr_; }
    
    /// Check if this is a valid, routable address
    bool IsRoutable() const;
    
    /// Check if this is a valid address
    bool IsValid() const;
    
    /// Check if this is the any address (0.0.0.0 or ::)
    bool IsBindAny() const;
    
    /// Check network type helpers
    bool IsIPv4() const { return network_ == Network::IPV4; }
    bool IsIPv6() const { return network_ == Network::IPV6; }
    bool IsTor() const { return network_ == Network::ONION; }
    bool IsI2P() const { return network_ == Network::I2P; }
    bool IsInternal() const { return network_ == Network::INTERNAL; }
    
    /// Check if this is a local address (127.0.0.0/8, ::1)
    bool IsLocal() const;
    
    /// Check if this is a private/RFC1918 address
    bool IsRFC1918() const;
    
    /// Check if this is a link-local address
    bool IsRFC3927() const;
    
    /// Check if this is IPv4-mapped IPv6 (::ffff:0:0/96)
    bool IsIPv4Mapped() const;
    
    /// Get IPv4 address from IPv4-mapped IPv6
    std::optional<std::array<uint8_t, 4>> GetMappedIPv4() const;
    
    /// Convert to string
    std::string ToString() const;
    
    /// Get reachability score (higher = more reachable)
    int GetReachability() const;
    
    /// Comparison operators
    bool operator==(const NetAddress& other) const;
    bool operator!=(const NetAddress& other) const { return !(*this == other); }
    bool operator<(const NetAddress& other) const;
    
    /// Serialization (BIP155 format)
    template<typename Stream>
    void Serialize(Stream& s) const {
        // BIP155: network_id, address_length, address
        uint8_t netId = GetBIP155NetId();
        ::shurium::Serialize(s, netId);
        ::shurium::WriteCompactSize(s, static_cast<uint64_t>(addr_.size()));
        s.Write(reinterpret_cast<const char*>(addr_.data()), addr_.size());
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        uint8_t netId;
        ::shurium::Unserialize(s, netId);
        uint64_t len = ::shurium::ReadCompactSize(s);
        if (len > 512) {
            throw std::runtime_error("Address too long");
        }
        addr_.resize(len);
        s.Read(reinterpret_cast<char*>(addr_.data()), len);
        SetFromBIP155(netId, addr_);
    }
    
private:
    std::vector<uint8_t> addr_;  ///< Raw address bytes
    Network network_{Network::UNROUTABLE};
    
    /// Get BIP155 network ID
    uint8_t GetBIP155NetId() const;
    
    /// Set from BIP155 format
    void SetFromBIP155(uint8_t netId, const std::vector<uint8_t>& data);
};

/**
 * A network service (IP address + port).
 * 
 * Extends NetAddress with a port number for full endpoint specification.
 */
class NetService : public NetAddress {
public:
    /// Default constructor
    NetService() : NetAddress(), port_(0) {}
    
    /// Construct from address and port
    NetService(const NetAddress& addr, uint16_t port) 
        : NetAddress(addr), port_(port) {}
    
    /// Construct from IPv4 and port
    NetService(const std::array<uint8_t, 4>& ipv4, uint16_t port)
        : NetAddress(ipv4), port_(port) {}
    
    /// Construct from IPv6 and port
    NetService(const std::array<uint8_t, 16>& ipv6, uint16_t port)
        : NetAddress(ipv6), port_(port) {}
    
    /// Parse from string "ip:port" or "[ipv6]:port"
    static std::optional<NetService> FromString(const std::string& str);
    
    /// Get the port
    uint16_t GetPort() const { return port_; }
    
    /// Set the port
    void SetPort(uint16_t port) { port_ = port; }
    
    /// Check if this is a valid service (valid address + non-zero port)
    bool IsValid() const { return NetAddress::IsValid() && port_ != 0; }
    
    /// Convert to string
    std::string ToString() const;
    
    /// Comparison operators
    bool operator==(const NetService& other) const;
    bool operator!=(const NetService& other) const { return !(*this == other); }
    bool operator<(const NetService& other) const;
    
    /// Serialization
    template<typename Stream>
    void Serialize(Stream& s) const {
        NetAddress::Serialize(s);
        // Port in big-endian (network byte order)
        uint8_t portBytes[2];
        portBytes[0] = static_cast<uint8_t>(port_ >> 8);
        portBytes[1] = static_cast<uint8_t>(port_);
        s.Write(reinterpret_cast<const char*>(portBytes), 2);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        NetAddress::Unserialize(s);
        uint8_t portBytes[2];
        s.Read(reinterpret_cast<char*>(portBytes), 2);
        port_ = (static_cast<uint16_t>(portBytes[0]) << 8) | portBytes[1];
    }
    
private:
    uint16_t port_;
};

/// Service flags indicating node capabilities (as in Bitcoin)
enum class ServiceFlags : uint64_t {
    NONE = 0,
    
    /// NODE_NETWORK - can serve full blockchain
    NETWORK = (1 << 0),
    
    /// NODE_BLOOM - supports bloom filters (BIP37)
    BLOOM = (1 << 2),
    
    /// NODE_WITNESS - supports SegWit
    WITNESS = (1 << 3),
    
    /// NODE_COMPACT_FILTERS - supports BIP157/158 compact filters
    COMPACT_FILTERS = (1 << 6),
    
    /// NODE_NETWORK_LIMITED - only serves last 288 blocks
    NETWORK_LIMITED = (1 << 10),
    
    /// SHURIUM-specific: Can verify PoUW solutions
    POUW_VERIFY = (1 << 16),
    
    /// SHURIUM-specific: Has identity verification capability
    IDENTITY = (1 << 17),
    
    /// SHURIUM-specific: Can process UBI claims
    UBI = (1 << 18),
};

/// Combine service flags
inline ServiceFlags operator|(ServiceFlags a, ServiceFlags b) {
    return static_cast<ServiceFlags>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

inline ServiceFlags operator&(ServiceFlags a, ServiceFlags b) {
    return static_cast<ServiceFlags>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}

inline ServiceFlags& operator|=(ServiceFlags& a, ServiceFlags b) {
    return a = a | b;
}

inline bool HasFlag(ServiceFlags flags, ServiceFlags flag) {
    return (static_cast<uint64_t>(flags) & static_cast<uint64_t>(flag)) != 0;
}

/**
 * A peer address with timestamp and services.
 * 
 * Stored in the address database for peer discovery.
 */
class PeerAddress : public NetService {
public:
    /// Default constructor
    PeerAddress() : NetService(), time_(0), services_(ServiceFlags::NONE) {}
    
    /// Construct with all fields
    PeerAddress(const NetService& service, int64_t time, ServiceFlags services)
        : NetService(service), time_(time), services_(services) {}
    
    /// Get last seen time (unix timestamp)
    int64_t GetTime() const { return time_; }
    
    /// Set last seen time
    void SetTime(int64_t time) { time_ = time; }
    
    /// Get service flags
    ServiceFlags GetServices() const { return services_; }
    
    /// Set service flags
    void SetServices(ServiceFlags services) { services_ = services; }
    
    /// Check if peer has a specific service
    bool HasService(ServiceFlags service) const {
        return HasFlag(services_, service);
    }
    
    /// Serialization
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, static_cast<uint32_t>(time_));
        ::shurium::Serialize(s, static_cast<uint64_t>(services_));
        NetService::Serialize(s);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        uint32_t time32;
        ::shurium::Unserialize(s, time32);
        time_ = time32;
        uint64_t services64;
        ::shurium::Unserialize(s, services64);
        services_ = static_cast<ServiceFlags>(services64);
        NetService::Unserialize(s);
    }
    
private:
    int64_t time_;
    ServiceFlags services_;
};

/// Hash function for NetAddress
struct NetAddressHasher {
    size_t operator()(const NetAddress& addr) const {
        const auto& bytes = addr.GetBytes();
        size_t seed = static_cast<size_t>(addr.GetNetwork());
        for (size_t i = 0; i < bytes.size() && i < 8; ++i) {
            seed ^= static_cast<size_t>(bytes[i]) << (i * 8 % 64);
        }
        return seed;
    }
};

/// Hash function for NetService
struct NetServiceHasher {
    size_t operator()(const NetService& service) const {
        NetAddressHasher addrHasher;
        return addrHasher(service) ^ (static_cast<size_t>(service.GetPort()) << 48);
    }
};

} // namespace shurium
