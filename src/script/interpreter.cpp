// SHURIUM - Script Interpreter Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file implements the script interpreter for validating transactions.
// Based on Bitcoin's script system with simplifications.

#include "shurium/script/interpreter.h"
#include "shurium/crypto/sha256.h"
#include "shurium/crypto/ripemd160.h"
#include "shurium/core/serialize.h"
#include <algorithm>
#include <cstring>

namespace shurium {

// ============================================================================
// Helper Functions
// ============================================================================

std::string ScriptErrorString(ScriptError err) {
    switch (err) {
        case ScriptError::OK: return "No error";
        case ScriptError::UNKNOWN: return "Unknown error";
        case ScriptError::EVAL_FALSE: return "Script evaluated without error but finished with a false/empty top stack element";
        case ScriptError::OP_RETURN: return "OP_RETURN was encountered";
        case ScriptError::SCRIPT_SIZE: return "Script is too large";
        case ScriptError::PUSH_SIZE: return "Push value size limit exceeded";
        case ScriptError::OP_COUNT: return "Operation limit exceeded";
        case ScriptError::STACK_SIZE: return "Stack size limit exceeded";
        case ScriptError::SIG_COUNT: return "Signature count negative or greater than pubkey count";
        case ScriptError::PUBKEY_COUNT: return "Pubkey count negative or limit exceeded";
        case ScriptError::INVALID_OPERAND_SIZE: return "Invalid operand size";
        case ScriptError::INVALID_NUMBER_RANGE: return "Script number overflow";
        case ScriptError::IMPOSSIBLE_ENCODING: return "Impossible encoding";
        case ScriptError::INVALID_SPLIT_RANGE: return "Invalid split range";
        case ScriptError::VERIFY: return "OP_VERIFY failed";
        case ScriptError::EQUALVERIFY: return "OP_EQUALVERIFY failed";
        case ScriptError::CHECKMULTISIGVERIFY: return "OP_CHECKMULTISIGVERIFY failed";
        case ScriptError::CHECKSIGVERIFY: return "OP_CHECKSIGVERIFY failed";
        case ScriptError::NUMEQUALVERIFY: return "OP_NUMEQUALVERIFY failed";
        case ScriptError::BAD_OPCODE: return "Bad opcode";
        case ScriptError::DISABLED_OPCODE: return "Disabled opcode";
        case ScriptError::INVALID_STACK_OPERATION: return "Invalid stack operation";
        case ScriptError::INVALID_ALTSTACK_OPERATION: return "Invalid altstack operation";
        case ScriptError::UNBALANCED_CONDITIONAL: return "Unbalanced conditional";
        case ScriptError::SIG_HASHTYPE: return "Signature hash type missing or not understood";
        case ScriptError::SIG_DER: return "Non-canonical DER signature";
        case ScriptError::SIG_HIGH_S: return "Non-canonical signature: S value is unnecessarily high";
        case ScriptError::SIG_NULLDUMMY: return "Dummy CHECKMULTISIG argument must be zero";
        case ScriptError::PUBKEYTYPE: return "Public key is neither compressed nor uncompressed";
        case ScriptError::CLEANSTACK: return "Stack size must be exactly one after execution";
        case ScriptError::MINIMALDATA: return "Data push larger than necessary";
        case ScriptError::MINIMALIF: return "OP_IF/NOTIF argument must be minimal";
        case ScriptError::SIG_NULLFAIL: return "Signature must be zero for failed CHECK(MULTI)SIG operation";
        case ScriptError::NEGATIVE_LOCKTIME: return "Negative locktime";
        case ScriptError::UNSATISFIED_LOCKTIME: return "Locktime requirement not satisfied";
        case ScriptError::SIG_PUSHONLY: return "Only push operators allowed in signatures";
        case ScriptError::DISCOURAGE_UPGRADABLE_NOPS: return "NOPx reserved for soft-fork upgrades";
        case ScriptError::PUBKEY_RECOVERY_FAILED: return "Public key recovery failed";
        default: return "Unknown error";
    }
}

bool CastToBool(const std::vector<uint8_t>& vch) {
    for (size_t i = 0; i < vch.size(); i++) {
        if (vch[i] != 0) {
            // Can be negative zero
            if (i == vch.size() - 1 && vch[i] == 0x80) {
                return false;
            }
            return true;
        }
    }
    return false;
}

// Numeric opcodes require minimal encoding
static bool IsMinimallyEncoded(const std::vector<uint8_t>& vch, size_t maxSize) {
    if (vch.size() > maxSize) {
        return false;
    }
    if (vch.size() > 0) {
        // Check if the top byte is 0x00 or 0x80 with unnecessary padding
        if ((vch.back() & 0x7f) == 0) {
            if (vch.size() <= 1 || (vch[vch.size() - 2] & 0x80) == 0) {
                return false;
            }
        }
    }
    return true;
}

bool IsValidPubKey(const std::vector<uint8_t>& pubkey) {
    if (pubkey.size() < 33) return false;
    
    if (pubkey[0] == 0x04) {
        // Uncompressed key: 65 bytes
        return pubkey.size() == 65;
    } else if (pubkey[0] == 0x02 || pubkey[0] == 0x03) {
        // Compressed key: 33 bytes
        return pubkey.size() == 33;
    }
    return false;
}

bool IsCompressedPubKey(const std::vector<uint8_t>& pubkey) {
    if (pubkey.size() != 33) return false;
    return pubkey[0] == 0x02 || pubkey[0] == 0x03;
}

bool IsValidDERSignature(const std::vector<uint8_t>& sig) {
    // Minimum DER signature is 8 bytes (empty R and S)
    // Maximum is around 72 bytes
    if (sig.size() < 9 || sig.size() > 73) return false;
    
    // A signature is of type 0x30 (compound)
    if (sig[0] != 0x30) return false;
    
    // Length of compound structure
    if (sig[1] != sig.size() - 3) return false;
    
    // R value
    if (sig[2] != 0x02) return false;
    size_t rLen = sig[3];
    if (rLen == 0 || rLen > 33) return false;
    if (4 + rLen >= sig.size()) return false;
    
    // R must not be negative (high bit set) unless it has a 0x00 prefix
    if (sig[4] & 0x80) return false;
    // R must not have leading zeros unless necessary
    if (rLen > 1 && sig[4] == 0x00 && !(sig[5] & 0x80)) return false;
    
    // S value
    size_t sPos = 4 + rLen;
    if (sig[sPos] != 0x02) return false;
    size_t sLen = sig[sPos + 1];
    if (sLen == 0 || sLen > 33) return false;
    if (sPos + 2 + sLen != sig.size() - 1) return false;  // -1 for sighash byte
    
    // S must not be negative
    if (sig[sPos + 2] & 0x80) return false;
    // S must not have leading zeros unless necessary
    if (sLen > 1 && sig[sPos + 2] == 0x00 && !(sig[sPos + 3] & 0x80)) return false;
    
    return true;
}

bool IsLowDERSignature(const std::vector<uint8_t>& sig, ScriptError* error) {
    if (!IsValidDERSignature(sig)) {
        if (error) *error = ScriptError::SIG_DER;
        return false;
    }
    
    // Extract S value and check if it's low
    // For now, accept all valid DER signatures
    // Full implementation would compare S against curve order/2
    return true;
}

// ============================================================================
// Signature Hash Calculation
// ============================================================================

Hash256 SignatureHash(const Transaction& tx, unsigned int nIn,
                      const Script& scriptCode, uint8_t nHashType) {
    if (nIn >= tx.vin.size()) {
        // Invalid input index - return 1 (special case in Bitcoin)
        Hash256 one;
        one[0] = 1;
        return one;
    }
    
    // Create a modified copy of the transaction for signing
    MutableTransaction txCopy(tx);
    
    // Clear all input scripts
    for (auto& input : txCopy.vin) {
        input.scriptSig.clear();
    }
    
    // Set the script for the input being signed
    // Remove OP_CODESEPARATOR from scriptCode
    Script scriptCodeCopy;
    auto pc = scriptCode.begin();
    while (pc < scriptCode.end()) {
        Opcode opcode;
        std::vector<uint8_t> data;
        if (!scriptCode.GetOp(pc, opcode, data)) break;
        if (opcode != OP_CODESEPARATOR) {
            if (data.empty()) {
                scriptCodeCopy << opcode;
            } else {
                scriptCodeCopy << data;
            }
        }
    }
    txCopy.vin[nIn].scriptSig = scriptCodeCopy;
    
    // Handle different hash types
    uint8_t nHashTypeBase = nHashType & 0x1f;
    
    if (nHashTypeBase == SIGHASH_NONE) {
        // Sign none of the outputs
        txCopy.vout.clear();
        // Set sequence to 0 for other inputs
        for (size_t i = 0; i < txCopy.vin.size(); i++) {
            if (i != nIn) {
                txCopy.vin[i].nSequence = 0;
            }
        }
    } else if (nHashTypeBase == SIGHASH_SINGLE) {
        // Sign only the output at the same index
        if (nIn >= txCopy.vout.size()) {
            // Return special value for SIGHASH_SINGLE bug
            Hash256 one;
            one[0] = 1;
            return one;
        }
        txCopy.vout.resize(nIn + 1);
        for (size_t i = 0; i < nIn; i++) {
            txCopy.vout[i].SetNull();
        }
        // Set sequence to 0 for other inputs
        for (size_t i = 0; i < txCopy.vin.size(); i++) {
            if (i != nIn) {
                txCopy.vin[i].nSequence = 0;
            }
        }
    }
    
    if (nHashType & SIGHASH_ANYONECANPAY) {
        // Sign only this input
        TxIn input = txCopy.vin[nIn];
        txCopy.vin.clear();
        txCopy.vin.push_back(input);
    }
    
    // Serialize the modified transaction
    DataStream ss;
    Serialize(ss, txCopy);
    
    // Append hash type as 4 bytes (little endian)
    uint32_t hashType32 = nHashType;
    ss.Write(reinterpret_cast<const char*>(&hashType32), 4);
    
    // Double SHA256
    return DoubleSHA256(ss.data(), ss.size());
}

// ============================================================================
// TransactionSignatureChecker Implementation
// ============================================================================

TransactionSignatureChecker::TransactionSignatureChecker(const Transaction* tx,
                                                         unsigned int nIn,
                                                         Amount amount)
    : txTo_(tx), nIn_(nIn), amount_(amount) {}

Hash256 TransactionSignatureChecker::ComputeSignatureHash(const Script& scriptCode,
                                                          uint8_t nHashType) const {
    return SignatureHash(*txTo_, nIn_, scriptCode, nHashType);
}

bool TransactionSignatureChecker::CheckSig(const std::vector<uint8_t>& signature,
                                           const std::vector<uint8_t>& pubkeyData,
                                           const Script& scriptCode) const {
    if (signature.empty()) return false;
    if (pubkeyData.empty()) return false;
    
    // Extract hash type (last byte of signature)
    uint8_t nHashType = signature.back();
    std::vector<uint8_t> sigWithoutHashType(signature.begin(), signature.end() - 1);
    
    // Compute the signature hash
    Hash256 sighash = ComputeSignatureHash(scriptCode, nHashType);
    
    // Create public key and verify
    PublicKey pubkey(pubkeyData);
    if (!pubkey.IsValid()) return false;
    
    return pubkey.Verify(sighash, sigWithoutHashType);
}

bool TransactionSignatureChecker::CheckLockTime(int64_t nLockTime) const {
    // There are two kinds of nLockTime: lock-by-blockheight and lock-by-blocktime
    // They are distinguished by whether nLockTime < LOCKTIME_THRESHOLD
    static constexpr int64_t LOCKTIME_THRESHOLD = 500000000;
    
    // Both must be the same type
    if (!((txTo_->nLockTime < LOCKTIME_THRESHOLD && nLockTime < LOCKTIME_THRESHOLD) ||
          (txTo_->nLockTime >= LOCKTIME_THRESHOLD && nLockTime >= LOCKTIME_THRESHOLD))) {
        return false;
    }
    
    // nLockTime must be <= transaction locktime
    if (nLockTime > static_cast<int64_t>(txTo_->nLockTime)) {
        return false;
    }
    
    // Sequence must not be final
    if (txTo_->vin[nIn_].nSequence == TxIn::SEQUENCE_FINAL) {
        return false;
    }
    
    return true;
}

bool TransactionSignatureChecker::CheckSequence(int64_t nSequence) const {
    // Relative lock-times are supported by comparing the passed
    // in operand to the sequence number of the input.
    int64_t txSequence = static_cast<int64_t>(txTo_->vin[nIn_].nSequence);
    
    // Sequence numbers with their most significant bit set are not
    // consensus constrained. Testing that the transaction's sequence
    // number does not have this bit set prevents using this property
    // to get around a CHECKSEQUENCEVERIFY check.
    if (txSequence & TxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
        return false;
    }
    
    // Mask off any bits that do not have consensus-enforced meaning
    // before doing the integer comparisons
    static constexpr uint32_t MASK = TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | TxIn::SEQUENCE_LOCKTIME_MASK;
    
    // Must be same type (time vs blocks)
    if (!((nSequence & TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) == 
          (txSequence & TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG))) {
        return false;
    }
    
    // Compare the masked values
    if ((nSequence & MASK) > (txSequence & MASK)) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Script Evaluation
// ============================================================================

// Helper macros for stack operations
#define stacktop(i) (stack.at(stack.size() + (i)))
#define altstacktop(i) (altstack.at(altstack.size() + (i)))

static inline void popstack(std::vector<std::vector<uint8_t>>& stack) {
    if (stack.empty()) throw std::runtime_error("popstack: empty stack");
    stack.pop_back();
}

bool EvalScript(std::vector<std::vector<uint8_t>>& stack,
                const Script& script,
                ScriptFlags flags,
                const BaseSignatureChecker& checker,
                ScriptError* serror) {
    
    static const std::vector<uint8_t> vchFalse(0);
    static const std::vector<uint8_t> vchTrue(1, 1);
    
    auto SetError = [&](ScriptError err) {
        if (serror) *serror = err;
        return false;
    };
    
    if (script.size() > MAX_SCRIPT_SIZE) {
        return SetError(ScriptError::SCRIPT_SIZE);
    }
    
    int nOpCount = 0;
    bool fRequireMinimal = HasFlag(flags, ScriptFlags::VERIFY_MINIMALDATA);
    
    std::vector<std::vector<uint8_t>> altstack;
    std::vector<bool> vfExec;  // Track if we're in an executing branch
    
    try {
        auto pc = script.begin();
        auto pend = script.end();
        auto pbegincodehash = script.begin();
        
        while (pc < pend) {
            bool fExec = vfExec.empty() || vfExec.back();
            
            Opcode opcode;
            std::vector<uint8_t> vchPushValue;
            if (!script.GetOp(pc, opcode, vchPushValue)) {
                return SetError(ScriptError::BAD_OPCODE);
            }
            
            if (vchPushValue.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                return SetError(ScriptError::PUSH_SIZE);
            }
            
            // Count non-push opcodes
            if (opcode > OP_16) {
                nOpCount++;
                if (nOpCount > MAX_OPS_PER_SCRIPT) {
                    return SetError(ScriptError::OP_COUNT);
                }
            }
            
            // Disabled opcodes
            if (opcode == OP_CAT || opcode == OP_SUBSTR || opcode == OP_LEFT ||
                opcode == OP_RIGHT || opcode == OP_INVERT || opcode == OP_AND ||
                opcode == OP_OR || opcode == OP_XOR || opcode == OP_2MUL ||
                opcode == OP_2DIV || opcode == OP_MUL || opcode == OP_DIV ||
                opcode == OP_MOD || opcode == OP_LSHIFT || opcode == OP_RSHIFT) {
                return SetError(ScriptError::DISABLED_OPCODE);
            }
            
            if (fExec && opcode <= OP_PUSHDATA4) {
                // Push data
                if (fRequireMinimal && !IsMinimallyEncoded(vchPushValue, MAX_SCRIPT_ELEMENT_SIZE)) {
                    return SetError(ScriptError::MINIMALDATA);
                }
                stack.push_back(vchPushValue);
            } else if (fExec || (OP_IF <= opcode && opcode <= OP_ENDIF)) {
                switch (opcode) {
                    // ============================================================
                    // Push values
                    // ============================================================
                    case OP_1NEGATE:
                    case OP_1:
                    case OP_2:
                    case OP_3:
                    case OP_4:
                    case OP_5:
                    case OP_6:
                    case OP_7:
                    case OP_8:
                    case OP_9:
                    case OP_10:
                    case OP_11:
                    case OP_12:
                    case OP_13:
                    case OP_14:
                    case OP_15:
                    case OP_16: {
                        // OP_1NEGATE = -1, OP_1 through OP_16 = 1 through 16
                        ScriptNum bn((opcode == OP_1NEGATE) ? -1 : (opcode - (OP_1 - 1)));
                        stack.push_back(bn.GetBytes());
                        break;
                    }
                    
                    // ============================================================
                    // Control flow
                    // ============================================================
                    case OP_NOP:
                        break;
                    
                    case OP_CHECKLOCKTIMEVERIFY: {
                        if (!HasFlag(flags, ScriptFlags::VERIFY_CHECKLOCKTIMEVERIFY)) {
                            // Not enabled; treat as NOP
                            if (HasFlag(flags, ScriptFlags::VERIFY_DISCOURAGE_UPGRADABLE_NOPS)) {
                                return SetError(ScriptError::DISCOURAGE_UPGRADABLE_NOPS);
                            }
                            break;
                        }
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        // Note: We use 5-byte numbers because locktime is uint32
                        ScriptNum nLockTime(stacktop(-1), fRequireMinimal, 5);
                        if (nLockTime < 0) {
                            return SetError(ScriptError::NEGATIVE_LOCKTIME);
                        }
                        if (!checker.CheckLockTime(nLockTime.GetInt64())) {
                            return SetError(ScriptError::UNSATISFIED_LOCKTIME);
                        }
                        break;
                    }
                    
                    case OP_CHECKSEQUENCEVERIFY: {
                        if (!HasFlag(flags, ScriptFlags::VERIFY_CHECKSEQUENCEVERIFY)) {
                            if (HasFlag(flags, ScriptFlags::VERIFY_DISCOURAGE_UPGRADABLE_NOPS)) {
                                return SetError(ScriptError::DISCOURAGE_UPGRADABLE_NOPS);
                            }
                            break;
                        }
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        ScriptNum nSequence(stacktop(-1), fRequireMinimal, 5);
                        if (nSequence < 0) {
                            return SetError(ScriptError::NEGATIVE_LOCKTIME);
                        }
                        // Disabled bit allows bypass
                        if ((nSequence.GetInt64() & TxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) != 0) {
                            break;
                        }
                        if (!checker.CheckSequence(nSequence.GetInt64())) {
                            return SetError(ScriptError::UNSATISFIED_LOCKTIME);
                        }
                        break;
                    }
                    
                    case OP_NOP1:
                    case OP_NOP4:
                    case OP_NOP5:
                    case OP_NOP6:
                    case OP_NOP7:
                    case OP_NOP8:
                    case OP_NOP9:
                    case OP_NOP10: {
                        if (HasFlag(flags, ScriptFlags::VERIFY_DISCOURAGE_UPGRADABLE_NOPS)) {
                            return SetError(ScriptError::DISCOURAGE_UPGRADABLE_NOPS);
                        }
                        break;
                    }
                    
                    case OP_IF:
                    case OP_NOTIF: {
                        bool fValue = false;
                        if (fExec) {
                            if (stack.size() < 1) {
                                return SetError(ScriptError::UNBALANCED_CONDITIONAL);
                            }
                            std::vector<uint8_t>& vch = stacktop(-1);
                            fValue = CastToBool(vch);
                            if (opcode == OP_NOTIF) {
                                fValue = !fValue;
                            }
                            popstack(stack);
                        }
                        vfExec.push_back(fValue);
                        break;
                    }
                    
                    case OP_ELSE: {
                        if (vfExec.empty()) {
                            return SetError(ScriptError::UNBALANCED_CONDITIONAL);
                        }
                        vfExec.back() = !vfExec.back();
                        break;
                    }
                    
                    case OP_ENDIF: {
                        if (vfExec.empty()) {
                            return SetError(ScriptError::UNBALANCED_CONDITIONAL);
                        }
                        vfExec.pop_back();
                        break;
                    }
                    
                    case OP_VERIFY: {
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        if (!CastToBool(stacktop(-1))) {
                            return SetError(ScriptError::VERIFY);
                        }
                        popstack(stack);
                        break;
                    }
                    
                    case OP_RETURN: {
                        return SetError(ScriptError::OP_RETURN);
                    }
                    
                    // ============================================================
                    // Stack operations
                    // ============================================================
                    case OP_TOALTSTACK: {
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        altstack.push_back(stacktop(-1));
                        popstack(stack);
                        break;
                    }
                    
                    case OP_FROMALTSTACK: {
                        if (altstack.size() < 1) {
                            return SetError(ScriptError::INVALID_ALTSTACK_OPERATION);
                        }
                        stack.push_back(altstacktop(-1));
                        popstack(altstack);
                        break;
                    }
                    
                    case OP_2DROP: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        popstack(stack);
                        popstack(stack);
                        break;
                    }
                    
                    case OP_2DUP: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> v1 = stacktop(-2);
                        std::vector<uint8_t> v2 = stacktop(-1);
                        stack.push_back(v1);
                        stack.push_back(v2);
                        break;
                    }
                    
                    case OP_3DUP: {
                        if (stack.size() < 3) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> v1 = stacktop(-3);
                        std::vector<uint8_t> v2 = stacktop(-2);
                        std::vector<uint8_t> v3 = stacktop(-1);
                        stack.push_back(v1);
                        stack.push_back(v2);
                        stack.push_back(v3);
                        break;
                    }
                    
                    case OP_2OVER: {
                        if (stack.size() < 4) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> v1 = stacktop(-4);
                        std::vector<uint8_t> v2 = stacktop(-3);
                        stack.push_back(v1);
                        stack.push_back(v2);
                        break;
                    }
                    
                    case OP_2ROT: {
                        if (stack.size() < 6) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> v1 = stacktop(-6);
                        std::vector<uint8_t> v2 = stacktop(-5);
                        stack.erase(stack.end() - 6, stack.end() - 4);
                        stack.push_back(v1);
                        stack.push_back(v2);
                        break;
                    }
                    
                    case OP_2SWAP: {
                        if (stack.size() < 4) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::swap(stacktop(-4), stacktop(-2));
                        std::swap(stacktop(-3), stacktop(-1));
                        break;
                    }
                    
                    case OP_IFDUP: {
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> vch = stacktop(-1);
                        if (CastToBool(vch)) {
                            stack.push_back(vch);
                        }
                        break;
                    }
                    
                    case OP_DEPTH: {
                        ScriptNum bn(static_cast<int64_t>(stack.size()));
                        stack.push_back(bn.GetBytes());
                        break;
                    }
                    
                    case OP_DROP: {
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        popstack(stack);
                        break;
                    }
                    
                    case OP_DUP: {
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> vch = stacktop(-1);
                        stack.push_back(vch);
                        break;
                    }
                    
                    case OP_NIP: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        stack.erase(stack.end() - 2);
                        break;
                    }
                    
                    case OP_OVER: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> vch = stacktop(-2);
                        stack.push_back(vch);
                        break;
                    }
                    
                    case OP_PICK:
                    case OP_ROLL: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        int n = ScriptNum(stacktop(-1), fRequireMinimal).GetInt();
                        popstack(stack);
                        if (n < 0 || static_cast<size_t>(n) >= stack.size()) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> vch = stacktop(-n - 1);
                        if (opcode == OP_ROLL) {
                            stack.erase(stack.end() - n - 1);
                        }
                        stack.push_back(vch);
                        break;
                    }
                    
                    case OP_ROT: {
                        if (stack.size() < 3) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::swap(stacktop(-3), stacktop(-2));
                        std::swap(stacktop(-2), stacktop(-1));
                        break;
                    }
                    
                    case OP_SWAP: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::swap(stacktop(-2), stacktop(-1));
                        break;
                    }
                    
                    case OP_TUCK: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t> vch = stacktop(-1);
                        stack.insert(stack.end() - 2, vch);
                        break;
                    }
                    
                    case OP_SIZE: {
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        ScriptNum bn(static_cast<int64_t>(stacktop(-1).size()));
                        stack.push_back(bn.GetBytes());
                        break;
                    }
                    
                    // ============================================================
                    // Bitwise logic
                    // ============================================================
                    case OP_EQUAL:
                    case OP_EQUALVERIFY: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t>& v1 = stacktop(-2);
                        std::vector<uint8_t>& v2 = stacktop(-1);
                        bool fEqual = (v1 == v2);
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fEqual ? vchTrue : vchFalse);
                        if (opcode == OP_EQUALVERIFY) {
                            if (fEqual) {
                                popstack(stack);
                            } else {
                                return SetError(ScriptError::EQUALVERIFY);
                            }
                        }
                        break;
                    }
                    
                    // ============================================================
                    // Numeric operations
                    // ============================================================
                    case OP_1ADD:
                    case OP_1SUB:
                    case OP_NEGATE:
                    case OP_ABS:
                    case OP_NOT:
                    case OP_0NOTEQUAL: {
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        ScriptNum bn(stacktop(-1), fRequireMinimal);
                        switch (opcode) {
                            case OP_1ADD:       bn = bn + 1; break;
                            case OP_1SUB:       bn = bn - 1; break;
                            case OP_NEGATE:     bn = -bn; break;
                            case OP_ABS:        if (bn < 0) bn = -bn; break;
                            case OP_NOT:        bn = ScriptNum(bn == 0 ? 1 : 0); break;
                            case OP_0NOTEQUAL:  bn = ScriptNum(bn != 0 ? 1 : 0); break;
                            default: break;
                        }
                        popstack(stack);
                        stack.push_back(bn.GetBytes());
                        break;
                    }
                    
                    case OP_ADD:
                    case OP_SUB:
                    case OP_BOOLAND:
                    case OP_BOOLOR:
                    case OP_NUMEQUAL:
                    case OP_NUMEQUALVERIFY:
                    case OP_NUMNOTEQUAL:
                    case OP_LESSTHAN:
                    case OP_GREATERTHAN:
                    case OP_LESSTHANOREQUAL:
                    case OP_GREATERTHANOREQUAL:
                    case OP_MIN:
                    case OP_MAX: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        ScriptNum bn1(stacktop(-2), fRequireMinimal);
                        ScriptNum bn2(stacktop(-1), fRequireMinimal);
                        ScriptNum bn(0);
                        switch (opcode) {
                            case OP_ADD:
                                bn = bn1 + bn2;
                                break;
                            case OP_SUB:
                                bn = bn1 - bn2;
                                break;
                            case OP_BOOLAND:
                                bn = ScriptNum((bn1 != 0 && bn2 != 0) ? 1 : 0);
                                break;
                            case OP_BOOLOR:
                                bn = ScriptNum((bn1 != 0 || bn2 != 0) ? 1 : 0);
                                break;
                            case OP_NUMEQUAL:
                            case OP_NUMEQUALVERIFY:
                                bn = ScriptNum((bn1.GetInt64() == bn2.GetInt64()) ? 1 : 0);
                                break;
                            case OP_NUMNOTEQUAL:
                                bn = ScriptNum((bn1.GetInt64() != bn2.GetInt64()) ? 1 : 0);
                                break;
                            case OP_LESSTHAN:
                                bn = ScriptNum((bn1 < bn2.GetInt64()) ? 1 : 0);
                                break;
                            case OP_GREATERTHAN:
                                bn = ScriptNum((bn1 > bn2.GetInt64()) ? 1 : 0);
                                break;
                            case OP_LESSTHANOREQUAL:
                                bn = ScriptNum((bn1 <= bn2.GetInt64()) ? 1 : 0);
                                break;
                            case OP_GREATERTHANOREQUAL:
                                bn = ScriptNum((bn1 >= bn2.GetInt64()) ? 1 : 0);
                                break;
                            case OP_MIN:
                                bn = (bn1 < bn2.GetInt64()) ? bn1 : bn2;
                                break;
                            case OP_MAX:
                                bn = (bn1 > bn2.GetInt64()) ? bn1 : bn2;
                                break;
                            default:
                                break;
                        }
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(bn.GetBytes());
                        
                        if (opcode == OP_NUMEQUALVERIFY) {
                            if (CastToBool(stacktop(-1))) {
                                popstack(stack);
                            } else {
                                return SetError(ScriptError::NUMEQUALVERIFY);
                            }
                        }
                        break;
                    }
                    
                    case OP_WITHIN: {
                        if (stack.size() < 3) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        ScriptNum bn1(stacktop(-3), fRequireMinimal);
                        ScriptNum bn2(stacktop(-2), fRequireMinimal);
                        ScriptNum bn3(stacktop(-1), fRequireMinimal);
                        bool fValue = (bn2 <= bn1.GetInt64() && bn1 < bn3.GetInt64());
                        popstack(stack);
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fValue ? vchTrue : vchFalse);
                        break;
                    }
                    
                    // ============================================================
                    // Crypto operations
                    // ============================================================
                    case OP_RIPEMD160:
                    case OP_SHA256:
                    case OP_HASH160:
                    case OP_HASH256: {
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        std::vector<uint8_t>& vch = stacktop(-1);
                        std::vector<uint8_t> vchHash;
                        
                        if (opcode == OP_RIPEMD160) {
                            Hash160 hash = RIPEMD160Hash(vch);
                            vchHash.assign(hash.begin(), hash.end());
                        } else if (opcode == OP_SHA256) {
                            Hash256 hash = SHA256Hash(vch);
                            vchHash.assign(hash.begin(), hash.end());
                        } else if (opcode == OP_HASH160) {
                            // RIPEMD160(SHA256(x))
                            Hash160 hash = Hash160FromData(vch);
                            vchHash.assign(hash.begin(), hash.end());
                        } else if (opcode == OP_HASH256) {
                            // SHA256(SHA256(x))
                            Hash256 hash = DoubleSHA256(vch);
                            vchHash.assign(hash.begin(), hash.end());
                        }
                        popstack(stack);
                        stack.push_back(vchHash);
                        break;
                    }
                    
                    case OP_CODESEPARATOR: {
                        pbegincodehash = pc;
                        break;
                    }
                    
                    case OP_CHECKSIG:
                    case OP_CHECKSIGVERIFY: {
                        if (stack.size() < 2) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        
                        std::vector<uint8_t>& vchSig = stacktop(-2);
                        std::vector<uint8_t>& vchPubKey = stacktop(-1);
                        
                        // Create subscript from codeseparator
                        Script scriptCode(pbegincodehash, pend);
                        
                        // Check signature encoding
                        if (!vchSig.empty()) {
                            if (HasFlag(flags, ScriptFlags::VERIFY_STRICTENC)) {
                                if (!IsValidDERSignature(vchSig)) {
                                    return SetError(ScriptError::SIG_DER);
                                }
                            }
                            if (HasFlag(flags, ScriptFlags::VERIFY_LOW_S)) {
                                if (!IsLowDERSignature(vchSig, serror)) {
                                    return false;
                                }
                            }
                        }
                        
                        // Check pubkey encoding
                        if (HasFlag(flags, ScriptFlags::VERIFY_STRICTENC)) {
                            if (!IsValidPubKey(vchPubKey)) {
                                return SetError(ScriptError::PUBKEYTYPE);
                            }
                        }
                        if (HasFlag(flags, ScriptFlags::VERIFY_COMPRESSED_PUBKEY)) {
                            if (!IsCompressedPubKey(vchPubKey)) {
                                return SetError(ScriptError::PUBKEYTYPE);
                            }
                        }
                        
                        bool fSuccess = checker.CheckSig(vchSig, vchPubKey, scriptCode);
                        
                        if (!fSuccess && HasFlag(flags, ScriptFlags::VERIFY_NULLDUMMY) && !vchSig.empty()) {
                            return SetError(ScriptError::SIG_NULLFAIL);
                        }
                        
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fSuccess ? vchTrue : vchFalse);
                        
                        if (opcode == OP_CHECKSIGVERIFY) {
                            if (fSuccess) {
                                popstack(stack);
                            } else {
                                return SetError(ScriptError::CHECKSIGVERIFY);
                            }
                        }
                        break;
                    }
                    
                    case OP_CHECKMULTISIG:
                    case OP_CHECKMULTISIGVERIFY: {
                        // ([sig ...] num_of_signatures [pubkey ...] num_of_pubkeys -- bool)
                        int i = 1;
                        if (static_cast<int>(stack.size()) < i) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        
                        int nKeysCount = ScriptNum(stacktop(-i), fRequireMinimal).GetInt();
                        if (nKeysCount < 0 || nKeysCount > MAX_PUBKEYS_PER_MULTISIG) {
                            return SetError(ScriptError::PUBKEY_COUNT);
                        }
                        nOpCount += nKeysCount;
                        if (nOpCount > MAX_OPS_PER_SCRIPT) {
                            return SetError(ScriptError::OP_COUNT);
                        }
                        i++;
                        int ikey = i;
                        i += nKeysCount;
                        
                        if (static_cast<int>(stack.size()) < i) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        
                        int nSigsCount = ScriptNum(stacktop(-i), fRequireMinimal).GetInt();
                        if (nSigsCount < 0 || nSigsCount > nKeysCount) {
                            return SetError(ScriptError::SIG_COUNT);
                        }
                        i++;
                        int isig = i;
                        i += nSigsCount;
                        
                        if (static_cast<int>(stack.size()) < i) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        
                        // Create subscript from codeseparator
                        Script scriptCode(pbegincodehash, pend);
                        
                        bool fSuccess = true;
                        while (fSuccess && nSigsCount > 0) {
                            std::vector<uint8_t>& vchSig = stacktop(-isig);
                            std::vector<uint8_t>& vchPubKey = stacktop(-ikey);
                            
                            // Check signature
                            bool fOk = checker.CheckSig(vchSig, vchPubKey, scriptCode);
                            
                            if (fOk) {
                                isig++;
                                nSigsCount--;
                            }
                            ikey++;
                            nKeysCount--;
                            
                            // If there are more signatures left than keys left,
                            // then too few keys
                            if (nSigsCount > nKeysCount) {
                                fSuccess = false;
                            }
                        }
                        
                        // Clean up stack of actual arguments
                        while (i-- > 1) {
                            // NULLDUMMY check: require dummy element be empty
                            if (!fSuccess && HasFlag(flags, ScriptFlags::VERIFY_NULLDUMMY) && 
                                static_cast<int>(stack.size()) >= -i && !stacktop(-1).empty()) {
                                return SetError(ScriptError::SIG_NULLFAIL);
                            }
                            if (i == static_cast<int>(stack.size())) {
                                // Check the dummy element
                                if (HasFlag(flags, ScriptFlags::VERIFY_NULLDUMMY) && !stacktop(-1).empty()) {
                                    return SetError(ScriptError::SIG_NULLDUMMY);
                                }
                            }
                            popstack(stack);
                        }
                        
                        // Dummy element is added because of a bug in original Bitcoin
                        if (stack.size() < 1) {
                            return SetError(ScriptError::INVALID_STACK_OPERATION);
                        }
                        if (HasFlag(flags, ScriptFlags::VERIFY_NULLDUMMY) && !stacktop(-1).empty()) {
                            return SetError(ScriptError::SIG_NULLDUMMY);
                        }
                        popstack(stack);
                        
                        stack.push_back(fSuccess ? vchTrue : vchFalse);
                        
                        if (opcode == OP_CHECKMULTISIGVERIFY) {
                            if (fSuccess) {
                                popstack(stack);
                            } else {
                                return SetError(ScriptError::CHECKMULTISIGVERIFY);
                            }
                        }
                        break;
                    }
                    
                    default:
                        return SetError(ScriptError::BAD_OPCODE);
                }
            }
            
            // Size limits
            if (stack.size() + altstack.size() > static_cast<size_t>(MAX_STACK_SIZE)) {
                return SetError(ScriptError::STACK_SIZE);
            }
        }
        
        if (!vfExec.empty()) {
            return SetError(ScriptError::UNBALANCED_CONDITIONAL);
        }
    } catch (const ScriptNumError&) {
        return SetError(ScriptError::INVALID_NUMBER_RANGE);
    } catch (...) {
        return SetError(ScriptError::UNKNOWN);
    }
    
    if (serror) *serror = ScriptError::OK;
    return true;
}

#undef stacktop
#undef altstacktop

// ============================================================================
// VerifyScript - Main entry point for script verification
// ============================================================================

bool VerifyScript(const Script& scriptSig,
                  const Script& scriptPubKey,
                  ScriptFlags flags,
                  const BaseSignatureChecker& checker,
                  ScriptError* serror) {
    
    auto SetError = [&](ScriptError err) {
        if (serror) *serror = err;
        return false;
    };
    
    // Verify scriptSig is push-only if flag is set
    if (HasFlag(flags, ScriptFlags::VERIFY_SIGPUSHONLY) && !scriptSig.IsPushOnly()) {
        return SetError(ScriptError::SIG_PUSHONLY);
    }
    
    // Evaluate scriptSig
    std::vector<std::vector<uint8_t>> stack;
    if (!EvalScript(stack, scriptSig, flags, checker, serror)) {
        return false;
    }
    
    // Save a copy of the stack for P2SH
    std::vector<std::vector<uint8_t>> stackCopy;
    if (HasFlag(flags, ScriptFlags::VERIFY_P2SH)) {
        stackCopy = stack;
    }
    
    // Evaluate scriptPubKey
    if (!EvalScript(stack, scriptPubKey, flags, checker, serror)) {
        return false;
    }
    
    // Stack must be non-empty
    if (stack.empty()) {
        return SetError(ScriptError::EVAL_FALSE);
    }
    
    // Top stack element must be true
    if (!CastToBool(stack.back())) {
        return SetError(ScriptError::EVAL_FALSE);
    }
    
    // P2SH evaluation
    if (HasFlag(flags, ScriptFlags::VERIFY_P2SH) && scriptPubKey.IsPayToScriptHash()) {
        // scriptSig must be push-only for P2SH
        if (!scriptSig.IsPushOnly()) {
            return SetError(ScriptError::SIG_PUSHONLY);
        }
        
        // stackCopy cannot be empty, as if it were, P2SH execution would have
        // failed earlier (during scriptPubKey evaluation)
        if (stackCopy.empty()) {
            return SetError(ScriptError::EVAL_FALSE);
        }
        
        // The serialized script is the top element of stackCopy
        const std::vector<uint8_t>& serializedScript = stackCopy.back();
        Script subscript(serializedScript.begin(), serializedScript.end());
        
        // Evaluate the P2SH subscript
        stack = stackCopy;
        stack.pop_back();  // Remove serialized script
        
        if (!EvalScript(stack, subscript, flags, checker, serror)) {
            return false;
        }
        
        if (stack.empty()) {
            return SetError(ScriptError::EVAL_FALSE);
        }
        
        if (!CastToBool(stack.back())) {
            return SetError(ScriptError::EVAL_FALSE);
        }
    }
    
    // Clean stack check - stack should have exactly one element
    // (Only when P2SH is not enabled or is already processed)
    // Note: Bitcoin has CLEANSTACK flag but we skip it for simplicity
    
    if (serror) *serror = ScriptError::OK;
    return true;
}

// ============================================================================
// CountSigOps
// ============================================================================

unsigned int CountSigOps(const Script& scriptSig,
                         const Script& scriptPubKey,
                         bool isP2SH) {
    unsigned int nSigOps = scriptPubKey.GetSigOpCount(true);
    
    if (isP2SH && scriptPubKey.IsPayToScriptHash()) {
        // For P2SH, we need to count sigops in the redeemScript
        // The redeemScript is the last push in scriptSig
        std::vector<std::vector<uint8_t>> stack;
        auto pc = scriptSig.begin();
        
        while (pc < scriptSig.end()) {
            Opcode opcode;
            std::vector<uint8_t> data;
            if (!scriptSig.GetOp(pc, opcode, data)) break;
            if (opcode > OP_16) break;  // Not push-only
            if (!data.empty()) {
                stack.push_back(data);
            } else if (opcode == OP_0) {
                stack.push_back(std::vector<uint8_t>());
            } else if (opcode >= OP_1 && opcode <= OP_16) {
                stack.push_back(std::vector<uint8_t>(1, opcode - OP_1 + 1));
            }
        }
        
        if (!stack.empty()) {
            Script redeemScript(stack.back().begin(), stack.back().end());
            nSigOps += redeemScript.GetSigOpCount(true);
        }
    }
    
    return nSigOps;
}

} // namespace shurium
