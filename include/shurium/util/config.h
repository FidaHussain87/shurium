// SHURIUM - Configuration File Parser
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Parses INI-style configuration files for SHURIUM node configuration.
//
// Configuration file format:
// - Lines starting with # or ; are comments
// - key=value pairs
// - Section headers: [section]
// - Values can be quoted: key="value with spaces"
// - Backslash continuation for multi-line values
// - Boolean values: true/false, yes/no, 1/0
// - Environment variable expansion: ${VAR_NAME}

#ifndef SHURIUM_UTIL_CONFIG_H
#define SHURIUM_UTIL_CONFIG_H

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace shurium {
namespace util {

// ============================================================================
// Configuration Constants
// ============================================================================

/// Default data directory name
#ifdef _WIN32
constexpr const char* DEFAULT_DATADIR_NAME = "Shurium";
#else
constexpr const char* DEFAULT_DATADIR_NAME = ".shurium";
#endif

/// Default config file name
constexpr const char* DEFAULT_CONFIG_FILENAME = "nexus.conf";

/// Maximum config file size (1 MB)
constexpr size_t MAX_CONFIG_SIZE = 1024 * 1024;

/// Maximum line length
constexpr size_t MAX_LINE_LENGTH = 4096;

/// Maximum include depth (to prevent infinite recursion)
constexpr int MAX_INCLUDE_DEPTH = 10;

// ============================================================================
// Configuration Entry
// ============================================================================

/**
 * A single configuration entry.
 */
struct ConfigEntry {
    std::string key;
    std::string value;
    std::string section;   // Empty for global section
    std::string source;    // File path where this was defined
    int lineNumber{0};
    bool isDefault{false}; // True if this is a default value
    
    /// Check if value is a boolean true
    bool IsTrue() const;
    
    /// Check if value is a boolean false
    bool IsFalse() const;
};

// ============================================================================
// Configuration Parse Result
// ============================================================================

/**
 * Result of parsing a configuration file.
 */
struct ConfigParseResult {
    bool success{false};
    std::string errorMessage;
    std::string errorFile;
    int errorLine{0};
    std::vector<std::string> warnings;
    
    static ConfigParseResult Success() {
        return {true, "", "", 0, {}};
    }
    
    static ConfigParseResult Error(const std::string& msg, 
                                   const std::string& file = "", 
                                   int line = 0) {
        return {false, msg, file, line, {}};
    }
};

// ============================================================================
// Configuration Manager
// ============================================================================

/**
 * Manages configuration from files and command-line arguments.
 * 
 * Priority order (highest to lowest):
 * 1. Command-line arguments
 * 2. Data directory config file
 * 3. User config file (~/.shurium/nexus.conf)
 * 4. System config file (/etc/nexus/nexus.conf)
 * 5. Built-in defaults
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    // ========================================================================
    // File Parsing
    // ========================================================================
    
    /**
     * Parse a configuration file.
     * 
     * @param filePath Path to the config file
     * @param overwrite If true, overwrite existing values
     * @return Parse result
     */
    ConfigParseResult ParseFile(const std::string& filePath, bool overwrite = false);
    
    /**
     * Parse configuration from a string.
     * 
     * @param content Config file content
     * @param sourceName Name for error messages
     * @param overwrite If true, overwrite existing values
     * @return Parse result
     */
    ConfigParseResult ParseString(const std::string& content, 
                                  const std::string& sourceName = "<string>",
                                  bool overwrite = false);
    
    /**
     * Parse command-line arguments.
     * Command-line args take highest priority.
     * 
     * @param argc Argument count
     * @param argv Argument values
     * @return Parse result
     */
    ConfigParseResult ParseCommandLine(int argc, char* argv[]);
    
    /**
     * Load all configuration files in standard order.
     * 
     * @param dataDir Data directory path (or empty for default)
     * @return Combined parse result
     */
    ConfigParseResult LoadAllConfigs(const std::string& dataDir = "");
    
    // ========================================================================
    // Value Retrieval
    // ========================================================================
    
    /// Check if a key exists
    bool HasKey(const std::string& key, const std::string& section = "") const;
    
    /// Get raw string value (returns nullopt if key doesn't exist)
    std::optional<std::string> TryGetString(const std::string& key, 
                                             const std::string& section = "") const;
    
    /// Get string value with default
    std::string GetString(const std::string& key, 
                         const std::string& defaultValue,
                         const std::string& section = "") const;
    
    /// Get integer value (returns nullopt if key doesn't exist or is invalid)
    std::optional<int64_t> TryGetInt(const std::string& key, 
                                      const std::string& section = "") const;
    
    /// Get integer value with default
    int64_t GetInt(const std::string& key, 
                   int64_t defaultValue,
                   const std::string& section = "") const;
    
    /// Get unsigned integer value (returns nullopt if key doesn't exist or is invalid)
    std::optional<uint64_t> TryGetUInt(const std::string& key, 
                                        const std::string& section = "") const;
    
    /// Get unsigned integer value with default
    uint64_t GetUInt(const std::string& key, 
                     uint64_t defaultValue,
                     const std::string& section = "") const;
    
    /// Get boolean value (returns nullopt if key doesn't exist or is invalid)
    std::optional<bool> TryGetBool(const std::string& key, 
                                    const std::string& section = "") const;
    
    /// Get boolean value with default
    bool GetBool(const std::string& key, 
                 bool defaultValue,
                 const std::string& section = "") const;
    
    /// Get floating point value (returns nullopt if key doesn't exist or is invalid)
    std::optional<double> TryGetDouble(const std::string& key, 
                                        const std::string& section = "") const;
    
    /// Get floating point value with default
    double GetDouble(const std::string& key, 
                     double defaultValue,
                     const std::string& section = "") const;
    
    /// Get list of values (comma-separated or multiple entries)
    std::vector<std::string> GetList(const std::string& key, 
                                     const std::string& section = "") const;
    
    /// Get path value (with ~ expansion)
    std::string GetPath(const std::string& key, 
                        const std::string& defaultValue = "",
                        const std::string& section = "") const;
    
    // ========================================================================
    // Value Setting
    // ========================================================================
    
    /// Set a value programmatically
    void Set(const std::string& key, const std::string& value, 
             const std::string& section = "");
    
    /// Set a default value (lower priority than config files)
    void SetDefault(const std::string& key, const std::string& value, 
                   const std::string& section = "");
    
    /// Add a value to a list
    void AddToList(const std::string& key, const std::string& value, 
                   const std::string& section = "");
    
    // ========================================================================
    // Sections
    // ========================================================================
    
    /// Get all section names
    std::vector<std::string> GetSections() const;
    
    /// Get all keys in a section (empty for global)
    std::vector<std::string> GetKeys(const std::string& section = "") const;
    
    /// Get all entries in a section
    std::vector<ConfigEntry> GetEntries(const std::string& section = "") const;
    
    // ========================================================================
    // Validation
    // ========================================================================
    
    /// Register a required key
    void RequireKey(const std::string& key, const std::string& section = "");
    
    /// Register an allowed key (for validation)
    void AllowKey(const std::string& key, const std::string& section = "");
    
    /// Validate configuration (check required keys, warn about unknown keys)
    std::vector<std::string> Validate() const;
    
    // ========================================================================
    // Utilities
    // ========================================================================
    
    /// Clear all configuration
    void Clear();
    
    /// Get number of entries
    size_t Size() const;
    
    /// Get data directory
    std::string GetDataDir() const;
    
    /// Set data directory
    void SetDataDir(const std::string& dir);
    
    /// Get default data directory path
    static std::string GetDefaultDataDir();
    
    /// Expand environment variables in a string
    static std::string ExpandEnvVars(const std::string& value);
    
    /// Expand ~ to home directory
    static std::string ExpandTilde(const std::string& path);
    
    /// Generate sample configuration file
    std::string GenerateSampleConfig() const;
    
    /// Dump all configuration to string
    std::string Dump() const;

private:
    /// Internal key for section:key combination
    std::string MakeKey(const std::string& key, const std::string& section) const;
    
    /// Parse a single line
    bool ParseLine(const std::string& line, const std::string& source, int lineNum,
                   std::string& currentSection, ConfigParseResult& result);
    
    /// Trim whitespace
    static std::string Trim(const std::string& str);
    
    /// Unquote a value
    static std::string Unquote(const std::string& str);
    
    /// Parse boolean string
    static std::optional<bool> ParseBool(const std::string& str);
    
    // Configuration data
    std::map<std::string, ConfigEntry> entries_;
    std::map<std::string, std::vector<std::string>> lists_;  // Multi-value entries
    
    // Validation
    std::set<std::string> requiredKeys_;
    std::set<std::string> allowedKeys_;
    
    // State
    std::string dataDir_;
    int includeDepth_{0};
};

// ============================================================================
// Global Configuration
// ============================================================================

/// Get global configuration manager
ConfigManager& GetConfig();

/// Initialize global configuration from command line
ConfigParseResult InitConfig(int argc, char* argv[]);

// ============================================================================
// Common Configuration Keys
// ============================================================================

namespace ConfigKeys {
    // General
    constexpr const char* DATADIR = "datadir";
    constexpr const char* CONF = "conf";
    constexpr const char* TESTNET = "testnet";
    constexpr const char* REGTEST = "regtest";
    constexpr const char* NETWORK = "network";
    constexpr const char* DEBUG = "debug";
    constexpr const char* PRINTTOCONSOLE = "printtoconsole";
    
    // Network
    constexpr const char* LISTEN = "listen";
    constexpr const char* PORT = "port";
    constexpr const char* BIND = "bind";
    constexpr const char* EXTERNALIP = "externalip";
    constexpr const char* MAXCONNECTIONS = "maxconnections";
    constexpr const char* CONNECT = "connect";
    constexpr const char* ADDNODE = "addnode";
    constexpr const char* SEEDNODE = "seednode";
    constexpr const char* DNSSEED = "dnsseed";
    constexpr const char* ONLYNET = "onlynet";
    constexpr const char* PROXY = "proxy";
    constexpr const char* TIMEOUT = "timeout";
    
    // RPC
    constexpr const char* SERVER = "server";
    constexpr const char* RPCUSER = "rpcuser";
    constexpr const char* RPCPASSWORD = "rpcpassword";
    constexpr const char* RPCPORT = "rpcport";
    constexpr const char* RPCBIND = "rpcbind";
    constexpr const char* RPCALLOWIP = "rpcallowip";
    constexpr const char* RPCCOOKIEFILE = "rpccookiefile";
    
    // Wallet
    constexpr const char* WALLET = "wallet";
    constexpr const char* DISABLEWALLET = "disablewallet";
    constexpr const char* WALLETDIR = "walletdir";
    constexpr const char* KEYPOOL = "keypool";
    constexpr const char* ADDRESSTYPE = "addresstype";
    
    // Mining/Staking
    constexpr const char* GEN = "gen";
    constexpr const char* GENPROCLIMIT = "genproclimit";
    constexpr const char* MININGADDRESS = "miningaddress";
    constexpr const char* STAKING = "staking";
    
    // Mempool
    constexpr const char* MAXMEMPOOL = "maxmempool";
    constexpr const char* MEMPOOLEXPIRY = "mempoolexpiry";
    constexpr const char* MINRELAYTXFEE = "minrelaytxfee";
    
    // Block
    constexpr const char* BLOCKMAXSIZE = "blockmaxsize";
    constexpr const char* BLOCKMAXWEIGHT = "blockmaxweight";
    constexpr const char* PRUNE = "prune";
    constexpr const char* REINDEX = "reindex";
    constexpr const char* CHECKBLOCKS = "checkblocks";
}

} // namespace util
} // namespace shurium

#endif // SHURIUM_UTIL_CONFIG_H
