// Copyright (c) 2024 The SHURIUM developers
// Distributed under the MIT software license

#include <gtest/gtest.h>
#include <shurium/network/address.h>
#include <shurium/network/protocol.h>
#include <shurium/network/peer.h>
#include <shurium/network/message_processor.h>
#include <shurium/network/addrman.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>

namespace shurium {
namespace {

// Helper to create a hash with a specific value
Hash256 MakeHash(uint8_t fill_value) {
    Hash256 hash;
    for (size_t i = 0; i < hash.size(); ++i) {
        hash[i] = fill_value;
    }
    return hash;
}

// ============================================================================
// NetAddress Tests
// ============================================================================

TEST(NetAddressTest, DefaultConstructor) {
    NetAddress addr;
    EXPECT_EQ(addr.GetNetwork(), Network::UNROUTABLE);
    EXPECT_TRUE(addr.GetBytes().empty());
    EXPECT_FALSE(addr.IsValid());
}

TEST(NetAddressTest, IPv4Constructor) {
    std::array<uint8_t, 4> ipv4 = {192, 168, 1, 1};
    NetAddress addr(ipv4);
    
    EXPECT_EQ(addr.GetNetwork(), Network::IPV4);
    EXPECT_TRUE(addr.IsIPv4());
    EXPECT_FALSE(addr.IsIPv6());
    EXPECT_TRUE(addr.IsValid());
    EXPECT_EQ(addr.GetBytes().size(), 4u);
}

TEST(NetAddressTest, IPv6Constructor) {
    std::array<uint8_t, 16> ipv6 = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0, 0, 0, 1};
    NetAddress addr(ipv6);
    
    EXPECT_EQ(addr.GetNetwork(), Network::IPV6);
    EXPECT_TRUE(addr.IsIPv6());
    EXPECT_FALSE(addr.IsIPv4());
    EXPECT_TRUE(addr.IsValid());
}

TEST(NetAddressTest, FromStringIPv4) {
    auto addr = NetAddress::FromString("192.168.1.100");
    ASSERT_TRUE(addr.has_value());
    EXPECT_TRUE(addr->IsIPv4());
    EXPECT_TRUE(addr->IsRFC1918());  // Private address
    
    const auto& bytes = addr->GetBytes();
    EXPECT_EQ(bytes[0], 192);
    EXPECT_EQ(bytes[1], 168);
    EXPECT_EQ(bytes[2], 1);
    EXPECT_EQ(bytes[3], 100);
}

TEST(NetAddressTest, FromStringIPv4Invalid) {
    EXPECT_FALSE(NetAddress::FromString("256.1.1.1").has_value());
    EXPECT_FALSE(NetAddress::FromString("1.2.3").has_value());
    EXPECT_FALSE(NetAddress::FromString("invalid").has_value());
}

TEST(NetAddressTest, FromStringIPv6Localhost) {
    auto addr = NetAddress::FromString("::1");
    ASSERT_TRUE(addr.has_value());
    EXPECT_TRUE(addr->IsIPv6());
    EXPECT_TRUE(addr->IsLocal());
}

TEST(NetAddressTest, FromStringIPv6Any) {
    auto addr = NetAddress::FromString("::");
    ASSERT_TRUE(addr.has_value());
    EXPECT_TRUE(addr->IsIPv6());
    EXPECT_TRUE(addr->IsBindAny());
}

TEST(NetAddressTest, ToStringIPv4) {
    std::array<uint8_t, 4> ipv4 = {10, 20, 30, 40};
    NetAddress addr(ipv4);
    EXPECT_EQ(addr.ToString(), "10.20.30.40");
}

TEST(NetAddressTest, IsLocal) {
    // IPv4 localhost
    auto localhost4 = NetAddress::FromString("127.0.0.1");
    ASSERT_TRUE(localhost4.has_value());
    EXPECT_TRUE(localhost4->IsLocal());
    
    auto localhost4_other = NetAddress::FromString("127.255.255.255");
    ASSERT_TRUE(localhost4_other.has_value());
    EXPECT_TRUE(localhost4_other->IsLocal());
    
    // IPv6 localhost
    auto localhost6 = NetAddress::FromString("::1");
    ASSERT_TRUE(localhost6.has_value());
    EXPECT_TRUE(localhost6->IsLocal());
    
    // Non-local
    auto public_addr = NetAddress::FromString("8.8.8.8");
    ASSERT_TRUE(public_addr.has_value());
    EXPECT_FALSE(public_addr->IsLocal());
}

TEST(NetAddressTest, IsRFC1918) {
    // 10.x.x.x
    auto addr1 = NetAddress::FromString("10.0.0.1");
    ASSERT_TRUE(addr1.has_value());
    EXPECT_TRUE(addr1->IsRFC1918());
    
    // 172.16.x.x - 172.31.x.x
    auto addr2 = NetAddress::FromString("172.16.0.1");
    ASSERT_TRUE(addr2.has_value());
    EXPECT_TRUE(addr2->IsRFC1918());
    
    auto addr3 = NetAddress::FromString("172.31.255.255");
    ASSERT_TRUE(addr3.has_value());
    EXPECT_TRUE(addr3->IsRFC1918());
    
    auto addr4 = NetAddress::FromString("172.32.0.1");  // Not private
    ASSERT_TRUE(addr4.has_value());
    EXPECT_FALSE(addr4->IsRFC1918());
    
    // 192.168.x.x
    auto addr5 = NetAddress::FromString("192.168.100.200");
    ASSERT_TRUE(addr5.has_value());
    EXPECT_TRUE(addr5->IsRFC1918());
    
    // Public
    auto addr6 = NetAddress::FromString("8.8.8.8");
    ASSERT_TRUE(addr6.has_value());
    EXPECT_FALSE(addr6->IsRFC1918());
}

TEST(NetAddressTest, IsRoutable) {
    // Private addresses not routable
    auto private_addr = NetAddress::FromString("192.168.1.1");
    ASSERT_TRUE(private_addr.has_value());
    EXPECT_FALSE(private_addr->IsRoutable());
    
    // Local addresses not routable
    auto local_addr = NetAddress::FromString("127.0.0.1");
    ASSERT_TRUE(local_addr.has_value());
    EXPECT_FALSE(local_addr->IsRoutable());
    
    // Public addresses are routable
    auto public_addr = NetAddress::FromString("8.8.8.8");
    ASSERT_TRUE(public_addr.has_value());
    EXPECT_TRUE(public_addr->IsRoutable());
}

TEST(NetAddressTest, Comparison) {
    auto addr1 = NetAddress::FromString("10.0.0.1");
    auto addr2 = NetAddress::FromString("10.0.0.1");
    auto addr3 = NetAddress::FromString("10.0.0.2");
    
    ASSERT_TRUE(addr1.has_value());
    ASSERT_TRUE(addr2.has_value());
    ASSERT_TRUE(addr3.has_value());
    
    EXPECT_EQ(*addr1, *addr2);
    EXPECT_NE(*addr1, *addr3);
    EXPECT_LT(*addr1, *addr3);
}

TEST(NetAddressTest, Serialization) {
    auto addr = NetAddress::FromString("192.168.1.100");
    ASSERT_TRUE(addr.has_value());
    
    // Serialize
    DataStream stream;
    addr->Serialize(stream);
    
    // Deserialize
    NetAddress addr2;
    addr2.Unserialize(stream);
    
    EXPECT_EQ(*addr, addr2);
}

// ============================================================================
// NetService Tests
// ============================================================================

TEST(NetServiceTest, DefaultConstructor) {
    NetService service;
    EXPECT_EQ(service.GetPort(), 0);
    EXPECT_FALSE(service.IsValid());
}

TEST(NetServiceTest, ConstructorWithPort) {
    std::array<uint8_t, 4> ipv4 = {192, 168, 1, 1};
    NetService service(ipv4, 8433);
    
    EXPECT_EQ(service.GetPort(), 8433);
    EXPECT_TRUE(service.IsValid());
    EXPECT_TRUE(service.IsIPv4());
}

TEST(NetServiceTest, FromString) {
    auto service = NetService::FromString("192.168.1.1:8433");
    ASSERT_TRUE(service.has_value());
    EXPECT_EQ(service->GetPort(), 8433);
    
    auto bytes = service->GetBytes();
    EXPECT_EQ(bytes[0], 192);
    EXPECT_EQ(bytes[3], 1);
}

TEST(NetServiceTest, FromStringIPv6) {
    auto service = NetService::FromString("[::1]:8433");
    ASSERT_TRUE(service.has_value());
    EXPECT_TRUE(service->IsIPv6());
    EXPECT_EQ(service->GetPort(), 8433);
}

TEST(NetServiceTest, ToString) {
    std::array<uint8_t, 4> ipv4 = {10, 0, 0, 1};
    NetService service(ipv4, 8433);
    EXPECT_EQ(service.ToString(), "10.0.0.1:8433");
}

TEST(NetServiceTest, Serialization) {
    auto service = NetService::FromString("10.20.30.40:1234");
    ASSERT_TRUE(service.has_value());
    
    // Serialize
    DataStream stream;
    service->Serialize(stream);
    
    // Deserialize
    NetService service2;
    service2.Unserialize(stream);
    
    EXPECT_EQ(*service, service2);
    EXPECT_EQ(service2.GetPort(), 1234);
}

// ============================================================================
// ServiceFlags Tests
// ============================================================================

TEST(ServiceFlagsTest, Combination) {
    ServiceFlags flags = ServiceFlags::NETWORK | ServiceFlags::WITNESS;
    EXPECT_TRUE(HasFlag(flags, ServiceFlags::NETWORK));
    EXPECT_TRUE(HasFlag(flags, ServiceFlags::WITNESS));
    EXPECT_FALSE(HasFlag(flags, ServiceFlags::BLOOM));
}

TEST(ServiceFlagsTest, ShuriumSpecific) {
    ServiceFlags flags = ServiceFlags::NETWORK | ServiceFlags::POUW_VERIFY | ServiceFlags::UBI;
    EXPECT_TRUE(HasFlag(flags, ServiceFlags::POUW_VERIFY));
    EXPECT_TRUE(HasFlag(flags, ServiceFlags::UBI));
    EXPECT_FALSE(HasFlag(flags, ServiceFlags::IDENTITY));
}

// ============================================================================
// PeerAddress Tests
// ============================================================================

TEST(PeerAddressTest, Constructor) {
    auto service = NetService::FromString("8.8.8.8:8433");
    ASSERT_TRUE(service.has_value());
    
    PeerAddress addr(*service, 1700000000, ServiceFlags::NETWORK);
    EXPECT_EQ(addr.GetPort(), 8433);
    EXPECT_EQ(addr.GetTime(), 1700000000);
    EXPECT_TRUE(addr.HasService(ServiceFlags::NETWORK));
}

// ============================================================================
// Protocol Constants Tests
// ============================================================================

TEST(ProtocolTest, Constants) {
    EXPECT_EQ(PROTOCOL_VERSION, 70001);
    EXPECT_EQ(MIN_PEER_PROTO_VERSION, 70000);
    EXPECT_EQ(MAX_PROTOCOL_MESSAGE_LENGTH, 4 * 1000 * 1000);
    EXPECT_EQ(MESSAGE_HEADER_SIZE, 24u);
    EXPECT_EQ(MESSAGE_TYPE_SIZE, 12u);
}

TEST(ProtocolTest, NetworkMagic) {
    // MAINNET = "NXUS"
    EXPECT_EQ(NetworkMagic::MAINNET[0], 'N');
    EXPECT_EQ(NetworkMagic::MAINNET[1], 'X');
    EXPECT_EQ(NetworkMagic::MAINNET[2], 'U');
    EXPECT_EQ(NetworkMagic::MAINNET[3], 'S');
}

TEST(ProtocolTest, DefaultPorts) {
    EXPECT_EQ(DefaultPort::MAINNET, 8433);
    EXPECT_EQ(DefaultPort::TESTNET, 18433);
    EXPECT_EQ(DefaultPort::REGTEST, 18444);
}

// ============================================================================
// Inv Tests
// ============================================================================

TEST(InvTest, DefaultConstructor) {
    Inv inv;
    EXPECT_EQ(inv.type, InvType::ERROR);
    EXPECT_FALSE(inv.IsTransaction());
    EXPECT_FALSE(inv.IsBlock());
}

TEST(InvTest, TransactionInv) {
    Hash256 hash = MakeHash(0x42);
    Inv inv(InvType::MSG_TX, hash);
    
    EXPECT_TRUE(inv.IsTransaction());
    EXPECT_FALSE(inv.IsBlock());
    EXPECT_EQ(inv.hash, hash);
}

TEST(InvTest, BlockInv) {
    Hash256 hash = MakeHash(0xAB);
    Inv inv(InvType::MSG_BLOCK, hash);
    
    EXPECT_FALSE(inv.IsTransaction());
    EXPECT_TRUE(inv.IsBlock());
}

TEST(InvTest, Serialization) {
    Hash256 hash = MakeHash(0x11);
    Inv inv(InvType::MSG_TX, hash);
    
    DataStream stream;
    inv.Serialize(stream);
    
    Inv inv2;
    inv2.Unserialize(stream);
    
    EXPECT_EQ(inv, inv2);
}

TEST(InvTest, ToString) {
    Hash256 hash = MakeHash(0xAB);
    Inv inv(InvType::MSG_BLOCK, hash);
    std::string str = inv.ToString();
    EXPECT_TRUE(str.find("BLOCK") != std::string::npos);
}

// ============================================================================
// MessageHeader Tests
// ============================================================================

TEST(MessageHeaderTest, DefaultConstructor) {
    MessageHeader header;
    EXPECT_EQ(header.payloadSize, 0u);
    EXPECT_EQ(header.GetCommand(), "");
}

TEST(MessageHeaderTest, SetCommand) {
    MessageHeader header;
    header.SetCommand("version");
    EXPECT_EQ(header.GetCommand(), "version");
    
    // Command truncated to 12 chars
    header.SetCommand("verylongcommandname");
    EXPECT_EQ(header.GetCommand().size(), 12u);
}

TEST(MessageHeaderTest, IsValid) {
    MessageHeader header;
    header.payloadSize = 1000;
    EXPECT_TRUE(header.IsValid());
    
    header.payloadSize = MAX_PROTOCOL_MESSAGE_LENGTH + 1;
    EXPECT_FALSE(header.IsValid());
}

TEST(MessageHeaderTest, IsValidMagic) {
    MessageHeader header;
    header.magic = NetworkMagic::MAINNET;
    EXPECT_TRUE(header.IsValidMagic(NetworkMagic::MAINNET));
    EXPECT_FALSE(header.IsValidMagic(NetworkMagic::TESTNET));
}

TEST(MessageHeaderTest, Serialization) {
    MessageHeader header;
    header.magic = NetworkMagic::MAINNET;
    header.SetCommand("ping");
    header.payloadSize = 8;
    header.checksum = {0x01, 0x02, 0x03, 0x04};
    
    DataStream stream;
    header.Serialize(stream);
    
    EXPECT_EQ(stream.TotalSize(), MESSAGE_HEADER_SIZE);
    
    MessageHeader header2;
    header2.Unserialize(stream);
    
    EXPECT_EQ(header2.magic, header.magic);
    EXPECT_EQ(header2.GetCommand(), "ping");
    EXPECT_EQ(header2.payloadSize, 8u);
    EXPECT_EQ(header2.checksum, header.checksum);
}

// ============================================================================
// VersionMessage Tests
// ============================================================================

TEST(VersionMessageTest, DefaultValues) {
    VersionMessage ver;
    EXPECT_EQ(ver.version, PROTOCOL_VERSION);
    EXPECT_EQ(ver.services, ServiceFlags::NONE);
    EXPECT_EQ(ver.nonce, 0u);
    EXPECT_TRUE(ver.relay);
}

TEST(VersionMessageTest, Serialization) {
    VersionMessage ver;
    ver.version = PROTOCOL_VERSION;
    ver.services = ServiceFlags::NETWORK | ServiceFlags::WITNESS;
    ver.timestamp = 1700000000;
    ver.nonce = 12345678;
    ver.userAgent = "/SHURIUM:0.1.0/";
    ver.startHeight = 100000;
    ver.relay = true;
    
    DataStream stream;
    ver.Serialize(stream);
    
    VersionMessage ver2;
    ver2.Unserialize(stream);
    
    EXPECT_EQ(ver2.version, ver.version);
    EXPECT_EQ(ver2.services, ver.services);
    EXPECT_EQ(ver2.timestamp, ver.timestamp);
    EXPECT_EQ(ver2.nonce, ver.nonce);
    EXPECT_EQ(ver2.userAgent, ver.userAgent);
    EXPECT_EQ(ver2.startHeight, ver.startHeight);
    EXPECT_EQ(ver2.relay, ver.relay);
}

// ============================================================================
// Message Creation Tests
// ============================================================================

TEST(MessageCreationTest, CreateEmptyMessage) {
    std::vector<uint8_t> payload;
    auto msg = CreateMessage(NetworkMagic::MAINNET, "ping", payload);
    
    EXPECT_EQ(msg.size(), MESSAGE_HEADER_SIZE);
    
    auto header = ParseMessageHeader(msg);
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->GetCommand(), "ping");
    EXPECT_EQ(header->payloadSize, 0u);
}

TEST(MessageCreationTest, CreateMessageWithPayload) {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    auto msg = CreateMessage(NetworkMagic::MAINNET, "test", payload);
    
    EXPECT_EQ(msg.size(), MESSAGE_HEADER_SIZE + payload.size());
    
    auto header = ParseMessageHeader(msg);
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->payloadSize, 4u);
}

TEST(MessageCreationTest, VerifyChecksum) {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto checksum = ComputeChecksum(payload);
    
    EXPECT_TRUE(VerifyChecksum(payload, checksum));
    
    // Modify payload
    payload[0] = 0xFF;
    EXPECT_FALSE(VerifyChecksum(payload, checksum));
}

// ============================================================================
// Peer Tests
// ============================================================================

TEST(PeerTest, CreateOutbound) {
    auto service = NetService::FromString("8.8.8.8:8433");
    ASSERT_TRUE(service.has_value());
    
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    ASSERT_NE(peer, nullptr);
    
    EXPECT_EQ(peer->GetId(), 1);
    EXPECT_EQ(peer->GetAddress(), *service);
    EXPECT_TRUE(peer->IsOutbound());
    EXPECT_FALSE(peer->IsInbound());
    EXPECT_EQ(peer->GetConnectionType(), ConnectionType::OUTBOUND_FULL_RELAY);
    EXPECT_EQ(peer->GetState(), PeerState::DISCONNECTED);
}

TEST(PeerTest, CreateInbound) {
    auto service = NetService::FromString("10.0.0.5:12345");
    ASSERT_TRUE(service.has_value());
    
    auto peer = Peer::CreateInbound(2, *service);
    ASSERT_NE(peer, nullptr);
    
    EXPECT_EQ(peer->GetId(), 2);
    EXPECT_TRUE(peer->IsInbound());
    EXPECT_FALSE(peer->IsOutbound());
    EXPECT_EQ(peer->GetConnectionType(), ConnectionType::INBOUND);
}

TEST(PeerTest, ProcessVersion) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    VersionMessage ver;
    ver.version = PROTOCOL_VERSION;
    ver.services = ServiceFlags::NETWORK;
    ver.userAgent = "/TestPeer/";
    ver.startHeight = 50000;
    ver.relay = true;
    
    EXPECT_TRUE(peer->ProcessVersion(ver));
    EXPECT_EQ(peer->GetVersion(), PROTOCOL_VERSION);
    EXPECT_TRUE(peer->HasService(ServiceFlags::NETWORK));
    EXPECT_EQ(peer->GetUserAgent(), "/TestPeer/");
    EXPECT_EQ(peer->GetStartingHeight(), 50000);
    EXPECT_TRUE(peer->RelaysTransactions());
}

TEST(PeerTest, ProcessVersionTooOld) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    VersionMessage ver;
    ver.version = MIN_PEER_PROTO_VERSION - 1;  // Too old
    
    EXPECT_FALSE(peer->ProcessVersion(ver));
    EXPECT_TRUE(peer->ShouldDisconnect());
}

TEST(PeerTest, ProcessVersionDuplicate) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    VersionMessage ver;
    ver.version = PROTOCOL_VERSION;
    
    EXPECT_TRUE(peer->ProcessVersion(ver));
    EXPECT_FALSE(peer->ProcessVersion(ver));  // Duplicate
}

TEST(PeerTest, ProcessVerack) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    // Verack before version should fail
    EXPECT_FALSE(peer->ProcessVerack());
    
    // Send version first
    VersionMessage ver;
    ver.version = PROTOCOL_VERSION;
    peer->ProcessVersion(ver);
    
    // Now verack should work
    EXPECT_TRUE(peer->ProcessVerack());
    EXPECT_TRUE(peer->IsEstablished());
}

TEST(PeerTest, CreateVersionMessage) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    auto ourAddr = NetService::FromString("192.168.1.1:8433");
    ServiceFlags ourServices = ServiceFlags::NETWORK | ServiceFlags::POUW_VERIFY;
    
    auto ver = peer->CreateVersionMessage(*ourAddr, 100000, ourServices);
    
    EXPECT_EQ(ver.version, PROTOCOL_VERSION);
    EXPECT_EQ(ver.services, ourServices);
    EXPECT_EQ(ver.startHeight, 100000);
    EXPECT_NE(ver.nonce, 0u);  // Should be random
}

TEST(PeerTest, Misbehavior) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    EXPECT_EQ(peer->GetMisbehaviorScore(), 0);
    
    // Small misbehavior
    EXPECT_FALSE(peer->Misbehaving(10, "test"));
    EXPECT_EQ(peer->GetMisbehaviorScore(), 10);
    EXPECT_FALSE(peer->ShouldDisconnect());
    
    // Large misbehavior triggers ban
    EXPECT_TRUE(peer->Misbehaving(100, "serious"));
    EXPECT_TRUE(peer->ShouldDisconnect());
}

TEST(PeerTest, InventoryTracking) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    Hash256 hash = MakeHash(0xAB);
    Inv inv(InvType::MSG_TX, hash);
    
    // Initially no inventory
    EXPECT_FALSE(peer->HasAnnounced(inv));
    EXPECT_FALSE(peer->HasInventory(inv));
    
    // Mark as announced by us
    peer->MarkAnnounced(inv);
    EXPECT_TRUE(peer->HasAnnounced(inv));
    
    // Add inventory from peer
    peer->AddInventory(inv);
    EXPECT_TRUE(peer->HasInventory(inv));
}

TEST(PeerTest, AnnouncementQueue) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    // Queue some announcements
    for (int i = 0; i < 5; ++i) {
        Hash256 hash = MakeHash(static_cast<uint8_t>(i));
        Inv inv(InvType::MSG_TX, hash);
        peer->QueueAnnouncement(inv);
    }
    
    EXPECT_EQ(peer->GetAnnouncementQueueSize(), 5u);
    
    // Get some announcements
    auto toSend = peer->GetAnnouncementsToSend(3);
    EXPECT_EQ(toSend.size(), 3u);
    
    // They should now be marked as announced
    for (const auto& inv : toSend) {
        EXPECT_TRUE(peer->HasAnnounced(inv));
    }
    
    // Remaining in queue
    EXPECT_EQ(peer->GetAnnouncementQueueSize(), 2u);
}

TEST(PeerTest, SendReceiveBuffers) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    EXPECT_FALSE(peer->HasDataToSend());
    
    // Queue some data
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    peer->QueueSend(data);
    EXPECT_TRUE(peer->HasDataToSend());
    
    // Get partial data
    auto part1 = peer->GetSendData(3);
    EXPECT_EQ(part1.size(), 3u);
    EXPECT_EQ(part1[0], 1);
    EXPECT_EQ(part1[2], 3);
    
    // Get rest
    auto part2 = peer->GetSendData(10);
    EXPECT_EQ(part2.size(), 2u);
    
    EXPECT_FALSE(peer->HasDataToSend());
}

TEST(PeerTest, Statistics) {
    auto service = NetService::FromString("8.8.8.8:8433");
    auto peer = Peer::CreateOutbound(1, *service, ConnectionType::OUTBOUND_FULL_RELAY);
    
    auto stats = peer->GetStats();
    EXPECT_EQ(stats.bytesSent, 0u);
    EXPECT_EQ(stats.bytesRecv, 0u);
    EXPECT_EQ(stats.messagesSent, 0u);
    EXPECT_FALSE(stats.fInbound);
    
    peer->RecordBytesSent(100);
    peer->RecordBytesReceived(200);
    peer->RecordMessageSent();
    peer->RecordMessageReceived();
    peer->RecordMessageReceived();
    
    stats = peer->GetStats();
    EXPECT_EQ(stats.bytesSent, 100u);
    EXPECT_EQ(stats.bytesRecv, 200u);
    EXPECT_EQ(stats.messagesSent, 1u);
    EXPECT_EQ(stats.messagesRecv, 2u);
}

// ============================================================================
// InvType Tests
// ============================================================================

TEST(InvTypeTest, ToString) {
    EXPECT_EQ(InvTypeToString(InvType::ERROR), "ERROR");
    EXPECT_EQ(InvTypeToString(InvType::MSG_TX), "TX");
    EXPECT_EQ(InvTypeToString(InvType::MSG_BLOCK), "BLOCK");
    EXPECT_EQ(InvTypeToString(InvType::MSG_POUW_SOLUTION), "POUW_SOLUTION");
    EXPECT_EQ(InvTypeToString(InvType::MSG_UBI_CLAIM), "UBI_CLAIM");
}

// ============================================================================
// BlockLocator Tests
// ============================================================================

TEST(BlockLocatorTest, DefaultConstructor) {
    BlockLocator locator;
    EXPECT_TRUE(locator.IsNull());
    EXPECT_TRUE(locator.vHave.empty());
}

TEST(BlockLocatorTest, WithHashes) {
    std::vector<BlockHash> hashes(10);
    for (size_t i = 0; i < hashes.size(); ++i) {
        hashes[i] = BlockHash(MakeHash(static_cast<uint8_t>(i)));
    }
    
    BlockLocator locator(std::move(hashes));
    EXPECT_FALSE(locator.IsNull());
    EXPECT_EQ(locator.vHave.size(), 10u);
}

TEST(BlockLocatorTest, Serialization) {
    std::vector<BlockHash> hashes(3);
    hashes[0] = BlockHash(MakeHash(0xAA));
    hashes[1] = BlockHash(MakeHash(0xBB));
    hashes[2] = BlockHash(MakeHash(0xCC));
    
    BlockLocator locator(std::move(hashes));
    
    DataStream stream;
    Serialize(stream, locator);
    
    BlockLocator locator2;
    Unserialize(stream, locator2);
    
    EXPECT_EQ(locator2.vHave.size(), 3u);
    EXPECT_EQ(locator2.vHave[0], BlockHash(MakeHash(0xAA)));
    EXPECT_EQ(locator2.vHave[2], BlockHash(MakeHash(0xCC)));
}

// ============================================================================
// PingPong Tests
// ============================================================================

TEST(PingPongTest, PingMessage) {
    PingMessage ping(12345);
    EXPECT_EQ(ping.nonce, 12345u);
    
    DataStream stream;
    ping.Serialize(stream);
    
    PingMessage ping2;
    ping2.Unserialize(stream);
    EXPECT_EQ(ping2.nonce, 12345u);
}

TEST(PingPongTest, PongMessage) {
    PongMessage pong(67890);
    
    DataStream stream;
    pong.Serialize(stream);
    
    PongMessage pong2;
    pong2.Unserialize(stream);
    EXPECT_EQ(pong2.nonce, 67890u);
}

// ============================================================================
// FeeFilterMessage Tests
// ============================================================================

TEST(FeeFilterTest, Serialization) {
    FeeFilterMessage msg(1000);
    
    DataStream stream;
    msg.Serialize(stream);
    
    FeeFilterMessage msg2;
    msg2.Unserialize(stream);
    EXPECT_EQ(msg2.minFeeRate, 1000);
}

// ============================================================================
// Hash Functions for Address/Service
// ============================================================================

TEST(NetAddressHasherTest, Hash) {
    auto addr = NetAddress::FromString("192.168.1.1");
    ASSERT_TRUE(addr.has_value());
    
    NetAddressHasher hasher;
    size_t hash = hasher(*addr);
    EXPECT_NE(hash, 0u);
    
    // Same address should give same hash
    auto addr2 = NetAddress::FromString("192.168.1.1");
    EXPECT_EQ(hasher(*addr), hasher(*addr2));
}

TEST(NetServiceHasherTest, Hash) {
    auto service = NetService::FromString("192.168.1.1:8433");
    ASSERT_TRUE(service.has_value());
    
    NetServiceHasher hasher;
    size_t hash = hasher(*service);
    EXPECT_NE(hash, 0u);
    
    // Different port should give different hash
    auto service2 = NetService::FromString("192.168.1.1:8434");
    EXPECT_NE(hasher(*service), hasher(*service2));
}

// ============================================================================
// MessageProcessor Tests  
// ============================================================================

TEST(MessageProcessorOptionsTest, DefaultValues) {
    MessageProcessorOptions opts;
    EXPECT_EQ(opts.processingIntervalMs, 100);
    EXPECT_EQ(opts.pingIntervalSec, 120);
    EXPECT_EQ(opts.pingTimeoutSec, 30);
    EXPECT_EQ(opts.maxMessagesPerPeer, 100);
    EXPECT_TRUE(opts.relayTransactions);
}

TEST(MessageStatsTest, DefaultValues) {
    MessageStats stats;
    EXPECT_EQ(stats.messagesProcessed, 0u);
    EXPECT_EQ(stats.versionMessages, 0u);
    EXPECT_EQ(stats.verackMessages, 0u);
    EXPECT_EQ(stats.pingMessages, 0u);
    EXPECT_EQ(stats.pongMessages, 0u);
    EXPECT_EQ(stats.invMessages, 0u);
    EXPECT_EQ(stats.getdataMessages, 0u);
    EXPECT_EQ(stats.headersMessages, 0u);
    EXPECT_EQ(stats.blockMessages, 0u);
    EXPECT_EQ(stats.txMessages, 0u);
    EXPECT_EQ(stats.addrMessages, 0u);
    EXPECT_EQ(stats.unknownMessages, 0u);
    EXPECT_EQ(stats.invalidMessages, 0u);
}

TEST(MessageProcessorTest, Construction) {
    MessageProcessorOptions opts;
    opts.processingIntervalMs = 50;
    opts.relayTransactions = false;
    
    MessageProcessor processor(opts);
    EXPECT_FALSE(processor.IsRunning());
}

TEST(MessageProcessorTest, StartWithoutConnectionManager) {
    MessageProcessor processor;
    // Should fail without connection manager
    EXPECT_FALSE(processor.Start());
}

TEST(MessageProcessorTest, GetStatsInitial) {
    MessageProcessor processor;
    auto stats = processor.GetStats();
    EXPECT_EQ(stats.messagesProcessed, 0u);
    EXPECT_EQ(stats.versionMessages, 0u);
}

TEST(MessageProcessorTest, ResetStats) {
    MessageProcessor processor;
    auto stats = processor.GetStats();
    // Initial stats should be zero
    EXPECT_EQ(stats.messagesProcessed, 0u);
    
    // Reset should work without error
    processor.ResetStats();
    stats = processor.GetStats();
    EXPECT_EQ(stats.messagesProcessed, 0u);
}

// ============================================================================
// AddressManager Tests
// ============================================================================

TEST(AddressManagerTest, Construction) {
    AddressManager addrman("main");
    EXPECT_EQ(addrman.Size(), 0u);
    EXPECT_EQ(addrman.NumTried(), 0u);
    EXPECT_EQ(addrman.NumNew(), 0u);
}

TEST(AddressManagerTest, ConstructionTestnet) {
    AddressManager addrman("test");
    EXPECT_EQ(addrman.Size(), 0u);
}

TEST(AddressManagerTest, ConstructionRegtest) {
    AddressManager addrman("regtest");
    EXPECT_EQ(addrman.Size(), 0u);
}

TEST(AddressManagerTest, AddValidAddress) {
    AddressManager addrman("main");
    
    // Create a routable address (8.8.8.8:8333)
    std::array<uint8_t, 4> ip = {8, 8, 8, 8};
    NetAddress netAddr(ip);
    NetService service(netAddr, 8333);
    PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
    
    NetService source;  // Empty source
    
    EXPECT_TRUE(addrman.Add(peerAddr, source, 0));
    EXPECT_EQ(addrman.Size(), 1u);
    EXPECT_EQ(addrman.NumNew(), 1u);
    EXPECT_EQ(addrman.NumTried(), 0u);
}

TEST(AddressManagerTest, AddDuplicateAddress) {
    AddressManager addrman("main");
    
    std::array<uint8_t, 4> ip = {8, 8, 8, 8};
    NetAddress netAddr(ip);
    NetService service(netAddr, 8333);
    PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
    
    NetService source;
    
    // First add should succeed
    EXPECT_TRUE(addrman.Add(peerAddr, source, 0));
    EXPECT_EQ(addrman.Size(), 1u);
    
    // Second add should return false (duplicate)
    EXPECT_FALSE(addrman.Add(peerAddr, source, 0));
    EXPECT_EQ(addrman.Size(), 1u);  // Size unchanged
}

TEST(AddressManagerTest, AddPrivateAddressRejected) {
    AddressManager addrman("main");
    
    // Private address 192.168.1.1 should be rejected
    std::array<uint8_t, 4> ip = {192, 168, 1, 1};
    NetAddress netAddr(ip);
    NetService service(netAddr, 8333);
    PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
    
    NetService source;
    
    EXPECT_FALSE(addrman.Add(peerAddr, source, 0));
    EXPECT_EQ(addrman.Size(), 0u);
}

TEST(AddressManagerTest, AddZeroPortRejected) {
    AddressManager addrman("main");
    
    // Zero port should be rejected
    std::array<uint8_t, 4> ip = {8, 8, 8, 8};
    NetAddress netAddr(ip);
    NetService service(netAddr, 0);  // Port 0
    PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
    
    NetService source;
    
    EXPECT_FALSE(addrman.Add(peerAddr, source, 0));
    EXPECT_EQ(addrman.Size(), 0u);
}

TEST(AddressManagerTest, AddMultipleAddresses) {
    AddressManager addrman("main");
    
    std::vector<PeerAddress> addrs;
    
    // Add 5 different addresses
    for (int i = 1; i <= 5; i++) {
        std::array<uint8_t, 4> ip = {8, 8, 8, static_cast<uint8_t>(i)};
        NetAddress netAddr(ip);
        NetService service(netAddr, 8333);
        addrs.emplace_back(service, GetAdjustedTime(), ServiceFlags::NETWORK);
    }
    
    NetService source;
    size_t added = addrman.Add(addrs, source, 0);
    
    EXPECT_EQ(added, 5u);
    EXPECT_EQ(addrman.Size(), 5u);
    EXPECT_EQ(addrman.NumNew(), 5u);
}

TEST(AddressManagerTest, SelectFromEmpty) {
    AddressManager addrman("main");
    
    auto result = addrman.Select();
    EXPECT_FALSE(result.has_value());
}

TEST(AddressManagerTest, SelectFromNonEmpty) {
    AddressManager addrman("main");
    
    // Add an address
    std::array<uint8_t, 4> ip = {8, 8, 8, 8};
    NetAddress netAddr(ip);
    NetService service(netAddr, 8333);
    PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
    
    NetService source;
    addrman.Add(peerAddr, source, 0);
    
    auto result = addrman.Select();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->GetPort(), 8333);
}

TEST(AddressManagerTest, Good) {
    AddressManager addrman("main");
    
    // Add an address
    std::array<uint8_t, 4> ip = {8, 8, 8, 8};
    NetAddress netAddr(ip);
    NetService service(netAddr, 8333);
    PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
    
    NetService source;
    addrman.Add(peerAddr, source, 0);
    
    EXPECT_EQ(addrman.NumNew(), 1u);
    EXPECT_EQ(addrman.NumTried(), 0u);
    
    // Mark as good (successfully connected)
    addrman.Good(service);
    
    // Should move to tried bucket
    EXPECT_EQ(addrman.NumNew(), 0u);
    EXPECT_EQ(addrman.NumTried(), 1u);
}

TEST(AddressManagerTest, Clear) {
    AddressManager addrman("main");
    
    // Add some addresses
    for (int i = 1; i <= 3; i++) {
        std::array<uint8_t, 4> ip = {8, 8, 8, static_cast<uint8_t>(i)};
        NetAddress netAddr(ip);
        NetService service(netAddr, 8333);
        PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
        addrman.Add(peerAddr, NetService(), 0);
    }
    
    EXPECT_EQ(addrman.Size(), 3u);
    
    addrman.Clear();
    
    EXPECT_EQ(addrman.Size(), 0u);
    EXPECT_EQ(addrman.NumNew(), 0u);
    EXPECT_EQ(addrman.NumTried(), 0u);
}

TEST(AddressManagerTest, GetAddr) {
    AddressManager addrman("main");
    
    // Add 10 addresses
    for (int i = 1; i <= 10; i++) {
        std::array<uint8_t, 4> ip = {8, 8, 8, static_cast<uint8_t>(i)};
        NetAddress netAddr(ip);
        NetService service(netAddr, 8333);
        PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
        addrman.Add(peerAddr, NetService(), 0);
    }
    
    // Request up to 5 addresses
    auto addrs = addrman.GetAddr(5);
    EXPECT_LE(addrs.size(), 5u);
    EXPECT_GT(addrs.size(), 0u);
}

TEST(AddressManagerTest, SelectMany) {
    AddressManager addrman("main");
    
    // Add 10 addresses
    for (int i = 1; i <= 10; i++) {
        std::array<uint8_t, 4> ip = {8, 8, 8, static_cast<uint8_t>(i)};
        NetAddress netAddr(ip);
        NetService service(netAddr, 8333);
        PeerAddress peerAddr(service, GetAdjustedTime(), ServiceFlags::NETWORK);
        addrman.Add(peerAddr, NetService(), 0);
    }
    
    // Select multiple addresses
    auto addrs = addrman.SelectMany(5);
    EXPECT_LE(addrs.size(), 5u);
    EXPECT_GT(addrs.size(), 0u);
}

TEST(AddressManagerTest, StartStop) {
    AddressManager addrman("main");
    
    // Should not crash
    addrman.Start();
    addrman.Stop();
}

// ============================================================================
// Address Manager Persistence Tests
// ============================================================================

TEST(AddressManagerTest, SaveEmptyManager) {
    std::string tempPath = "/tmp/shurium_peers_test_" + 
                           std::to_string(std::time(nullptr)) + "_" +
                           std::to_string(rand()) + ".dat";
    
    AddressManager addrman("main");
    EXPECT_TRUE(addrman.Save(tempPath));
    
    // Verify file was created
    std::ifstream file(tempPath, std::ios::binary);
    EXPECT_TRUE(file.good());
    file.close();
    
    // Cleanup
    std::remove(tempPath.c_str());
}

TEST(AddressManagerTest, SaveAndLoadEmpty) {
    std::string tempPath = "/tmp/shurium_peers_test_" + 
                           std::to_string(std::time(nullptr)) + "_" +
                           std::to_string(rand()) + ".dat";
    
    // Save empty manager
    {
        AddressManager addrman("main");
        EXPECT_TRUE(addrman.Save(tempPath));
    }
    
    // Load and verify
    {
        AddressManager addrman("main");
        EXPECT_TRUE(addrman.Load(tempPath));
        EXPECT_EQ(addrman.Size(), 0);
    }
    
    // Cleanup
    std::remove(tempPath.c_str());
}

TEST(AddressManagerTest, SaveAndLoadWithAddresses) {
    std::string tempPath = "/tmp/shurium_peers_test_" + 
                           std::to_string(std::time(nullptr)) + "_" +
                           std::to_string(rand()) + ".dat";
    
    // Add addresses and save
    {
        AddressManager addrman("main");
        
        // Add some routable addresses
        std::array<uint8_t, 4> ip1 = {8, 8, 8, 8};
        std::array<uint8_t, 4> ip2 = {1, 1, 1, 1};
        std::array<uint8_t, 4> ip3 = {185, 199, 108, 153};  // GitHub
        
        NetService sourceService;  // Default/empty source
        
        PeerAddress addr1(NetService(NetAddress(ip1), 8333), GetAdjustedTime(), ServiceFlags::NETWORK);
        PeerAddress addr2(NetService(NetAddress(ip2), 8333), GetAdjustedTime(), ServiceFlags::NETWORK);
        PeerAddress addr3(NetService(NetAddress(ip3), 8333), GetAdjustedTime(), ServiceFlags::NETWORK | ServiceFlags::WITNESS);
        
        EXPECT_TRUE(addrman.Add(addr1, sourceService));
        EXPECT_TRUE(addrman.Add(addr2, sourceService));
        EXPECT_TRUE(addrman.Add(addr3, sourceService));
        
        EXPECT_EQ(addrman.Size(), 3);
        EXPECT_EQ(addrman.NumNew(), 3);
        EXPECT_EQ(addrman.NumTried(), 0);
        
        EXPECT_TRUE(addrman.Save(tempPath));
    }
    
    // Load and verify
    {
        AddressManager addrman("main");
        EXPECT_TRUE(addrman.Load(tempPath));
        
        EXPECT_EQ(addrman.Size(), 3);
        EXPECT_EQ(addrman.NumNew(), 3);
        EXPECT_EQ(addrman.NumTried(), 0);
    }
    
    // Cleanup
    std::remove(tempPath.c_str());
}

TEST(AddressManagerTest, SaveAndLoadWithTriedAddresses) {
    std::string tempPath = "/tmp/shurium_peers_test_" + 
                           std::to_string(std::time(nullptr)) + "_" +
                           std::to_string(rand()) + ".dat";
    
    // Add addresses, mark some as good, and save
    {
        AddressManager addrman("main");
        
        std::array<uint8_t, 4> ip1 = {8, 8, 8, 8};
        std::array<uint8_t, 4> ip2 = {1, 1, 1, 1};
        
        NetService sourceService;
        
        PeerAddress addr1(NetService(NetAddress(ip1), 8333), GetAdjustedTime(), ServiceFlags::NETWORK);
        PeerAddress addr2(NetService(NetAddress(ip2), 8333), GetAdjustedTime(), ServiceFlags::NETWORK);
        
        addrman.Add(addr1, sourceService);
        addrman.Add(addr2, sourceService);
        
        // Mark addr1 as successfully connected
        addrman.Good(NetService(NetAddress(ip1), 8333));
        
        EXPECT_EQ(addrman.NumNew(), 1);
        EXPECT_EQ(addrman.NumTried(), 1);
        
        EXPECT_TRUE(addrman.Save(tempPath));
    }
    
    // Load and verify
    {
        AddressManager addrman("main");
        EXPECT_TRUE(addrman.Load(tempPath));
        
        EXPECT_EQ(addrman.Size(), 2);
        EXPECT_EQ(addrman.NumNew(), 1);
        EXPECT_EQ(addrman.NumTried(), 1);
    }
    
    // Cleanup
    std::remove(tempPath.c_str());
}

TEST(AddressManagerTest, LoadNonexistentFile) {
    AddressManager addrman("main");
    
    // Loading non-existent file should succeed (returns true, empty manager)
    EXPECT_TRUE(addrman.Load("/nonexistent/path/to/peers.dat"));
    EXPECT_EQ(addrman.Size(), 0);
}

TEST(AddressManagerTest, LoadInvalidMagic) {
    std::string tempPath = "/tmp/shurium_peers_test_" + 
                           std::to_string(std::time(nullptr)) + "_" +
                           std::to_string(rand()) + ".dat";
    
    // Write invalid file
    {
        std::ofstream file(tempPath, std::ios::binary);
        uint32_t badMagic = 0xDEADBEEF;
        file.write(reinterpret_cast<const char*>(&badMagic), 4);
        // Write some more bytes to meet minimum size
        for (int i = 0; i < 20; ++i) {
            uint8_t zero = 0;
            file.write(reinterpret_cast<const char*>(&zero), 1);
        }
    }
    
    AddressManager addrman("main");
    EXPECT_FALSE(addrman.Load(tempPath));
    
    // Cleanup
    std::remove(tempPath.c_str());
}

TEST(AddressManagerTest, LoadNetworkMismatch) {
    std::string tempPath = "/tmp/shurium_peers_test_" + 
                           std::to_string(std::time(nullptr)) + "_" +
                           std::to_string(rand()) + ".dat";
    
    // Save with mainnet
    {
        AddressManager addrman("main");
        std::array<uint8_t, 4> ip = {8, 8, 8, 8};
        PeerAddress addr(NetService(NetAddress(ip), 8333), GetAdjustedTime(), ServiceFlags::NETWORK);
        addrman.Add(addr, NetService());
        EXPECT_TRUE(addrman.Save(tempPath));
    }
    
    // Try to load with testnet - should fail
    {
        AddressManager addrman("test");
        EXPECT_FALSE(addrman.Load(tempPath));
    }
    
    // Cleanup
    std::remove(tempPath.c_str());
}

TEST(AddressManagerTest, SaveMultipleTimes) {
    std::string tempPath = "/tmp/shurium_peers_test_" + 
                           std::to_string(std::time(nullptr)) + "_" +
                           std::to_string(rand()) + ".dat";
    
    AddressManager addrman("main");
    
    // Add an address and save
    std::array<uint8_t, 4> ip1 = {8, 8, 8, 8};
    PeerAddress addr1(NetService(NetAddress(ip1), 8333), GetAdjustedTime(), ServiceFlags::NETWORK);
    addrman.Add(addr1, NetService());
    EXPECT_TRUE(addrman.Save(tempPath));
    
    // Add another address and save again
    std::array<uint8_t, 4> ip2 = {1, 1, 1, 1};
    PeerAddress addr2(NetService(NetAddress(ip2), 8333), GetAdjustedTime(), ServiceFlags::NETWORK);
    addrman.Add(addr2, NetService());
    EXPECT_TRUE(addrman.Save(tempPath));
    
    // Load and verify both addresses are present
    AddressManager addrman2("main");
    EXPECT_TRUE(addrman2.Load(tempPath));
    EXPECT_EQ(addrman2.Size(), 2);
    
    // Cleanup
    std::remove(tempPath.c_str());
}

// ============================================================================
// Message Validation Tests
// ============================================================================

TEST(MessageValidationTest, ValidateCommandValid) {
    // Known commands should be valid
    EXPECT_TRUE(ValidateCommand(NetMsgType::VERSION).valid);
    EXPECT_TRUE(ValidateCommand(NetMsgType::VERACK).valid);
    EXPECT_TRUE(ValidateCommand(NetMsgType::PING).valid);
    EXPECT_TRUE(ValidateCommand(NetMsgType::PONG).valid);
    EXPECT_TRUE(ValidateCommand(NetMsgType::INV).valid);
    EXPECT_TRUE(ValidateCommand(NetMsgType::BLOCK).valid);
    EXPECT_TRUE(ValidateCommand(NetMsgType::TX).valid);
}

TEST(MessageValidationTest, ValidateCommandUnknown) {
    // Unknown commands are allowed (for protocol extensions)
    auto result = ValidateCommand("unknowncmd");
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.misbehaviorScore, 0);
}

TEST(MessageValidationTest, ValidateCommandEmpty) {
    auto result = ValidateCommand("");
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.misbehaviorScore, 0);
}

TEST(MessageValidationTest, ValidateCommandInvalidChars) {
    // Commands with invalid characters should be rejected
    auto result = ValidateCommand("bad@cmd");
    EXPECT_FALSE(result.valid);
    
    result = ValidateCommand("bad cmd");  // Space
    EXPECT_FALSE(result.valid);
    
    result = ValidateCommand("bad\ncmd");  // Newline
    EXPECT_FALSE(result.valid);
}

TEST(MessageValidationTest, ValidatePayloadSizeVersion) {
    // Version message size limits
    EXPECT_FALSE(ValidatePayloadSize(NetMsgType::VERSION, 30).valid);  // Too small
    EXPECT_TRUE(ValidatePayloadSize(NetMsgType::VERSION, 100).valid);  // Good
    EXPECT_FALSE(ValidatePayloadSize(NetMsgType::VERSION, 5000).valid);  // Too large
}

TEST(MessageValidationTest, ValidatePayloadSizeVerack) {
    // Verack should have no payload
    EXPECT_TRUE(ValidatePayloadSize(NetMsgType::VERACK, 0).valid);
    EXPECT_FALSE(ValidatePayloadSize(NetMsgType::VERACK, 1).valid);
}

TEST(MessageValidationTest, ValidatePayloadSizePing) {
    // Ping can be 0 or 8 bytes
    EXPECT_TRUE(ValidatePayloadSize(NetMsgType::PING, 0).valid);
    EXPECT_TRUE(ValidatePayloadSize(NetMsgType::PING, 8).valid);
    EXPECT_FALSE(ValidatePayloadSize(NetMsgType::PING, 4).valid);
    EXPECT_FALSE(ValidatePayloadSize(NetMsgType::PING, 16).valid);
}

TEST(MessageValidationTest, ValidatePayloadSizeBlock) {
    // Block can be up to max protocol size
    EXPECT_TRUE(ValidatePayloadSize(NetMsgType::BLOCK, 1000).valid);
    EXPECT_TRUE(ValidatePayloadSize(NetMsgType::BLOCK, 1000000).valid);
    EXPECT_FALSE(ValidatePayloadSize(NetMsgType::BLOCK, MAX_PROTOCOL_MESSAGE_LENGTH + 1).valid);
}

TEST(MessageValidationTest, IsValidInvType) {
    EXPECT_TRUE(IsValidInvType(InvType::MSG_TX));
    EXPECT_TRUE(IsValidInvType(InvType::MSG_BLOCK));
    EXPECT_TRUE(IsValidInvType(InvType::MSG_FILTERED_BLOCK));
    EXPECT_TRUE(IsValidInvType(InvType::MSG_POUW_SOLUTION));
    EXPECT_FALSE(IsValidInvType(static_cast<InvType>(999)));
}

TEST(MessageValidationTest, IsReasonableTimestamp) {
    int64_t now = GetTime();
    
    // Current time is valid
    EXPECT_TRUE(IsReasonableTimestamp(now));
    
    // 1 hour ago is valid
    EXPECT_TRUE(IsReasonableTimestamp(now - 3600));
    
    // 1 hour in the future is valid
    EXPECT_TRUE(IsReasonableTimestamp(now + 3600));
    
    // 1 year ago is invalid (default max age is 1 week)
    EXPECT_FALSE(IsReasonableTimestamp(now - 365 * 24 * 3600));
    
    // 1 day in the future is invalid (default max future is 2 hours)
    EXPECT_FALSE(IsReasonableTimestamp(now + 24 * 3600));
}

TEST(MessageValidationTest, SanitizeUserAgent) {
    // Normal user agent passes through
    EXPECT_EQ(SanitizeUserAgent("/SHURIUM:0.1.0/"), "/SHURIUM:0.1.0/");
    
    // Control characters are removed
    std::string withControl = "Test";
    withControl += '\x01';
    withControl += '\x02';
    withControl += "Agent";
    std::string sanitized = SanitizeUserAgent(withControl);
    // Verify control chars are removed (result is shorter or same content without them)
    EXPECT_TRUE(sanitized.find('\x01') == std::string::npos);
    EXPECT_TRUE(sanitized.find('\x02') == std::string::npos);
    
    // Newlines are removed
    EXPECT_TRUE(SanitizeUserAgent("Test\nAgent").find('\n') == std::string::npos);
    
    // Tabs are converted to spaces
    EXPECT_EQ(SanitizeUserAgent("Test\tAgent"), "Test Agent");
    
    // High bytes are removed
    std::string highBytes = "Test";
    highBytes += '\x80';
    highBytes += "Agent";
    sanitized = SanitizeUserAgent(highBytes);
    EXPECT_TRUE(sanitized.find('\x80') == std::string::npos);
    
    // Truncation to max length
    std::string longAgent(300, 'A');
    sanitized = SanitizeUserAgent(longAgent);
    EXPECT_EQ(sanitized.size(), MAX_SUBVERSION_LENGTH);
}

TEST(MessageValidationTest, ValidateVersionMessage) {
    VersionMessage version;
    version.version = PROTOCOL_VERSION;
    version.services = ServiceFlags::NETWORK;
    version.timestamp = GetTime();
    version.userAgent = "/SHURIUM:0.1.0/";
    version.startHeight = 100;
    version.relay = true;
    
    // Valid version
    EXPECT_TRUE(ValidateVersionMessage(version).valid);
    
    // Old protocol version
    version.version = MIN_PEER_PROTO_VERSION - 1;
    EXPECT_FALSE(ValidateVersionMessage(version).valid);
    version.version = PROTOCOL_VERSION;
    
    // Negative start height
    version.startHeight = -1;
    EXPECT_FALSE(ValidateVersionMessage(version).valid);
    version.startHeight = 100;
    
    // Very old timestamp
    version.timestamp = GetTime() - 365 * 24 * 3600;  // 1 year ago
    EXPECT_FALSE(ValidateVersionMessage(version).valid);
    version.timestamp = GetTime();
    
    // Future timestamp
    version.timestamp = GetTime() + 24 * 3600;  // 1 day in future
    EXPECT_FALSE(ValidateVersionMessage(version).valid);
}

} // namespace
} // namespace shurium
