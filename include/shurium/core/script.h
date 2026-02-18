// SHURIUM - Script Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the SHURIUM scripting system for transaction validation.
// Based on Bitcoin's Script with simplifications for clarity.

#ifndef SHURIUM_CORE_SCRIPT_H
#define SHURIUM_CORE_SCRIPT_H

#include "shurium/core/types.h"
#include "shurium/core/serialize.h"
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

namespace shurium {

// ============================================================================
// Script Limits
// ============================================================================

/// Maximum number of bytes pushable to the stack
static constexpr unsigned int MAX_SCRIPT_ELEMENT_SIZE = 520;

/// Maximum number of non-push operations per script
static constexpr int MAX_OPS_PER_SCRIPT = 201;

/// Maximum number of public keys per multisig
static constexpr int MAX_PUBKEYS_PER_MULTISIG = 20;

/// Maximum script length in bytes
static constexpr int MAX_SCRIPT_SIZE = 10000;

/// Maximum number of values on script interpreter stack
static constexpr int MAX_STACK_SIZE = 1000;

// ============================================================================
// Opcodes
// ============================================================================

/// Script opcodes (Bitcoin-compatible subset)
enum Opcode : uint8_t {
    // Push value
    OP_0 = 0x00,
    OP_FALSE = OP_0,
    OP_PUSHDATA1 = 0x4c,
    OP_PUSHDATA2 = 0x4d,
    OP_PUSHDATA4 = 0x4e,
    OP_1NEGATE = 0x4f,
    OP_RESERVED = 0x50,
    OP_1 = 0x51,
    OP_TRUE = OP_1,
    OP_2 = 0x52,
    OP_3 = 0x53,
    OP_4 = 0x54,
    OP_5 = 0x55,
    OP_6 = 0x56,
    OP_7 = 0x57,
    OP_8 = 0x58,
    OP_9 = 0x59,
    OP_10 = 0x5a,
    OP_11 = 0x5b,
    OP_12 = 0x5c,
    OP_13 = 0x5d,
    OP_14 = 0x5e,
    OP_15 = 0x5f,
    OP_16 = 0x60,

    // Control
    OP_NOP = 0x61,
    OP_VER = 0x62,
    OP_IF = 0x63,
    OP_NOTIF = 0x64,
    OP_VERIF = 0x65,
    OP_VERNOTIF = 0x66,
    OP_ELSE = 0x67,
    OP_ENDIF = 0x68,
    OP_VERIFY = 0x69,
    OP_RETURN = 0x6a,

    // Stack ops
    OP_TOALTSTACK = 0x6b,
    OP_FROMALTSTACK = 0x6c,
    OP_2DROP = 0x6d,
    OP_2DUP = 0x6e,
    OP_3DUP = 0x6f,
    OP_2OVER = 0x70,
    OP_2ROT = 0x71,
    OP_2SWAP = 0x72,
    OP_IFDUP = 0x73,
    OP_DEPTH = 0x74,
    OP_DROP = 0x75,
    OP_DUP = 0x76,
    OP_NIP = 0x77,
    OP_OVER = 0x78,
    OP_PICK = 0x79,
    OP_ROLL = 0x7a,
    OP_ROT = 0x7b,
    OP_SWAP = 0x7c,
    OP_TUCK = 0x7d,

    // Splice ops (disabled in Bitcoin)
    OP_CAT = 0x7e,
    OP_SUBSTR = 0x7f,
    OP_LEFT = 0x80,
    OP_RIGHT = 0x81,
    OP_SIZE = 0x82,

    // Bit logic
    OP_INVERT = 0x83,
    OP_AND = 0x84,
    OP_OR = 0x85,
    OP_XOR = 0x86,
    OP_EQUAL = 0x87,
    OP_EQUALVERIFY = 0x88,
    OP_RESERVED1 = 0x89,
    OP_RESERVED2 = 0x8a,

    // Numeric
    OP_1ADD = 0x8b,
    OP_1SUB = 0x8c,
    OP_2MUL = 0x8d,
    OP_2DIV = 0x8e,
    OP_NEGATE = 0x8f,
    OP_ABS = 0x90,
    OP_NOT = 0x91,
    OP_0NOTEQUAL = 0x92,
    OP_ADD = 0x93,
    OP_SUB = 0x94,
    OP_MUL = 0x95,
    OP_DIV = 0x96,
    OP_MOD = 0x97,
    OP_LSHIFT = 0x98,
    OP_RSHIFT = 0x99,
    OP_BOOLAND = 0x9a,
    OP_BOOLOR = 0x9b,
    OP_NUMEQUAL = 0x9c,
    OP_NUMEQUALVERIFY = 0x9d,
    OP_NUMNOTEQUAL = 0x9e,
    OP_LESSTHAN = 0x9f,
    OP_GREATERTHAN = 0xa0,
    OP_LESSTHANOREQUAL = 0xa1,
    OP_GREATERTHANOREQUAL = 0xa2,
    OP_MIN = 0xa3,
    OP_MAX = 0xa4,
    OP_WITHIN = 0xa5,

    // Crypto
    OP_RIPEMD160 = 0xa6,
    OP_SHA1 = 0xa7,
    OP_SHA256 = 0xa8,
    OP_HASH160 = 0xa9,
    OP_HASH256 = 0xaa,
    OP_CODESEPARATOR = 0xab,
    OP_CHECKSIG = 0xac,
    OP_CHECKSIGVERIFY = 0xad,
    OP_CHECKMULTISIG = 0xae,
    OP_CHECKMULTISIGVERIFY = 0xaf,

    // Expansion
    OP_NOP1 = 0xb0,
    OP_CHECKLOCKTIMEVERIFY = 0xb1,
    OP_NOP2 = OP_CHECKLOCKTIMEVERIFY,
    OP_CHECKSEQUENCEVERIFY = 0xb2,
    OP_NOP3 = OP_CHECKSEQUENCEVERIFY,
    OP_NOP4 = 0xb3,
    OP_NOP5 = 0xb4,
    OP_NOP6 = 0xb5,
    OP_NOP7 = 0xb6,
    OP_NOP8 = 0xb7,
    OP_NOP9 = 0xb8,
    OP_NOP10 = 0xb9,

    OP_INVALIDOPCODE = 0xff,
};

/// Get human-readable name for an opcode
std::string GetOpName(Opcode opcode);

// ============================================================================
// ScriptNum - Script integer with special encoding
// ============================================================================

/// Exception for script number errors
class ScriptNumError : public std::runtime_error {
public:
    explicit ScriptNumError(const std::string& msg) : std::runtime_error(msg) {}
};

/// Script number type with Bitcoin-compatible encoding
/// Numbers are encoded in little-endian with sign bit in the most significant byte
class ScriptNum {
public:
    static constexpr size_t DEFAULT_MAX_NUM_SIZE = 4;

    explicit ScriptNum(int64_t n) : value_(n) {}
    
    explicit ScriptNum(const std::vector<uint8_t>& bytes, 
                       bool requireMinimal = true,
                       size_t maxNumSize = DEFAULT_MAX_NUM_SIZE);

    // Accessors
    int64_t GetInt64() const { return value_; }
    int GetInt() const;
    std::vector<uint8_t> GetBytes() const;
    
    // Arithmetic operators
    ScriptNum operator+(int64_t rhs) const { return ScriptNum(value_ + rhs); }
    ScriptNum operator-(int64_t rhs) const { return ScriptNum(value_ - rhs); }
    ScriptNum operator+(const ScriptNum& rhs) const { return ScriptNum(value_ + rhs.value_); }
    ScriptNum operator-(const ScriptNum& rhs) const { return ScriptNum(value_ - rhs.value_); }
    ScriptNum operator-() const { return ScriptNum(-value_); }
    
    // Comparison operators
    bool operator==(int64_t rhs) const { return value_ == rhs; }
    bool operator!=(int64_t rhs) const { return value_ != rhs; }
    bool operator<(int64_t rhs) const { return value_ < rhs; }
    bool operator>(int64_t rhs) const { return value_ > rhs; }
    bool operator<=(int64_t rhs) const { return value_ <= rhs; }
    bool operator>=(int64_t rhs) const { return value_ >= rhs; }
    
    /// Serialize a value to bytes
    static std::vector<uint8_t> Serialize(int64_t value);

private:
    static int64_t SetBytes(const std::vector<uint8_t>& bytes);
    int64_t value_;
};

// ============================================================================
// Script Class
// ============================================================================

/// Serialized script, used inside transaction inputs and outputs
class Script : public std::vector<uint8_t> {
public:
    using base_type = std::vector<uint8_t>;
    using base_type::base_type;  // Inherit constructors
    
    // ========================================================================
    // Construction and Building
    // ========================================================================
    
    /// Push an opcode
    Script& operator<<(Opcode opcode);
    
    /// Push a small integer (0-16)
    Script& operator<<(int64_t n);
    
    /// Push a ScriptNum
    Script& operator<<(const ScriptNum& num);
    
    /// Push raw data with appropriate size prefix
    Script& operator<<(const std::vector<uint8_t>& data);
    
    /// Push Hash160
    Script& operator<<(const Hash160& hash);
    
    /// Push Hash256
    Script& operator<<(const Hash256& hash);
    
    // ========================================================================
    // Parsing
    // ========================================================================
    
    /// Get next opcode and optional data from script
    bool GetOp(const_iterator& pc, Opcode& opcodeRet, std::vector<uint8_t>& dataRet) const;
    bool GetOp(const_iterator& pc, Opcode& opcodeRet) const;
    
    // ========================================================================
    // Script Pattern Detection
    // ========================================================================
    
    /// Check if this is a Pay-to-Public-Key-Hash script
    /// Pattern: OP_DUP OP_HASH160 <20 bytes> OP_EQUALVERIFY OP_CHECKSIG
    bool IsPayToPublicKeyHash() const;
    
    /// Check if this is a Pay-to-Script-Hash script
    /// Pattern: OP_HASH160 <20 bytes> OP_EQUAL
    bool IsPayToScriptHash() const;
    
    /// Check if this script is provably unspendable (starts with OP_RETURN or too large)
    bool IsUnspendable() const;
    
    /// Check if script consists only of push operations
    bool IsPushOnly() const;
    bool IsPushOnly(const_iterator pc) const;
    
    /// Check if all opcodes are valid
    bool HasValidOps() const;
    
    // ========================================================================
    // Data Extraction
    // ========================================================================
    
    /// Extract the public key hash from a P2PKH script
    bool ExtractPubKeyHash(Hash160& hash) const;
    
    /// Extract the script hash from a P2SH script
    bool ExtractScriptHash(Hash160& hash) const;
    
    // ========================================================================
    // Signature Operations
    // ========================================================================
    
    /// Count signature operations (sigops) in this script
    /// @param accurate If true, count multisig accurately; if false, count as 20
    unsigned int GetSigOpCount(bool accurate = false) const;
    
    // ========================================================================
    // Static Helpers
    // ========================================================================
    
    /// Decode OP_N to integer (0-16)
    static int DecodeOP_N(Opcode opcode);
    
    /// Encode integer (0-16) to OP_N
    static Opcode EncodeOP_N(int n);
    
    // ========================================================================
    // Standard Script Builders
    // ========================================================================
    
    /// Create P2PKH script: OP_DUP OP_HASH160 <hash> OP_EQUALVERIFY OP_CHECKSIG
    static Script CreateP2PKH(const Hash160& pubKeyHash);
    
    /// Create P2SH script: OP_HASH160 <hash> OP_EQUAL
    static Script CreateP2SH(const Hash160& scriptHash);
    
    /// Create OP_RETURN script for data embedding
    static Script CreateOpReturn(const std::vector<uint8_t>& data);
    
    // ========================================================================
    // Conversion
    // ========================================================================
    
    /// Convert script to human-readable string
    std::string ToString() const;

private:
    /// Append data size prefix
    void AppendDataSize(uint32_t size);
    
    /// Push integer onto script
    Script& PushInt64(int64_t n);
};

// ============================================================================
// Serialization
// ============================================================================

template<typename Stream>
void Serialize(Stream& s, const Script& script) {
    WriteCompactSize(s, script.size());
    if (!script.empty()) {
        s.Write(script.data(), script.size());
    }
}

template<typename Stream>
void Unserialize(Stream& s, Script& script) {
    uint64_t size = ReadCompactSize(s);
    script.resize(size);
    if (size > 0) {
        s.Read(script.data(), size);
    }
}

} // namespace shurium

#endif // SHURIUM_CORE_SCRIPT_H
