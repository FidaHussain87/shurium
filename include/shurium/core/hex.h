// SHURIUM - Hex Encoding/Decoding Utilities
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#ifndef SHURIUM_CORE_HEX_H
#define SHURIUM_CORE_HEX_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <stdexcept>

namespace shurium {

// Use uint8_t directly to avoid circular dependency with types.h
using HexByte = uint8_t;

/// Convert bytes to hex string
std::string BytesToHex(const HexByte* data, size_t len);
std::string BytesToHex(const std::vector<HexByte>& data);

template<size_t N>
std::string BytesToHex(const std::array<HexByte, N>& data) {
    return BytesToHex(data.data(), N);
}

/// Convert hex string to bytes
std::vector<HexByte> HexToBytes(const std::string& hex);

/// Check if string is valid hex
bool IsValidHex(const std::string& str);

/// Reverse bytes (for display purposes)
std::vector<HexByte> ReverseBytes(const HexByte* data, size_t len);

template<size_t N>
std::array<HexByte, N> ReverseBytes(const std::array<HexByte, N>& data) {
    std::array<HexByte, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = data[N - 1 - i];
    }
    return result;
}

} // namespace shurium

#endif // SHURIUM_CORE_HEX_H
