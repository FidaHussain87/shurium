// SHURIUM - Serialization Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines serialization primitives for SHURIUM.
// Based on Bitcoin's serialization with simplifications for clarity.

#ifndef SHURIUM_CORE_SERIALIZE_H
#define SHURIUM_CORE_SERIALIZE_H

#include "shurium/core/types.h"
#include "shurium/core/hex.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <string>
#include <array>
#include <stdexcept>
#include <ios>
#include <type_traits>
#include <limits>

namespace shurium {

// ============================================================================
// Constants
// ============================================================================

/// Maximum size for serialized objects to prevent memory exhaustion
static constexpr uint64_t MAX_SIZE = 0x02000000;  // 32 MB

/// Maximum vector allocation size
static constexpr unsigned int MAX_VECTOR_ALLOCATE = 5000000;

// ============================================================================
// Endianness Helpers (Always Little-Endian for serialization)
// ============================================================================

namespace detail {

inline uint16_t htole16(uint16_t host) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap16(host);
#else
    return host;
#endif
}

inline uint32_t htole32(uint32_t host) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(host);
#else
    return host;
#endif
}

inline uint64_t htole64(uint64_t host) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap64(host);
#else
    return host;
#endif
}

inline uint16_t le16toh(uint16_t little) { return htole16(little); }
inline uint32_t le32toh(uint32_t little) { return htole32(little); }
inline uint64_t le64toh(uint64_t little) { return htole64(little); }

} // namespace detail

// ============================================================================
// DataStream - In-memory byte buffer for serialization
// ============================================================================

class DataStream {
public:
    using value_type = uint8_t;
    using size_type = std::size_t;
    using iterator = std::vector<uint8_t>::iterator;
    using const_iterator = std::vector<uint8_t>::const_iterator;

private:
    std::vector<uint8_t> data_;
    size_type read_pos_ = 0;

public:
    // ========================================================================
    // Constructors
    // ========================================================================
    
    DataStream() = default;
    
    explicit DataStream(const std::vector<uint8_t>& data) : data_(data), read_pos_(0) {}
    
    explicit DataStream(std::vector<uint8_t>&& data) : data_(std::move(data)), read_pos_(0) {}
    
    DataStream(const uint8_t* data, size_type len) : data_(data, data + len), read_pos_(0) {}
    
    // ========================================================================
    // Size and Capacity
    // ========================================================================
    
    /// Returns unread bytes remaining
    size_type size() const noexcept { return data_.size() - read_pos_; }
    
    /// Total buffer size (including read bytes)
    size_type TotalSize() const noexcept { return data_.size(); }
    
    bool empty() const noexcept { return size() == 0; }
    
    void reserve(size_type n) { data_.reserve(n + read_pos_); }
    
    void clear() {
        data_.clear();
        read_pos_ = 0;
    }
    
    // ========================================================================
    // Data Access
    // ========================================================================
    
    /// Get pointer to unread data
    const uint8_t* data() const noexcept { return data_.data() + read_pos_; }
    uint8_t* data() noexcept { return data_.data() + read_pos_; }
    
    /// Get reference to unread data
    const std::vector<uint8_t>& Data() const noexcept { return data_; }
    
    const_iterator begin() const noexcept { return data_.begin() + read_pos_; }
    const_iterator end() const noexcept { return data_.end(); }
    iterator begin() noexcept { return data_.begin() + read_pos_; }
    iterator end() noexcept { return data_.end(); }
    
    // ========================================================================
    // Write Operations
    // ========================================================================
    
    void Write(const uint8_t* src, size_type len) {
        data_.insert(data_.end(), src, src + len);
    }
    
    void Write(const char* src, size_type len) {
        Write(reinterpret_cast<const uint8_t*>(src), len);
    }
    
    // ========================================================================
    // Read Operations
    // ========================================================================
    
    void Read(uint8_t* dst, size_type len) {
        if (len > size()) {
            throw std::ios_base::failure("DataStream::Read(): end of data");
        }
        std::memcpy(dst, data_.data() + read_pos_, len);
        read_pos_ += len;
        
        // Clear buffer if fully read
        if (read_pos_ == data_.size()) {
            data_.clear();
            read_pos_ = 0;
        }
    }
    
    void Read(char* dst, size_type len) {
        Read(reinterpret_cast<uint8_t*>(dst), len);
    }
    
    /// Skip n bytes
    void Ignore(size_type n) {
        if (n > size()) {
            throw std::ios_base::failure("DataStream::Ignore(): end of data");
        }
        read_pos_ += n;
        
        if (read_pos_ == data_.size()) {
            data_.clear();
            read_pos_ = 0;
        }
    }
    
    /// Rewind read position to beginning
    void Rewind() {
        read_pos_ = 0;
    }
    
    /// Rewind by n bytes
    bool Rewind(size_type n) {
        if (n > read_pos_) return false;
        read_pos_ -= n;
        return true;
    }
    
    // ========================================================================
    // Hex Conversion
    // ========================================================================
    
    std::string ToHex() const;
    void FromHex(const std::string& hex);
    
    // ========================================================================
    // Stream Operators (defined later with Serialize/Unserialize)
    // ========================================================================
    
    template<typename T>
    DataStream& operator<<(const T& obj);
    
    template<typename T>
    DataStream& operator>>(T& obj);
};

// ============================================================================
// Low-Level Serialization Functions
// ============================================================================

// Write integers to stream (little-endian)
template<typename Stream>
inline void ser_writedata8(Stream& s, uint8_t obj) {
    s.Write(&obj, 1);
}

template<typename Stream>
inline void ser_writedata16(Stream& s, uint16_t obj) {
    obj = detail::htole16(obj);
    s.Write(reinterpret_cast<const uint8_t*>(&obj), 2);
}

template<typename Stream>
inline void ser_writedata32(Stream& s, uint32_t obj) {
    obj = detail::htole32(obj);
    s.Write(reinterpret_cast<const uint8_t*>(&obj), 4);
}

template<typename Stream>
inline void ser_writedata64(Stream& s, uint64_t obj) {
    obj = detail::htole64(obj);
    s.Write(reinterpret_cast<const uint8_t*>(&obj), 8);
}

// Read integers from stream (little-endian)
template<typename Stream>
inline uint8_t ser_readdata8(Stream& s) {
    uint8_t obj;
    s.Read(&obj, 1);
    return obj;
}

template<typename Stream>
inline uint16_t ser_readdata16(Stream& s) {
    uint16_t obj;
    s.Read(reinterpret_cast<uint8_t*>(&obj), 2);
    return detail::le16toh(obj);
}

template<typename Stream>
inline uint32_t ser_readdata32(Stream& s) {
    uint32_t obj;
    s.Read(reinterpret_cast<uint8_t*>(&obj), 4);
    return detail::le32toh(obj);
}

template<typename Stream>
inline uint64_t ser_readdata64(Stream& s) {
    uint64_t obj;
    s.Read(reinterpret_cast<uint8_t*>(&obj), 8);
    return detail::le64toh(obj);
}

// ============================================================================
// CompactSize Encoding
// ============================================================================
// Compact Size format:
//   size <  253        -- 1 byte
//   size <= 0xFFFF     -- 3 bytes (0xFD + 2 bytes little-endian)
//   size <= 0xFFFFFFFF -- 5 bytes (0xFE + 4 bytes little-endian)
//   size >  0xFFFFFFFF -- 9 bytes (0xFF + 8 bytes little-endian)

template<typename Stream>
void WriteCompactSize(Stream& s, uint64_t size) {
    if (size < 253) {
        ser_writedata8(s, static_cast<uint8_t>(size));
    } else if (size <= 0xFFFF) {
        ser_writedata8(s, 0xFD);
        ser_writedata16(s, static_cast<uint16_t>(size));
    } else if (size <= 0xFFFFFFFF) {
        ser_writedata8(s, 0xFE);
        ser_writedata32(s, static_cast<uint32_t>(size));
    } else {
        ser_writedata8(s, 0xFF);
        ser_writedata64(s, size);
    }
}

template<typename Stream>
uint64_t ReadCompactSize(Stream& s, bool range_check = true) {
    uint8_t marker = ser_readdata8(s);
    uint64_t size = 0;
    
    if (marker < 253) {
        size = marker;
    } else if (marker == 253) {
        size = ser_readdata16(s);
        if (size < 253) {
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
        }
    } else if (marker == 254) {
        size = ser_readdata32(s);
        if (size < 0x10000) {
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
        }
    } else {
        size = ser_readdata64(s);
        if (size < 0x100000000ULL) {
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
        }
    }
    
    if (range_check && size > MAX_SIZE) {
        throw std::ios_base::failure("ReadCompactSize(): size too large");
    }
    
    return size;
}

// ============================================================================
// Serialize/Unserialize for Basic Types
// ============================================================================

// 8-bit integers
template<typename Stream>
inline void Serialize(Stream& s, uint8_t a) { ser_writedata8(s, a); }

template<typename Stream>
inline void Serialize(Stream& s, int8_t a) { ser_writedata8(s, static_cast<uint8_t>(a)); }

template<typename Stream>
inline void Unserialize(Stream& s, uint8_t& a) { a = ser_readdata8(s); }

template<typename Stream>
inline void Unserialize(Stream& s, int8_t& a) { a = static_cast<int8_t>(ser_readdata8(s)); }

// 16-bit integers
template<typename Stream>
inline void Serialize(Stream& s, uint16_t a) { ser_writedata16(s, a); }

template<typename Stream>
inline void Serialize(Stream& s, int16_t a) { ser_writedata16(s, static_cast<uint16_t>(a)); }

template<typename Stream>
inline void Unserialize(Stream& s, uint16_t& a) { a = ser_readdata16(s); }

template<typename Stream>
inline void Unserialize(Stream& s, int16_t& a) { a = static_cast<int16_t>(ser_readdata16(s)); }

// 32-bit integers
template<typename Stream>
inline void Serialize(Stream& s, uint32_t a) { ser_writedata32(s, a); }

template<typename Stream>
inline void Serialize(Stream& s, int32_t a) { ser_writedata32(s, static_cast<uint32_t>(a)); }

template<typename Stream>
inline void Unserialize(Stream& s, uint32_t& a) { a = ser_readdata32(s); }

template<typename Stream>
inline void Unserialize(Stream& s, int32_t& a) { a = static_cast<int32_t>(ser_readdata32(s)); }

// 64-bit integers
template<typename Stream>
inline void Serialize(Stream& s, uint64_t a) { ser_writedata64(s, a); }

template<typename Stream>
inline void Serialize(Stream& s, int64_t a) { ser_writedata64(s, static_cast<uint64_t>(a)); }

template<typename Stream>
inline void Unserialize(Stream& s, uint64_t& a) { a = ser_readdata64(s); }

template<typename Stream>
inline void Unserialize(Stream& s, int64_t& a) { a = static_cast<int64_t>(ser_readdata64(s)); }

// Boolean
template<typename Stream>
inline void Serialize(Stream& s, bool a) { ser_writedata8(s, a ? 1 : 0); }

template<typename Stream>
inline void Unserialize(Stream& s, bool& a) { a = (ser_readdata8(s) != 0); }

// ============================================================================
// Serialize/Unserialize for Vectors
// ============================================================================

template<typename Stream, typename T>
void Serialize(Stream& s, const std::vector<T>& v) {
    WriteCompactSize(s, v.size());
    for (const auto& item : v) {
        Serialize(s, item);
    }
}

template<typename Stream, typename T>
void Unserialize(Stream& s, std::vector<T>& v) {
    uint64_t size = ReadCompactSize(s);
    v.clear();
    v.reserve(std::min(size, static_cast<uint64_t>(MAX_VECTOR_ALLOCATE / sizeof(T))));
    for (uint64_t i = 0; i < size; ++i) {
        T item;
        Unserialize(s, item);
        v.push_back(std::move(item));
    }
}

// Specialization for uint8_t vectors (raw bytes, more efficient)
template<typename Stream>
void Serialize(Stream& s, const std::vector<uint8_t>& v) {
    WriteCompactSize(s, v.size());
    if (!v.empty()) {
        s.Write(v.data(), v.size());
    }
}

template<typename Stream>
void Unserialize(Stream& s, std::vector<uint8_t>& v) {
    uint64_t size = ReadCompactSize(s);
    v.resize(size);
    if (size > 0) {
        s.Read(v.data(), size);
    }
}

// ============================================================================
// Serialize/Unserialize for Strings
// ============================================================================

template<typename Stream>
void Serialize(Stream& s, const std::string& str) {
    WriteCompactSize(s, str.size());
    if (!str.empty()) {
        s.Write(str.data(), str.size());
    }
}

template<typename Stream>
void Unserialize(Stream& s, std::string& str) {
    uint64_t size = ReadCompactSize(s);
    str.resize(size);
    if (size > 0) {
        s.Read(&str[0], size);
    }
}

// ============================================================================
// Serialize/Unserialize for Fixed-Size Arrays
// ============================================================================

template<typename Stream, typename T, size_t N>
void Serialize(Stream& s, const std::array<T, N>& arr) {
    for (const auto& item : arr) {
        Serialize(s, item);
    }
}

template<typename Stream, typename T, size_t N>
void Unserialize(Stream& s, std::array<T, N>& arr) {
    for (auto& item : arr) {
        Unserialize(s, item);
    }
}

// Specialization for byte arrays (raw bytes)
template<typename Stream, size_t N>
void Serialize(Stream& s, const std::array<uint8_t, N>& arr) {
    s.Write(arr.data(), N);
}

template<typename Stream, size_t N>
void Unserialize(Stream& s, std::array<uint8_t, N>& arr) {
    s.Read(arr.data(), N);
}

// ============================================================================
// Serialize/Unserialize for Hash Types
// ============================================================================

template<typename Stream, size_t BITS>
void Serialize(Stream& s, const BaseHash<BITS>& hash) {
    s.Write(hash.data(), BaseHash<BITS>::SIZE);
}

template<typename Stream, size_t BITS>
void Unserialize(Stream& s, BaseHash<BITS>& hash) {
    s.Read(hash.data(), BaseHash<BITS>::SIZE);
}

// Explicit specializations for Hash256, Hash160, etc.
template<typename Stream>
void Serialize(Stream& s, const Hash256& hash) {
    s.Write(hash.data(), 32);
}

template<typename Stream>
void Unserialize(Stream& s, Hash256& hash) {
    s.Read(hash.data(), 32);
}

template<typename Stream>
void Serialize(Stream& s, const Hash160& hash) {
    s.Write(hash.data(), 20);
}

template<typename Stream>
void Unserialize(Stream& s, Hash160& hash) {
    s.Read(hash.data(), 20);
}

// ============================================================================
// GetSerializeSize - Calculate serialized size without serializing
// ============================================================================

// Size calculator stream - just counts bytes
class SizeComputer {
private:
    size_t size_ = 0;

public:
    void Write(const uint8_t*, size_t len) { size_ += len; }
    void Write(const char*, size_t len) { size_ += len; }
    size_t size() const noexcept { return size_; }
};

template<typename T>
size_t GetSerializeSize(const T& obj) {
    SizeComputer sc;
    Serialize(sc, obj);
    return sc.size();
}

// ============================================================================
// DataStream Stream Operators Implementation
// ============================================================================

template<typename T>
DataStream& DataStream::operator<<(const T& obj) {
    Serialize(*this, obj);
    return *this;
}

template<typename T>
DataStream& DataStream::operator>>(T& obj) {
    Unserialize(*this, obj);
    return *this;
}

} // namespace shurium

#endif // SHURIUM_CORE_SERIALIZE_H
