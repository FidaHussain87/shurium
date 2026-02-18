// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#include <shurium/network/address.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace shurium {

// ============================================================================
// NetAddress Implementation
// ============================================================================

NetAddress::NetAddress() : network_(Network::UNROUTABLE) {
    addr_.clear();
}

NetAddress::NetAddress(const std::array<uint8_t, 4>& ipv4)
    : addr_(ipv4.begin(), ipv4.end()), network_(Network::IPV4) {
}

NetAddress::NetAddress(const std::array<uint8_t, 16>& ipv6)
    : addr_(ipv6.begin(), ipv6.end()), network_(Network::IPV6) {
}

NetAddress::NetAddress(const std::vector<uint8_t>& addr, Network net)
    : addr_(addr), network_(net) {
}

std::optional<NetAddress> NetAddress::FromString(const std::string& str) {
    // Try IPv4 first: a.b.c.d
    std::array<uint8_t, 4> ipv4;
    unsigned int a, b, c, d;
    if (sscanf(str.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        if (a <= 255 && b <= 255 && c <= 255 && d <= 255) {
            ipv4[0] = static_cast<uint8_t>(a);
            ipv4[1] = static_cast<uint8_t>(b);
            ipv4[2] = static_cast<uint8_t>(c);
            ipv4[3] = static_cast<uint8_t>(d);
            return NetAddress(ipv4);
        }
    }
    
    // Try IPv6: various formats
    // Simplified: handle common formats like ::1, fe80::1, etc.
    if (str.find(':') != std::string::npos) {
        std::array<uint8_t, 16> ipv6{};
        
        // Handle ::1 (localhost)
        if (str == "::1") {
            ipv6[15] = 1;
            return NetAddress(ipv6);
        }
        
        // Handle :: (any address)
        if (str == "::") {
            return NetAddress(ipv6);
        }
        
        // Handle full IPv6 format: xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx
        std::vector<uint16_t> groups;
        std::string remaining = str;
        size_t doubleColonPos = remaining.find("::");
        
        if (doubleColonPos == std::string::npos) {
            // No ::, must be 8 groups
            std::stringstream ss(remaining);
            std::string group;
            while (std::getline(ss, group, ':')) {
                unsigned int val;
                if (sscanf(group.c_str(), "%x", &val) == 1 && val <= 0xFFFF) {
                    groups.push_back(static_cast<uint16_t>(val));
                } else {
                    return std::nullopt;
                }
            }
            if (groups.size() != 8) {
                return std::nullopt;
            }
        } else {
            // Has ::, split into before and after parts
            std::string before = remaining.substr(0, doubleColonPos);
            std::string after = doubleColonPos + 2 < remaining.size() 
                ? remaining.substr(doubleColonPos + 2) 
                : "";
            
            std::vector<uint16_t> beforeGroups, afterGroups;
            
            if (!before.empty()) {
                std::stringstream ss(before);
                std::string group;
                while (std::getline(ss, group, ':')) {
                    unsigned int val;
                    if (sscanf(group.c_str(), "%x", &val) == 1 && val <= 0xFFFF) {
                        beforeGroups.push_back(static_cast<uint16_t>(val));
                    } else {
                        return std::nullopt;
                    }
                }
            }
            
            if (!after.empty()) {
                std::stringstream ss(after);
                std::string group;
                while (std::getline(ss, group, ':')) {
                    unsigned int val;
                    if (sscanf(group.c_str(), "%x", &val) == 1 && val <= 0xFFFF) {
                        afterGroups.push_back(static_cast<uint16_t>(val));
                    } else {
                        return std::nullopt;
                    }
                }
            }
            
            size_t totalGroups = beforeGroups.size() + afterGroups.size();
            if (totalGroups > 8) {
                return std::nullopt;
            }
            
            // Build groups with zeros in between
            groups = beforeGroups;
            groups.resize(8 - afterGroups.size(), 0);
            groups.insert(groups.end(), afterGroups.begin(), afterGroups.end());
        }
        
        // Convert groups to bytes (big-endian)
        for (size_t i = 0; i < 8; ++i) {
            ipv6[i * 2] = static_cast<uint8_t>(groups[i] >> 8);
            ipv6[i * 2 + 1] = static_cast<uint8_t>(groups[i]);
        }
        
        return NetAddress(ipv6);
    }
    
    return std::nullopt;
}

bool NetAddress::IsRoutable() const {
    return IsValid() && !IsRFC1918() && !IsRFC3927() && !IsLocal() && 
           network_ != Network::UNROUTABLE && network_ != Network::INTERNAL;
}

bool NetAddress::IsValid() const {
    switch (network_) {
        case Network::IPV4:
            return addr_.size() == ADDR_IPV4_SIZE;
        case Network::IPV6:
            return addr_.size() == ADDR_IPV6_SIZE;
        case Network::ONION:
            return addr_.size() == ADDR_TORV3_SIZE;
        case Network::I2P:
            return addr_.size() == ADDR_I2P_SIZE;
        case Network::INTERNAL:
            return addr_.size() == ADDR_INTERNAL_SIZE;
        default:
            return false;
    }
}

bool NetAddress::IsBindAny() const {
    if (network_ == Network::IPV4 && addr_.size() == 4) {
        return addr_[0] == 0 && addr_[1] == 0 && addr_[2] == 0 && addr_[3] == 0;
    }
    if (network_ == Network::IPV6 && addr_.size() == 16) {
        for (const auto& byte : addr_) {
            if (byte != 0) return false;
        }
        return true;
    }
    return false;
}

bool NetAddress::IsLocal() const {
    // IPv4 127.0.0.0/8
    if (network_ == Network::IPV4 && addr_.size() >= 1) {
        return addr_[0] == 127;
    }
    // IPv6 ::1
    if (network_ == Network::IPV6 && addr_.size() == 16) {
        for (size_t i = 0; i < 15; ++i) {
            if (addr_[i] != 0) return false;
        }
        return addr_[15] == 1;
    }
    return false;
}

bool NetAddress::IsRFC1918() const {
    // Private IPv4 ranges:
    // 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16
    if (network_ != Network::IPV4 || addr_.size() != 4) {
        return false;
    }
    return addr_[0] == 10 ||
           (addr_[0] == 172 && (addr_[1] >= 16 && addr_[1] <= 31)) ||
           (addr_[0] == 192 && addr_[1] == 168);
}

bool NetAddress::IsRFC3927() const {
    // Link-local: 169.254.0.0/16
    if (network_ != Network::IPV4 || addr_.size() != 4) {
        return false;
    }
    return addr_[0] == 169 && addr_[1] == 254;
}

bool NetAddress::IsIPv4Mapped() const {
    // ::ffff:0:0/96
    if (network_ != Network::IPV6 || addr_.size() != 16) {
        return false;
    }
    // Check for ::ffff: prefix
    for (size_t i = 0; i < 10; ++i) {
        if (addr_[i] != 0) return false;
    }
    return addr_[10] == 0xff && addr_[11] == 0xff;
}

std::optional<std::array<uint8_t, 4>> NetAddress::GetMappedIPv4() const {
    if (!IsIPv4Mapped()) {
        return std::nullopt;
    }
    std::array<uint8_t, 4> ipv4;
    std::copy(addr_.begin() + 12, addr_.end(), ipv4.begin());
    return ipv4;
}

std::string NetAddress::ToString() const {
    if (network_ == Network::IPV4 && addr_.size() == 4) {
        std::ostringstream ss;
        ss << static_cast<int>(addr_[0]) << '.'
           << static_cast<int>(addr_[1]) << '.'
           << static_cast<int>(addr_[2]) << '.'
           << static_cast<int>(addr_[3]);
        return ss.str();
    }
    
    if (network_ == Network::IPV6 && addr_.size() == 16) {
        // Find longest run of zeros for :: compression
        std::vector<uint16_t> groups(8);
        for (size_t i = 0; i < 8; ++i) {
            groups[i] = (static_cast<uint16_t>(addr_[i * 2]) << 8) | addr_[i * 2 + 1];
        }
        
        // Find best position for ::
        int bestStart = -1, bestLen = 0;
        int curStart = -1, curLen = 0;
        for (int i = 0; i < 8; ++i) {
            if (groups[i] == 0) {
                if (curStart == -1) curStart = i;
                curLen++;
            } else {
                if (curLen > bestLen && curLen > 1) {
                    bestStart = curStart;
                    bestLen = curLen;
                }
                curStart = -1;
                curLen = 0;
            }
        }
        if (curLen > bestLen && curLen > 1) {
            bestStart = curStart;
            bestLen = curLen;
        }
        
        std::ostringstream ss;
        ss << std::hex;
        bool inCompression = false;
        for (int i = 0; i < 8; ++i) {
            if (bestStart != -1 && i >= bestStart && i < bestStart + bestLen) {
                if (!inCompression) {
                    ss << "::";
                    inCompression = true;
                }
            } else {
                if (i > 0 && !inCompression) {
                    ss << ':';
                }
                inCompression = false;
                ss << groups[i];
            }
        }
        return ss.str();
    }
    
    if (network_ == Network::ONION) {
        return "[tor]";  // Simplified
    }
    
    if (network_ == Network::I2P) {
        return "[i2p]";  // Simplified
    }
    
    return "[invalid]";
}

int NetAddress::GetReachability() const {
    if (!IsRoutable()) return 0;
    
    switch (network_) {
        case Network::IPV4:
            return 4;
        case Network::IPV6:
            return 6;
        case Network::ONION:
            return 3;
        case Network::I2P:
            return 2;
        default:
            return 0;
    }
}

bool NetAddress::operator==(const NetAddress& other) const {
    return network_ == other.network_ && addr_ == other.addr_;
}

bool NetAddress::operator<(const NetAddress& other) const {
    if (network_ != other.network_) {
        return network_ < other.network_;
    }
    return addr_ < other.addr_;
}

uint8_t NetAddress::GetBIP155NetId() const {
    switch (network_) {
        case Network::IPV4: return BIP155::IPV4;
        case Network::IPV6: return BIP155::IPV6;
        case Network::ONION: return BIP155::TORV3;
        case Network::I2P: return BIP155::I2P;
        default: return 0;
    }
}

void NetAddress::SetFromBIP155(uint8_t netId, const std::vector<uint8_t>& data) {
    addr_ = data;
    switch (netId) {
        case BIP155::IPV4:
            network_ = Network::IPV4;
            break;
        case BIP155::IPV6:
            network_ = Network::IPV6;
            break;
        case BIP155::TORV3:
            network_ = Network::ONION;
            break;
        case BIP155::I2P:
            network_ = Network::I2P;
            break;
        default:
            network_ = Network::UNROUTABLE;
            break;
    }
}

// ============================================================================
// NetService Implementation
// ============================================================================

std::optional<NetService> NetService::FromString(const std::string& str) {
    std::string addrStr;
    uint16_t port = 0;
    
    // Check for [ipv6]:port format
    if (str.front() == '[') {
        size_t endBracket = str.find(']');
        if (endBracket == std::string::npos) {
            return std::nullopt;
        }
        addrStr = str.substr(1, endBracket - 1);
        
        if (endBracket + 1 < str.size() && str[endBracket + 1] == ':') {
            try {
                port = static_cast<uint16_t>(std::stoul(str.substr(endBracket + 2)));
            } catch (...) {
                return std::nullopt;
            }
        }
    } else {
        // IPv4:port or hostname:port
        size_t lastColon = str.rfind(':');
        if (lastColon == std::string::npos) {
            addrStr = str;
        } else {
            addrStr = str.substr(0, lastColon);
            try {
                port = static_cast<uint16_t>(std::stoul(str.substr(lastColon + 1)));
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    
    auto addr = NetAddress::FromString(addrStr);
    if (!addr) {
        return std::nullopt;
    }
    
    return NetService(*addr, port);
}

std::string NetService::ToString() const {
    std::string addrStr = NetAddress::ToString();
    if (IsIPv6()) {
        return "[" + addrStr + "]:" + std::to_string(port_);
    }
    return addrStr + ":" + std::to_string(port_);
}

bool NetService::operator==(const NetService& other) const {
    return NetAddress::operator==(other) && port_ == other.port_;
}

bool NetService::operator<(const NetService& other) const {
    if (static_cast<const NetAddress&>(*this) != static_cast<const NetAddress&>(other)) {
        return NetAddress::operator<(other);
    }
    return port_ < other.port_;
}

} // namespace shurium
