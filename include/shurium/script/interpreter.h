// SHURIUM - Script Interpreter
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file implements the script interpreter for validating transactions.
// Based on Bitcoin's script system with simplifications.

#ifndef SHURIUM_SCRIPT_INTERPRETER_H
#define SHURIUM_SCRIPT_INTERPRETER_H

#include "shurium/core/script.h"
#include "shurium/core/transaction.h"
#include "shurium/crypto/keys.h"
#include <cstdint>
#include <vector>
#include <string>

namespace shurium {

// ============================================================================
// Script Verification Flags
// ============================================================================

/// Flags that control script verification behavior
enum class ScriptFlags : uint32_t {
    VERIFY_NONE = 0,
    
    /// Evaluate P2SH subscripts (softfork)
    VERIFY_P2SH = (1U << 0),
    
    /// Enforce strict DER encoding for signatures
    VERIFY_STRICTENC = (1U << 1),
    
    /// Enforce minimal data pushes
    VERIFY_MINIMALDATA = (1U << 2),
    
    /// Discourage use of NOPs reserved for upgrades
    VERIFY_DISCOURAGE_UPGRADABLE_NOPS = (1U << 3),
    
    /// Verify OP_CHECKLOCKTIMEVERIFY
    VERIFY_CHECKLOCKTIMEVERIFY = (1U << 4),
    
    /// Verify OP_CHECKSEQUENCEVERIFY
    VERIFY_CHECKSEQUENCEVERIFY = (1U << 5),
    
    /// Using a non-push operator in the scriptSig causes script failure
    VERIFY_SIGPUSHONLY = (1U << 6),
    
    /// Require minimal encoding for signatures
    VERIFY_LOW_S = (1U << 7),
    
    /// Verify dummy stack element consumed by CHECKMULTISIG is of zero-length
    VERIFY_NULLDUMMY = (1U << 8),
    
    /// Public keys in scripts must be compressed
    VERIFY_COMPRESSED_PUBKEY = (1U << 9),
    
    /// Standard verification flags (used for mempool acceptance)
    STANDARD_VERIFY_FLAGS = VERIFY_P2SH | VERIFY_STRICTENC | VERIFY_MINIMALDATA |
                            VERIFY_DISCOURAGE_UPGRADABLE_NOPS | VERIFY_CHECKLOCKTIMEVERIFY |
                            VERIFY_CHECKSEQUENCEVERIFY | VERIFY_LOW_S | VERIFY_NULLDUMMY,
    
    /// Mandatory verification flags (used for block validation)
    MANDATORY_VERIFY_FLAGS = VERIFY_P2SH,
};

inline ScriptFlags operator|(ScriptFlags a, ScriptFlags b) {
    return static_cast<ScriptFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ScriptFlags operator&(ScriptFlags a, ScriptFlags b) {
    return static_cast<ScriptFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool HasFlag(ScriptFlags flags, ScriptFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// Signature Hash Types
// ============================================================================

/// Signature hash type flags
enum SigHashType : uint8_t {
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80,
};

// ============================================================================
// Script Error Codes
// ============================================================================

/// Error codes returned by script evaluation
enum class ScriptError {
    OK = 0,
    UNKNOWN,
    EVAL_FALSE,
    OP_RETURN,
    
    // Stack errors
    SCRIPT_SIZE,
    PUSH_SIZE,
    OP_COUNT,
    STACK_SIZE,
    SIG_COUNT,
    PUBKEY_COUNT,
    
    // Operand errors
    INVALID_OPERAND_SIZE,
    INVALID_NUMBER_RANGE,
    IMPOSSIBLE_ENCODING,
    INVALID_SPLIT_RANGE,
    
    // Verification errors
    VERIFY,
    EQUALVERIFY,
    CHECKMULTISIGVERIFY,
    CHECKSIGVERIFY,
    NUMEQUALVERIFY,
    
    // Control flow errors
    BAD_OPCODE,
    DISABLED_OPCODE,
    INVALID_STACK_OPERATION,
    INVALID_ALTSTACK_OPERATION,
    UNBALANCED_CONDITIONAL,
    
    // Signature errors
    SIG_HASHTYPE,
    SIG_DER,
    SIG_HIGH_S,
    SIG_NULLDUMMY,
    PUBKEYTYPE,
    CLEANSTACK,
    MINIMALDATA,
    MINIMALIF,
    SIG_NULLFAIL,
    
    // Timelock errors
    NEGATIVE_LOCKTIME,
    UNSATISFIED_LOCKTIME,
    
    // P2SH errors
    SIG_PUSHONLY,
    
    // Other
    DISCOURAGE_UPGRADABLE_NOPS,
    PUBKEY_RECOVERY_FAILED,
    
    ERROR_COUNT
};

/// Get human-readable error message
std::string ScriptErrorString(ScriptError err);

// ============================================================================
// Signature Checker - Abstract base for signature verification
// ============================================================================

/**
 * Abstract interface for signature verification.
 * 
 * This allows script verification to work without knowing the specific
 * transaction format or signature hash calculation method.
 */
class BaseSignatureChecker {
public:
    virtual ~BaseSignatureChecker() = default;
    
    /**
     * Verify an ECDSA signature.
     * @param signature DER-encoded signature (with sighash byte)
     * @param pubkey Public key
     * @param scriptCode Script being executed (for signature hash)
     * @return true if signature is valid
     */
    virtual bool CheckSig(const std::vector<uint8_t>& signature,
                          const std::vector<uint8_t>& pubkey,
                          const Script& scriptCode) const = 0;
    
    /**
     * Verify OP_CHECKLOCKTIMEVERIFY constraint.
     * @param nLockTime Lock time value from script
     * @return true if constraint is satisfied
     */
    virtual bool CheckLockTime(int64_t nLockTime) const = 0;
    
    /**
     * Verify OP_CHECKSEQUENCEVERIFY constraint.
     * @param nSequence Sequence value from script
     * @return true if constraint is satisfied
     */
    virtual bool CheckSequence(int64_t nSequence) const = 0;
};

/**
 * Dummy signature checker that always returns false.
 * Used for script validation without transaction context.
 */
class DummySignatureChecker : public BaseSignatureChecker {
public:
    bool CheckSig(const std::vector<uint8_t>& signature,
                  const std::vector<uint8_t>& pubkey,
                  const Script& scriptCode) const override { return false; }
    
    bool CheckLockTime(int64_t nLockTime) const override { return false; }
    bool CheckSequence(int64_t nSequence) const override { return false; }
};

/**
 * Transaction signature checker for verifying real transaction signatures.
 */
class TransactionSignatureChecker : public BaseSignatureChecker {
public:
    /**
     * Create a signature checker for a specific input.
     * @param tx Transaction being verified
     * @param nIn Input index being verified
     * @param amount Value of the input being spent
     */
    TransactionSignatureChecker(const Transaction* tx, unsigned int nIn, Amount amount);
    
    bool CheckSig(const std::vector<uint8_t>& signature,
                  const std::vector<uint8_t>& pubkey,
                  const Script& scriptCode) const override;
    
    bool CheckLockTime(int64_t nLockTime) const override;
    bool CheckSequence(int64_t nSequence) const override;

private:
    const Transaction* txTo_;
    unsigned int nIn_;
    Amount amount_;
    
    /// Compute signature hash for the input
    Hash256 ComputeSignatureHash(const Script& scriptCode, uint8_t nHashType) const;
};

// ============================================================================
// Script Interpreter Functions
// ============================================================================

/**
 * Evaluate a script.
 * 
 * @param stack Initial stack contents (modified in place)
 * @param script Script to execute
 * @param flags Verification flags
 * @param checker Signature checker for CHECKSIG operations
 * @param error Output: error code if failed
 * @return true if script executed successfully
 */
bool EvalScript(std::vector<std::vector<uint8_t>>& stack,
                const Script& script,
                ScriptFlags flags,
                const BaseSignatureChecker& checker,
                ScriptError* error = nullptr);

/**
 * Verify that a scriptSig + scriptPubKey pair is valid.
 * 
 * This is the main entry point for transaction validation.
 * It handles P2SH evaluation when enabled.
 * 
 * @param scriptSig Unlocking script (from transaction input)
 * @param scriptPubKey Locking script (from previous output)
 * @param flags Verification flags
 * @param checker Signature checker
 * @param error Output: error code if failed
 * @return true if scripts verify successfully
 */
bool VerifyScript(const Script& scriptSig,
                  const Script& scriptPubKey,
                  ScriptFlags flags,
                  const BaseSignatureChecker& checker,
                  ScriptError* error = nullptr);

// ============================================================================
// Signature Hash Calculation
// ============================================================================

/**
 * Calculate the signature hash for a transaction input.
 * 
 * @param tx Transaction containing the input
 * @param nIn Input index
 * @param scriptCode Script being signed
 * @param nHashType Signature hash type
 * @return 256-bit signature hash
 */
Hash256 SignatureHash(const Transaction& tx,
                      unsigned int nIn,
                      const Script& scriptCode,
                      uint8_t nHashType);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Check if a signature is valid DER encoding.
 */
bool IsValidDERSignature(const std::vector<uint8_t>& sig);

/**
 * Check if a signature has low S value (for malleability protection).
 */
bool IsLowDERSignature(const std::vector<uint8_t>& sig, ScriptError* error = nullptr);

/**
 * Check if a public key is valid.
 */
bool IsValidPubKey(const std::vector<uint8_t>& pubkey);

/**
 * Check if a public key is compressed.
 */
bool IsCompressedPubKey(const std::vector<uint8_t>& pubkey);

/**
 * Cast stack value to bool.
 */
bool CastToBool(const std::vector<uint8_t>& vch);

/**
 * Count signature operations in a scriptSig + scriptPubKey pair.
 * Accounts for P2SH if applicable.
 */
unsigned int CountSigOps(const Script& scriptSig,
                         const Script& scriptPubKey,
                         bool isP2SH);

} // namespace shurium

#endif // SHURIUM_SCRIPT_INTERPRETER_H
