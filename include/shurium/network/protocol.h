// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#pragma once

#include <shurium/core/serialize.h>
#include <shurium/core/types.h>
#include <shurium/core/block.h>
#include <shurium/network/address.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace shurium {

// ============================================================================
// Protocol Constants
// ============================================================================

/// SHURIUM protocol version
constexpr int32_t PROTOCOL_VERSION = 70001;

/// Minimum supported protocol version
constexpr int32_t MIN_PEER_PROTO_VERSION = 70000;

/// Initial protocol version (before handshake)
constexpr int32_t INIT_PROTO_VERSION = 209;

/// Maximum protocol message size (4 MB)
constexpr size_t MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000;

/// Maximum length of user agent string
constexpr size_t MAX_SUBVERSION_LENGTH = 256;

/// Maximum number of items in an inv message
constexpr size_t MAX_INV_SZ = 50000;

/// Maximum headers per message
constexpr size_t MAX_HEADERS_RESULTS = 2000;

/// Maximum addresses in an addr message
constexpr size_t MAX_ADDR_TO_SEND = 1000;

/// Message type field size (null-padded)
constexpr size_t MESSAGE_TYPE_SIZE = 12;

/// Message header size
constexpr size_t MESSAGE_HEADER_SIZE = 4 + 12 + 4 + 4;  // magic + type + size + checksum

// Network magic bytes (identify which network a message is for)
namespace NetworkMagic {
    /// Mainnet magic bytes
    constexpr std::array<uint8_t, 4> MAINNET = {0x4E, 0x58, 0x55, 0x53};  // "NXUS"
    
    /// Testnet magic bytes  
    constexpr std::array<uint8_t, 4> TESTNET = {0x54, 0x4E, 0x58, 0x53};  // "TNXS"
    
    /// Regtest magic bytes
    constexpr std::array<uint8_t, 4> REGTEST = {0x52, 0x4E, 0x58, 0x53};  // "RNXS"
}

// Default ports
namespace DefaultPort {
    constexpr uint16_t MAINNET = 8433;
    constexpr uint16_t TESTNET = 18433;
    constexpr uint16_t REGTEST = 18444;
}

// ============================================================================
// Message Types
// ============================================================================

namespace NetMsgType {
    // Connection handshake
    constexpr const char* VERSION = "version";
    constexpr const char* VERACK = "verack";
    constexpr const char* SENDHEADERS = "sendheaders";
    
    // Address relay
    constexpr const char* ADDR = "addr";
    constexpr const char* ADDRV2 = "addrv2";
    constexpr const char* GETADDR = "getaddr";
    constexpr const char* SENDADDRV2 = "sendaddrv2";
    
    // Inventory/data
    constexpr const char* INV = "inv";
    constexpr const char* GETDATA = "getdata";
    constexpr const char* NOTFOUND = "notfound";
    
    // Blocks
    constexpr const char* BLOCK = "block";
    constexpr const char* GETBLOCKS = "getblocks";
    constexpr const char* GETHEADERS = "getheaders";
    constexpr const char* HEADERS = "headers";
    
    // Transactions
    constexpr const char* TX = "tx";
    constexpr const char* MEMPOOL = "mempool";
    constexpr const char* FEEFILTER = "feefilter";
    
    // Keepalive
    constexpr const char* PING = "ping";
    constexpr const char* PONG = "pong";
    
    // Rejection (deprecated but useful for debugging)
    constexpr const char* REJECT = "reject";
    
    // SHURIUM-specific
    constexpr const char* POUWSOL = "pouwsol";       // Proof of Useful Work solution
    constexpr const char* GETPOUW = "getpouw";       // Request PoUW problem
    constexpr const char* POUWPROB = "pouwprob";     // PoUW problem distribution
    constexpr const char* UBICLAIM = "ubiclaim";     // UBI claim submission
    constexpr const char* IDENTITY = "identity";     // Identity proof
}

// ============================================================================
// Inventory Types
// ============================================================================

/// Types of inventory items
enum class InvType : uint32_t {
    ERROR = 0,
    MSG_TX = 1,           ///< Transaction
    MSG_BLOCK = 2,        ///< Block
    MSG_FILTERED_BLOCK = 3, ///< Merkle block (for SPV)
    
    // SHURIUM-specific
    MSG_POUW_SOLUTION = 16,   ///< PoUW solution
    MSG_POUW_PROBLEM = 17,    ///< PoUW problem
    MSG_UBI_CLAIM = 18,       ///< UBI claim
    MSG_IDENTITY_PROOF = 19,  ///< Identity proof
};

/**
 * Inventory item - identifies data by type and hash.
 * 
 * Used in inv, getdata, and notfound messages.
 */
class Inv {
public:
    InvType type{InvType::ERROR};
    Hash256 hash;
    
    Inv() = default;
    Inv(InvType t, const Hash256& h) : type(t), hash(h) {}
    
    bool IsTransaction() const { return type == InvType::MSG_TX; }
    bool IsBlock() const { return type == InvType::MSG_BLOCK; }
    
    std::string ToString() const;
    
    bool operator==(const Inv& other) const {
        return type == other.type && hash == other.hash;
    }
    bool operator<(const Inv& other) const {
        if (type != other.type) return type < other.type;
        return hash < other.hash;
    }
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, static_cast<uint32_t>(type));
        ::shurium::Serialize(s, hash);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        uint32_t t;
        ::shurium::Unserialize(s, t);
        type = static_cast<InvType>(t);
        ::shurium::Unserialize(s, hash);
    }
};

/// Free function serializer for Inv (needed for std::vector<Inv> serialization)
template<typename Stream>
void Serialize(Stream& s, const Inv& inv) {
    inv.Serialize(s);
}

template<typename Stream>
void Unserialize(Stream& s, Inv& inv) {
    inv.Unserialize(s);
}

// ============================================================================
// Message Header
// ============================================================================

/**
 * Network message header.
 * 
 * Format:
 *   4 bytes  - Magic bytes (network identifier)
 *   12 bytes - Command name (null-padded)
 *   4 bytes  - Payload size
 *   4 bytes  - Checksum (first 4 bytes of double SHA256 of payload)
 */
class MessageHeader {
public:
    std::array<uint8_t, 4> magic;
    std::array<char, MESSAGE_TYPE_SIZE> command;
    uint32_t payloadSize{0};
    std::array<uint8_t, 4> checksum;
    
    MessageHeader() {
        magic.fill(0);
        command.fill(0);
        checksum.fill(0);
    }
    
    MessageHeader(const std::array<uint8_t, 4>& net, const std::string& cmd, 
                  uint32_t size, const std::array<uint8_t, 4>& chksum)
        : magic(net), payloadSize(size), checksum(chksum) {
        SetCommand(cmd);
    }
    
    /// Set command name (truncated/null-padded to 12 bytes)
    void SetCommand(const std::string& cmd) {
        command.fill(0);
        size_t len = std::min(cmd.size(), MESSAGE_TYPE_SIZE);
        std::copy(cmd.begin(), cmd.begin() + len, command.begin());
    }
    
    /// Get command name as string (up to null terminator)
    std::string GetCommand() const {
        return std::string(command.data(), strnlen(command.data(), MESSAGE_TYPE_SIZE));
    }
    
    /// Check if header is valid (size limits)
    bool IsValid() const {
        return payloadSize <= MAX_PROTOCOL_MESSAGE_LENGTH;
    }
    
    /// Check if magic matches expected network
    bool IsValidMagic(const std::array<uint8_t, 4>& expected) const {
        return magic == expected;
    }
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        s.Write(reinterpret_cast<const char*>(magic.data()), magic.size());
        s.Write(command.data(), command.size());
        ::shurium::Serialize(s, payloadSize);
        s.Write(reinterpret_cast<const char*>(checksum.data()), checksum.size());
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        s.Read(reinterpret_cast<char*>(magic.data()), magic.size());
        s.Read(command.data(), command.size());
        ::shurium::Unserialize(s, payloadSize);
        s.Read(reinterpret_cast<char*>(checksum.data()), checksum.size());
    }
};

// ============================================================================
// Version Message
// ============================================================================

/**
 * Version message payload.
 * 
 * Sent as the first message in a connection. Contains protocol version,
 * service flags, timestamp, addresses, and other connection info.
 */
class VersionMessage {
public:
    int32_t version{PROTOCOL_VERSION};
    ServiceFlags services{ServiceFlags::NONE};
    int64_t timestamp{0};
    NetService addrRecv;  // Address of receiving node
    NetService addrFrom;  // Address of sending node
    uint64_t nonce{0};    // Random nonce for self-connection detection
    std::string userAgent;
    int32_t startHeight{0};
    bool relay{true};     // Whether to relay transactions (BIP37)
    
    VersionMessage() = default;
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, version);
        ::shurium::Serialize(s, static_cast<uint64_t>(services));
        ::shurium::Serialize(s, timestamp);
        // Receiver address (simplified - no services prefix for now)
        addrRecv.Serialize(s);
        // Sender address
        addrFrom.Serialize(s);
        ::shurium::Serialize(s, nonce);
        ::shurium::Serialize(s, userAgent);
        ::shurium::Serialize(s, startHeight);
        ::shurium::Serialize(s, relay);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, version);
        uint64_t svc;
        ::shurium::Unserialize(s, svc);
        services = static_cast<ServiceFlags>(svc);
        ::shurium::Unserialize(s, timestamp);
        addrRecv.Unserialize(s);
        addrFrom.Unserialize(s);
        ::shurium::Unserialize(s, nonce);
        ::shurium::Unserialize(s, userAgent);
        ::shurium::Unserialize(s, startHeight);
        ::shurium::Unserialize(s, relay);
    }
};

// ============================================================================
// Headers Message
// ============================================================================

// Forward declaration
class BlockHeader;

/**
 * Headers message - a sequence of block headers.
 */
class HeadersMessage {
public:
    std::vector<BlockHeader> headers;
    
    template<typename Stream>
    void Serialize(Stream& s) const;
    
    template<typename Stream>
    void Unserialize(Stream& s);
};

// ============================================================================
// Ping/Pong Messages
// ============================================================================

/**
 * Ping message with nonce.
 */
class PingMessage {
public:
    uint64_t nonce{0};
    
    PingMessage() = default;
    explicit PingMessage(uint64_t n) : nonce(n) {}
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, nonce);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, nonce);
    }
};

using PongMessage = PingMessage;

// ============================================================================
// Fee Filter Message
// ============================================================================

/**
 * Fee filter message - minimum fee rate for relaying transactions.
 */
class FeeFilterMessage {
public:
    int64_t minFeeRate{0};  // Fee rate in satoshis per kilobyte
    
    FeeFilterMessage() = default;
    explicit FeeFilterMessage(int64_t rate) : minFeeRate(rate) {}
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, minFeeRate);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, minFeeRate);
    }
};

// ============================================================================
// Reject Message (deprecated but useful for debugging)
// ============================================================================

/// Rejection codes
enum class RejectCode : uint8_t {
    MALFORMED = 0x01,
    INVALID = 0x10,
    OBSOLETE = 0x11,
    DUPLICATE = 0x12,
    NONSTANDARD = 0x40,
    DUST = 0x41,
    INSUFFICIENT_FEE = 0x42,
    CHECKPOINT = 0x43,
};

/**
 * Reject message - reports why a message was rejected.
 */
class RejectMessage {
public:
    std::string message;    // Type of message rejected
    RejectCode code{RejectCode::INVALID};
    std::string reason;     // Human-readable reason
    Hash256 data;           // Optional hash of rejected item
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, message);
        ::shurium::Serialize(s, static_cast<uint8_t>(code));
        ::shurium::Serialize(s, reason);
        if (message == NetMsgType::TX || message == NetMsgType::BLOCK) {
            ::shurium::Serialize(s, data);
        }
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, message);
        uint8_t c;
        ::shurium::Unserialize(s, c);
        code = static_cast<RejectCode>(c);
        ::shurium::Unserialize(s, reason);
        if (message == NetMsgType::TX || message == NetMsgType::BLOCK) {
            ::shurium::Unserialize(s, data);
        }
    }
};

// ============================================================================
// GetBlocks/GetHeaders Messages
// ============================================================================

/**
 * GetBlocks message - request block inventory.
 */
class GetBlocksMessage {
public:
    uint32_t version{PROTOCOL_VERSION};
    BlockLocator locator;
    Hash256 hashStop;  // Stop at this hash (zero = no limit)
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, version);
        ::shurium::Serialize(s, locator);
        ::shurium::Serialize(s, hashStop);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, version);
        ::shurium::Unserialize(s, locator);
        ::shurium::Unserialize(s, hashStop);
    }
};

using GetHeadersMessage = GetBlocksMessage;

// ============================================================================
// Addr Message
// ============================================================================

/**
 * Addr message - relay peer addresses.
 */
class AddrMessage {
public:
    std::vector<PeerAddress> addresses;
    
    template<typename Stream>
    void Serialize(Stream& s) const {
        ::shurium::Serialize(s, addresses);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        ::shurium::Unserialize(s, addresses);
    }
};

/// Free function serializer for PeerAddress (needed for std::vector<PeerAddress> serialization)
template<typename Stream>
void Serialize(Stream& s, const PeerAddress& addr) {
    addr.Serialize(s);
}

template<typename Stream>
void Unserialize(Stream& s, PeerAddress& addr) {
    addr.Unserialize(s);
}

// ============================================================================
// Utility Functions
// ============================================================================

/// Compute checksum for message payload (first 4 bytes of double SHA256)
std::array<uint8_t, 4> ComputeChecksum(const std::vector<uint8_t>& payload);

/// Create a complete message with header
std::vector<uint8_t> CreateMessage(const std::array<uint8_t, 4>& magic,
                                    const std::string& command,
                                    const std::vector<uint8_t>& payload);

/// Parse message header from bytes
std::optional<MessageHeader> ParseMessageHeader(const std::vector<uint8_t>& data);

/// Verify checksum of message payload
bool VerifyChecksum(const std::vector<uint8_t>& payload, 
                    const std::array<uint8_t, 4>& checksum);

/// Get string representation of inventory type
std::string InvTypeToString(InvType type);

// ============================================================================
// Message Validation
// ============================================================================

/**
 * Message validation result.
 */
struct MessageValidationResult {
    bool valid{false};
    int misbehaviorScore{0};
    std::string reason;
    
    static MessageValidationResult Valid() {
        return {true, 0, ""};
    }
    
    static MessageValidationResult Invalid(int score, const std::string& msg) {
        return {false, score, msg};
    }
};

/**
 * Validate a network message command name.
 * Checks for valid characters and known commands.
 * 
 * @param command The command name to validate
 * @return Validation result
 */
MessageValidationResult ValidateCommand(const std::string& command);

/**
 * Validate message payload size for a specific command.
 * Different message types have different size limits.
 * 
 * @param command The message command
 * @param payloadSize Size of the payload in bytes
 * @return Validation result
 */
MessageValidationResult ValidatePayloadSize(const std::string& command, size_t payloadSize);

/**
 * Validate a version message.
 * 
 * @param version The version message to validate
 * @return Validation result
 */
MessageValidationResult ValidateVersionMessage(const VersionMessage& version);

/**
 * Validate inventory type is known.
 * 
 * @param type The inventory type to check
 * @return true if valid, false if unknown
 */
bool IsValidInvType(InvType type);

/**
 * Check if a timestamp is reasonable (not too far in the past or future).
 * Used to validate version messages and address timestamps.
 * 
 * @param timestamp Unix timestamp to validate
 * @param maxAgeSec Maximum seconds in the past (default: 1 week)
 * @param maxFutureSec Maximum seconds in the future (default: 2 hours)
 * @return true if timestamp is within acceptable range
 */
bool IsReasonableTimestamp(int64_t timestamp, int64_t maxAgeSec = 604800, int64_t maxFutureSec = 7200);

/**
 * Sanitize a user agent string.
 * Removes non-printable characters and truncates to max length.
 * 
 * @param userAgent The user agent string to sanitize
 * @return Sanitized string
 */
std::string SanitizeUserAgent(const std::string& userAgent);

} // namespace shurium
