// SHURIUM - Script Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/core/script.h"
#include "shurium/core/hex.h"
#include <cstring>
#include <limits>
#include <sstream>

namespace shurium {

// ============================================================================
// Opcode Names
// ============================================================================

std::string GetOpName(Opcode opcode) {
    switch (opcode) {
        // Push value
        case OP_0: return "OP_0";
        case OP_PUSHDATA1: return "OP_PUSHDATA1";
        case OP_PUSHDATA2: return "OP_PUSHDATA2";
        case OP_PUSHDATA4: return "OP_PUSHDATA4";
        case OP_1NEGATE: return "OP_1NEGATE";
        case OP_RESERVED: return "OP_RESERVED";
        case OP_1: return "OP_1";
        case OP_2: return "OP_2";
        case OP_3: return "OP_3";
        case OP_4: return "OP_4";
        case OP_5: return "OP_5";
        case OP_6: return "OP_6";
        case OP_7: return "OP_7";
        case OP_8: return "OP_8";
        case OP_9: return "OP_9";
        case OP_10: return "OP_10";
        case OP_11: return "OP_11";
        case OP_12: return "OP_12";
        case OP_13: return "OP_13";
        case OP_14: return "OP_14";
        case OP_15: return "OP_15";
        case OP_16: return "OP_16";
        
        // Control
        case OP_NOP: return "OP_NOP";
        case OP_VER: return "OP_VER";
        case OP_IF: return "OP_IF";
        case OP_NOTIF: return "OP_NOTIF";
        case OP_VERIF: return "OP_VERIF";
        case OP_VERNOTIF: return "OP_VERNOTIF";
        case OP_ELSE: return "OP_ELSE";
        case OP_ENDIF: return "OP_ENDIF";
        case OP_VERIFY: return "OP_VERIFY";
        case OP_RETURN: return "OP_RETURN";
        
        // Stack ops
        case OP_TOALTSTACK: return "OP_TOALTSTACK";
        case OP_FROMALTSTACK: return "OP_FROMALTSTACK";
        case OP_2DROP: return "OP_2DROP";
        case OP_2DUP: return "OP_2DUP";
        case OP_3DUP: return "OP_3DUP";
        case OP_2OVER: return "OP_2OVER";
        case OP_2ROT: return "OP_2ROT";
        case OP_2SWAP: return "OP_2SWAP";
        case OP_IFDUP: return "OP_IFDUP";
        case OP_DEPTH: return "OP_DEPTH";
        case OP_DROP: return "OP_DROP";
        case OP_DUP: return "OP_DUP";
        case OP_NIP: return "OP_NIP";
        case OP_OVER: return "OP_OVER";
        case OP_PICK: return "OP_PICK";
        case OP_ROLL: return "OP_ROLL";
        case OP_ROT: return "OP_ROT";
        case OP_SWAP: return "OP_SWAP";
        case OP_TUCK: return "OP_TUCK";
        
        // Splice (disabled)
        case OP_CAT: return "OP_CAT";
        case OP_SUBSTR: return "OP_SUBSTR";
        case OP_LEFT: return "OP_LEFT";
        case OP_RIGHT: return "OP_RIGHT";
        case OP_SIZE: return "OP_SIZE";
        
        // Bit logic
        case OP_INVERT: return "OP_INVERT";
        case OP_AND: return "OP_AND";
        case OP_OR: return "OP_OR";
        case OP_XOR: return "OP_XOR";
        case OP_EQUAL: return "OP_EQUAL";
        case OP_EQUALVERIFY: return "OP_EQUALVERIFY";
        case OP_RESERVED1: return "OP_RESERVED1";
        case OP_RESERVED2: return "OP_RESERVED2";
        
        // Numeric
        case OP_1ADD: return "OP_1ADD";
        case OP_1SUB: return "OP_1SUB";
        case OP_2MUL: return "OP_2MUL";
        case OP_2DIV: return "OP_2DIV";
        case OP_NEGATE: return "OP_NEGATE";
        case OP_ABS: return "OP_ABS";
        case OP_NOT: return "OP_NOT";
        case OP_0NOTEQUAL: return "OP_0NOTEQUAL";
        case OP_ADD: return "OP_ADD";
        case OP_SUB: return "OP_SUB";
        case OP_MUL: return "OP_MUL";
        case OP_DIV: return "OP_DIV";
        case OP_MOD: return "OP_MOD";
        case OP_LSHIFT: return "OP_LSHIFT";
        case OP_RSHIFT: return "OP_RSHIFT";
        case OP_BOOLAND: return "OP_BOOLAND";
        case OP_BOOLOR: return "OP_BOOLOR";
        case OP_NUMEQUAL: return "OP_NUMEQUAL";
        case OP_NUMEQUALVERIFY: return "OP_NUMEQUALVERIFY";
        case OP_NUMNOTEQUAL: return "OP_NUMNOTEQUAL";
        case OP_LESSTHAN: return "OP_LESSTHAN";
        case OP_GREATERTHAN: return "OP_GREATERTHAN";
        case OP_LESSTHANOREQUAL: return "OP_LESSTHANOREQUAL";
        case OP_GREATERTHANOREQUAL: return "OP_GREATERTHANOREQUAL";
        case OP_MIN: return "OP_MIN";
        case OP_MAX: return "OP_MAX";
        case OP_WITHIN: return "OP_WITHIN";
        
        // Crypto
        case OP_RIPEMD160: return "OP_RIPEMD160";
        case OP_SHA1: return "OP_SHA1";
        case OP_SHA256: return "OP_SHA256";
        case OP_HASH160: return "OP_HASH160";
        case OP_HASH256: return "OP_HASH256";
        case OP_CODESEPARATOR: return "OP_CODESEPARATOR";
        case OP_CHECKSIG: return "OP_CHECKSIG";
        case OP_CHECKSIGVERIFY: return "OP_CHECKSIGVERIFY";
        case OP_CHECKMULTISIG: return "OP_CHECKMULTISIG";
        case OP_CHECKMULTISIGVERIFY: return "OP_CHECKMULTISIGVERIFY";
        
        // Expansion
        case OP_NOP1: return "OP_NOP1";
        case OP_CHECKLOCKTIMEVERIFY: return "OP_CHECKLOCKTIMEVERIFY";
        case OP_CHECKSEQUENCEVERIFY: return "OP_CHECKSEQUENCEVERIFY";
        case OP_NOP4: return "OP_NOP4";
        case OP_NOP5: return "OP_NOP5";
        case OP_NOP6: return "OP_NOP6";
        case OP_NOP7: return "OP_NOP7";
        case OP_NOP8: return "OP_NOP8";
        case OP_NOP9: return "OP_NOP9";
        case OP_NOP10: return "OP_NOP10";
        
        case OP_INVALIDOPCODE: return "OP_INVALIDOPCODE";
        
        default:
            return "OP_UNKNOWN";
    }
}

// ============================================================================
// ScriptNum Implementation
// ============================================================================

ScriptNum::ScriptNum(const std::vector<uint8_t>& bytes, bool requireMinimal, size_t maxNumSize) {
    if (bytes.size() > maxNumSize) {
        throw ScriptNumError("script number overflow");
    }
    if (requireMinimal && !bytes.empty()) {
        // Check minimal encoding
        if ((bytes.back() & 0x7f) == 0) {
            if (bytes.size() <= 1 || (bytes[bytes.size() - 2] & 0x80) == 0) {
                throw ScriptNumError("non-minimally encoded script number");
            }
        }
    }
    value_ = SetBytes(bytes);
}

int ScriptNum::GetInt() const {
    if (value_ > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (value_ < std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value_);
}

std::vector<uint8_t> ScriptNum::GetBytes() const {
    return Serialize(value_);
}

std::vector<uint8_t> ScriptNum::Serialize(int64_t value) {
    if (value == 0) {
        return {};
    }
    
    std::vector<uint8_t> result;
    const bool neg = value < 0;
    uint64_t absvalue = neg ? ~static_cast<uint64_t>(value) + 1 : static_cast<uint64_t>(value);
    
    while (absvalue) {
        result.push_back(absvalue & 0xff);
        absvalue >>= 8;
    }
    
    // If MSB >= 0x80, add sign byte
    if (result.back() & 0x80) {
        result.push_back(neg ? 0x80 : 0x00);
    } else if (neg) {
        result.back() |= 0x80;
    }
    
    return result;
}

int64_t ScriptNum::SetBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return 0;
    }
    
    int64_t result = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        result |= static_cast<int64_t>(bytes[i]) << (8 * i);
    }
    
    // Check sign bit
    if (bytes.back() & 0x80) {
        return -(static_cast<int64_t>(result & ~(0x80ULL << (8 * (bytes.size() - 1)))));
    }
    
    return result;
}

// ============================================================================
// Script Implementation
// ============================================================================

void Script::AppendDataSize(uint32_t size) {
    if (size < OP_PUSHDATA1) {
        push_back(static_cast<uint8_t>(size));
    } else if (size <= 0xff) {
        push_back(OP_PUSHDATA1);
        push_back(static_cast<uint8_t>(size));
    } else if (size <= 0xffff) {
        push_back(OP_PUSHDATA2);
        push_back(size & 0xff);
        push_back((size >> 8) & 0xff);
    } else {
        push_back(OP_PUSHDATA4);
        push_back(size & 0xff);
        push_back((size >> 8) & 0xff);
        push_back((size >> 16) & 0xff);
        push_back((size >> 24) & 0xff);
    }
}

Script& Script::operator<<(Opcode opcode) {
    push_back(static_cast<uint8_t>(opcode));
    return *this;
}

Script& Script::PushInt64(int64_t n) {
    if (n == -1 || (n >= 1 && n <= 16)) {
        push_back(n + (OP_1 - 1));
    } else if (n == 0) {
        push_back(OP_0);
    } else {
        *this << ScriptNum::Serialize(n);
    }
    return *this;
}

Script& Script::operator<<(int64_t n) {
    return PushInt64(n);
}

Script& Script::operator<<(const ScriptNum& num) {
    *this << num.GetBytes();
    return *this;
}

Script& Script::operator<<(const std::vector<uint8_t>& data) {
    AppendDataSize(data.size());
    insert(end(), data.begin(), data.end());
    return *this;
}

Script& Script::operator<<(const Hash160& hash) {
    AppendDataSize(20);
    insert(end(), hash.begin(), hash.end());
    return *this;
}

Script& Script::operator<<(const Hash256& hash) {
    AppendDataSize(32);
    insert(end(), hash.begin(), hash.end());
    return *this;
}

bool Script::GetOp(const_iterator& pc, Opcode& opcodeRet, std::vector<uint8_t>& dataRet) const {
    dataRet.clear();
    
    if (pc >= end()) {
        return false;
    }
    
    uint8_t opcode = *pc++;
    opcodeRet = static_cast<Opcode>(opcode);
    
    // Handle data push opcodes
    if (opcode <= OP_PUSHDATA4) {
        size_t nSize = 0;
        
        if (opcode < OP_PUSHDATA1) {
            nSize = opcode;
        } else if (opcode == OP_PUSHDATA1) {
            if (pc >= end()) return false;
            nSize = *pc++;
        } else if (opcode == OP_PUSHDATA2) {
            if (pc + 2 > end()) return false;
            nSize = pc[0] | (pc[1] << 8);
            pc += 2;
        } else if (opcode == OP_PUSHDATA4) {
            if (pc + 4 > end()) return false;
            nSize = pc[0] | (pc[1] << 8) | (pc[2] << 16) | (pc[3] << 24);
            pc += 4;
        }
        
        if (pc + nSize > end()) {
            return false;
        }
        
        dataRet.assign(pc, pc + nSize);
        pc += nSize;
    }
    
    return true;
}

bool Script::GetOp(const_iterator& pc, Opcode& opcodeRet) const {
    std::vector<uint8_t> data;
    return GetOp(pc, opcodeRet, data);
}

bool Script::IsPayToPublicKeyHash() const {
    // OP_DUP OP_HASH160 <20 bytes> OP_EQUALVERIFY OP_CHECKSIG
    return size() == 25 &&
           (*this)[0] == OP_DUP &&
           (*this)[1] == OP_HASH160 &&
           (*this)[2] == 20 &&
           (*this)[23] == OP_EQUALVERIFY &&
           (*this)[24] == OP_CHECKSIG;
}

bool Script::IsPayToScriptHash() const {
    // OP_HASH160 <20 bytes> OP_EQUAL
    return size() == 23 &&
           (*this)[0] == OP_HASH160 &&
           (*this)[1] == 20 &&
           (*this)[22] == OP_EQUAL;
}

bool Script::IsUnspendable() const {
    return (size() > 0 && (*this)[0] == OP_RETURN) || 
           (size() > static_cast<size_t>(MAX_SCRIPT_SIZE));
}

bool Script::IsPushOnly(const_iterator pc) const {
    while (pc < end()) {
        Opcode opcode;
        if (!GetOp(pc, opcode)) {
            return false;
        }
        if (opcode > OP_16) {
            return false;
        }
    }
    return true;
}

bool Script::IsPushOnly() const {
    return IsPushOnly(begin());
}

bool Script::HasValidOps() const {
    auto pc = begin();
    while (pc < end()) {
        Opcode opcode;
        std::vector<uint8_t> data;
        if (!GetOp(pc, opcode, data)) {
            return false;
        }
        if (opcode > OP_NOP10 && opcode != OP_INVALIDOPCODE) {
            return false;
        }
    }
    return true;
}

bool Script::ExtractPubKeyHash(Hash160& hash) const {
    if (!IsPayToPublicKeyHash()) {
        return false;
    }
    std::memcpy(hash.data(), data() + 3, 20);
    return true;
}

bool Script::ExtractScriptHash(Hash160& hash) const {
    if (!IsPayToScriptHash()) {
        return false;
    }
    std::memcpy(hash.data(), data() + 2, 20);
    return true;
}

unsigned int Script::GetSigOpCount(bool accurate) const {
    unsigned int n = 0;
    auto pc = begin();
    Opcode lastOpcode = OP_INVALIDOPCODE;
    
    while (pc < end()) {
        Opcode opcode;
        if (!GetOp(pc, opcode)) {
            break;
        }
        
        if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY) {
            n++;
        } else if (opcode == OP_CHECKMULTISIG || opcode == OP_CHECKMULTISIGVERIFY) {
            if (accurate && lastOpcode >= OP_1 && lastOpcode <= OP_16) {
                n += DecodeOP_N(lastOpcode);
            } else {
                n += MAX_PUBKEYS_PER_MULTISIG;
            }
        }
        
        lastOpcode = opcode;
    }
    
    return n;
}

int Script::DecodeOP_N(Opcode opcode) {
    if (opcode == OP_0) {
        return 0;
    }
    if (opcode < OP_1 || opcode > OP_16) {
        return -1;
    }
    return static_cast<int>(opcode) - static_cast<int>(OP_1) + 1;
}

Opcode Script::EncodeOP_N(int n) {
    if (n < 0 || n > 16) {
        return OP_INVALIDOPCODE;
    }
    if (n == 0) {
        return OP_0;
    }
    return static_cast<Opcode>(OP_1 + n - 1);
}

Script Script::CreateP2PKH(const Hash160& pubKeyHash) {
    Script script;
    script << OP_DUP << OP_HASH160 << pubKeyHash << OP_EQUALVERIFY << OP_CHECKSIG;
    return script;
}

Script Script::CreateP2SH(const Hash160& scriptHash) {
    Script script;
    script << OP_HASH160 << scriptHash << OP_EQUAL;
    return script;
}

Script Script::CreateOpReturn(const std::vector<uint8_t>& data) {
    Script script;
    script << OP_RETURN << data;
    return script;
}

std::string Script::ToString() const {
    std::ostringstream oss;
    auto pc = begin();
    bool first = true;
    
    while (pc < end()) {
        if (!first) {
            oss << " ";
        }
        first = false;
        
        Opcode opcode;
        std::vector<uint8_t> data;
        if (!GetOp(pc, opcode, data)) {
            oss << "[error]";
            break;
        }
        
        if (data.empty()) {
            oss << GetOpName(opcode);
        } else {
            oss << BytesToHex(data);
        }
    }
    
    return oss.str();
}

} // namespace shurium
