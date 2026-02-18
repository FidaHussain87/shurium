// SHURIUM - Hex Encoding/Decoding Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/core/hex.h"

namespace shurium {

namespace {
    constexpr char HEX_CHARS[] = "0123456789abcdef";
    
    inline int HexCharToNibble(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
}

std::string BytesToHex(const HexByte* data, size_t len) {
    std::string result;
    result.reserve(len * 2);
    
    for (size_t i = 0; i < len; ++i) {
        result.push_back(HEX_CHARS[data[i] >> 4]);
        result.push_back(HEX_CHARS[data[i] & 0x0F]);
    }
    
    return result;
}

std::string BytesToHex(const std::vector<HexByte>& data) {
    return BytesToHex(data.data(), data.size());
}

std::vector<HexByte> HexToBytes(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have even length");
    }
    
    std::vector<HexByte> result;
    result.reserve(hex.length() / 2);
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        int high = HexCharToNibble(hex[i]);
        int low = HexCharToNibble(hex[i + 1]);
        
        if (high < 0 || low < 0) {
            throw std::invalid_argument("Invalid hex character");
        }
        
        result.push_back(static_cast<HexByte>((high << 4) | low));
    }
    
    return result;
}

bool IsValidHex(const std::string& str) {
    if (str.empty() || str.length() % 2 != 0) {
        return false;
    }
    
    for (char c : str) {
        if (HexCharToNibble(c) < 0) {
            return false;
        }
    }
    
    return true;
}

std::vector<HexByte> ReverseBytes(const HexByte* data, size_t len) {
    std::vector<HexByte> result(len);
    for (size_t i = 0; i < len; ++i) {
        result[i] = data[len - 1 - i];
    }
    return result;
}

} // namespace shurium
