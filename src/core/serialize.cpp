// SHURIUM - Serialization Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/core/serialize.h"
#include "shurium/core/hex.h"

namespace shurium {

// ============================================================================
// DataStream Implementation
// ============================================================================

std::string DataStream::ToHex() const {
    return BytesToHex(data_.data() + read_pos_, data_.size() - read_pos_);
}

void DataStream::FromHex(const std::string& hex) {
    clear();
    std::vector<uint8_t> bytes = HexToBytes(hex);
    data_ = std::move(bytes);
}

} // namespace shurium
