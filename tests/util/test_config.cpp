// SHURIUM - Configuration File Parser Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include "shurium/util/config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace shurium {
namespace util {
namespace test {

// ============================================================================
// Test Fixtures
// ============================================================================

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.Clear();
    }
    
    void TearDown() override {
        // Clean up any test files
        for (const auto& file : tempFiles_) {
            std::remove(file.c_str());
        }
        tempFiles_.clear();
    }
    
    std::string CreateTempFile(const std::string& content) {
        char filename[] = "/tmp/shurium_config_test_XXXXXX";
        int fd = mkstemp(filename);
        if (fd < 0) {
            throw std::runtime_error("Failed to create temp file");
        }
        close(fd);
        
        std::ofstream file(filename);
        file << content;
        file.close();
        
        tempFiles_.push_back(filename);
        return filename;
    }
    
    ConfigManager config_;
    std::vector<std::string> tempFiles_;
};

// ============================================================================
// Basic Parsing Tests
// ============================================================================

TEST_F(ConfigTest, ParseEmptyString) {
    auto result = config_.ParseString("");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.Size(), 0);
}

TEST_F(ConfigTest, ParseComments) {
    std::string content = R"(
# This is a comment
; This is also a comment
# key=value
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.Size(), 0);
}

TEST_F(ConfigTest, ParseKeyValuePair) {
    auto result = config_.ParseString("key=value");
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(config_.HasKey("key"));
    EXPECT_EQ(config_.GetString("key", "default"), "value");
}

TEST_F(ConfigTest, ParseMultipleKeyValuePairs) {
    std::string content = R"(
key1=value1
key2=value2
key3=value3
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.Size(), 3);
    EXPECT_EQ(config_.GetString("key1", "default"), "value1");
    EXPECT_EQ(config_.GetString("key2", "default"), "value2");
    EXPECT_EQ(config_.GetString("key3", "default"), "value3");
}

TEST_F(ConfigTest, ParseKeyWithSpaces) {
    std::string content = R"(
  key1  =  value1  
key2 = value2
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.GetString("key1", "default"), "value1");
    EXPECT_EQ(config_.GetString("key2", "default"), "value2");
}

TEST_F(ConfigTest, ParseQuotedValue) {
    std::string content = R"(
key1="value with spaces"
key2='single quoted'
key3="with \"escaped\" quotes"
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.GetString("key1", "default"), "value with spaces");
    EXPECT_EQ(config_.GetString("key2", "default"), "single quoted");
    EXPECT_EQ(config_.GetString("key3", "default"), "with \"escaped\" quotes");
}

TEST_F(ConfigTest, ParseEscapeSequences) {
    std::string content = R"(key="line1\nline2\ttabbed")";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.GetString("key", "default"), "line1\nline2\ttabbed");
}

TEST_F(ConfigTest, ParseBooleanFlag) {
    std::string content = R"(
enabled
disabled=false
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(config_.GetBool("enabled", false));
    EXPECT_FALSE(config_.GetBool("disabled", true));
}

TEST_F(ConfigTest, ParseNegatedFlag) {
    std::string content = "nodebug";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(config_.GetBool("debug", true));
}

// ============================================================================
// Section Tests
// ============================================================================

TEST_F(ConfigTest, ParseSection) {
    std::string content = R"(
global=globalvalue

[section1]
key1=value1

[section2]
key2=value2
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    EXPECT_EQ(config_.GetString("global", "default"), "globalvalue");
    EXPECT_EQ(config_.GetString("key1", "default", "section1"), "value1");
    EXPECT_EQ(config_.GetString("key2", "default", "section2"), "value2");
}

TEST_F(ConfigTest, GetSections) {
    std::string content = R"(
[network]
port=8333

[rpc]
port=8332

[wallet]
disabled=false
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    auto sections = config_.GetSections();
    EXPECT_EQ(sections.size(), 3);
    EXPECT_TRUE(std::find(sections.begin(), sections.end(), "network") != sections.end());
    EXPECT_TRUE(std::find(sections.begin(), sections.end(), "rpc") != sections.end());
    EXPECT_TRUE(std::find(sections.begin(), sections.end(), "wallet") != sections.end());
}

TEST_F(ConfigTest, GetKeysInSection) {
    std::string content = R"(
[section]
key1=value1
key2=value2
key3=value3
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    auto keys = config_.GetKeys("section");
    EXPECT_EQ(keys.size(), 3);
}

// ============================================================================
// Type Conversion Tests
// ============================================================================

TEST_F(ConfigTest, GetInt) {
    std::string content = R"(
positive=42
negative=-100
zero=0
large=1234567890
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    EXPECT_EQ(config_.GetInt("positive", 0), 42);
    EXPECT_EQ(config_.GetInt("negative", 0), -100);
    EXPECT_EQ(config_.GetInt("zero", 99), 0);
    EXPECT_EQ(config_.GetInt("large", 0), 1234567890);
    EXPECT_EQ(config_.GetInt("missing", 999), 999);
}

TEST_F(ConfigTest, GetIntWithSuffix) {
    std::string content = R"(
kilobytes=100k
megabytes=50m
gigabytes=2g
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    EXPECT_EQ(config_.GetInt("kilobytes", 0), 100 * 1024);
    EXPECT_EQ(config_.GetInt("megabytes", 0), 50 * 1024 * 1024);
    EXPECT_EQ(config_.GetInt("gigabytes", 0), 2LL * 1024 * 1024 * 1024);
}

TEST_F(ConfigTest, GetUInt) {
    std::string content = R"(
positive=42
negative=-100
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    auto positiveOpt = config_.TryGetUInt("positive");
    EXPECT_TRUE(positiveOpt.has_value());
    EXPECT_EQ(*positiveOpt, 42u);
    
    auto negativeOpt = config_.TryGetUInt("negative");
    EXPECT_FALSE(negativeOpt.has_value());
    
    EXPECT_EQ(config_.GetUInt("missing", 999), 999u);
}

TEST_F(ConfigTest, GetBool) {
    std::string content = R"(
true1=true
true2=yes
true3=on
true4=1
false1=false
false2=no
false3=off
false4=0
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    EXPECT_TRUE(config_.GetBool("true1", false));
    EXPECT_TRUE(config_.GetBool("true2", false));
    EXPECT_TRUE(config_.GetBool("true3", false));
    EXPECT_TRUE(config_.GetBool("true4", false));
    
    EXPECT_FALSE(config_.GetBool("false1", true));
    EXPECT_FALSE(config_.GetBool("false2", true));
    EXPECT_FALSE(config_.GetBool("false3", true));
    EXPECT_FALSE(config_.GetBool("false4", true));
    
    EXPECT_TRUE(config_.GetBool("missing", true));
    EXPECT_FALSE(config_.GetBool("missing", false));
}

TEST_F(ConfigTest, GetDouble) {
    std::string content = R"(
pi=3.14159
negative=-2.5
zero=0.0
scientific=1.23e10
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    EXPECT_NEAR(config_.GetDouble("pi", 0.0), 3.14159, 0.00001);
    EXPECT_NEAR(config_.GetDouble("negative", 0.0), -2.5, 0.00001);
    EXPECT_NEAR(config_.GetDouble("zero", 99.0), 0.0, 0.00001);
    EXPECT_NEAR(config_.GetDouble("scientific", 0.0), 1.23e10, 1e5);
    EXPECT_NEAR(config_.GetDouble("missing", 99.5), 99.5, 0.00001);
}

TEST_F(ConfigTest, GetList) {
    std::string content = R"(
addnode=192.168.1.1:8333
addnode=192.168.1.2:8333
addnode=192.168.1.3:8333
csv=a,b,c,d
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    auto nodes = config_.GetList("addnode");
    EXPECT_EQ(nodes.size(), 3);
    EXPECT_EQ(nodes[0], "192.168.1.1:8333");
    
    auto csv = config_.GetList("csv");
    EXPECT_EQ(csv.size(), 4);
    EXPECT_EQ(csv[0], "a");
    EXPECT_EQ(csv[1], "b");
    EXPECT_EQ(csv[2], "c");
    EXPECT_EQ(csv[3], "d");
}

// ============================================================================
// Environment Variable Expansion Tests
// ============================================================================

TEST_F(ConfigTest, ExpandEnvVarsBraced) {
    setenv("TEST_VAR", "test_value", 1);
    
    std::string result = ConfigManager::ExpandEnvVars("prefix_${TEST_VAR}_suffix");
    EXPECT_EQ(result, "prefix_test_value_suffix");
    
    unsetenv("TEST_VAR");
}

TEST_F(ConfigTest, ExpandEnvVarsUnbraced) {
    setenv("TESTVAR", "test_value", 1);
    
    std::string result = ConfigManager::ExpandEnvVars("prefix_$TESTVAR/suffix");
    EXPECT_EQ(result, "prefix_test_value/suffix");
    
    unsetenv("TESTVAR");
}

TEST_F(ConfigTest, ExpandEnvVarsUndefined) {
    unsetenv("UNDEFINED_VAR");
    
    std::string result = ConfigManager::ExpandEnvVars("prefix_${UNDEFINED_VAR}_suffix");
    EXPECT_EQ(result, "prefix__suffix");
}

TEST_F(ConfigTest, ExpandEnvVarsInConfig) {
    setenv("SHURIUM_DATA", "/custom/data", 1);
    
    std::string content = "datadir=${SHURIUM_DATA}/subdir";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.GetString("datadir", "default"), "/custom/data/subdir");
    
    unsetenv("SHURIUM_DATA");
}

// ============================================================================
// Tilde Expansion Tests
// ============================================================================

TEST_F(ConfigTest, ExpandTilde) {
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    if (!home.empty()) {
        std::string result = ConfigManager::ExpandTilde("~/subdir/file");
        EXPECT_EQ(result, home + "/subdir/file");
    }
}

TEST_F(ConfigTest, ExpandTildeAlone) {
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    if (!home.empty()) {
        std::string result = ConfigManager::ExpandTilde("~");
        EXPECT_EQ(result, home);
    }
}

TEST_F(ConfigTest, NoExpandTildeInMiddle) {
    std::string result = ConfigManager::ExpandTilde("/path/to/~something");
    EXPECT_EQ(result, "/path/to/~something");
}

TEST_F(ConfigTest, GetPath) {
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    if (!home.empty()) {
        config_.Set("path", "~/test/path");
        std::string result = config_.GetPath("path");
        EXPECT_EQ(result, home + "/test/path");
    }
}

// ============================================================================
// Line Continuation Tests
// ============================================================================

TEST_F(ConfigTest, LineContinuation) {
    std::string content = "longvalue=first \\\nsecond \\\nthird";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.GetString("longvalue", "default"), "first second third");
}

// ============================================================================
// File Parsing Tests
// ============================================================================

TEST_F(ConfigTest, ParseFile) {
    std::string content = R"(
# Test config file
server=1
rpcuser=testuser
rpcpassword=testpass
port=8333
)";
    std::string filename = CreateTempFile(content);
    
    auto result = config_.ParseFile(filename);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.GetString("server", "default"), "1");
    EXPECT_EQ(config_.GetString("rpcuser", "default"), "testuser");
    EXPECT_EQ(config_.GetString("rpcpassword", "default"), "testpass");
    EXPECT_EQ(config_.GetInt("port", 0), 8333);
}

TEST_F(ConfigTest, ParseNonexistentFile) {
    auto result = config_.ParseFile("/nonexistent/path/config.conf");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ConfigTest, IncludeFile) {
    // Create included file
    std::string includedContent = "includedkey=includedvalue";
    std::string includedFile = CreateTempFile(includedContent);
    
    // Create main file with include directive
    std::string mainContent = "mainkey=mainvalue\ninclude " + includedFile;
    std::string mainFile = CreateTempFile(mainContent);
    
    auto result = config_.ParseFile(mainFile);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.GetString("mainkey", "default"), "mainvalue");
    EXPECT_EQ(config_.GetString("includedkey", "default"), "includedvalue");
}

// ============================================================================
// Command Line Parsing Tests
// ============================================================================

TEST_F(ConfigTest, ParseCommandLineBasic) {
    char* argv[] = {
        const_cast<char*>("shuriumd"),
        const_cast<char*>("-server"),
        const_cast<char*>("-port=8333"),
        const_cast<char*>("--rpcuser=admin"),
        nullptr
    };
    int argc = 4;
    
    auto result = config_.ParseCommandLine(argc, argv);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(config_.GetBool("server", false));
    EXPECT_EQ(config_.GetInt("port", 0), 8333);
    EXPECT_EQ(config_.GetString("rpcuser", "default"), "admin");
}

TEST_F(ConfigTest, ParseCommandLineNegated) {
    char* argv[] = {
        const_cast<char*>("shuriumd"),
        const_cast<char*>("-nodebug"),
        const_cast<char*>("-nolisten"),
        nullptr
    };
    int argc = 3;
    
    auto result = config_.ParseCommandLine(argc, argv);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(config_.GetBool("debug", true));
    EXPECT_FALSE(config_.GetBool("listen", true));
}

TEST_F(ConfigTest, ParseCommandLineValueAsNextArg) {
    char* argv[] = {
        const_cast<char*>("shuriumd"),
        const_cast<char*>("-datadir"),
        const_cast<char*>("/custom/data"),
        nullptr
    };
    int argc = 3;
    
    auto result = config_.ParseCommandLine(argc, argv);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(config_.GetString("datadir", "default"), "/custom/data");
}

// ============================================================================
// Value Setting Tests
// ============================================================================

TEST_F(ConfigTest, SetValue) {
    config_.Set("key", "value");
    EXPECT_TRUE(config_.HasKey("key"));
    EXPECT_EQ(config_.GetString("key", "default"), "value");
}

TEST_F(ConfigTest, SetOverwrites) {
    config_.Set("key", "original");
    EXPECT_EQ(config_.GetString("key", "default"), "original");
    
    config_.Set("key", "updated");
    EXPECT_EQ(config_.GetString("key", "default"), "updated");
}

TEST_F(ConfigTest, SetDefault) {
    config_.SetDefault("key", "default");
    EXPECT_EQ(config_.GetString("key", "other"), "default");
    
    config_.Set("key", "explicit");
    EXPECT_EQ(config_.GetString("key", "other"), "explicit");
    
    // SetDefault should not overwrite explicit value
    config_.SetDefault("key", "another_default");
    EXPECT_EQ(config_.GetString("key", "other"), "explicit");
}

TEST_F(ConfigTest, SetInSection) {
    config_.Set("key", "global_value");
    config_.Set("key", "section_value", "mysection");
    
    EXPECT_EQ(config_.GetString("key", "default"), "global_value");
    EXPECT_EQ(config_.GetString("key", "default", "mysection"), "section_value");
}

TEST_F(ConfigTest, AddToList) {
    config_.AddToList("nodes", "192.168.1.1");
    config_.AddToList("nodes", "192.168.1.2");
    config_.AddToList("nodes", "192.168.1.3");
    
    auto nodes = config_.GetList("nodes");
    EXPECT_EQ(nodes.size(), 3);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(ConfigTest, RequiredKeyMissing) {
    config_.RequireKey("required");
    
    auto errors = config_.Validate();
    EXPECT_EQ(errors.size(), 1);
    EXPECT_TRUE(errors[0].find("required") != std::string::npos);
}

TEST_F(ConfigTest, RequiredKeyPresent) {
    config_.RequireKey("required");
    config_.Set("required", "value");
    
    auto errors = config_.Validate();
    EXPECT_TRUE(errors.empty());
}

TEST_F(ConfigTest, UnknownKeyWarning) {
    config_.AllowKey("allowed");
    config_.Set("allowed", "value");
    config_.Set("unknown", "value");
    
    auto errors = config_.Validate();
    EXPECT_EQ(errors.size(), 1);
    EXPECT_TRUE(errors[0].find("unknown") != std::string::npos);
}

// ============================================================================
// Utility Tests
// ============================================================================

TEST_F(ConfigTest, Clear) {
    config_.Set("key1", "value1");
    config_.Set("key2", "value2");
    EXPECT_EQ(config_.Size(), 2);
    
    config_.Clear();
    EXPECT_EQ(config_.Size(), 0);
    EXPECT_FALSE(config_.HasKey("key1"));
}

TEST_F(ConfigTest, GetDataDir) {
    // Default should return something
    std::string defaultDir = config_.GetDataDir();
    EXPECT_FALSE(defaultDir.empty());
}

TEST_F(ConfigTest, SetDataDir) {
    config_.SetDataDir("/custom/data");
    EXPECT_EQ(config_.GetDataDir(), "/custom/data");
}

TEST_F(ConfigTest, GenerateSampleConfig) {
    std::string sample = config_.GenerateSampleConfig();
    EXPECT_FALSE(sample.empty());
    EXPECT_TRUE(sample.find("datadir") != std::string::npos);
    EXPECT_TRUE(sample.find("rpcuser") != std::string::npos);
}

TEST_F(ConfigTest, Dump) {
    config_.Set("key1", "value1");
    config_.Set("key2", "value2");
    
    std::string dump = config_.Dump();
    EXPECT_FALSE(dump.empty());
    EXPECT_TRUE(dump.find("key1") != std::string::npos);
    EXPECT_TRUE(dump.find("key2") != std::string::npos);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ConfigTest, InvalidSectionHeader) {
    auto result = config_.ParseString("[unclosed");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.find("bracket") != std::string::npos);
}

TEST_F(ConfigTest, EmptyKey) {
    auto result = config_.ParseString("=value");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.find("Empty key") != std::string::npos);
}

TEST_F(ConfigTest, InvalidKeyCharacter) {
    auto result = config_.ParseString("key with spaces=value");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.find("Invalid character") != std::string::npos);
}

TEST_F(ConfigTest, LineTooLong) {
    // Create a line that's too long
    std::string longValue(MAX_LINE_LENGTH + 100, 'x');
    std::string content = "key=" + longValue;
    
    auto result = config_.ParseString(content);
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.find("too long") != std::string::npos);
}

// ============================================================================
// ConfigEntry Tests
// ============================================================================

TEST_F(ConfigTest, ConfigEntryIsTrue) {
    ConfigEntry entry;
    entry.value = "true";
    EXPECT_TRUE(entry.IsTrue());
    
    entry.value = "yes";
    EXPECT_TRUE(entry.IsTrue());
    
    entry.value = "1";
    EXPECT_TRUE(entry.IsTrue());
    
    entry.value = "on";
    EXPECT_TRUE(entry.IsTrue());
    
    entry.value = "false";
    EXPECT_FALSE(entry.IsTrue());
}

TEST_F(ConfigTest, ConfigEntryIsFalse) {
    ConfigEntry entry;
    entry.value = "false";
    EXPECT_TRUE(entry.IsFalse());
    
    entry.value = "no";
    EXPECT_TRUE(entry.IsFalse());
    
    entry.value = "0";
    EXPECT_TRUE(entry.IsFalse());
    
    entry.value = "off";
    EXPECT_TRUE(entry.IsFalse());
    
    entry.value = "true";
    EXPECT_FALSE(entry.IsFalse());
}

// ============================================================================
// GetEntries Tests
// ============================================================================

TEST_F(ConfigTest, GetEntriesGlobalSection) {
    std::string content = R"(
key1=value1
key2=value2

[section]
key3=value3
)";
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    auto globalEntries = config_.GetEntries();
    EXPECT_EQ(globalEntries.size(), 2);
    
    auto sectionEntries = config_.GetEntries("section");
    EXPECT_EQ(sectionEntries.size(), 1);
}

// ============================================================================
// TryGet* (Optional Return Value) Tests
// ============================================================================

TEST_F(ConfigTest, TryGetStringMissing) {
    auto value = config_.TryGetString("missing");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigTest, TryGetStringPresent) {
    config_.Set("key", "value");
    auto value = config_.TryGetString("key");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, "value");
}

TEST_F(ConfigTest, TryGetIntMissing) {
    auto value = config_.TryGetInt("missing");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigTest, TryGetIntInvalid) {
    config_.Set("notanumber", "abc");
    auto value = config_.TryGetInt("notanumber");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigTest, TryGetIntValid) {
    config_.Set("number", "42");
    auto value = config_.TryGetInt("number");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);
}

TEST_F(ConfigTest, TryGetBoolMissing) {
    auto value = config_.TryGetBool("missing");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigTest, TryGetBoolInvalid) {
    config_.Set("notabool", "maybe");
    auto value = config_.TryGetBool("notabool");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigTest, TryGetBoolValid) {
    config_.Set("flag", "true");
    auto value = config_.TryGetBool("flag");
    EXPECT_TRUE(value.has_value());
    EXPECT_TRUE(*value);
}

TEST_F(ConfigTest, TryGetDoubleMissing) {
    auto value = config_.TryGetDouble("missing");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigTest, TryGetDoubleInvalid) {
    config_.Set("notadouble", "abc");
    auto value = config_.TryGetDouble("notadouble");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigTest, TryGetDoubleValid) {
    config_.Set("decimal", "3.14");
    auto value = config_.TryGetDouble("decimal");
    EXPECT_TRUE(value.has_value());
    EXPECT_NEAR(*value, 3.14, 0.001);
}

// ============================================================================
// Priority Tests
// ============================================================================

TEST_F(ConfigTest, CommandLineOverridesConfig) {
    // First parse config
    config_.ParseString("port=8333");
    EXPECT_EQ(config_.GetInt("port", 0), 8333);
    
    // Then parse command line - should override
    char* argv[] = {
        const_cast<char*>("shuriumd"),
        const_cast<char*>("-port=9999"),
        nullptr
    };
    config_.ParseCommandLine(2, argv);
    
    EXPECT_EQ(config_.GetInt("port", 0), 9999);
}

// ============================================================================
// Real-World Config Format Tests
// ============================================================================

TEST_F(ConfigTest, ParseBitcoinStyleConfig) {
    std::string content = R"(
# SHURIUM configuration file

# Network
server=1
listen=1
port=8333
maxconnections=125

# RPC
rpcuser=shuriumrpc
rpcpassword=verysecretpassword
rpcport=8332
rpcallowip=127.0.0.1
rpcallowip=192.168.1.0/24

# Wallet
disablewallet=0
keypool=100

# Debug
debug=0
printtoconsole=0

[test]
testnet=1
connect=testnode.example.com:18333
)";
    
    auto result = config_.ParseString(content);
    EXPECT_TRUE(result.success);
    
    EXPECT_TRUE(config_.GetBool("server", false));
    EXPECT_TRUE(config_.GetBool("listen", false));
    EXPECT_EQ(config_.GetInt("port", 0), 8333);
    EXPECT_EQ(config_.GetInt("maxconnections", 0), 125);
    EXPECT_EQ(config_.GetString("rpcuser", "default"), "shuriumrpc");
    EXPECT_EQ(config_.GetString("rpcpassword", "default"), "verysecretpassword");
    EXPECT_EQ(config_.GetInt("rpcport", 0), 8332);
    
    auto allowedIps = config_.GetList("rpcallowip");
    EXPECT_EQ(allowedIps.size(), 2);
    
    EXPECT_FALSE(config_.GetBool("disablewallet", true));
    EXPECT_EQ(config_.GetInt("keypool", 0), 100);
    
    // Test section
    EXPECT_TRUE(config_.GetBool("testnet", false, "test"));
    EXPECT_EQ(config_.GetString("connect", "default", "test"), "testnode.example.com:18333");
}

// ============================================================================
// Common Config Keys Tests
// ============================================================================

TEST_F(ConfigTest, ConfigKeysAreDefined) {
    // Test that all config keys are non-empty strings
    EXPECT_TRUE(std::string(ConfigKeys::DATADIR).length() > 0);
    EXPECT_TRUE(std::string(ConfigKeys::TESTNET).length() > 0);
    EXPECT_TRUE(std::string(ConfigKeys::SERVER).length() > 0);
    EXPECT_TRUE(std::string(ConfigKeys::RPCUSER).length() > 0);
    EXPECT_TRUE(std::string(ConfigKeys::RPCPASSWORD).length() > 0);
    EXPECT_TRUE(std::string(ConfigKeys::PORT).length() > 0);
    EXPECT_TRUE(std::string(ConfigKeys::MAXCONNECTIONS).length() > 0);
    EXPECT_TRUE(std::string(ConfigKeys::WALLET).length() > 0);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(ConfigTest, EmptyValueAllowed) {
    config_.ParseString("key=");
    EXPECT_TRUE(config_.HasKey("key"));
    EXPECT_EQ(config_.GetString("key", "default"), "");
}

TEST_F(ConfigTest, MultipleEquals) {
    config_.ParseString("key=value=with=equals");
    EXPECT_EQ(config_.GetString("key", "default"), "value=with=equals");
}

TEST_F(ConfigTest, SpecialCharactersInValue) {
    config_.ParseString("key=\"value with special !@#$%^&*() chars\"");
    EXPECT_EQ(config_.GetString("key", "default"), "value with special !@#$%^&*() chars");
}

TEST_F(ConfigTest, KeyWithDots) {
    config_.ParseString("key.subkey=value");
    EXPECT_TRUE(config_.HasKey("key.subkey"));
    EXPECT_EQ(config_.GetString("key.subkey", "default"), "value");
}

TEST_F(ConfigTest, KeyWithHyphens) {
    config_.ParseString("my-key=value");
    EXPECT_TRUE(config_.HasKey("my-key"));
    EXPECT_EQ(config_.GetString("my-key", "default"), "value");
}

TEST_F(ConfigTest, KeyWithUnderscores) {
    config_.ParseString("my_key=value");
    EXPECT_TRUE(config_.HasKey("my_key"));
    EXPECT_EQ(config_.GetString("my_key", "default"), "value");
}

} // namespace test
} // namespace util
} // namespace shurium
