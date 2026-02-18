// SHURIUM Daemon - Main Entry Point
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// The shuriumd daemon is the main SHURIUM node implementation.
// It provides:
// - Full blockchain validation
// - P2P network participation
// - RPC server for external interaction
// - Wallet functionality
// - Staking and governance participation

#include <shurium/core/types.h>
#include <shurium/node/context.h>
#include <shurium/rpc/server.h>
#include <shurium/rpc/commands.h>
#include <shurium/util/logging.h>
#include <shurium/wallet/wallet.h>
#include <shurium/miner/miner.h>
#include <shurium/staking/staking.h>
#include <shurium/crypto/keys.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#endif

namespace shurium {

// ============================================================================
// Version Information
// ============================================================================

constexpr const char* VERSION = "0.1.0";
constexpr const char* VERSION_NAME = "Genesis";
constexpr const char* CLIENT_NAME = "SHURIUM Daemon";

// ============================================================================
// Default Configuration Values
// ============================================================================

namespace defaults {
    constexpr const char* DATADIR_NAME = ".shurium";
    constexpr const char* CONFIG_FILENAME = "nexus.conf";
    constexpr const char* PID_FILENAME = "shuriumd.pid";
    constexpr const char* LOG_FILENAME = "debug.log";
    
    constexpr const char* RPC_BIND = "127.0.0.1";
    constexpr uint16_t RPC_PORT = 8332;
    constexpr uint16_t TESTNET_RPC_PORT = 18332;
    constexpr uint16_t REGTEST_RPC_PORT = 18443;
    
    constexpr uint16_t P2P_PORT = 8333;
    constexpr uint16_t TESTNET_P2P_PORT = 18333;
    constexpr uint16_t REGTEST_P2P_PORT = 18444;
    
    constexpr int MAX_CONNECTIONS = 125;
    constexpr int RPC_THREADS = 4;
    constexpr int DB_CACHE_MB = 450;
}

// ============================================================================
// Daemon Configuration
// ============================================================================

struct DaemonConfig {
    // === Network ===
    std::string network{"main"};  // main, testnet, regtest
    bool testnet{false};
    bool regtest{false};
    
    // === Data Directory ===
    std::string dataDir;
    std::string configFile;
    std::string pidFile;
    
    // === RPC Settings ===
    bool rpcEnabled{true};
    std::string rpcBind{defaults::RPC_BIND};
    uint16_t rpcPort{defaults::RPC_PORT};
    std::string rpcUser;
    std::string rpcPassword;
    bool rpcAllowRemote{false};
    int rpcThreads{defaults::RPC_THREADS};
    
    // === P2P Network ===
    bool listen{true};
    std::string bind{"0.0.0.0"};
    uint16_t port{defaults::P2P_PORT};
    int maxConnections{defaults::MAX_CONNECTIONS};
    std::vector<std::string> addNodes;
    std::vector<std::string> connectNodes;
    bool dnsSeed{true};
    
    // === Blockchain ===
    int dbCache{defaults::DB_CACHE_MB};
    bool txIndex{false};
    bool reindex{false};
    bool prune{false};
    int pruneSize{550};  // MB
    
    // === Wallet ===
    bool walletEnabled{true};
    std::string walletFile{"wallet.dat"};
    bool walletBroadcast{true};
    
    // === Mining/Staking ===
    bool mining{false};
    bool staking{false};
    std::string miningAddress;
    int miningThreads{1};
    
    // === Logging ===
    std::string logLevel{"info"};
    bool logTimestamps{true};
    bool logToConsole{true};
    bool logToFile{true};
    std::vector<std::string> debugCategories;
    
    // === Daemon Mode ===
    bool daemon{false};
    bool printToConsole{true};
    
    // === Other ===
    bool checkBlocks{true};
    int checkLevel{3};
    bool assumeValid{true};
    std::string assumeValidBlock;
};

// ============================================================================
// Global State
// ============================================================================

static std::atomic<bool> g_running{false};
static std::mutex g_shutdownMutex;
static std::condition_variable g_shutdownCondition;

static DaemonConfig g_config;
static std::unique_ptr<rpc::RPCServer> g_rpcServer;
static std::unique_ptr<rpc::RPCCommandTable> g_rpcCommands;
static std::unique_ptr<NodeContext> g_node;
static std::shared_ptr<wallet::Wallet> g_wallet;
static std::unique_ptr<miner::Miner> g_miner;
static std::unique_ptr<staking::StakingEngine> g_stakingEngine;

// ============================================================================
// Signal Handling
// ============================================================================

// Flag for config reload request (processed in main loop)
static std::atomic<bool> g_reloadConfig{false};

// Forward declaration - implemented after ConfigParser
void ReloadConfiguration();

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Received shutdown signal (" << signal << ")";
        shurium::RequestShutdown();
        g_shutdownCondition.notify_all();
    }
#ifndef _WIN32
    else if (signal == SIGHUP) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Received SIGHUP, scheduling config reload";
        g_reloadConfig.store(true);
    }
#endif
}

void SetupSignalHandlers() {
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#ifndef _WIN32
    std::signal(SIGHUP, SignalHandler);
    std::signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe
#endif
}

// ============================================================================
// Path Utilities
// ============================================================================

std::string GetDefaultDataDir() {
#ifdef _WIN32
    char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "\\" + defaults::DATADIR_NAME;
    }
    return std::string(".\\") + defaults::DATADIR_NAME;
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd) {
            home = pwd->pw_dir;
        }
    }
    if (home) {
        return std::string(home) + "/" + defaults::DATADIR_NAME;
    }
    return std::string("./") + defaults::DATADIR_NAME;
#endif
}

bool CreateDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
#ifdef _WIN32
    return mkdir(path.c_str()) == 0;
#else
    return mkdir(path.c_str(), 0700) == 0;
#endif
}

bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string JoinPath(const std::string& dir, const std::string& file) {
#ifdef _WIN32
    return dir + "\\" + file;
#else
    return dir + "/" + file;
#endif
}

// ============================================================================
// Configuration File Parser
// ============================================================================

class ConfigParser {
public:
    bool Load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        std::string line;
        int lineNum = 0;
        
        while (std::getline(file, line)) {
            lineNum++;
            
            // Remove comments
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }
            
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            size_t end = line.find_last_not_of(" \t\r\n");
            line = line.substr(start, end - start + 1);
            
            if (line.empty()) continue;
            
            // Parse key=value
            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                // Boolean flag (key without value means true)
                options_[line] = "1";
            } else {
                std::string key = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                
                // Trim key
                end = key.find_last_not_of(" \t");
                if (end != std::string::npos) {
                    key = key.substr(0, end + 1);
                }
                
                // Trim value
                start = value.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    end = value.find_last_not_of(" \t\r\n");
                    value = value.substr(start, end - start + 1);
                } else {
                    value = "";
                }
                
                options_[key] = value;
            }
        }
        
        return true;
    }
    
    bool HasOption(const std::string& key) const {
        return options_.find(key) != options_.end();
    }
    
    std::string GetString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = options_.find(key);
        return (it != options_.end()) ? it->second : defaultValue;
    }
    
    int GetInt(const std::string& key, int defaultValue = 0) const {
        auto it = options_.find(key);
        if (it != options_.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
    
    bool GetBool(const std::string& key, bool defaultValue = false) const {
        auto it = options_.find(key);
        if (it != options_.end()) {
            const std::string& val = it->second;
            return val == "1" || val == "true" || val == "yes" || val == "on";
        }
        return defaultValue;
    }
    
    std::vector<std::string> GetMultiple(const std::string& key) const {
        std::vector<std::string> result;
        auto range = multiOptions_.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(it->second);
        }
        // Also check single option
        auto single = options_.find(key);
        if (single != options_.end() && !single->second.empty()) {
            result.push_back(single->second);
        }
        return result;
    }

private:
    std::map<std::string, std::string> options_;
    std::multimap<std::string, std::string> multiOptions_;
};

// ============================================================================
// Command Line Parsing
// ============================================================================

void PrintHelp() {
    std::cout << "SHURIUM Daemon - " << CLIENT_NAME << " v" << VERSION << " (" << VERSION_NAME << ")\n\n";
    std::cout << "Usage: shuriumd [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "  -v, --version              Show version information\n";
    std::cout << "  -c, --conf=FILE            Config file path (default: <datadir>/nexus.conf)\n";
    std::cout << "  -d, --datadir=DIR          Data directory path\n";
    std::cout << "  -D, --daemon               Run as daemon in background\n";
    std::cout << "  --testnet                  Use testnet\n";
    std::cout << "  --regtest                  Use regression test mode\n";
    std::cout << "\nRPC Options:\n";
    std::cout << "  --rpcbind=ADDR             RPC bind address (default: 127.0.0.1)\n";
    std::cout << "  --rpcport=PORT             RPC port (default: 8332)\n";
    std::cout << "  --rpcuser=USER             RPC username\n";
    std::cout << "  --rpcpassword=PASS         RPC password\n";
    std::cout << "  --rpcallowip=IP            Allow RPC from IP (can repeat)\n";
    std::cout << "  --rpcthreads=N             RPC thread count (default: 4)\n";
    std::cout << "  --server=0/1               Enable/disable RPC server (default: 1)\n";
    std::cout << "\nNetwork Options:\n";
    std::cout << "  --listen=0/1               Accept incoming connections (default: 1)\n";
    std::cout << "  --bind=ADDR                Bind to address\n";
    std::cout << "  --port=PORT                Listen port (default: 8333)\n";
    std::cout << "  --maxconnections=N         Max connections (default: 125)\n";
    std::cout << "  --addnode=IP               Add node to connect to (can repeat)\n";
    std::cout << "  --connect=IP               Connect only to these nodes (can repeat)\n";
    std::cout << "  --dnsseed=0/1              Use DNS seeds (default: 1)\n";
    std::cout << "\nBlockchain Options:\n";
    std::cout << "  --dbcache=N                Database cache size in MB (default: 450)\n";
    std::cout << "  --txindex                  Enable transaction index\n";
    std::cout << "  --reindex                  Rebuild blockchain index\n";
    std::cout << "  --prune=N                  Prune blockchain to N MB\n";
    std::cout << "\nWallet Options:\n";
    std::cout << "  --disablewallet            Disable wallet functionality\n";
    std::cout << "  --wallet=FILE              Wallet file name\n";
    std::cout << "\nMining/Staking Options:\n";
    std::cout << "  --gen=0/1                  Enable mining (default: 0)\n";
    std::cout << "  --genthreads=N             Mining threads (default: 1)\n";
    std::cout << "  --miningaddress=ADDR       Address for mining rewards\n";
    std::cout << "  --staking=0/1              Enable staking (default: 0)\n";
    std::cout << "\nLogging Options:\n";
    std::cout << "  --debug=CATEGORY           Enable debug for category (can repeat)\n";
    std::cout << "  --loglevel=LEVEL           Log level: trace, debug, info, warn, error\n";
    std::cout << "  --printtoconsole=0/1       Print to console (default: 1)\n";
    std::cout << "\n";
}

void PrintVersion() {
    std::cout << "SHURIUM Daemon v" << VERSION << " (" << VERSION_NAME << ")\n";
    std::cout << "Protocol version: " << PROTOCOL_VERSION << "\n";
    std::cout << "Copyright (c) 2024 SHURIUM Developers\n";
    std::cout << "MIT License\n";
}

bool ParseCommandLine(int argc, char* argv[], DaemonConfig& config) {
    static struct option longOptions[] = {
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, 'v'},
        {"conf", required_argument, nullptr, 'c'},
        {"datadir", required_argument, nullptr, 'd'},
        {"daemon", no_argument, nullptr, 'D'},
        {"testnet", no_argument, nullptr, 1001},
        {"regtest", no_argument, nullptr, 1002},
        {"rpcbind", required_argument, nullptr, 1003},
        {"rpcport", required_argument, nullptr, 1004},
        {"rpcuser", required_argument, nullptr, 1005},
        {"rpcpassword", required_argument, nullptr, 1006},
        {"rpcallowip", required_argument, nullptr, 1007},
        {"rpcthreads", required_argument, nullptr, 1008},
        {"server", required_argument, nullptr, 1009},
        {"listen", required_argument, nullptr, 1010},
        {"bind", required_argument, nullptr, 1011},
        {"port", required_argument, nullptr, 1012},
        {"maxconnections", required_argument, nullptr, 1013},
        {"addnode", required_argument, nullptr, 1014},
        {"connect", required_argument, nullptr, 1015},
        {"dnsseed", required_argument, nullptr, 1016},
        {"dbcache", required_argument, nullptr, 1017},
        {"txindex", no_argument, nullptr, 1018},
        {"reindex", no_argument, nullptr, 1019},
        {"prune", required_argument, nullptr, 1020},
        {"disablewallet", no_argument, nullptr, 1021},
        {"wallet", required_argument, nullptr, 1022},
        {"gen", required_argument, nullptr, 1023},
        {"genthreads", required_argument, nullptr, 1024},
        {"staking", required_argument, nullptr, 1025},
        {"miningaddress", required_argument, nullptr, 1029},
        {"debug", required_argument, nullptr, 1026},
        {"loglevel", required_argument, nullptr, 1027},
        {"printtoconsole", required_argument, nullptr, 1028},
        {nullptr, 0, nullptr, 0}
    };
    
    int opt;
    int optionIndex = 0;
    
    while ((opt = getopt_long(argc, argv, "hvc:d:D", longOptions, &optionIndex)) != -1) {
        switch (opt) {
            case 'h':
                PrintHelp();
                return false;
            case 'v':
                PrintVersion();
                return false;
            case 'c':
                config.configFile = optarg;
                break;
            case 'd':
                config.dataDir = optarg;
                break;
            case 'D':
                config.daemon = true;
                break;
            case 1001:  // --testnet
                config.testnet = true;
                config.network = "testnet";
                break;
            case 1002:  // --regtest
                config.regtest = true;
                config.network = "regtest";
                break;
            case 1003:  // --rpcbind
                config.rpcBind = optarg;
                break;
            case 1004:  // --rpcport
                config.rpcPort = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 1005:  // --rpcuser
                config.rpcUser = optarg;
                break;
            case 1006:  // --rpcpassword
                config.rpcPassword = optarg;
                break;
            case 1007:  // --rpcallowip
                config.rpcAllowRemote = true;
                break;
            case 1008:  // --rpcthreads
                config.rpcThreads = std::stoi(optarg);
                break;
            case 1009:  // --server
                config.rpcEnabled = (std::string(optarg) != "0");
                break;
            case 1010:  // --listen
                config.listen = (std::string(optarg) != "0");
                break;
            case 1011:  // --bind
                config.bind = optarg;
                break;
            case 1012:  // --port
                config.port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 1013:  // --maxconnections
                config.maxConnections = std::stoi(optarg);
                break;
            case 1014:  // --addnode
                config.addNodes.push_back(optarg);
                break;
            case 1015:  // --connect
                config.connectNodes.push_back(optarg);
                break;
            case 1016:  // --dnsseed
                config.dnsSeed = (std::string(optarg) != "0");
                break;
            case 1017:  // --dbcache
                config.dbCache = std::stoi(optarg);
                break;
            case 1018:  // --txindex
                config.txIndex = true;
                break;
            case 1019:  // --reindex
                config.reindex = true;
                break;
            case 1020:  // --prune
                config.prune = true;
                config.pruneSize = std::stoi(optarg);
                break;
            case 1021:  // --disablewallet
                config.walletEnabled = false;
                break;
            case 1022:  // --wallet
                config.walletFile = optarg;
                break;
            case 1023:  // --gen
                config.mining = (std::string(optarg) != "0");
                break;
            case 1024:  // --genthreads
                config.miningThreads = std::stoi(optarg);
                break;
            case 1025:  // --staking
                config.staking = (std::string(optarg) != "0");
                break;
            case 1029:  // --miningaddress
                config.miningAddress = optarg;
                break;
            case 1026:  // --debug
                config.debugCategories.push_back(optarg);
                break;
            case 1027:  // --loglevel
                config.logLevel = optarg;
                break;
            case 1028:  // --printtoconsole
                config.printToConsole = (std::string(optarg) != "0");
                break;
            default:
                return false;
        }
    }
    
    return true;
}

void LoadConfigFile(DaemonConfig& config) {
    std::string configPath = config.configFile;
    if (configPath.empty()) {
        configPath = JoinPath(config.dataDir, defaults::CONFIG_FILENAME);
    }
    
    ConfigParser parser;
    if (parser.Load(configPath)) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Loaded config from " << configPath;
        
        // Only override if not set on command line
        if (config.rpcBind == defaults::RPC_BIND && parser.HasOption("rpcbind")) {
            config.rpcBind = parser.GetString("rpcbind");
        }
        if (config.rpcPort == defaults::RPC_PORT && parser.HasOption("rpcport")) {
            config.rpcPort = static_cast<uint16_t>(parser.GetInt("rpcport"));
        }
        if (config.rpcUser.empty() && parser.HasOption("rpcuser")) {
            config.rpcUser = parser.GetString("rpcuser");
        }
        if (config.rpcPassword.empty() && parser.HasOption("rpcpassword")) {
            config.rpcPassword = parser.GetString("rpcpassword");
        }
        if (parser.HasOption("rpcallowip")) {
            config.rpcAllowRemote = true;
        }
        if (parser.HasOption("server")) {
            config.rpcEnabled = parser.GetBool("server", true);
        }
        if (parser.HasOption("testnet")) {
            config.testnet = parser.GetBool("testnet");
            if (config.testnet) config.network = "testnet";
        }
        if (parser.HasOption("regtest")) {
            config.regtest = parser.GetBool("regtest");
            if (config.regtest) config.network = "regtest";
        }
        if (parser.HasOption("txindex")) {
            config.txIndex = parser.GetBool("txindex");
        }
        if (parser.HasOption("listen")) {
            config.listen = parser.GetBool("listen", true);
        }
        if (parser.HasOption("dbcache")) {
            config.dbCache = parser.GetInt("dbcache", defaults::DB_CACHE_MB);
        }
        if (parser.HasOption("maxconnections")) {
            config.maxConnections = parser.GetInt("maxconnections", defaults::MAX_CONNECTIONS);
        }
        if (parser.HasOption("disablewallet")) {
            config.walletEnabled = !parser.GetBool("disablewallet");
        }
        if (parser.HasOption("staking")) {
            config.staking = parser.GetBool("staking");
        }
        
        // Add nodes from config
        auto addNodes = parser.GetMultiple("addnode");
        for (const auto& node : addNodes) {
            config.addNodes.push_back(node);
        }
    }
}

// ============================================================================
// Config Reload (SIGHUP handler)
// ============================================================================

void ReloadConfiguration() {
    LOG_INFO(util::LogCategory::DEFAULT) << "Reloading configuration...";
    
    std::string configPath = g_config.configFile;
    if (configPath.empty()) {
        configPath = JoinPath(g_config.dataDir, defaults::CONFIG_FILENAME);
    }
    
    ConfigParser parser;
    if (!parser.Load(configPath)) {
        LOG_WARN(util::LogCategory::DEFAULT) << "Failed to reload config file: " << configPath;
        return;
    }
    
    // Reload dynamic settings that can be safely changed at runtime
    
    // 1. Logging settings
    if (parser.HasOption("debug")) {
        auto debugCats = parser.GetMultiple("debug");
        for (const auto& cat : debugCats) {
            LOG_INFO(util::LogCategory::DEFAULT) << "Enabling debug for: " << cat;
        }
    }
    
    if (parser.HasOption("loglevel")) {
        std::string level = parser.GetString("loglevel");
        util::LogLevel logLevel = util::LogLevelFromString(level);
        util::Logger::Instance().SetLevel(logLevel);
        LOG_INFO(util::LogCategory::DEFAULT) << "Log level set to: " << level;
    }
    
    // 2. Connection settings (add new nodes)
    if (g_node && g_node->connman) {
        auto addNodes = parser.GetMultiple("addnode");
        for (const auto& node : addNodes) {
            // Check if already in our list
            bool found = false;
            for (const auto& existing : g_config.addNodes) {
                if (existing == node) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                LOG_INFO(util::LogCategory::DEFAULT) << "Adding node from config: " << node;
                g_config.addNodes.push_back(node);
                // Parse and connect
                auto addr = NetService::FromString(node);
                if (addr) {
                    g_node->connman->ConnectTo(*addr, ConnectionType::MANUAL);
                }
            }
        }
    }
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Configuration reload complete";
}

// ============================================================================
// Daemon Initialization
// ============================================================================

void SetupLogging(const DaemonConfig& config) {
    auto& logger = util::Logger::Instance();
    logger.Initialize();
    
    // Clear default sinks added by Initialize() to prevent duplicates
    logger.ClearSinks();
    
    // Set log level
    util::LogLevel level = util::LogLevelFromString(config.logLevel);
    logger.SetLevel(level);
    
    // Console sink
    if (config.printToConsole && !config.daemon) {
        auto consoleSink = std::make_shared<util::ConsoleSink>();
        util::ConsoleSink::Config consoleConfig;
        consoleConfig.level = level;
        consoleConfig.useColors = true;
        consoleConfig.showTimestamp = config.logTimestamps;
        consoleSink->SetConfig(consoleConfig);
        logger.AddSink(consoleSink);
    }
    
    // File sink
    if (config.logToFile) {
        std::string logPath = JoinPath(config.dataDir, defaults::LOG_FILENAME);
        auto fileSink = std::make_shared<util::FileSink>(logPath);
        util::FileSink::Config fileConfig;
        fileConfig.path = logPath;
        fileConfig.level = util::LogLevel::Debug;  // Always log debug to file
        fileConfig.rotate = true;
        fileSink->SetConfig(fileConfig);
        logger.AddSink(fileSink);
    }
    
    // Enable debug categories
    for (const auto& cat : config.debugCategories) {
        logger.EnableCategory(cat);
    }
}

bool InitializeDataDir(DaemonConfig& config) {
    // Set default data directory if not specified
    if (config.dataDir.empty()) {
        config.dataDir = GetDefaultDataDir();
    }
    
    // Add network subdirectory
    if (config.testnet) {
        config.dataDir = JoinPath(config.dataDir, "testnet");
    } else if (config.regtest) {
        config.dataDir = JoinPath(config.dataDir, "regtest");
    }
    
    // Create directory if it doesn't exist
    if (!CreateDirectory(config.dataDir)) {
        if (!FileExists(config.dataDir)) {
            std::cerr << "Error: Cannot create data directory: " << config.dataDir << "\n";
            return false;
        }
    }
    
    // Set PID file path
    config.pidFile = JoinPath(config.dataDir, defaults::PID_FILENAME);
    
    return true;
}

bool WritePidFile(const std::string& pidFile) {
    std::ofstream file(pidFile);
    if (!file.is_open()) {
        return false;
    }
#ifdef _WIN32
    file << GetCurrentProcessId();
#else
    file << getpid();
#endif
    return true;
}

void RemovePidFile(const std::string& pidFile) {
    std::remove(pidFile.c_str());
}

bool StartRPCServer(const DaemonConfig& config) {
    if (!config.rpcEnabled) {
        LOG_INFO(util::LogCategory::RPC) << "RPC server disabled";
        return true;
    }
    
    // Configure RPC server
    rpc::RPCServerConfig rpcConfig;
    rpcConfig.bindAddress = config.rpcBind;
    rpcConfig.port = config.rpcPort;
    rpcConfig.rpcUser = config.rpcUser;
    rpcConfig.rpcPassword = config.rpcPassword;
    rpcConfig.allowRemote = config.rpcAllowRemote;
    rpcConfig.threadPoolSize = static_cast<size_t>(config.rpcThreads);
    rpcConfig.enableRateLimiting = true;
    
    // Create RPC server
    g_rpcServer = std::make_unique<rpc::RPCServer>(rpcConfig);
    
    // Create and register commands
    g_rpcCommands = std::make_unique<rpc::RPCCommandTable>();
    g_rpcCommands->RegisterCommands(*g_rpcServer);
    
    // Start server
    if (!g_rpcServer->Start()) {
        LOG_ERROR(util::LogCategory::RPC) << "Failed to start RPC server on " 
                                          << config.rpcBind << ":" << config.rpcPort;
        return false;
    }
    
    LOG_INFO(util::LogCategory::RPC) << "RPC server listening on " 
                                     << config.rpcBind << ":" << config.rpcPort;
    
    return true;
}

void StopRPCServer() {
    if (g_rpcServer) {
        LOG_INFO(util::LogCategory::RPC) << "Stopping RPC server...";
        g_rpcServer->Stop();
        g_rpcServer.reset();
    }
    g_rpcCommands.reset();
}

// ============================================================================
// Wallet Initialization
// ============================================================================

bool InitializeWallet(const DaemonConfig& config) {
    if (!config.walletEnabled) {
        LOG_INFO(util::LogCategory::WALLET) << "Wallet disabled";
        return true;
    }
    
    std::string walletPath = JoinPath(config.dataDir, config.walletFile);
    
    // Try to load existing wallet
    if (FileExists(walletPath)) {
        LOG_INFO(util::LogCategory::WALLET) << "Loading wallet from " << walletPath;
        try {
            wallet::Wallet::Config walletConfig;
            walletConfig.name = "default";
            walletConfig.testnet = config.testnet;
            
            g_wallet = wallet::Wallet::Load(walletPath, walletConfig);
            if (g_wallet) {
                LOG_INFO(util::LogCategory::WALLET) << "Wallet loaded successfully";
                
                // Set wallet in RPC command table
                if (g_rpcCommands) {
                    g_rpcCommands->SetWallet(g_wallet);
                }
                return true;
            }
        } catch (const std::exception& e) {
            LOG_ERROR(util::LogCategory::WALLET) << "Failed to load wallet: " << e.what();
            return false;
        }
    }
    
    // No existing wallet - create a new one
    LOG_INFO(util::LogCategory::WALLET) << "Creating new wallet at " << walletPath;
    try {
        wallet::Wallet::Config walletConfig;
        walletConfig.name = "default";
        walletConfig.testnet = config.testnet;
        
        // Generate new wallet (with empty password initially - user should encrypt it)
        g_wallet = wallet::Wallet::Generate("", wallet::Mnemonic::Strength::Words24, walletConfig);
        
        if (g_wallet) {
            // Save the wallet
            if (g_wallet->Save(walletPath)) {
                LOG_INFO(util::LogCategory::WALLET) << "New wallet created and saved";
                LOG_WARN(util::LogCategory::WALLET) << "IMPORTANT: Use 'encryptwallet' to secure your wallet with a password!";
                
                // Set wallet in RPC command table
                if (g_rpcCommands) {
                    g_rpcCommands->SetWallet(g_wallet);
                }
                return true;
            } else {
                LOG_ERROR(util::LogCategory::WALLET) << "Failed to save wallet";
                return false;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(util::LogCategory::WALLET) << "Failed to create wallet: " << e.what();
        return false;
    }
    
    LOG_ERROR(util::LogCategory::WALLET) << "Failed to initialize wallet";
    return false;
}

#ifndef _WIN32
bool Daemonize() {
    // Fork
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid > 0) {
        // Parent exits
        _exit(0);
    }
    
    // Create new session
    if (setsid() < 0) {
        return false;
    }
    
    // Fork again
    pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid > 0) {
        _exit(0);
    }
    
    // Change working directory
    if (chdir("/") < 0) {
        // Non-fatal
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirect to /dev/null
    open("/dev/null", O_RDONLY);  // stdin
    open("/dev/null", O_WRONLY);  // stdout
    open("/dev/null", O_WRONLY);  // stderr
    
    return true;
}
#endif

void WaitForShutdown() {
    std::unique_lock<std::mutex> lock(g_shutdownMutex);
    while (!shurium::ShutdownRequested()) {
        g_shutdownCondition.wait_for(lock, std::chrono::seconds(1));
        
        // Check for config reload request (from SIGHUP)
        if (g_reloadConfig.exchange(false)) {
            lock.unlock();
            ReloadConfiguration();
            lock.lock();
        }
    }
}

void Shutdown() {
    LOG_INFO(util::LogCategory::DEFAULT) << "Shutting down...";
    
    // Stop miner first
    if (g_miner) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Stopping miner...";
        g_miner->Stop();
        g_miner.reset();
    }
    
    // Stop RPC server (so no new requests come in)
    StopRPCServer();
    
    // Reset staking engine
    g_stakingEngine.reset();
    
    // Shutdown the node (network, mempool, chain state, databases)
    if (g_node) {
        ShutdownNode(*g_node);
        g_node.reset();
    }
    
    // Remove PID file
    if (!g_config.pidFile.empty()) {
        RemovePidFile(g_config.pidFile);
    }
    
    LOG_INFO(util::LogCategory::DEFAULT) << "Shutdown complete";
    
    // Shutdown logging
    util::Logger::Instance().Shutdown();
}

// ============================================================================
// Main Entry Point
// ============================================================================

int AppMain(int argc, char* argv[]) {
    // Parse command line
    if (!ParseCommandLine(argc, argv, g_config)) {
        return 0;  // Help or version was shown, or error
    }
    
    // Initialize data directory
    if (!InitializeDataDir(g_config)) {
        return 1;
    }
    
    // Setup logging first
    SetupLogging(g_config);
    
    LOG_INFO(util::LogCategory::DEFAULT) << "SHURIUM Daemon v" << VERSION << " starting...";
    LOG_INFO(util::LogCategory::DEFAULT) << "Data directory: " << g_config.dataDir;
    LOG_INFO(util::LogCategory::DEFAULT) << "Network: " << g_config.network;
    
    // Load config file
    LoadConfigFile(g_config);
    
    // Adjust ports for network
    if (g_config.testnet) {
        if (g_config.rpcPort == defaults::RPC_PORT) {
            g_config.rpcPort = defaults::TESTNET_RPC_PORT;
        }
        if (g_config.port == defaults::P2P_PORT) {
            g_config.port = defaults::TESTNET_P2P_PORT;
        }
    } else if (g_config.regtest) {
        if (g_config.rpcPort == defaults::RPC_PORT) {
            g_config.rpcPort = defaults::REGTEST_RPC_PORT;
        }
        if (g_config.port == defaults::P2P_PORT) {
            g_config.port = defaults::REGTEST_P2P_PORT;
        }
    }
    
    // Daemonize if requested
#ifndef _WIN32
    if (g_config.daemon) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Daemonizing...";
        if (!Daemonize()) {
            LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to daemonize";
            return 1;
        }
    }
#endif
    
    // Write PID file
    if (!WritePidFile(g_config.pidFile)) {
        LOG_WARN(util::LogCategory::DEFAULT) << "Could not write PID file: " << g_config.pidFile;
    }
    
    // Setup signal handlers
    SetupSignalHandlers();
    
    // ========================================================================
    // Initialize Node (blockchain, mempool, network)
    // ========================================================================
    
    g_node = std::make_unique<NodeContext>();
    
    // Convert DaemonConfig to NodeInitOptions
    NodeInitOptions nodeOptions;
    nodeOptions.dataDir = g_config.dataDir;
    nodeOptions.network = g_config.network;
    nodeOptions.dbCacheMB = g_config.dbCache;
    nodeOptions.txIndex = g_config.txIndex;
    nodeOptions.reindex = g_config.reindex;
    nodeOptions.prune = g_config.prune;
    nodeOptions.pruneSizeMB = g_config.pruneSize;
    nodeOptions.listen = g_config.listen;
    nodeOptions.bindAddress = g_config.bind;
    nodeOptions.port = g_config.port;
    nodeOptions.maxConnections = g_config.maxConnections;
    nodeOptions.addNodes = g_config.addNodes;
    nodeOptions.connectNodes = g_config.connectNodes;
    nodeOptions.dnsSeed = g_config.dnsSeed;
    nodeOptions.mining = g_config.mining;
    nodeOptions.staking = g_config.staking;
    nodeOptions.miningThreads = g_config.miningThreads;
    nodeOptions.miningAddress = g_config.miningAddress;
    nodeOptions.checkBlocks = g_config.checkBlocks;
    nodeOptions.checkLevel = g_config.checkLevel;
    nodeOptions.assumeValidBlock = g_config.assumeValidBlock;
    
    // Initialize node (databases, chain state, mempool)
    if (!InitializeNode(*g_node, nodeOptions)) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to initialize node";
        Shutdown();
        return 1;
    }
    
    // Start P2P network
    if (!StartNetwork(*g_node, nodeOptions)) {
        LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to start network";
        Shutdown();
        return 1;
    }
    
    // Start block synchronization
    if (!StartSync(*g_node)) {
        LOG_WARN(util::LogCategory::DEFAULT) << "Failed to start sync (continuing anyway)";
    }
    
    // Start RPC server
    if (!StartRPCServer(g_config)) {
        Shutdown();
        return 1;
    }
    
    // Wire up node components to RPC commands
    if (g_rpcCommands && g_node) {
        if (g_node->blockDB) {
            g_rpcCommands->SetBlockDB(std::shared_ptr<db::BlockDB>(g_node->blockDB.get(), [](db::BlockDB*){}));
        }
        if (g_node->chainman) {
            g_rpcCommands->SetChainStateManager(std::shared_ptr<ChainStateManager>(g_node->chainman.get(), [](ChainStateManager*){}));
            // Also set the active ChainState
            g_rpcCommands->SetChainState(std::shared_ptr<ChainState>(&g_node->chainman->GetActiveChainState(), [](ChainState*){}));
        }
        if (g_node->mempool) {
            g_rpcCommands->SetMempool(std::shared_ptr<Mempool>(g_node->mempool.get(), [](Mempool*){}));
        }
        if (g_node->msgproc) {
            g_rpcCommands->SetMessageProcessor(g_node->msgproc.get());
        }
    }
    
    // Initialize wallet (after RPC server so g_rpcCommands exists)
    if (!InitializeWallet(g_config)) {
        LOG_WARN(util::LogCategory::DEFAULT) << "Wallet initialization failed (continuing without wallet)";
        // Don't fail - user can use loadwallet/createwallet commands later
    }
    
    // Initialize staking engine (needed for RPC and staking)
    g_stakingEngine = std::make_unique<staking::StakingEngine>();
    if (g_rpcCommands) {
        g_rpcCommands->SetStakingEngine(std::shared_ptr<staking::StakingEngine>(
            g_stakingEngine.get(), [](staking::StakingEngine*){}));
    }
    
    // Start mining if enabled
    if (g_config.mining) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Mining enabled with " 
                                             << g_config.miningThreads << " threads";
        
        // Get mining address (from config or wallet)
        Hash160 miningAddress;
        if (!g_config.miningAddress.empty()) {
            // Parse address from config using DecodeAddress
            auto scriptPubKey = DecodeAddress(g_config.miningAddress);
            if (!scriptPubKey.empty() && scriptPubKey.size() >= 22) {
                // Extract hash from P2PKH (OP_DUP OP_HASH160 <20 bytes> OP_EQUALVERIFY OP_CHECKSIG)
                // or P2WPKH (OP_0 <20 bytes>)
                if (scriptPubKey.size() == 25 && scriptPubKey[0] == 0x76) {
                    // P2PKH: skip OP_DUP(0x76) OP_HASH160(0xa9) 0x14(length)
                    std::memcpy(miningAddress.data(), &scriptPubKey[3], 20);
                } else if (scriptPubKey.size() == 22 && scriptPubKey[0] == 0x00) {
                    // P2WPKH: skip OP_0(0x00) 0x14(length)
                    std::memcpy(miningAddress.data(), &scriptPubKey[2], 20);
                }
            }
            if (miningAddress.IsNull()) {
                LOG_ERROR(util::LogCategory::DEFAULT) << "Invalid mining address: " << g_config.miningAddress;
            }
        } else if (g_wallet) {
            // Get address from wallet
            auto addresses = g_wallet->GetAddresses();
            if (!addresses.empty()) {
                auto scriptPubKey = DecodeAddress(addresses[0]);
                if (!scriptPubKey.empty() && scriptPubKey.size() >= 22) {
                    if (scriptPubKey.size() == 25 && scriptPubKey[0] == 0x76) {
                        std::memcpy(miningAddress.data(), &scriptPubKey[3], 20);
                    } else if (scriptPubKey.size() == 22 && scriptPubKey[0] == 0x00) {
                        std::memcpy(miningAddress.data(), &scriptPubKey[2], 20);
                    }
                }
                if (!miningAddress.IsNull()) {
                    LOG_INFO(util::LogCategory::DEFAULT) << "Using wallet address for mining: " << addresses[0];
                }
            }
        }
        
        if (miningAddress.IsNull()) {
            LOG_WARN(util::LogCategory::DEFAULT) << "No mining address available. Mining disabled.";
            LOG_WARN(util::LogCategory::DEFAULT) << "Use --miningaddress=<addr> or create a wallet first.";
        } else {
            // Create and start miner
            miner::MinerOptions minerOpts;
            minerOpts.numThreads = g_config.miningThreads;
            minerOpts.coinbaseAddress = miningAddress;
            
            g_miner = std::make_unique<miner::Miner>(
                *g_node->chainman,
                *g_node->mempool,
                *g_node->params,
                minerOpts
            );
            
            // Set message processor for block relay
            if (g_node->msgproc) {
                g_miner->SetMessageProcessor(g_node->msgproc.get());
            }
            
            // Set callback for block found
            g_miner->SetBlockFoundCallback([](const Block& block, bool accepted) {
                if (accepted) {
                    LOG_INFO(util::LogCategory::DEFAULT) << "Mined block " 
                        << block.GetHash().ToHex().substr(0, 16) << "... accepted!";
                } else {
                    LOG_WARN(util::LogCategory::DEFAULT) << "Mined block " 
                        << block.GetHash().ToHex().substr(0, 16) << "... rejected";
                }
            });
            
            if (g_miner->Start()) {
                LOG_INFO(util::LogCategory::DEFAULT) << "Miner started successfully";
            } else {
                LOG_ERROR(util::LogCategory::DEFAULT) << "Failed to start miner";
                g_miner.reset();
            }
        }
    }
    
    // Start staking if enabled
    if (g_config.staking) {
        LOG_INFO(util::LogCategory::DEFAULT) << "Staking enabled";
        // Note: Staking requires being a registered validator with sufficient stake.
        // The staking engine is initialized above and can be used via RPC commands
        // (registervalidator, delegate, etc.) to participate in staking.
        // Actual block production for staking would be handled similarly to mining,
        // but with validator selection based on stake rather than proof-of-work.
        LOG_INFO(util::LogCategory::DEFAULT) << "Use 'registervalidator' RPC to become a validator";
    }
    
    g_running.store(true);
    LOG_INFO(util::LogCategory::DEFAULT) << "SHURIUM Daemon started successfully";
    
    // Wait for shutdown signal
    WaitForShutdown();
    
    // Clean shutdown
    g_running.store(false);
    Shutdown();
    
    return 0;
}

} // namespace shurium

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        return shurium::AppMain(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error" << std::endl;
        return 1;
    }
}
