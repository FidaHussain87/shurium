// SHURIUM - Network Message Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements network message serialization and parsing utilities.
// Most serialization is done inline in protocol.h templates, but this
// file provides additional message handling and validation functions.

#include <shurium/network/protocol.h>
#include <shurium/crypto/sha256.h>
#include <shurium/core/block.h>

#include <cstring>
#include <sstream>
#include <iomanip>

namespace shurium {

// ============================================================================
// Message Builder
// ============================================================================

/**
 * Helper class for building network messages.
 */
class MessageBuilder {
public:
    explicit MessageBuilder(const std::array<uint8_t, 4>& networkMagic)
        : magic_(networkMagic) {
    }
    
    /// Build a version message
    std::vector<uint8_t> BuildVersionMessage(const VersionMessage& version) {
        DataStream stream;
        version.Serialize(stream);
        return CreateMessage(magic_, NetMsgType::VERSION, 
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a verack message (empty payload)
    std::vector<uint8_t> BuildVerackMessage() {
        return CreateMessage(magic_, NetMsgType::VERACK, {});
    }
    
    /// Build a ping message
    std::vector<uint8_t> BuildPingMessage(uint64_t nonce) {
        PingMessage ping(nonce);
        DataStream stream;
        ping.Serialize(stream);
        return CreateMessage(magic_, NetMsgType::PING,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a pong message
    std::vector<uint8_t> BuildPongMessage(uint64_t nonce) {
        PongMessage pong(nonce);
        DataStream stream;
        pong.Serialize(stream);
        return CreateMessage(magic_, NetMsgType::PONG,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build an inv message
    std::vector<uint8_t> BuildInvMessage(const std::vector<Inv>& inventory) {
        DataStream stream;
        ::shurium::Serialize(stream, inventory);
        return CreateMessage(magic_, NetMsgType::INV,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a getdata message
    std::vector<uint8_t> BuildGetDataMessage(const std::vector<Inv>& inventory) {
        DataStream stream;
        ::shurium::Serialize(stream, inventory);
        return CreateMessage(magic_, NetMsgType::GETDATA,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a getheaders message
    std::vector<uint8_t> BuildGetHeadersMessage(const GetHeadersMessage& msg) {
        DataStream stream;
        msg.Serialize(stream);
        return CreateMessage(magic_, NetMsgType::GETHEADERS,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a getblocks message
    std::vector<uint8_t> BuildGetBlocksMessage(const GetBlocksMessage& msg) {
        DataStream stream;
        msg.Serialize(stream);
        return CreateMessage(magic_, NetMsgType::GETBLOCKS,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a headers message
    std::vector<uint8_t> BuildHeadersMessage(const std::vector<BlockHeader>& headers) {
        DataStream stream;
        WriteCompactSize(stream, headers.size());
        for (const auto& header : headers) {
            ::shurium::Serialize(stream, header);
            // Each header is followed by a transaction count (always 0 in headers msg)
            WriteCompactSize(stream, 0);
        }
        return CreateMessage(magic_, NetMsgType::HEADERS,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a block message
    std::vector<uint8_t> BuildBlockMessage(const Block& block) {
        DataStream stream;
        ::shurium::Serialize(stream, block);
        return CreateMessage(magic_, NetMsgType::BLOCK,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a tx message
    std::vector<uint8_t> BuildTxMessage(const Transaction& tx) {
        DataStream stream;
        ::shurium::Serialize(stream, tx);
        return CreateMessage(magic_, NetMsgType::TX,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build an addr message
    std::vector<uint8_t> BuildAddrMessage(const std::vector<PeerAddress>& addresses) {
        AddrMessage msg;
        msg.addresses = addresses;
        DataStream stream;
        msg.Serialize(stream);
        return CreateMessage(magic_, NetMsgType::ADDR,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a getaddr message (empty payload)
    std::vector<uint8_t> BuildGetAddrMessage() {
        return CreateMessage(magic_, NetMsgType::GETADDR, {});
    }
    
    /// Build a sendheaders message (empty payload)
    std::vector<uint8_t> BuildSendHeadersMessage() {
        return CreateMessage(magic_, NetMsgType::SENDHEADERS, {});
    }
    
    /// Build a feefilter message
    std::vector<uint8_t> BuildFeeFilterMessage(int64_t minFeeRate) {
        FeeFilterMessage msg(minFeeRate);
        DataStream stream;
        msg.Serialize(stream);
        return CreateMessage(magic_, NetMsgType::FEEFILTER,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a reject message
    std::vector<uint8_t> BuildRejectMessage(const std::string& message,
                                             RejectCode code,
                                             const std::string& reason,
                                             const Hash256& data = Hash256()) {
        RejectMessage reject;
        reject.message = message;
        reject.code = code;
        reject.reason = reason;
        reject.data = data;
        DataStream stream;
        reject.Serialize(stream);
        return CreateMessage(magic_, NetMsgType::REJECT,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }
    
    /// Build a notfound message
    std::vector<uint8_t> BuildNotFoundMessage(const std::vector<Inv>& inventory) {
        DataStream stream;
        ::shurium::Serialize(stream, inventory);
        return CreateMessage(magic_, NetMsgType::NOTFOUND,
                            std::vector<uint8_t>(stream.begin(), stream.end()));
    }

private:
    std::array<uint8_t, 4> magic_;
};

// ============================================================================
// Message Parser
// ============================================================================

/**
 * Helper class for parsing network messages.
 */
class MessageParser {
public:
    /// Parse a version message from payload
    static bool ParseVersionMessage(const std::vector<uint8_t>& payload,
                                    VersionMessage& version) {
        try {
            DataStream stream(payload.data(), payload.size());
            version.Unserialize(stream);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse a ping message
    static bool ParsePingMessage(const std::vector<uint8_t>& payload,
                                 PingMessage& ping) {
        try {
            DataStream stream(payload.data(), payload.size());
            ping.Unserialize(stream);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse an inv message
    static bool ParseInvMessage(const std::vector<uint8_t>& payload,
                                std::vector<Inv>& inventory) {
        try {
            DataStream stream(payload.data(), payload.size());
            ::shurium::Unserialize(stream, inventory);
            return inventory.size() <= MAX_INV_SZ;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse a headers message
    static bool ParseHeadersMessage(const std::vector<uint8_t>& payload,
                                    std::vector<BlockHeader>& headers) {
        try {
            DataStream stream(payload.data(), payload.size());
            size_t count = ReadCompactSize(stream);
            if (count > MAX_HEADERS_RESULTS) {
                return false;
            }
            headers.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                BlockHeader header;
                ::shurium::Unserialize(stream, header);
                // Read and discard transaction count
                ReadCompactSize(stream);
                headers.push_back(std::move(header));
            }
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse a block message
    static bool ParseBlockMessage(const std::vector<uint8_t>& payload,
                                  Block& block) {
        try {
            DataStream stream(payload.data(), payload.size());
            ::shurium::Unserialize(stream, block);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse a tx message
    static bool ParseTxMessage(const std::vector<uint8_t>& payload,
                               MutableTransaction& tx) {
        try {
            DataStream stream(payload.data(), payload.size());
            ::shurium::Unserialize(stream, tx);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse an addr message
    static bool ParseAddrMessage(const std::vector<uint8_t>& payload,
                                 std::vector<PeerAddress>& addresses) {
        try {
            DataStream stream(payload.data(), payload.size());
            AddrMessage msg;
            msg.Unserialize(stream);
            addresses = std::move(msg.addresses);
            return addresses.size() <= MAX_ADDR_TO_SEND;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse a getblocks/getheaders message
    static bool ParseGetBlocksMessage(const std::vector<uint8_t>& payload,
                                      GetBlocksMessage& msg) {
        try {
            DataStream stream(payload.data(), payload.size());
            msg.Unserialize(stream);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse a feefilter message
    static bool ParseFeeFilterMessage(const std::vector<uint8_t>& payload,
                                      FeeFilterMessage& msg) {
        try {
            DataStream stream(payload.data(), payload.size());
            msg.Unserialize(stream);
            return msg.minFeeRate >= 0;
        } catch (...) {
            return false;
        }
    }
    
    /// Parse a reject message
    static bool ParseRejectMessage(const std::vector<uint8_t>& payload,
                                   RejectMessage& msg) {
        try {
            DataStream stream(payload.data(), payload.size());
            msg.Unserialize(stream);
            return true;
        } catch (...) {
            return false;
        }
    }
};

// ============================================================================
// Message Validation
// ============================================================================

/// Validate a message header
bool ValidateMessageHeader(const MessageHeader& header,
                          const std::array<uint8_t, 4>& expectedMagic) {
    // Check magic bytes
    if (!header.IsValidMagic(expectedMagic)) {
        return false;
    }
    
    // Check payload size
    if (!header.IsValid()) {
        return false;
    }
    
    return true;
}

/// Validate a complete message (header + payload)
bool ValidateMessage(const MessageHeader& header,
                     const std::vector<uint8_t>& payload,
                     const std::array<uint8_t, 4>& expectedMagic) {
    // Validate header
    if (!ValidateMessageHeader(header, expectedMagic)) {
        return false;
    }
    
    // Check payload size matches
    if (payload.size() != header.payloadSize) {
        return false;
    }
    
    // Verify checksum
    return VerifyChecksum(payload, header.checksum);
}

/// Get the command string from a header
std::string GetMessageCommand(const MessageHeader& header) {
    return header.GetCommand();
}

/// Check if a command is known
bool IsKnownCommand(const std::string& command) {
    return command == NetMsgType::VERSION ||
           command == NetMsgType::VERACK ||
           command == NetMsgType::ADDR ||
           command == NetMsgType::ADDRV2 ||
           command == NetMsgType::GETADDR ||
           command == NetMsgType::SENDADDRV2 ||
           command == NetMsgType::INV ||
           command == NetMsgType::GETDATA ||
           command == NetMsgType::NOTFOUND ||
           command == NetMsgType::BLOCK ||
           command == NetMsgType::GETBLOCKS ||
           command == NetMsgType::GETHEADERS ||
           command == NetMsgType::HEADERS ||
           command == NetMsgType::TX ||
           command == NetMsgType::MEMPOOL ||
           command == NetMsgType::FEEFILTER ||
           command == NetMsgType::PING ||
           command == NetMsgType::PONG ||
           command == NetMsgType::REJECT ||
           command == NetMsgType::SENDHEADERS ||
           command == NetMsgType::POUWSOL ||
           command == NetMsgType::GETPOUW ||
           command == NetMsgType::POUWPROB ||
           command == NetMsgType::UBICLAIM ||
           command == NetMsgType::IDENTITY;
}

} // namespace shurium
