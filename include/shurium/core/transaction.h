// SHURIUM - Transaction Header
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// This file defines the transaction primitives for SHURIUM.
// Based on Bitcoin's transaction model with simplifications.

#ifndef SHURIUM_CORE_TRANSACTION_H
#define SHURIUM_CORE_TRANSACTION_H

#include "shurium/core/types.h"
#include "shurium/core/script.h"
#include "shurium/core/serialize.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <limits>

namespace shurium {

// ============================================================================
// OutPoint - Reference to a previous transaction output
// ============================================================================

/// An outpoint - a combination of a transaction hash and an index n into its vout
class OutPoint {
public:
    TxHash hash;
    uint32_t n;

    /// Index value representing a null/invalid outpoint
    static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

    /// Default constructor - creates a null outpoint
    OutPoint() : hash(), n(NULL_INDEX) {}

    /// Construct with specific hash and index
    OutPoint(const TxHash& hashIn, uint32_t nIn) : hash(hashIn), n(nIn) {}

    /// Set to null state
    void SetNull() {
        hash.SetNull();
        n = NULL_INDEX;
    }

    /// Check if this is a null outpoint
    bool IsNull() const {
        return hash.IsNull() && n == NULL_INDEX;
    }

    /// Comparison operators
    friend bool operator<(const OutPoint& a, const OutPoint& b) {
        if (a.hash != b.hash) return a.hash < b.hash;
        return a.n < b.n;
    }

    friend bool operator==(const OutPoint& a, const OutPoint& b) {
        return a.hash == b.hash && a.n == b.n;
    }

    friend bool operator!=(const OutPoint& a, const OutPoint& b) {
        return !(a == b);
    }

    /// Convert to human-readable string
    std::string ToString() const;
};

/// Serialization for OutPoint
template<typename Stream>
void Serialize(Stream& s, const OutPoint& outpoint) {
    Serialize(s, outpoint.hash);
    Serialize(s, outpoint.n);
}

template<typename Stream>
void Unserialize(Stream& s, OutPoint& outpoint) {
    Unserialize(s, outpoint.hash);
    Unserialize(s, outpoint.n);
}

// ============================================================================
// TxIn - Transaction Input
// ============================================================================

/// An input of a transaction. Contains the location of a previous
/// transaction's output that it claims and a signature that matches the
/// output's public key.
class TxIn {
public:
    OutPoint prevout;
    Script scriptSig;
    uint32_t nSequence;

    /// Setting nSequence to this value for every input in a transaction
    /// disables nLockTime/IsFinalTx().
    static const uint32_t SEQUENCE_FINAL = 0xFFFFFFFF;

    /// Maximum sequence number that enables both nLockTime and OP_CHECKLOCKTIMEVERIFY
    static const uint32_t MAX_SEQUENCE_NONFINAL = SEQUENCE_FINAL - 1;

    /// If this flag is set, nSequence is NOT interpreted as a relative lock-time
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1U << 31);

    /// If nSequence encodes a relative lock-time and this flag is set,
    /// the relative lock-time has units of 512 seconds, otherwise blocks
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1U << 22);

    /// Mask to extract the lock-time from the sequence field
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000FFFF;

    /// Default constructor
    TxIn() : nSequence(SEQUENCE_FINAL) {}

    /// Construct with outpoint only
    explicit TxIn(const OutPoint& prevoutIn, 
                  Script scriptSigIn = Script(), 
                  uint32_t nSequenceIn = SEQUENCE_FINAL)
        : prevout(prevoutIn), scriptSig(std::move(scriptSigIn)), nSequence(nSequenceIn) {}

    /// Construct with separate hash and index
    TxIn(const TxHash& hashPrevTx, uint32_t nOut, 
         Script scriptSigIn = Script(), 
         uint32_t nSequenceIn = SEQUENCE_FINAL)
        : prevout(hashPrevTx, nOut), scriptSig(std::move(scriptSigIn)), nSequence(nSequenceIn) {}

    /// Equality operator
    friend bool operator==(const TxIn& a, const TxIn& b) {
        return a.prevout == b.prevout &&
               a.scriptSig == b.scriptSig &&
               a.nSequence == b.nSequence;
    }

    friend bool operator!=(const TxIn& a, const TxIn& b) {
        return !(a == b);
    }

    /// Convert to human-readable string
    std::string ToString() const;
};

/// Serialization for TxIn
template<typename Stream>
void Serialize(Stream& s, const TxIn& txin) {
    Serialize(s, txin.prevout);
    Serialize(s, txin.scriptSig);
    Serialize(s, txin.nSequence);
}

template<typename Stream>
void Unserialize(Stream& s, TxIn& txin) {
    Unserialize(s, txin.prevout);
    Unserialize(s, txin.scriptSig);
    Unserialize(s, txin.nSequence);
}

// ============================================================================
// TxOut - Transaction Output
// ============================================================================

/// An output of a transaction. Contains the public key that the next input
/// must be able to sign with to claim it.
class TxOut {
public:
    Amount nValue;
    Script scriptPubKey;

    /// Default constructor - creates a null output
    TxOut() {
        SetNull();
    }

    /// Construct with value and script
    TxOut(Amount nValueIn, Script scriptPubKeyIn)
        : nValue(nValueIn), scriptPubKey(std::move(scriptPubKeyIn)) {}

    /// Set to null state
    void SetNull() {
        nValue = -1;
        scriptPubKey.clear();
    }

    /// Check if this is a null output
    bool IsNull() const {
        return nValue == -1;
    }

    /// Equality operator
    friend bool operator==(const TxOut& a, const TxOut& b) {
        return a.nValue == b.nValue && a.scriptPubKey == b.scriptPubKey;
    }

    friend bool operator!=(const TxOut& a, const TxOut& b) {
        return !(a == b);
    }

    /// Convert to human-readable string
    std::string ToString() const;
};

/// Serialization for TxOut
template<typename Stream>
void Serialize(Stream& s, const TxOut& txout) {
    Serialize(s, static_cast<int64_t>(txout.nValue));
    Serialize(s, txout.scriptPubKey);
}

template<typename Stream>
void Unserialize(Stream& s, TxOut& txout) {
    int64_t value;
    Unserialize(s, value);
    txout.nValue = value;
    Unserialize(s, txout.scriptPubKey);
}

// ============================================================================
// Forward Declarations
// ============================================================================

class Transaction;
class MutableTransaction;

// ============================================================================
// MutableTransaction - Mutable version of Transaction
// ============================================================================

/// A mutable version of a transaction that can be modified
class MutableTransaction {
public:
    std::vector<TxIn> vin;
    std::vector<TxOut> vout;
    uint32_t version;
    uint32_t nLockTime;

    /// Default transaction version
    static const uint32_t CURRENT_VERSION = 2;

    /// Default constructor
    MutableTransaction();

    /// Construct from an immutable Transaction
    explicit MutableTransaction(const Transaction& tx);

    /// Check if this transaction has no inputs and outputs
    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    /// Compute the hash of this transaction
    TxHash GetHash() const;

    /// Get the total output value
    Amount GetValueOut() const;

    /// Get the total serialized size
    size_t GetTotalSize() const;
};

/// Serialization for MutableTransaction
template<typename Stream>
void Serialize(Stream& s, const MutableTransaction& tx) {
    Serialize(s, tx.version);
    Serialize(s, tx.vin);
    Serialize(s, tx.vout);
    Serialize(s, tx.nLockTime);
}

template<typename Stream>
void Unserialize(Stream& s, MutableTransaction& tx) {
    Unserialize(s, tx.version);
    Unserialize(s, tx.vin);
    Unserialize(s, tx.vout);
    Unserialize(s, tx.nLockTime);
}

// ============================================================================
// Transaction - Immutable Transaction
// ============================================================================

/// The basic transaction that is broadcasted on the network and contained in
/// blocks. A transaction can contain multiple inputs and outputs.
/// 
/// This is an immutable class - once constructed, it cannot be modified.
/// Use MutableTransaction for building transactions.
class Transaction {
public:
    /// Default transaction version
    static const uint32_t CURRENT_VERSION = 2;

    // Transaction data (const to prevent modification)
    const std::vector<TxIn> vin;
    const std::vector<TxOut> vout;
    const uint32_t version;
    const uint32_t nLockTime;

private:
    /// Cached transaction hash (computed once)
    const TxHash hash;

    /// Compute the transaction hash
    TxHash ComputeHash() const;

public:
    /// Construct from a MutableTransaction
    explicit Transaction(const MutableTransaction& tx);
    explicit Transaction(MutableTransaction&& tx);

    /// Check if transaction has no inputs and outputs
    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    /// Get the cached transaction hash
    const TxHash& GetHash() const { return hash; }

    /// Get the total output value
    Amount GetValueOut() const;

    /// Get the total serialized size
    size_t GetTotalSize() const;

    /// Check if this is a coinbase transaction
    /// A coinbase has exactly one input with a null prevout
    bool IsCoinBase() const {
        return vin.size() == 1 && vin[0].prevout.IsNull();
    }

    /// Equality operator (compares hashes)
    friend bool operator==(const Transaction& a, const Transaction& b) {
        return a.GetHash() == b.GetHash();
    }

    friend bool operator!=(const Transaction& a, const Transaction& b) {
        return !(a == b);
    }

    /// Convert to human-readable string
    std::string ToString() const;
};

/// Serialization for Transaction (read-only, uses the stored values)
template<typename Stream>
void Serialize(Stream& s, const Transaction& tx) {
    Serialize(s, tx.version);
    Serialize(s, tx.vin);
    Serialize(s, tx.vout);
    Serialize(s, tx.nLockTime);
}

// Note: Unserialize for Transaction is not provided because Transaction is immutable.
// Use MutableTransaction for deserialization, then convert to Transaction.

// ============================================================================
// Shared Transaction Reference
// ============================================================================

/// Shared pointer to an immutable transaction
using TransactionRef = std::shared_ptr<const Transaction>;

/// Helper to create a TransactionRef
template<typename Tx>
inline TransactionRef MakeTransactionRef(Tx&& txIn) {
    return std::make_shared<const Transaction>(std::forward<Tx>(txIn));
}

} // namespace shurium

#endif // SHURIUM_CORE_TRANSACTION_H
