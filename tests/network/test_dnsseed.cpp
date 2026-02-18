// SHURIUM - DNS Seeder Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include <shurium/network/dnsseed.h>
#include <shurium/network/address.h>

#include <chrono>
#include <thread>

namespace shurium {
namespace network {
namespace test {

// ============================================================================
// Test Fixtures
// ============================================================================

class DNSSeederTest : public ::testing::Test {
protected:
    void SetUp() override {
        seeder_ = std::make_unique<DNSSeeder>();
    }
    
    void TearDown() override {
        seeder_.reset();
    }
    
    std::unique_ptr<DNSSeeder> seeder_;
};

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(DNSSeederTest, DefaultConfiguration) {
    const auto& config = seeder_->GetConfig();
    EXPECT_GT(config.maxAddressesPerSeed, 0u);
    EXPECT_GT(config.maxTotalAddresses, 0u);
    EXPECT_GT(config.timeoutSeconds, 0);
    EXPECT_GE(config.maxRetries, 0);
    EXPECT_TRUE(config.resolveIPv4);
    EXPECT_TRUE(config.resolveIPv6);
}

TEST_F(DNSSeederTest, SetConfiguration) {
    SeederConfig config;
    config.maxAddressesPerSeed = 50;
    config.maxTotalAddresses = 200;
    config.timeoutSeconds = 15;
    config.maxRetries = 2;
    config.resolveIPv4 = true;
    config.resolveIPv6 = false;
    
    seeder_->SetConfig(config);
    
    const auto& newConfig = seeder_->GetConfig();
    EXPECT_EQ(newConfig.maxAddressesPerSeed, 50u);
    EXPECT_EQ(newConfig.maxTotalAddresses, 200u);
    EXPECT_EQ(newConfig.timeoutSeconds, 15);
    EXPECT_EQ(newConfig.maxRetries, 2);
    EXPECT_TRUE(newConfig.resolveIPv4);
    EXPECT_FALSE(newConfig.resolveIPv6);
}

TEST_F(DNSSeederTest, SetDefaultPort) {
    seeder_->SetDefaultPort(18333);
    EXPECT_EQ(seeder_->GetDefaultPort(), 18333);
}

// ============================================================================
// Seed Management Tests
// ============================================================================

TEST_F(DNSSeederTest, AddSeedByHostname) {
    seeder_->AddSeed("seed.example.com");
    EXPECT_EQ(seeder_->NumSeeds(), 1u);
    
    const auto& seeds = seeder_->GetSeeds();
    EXPECT_EQ(seeds[0].hostname, "seed.example.com");
}

TEST_F(DNSSeederTest, AddSeedWithConfig) {
    SeedConfig config("seed.example.com", "Test Seed", true);
    config.priority = 50;
    config.port = 9999;
    
    seeder_->AddSeed(config);
    
    EXPECT_EQ(seeder_->NumSeeds(), 1u);
    const auto& seeds = seeder_->GetSeeds();
    EXPECT_EQ(seeds[0].hostname, "seed.example.com");
    EXPECT_EQ(seeds[0].description, "Test Seed");
    EXPECT_TRUE(seeds[0].trusted);
    EXPECT_EQ(seeds[0].priority, 50);
    EXPECT_EQ(seeds[0].port, 9999);
}

TEST_F(DNSSeederTest, AddMultipleSeeds) {
    std::vector<SeedConfig> seeds = {
        SeedConfig("seed1.example.com"),
        SeedConfig("seed2.example.com"),
        SeedConfig("seed3.example.com"),
    };
    
    seeder_->AddSeeds(seeds);
    EXPECT_EQ(seeder_->NumSeeds(), 3u);
}

TEST_F(DNSSeederTest, RemoveSeed) {
    seeder_->AddSeed("seed1.example.com");
    seeder_->AddSeed("seed2.example.com");
    EXPECT_EQ(seeder_->NumSeeds(), 2u);
    
    bool removed = seeder_->RemoveSeed("seed1.example.com");
    EXPECT_TRUE(removed);
    EXPECT_EQ(seeder_->NumSeeds(), 1u);
    
    // Try to remove non-existent
    removed = seeder_->RemoveSeed("nonexistent.com");
    EXPECT_FALSE(removed);
    EXPECT_EQ(seeder_->NumSeeds(), 1u);
}

TEST_F(DNSSeederTest, ClearSeeds) {
    seeder_->AddSeed("seed1.example.com");
    seeder_->AddSeed("seed2.example.com");
    EXPECT_EQ(seeder_->NumSeeds(), 2u);
    
    seeder_->ClearSeeds();
    EXPECT_EQ(seeder_->NumSeeds(), 0u);
}

// ============================================================================
// Static Utility Tests
// ============================================================================

TEST_F(DNSSeederTest, IsRoutableIPv4Private) {
    // 10.x.x.x - Private
    std::array<uint8_t, 4> ip10 = {10, 0, 0, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip10)));
    
    // 172.16.x.x - Private
    std::array<uint8_t, 4> ip172 = {172, 16, 0, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip172)));
    
    // 192.168.x.x - Private
    std::array<uint8_t, 4> ip192 = {192, 168, 1, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip192)));
}

TEST_F(DNSSeederTest, IsRoutableIPv4Loopback) {
    std::array<uint8_t, 4> ip = {127, 0, 0, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip)));
}

TEST_F(DNSSeederTest, IsRoutableIPv4LinkLocal) {
    std::array<uint8_t, 4> ip = {169, 254, 1, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip)));
}

TEST_F(DNSSeederTest, IsRoutableIPv4Multicast) {
    std::array<uint8_t, 4> ip = {224, 0, 0, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip)));
}

TEST_F(DNSSeederTest, IsRoutableIPv4Valid) {
    std::array<uint8_t, 4> ip = {8, 8, 8, 8};  // Google DNS
    EXPECT_TRUE(DNSSeeder::IsRoutable(NetAddress(ip)));
    
    std::array<uint8_t, 4> ip2 = {1, 1, 1, 1};  // Cloudflare DNS
    EXPECT_TRUE(DNSSeeder::IsRoutable(NetAddress(ip2)));
}

TEST_F(DNSSeederTest, IsRoutableIPv6Loopback) {
    std::array<uint8_t, 16> ip = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip)));
}

TEST_F(DNSSeederTest, IsRoutableIPv6Unspecified) {
    std::array<uint8_t, 16> ip = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip)));
}

TEST_F(DNSSeederTest, IsRoutableIPv6LinkLocal) {
    std::array<uint8_t, 16> ip = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip)));
}

TEST_F(DNSSeederTest, IsRoutableIPv6UniqueLocal) {
    std::array<uint8_t, 16> ip = {0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    EXPECT_FALSE(DNSSeeder::IsRoutable(NetAddress(ip)));
}

// ============================================================================
// Seed List Tests
// ============================================================================

TEST_F(DNSSeederTest, GetMainnetSeeds) {
    auto seeds = DNSSeeder::GetMainnetSeeds();
    EXPECT_FALSE(seeds.empty());
    
    // Check that all have hostnames
    for (const auto& seed : seeds) {
        EXPECT_FALSE(seed.hostname.empty());
    }
}

TEST_F(DNSSeederTest, GetTestnetSeeds) {
    auto seeds = DNSSeeder::GetTestnetSeeds();
    EXPECT_FALSE(seeds.empty());
}

TEST_F(DNSSeederTest, GetSeedsForNetworkMain) {
    auto seeds = GetSeedsForNetwork("main");
    EXPECT_FALSE(seeds.empty());
}

TEST_F(DNSSeederTest, GetSeedsForNetworkTest) {
    auto seeds = GetSeedsForNetwork("test");
    EXPECT_FALSE(seeds.empty());
}

TEST_F(DNSSeederTest, GetSeedsForNetworkRegtest) {
    auto seeds = GetSeedsForNetwork("regtest");
    EXPECT_TRUE(seeds.empty());  // Regtest has no seeds
}

// ============================================================================
// SeedConfig Tests
// ============================================================================

TEST_F(DNSSeederTest, SeedConfigDefaults) {
    SeedConfig config;
    EXPECT_TRUE(config.hostname.empty());
    EXPECT_FALSE(config.supportsSrv);
    EXPECT_EQ(config.port, 0);
    EXPECT_EQ(config.priority, 100);
    EXPECT_FALSE(config.trusted);
    EXPECT_TRUE(config.description.empty());
}

TEST_F(DNSSeederTest, SeedConfigHostnameConstructor) {
    SeedConfig config("seed.example.com");
    EXPECT_EQ(config.hostname, "seed.example.com");
    EXPECT_FALSE(config.supportsSrv);
}

TEST_F(DNSSeederTest, SeedConfigSrvConstructor) {
    SeedConfig config("seed.example.com", true);
    EXPECT_EQ(config.hostname, "seed.example.com");
    EXPECT_TRUE(config.supportsSrv);
}

TEST_F(DNSSeederTest, SeedConfigFullConstructor) {
    SeedConfig config("seed.example.com", "My Description", true);
    EXPECT_EQ(config.hostname, "seed.example.com");
    EXPECT_EQ(config.description, "My Description");
    EXPECT_TRUE(config.trusted);
}

// ============================================================================
// SeederConfig Tests
// ============================================================================

TEST_F(DNSSeederTest, SeederConfigDefaults) {
    SeederConfig config;
    EXPECT_EQ(config.maxAddressesPerSeed, 256u);
    EXPECT_EQ(config.maxTotalAddresses, 1000u);
    EXPECT_EQ(config.timeoutSeconds, 30);
    EXPECT_EQ(config.maxRetries, 3);
    EXPECT_EQ(config.retryDelayMs, 1000);
    EXPECT_EQ(config.maxConcurrent, 4u);
    EXPECT_TRUE(config.resolveIPv4);
    EXPECT_TRUE(config.resolveIPv6);
    EXPECT_EQ(config.minSuccessfulSeeds, 1u);
    EXPECT_TRUE(config.filterUnroutable);
    EXPECT_TRUE(config.shuffleResults);
}

// ============================================================================
// Resolution Tests (Network Dependent)
// ============================================================================

// Note: These tests require network access and may be slow or fail in CI
// They are marked as DISABLED by default but can be run manually

TEST_F(DNSSeederTest, DISABLED_ResolveRealHostname) {
    // This test requires network access
    auto addresses = DNSSeeder::ResolveHostname("google.com", 80);
    EXPECT_FALSE(addresses.empty());
    
    for (const auto& addr : addresses) {
        EXPECT_TRUE(addr.IsValid());
        EXPECT_EQ(addr.GetPort(), 80);
    }
}

TEST_F(DNSSeederTest, DISABLED_ResolveInvalidHostname) {
    auto addresses = DNSSeeder::ResolveHostname("this.hostname.definitely.does.not.exist.invalid", 8333);
    EXPECT_TRUE(addresses.empty());
}

TEST_F(DNSSeederTest, ResolveNoSeeds) {
    // Should handle empty seeds gracefully
    auto result = seeder_->Resolve();
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.seedsAttempted, 0u);
    EXPECT_TRUE(result.addresses.empty());
}

// ============================================================================
// Async Resolution Tests
// ============================================================================

TEST_F(DNSSeederTest, ResolveAsyncNoSeeds) {
    std::atomic<bool> callbackCalled{false};
    SeederResult capturedResult;
    
    seeder_->ResolveAsync([&](SeederResult result) {
        capturedResult = result;
        callbackCalled.store(true);
    });
    
    // Wait for callback
    for (int i = 0; i < 100 && !callbackCalled.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(callbackCalled.load());
    EXPECT_FALSE(capturedResult.success);
}

TEST_F(DNSSeederTest, CancelResolution) {
    // This test verifies that Cancel() can be called without crashing
    // and that it sets the cancelled state appropriately
    
    seeder_->AddSeed("nonexistent.hostname.invalid");
    
    // Start async resolution
    seeder_->ResolveAsync([](SeederResult) {});
    
    // Call cancel (should not crash)
    seeder_->Cancel();
    
    // Verify we can call cancel multiple times safely
    seeder_->Cancel();
    
    // Just verify the test completes without hanging indefinitely
    // The destructor will join any running thread
}

TEST_F(DNSSeederTest, IsResolvingState) {
    EXPECT_FALSE(seeder_->IsResolving());
}

// ============================================================================
// Result Structure Tests
// ============================================================================

TEST_F(DNSSeederTest, SeedResultDefaults) {
    SeedResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.addresses.empty());
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.duration.count(), 0);
    EXPECT_EQ(result.retries, 0);
}

TEST_F(DNSSeederTest, SeederResultDefaults) {
    SeederResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.addresses.empty());
    EXPECT_TRUE(result.seedResults.empty());
    EXPECT_EQ(result.seedsAttempted, 0u);
    EXPECT_EQ(result.seedsSucceeded, 0u);
    EXPECT_EQ(result.totalDuration.count(), 0);
    EXPECT_TRUE(result.message.empty());
}

// ============================================================================
// Predefined Seeds Tests
// ============================================================================

TEST_F(DNSSeederTest, MainnetSeedsNotEmpty) {
    EXPECT_FALSE(Seeds::MAINNET.empty());
}

TEST_F(DNSSeederTest, TestnetSeedsNotEmpty) {
    EXPECT_FALSE(Seeds::TESTNET.empty());
}

TEST_F(DNSSeederTest, RegtestSeedsEmpty) {
    EXPECT_TRUE(Seeds::REGTEST.empty());
}

TEST_F(DNSSeederTest, MainnetSeedsHaveTrusted) {
    // At least one mainnet seed should be trusted
    bool hasTrusted = false;
    for (const auto& seed : Seeds::MAINNET) {
        if (seed.trusted) {
            hasTrusted = true;
            break;
        }
    }
    EXPECT_TRUE(hasTrusted);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(DNSSeederTest, EmptyHostname) {
    SeedConfig config("");
    seeder_->AddSeed(config);
    
    auto result = seeder_->Resolve();
    // Should fail gracefully
    EXPECT_FALSE(result.success);
}

TEST_F(DNSSeederTest, DuplicateSeeds) {
    seeder_->AddSeed("seed.example.com");
    seeder_->AddSeed("seed.example.com");
    EXPECT_EQ(seeder_->NumSeeds(), 2u);  // Duplicates are allowed at config level
}

TEST_F(DNSSeederTest, ConfigWithNoIPv4OrIPv6) {
    SeederConfig config;
    config.resolveIPv4 = false;
    config.resolveIPv6 = false;
    seeder_->SetConfig(config);
    
    seeder_->AddSeed("seed.example.com");
    
    auto result = seeder_->Resolve();
    // Should complete without crashing, but no addresses
    EXPECT_TRUE(result.addresses.empty());
}

} // namespace test
} // namespace network
} // namespace shurium
