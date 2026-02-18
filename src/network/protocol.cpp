// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#include <shurium/network/protocol.h>
#include <shurium/crypto/sha256.h>
#include <shurium/util/time.h>

#include <cctype>
#include <cstring>
#include <set>
#include <sstream>
#include <iomanip>

namespace shurium {

// ============================================================================
// Inv Implementation
// ============================================================================

std::string Inv::ToString() const {
    std::ostringstream ss;
    ss << InvTypeToString(type) << " ";
    // Print first 16 chars of hash
    for (size_t i = 0; i < 8 && i < hash.size(); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    ss << "...";
    return ss.str();
}

// ============================================================================
// Utility Functions
// ============================================================================

std::array<uint8_t, 4> ComputeChecksum(const std::vector<uint8_t>& payload) {
    // Double SHA256
    Hash256 hash = DoubleSHA256(payload.data(), payload.size());
    
    std::array<uint8_t, 4> checksum;
    std::copy(hash.begin(), hash.begin() + 4, checksum.begin());
    return checksum;
}

std::vector<uint8_t> CreateMessage(const std::array<uint8_t, 4>& magic,
                                    const std::string& command,
                                    const std::vector<uint8_t>& payload) {
    // Build header
    MessageHeader header;
    header.magic = magic;
    header.SetCommand(command);
    header.payloadSize = static_cast<uint32_t>(payload.size());
    header.checksum = ComputeChecksum(payload);
    
    // Serialize header
    DataStream headerStream;
    header.Serialize(headerStream);
    
    // Combine header and payload
    std::vector<uint8_t> result;
    result.reserve(MESSAGE_HEADER_SIZE + payload.size());
    result.insert(result.end(), headerStream.begin(), headerStream.end());
    result.insert(result.end(), payload.begin(), payload.end());
    
    return result;
}

std::optional<MessageHeader> ParseMessageHeader(const std::vector<uint8_t>& data) {
    if (data.size() < MESSAGE_HEADER_SIZE) {
        return std::nullopt;
    }
    
    DataStream stream(data.data(), MESSAGE_HEADER_SIZE);
    MessageHeader header;
    try {
        header.Unserialize(stream);
    } catch (...) {
        return std::nullopt;
    }
    
    if (!header.IsValid()) {
        return std::nullopt;
    }
    
    return header;
}

bool VerifyChecksum(const std::vector<uint8_t>& payload,
                    const std::array<uint8_t, 4>& checksum) {
    auto computed = ComputeChecksum(payload);
    return computed == checksum;
}

std::string InvTypeToString(InvType type) {
    switch (type) {
        case InvType::ERROR:
            return "ERROR";
        case InvType::MSG_TX:
            return "TX";
        case InvType::MSG_BLOCK:
            return "BLOCK";
        case InvType::MSG_FILTERED_BLOCK:
            return "FILTERED_BLOCK";
        case InvType::MSG_POUW_SOLUTION:
            return "POUW_SOLUTION";
        case InvType::MSG_POUW_PROBLEM:
            return "POUW_PROBLEM";
        case InvType::MSG_UBI_CLAIM:
            return "UBI_CLAIM";
        case InvType::MSG_IDENTITY_PROOF:
            return "IDENTITY_PROOF";
        default:
            return "UNKNOWN";
    }
}

// ============================================================================
// Message Validation Implementation
// ============================================================================

// Set of known valid commands
static const std::set<std::string> VALID_COMMANDS = {
    NetMsgType::VERSION,
    NetMsgType::VERACK,
    NetMsgType::SENDHEADERS,
    NetMsgType::ADDR,
    NetMsgType::ADDRV2,
    NetMsgType::GETADDR,
    NetMsgType::SENDADDRV2,
    NetMsgType::INV,
    NetMsgType::GETDATA,
    NetMsgType::NOTFOUND,
    NetMsgType::BLOCK,
    NetMsgType::GETBLOCKS,
    NetMsgType::GETHEADERS,
    NetMsgType::HEADERS,
    NetMsgType::TX,
    NetMsgType::MEMPOOL,
    NetMsgType::FEEFILTER,
    NetMsgType::PING,
    NetMsgType::PONG,
    NetMsgType::REJECT,
    NetMsgType::POUWSOL,
    NetMsgType::GETPOUW,
    NetMsgType::POUWPROB,
    NetMsgType::UBICLAIM,
    NetMsgType::IDENTITY
};

MessageValidationResult ValidateCommand(const std::string& command) {
    // Check length
    if (command.empty()) {
        return MessageValidationResult::Invalid(20, "Empty command");
    }
    if (command.size() > MESSAGE_TYPE_SIZE) {
        return MessageValidationResult::Invalid(20, "Command too long");
    }
    
    // Check for valid characters (alphanumeric only)
    for (char c : command) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            return MessageValidationResult::Invalid(20, "Invalid character in command");
        }
    }
    
    // Check if it's a known command (don't reject unknown, but could log)
    if (VALID_COMMANDS.find(command) == VALID_COMMANDS.end()) {
        // Unknown commands are allowed but could be suspicious
        // Don't assign misbehavior for unknown commands to allow protocol extensions
        return MessageValidationResult::Valid();
    }
    
    return MessageValidationResult::Valid();
}

MessageValidationResult ValidatePayloadSize(const std::string& command, size_t payloadSize) {
    // General maximum
    if (payloadSize > MAX_PROTOCOL_MESSAGE_LENGTH) {
        return MessageValidationResult::Invalid(100, "Payload exceeds maximum size");
    }
    
    // Command-specific limits
    if (command == NetMsgType::VERSION) {
        // Version message should be reasonable size (< 1KB typically)
        if (payloadSize > 2000) {
            return MessageValidationResult::Invalid(20, "Version message too large");
        }
        // Minimum size check (at least basic fields)
        if (payloadSize < 46) {
            return MessageValidationResult::Invalid(20, "Version message too small");
        }
    }
    else if (command == NetMsgType::VERACK || command == NetMsgType::SENDHEADERS ||
             command == NetMsgType::GETADDR || command == NetMsgType::MEMPOOL ||
             command == NetMsgType::SENDADDRV2) {
        // These messages should have no payload
        if (payloadSize != 0) {
            return MessageValidationResult::Invalid(10, "Unexpected payload for command");
        }
    }
    else if (command == NetMsgType::PING || command == NetMsgType::PONG) {
        // Ping/pong with nonce is exactly 8 bytes
        // Old versions without nonce would be 0 bytes
        if (payloadSize != 8 && payloadSize != 0) {
            return MessageValidationResult::Invalid(10, "Invalid ping/pong size");
        }
    }
    else if (command == NetMsgType::FEEFILTER) {
        // Fee filter is exactly 8 bytes (int64)
        if (payloadSize != 8) {
            return MessageValidationResult::Invalid(10, "Invalid feefilter size");
        }
    }
    else if (command == NetMsgType::INV || command == NetMsgType::GETDATA ||
             command == NetMsgType::NOTFOUND) {
        // Maximum based on MAX_INV_SZ items, each 36 bytes + varint
        // 50000 * 36 + 3 bytes for count = ~1.8MB
        size_t maxSize = MAX_INV_SZ * 36 + 9;
        if (payloadSize > maxSize) {
            return MessageValidationResult::Invalid(20, "Inventory message too large");
        }
    }
    else if (command == NetMsgType::HEADERS) {
        // Maximum based on MAX_HEADERS_RESULTS headers
        // Each header is ~80 bytes + varint for tx count
        size_t maxSize = MAX_HEADERS_RESULTS * 82 + 9;
        if (payloadSize > maxSize) {
            return MessageValidationResult::Invalid(20, "Headers message too large");
        }
    }
    else if (command == NetMsgType::ADDR || command == NetMsgType::ADDRV2) {
        // Maximum based on MAX_ADDR_TO_SEND addresses
        // Each address is ~30 bytes
        size_t maxSize = MAX_ADDR_TO_SEND * 32 + 9;
        if (payloadSize > maxSize) {
            return MessageValidationResult::Invalid(20, "Address message too large");
        }
    }
    else if (command == NetMsgType::BLOCK) {
        // Block can be up to max message size (4MB currently)
        // Already checked above
    }
    else if (command == NetMsgType::TX) {
        // Transaction - reasonably limited (100KB for a single tx is very large)
        if (payloadSize > 100000) {
            return MessageValidationResult::Invalid(10, "Transaction message too large");
        }
    }
    
    return MessageValidationResult::Valid();
}

MessageValidationResult ValidateVersionMessage(const VersionMessage& version) {
    // Protocol version range check
    if (version.version < 0) {
        return MessageValidationResult::Invalid(50, "Negative protocol version");
    }
    if (version.version > 100000) {
        // Implausibly high version number
        return MessageValidationResult::Invalid(10, "Implausible protocol version");
    }
    
    // Minimum version check
    if (version.version < MIN_PEER_PROTO_VERSION) {
        return MessageValidationResult::Invalid(0, "Protocol version too old");
    }
    
    // Timestamp validation
    if (!IsReasonableTimestamp(version.timestamp)) {
        return MessageValidationResult::Invalid(10, "Unreasonable timestamp");
    }
    
    // User agent validation
    if (version.userAgent.size() > MAX_SUBVERSION_LENGTH) {
        return MessageValidationResult::Invalid(10, "User agent too long");
    }
    
    // Check for control characters in user agent
    for (char c : version.userAgent) {
        if (static_cast<unsigned char>(c) < 32 && c != '\t') {
            return MessageValidationResult::Invalid(10, "Invalid character in user agent");
        }
    }
    
    // Start height validation
    if (version.startHeight < 0) {
        return MessageValidationResult::Invalid(20, "Negative start height");
    }
    if (version.startHeight > 100000000) {
        // Implausibly high block height
        return MessageValidationResult::Invalid(10, "Implausible start height");
    }
    
    // Nonce should not be zero (used for self-connection detection)
    // But this is informational, not an error
    
    return MessageValidationResult::Valid();
}

bool IsValidInvType(InvType type) {
    switch (type) {
        case InvType::ERROR:
        case InvType::MSG_TX:
        case InvType::MSG_BLOCK:
        case InvType::MSG_FILTERED_BLOCK:
        case InvType::MSG_POUW_SOLUTION:
        case InvType::MSG_POUW_PROBLEM:
        case InvType::MSG_UBI_CLAIM:
        case InvType::MSG_IDENTITY_PROOF:
            return true;
        default:
            return false;
    }
}

bool IsReasonableTimestamp(int64_t timestamp, int64_t maxAgeSec, int64_t maxFutureSec) {
    int64_t now = GetTime();
    
    // Check if too far in the past
    if (timestamp < now - maxAgeSec) {
        return false;
    }
    
    // Check if too far in the future
    if (timestamp > now + maxFutureSec) {
        return false;
    }
    
    return true;
}

std::string SanitizeUserAgent(const std::string& userAgent) {
    std::string result;
    result.reserve(std::min(userAgent.size(), MAX_SUBVERSION_LENGTH));
    
    for (size_t i = 0; i < userAgent.size() && result.size() < MAX_SUBVERSION_LENGTH; ++i) {
        unsigned char c = static_cast<unsigned char>(userAgent[i]);
        
        // Allow printable ASCII characters and tabs
        if (c >= 32 && c < 127) {
            result += static_cast<char>(c);
        } else if (c == '\t') {
            result += ' ';  // Replace tab with space
        }
        // Skip other control characters and high bytes
    }
    
    return result;
}

} // namespace shurium
