// SHURIUM CLI - Command Line Interface
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// The shurium-cli tool provides command-line access to a running SHURIUM node.
// It communicates with the node via JSON-RPC.

#include <shurium/rpc/client.h>
#include <shurium/rpc/server.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace shurium {
namespace cli {

// ============================================================================
// Version Information
// ============================================================================

constexpr const char* VERSION = "0.1.0";
constexpr const char* CLIENT_NAME = "SHURIUM CLI";

// ============================================================================
// Default Configuration
// ============================================================================

namespace defaults {
    constexpr const char* DATADIR_NAME = ".shurium";
    constexpr const char* CONFIG_FILENAME = "nexus.conf";
    constexpr const char* RPC_HOST = "127.0.0.1";
    constexpr uint16_t RPC_PORT = 8332;
    constexpr uint16_t TESTNET_RPC_PORT = 18332;
    constexpr uint16_t REGTEST_RPC_PORT = 18443;
}

// ============================================================================
// CLI Configuration
// ============================================================================

struct CLIConfig {
    // Connection settings
    std::string rpcHost{defaults::RPC_HOST};
    uint16_t rpcPort{defaults::RPC_PORT};
    std::string rpcUser;
    std::string rpcPassword;
    std::string rpcCookieFile;
    bool useSSL{false};
    
    // Network selection
    bool testnet{false};
    bool regtest{false};
    
    // Paths
    std::string dataDir;
    std::string configFile;
    
    // Output options
    bool prettyPrint{true};
    bool rawOutput{false};
    bool stdinMode{false};
    
    // Command
    std::string method;
    std::vector<std::string> args;
    
    // Flags
    bool showHelp{false};
    bool showVersion{false};
    bool namedParams{false};
};

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

std::string JoinPath(const std::string& dir, const std::string& file) {
#ifdef _WIN32
    return dir + "\\" + file;
#else
    return dir + "/" + file;
#endif
}

bool FileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// ============================================================================
// Configuration Loading
// ============================================================================

class ConfigParser {
public:
    bool Load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
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
                options_[line] = "1";
            } else {
                std::string key = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                
                // Trim
                end = key.find_last_not_of(" \t");
                if (end != std::string::npos) key = key.substr(0, end + 1);
                
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
            try { return std::stoi(it->second); }
            catch (...) { return defaultValue; }
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

private:
    std::map<std::string, std::string> options_;
};

std::string ReadCookieFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string content;
    std::getline(file, content);
    return content;
}

void LoadConfigFile(CLIConfig& config) {
    std::string dataDir = config.dataDir;
    if (dataDir.empty()) {
        dataDir = GetDefaultDataDir();
    }
    
    // Adjust for network
    if (config.testnet) {
        dataDir = JoinPath(dataDir, "testnet");
    } else if (config.regtest) {
        dataDir = JoinPath(dataDir, "regtest");
    }
    
    std::string configPath = config.configFile;
    if (configPath.empty()) {
        configPath = JoinPath(dataDir, defaults::CONFIG_FILENAME);
    }
    
    ConfigParser parser;
    if (parser.Load(configPath)) {
        // Only override if not set on command line
        if (config.rpcHost == defaults::RPC_HOST && parser.HasOption("rpcconnect")) {
            config.rpcHost = parser.GetString("rpcconnect");
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
        if (parser.HasOption("testnet") && parser.GetBool("testnet")) {
            config.testnet = true;
        }
        if (parser.HasOption("regtest") && parser.GetBool("regtest")) {
            config.regtest = true;
        }
    }
    
    // Try to read cookie file if no credentials set
    if (config.rpcUser.empty() && config.rpcPassword.empty()) {
        std::string cookiePath = config.rpcCookieFile;
        if (cookiePath.empty()) {
            cookiePath = JoinPath(dataDir, ".cookie");
        }
        std::string cookie = ReadCookieFile(cookiePath);
        if (!cookie.empty()) {
            size_t colonPos = cookie.find(':');
            if (colonPos != std::string::npos) {
                config.rpcUser = cookie.substr(0, colonPos);
                config.rpcPassword = cookie.substr(colonPos + 1);
            }
        }
    }
}

// ============================================================================
// Help Text
// ============================================================================

void PrintHelp() {
    std::cout << CLIENT_NAME << " v" << VERSION << "\n\n";
    std::cout << "Usage: shurium-cli [options] <command> [params]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "  -v, --version              Show version information\n";
    std::cout << "  -c, --conf=FILE            Config file path\n";
    std::cout << "  -d, --datadir=DIR          Data directory path\n";
    std::cout << "  --testnet                  Use testnet\n";
    std::cout << "  --regtest                  Use regression test mode\n";
    std::cout << "\nRPC Options:\n";
    std::cout << "  --rpcconnect=HOST          RPC server host (default: 127.0.0.1)\n";
    std::cout << "  --rpcport=PORT             RPC server port (default: 8332)\n";
    std::cout << "  --rpcuser=USER             RPC username\n";
    std::cout << "  --rpcpassword=PASS         RPC password\n";
    std::cout << "  --rpccookiefile=FILE       RPC cookie file path\n";
    std::cout << "\nOutput Options:\n";
    std::cout << "  --raw                      Output raw JSON response\n";
    std::cout << "  --stdin                    Read extra arguments from stdin\n";
    std::cout << "  --named                    Use named parameters\n";
    std::cout << "\nCommands:\n";
    std::cout << "  Use 'shurium-cli help' for a list of available commands\n";
    std::cout << "  Use 'shurium-cli help <command>' for help on a specific command\n";
    std::cout << "\nExamples:\n";
    std::cout << "  shurium-cli getblockchaininfo\n";
    std::cout << "  shurium-cli getblock <hash>\n";
    std::cout << "  shurium-cli sendtoaddress <address> <amount>\n";
    std::cout << "  shurium-cli --testnet getbalance\n";
    std::cout << "\n";
}

void PrintVersion() {
    std::cout << CLIENT_NAME << " v" << VERSION << "\n";
    std::cout << "Copyright (c) 2024 SHURIUM Developers\n";
    std::cout << "MIT License\n";
}

// ============================================================================
// Command Line Parsing
// ============================================================================

bool ParseCommandLine(int argc, char* argv[], CLIConfig& config) {
    static struct option longOptions[] = {
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, 'v'},
        {"conf", required_argument, nullptr, 'c'},
        {"datadir", required_argument, nullptr, 'd'},
        {"testnet", no_argument, nullptr, 1001},
        {"regtest", no_argument, nullptr, 1002},
        {"rpcconnect", required_argument, nullptr, 1003},
        {"rpcport", required_argument, nullptr, 1004},
        {"rpcuser", required_argument, nullptr, 1005},
        {"rpcpassword", required_argument, nullptr, 1006},
        {"rpccookiefile", required_argument, nullptr, 1007},
        {"raw", no_argument, nullptr, 1008},
        {"stdin", no_argument, nullptr, 1009},
        {"named", no_argument, nullptr, 1010},
        {nullptr, 0, nullptr, 0}
    };
    
    int opt;
    int optionIndex = 0;
    
    // Reset getopt
    optind = 1;
    
    while ((opt = getopt_long(argc, argv, "hvc:d:", longOptions, &optionIndex)) != -1) {
        switch (opt) {
            case 'h':
                config.showHelp = true;
                return true;
            case 'v':
                config.showVersion = true;
                return true;
            case 'c':
                config.configFile = optarg;
                break;
            case 'd':
                config.dataDir = optarg;
                break;
            case 1001:  // --testnet
                config.testnet = true;
                break;
            case 1002:  // --regtest
                config.regtest = true;
                break;
            case 1003:  // --rpcconnect
                config.rpcHost = optarg;
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
            case 1007:  // --rpccookiefile
                config.rpcCookieFile = optarg;
                break;
            case 1008:  // --raw
                config.rawOutput = true;
                config.prettyPrint = false;
                break;
            case 1009:  // --stdin
                config.stdinMode = true;
                break;
            case 1010:  // --named
                config.namedParams = true;
                break;
            case '?':
            default:
                return false;
        }
    }
    
    // Remaining arguments are command and params
    for (int i = optind; i < argc; ++i) {
        if (config.method.empty()) {
            config.method = argv[i];
        } else {
            config.args.push_back(argv[i]);
        }
    }
    
    // Read additional args from stdin if requested
    if (config.stdinMode && !isatty(fileno(stdin))) {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty()) {
                config.args.push_back(line);
            }
        }
    }
    
    return true;
}

// ============================================================================
// JSON Argument Parsing
// ============================================================================

rpc::JSONValue ParseArgument(const std::string& arg) {
    // Try to parse as JSON first
    auto jsonVal = rpc::JSONValue::TryParse(arg);
    if (jsonVal.has_value()) {
        return *jsonVal;
    }
    
    // Try to parse as number
    try {
        // Check if it looks like an integer
        bool isInteger = true;
        bool hasDecimal = false;
        size_t start = 0;
        
        if (!arg.empty() && (arg[0] == '-' || arg[0] == '+')) {
            start = 1;
        }
        
        for (size_t i = start; i < arg.size(); ++i) {
            if (arg[i] == '.') {
                hasDecimal = true;
                isInteger = false;
            } else if (!std::isdigit(arg[i])) {
                isInteger = false;
                hasDecimal = false;
                break;
            }
        }
        
        if (isInteger && !hasDecimal && !arg.empty()) {
            int64_t val = std::stoll(arg);
            return rpc::JSONValue(val);
        } else if (hasDecimal) {
            double val = std::stod(arg);
            return rpc::JSONValue(val);
        }
    } catch (...) {
        // Not a number
    }
    
    // Check for boolean
    if (arg == "true" || arg == "false") {
        return rpc::JSONValue(arg == "true");
    }
    
    // Check for null
    if (arg == "null") {
        return rpc::JSONValue();
    }
    
    // Return as string
    return rpc::JSONValue(arg);
}

rpc::JSONValue BuildParams(const CLIConfig& config) {
    if (config.args.empty()) {
        return rpc::JSONValue();
    }
    
    if (config.namedParams) {
        // Parse as key=value pairs
        rpc::JSONValue::Object obj;
        for (const auto& arg : config.args) {
            size_t eqPos = arg.find('=');
            if (eqPos != std::string::npos) {
                std::string key = arg.substr(0, eqPos);
                std::string value = arg.substr(eqPos + 1);
                obj[key] = ParseArgument(value);
            } else {
                // Argument without = is treated as positional (error)
                std::cerr << "Warning: Named parameter without value: " << arg << "\n";
            }
        }
        return rpc::JSONValue(std::move(obj));
    } else {
        // Parse as positional array
        rpc::JSONValue::Array arr;
        for (const auto& arg : config.args) {
            arr.push_back(ParseArgument(arg));
        }
        return rpc::JSONValue(std::move(arr));
    }
}

// ============================================================================
// Result Formatting
// ============================================================================

std::string FormatValue(const rpc::JSONValue& value, int indent = 0) {
    std::ostringstream ss;
    std::string pad(indent * 2, ' ');
    
    switch (value.GetType()) {
        case rpc::JSONValue::Type::Null:
            ss << "null";
            break;
            
        case rpc::JSONValue::Type::Bool:
            ss << (value.GetBool() ? "true" : "false");
            break;
            
        case rpc::JSONValue::Type::Int:
            ss << value.GetInt();
            break;
            
        case rpc::JSONValue::Type::Double:
            ss << std::fixed << std::setprecision(8) << value.GetDouble();
            break;
            
        case rpc::JSONValue::Type::String:
            ss << value.GetString();
            break;
            
        case rpc::JSONValue::Type::Array:
            if (value.Size() == 0) {
                ss << "[]";
            } else {
                ss << "[\n";
                for (size_t i = 0; i < value.Size(); ++i) {
                    ss << std::string((indent + 1) * 2, ' ');
                    // Check if element is complex
                    const auto& elem = value[i];
                    if (elem.IsObject() || elem.IsArray()) {
                        ss << FormatValue(elem, indent + 1);
                    } else {
                        ss << FormatValue(elem, 0);
                    }
                    if (i + 1 < value.Size()) ss << ",";
                    ss << "\n";
                }
                ss << pad << "]";
            }
            break;
            
        case rpc::JSONValue::Type::Object: {
            const auto& obj = value.GetObject();
            if (obj.empty()) {
                ss << "{}";
            } else {
                ss << "{\n";
                size_t count = 0;
                for (const auto& [key, val] : obj) {
                    ss << std::string((indent + 1) * 2, ' ');
                    ss << "\"" << key << "\": ";
                    if (val.IsObject() || val.IsArray()) {
                        ss << FormatValue(val, indent + 1);
                    } else {
                        ss << FormatValue(val, 0);
                    }
                    if (++count < obj.size()) ss << ",";
                    ss << "\n";
                }
                ss << pad << "}";
            }
            break;
        }
    }
    
    return ss.str();
}

void PrintResult(const rpc::JSONValue& result, bool pretty, bool raw) {
    if (raw) {
        std::cout << result.ToJSON(false) << "\n";
    } else if (pretty) {
        std::cout << FormatValue(result) << "\n";
    } else {
        std::cout << result.ToJSON(true) << "\n";
    }
}

void PrintError(int code, const std::string& message) {
    std::cerr << "error code: " << code << "\n";
    std::cerr << "error message:\n" << message << "\n";
}

// ============================================================================
// RPC Call
// ============================================================================

int ExecuteCommand(const CLIConfig& config) {
    // Configure RPC client
    rpc::RPCClientConfig clientConfig;
    clientConfig.host = config.rpcHost;
    clientConfig.port = config.rpcPort;
    clientConfig.rpcUser = config.rpcUser;
    clientConfig.rpcPassword = config.rpcPassword;
    clientConfig.useSSL = config.useSSL;
    clientConfig.connectTimeout = 5;
    clientConfig.requestTimeout = 300;  // Long timeout for slow operations
    
    // Create client
    rpc::RPCClient client(clientConfig);
    
    // Connect
    if (!client.Connect()) {
        std::cerr << "error: Could not connect to server " 
                  << config.rpcHost << ":" << config.rpcPort << "\n";
        std::cerr << "Make sure shuriumd is running and RPC is enabled.\n";
        return 1;
    }
    
    // Build request parameters
    rpc::JSONValue params = BuildParams(config);
    
    // Make RPC call
    rpc::RPCResponse response = client.Call(config.method, params);
    
    // Handle response
    if (response.IsError()) {
        PrintError(response.GetErrorCode(), response.GetErrorMessage());
        return response.GetErrorCode();
    }
    
    // Print result
    PrintResult(response.GetResult(), config.prettyPrint, config.rawOutput);
    
    return 0;
}

// ============================================================================
// Built-in Commands
// ============================================================================

int HandleBuiltinCommand(const CLIConfig& config) {
    // Handle local help without connecting to server
    if (config.method == "help" && config.args.empty()) {
        std::cout << "== Blockchain ==\n";
        std::cout << "getblockchaininfo        Returns blockchain state info\n";
        std::cout << "getbestblockhash         Returns the best block hash\n";
        std::cout << "getblockcount            Returns the block count\n";
        std::cout << "getblock <hash>          Returns block data\n";
        std::cout << "getblockhash <height>    Returns block hash at height\n";
        std::cout << "getblockheader <hash>    Returns block header\n";
        std::cout << "getchaintips             Returns chain tips\n";
        std::cout << "getdifficulty            Returns difficulty\n";
        std::cout << "getmempoolinfo           Returns mempool info\n";
        std::cout << "getrawmempool            Returns mempool transactions\n";
        std::cout << "gettransaction <txid>    Returns transaction details\n";
        std::cout << "getrawtransaction <txid> Returns raw transaction\n";
        std::cout << "sendrawtransaction <hex> Broadcasts raw transaction\n";
        std::cout << "\n== Network ==\n";
        std::cout << "getnetworkinfo           Returns network info\n";
        std::cout << "getpeerinfo              Returns peer info\n";
        std::cout << "getconnectioncount       Returns connection count\n";
        std::cout << "addnode <ip> <cmd>       Add/remove node\n";
        std::cout << "disconnectnode <ip>      Disconnect from node\n";
        std::cout << "ping                     Ping all peers\n";
        std::cout << "\n== Wallet ==\n";
        std::cout << "getwalletinfo            Returns wallet info\n";
        std::cout << "getbalance               Returns wallet balance\n";
        std::cout << "getnewaddress            Generates new address\n";
        std::cout << "listaddresses            Lists wallet addresses\n";
        std::cout << "sendtoaddress <addr> <amt>  Send to address\n";
        std::cout << "listtransactions         Lists wallet transactions\n";
        std::cout << "listunspent              Lists unspent outputs\n";
        std::cout << "signmessage <addr> <msg> Sign a message\n";
        std::cout << "verifymessage ...        Verify signed message\n";
        std::cout << "walletlock               Lock the wallet\n";
        std::cout << "walletpassphrase <pw> <t> Unlock wallet\n";
        std::cout << "backupwallet <file>      Backup wallet\n";
        std::cout << "\n== Identity ==\n";
        std::cout << "getidentityinfo <addr>   Get identity info\n";
        std::cout << "createidentity <proof>   Create identity\n";
        std::cout << "claimubi <id>            Claim UBI\n";
        std::cout << "getubiinfo <id>          Get UBI info\n";
        std::cout << "\n== Staking ==\n";
        std::cout << "getstakinginfo           Returns staking info\n";
        std::cout << "listvalidators           Lists validators\n";
        std::cout << "getvalidatorinfo <id>    Get validator info\n";
        std::cout << "createvalidator ...      Register as validator\n";
        std::cout << "delegate <id> <amt>      Delegate stake\n";
        std::cout << "undelegate <id> <amt>    Undelegate stake\n";
        std::cout << "listdelegations          List delegations\n";
        std::cout << "claimrewards             Claim staking rewards\n";
        std::cout << "\n== Governance ==\n";
        std::cout << "getgovernanceinfo        Returns governance info\n";
        std::cout << "listproposals            Lists proposals\n";
        std::cout << "getproposal <id>         Get proposal details\n";
        std::cout << "createproposal ...       Create proposal\n";
        std::cout << "vote <id> <choice>       Vote on proposal\n";
        std::cout << "listparameters           List governance params\n";
        std::cout << "\n== Mining ==\n";
        std::cout << "getmininginfo            Returns mining info\n";
        std::cout << "getblocktemplate         Get block template\n";
        std::cout << "submitblock <hex>        Submit block\n";
        std::cout << "getwork                  Get PoUW problem\n";
        std::cout << "submitwork <id> <sol>    Submit solution\n";
        std::cout << "\n== Utility ==\n";
        std::cout << "help [command]           Show help\n";
        std::cout << "stop                     Stop the daemon\n";
        std::cout << "uptime                   Get daemon uptime\n";
        std::cout << "getmemoryinfo            Memory usage info\n";
        std::cout << "validateaddress <addr>   Validate an address\n";
        std::cout << "estimatefee <nblocks>    Estimate fee\n";
        std::cout << "\nUse 'shurium-cli help <command>' for more info on a command.\n";
        return 0;
    }
    
    return -1;  // Not a built-in command
}

// ============================================================================
// Main Entry Point
// ============================================================================

int AppMain(int argc, char* argv[]) {
    CLIConfig config;
    
    // Parse command line
    if (!ParseCommandLine(argc, argv, config)) {
        std::cerr << "Error parsing command line. Use --help for usage.\n";
        return 1;
    }
    
    // Handle special flags
    if (config.showHelp) {
        PrintHelp();
        return 0;
    }
    
    if (config.showVersion) {
        PrintVersion();
        return 0;
    }
    
    // Check for command
    if (config.method.empty()) {
        std::cerr << "Error: No command specified.\n";
        std::cerr << "Use 'shurium-cli --help' for usage information.\n";
        return 1;
    }
    
    // Load config file
    LoadConfigFile(config);
    
    // Adjust port for network
    if (config.testnet && config.rpcPort == defaults::RPC_PORT) {
        config.rpcPort = defaults::TESTNET_RPC_PORT;
    } else if (config.regtest && config.rpcPort == defaults::RPC_PORT) {
        config.rpcPort = defaults::REGTEST_RPC_PORT;
    }
    
    // Try built-in commands first
    int builtinResult = HandleBuiltinCommand(config);
    if (builtinResult != -1) {
        return builtinResult;
    }
    
    // Execute RPC command
    return ExecuteCommand(config);
}

} // namespace cli
} // namespace shurium

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        return shurium::cli::AppMain(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error" << std::endl;
        return 1;
    }
}
