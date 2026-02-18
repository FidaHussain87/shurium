// SHURIUM - Transaction Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/core/transaction.h"
#include "shurium/crypto/sha256.h"
#include <sstream>
#include <iomanip>
#include <numeric>

namespace shurium {

// ============================================================================
// Static Member Definitions
// ============================================================================

const uint32_t TxIn::SEQUENCE_FINAL;
const uint32_t TxIn::MAX_SEQUENCE_NONFINAL;
const uint32_t TxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG;
const uint32_t TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG;
const uint32_t TxIn::SEQUENCE_LOCKTIME_MASK;

const uint32_t MutableTransaction::CURRENT_VERSION;
const uint32_t Transaction::CURRENT_VERSION;

// ============================================================================
// OutPoint Implementation
// ============================================================================

std::string OutPoint::ToString() const {
    std::ostringstream ss;
    ss << "OutPoint(" << hash.ToHex().substr(0, 16) << "..., " << n << ")";
    return ss.str();
}

// ============================================================================
// TxIn Implementation
// ============================================================================

std::string TxIn::ToString() const {
    std::ostringstream ss;
    ss << "TxIn(";
    if (prevout.IsNull()) {
        ss << "coinbase";
    } else {
        ss << prevout.ToString();
    }
    ss << ", scriptSig=" << scriptSig.size() << " bytes";
    if (nSequence != SEQUENCE_FINAL) {
        ss << ", nSequence=" << nSequence;
    }
    ss << ")";
    return ss.str();
}

// ============================================================================
// TxOut Implementation
// ============================================================================

std::string TxOut::ToString() const {
    std::ostringstream ss;
    ss << "TxOut(";
    if (IsNull()) {
        ss << "null";
    } else {
        // Convert to NXS for display
        int64_t wholePart = nValue / COIN;
        int64_t fractionalPart = nValue % COIN;
        ss << wholePart;
        if (fractionalPart > 0) {
            ss << "." << std::setfill('0') << std::setw(8) << fractionalPart;
        }
        ss << " NXS";
    }
    ss << ", scriptPubKey=" << scriptPubKey.size() << " bytes)";
    return ss.str();
}

// ============================================================================
// MutableTransaction Implementation
// ============================================================================

MutableTransaction::MutableTransaction()
    : version(CURRENT_VERSION), nLockTime(0) {}

MutableTransaction::MutableTransaction(const Transaction& tx)
    : vin(tx.vin), vout(tx.vout), version(tx.version), nLockTime(tx.nLockTime) {}

TxHash MutableTransaction::GetHash() const {
    // Serialize the transaction
    DataStream ss;
    Serialize(ss, *this);
    
    // Double SHA256 (like Bitcoin)
    Hash256 hash = DoubleSHA256(ss.data(), ss.TotalSize());
    
    return TxHash(hash);
}

Amount MutableTransaction::GetValueOut() const {
    Amount total = 0;
    for (const auto& out : vout) {
        if (!MoneyRange(out.nValue)) {
            throw std::runtime_error("TxOut value out of range");
        }
        total += out.nValue;
        if (!MoneyRange(total)) {
            throw std::runtime_error("Total TxOut value out of range");
        }
    }
    return total;
}

size_t MutableTransaction::GetTotalSize() const {
    return GetSerializeSize(*this);
}

// ============================================================================
// Transaction Implementation
// ============================================================================

Transaction::Transaction(const MutableTransaction& tx)
    : vin(tx.vin),
      vout(tx.vout),
      version(tx.version),
      nLockTime(tx.nLockTime),
      hash(ComputeHash()) {}

Transaction::Transaction(MutableTransaction&& tx)
    : vin(std::move(tx.vin)),
      vout(std::move(tx.vout)),
      version(tx.version),
      nLockTime(tx.nLockTime),
      hash(ComputeHash()) {}

TxHash Transaction::ComputeHash() const {
    // Serialize the transaction
    DataStream ss;
    Serialize(ss, *this);
    
    // Double SHA256 (like Bitcoin)
    Hash256 hash = DoubleSHA256(ss.data(), ss.TotalSize());
    
    return TxHash(hash);
}

Amount Transaction::GetValueOut() const {
    Amount total = 0;
    for (const auto& out : vout) {
        if (!MoneyRange(out.nValue)) {
            throw std::runtime_error("TxOut value out of range");
        }
        total += out.nValue;
        if (!MoneyRange(total)) {
            throw std::runtime_error("Total TxOut value out of range");
        }
    }
    return total;
}

size_t Transaction::GetTotalSize() const {
    return GetSerializeSize(*this);
}

std::string Transaction::ToString() const {
    std::ostringstream ss;
    ss << "Transaction(\n";
    ss << "  hash=" << hash.ToHex().substr(0, 16) << "...\n";
    ss << "  version=" << version << "\n";
    ss << "  vin.size=" << vin.size() << "\n";
    for (size_t i = 0; i < vin.size(); ++i) {
        ss << "    [" << i << "] " << vin[i].ToString() << "\n";
    }
    ss << "  vout.size=" << vout.size() << "\n";
    for (size_t i = 0; i < vout.size(); ++i) {
        ss << "    [" << i << "] " << vout[i].ToString() << "\n";
    }
    ss << "  nLockTime=" << nLockTime << "\n";
    ss << ")";
    return ss.str();
}

} // namespace shurium
