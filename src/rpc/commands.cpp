// SHURIUM - RPC Commands Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

// Include the actual chain and mempool headers BEFORE commands.h
// to provide the real definitions that match the forward declarations.
#include <shurium/chain/chainstate.h>
#include <shurium/chain/blockindex.h>
#include <shurium/mempool/mempool.h>
#include <shurium/db/blockdb.h>
#include <shurium/consensus/params.h>
#include <shurium/consensus/validation.h>
#include <shurium/wallet/wallet.h>
#include <shurium/miner/blockassembler.h>
#include <shurium/miner/miner.h>
#include <shurium/network/message_processor.h>
#include <shurium/network/network_manager.h>
#include <shurium/identity/identity.h>
#include <shurium/economics/ubi.h>
#include <shurium/staking/staking.h>
#include <shurium/governance/governance.h>
#include <shurium/marketplace/marketplace.h>
#include <shurium/marketplace/problem.h>
#include <shurium/marketplace/solution.h>
#include <shurium/marketplace/verifier.h>
#include <shurium/node/context.h>
#include <shurium/util/logging.h>

#include <shurium/rpc/commands.h>
#include <shurium/rpc/server.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace shurium {
namespace rpc {

// ============================================================================
// Static Helper Variables
// ============================================================================

static std::chrono::steady_clock::time_point g_startTime = std::chrono::steady_clock::now();

// Helper to join directory path with filename
static std::string JoinWalletPath(const std::string& dir, const std::string& filename) {
    if (dir.empty()) {
        return filename;
    }
#ifdef _WIN32
    return dir + "\\" + filename;
#else
    return dir + "/" + filename;
#endif
}

// ============================================================================
// Helper Functions Implementation
// ============================================================================

Amount ParseAmount(const JSONValue& value) {
    if (value.IsInt()) {
        return value.GetInt();
    }
    if (value.IsDouble()) {
        double d = value.GetDouble();
        return static_cast<Amount>(d * COIN);
    }
    if (value.IsString()) {
        double d = std::stod(value.GetString());
        return static_cast<Amount>(d * COIN);
    }
    throw std::runtime_error("Invalid amount format");
}

JSONValue FormatAmount(Amount amount) {
    double value = static_cast<double>(amount) / COIN;
    return JSONValue(value);
}

std::string ParseAddress(const std::string& str) {
    // Basic validation - just return trimmed string
    std::string result = str;
    // Trim whitespace
    size_t start = result.find_first_not_of(" \t\n\r");
    size_t end = result.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return result.substr(start, end - start + 1);
}

bool ValidateAddress(const std::string& address) {
    // Check for bech32 addresses (shr1... or tshr1...)
    if (address.substr(0, 4) == "shr1" || address.substr(0, 5) == "tshr1") {
        // Bech32 address validation
        auto decoded = DecodeBech32(address);
        return decoded.has_value();
    }
    
    // Legacy base58 validation: check length and character set
    if (address.length() < 26 || address.length() > 62) {
        return false;
    }
    // Check for valid base58 characters (no 0, O, I, l)
    for (char c : address) {
        if (!std::isalnum(c)) return false;
        if (c == '0' || c == 'O' || c == 'I' || c == 'l') return false;
    }
    return true;
}

std::vector<Byte> ParseHex(const std::string& hex) {
    std::vector<Byte> result;
    result.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        if (i + 1 >= hex.size()) break;
        
        std::string byteStr = hex.substr(i, 2);
        Byte b = static_cast<Byte>(std::stoul(byteStr, nullptr, 16));
        result.push_back(b);
    }
    return result;
}

std::string FormatHex(const std::vector<Byte>& bytes) {
    return FormatHex(bytes.data(), bytes.size());
}

std::string FormatHex(const Byte* data, size_t len) {
    static const char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);
    
    for (size_t i = 0; i < len; ++i) {
        result += hexChars[(data[i] >> 4) & 0x0F];
        result += hexChars[data[i] & 0x0F];
    }
    return result;
}

// Template specializations for GetRequiredParam
template<>
std::string GetRequiredParam<std::string>(const RPCRequest& req, const std::string& name) {
    const JSONValue& param = req.GetParam(name);
    if (param.IsNull()) {
        throw std::runtime_error("Missing required parameter: " + name);
    }
    if (!param.IsString()) {
        throw std::runtime_error("Parameter must be string: " + name);
    }
    return param.GetString();
}

template<>
std::string GetRequiredParam<std::string>(const RPCRequest& req, size_t index) {
    const JSONValue& param = req.GetParam(index);
    if (param.IsNull()) {
        throw std::runtime_error("Missing required parameter at index " + std::to_string(index));
    }
    if (!param.IsString()) {
        throw std::runtime_error("Parameter at index " + std::to_string(index) + " must be string");
    }
    return param.GetString();
}

template<>
int64_t GetRequiredParam<int64_t>(const RPCRequest& req, const std::string& name) {
    const JSONValue& param = req.GetParam(name);
    if (param.IsNull()) {
        throw std::runtime_error("Missing required parameter: " + name);
    }
    if (!param.IsInt()) {
        throw std::runtime_error("Parameter must be integer: " + name);
    }
    return param.GetInt();
}

template<>
int64_t GetRequiredParam<int64_t>(const RPCRequest& req, size_t index) {
    const JSONValue& param = req.GetParam(index);
    if (param.IsNull()) {
        throw std::runtime_error("Missing required parameter at index " + std::to_string(index));
    }
    if (!param.IsInt()) {
        throw std::runtime_error("Parameter at index " + std::to_string(index) + " must be integer");
    }
    return param.GetInt();
}

template<>
bool GetRequiredParam<bool>(const RPCRequest& req, const std::string& name) {
    const JSONValue& param = req.GetParam(name);
    if (param.IsNull()) {
        throw std::runtime_error("Missing required parameter: " + name);
    }
    if (!param.IsBool()) {
        throw std::runtime_error("Parameter must be boolean: " + name);
    }
    return param.GetBool();
}

template<>
bool GetRequiredParam<bool>(const RPCRequest& req, size_t index) {
    const JSONValue& param = req.GetParam(index);
    if (param.IsNull()) {
        throw std::runtime_error("Missing required parameter at index " + std::to_string(index));
    }
    if (!param.IsBool()) {
        throw std::runtime_error("Parameter at index " + std::to_string(index) + " must be boolean");
    }
    return param.GetBool();
}

template<>
double GetRequiredParam<double>(const RPCRequest& req, const std::string& name) {
    const JSONValue& param = req.GetParam(name);
    if (param.IsNull()) {
        throw std::runtime_error("Missing required parameter: " + name);
    }
    if (!param.IsNumber()) {
        throw std::runtime_error("Parameter must be number: " + name);
    }
    return param.GetDouble();
}

// Template specializations for GetOptionalParam
template<>
std::string GetOptionalParam<std::string>(const RPCRequest& req, const std::string& name, const std::string& defaultValue) {
    const JSONValue& param = req.GetParam(name);
    if (param.IsNull()) return defaultValue;
    return param.IsString() ? param.GetString() : defaultValue;
}

template<>
std::string GetOptionalParam<std::string>(const RPCRequest& req, size_t index, const std::string& defaultValue) {
    const JSONValue& param = req.GetParam(index);
    if (param.IsNull()) return defaultValue;
    return param.IsString() ? param.GetString() : defaultValue;
}

template<>
int64_t GetOptionalParam<int64_t>(const RPCRequest& req, const std::string& name, const int64_t& defaultValue) {
    const JSONValue& param = req.GetParam(name);
    if (param.IsNull()) return defaultValue;
    return param.IsInt() ? param.GetInt() : defaultValue;
}

template<>
int64_t GetOptionalParam<int64_t>(const RPCRequest& req, size_t index, const int64_t& defaultValue) {
    const JSONValue& param = req.GetParam(index);
    if (param.IsNull()) return defaultValue;
    return param.IsInt() ? param.GetInt() : defaultValue;
}

template<>
bool GetOptionalParam<bool>(const RPCRequest& req, const std::string& name, const bool& defaultValue) {
    const JSONValue& param = req.GetParam(name);
    if (param.IsNull()) return defaultValue;
    return param.IsBool() ? param.GetBool() : defaultValue;
}

template<>
bool GetOptionalParam<bool>(const RPCRequest& req, size_t index, const bool& defaultValue) {
    const JSONValue& param = req.GetParam(index);
    if (param.IsNull()) return defaultValue;
    return param.IsBool() ? param.GetBool() : defaultValue;
}

template<>
double GetOptionalParam<double>(const RPCRequest& req, const std::string& name, const double& defaultValue) {
    const JSONValue& param = req.GetParam(name);
    if (param.IsNull()) return defaultValue;
    return param.IsNumber() ? param.GetDouble() : defaultValue;
}

// ============================================================================
// RPCCommandTable Implementation
// ============================================================================

RPCCommandTable::RPCCommandTable() {
}

RPCCommandTable::~RPCCommandTable() {
}

void RPCCommandTable::SetChainState(std::shared_ptr<ChainState> chainState) {
    chainState_ = std::move(chainState);
}

void RPCCommandTable::SetChainStateManager(std::shared_ptr<ChainStateManager> chainManager) {
    chainManager_ = std::move(chainManager);
}

void RPCCommandTable::SetMempool(std::shared_ptr<Mempool> mempool) {
    mempool_ = std::move(mempool);
}

void RPCCommandTable::SetWallet(std::shared_ptr<wallet::Wallet> wallet) {
    wallet_ = std::move(wallet);
}

void RPCCommandTable::SetIdentityManager(std::shared_ptr<identity::IdentityManager> identity) {
    identity_ = std::move(identity);
}

void RPCCommandTable::SetUBIDistributor(std::shared_ptr<economics::UBIDistributor> ubi) {
    ubiDistributor_ = std::move(ubi);
}

void RPCCommandTable::SetStakingEngine(std::shared_ptr<staking::StakingEngine> staking) {
    staking_ = std::move(staking);
}

void RPCCommandTable::SetGovernanceEngine(std::shared_ptr<governance::GovernanceEngine> governance) {
    governance_ = std::move(governance);
}

void RPCCommandTable::SetNetworkManager(std::shared_ptr<network::NetworkManager> network) {
    network_ = std::move(network);
}

void RPCCommandTable::SetMessageProcessor(MessageProcessor* msgproc) {
    msgproc_ = msgproc;
}

void RPCCommandTable::SetBlockDB(std::shared_ptr<db::BlockDB> blockdb) {
    blockdb_ = std::move(blockdb);
}

void RPCCommandTable::SetDataDir(const std::string& dataDir) {
    dataDir_ = dataDir;
}

void RPCCommandTable::RegisterCommands(RPCServer& server) {
    // Register all command categories
    RegisterBlockchainCommands();
    RegisterNetworkCommands();
    RegisterWalletCommands();
    RegisterIdentityCommands();
    RegisterStakingCommands();
    RegisterGovernanceCommands();
    RegisterMiningCommands();
    RegisterUtilityCommands();
    
    // Register all commands with the server
    for (const auto& cmd : commands_) {
        server.RegisterMethod(cmd);
    }
}

std::vector<RPCMethod> RPCCommandTable::GetAllCommands() const {
    return commands_;
}

std::vector<RPCMethod> RPCCommandTable::GetCommandsByCategory(const std::string& category) const {
    std::vector<RPCMethod> result;
    for (const auto& cmd : commands_) {
        if (cmd.category == category) {
            result.push_back(cmd);
        }
    }
    return result;
}


// ============================================================================
// Command Registration - Blockchain
// ============================================================================

void RPCCommandTable::RegisterBlockchainCommands() {
    RPCCommandTable* table = this;
    
    commands_.push_back({
        "getblockchaininfo",
        Category::BLOCKCHAIN,
        "Returns an object containing various state info regarding blockchain processing.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getblockchaininfo(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getbestblockhash",
        Category::BLOCKCHAIN,
        "Returns the hash of the best (tip) block in the most-work fully-validated chain.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getbestblockhash(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getblockcount",
        Category::BLOCKCHAIN,
        "Returns the height of the most-work fully-validated chain.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getblockcount(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getblock",
        Category::BLOCKCHAIN,
        "Returns block data for the given block hash.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getblock(req, ctx, table);
        },
        false, false,
        {"blockhash", "verbosity"},
        {"The block hash", "0 for hex, 1 for JSON, 2 for JSON with tx details"}
    });
    
    commands_.push_back({
        "getblockhash",
        Category::BLOCKCHAIN,
        "Returns hash of block at given height.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getblockhash(req, ctx, table);
        },
        false, false,
        {"height"},
        {"The height index"}
    });
    
    commands_.push_back({
        "getblockheader",
        Category::BLOCKCHAIN,
        "Returns block header for the given block hash.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getblockheader(req, ctx, table);
        },
        false, false,
        {"blockhash", "verbose"},
        {"The block hash", "true for JSON, false for hex"}
    });
    
    commands_.push_back({
        "getchaintips",
        Category::BLOCKCHAIN,
        "Return information about all known tips in the block tree.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getchaintips(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getdifficulty",
        Category::BLOCKCHAIN,
        "Returns the proof-of-work difficulty as a multiple of the minimum difficulty.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getdifficulty(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getmempoolinfo",
        Category::BLOCKCHAIN,
        "Returns details on the active state of the TX memory pool.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getmempoolinfo(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getrawmempool",
        Category::BLOCKCHAIN,
        "Returns all transaction ids in memory pool.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getrawmempool(req, ctx, table);
        },
        false, false,
        {"verbose"},
        {"True for JSON object, false for array of txids"}
    });
    
    commands_.push_back({
        "gettransaction",
        Category::BLOCKCHAIN,
        "Get detailed information about a transaction.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_gettransaction(req, ctx, table);
        },
        false, false,
        {"txid"},
        {"The transaction id"}
    });
    
    commands_.push_back({
        "getrawtransaction",
        Category::BLOCKCHAIN,
        "Return the raw transaction data.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getrawtransaction(req, ctx, table);
        },
        false, false,
        {"txid", "verbose"},
        {"The transaction id", "If true, return JSON object"}
    });
    
    commands_.push_back({
        "decoderawtransaction",
        Category::BLOCKCHAIN,
        "Decode a hex-encoded transaction.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_decoderawtransaction(req, ctx, table);
        },
        false, false,
        {"hexstring"},
        {"The transaction hex string"}
    });
    
    commands_.push_back({
        "sendrawtransaction",
        Category::BLOCKCHAIN,
        "Submit a raw transaction to the network.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_sendrawtransaction(req, ctx, table);
        },
        false, false,
        {"hexstring"},
        {"The hex string of the raw transaction"}
    });
}

// ============================================================================
// Command Registration - Network
// ============================================================================

void RPCCommandTable::RegisterNetworkCommands() {
    RPCCommandTable* table = this;
    
    commands_.push_back({
        "getnetworkinfo",
        Category::NETWORK,
        "Returns information about the network.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getnetworkinfo(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getpeerinfo",
        Category::NETWORK,
        "Returns data about each connected network node.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getpeerinfo(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getconnectioncount",
        Category::NETWORK,
        "Returns the number of connections to other nodes.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getconnectioncount(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "addnode",
        Category::NETWORK,
        "Add or remove a node from the addnode list.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_addnode(req, ctx, table);
        },
        true, false,
        {"node", "command"},
        {"The node address", "add, remove, or onetry"}
    });
    
    commands_.push_back({
        "disconnectnode",
        Category::NETWORK,
        "Disconnect from a specified node.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_disconnectnode(req, ctx, table);
        },
        true, false,
        {"address"},
        {"The IP address/port of the node"}
    });
    
    commands_.push_back({
        "getaddednodeinfo",
        Category::NETWORK,
        "Returns information about the given added node.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getaddednodeinfo(req, ctx, table);
        },
        false, false,
        {"node"},
        {"The node address (optional)"}
    });
    
    commands_.push_back({
        "setnetworkactive",
        Category::NETWORK,
        "Disable/enable all p2p network activity.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_setnetworkactive(req, ctx, table);
        },
        true, false,
        {"state"},
        {"true to enable, false to disable"}
    });
    
    commands_.push_back({
        "ping",
        Category::NETWORK,
        "Request a ping to all connected peers.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_ping(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
}


// ============================================================================
// Command Registration - Wallet
// ============================================================================

void RPCCommandTable::RegisterWalletCommands() {
    RPCCommandTable* table = this;
    
    commands_.push_back({
        "getwalletinfo",
        Category::WALLET,
        "Returns wallet state info.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getwalletinfo(req, ctx, table);
        },
        false, true,
        {},
        {}
    });
    
    commands_.push_back({
        "getbalance",
        Category::WALLET,
        "Returns the total available balance.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getbalance(req, ctx, table);
        },
        false, true,
        {"minconf"},
        {"Minimum confirmations (default=1)"}
    });
    
    commands_.push_back({
        "getunconfirmedbalance",
        Category::WALLET,
        "Returns the unconfirmed balance.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getunconfirmedbalance(req, ctx, table);
        },
        false, true,
        {},
        {}
    });
    
    commands_.push_back({
        "getnewaddress",
        Category::WALLET,
        "Returns a new address for receiving payments.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getnewaddress(req, ctx, table);
        },
        false, true,
        {"label"},
        {"Address label (optional)"}
    });
    
    commands_.push_back({
        "getaddressinfo",
        Category::WALLET,
        "Return information about the given address.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getaddressinfo(req, ctx, table);
        },
        false, true,
        {"address"},
        {"The address to look up"}
    });
    
    commands_.push_back({
        "listaddresses",
        Category::WALLET,
        "Returns list of wallet addresses.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_listaddresses(req, ctx, table);
        },
        false, true,
        {},
        {}
    });
    
    commands_.push_back({
        "sendtoaddress",
        Category::WALLET,
        "Send an amount to a given address.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_sendtoaddress(req, ctx, table);
        },
        true, true,
        {"address", "amount", "comment"},
        {"The destination address", "The amount to send", "A comment (optional)"}
    });
    
    commands_.push_back({
        "sendmany",
        Category::WALLET,
        "Send multiple times to multiple addresses.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_sendmany(req, ctx, table);
        },
        true, true,
        {"amounts"},
        {"A JSON object with addresses and amounts"}
    });
    
    commands_.push_back({
        "listtransactions",
        Category::WALLET,
        "Returns recent transactions for the wallet.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_listtransactions(req, ctx, table);
        },
        false, true,
        {"count", "skip"},
        {"Number of transactions (default=10)", "Number to skip (default=0)"}
    });
    
    commands_.push_back({
        "listunspent",
        Category::WALLET,
        "Returns unspent transaction outputs.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_listunspent(req, ctx, table);
        },
        false, true,
        {"minconf", "maxconf"},
        {"Minimum confirmations", "Maximum confirmations"}
    });
    
    commands_.push_back({
        "signmessage",
        Category::WALLET,
        "Sign a message with the private key of an address.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_signmessage(req, ctx, table);
        },
        true, true,
        {"address", "message"},
        {"The address to use", "The message to sign"}
    });
    
    commands_.push_back({
        "verifymessage",
        Category::WALLET,
        "Verify a signed message.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_verifymessage(req, ctx, table);
        },
        false, false,
        {"address", "signature", "message"},
        {"The address", "The signature", "The message"}
    });
    
    commands_.push_back({
        "dumpprivkey",
        Category::WALLET,
        "Reveals the private key corresponding to an address.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_dumpprivkey(req, ctx, table);
        },
        true, true,
        {"address"},
        {"The address for the private key"}
    });
    
    commands_.push_back({
        "importprivkey",
        Category::WALLET,
        "Adds a private key to your wallet.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_importprivkey(req, ctx, table);
        },
        true, true,
        {"privkey", "label", "rescan"},
        {"The private key", "An optional label", "Rescan the wallet (default=true)"}
    });
    
    commands_.push_back({
        "walletlock",
        Category::WALLET,
        "Removes the wallet encryption key from memory, locking the wallet.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_walletlock(req, ctx, table);
        },
        true, true,
        {},
        {}
    });
    
    commands_.push_back({
        "walletpassphrase",
        Category::WALLET,
        "Stores the wallet decryption key in memory for timeout seconds.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_walletpassphrase(req, ctx, table);
        },
        true, true,
        {"passphrase", "timeout"},
        {"The wallet passphrase", "Timeout in seconds"}
    });
    
    commands_.push_back({
        "walletpassphrasechange",
        Category::WALLET,
        "Changes the wallet passphrase.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_walletpassphrasechange(req, ctx, table);
        },
        true, true,
        {"oldpassphrase", "newpassphrase"},
        {"The current passphrase", "The new passphrase"}
    });
    
    commands_.push_back({
        "encryptwallet",
        Category::WALLET,
        "Encrypts the wallet with a passphrase.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_encryptwallet(req, ctx, table);
        },
        true, true,
        {"passphrase"},
        {"The passphrase to encrypt with"}
    });
    
    commands_.push_back({
        "backupwallet",
        Category::WALLET,
        "Safely copies wallet file to destination.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_backupwallet(req, ctx, table);
        },
        true, true,
        {"destination"},
        {"The destination filename"}
    });
    
    commands_.push_back({
        "loadwallet",
        Category::WALLET,
        "Loads a wallet from a wallet file.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_loadwallet(req, ctx, table);
        },
        false, false,
        {"filename"},
        {"The wallet file to load (in data directory)"}
    });
    
    commands_.push_back({
        "createwallet",
        Category::WALLET,
        "Creates a new wallet.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_createwallet(req, ctx, table);
        },
        false, false,
        {"wallet_name", "passphrase"},
        {"The name for the new wallet", "Optional passphrase to encrypt"}
    });
    
    commands_.push_back({
        "unloadwallet",
        Category::WALLET,
        "Unloads the current wallet.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_unloadwallet(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "restorewallet",
        Category::WALLET,
        "Restores a wallet from a 24-word mnemonic phrase.\n"
        "Arguments:\n"
        "1. wallet_name  (string, required) Name for the wallet\n"
        "2. mnemonic     (string, required) 24-word recovery phrase\n"
        "3. passphrase   (string, optional) BIP39 passphrase (default: empty)",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_restorewallet(req, ctx, table);
        },
        false, false,
        {"wallet_name", "mnemonic", "passphrase"},
        {}
    });
}

// ============================================================================
// Command Registration - Identity
// ============================================================================

void RPCCommandTable::RegisterIdentityCommands() {
    RPCCommandTable* table = this;
    
    commands_.push_back({
        "getidentityinfo",
        Category::IDENTITY,
        "Returns identity information for an address.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getidentityinfo(req, ctx, table);
        },
        false, false,
        {"address"},
        {"The address to look up"}
    });
    
    commands_.push_back({
        "createidentity",
        Category::IDENTITY,
        "Creates a new identity with proof of uniqueness.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_createidentity(req, ctx, table);
        },
        true, true,
        {"proof"},
        {"The identity proof data"}
    });
    
    commands_.push_back({
        "verifyidentity",
        Category::IDENTITY,
        "Verifies an identity proof.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_verifyidentity(req, ctx, table);
        },
        false, false,
        {"identityid", "proof"},
        {"The identity ID", "The proof to verify"}
    });
    
    commands_.push_back({
        "getidentitystatus",
        Category::IDENTITY,
        "Returns the verification status of an identity.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getidentitystatus(req, ctx, table);
        },
        false, false,
        {"identityid"},
        {"The identity ID"}
    });
    
    commands_.push_back({
        "claimubi",
        Category::IDENTITY,
        "Claims available UBI for a verified identity.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_claimubi(req, ctx, table);
        },
        true, true,
        {"identityid"},
        {"The identity ID to claim for"}
    });
    
    commands_.push_back({
        "getubiinfo",
        Category::IDENTITY,
        "Returns UBI information for an identity.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getubiinfo(req, ctx, table);
        },
        false, false,
        {"identityid"},
        {"The identity ID"}
    });
    
    commands_.push_back({
        "getubihistory",
        Category::IDENTITY,
        "Returns UBI claim history for an identity.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getubihistory(req, ctx, table);
        },
        false, false,
        {"identityid", "count"},
        {"The identity ID", "Number of records (default=10)"}
    });
}


// ============================================================================
// Command Registration - Staking
// ============================================================================

void RPCCommandTable::RegisterStakingCommands() {
    RPCCommandTable* table = this;
    
    commands_.push_back({
        "getstakinginfo",
        Category::STAKING,
        "Returns staking-related information.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getstakinginfo(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getvalidatorinfo",
        Category::STAKING,
        "Returns information about a validator.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getvalidatorinfo(req, ctx, table);
        },
        false, false,
        {"validatorid"},
        {"The validator ID or address"}
    });
    
    commands_.push_back({
        "listvalidators",
        Category::STAKING,
        "Returns list of validators.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_listvalidators(req, ctx, table);
        },
        false, false,
        {"status"},
        {"Filter by status (active, inactive, jailed, all)"}
    });
    
    commands_.push_back({
        "createvalidator",
        Category::STAKING,
        "Register as a validator.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_createvalidator(req, ctx, table);
        },
        true, true,
        {"amount", "commission", "moniker"},
        {"Initial stake amount", "Commission rate (basis points)", "Validator name"}
    });
    
    commands_.push_back({
        "updatevalidator",
        Category::STAKING,
        "Update validator parameters.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_updatevalidator(req, ctx, table);
        },
        true, true,
        {"validatorid", "commission", "moniker"},
        {"Validator ID", "New commission rate", "New name"}
    });
    
    commands_.push_back({
        "delegate",
        Category::STAKING,
        "Delegate stake to a validator.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_delegate(req, ctx, table);
        },
        true, true,
        {"validatorid", "amount"},
        {"The validator to delegate to", "Amount to delegate"}
    });
    
    commands_.push_back({
        "undelegate",
        Category::STAKING,
        "Undelegate stake from a validator.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_undelegate(req, ctx, table);
        },
        true, true,
        {"validatorid", "amount"},
        {"The validator to undelegate from", "Amount to undelegate"}
    });
    
    commands_.push_back({
        "listdelegations",
        Category::STAKING,
        "List your delegations.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_listdelegations(req, ctx, table);
        },
        false, true,
        {},
        {}
    });
    
    commands_.push_back({
        "claimrewards",
        Category::STAKING,
        "Claim pending staking rewards.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_claimrewards(req, ctx, table);
        },
        true, true,
        {"validatorid"},
        {"Validator to claim from (optional, all if omitted)"}
    });
    
    commands_.push_back({
        "getpendingrewards",
        Category::STAKING,
        "Get pending staking rewards.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getpendingrewards(req, ctx, table);
        },
        false, true,
        {},
        {}
    });
    
    commands_.push_back({
        "unjailvalidator",
        Category::STAKING,
        "Unjail a jailed validator.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_unjailvalidator(req, ctx, table);
        },
        true, true,
        {"validatorid"},
        {"The validator to unjail"}
    });
}

// ============================================================================
// Command Registration - Governance
// ============================================================================

void RPCCommandTable::RegisterGovernanceCommands() {
    RPCCommandTable* table = this;
    
    commands_.push_back({
        "getgovernanceinfo",
        Category::GOVERNANCE,
        "Returns governance-related information.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getgovernanceinfo(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "listproposals",
        Category::GOVERNANCE,
        "List governance proposals.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_listproposals(req, ctx, table);
        },
        false, false,
        {"status"},
        {"Filter by status (active, passed, rejected, all)"}
    });
    
    commands_.push_back({
        "getproposal",
        Category::GOVERNANCE,
        "Get details of a proposal.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getproposal(req, ctx, table);
        },
        false, false,
        {"proposalid"},
        {"The proposal ID"}
    });
    
    commands_.push_back({
        "createproposal",
        Category::GOVERNANCE,
        "Create a new governance proposal.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_createproposal(req, ctx, table);
        },
        true, true,
        {"type", "title", "description", "deposit"},
        {"Proposal type", "Title", "Description", "Deposit amount"}
    });
    
    commands_.push_back({
        "vote",
        Category::GOVERNANCE,
        "Vote on a proposal.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_vote(req, ctx, table);
        },
        true, true,
        {"proposalid", "choice"},
        {"The proposal ID", "Vote choice (yes, no, abstain, veto)"}
    });
    
    commands_.push_back({
        "getvoteinfo",
        Category::GOVERNANCE,
        "Get vote information for a proposal.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getvoteinfo(req, ctx, table);
        },
        false, false,
        {"proposalid", "voter"},
        {"The proposal ID", "Voter address (optional)"}
    });
    
    commands_.push_back({
        "delegatevote",
        Category::GOVERNANCE,
        "Delegate voting power to another address.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_delegatevote(req, ctx, table);
        },
        true, true,
        {"delegate"},
        {"The address to delegate to"}
    });
    
    commands_.push_back({
        "undelegatevote",
        Category::GOVERNANCE,
        "Remove voting power delegation.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_undelegatevote(req, ctx, table);
        },
        true, true,
        {},
        {}
    });
    
    commands_.push_back({
        "getparameter",
        Category::GOVERNANCE,
        "Get a governance parameter value.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getparameter(req, ctx, table);
        },
        false, false,
        {"name"},
        {"The parameter name"}
    });
    
    commands_.push_back({
        "listparameters",
        Category::GOVERNANCE,
        "List all governance parameters.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_listparameters(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
}

// ============================================================================
// Command Registration - Mining/PoUW
// ============================================================================

void RPCCommandTable::RegisterMiningCommands() {
    RPCCommandTable* table = this;
    
    commands_.push_back({
        "getmininginfo",
        Category::MINING,
        "Returns mining-related information.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getmininginfo(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getblocktemplate",
        Category::MINING,
        "Returns data needed to construct a block to work on.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getblocktemplate(req, ctx, table);
        },
        false, false,
        {"template_request"},
        {"A JSON object with template parameters"}
    });
    
    commands_.push_back({
        "submitblock",
        Category::MINING,
        "Attempts to submit new block to network.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_submitblock(req, ctx, table);
        },
        true, false,
        {"hexdata"},
        {"The hex-encoded block data"}
    });
    
    commands_.push_back({
        "getwork",
        Category::MINING,
        "Get a PoUW problem to work on.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getwork(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "submitwork",
        Category::MINING,
        "Submit a PoUW solution.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_submitwork(req, ctx, table);
        },
        true, false,
        {"problemid", "solution"},
        {"The problem ID", "The solution data"}
    });
    
    commands_.push_back({
        "listproblems",
        Category::MINING,
        "List available PoUW problems.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_listproblems(req, ctx, table);
        },
        false, false,
        {"status"},
        {"Filter by status (pending, assigned, solved, all)"}
    });
    
    commands_.push_back({
        "getproblem",
        Category::MINING,
        "Get details of a PoUW problem.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getproblem(req, ctx, table);
        },
        false, false,
        {"problemid"},
        {"The problem ID"}
    });
    
    commands_.push_back({
        "generatetoaddress",
        Category::MINING,
        "Mine blocks immediately to a specified address (regtest only).",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_generatetoaddress(req, ctx, table);
        },
        true, false,
        {"nblocks", "address"},
        {"How many blocks to generate", "Address to send rewards to"}
    });
}

// ============================================================================
// Command Registration - Utility
// ============================================================================

void RPCCommandTable::RegisterUtilityCommands() {
    RPCCommandTable* table = this;
    
    commands_.push_back({
        "help",
        Category::UTILITY,
        "List all commands, or get help for a specified command.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_help(req, ctx, table);
        },
        false, false,
        {"command"},
        {"The command to get help for (optional)"}
    });
    
    commands_.push_back({
        "stop",
        Category::UTILITY,
        "Stop the server.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_stop(req, ctx, table);
        },
        true, false,
        {},
        {}
    });
    
    commands_.push_back({
        "uptime",
        Category::UTILITY,
        "Returns the total uptime of the server.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_uptime(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "getmemoryinfo",
        Category::UTILITY,
        "Returns memory usage information.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_getmemoryinfo(req, ctx, table);
        },
        false, false,
        {},
        {}
    });
    
    commands_.push_back({
        "logging",
        Category::UTILITY,
        "Get or set logging configuration.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_logging(req, ctx, table);
        },
        true, false,
        {"include", "exclude"},
        {"Categories to include", "Categories to exclude"}
    });
    
    commands_.push_back({
        "echo",
        Category::UTILITY,
        "Echo back the input (for testing).",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_echo(req, ctx, table);
        },
        false, false,
        {"args"},
        {"Arguments to echo"}
    });
    
    commands_.push_back({
        "validateaddress",
        Category::UTILITY,
        "Return information about the given address.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_validateaddress(req, ctx, table);
        },
        false, false,
        {"address"},
        {"The address to validate"}
    });
    
    commands_.push_back({
        "createmultisig",
        Category::UTILITY,
        "Creates a multi-signature address.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_createmultisig(req, ctx, table);
        },
        false, false,
        {"nrequired", "keys"},
        {"Required signatures", "Public keys"}
    });
    
    commands_.push_back({
        "estimatefee",
        Category::UTILITY,
        "Estimates the fee per kilobyte.",
        [table](const RPCRequest& req, const RPCContext& ctx) {
            return cmd_estimatefee(req, ctx, table);
        },
        false, false,
        {"nblocks"},
        {"Target confirmation blocks"}
    });
}


// ============================================================================
// Blockchain Command Implementations
// ============================================================================

// Helper: Convert nBits to difficulty (Bitcoin-compatible formula)
static double GetDifficultyFromBits(uint32_t nBits) {
    // Extract exponent and mantissa from compact format
    int nShift = (nBits >> 24) & 0xff;
    double dDiff = static_cast<double>(0x0000ffff) / static_cast<double>(nBits & 0x00ffffff);
    
    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }
    
    return dDiff;
}

// Helper: Convert uint64_t chainwork to hex string (padded to 64 chars)
static std::string ChainWorkToHex(uint64_t chainWork) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(64) << std::hex << chainWork;
    return oss.str();
}

// Helper: Convert Hash256 (or derived types like BlockHash) to hex string
static std::string HashToHex(const Hash256& hash) {
    std::ostringstream oss;
    // Hash bytes need to be reversed for display (big-endian)
    for (int i = 31; i >= 0; --i) {
        oss << std::hex << std::setfill('0') << std::setw(2) 
            << static_cast<unsigned int>(hash[i]);
    }
    return oss.str();
}

// Convenience alias for BlockHash
static std::string BlockHashToHex(const BlockHash& hash) {
    return HashToHex(hash);
}

// Helper: Parse hex string to BlockHash
static BlockHash HexToBlockHash(const std::string& hex) {
    BlockHash hash;
    if (hex.size() != 64) return hash;
    
    // Convert from display format (big-endian) to internal format (little-endian)
    for (size_t i = 0; i < 32; ++i) {
        std::string byteStr = hex.substr(62 - i * 2, 2);
        hash[i] = static_cast<Byte>(std::stoul(byteStr, nullptr, 16));
    }
    return hash;
}

RPCResponse cmd_getblockchaininfo(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table) {
    JSONValue::Object result;
    
    // Default values
    result["chain"] = "main";
    result["blocks"] = int64_t(0);
    result["headers"] = int64_t(0);
    result["bestblockhash"] = "0000000000000000000000000000000000000000000000000000000000000000";
    result["difficulty"] = 1.0;
    result["mediantime"] = GetTime();
    result["verificationprogress"] = 1.0;
    result["initialblockdownload"] = false;
    result["chainwork"] = "0000000000000000000000000000000000000000000000000000000000000000";
    result["size_on_disk"] = int64_t(0);
    result["pruned"] = false;
    
    ChainState* chainState = table->GetChainState();
    if (chainState) {
        BlockIndex* tip = chainState->GetTip();
        if (tip) {
            result["blocks"] = static_cast<int64_t>(tip->nHeight);
            result["headers"] = static_cast<int64_t>(tip->nHeight); // Same as blocks when synced
            result["bestblockhash"] = BlockHashToHex(tip->GetBlockHash());
            result["difficulty"] = GetDifficultyFromBits(tip->nBits);
            result["mediantime"] = tip->GetMedianTimePast();
            result["chainwork"] = ChainWorkToHex(tip->nChainWork);
            
            // Calculate verification progress (simplified: assume synced if we have a tip)
            result["verificationprogress"] = 1.0;
            result["initialblockdownload"] = false;
        }
    }
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_getbestblockhash(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table) {
    std::string hash = "0000000000000000000000000000000000000000000000000000000000000000";
    
    ChainState* chainState = table->GetChainState();
    if (chainState) {
        BlockIndex* tip = chainState->GetTip();
        if (tip) {
            hash = BlockHashToHex(tip->GetBlockHash());
        }
    }
    
    return RPCResponse::Success(JSONValue(hash), req.GetId());
}

RPCResponse cmd_getblockcount(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    int64_t height = 0;
    
    ChainState* chainState = table->GetChainState();
    if (chainState) {
        height = chainState->GetHeight();
    }
    
    return RPCResponse::Success(JSONValue(height), req.GetId());
}

RPCResponse cmd_getblock(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table) {
    try {
        std::string blockhash = GetRequiredParam<std::string>(req, size_t(0));
        int64_t verbosity = GetOptionalParam<int64_t>(req, size_t(1), int64_t(1));
        
        // Parse the hash
        BlockHash hash = HexToBlockHash(blockhash);
        
        ChainState* chainState = table->GetChainState();
        if (!chainState) {
            return RPCError(-1, "Chain state not available", req.GetId());
        }
        
        // Look up the block index via the chain
        const Chain& chain = chainState->GetChain();
        BlockIndex* pindex = nullptr;
        
        // Search for the block in the chain by hash
        for (int h = chain.Height(); h >= 0; --h) {
            BlockIndex* idx = chain[h];
            if (idx && idx->GetBlockHash() == hash) {
                pindex = idx;
                break;
            }
        }
        
        if (!pindex) {
            return RPCError(-5, "Block not found", req.GetId());
        }
        
        // Verbosity 0: return hex-encoded serialized block
        if (verbosity == 0) {
            // Check if we have the block data
            if (!HasStatus(pindex->nStatus, BlockStatus::HAVE_DATA)) {
                return RPCError(-1, "Block data not available (pruned or not downloaded)", req.GetId());
            }
            
            db::BlockDB* blockdb = table->GetBlockDB();
            if (!blockdb) {
                return RPCError(-32603, "Block database not available", req.GetId());
            }
            
            // Read the block from disk
            db::DiskBlockPos pos(pindex->nFile, pindex->nDataPos);
            Block block;
            db::Status status = blockdb->ReadBlock(pos, block);
            
            if (!status.ok()) {
                return RPCError(-1, "Failed to read block from disk: " + status.ToString(), req.GetId());
            }
            
            // Serialize block to bytes
            DataStream ss;
            Serialize(ss, block);
            
            // Convert to hex
            std::string hexBlock = FormatHex(ss.data(), ss.TotalSize());
            
            return RPCResponse::Success(JSONValue(hexBlock), req.GetId());
        }
        
        // Build block info from BlockIndex
        JSONValue::Object result;
        result["hash"] = blockhash;
        
        // Calculate confirmations
        int confirmations = chain.Height() - pindex->nHeight + 1;
        result["confirmations"] = static_cast<int64_t>(confirmations);
        
        // Try to read actual block for size and transactions
        Block block;
        bool haveFullBlock = false;
        
        if (HasStatus(pindex->nStatus, BlockStatus::HAVE_DATA)) {
            db::BlockDB* blockdb = table->GetBlockDB();
            if (blockdb) {
                db::DiskBlockPos pos(pindex->nFile, pindex->nDataPos);
                db::Status status = blockdb->ReadBlock(pos, block);
                haveFullBlock = status.ok();
            }
        }
        
        if (haveFullBlock) {
            // Calculate actual block size
            DataStream ss;
            Serialize(ss, block);
            int64_t totalSize = static_cast<int64_t>(ss.TotalSize());
            result["size"] = totalSize;
            
            // Stripped size is size without witness data
            // In SHURIUM, we don't have separate witness serialization yet,
            // so stripped size equals total size
            result["strippedsize"] = totalSize;
            
            // Weight = base_size * 3 + total_size (BIP141)
            // Without segwit: weight = size * 4
            result["weight"] = totalSize * 4;
        } else {
            result["size"] = static_cast<int64_t>(0);
            result["strippedsize"] = static_cast<int64_t>(0);
            result["weight"] = static_cast<int64_t>(0);
        }
        
        result["height"] = static_cast<int64_t>(pindex->nHeight);
        result["version"] = static_cast<int64_t>(pindex->nVersion);
        result["versionHex"] = ([&]() {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0') << std::setw(8) << pindex->nVersion;
            return oss.str();
        })();
        result["merkleroot"] = HashToHex(pindex->hashMerkleRoot);
        
        // Transactions
        JSONValue::Array txArray;
        if (haveFullBlock) {
            for (const auto& tx : block.vtx) {
                if (verbosity == 1) {
                    // Just txids
                    txArray.push_back(JSONValue(HashToHex(tx->GetHash())));
                } else {
                    // Verbosity 2: full transaction objects
                    JSONValue::Object txObj;
                    txObj["txid"] = HashToHex(tx->GetHash());
                    txObj["version"] = static_cast<int64_t>(tx->version);
                    txObj["size"] = static_cast<int64_t>(tx->GetTotalSize());
                    txObj["vsize"] = static_cast<int64_t>(tx->GetTotalSize());  // Simplified - same as size without witness
                    txObj["locktime"] = static_cast<int64_t>(tx->nLockTime);
                    
                    // Inputs
                    JSONValue::Array vinArray;
                    for (const auto& txin : tx->vin) {
                        JSONValue::Object vinObj;
                        if (tx->IsCoinBase()) {
                            vinObj["coinbase"] = FormatHex(txin.scriptSig.data(), txin.scriptSig.size());
                        } else {
                            vinObj["txid"] = HashToHex(Hash256(txin.prevout.hash));
                            vinObj["vout"] = static_cast<int64_t>(txin.prevout.n);
                            vinObj["scriptSig"] = JSONValue::Object();
                        }
                        vinObj["sequence"] = static_cast<int64_t>(txin.nSequence);
                        vinArray.push_back(JSONValue(std::move(vinObj)));
                    }
                    txObj["vin"] = JSONValue(std::move(vinArray));
                    
                    // Outputs
                    JSONValue::Array voutArray;
                    int n = 0;
                    for (const auto& txout : tx->vout) {
                        JSONValue::Object voutObj;
                        voutObj["value"] = FormatAmount(txout.nValue);
                        voutObj["n"] = static_cast<int64_t>(n++);
                        JSONValue::Object scriptObj;
                        scriptObj["hex"] = FormatHex(txout.scriptPubKey.data(), txout.scriptPubKey.size());
                        voutObj["scriptPubKey"] = JSONValue(std::move(scriptObj));
                        voutArray.push_back(JSONValue(std::move(voutObj)));
                    }
                    txObj["vout"] = JSONValue(std::move(voutArray));
                    
                    txArray.push_back(JSONValue(std::move(txObj)));
                }
            }
        }
        result["tx"] = JSONValue(std::move(txArray));
        result["nTx"] = static_cast<int64_t>(haveFullBlock ? block.vtx.size() : pindex->nTx);
        
        result["time"] = static_cast<int64_t>(pindex->nTime);
        result["mediantime"] = pindex->GetMedianTimePast();
        result["nonce"] = static_cast<int64_t>(pindex->nNonce);
        result["bits"] = ([&]() {
            std::ostringstream oss;
            oss << std::hex << pindex->nBits;
            return oss.str();
        })();
        result["difficulty"] = GetDifficultyFromBits(pindex->nBits);
        result["chainwork"] = ChainWorkToHex(pindex->nChainWork);
        
        // Previous block hash
        if (pindex->pprev) {
            result["previousblockhash"] = BlockHashToHex(pindex->pprev->GetBlockHash());
        }
        
        // Next block hash
        BlockIndex* pnext = chain.Next(pindex);
        if (pnext) {
            result["nextblockhash"] = BlockHashToHex(pnext->GetBlockHash());
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getblockhash(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    try {
        int64_t height = GetRequiredParam<int64_t>(req, size_t(0));
        
        ChainState* chainState = table->GetChainState();
        if (!chainState) {
            return RPCError(-1, "Chain state not available", req.GetId());
        }
        
        if (height < 0 || height > chainState->GetHeight()) {
            return RPCError(-8, "Block height out of range", req.GetId());
        }
        
        const Chain& chain = chainState->GetChain();
        BlockIndex* pindex = chain[static_cast<int>(height)];
        
        if (!pindex) {
            return RPCError(-5, "Block not found", req.GetId());
        }
        
        std::string hash = BlockHashToHex(pindex->GetBlockHash());
        return RPCResponse::Success(JSONValue(hash), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getblockheader(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    try {
        std::string blockhash = GetRequiredParam<std::string>(req, size_t(0));
        bool verbose = GetOptionalParam<bool>(req, size_t(1), true);
        
        // Parse the hash
        BlockHash hash = HexToBlockHash(blockhash);
        
        ChainState* chainState = table->GetChainState();
        if (!chainState) {
            return RPCError(-1, "Chain state not available", req.GetId());
        }
        
        // Look up the block index
        const Chain& chain = chainState->GetChain();
        BlockIndex* pindex = nullptr;
        
        for (int h = chain.Height(); h >= 0; --h) {
            BlockIndex* idx = chain[h];
            if (idx && idx->GetBlockHash() == hash) {
                pindex = idx;
                break;
            }
        }
        
        if (!pindex) {
            return RPCError(-5, "Block not found", req.GetId());
        }
        
        if (!verbose) {
            // Return hex-encoded header (80 bytes)
            // Reconstruct the block header from the index
            BlockHeader header = pindex->GetBlockHeader();
            
            // Serialize to bytes
            DataStream ss;
            Serialize(ss, header);
            
            // Convert to hex (should be exactly 80 bytes)
            std::string hexHeader = FormatHex(ss.data(), ss.TotalSize());
            return RPCResponse::Success(JSONValue(hexHeader), req.GetId());
        }
        
        JSONValue::Object result;
        result["hash"] = blockhash;
        
        int confirmations = chain.Height() - pindex->nHeight + 1;
        result["confirmations"] = static_cast<int64_t>(confirmations);
        
        result["height"] = static_cast<int64_t>(pindex->nHeight);
        result["version"] = static_cast<int64_t>(pindex->nVersion);
        result["versionHex"] = ([&]() {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0') << std::setw(8) << pindex->nVersion;
            return oss.str();
        })();
        result["merkleroot"] = HashToHex(pindex->hashMerkleRoot);
        result["time"] = static_cast<int64_t>(pindex->nTime);
        result["mediantime"] = pindex->GetMedianTimePast();
        result["nonce"] = static_cast<int64_t>(pindex->nNonce);
        result["bits"] = ([&]() {
            std::ostringstream oss;
            oss << std::hex << pindex->nBits;
            return oss.str();
        })();
        result["difficulty"] = GetDifficultyFromBits(pindex->nBits);
        result["chainwork"] = ChainWorkToHex(pindex->nChainWork);
        result["nTx"] = static_cast<int64_t>(pindex->nTx);
        
        if (pindex->pprev) {
            result["previousblockhash"] = BlockHashToHex(pindex->pprev->GetBlockHash());
        }
        
        BlockIndex* pnext = chain.Next(pindex);
        if (pnext) {
            result["nextblockhash"] = BlockHashToHex(pnext->GetBlockHash());
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getchaintips(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    JSONValue::Array tips;
    
    ChainState* chainState = table->GetChainState();
    if (chainState) {
        BlockIndex* tip = chainState->GetTip();
        if (tip) {
            JSONValue::Object mainTip;
            mainTip["height"] = static_cast<int64_t>(tip->nHeight);
            mainTip["hash"] = BlockHashToHex(tip->GetBlockHash());
            mainTip["branchlen"] = int64_t(0);  // Main chain has branchlen 0
            mainTip["status"] = "active";
            tips.push_back(JSONValue(std::move(mainTip)));
        }
    }
    
    // If no chainstate, return default empty tip
    if (tips.empty()) {
        JSONValue::Object defaultTip;
        defaultTip["height"] = int64_t(0);
        defaultTip["hash"] = "0000000000000000000000000000000000000000000000000000000000000000";
        defaultTip["branchlen"] = int64_t(0);
        defaultTip["status"] = "active";
        tips.push_back(JSONValue(std::move(defaultTip)));
    }
    
    return RPCResponse::Success(JSONValue(std::move(tips)), req.GetId());
}

RPCResponse cmd_getdifficulty(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    double difficulty = 1.0;
    
    ChainState* chainState = table->GetChainState();
    if (chainState) {
        BlockIndex* tip = chainState->GetTip();
        if (tip) {
            difficulty = GetDifficultyFromBits(tip->nBits);
        }
    }
    
    return RPCResponse::Success(JSONValue(difficulty), req.GetId());
}

RPCResponse cmd_getmempoolinfo(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    JSONValue::Object result;
    
    result["loaded"] = true;
    result["size"] = int64_t(0);
    result["bytes"] = int64_t(0);
    result["usage"] = int64_t(0);
    result["maxmempool"] = int64_t(300000000);  // 300 MB default
    result["mempoolminfee"] = 0.00001;
    result["minrelaytxfee"] = 0.00001;
    
    Mempool* mempool = table->GetMempool();
    if (mempool) {
        result["size"] = static_cast<int64_t>(mempool->Size());
        result["bytes"] = static_cast<int64_t>(mempool->GetTotalSize());
        result["usage"] = static_cast<int64_t>(mempool->GetTotalSize());  // Simplified
        
        MempoolLimits limits = mempool->GetLimits();
        result["maxmempool"] = static_cast<int64_t>(limits.maxSize);
    }
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_getrawmempool(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    bool verbose = GetOptionalParam<bool>(req, size_t(0), false);
    
    Mempool* mempool = table->GetMempool();
    
    if (verbose) {
        JSONValue::Object result;
        
        if (mempool) {
            std::vector<TxMempoolInfo> txinfos = mempool->GetAllTxInfo();
            for (const auto& info : txinfos) {
                JSONValue::Object entry;
                entry["size"] = static_cast<int64_t>(info.vsize);
                entry["vsize"] = static_cast<int64_t>(info.vsize);
                entry["fee"] = FormatAmount(info.fee);
                entry["time"] = info.time;
                
                // Convert TxHash to hex
                std::string txidHex = HashToHex(info.tx->GetHash());
                result[txidHex] = JSONValue(std::move(entry));
            }
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    }
    
    // Non-verbose: return array of txids
    JSONValue::Array txids;
    
    if (mempool) {
        std::vector<TxMempoolInfo> txinfos = mempool->GetAllTxInfo();
        for (const auto& info : txinfos) {
            txids.push_back(JSONValue(HashToHex(info.tx->GetHash())));
        }
    }
    
    return RPCResponse::Success(JSONValue(std::move(txids)), req.GetId());
}

RPCResponse cmd_gettransaction(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    try {
        std::string txid = GetRequiredParam<std::string>(req, size_t(0));
        
        JSONValue::Object result;
        result["txid"] = txid;
        result["hash"] = txid;
        result["version"] = int64_t(1);
        result["size"] = int64_t(0);
        result["locktime"] = int64_t(0);
        result["vin"] = JSONValue::Array();
        result["vout"] = JSONValue::Array();
        result["confirmations"] = int64_t(0);
        result["time"] = GetTime();
        result["blocktime"] = GetTime();
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getrawtransaction(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table) {
    try {
        std::string txid = GetRequiredParam<std::string>(req, size_t(0));
        bool verbose = GetOptionalParam<bool>(req, size_t(1), false);
        
        if (!verbose) {
            // Return hex-encoded transaction
            return RPCResponse::Success(JSONValue(""), req.GetId());
        }
        
        return cmd_gettransaction(req, ctx, table);
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_decoderawtransaction(const RPCRequest& req, const RPCContext& ctx,
                                     RPCCommandTable* table) {
    try {
        std::string hexstring = GetRequiredParam<std::string>(req, size_t(0));
        
        JSONValue::Object result;
        result["txid"] = "";
        result["hash"] = "";
        result["version"] = int64_t(1);
        result["size"] = static_cast<int64_t>(hexstring.size() / 2);
        result["locktime"] = int64_t(0);
        result["vin"] = JSONValue::Array();
        result["vout"] = JSONValue::Array();
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_sendrawtransaction(const RPCRequest& req, const RPCContext& ctx,
                                   RPCCommandTable* table) {
    try {
        std::string hexstring = GetRequiredParam<std::string>(req, size_t(0));
        
        // Decode the transaction from hex
        std::vector<Byte> txData = ParseHex(hexstring);
        if (txData.empty()) {
            return RPCResponse::Error(ErrorCode::TX_REJECTED, "TX decode failed", req.GetId());
        }
        
        // Deserialize the transaction
        DataStream ss(txData);
        MutableTransaction mtx;
        try {
            Unserialize(ss, mtx);
        } catch (const std::exception& e) {
            return RPCResponse::Error(ErrorCode::TX_REJECTED, 
                std::string("TX decode failed: ") + e.what(), req.GetId());
        }
        
        // Convert to immutable transaction
        TransactionRef tx = MakeTransactionRef(std::move(mtx));
        
        // Get mempool and chain state
        Mempool* mempool = table->GetMempool();
        if (!mempool) {
            return RPCError(-1, "Mempool not available", req.GetId());
        }
        
        ChainState* chainState = table->GetChainState();
        if (!chainState) {
            return RPCError(-1, "Chain state not available", req.GetId());
        }
        
        // Get current chain height
        int32_t chainHeight = chainState->GetHeight();
        
        // Get UTXO view from chain state
        CoinsViewCache& coins = chainState->GetCoins();
        
        // Accept to mempool
        MempoolAcceptResult result = AcceptToMempool(tx, *mempool, coins, chainHeight);
        
        if (!result.IsValid()) {
            return RPCResponse::Error(ErrorCode::TX_REJECTED, result.rejectReason, req.GetId());
        }
        
        // Return the transaction id
        return RPCResponse::Success(JSONValue(HashToHex(result.txid)), req.GetId());
        
    } catch (const std::exception& e) {
        return RPCResponse::Error(ErrorCode::TX_REJECTED, e.what(), req.GetId());
    }
}


// ============================================================================
// Network Command Implementations
// ============================================================================

RPCResponse cmd_getnetworkinfo(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    JSONValue::Object result;
    
    result["version"] = int64_t(1000000);
    result["subversion"] = "/SHURIUM:1.0.0/";
    result["protocolversion"] = int64_t(70015);
    result["localservices"] = "0000000000000001";
    result["localservicesnames"] = JSONValue::Array{JSONValue("NETWORK")};
    result["localrelay"] = true;
    result["timeoffset"] = int64_t(0);
    result["networkactive"] = true;
    result["connections"] = int64_t(0);
    result["connections_in"] = int64_t(0);
    result["connections_out"] = int64_t(0);
    
    JSONValue::Array networks;
    JSONValue::Object ipv4;
    ipv4["name"] = "ipv4";
    ipv4["limited"] = false;
    ipv4["reachable"] = true;
    ipv4["proxy"] = "";
    networks.push_back(JSONValue(std::move(ipv4)));
    result["networks"] = JSONValue(std::move(networks));
    
    result["relayfee"] = 0.00001;
    result["incrementalfee"] = 0.00001;
    result["localaddresses"] = JSONValue::Array();
    result["warnings"] = "";
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_getpeerinfo(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table) {
    JSONValue::Array peers;
    
    network::NetworkManager* netman = table->GetNetworkManager();
    if (netman) {
        auto peerList = netman->GetPeers();
        for (const auto& peer : peerList) {
            if (!peer) continue;
            
            JSONValue::Object peerObj;
            peerObj["id"] = static_cast<int64_t>(peer->GetId());
            peerObj["addr"] = peer->GetAddress().ToString();
            peerObj["addrbind"] = ""; // Would need local address tracking
            
            PeerStats stats = peer->GetStats();
            peerObj["services"] = std::to_string(static_cast<uint64_t>(stats.services));
            peerObj["servicesnames"] = JSONValue::Array(); // Service flags to names
            peerObj["relaytxes"] = stats.fRelayTxes;
            peerObj["lastsend"] = stats.lastSendTime;
            peerObj["lastrecv"] = stats.lastRecvTime;
            peerObj["bytessent"] = static_cast<int64_t>(stats.bytesSent);
            peerObj["bytesrecv"] = static_cast<int64_t>(stats.bytesRecv);
            peerObj["conntime"] = stats.connectedTime;
            peerObj["timeoffset"] = int64_t(0); // Would need to track time offset
            peerObj["pingtime"] = static_cast<double>(stats.pingLatencyMicros) / 1000000.0;
            peerObj["pingwait"] = static_cast<double>(stats.pingWaitTime) / 1000000.0;
            peerObj["version"] = static_cast<int64_t>(stats.nVersion);
            peerObj["subver"] = stats.userAgent;
            peerObj["inbound"] = stats.fInbound;
            peerObj["startingheight"] = static_cast<int64_t>(stats.startingHeight);
            peerObj["banscore"] = static_cast<int64_t>(stats.misbehaviorScore);
            peerObj["synced_headers"] = int64_t(-1); // Would need sync tracking
            peerObj["synced_blocks"] = int64_t(-1);
            
            // Connection type
            std::string connType;
            switch (peer->GetConnectionType()) {
                case ConnectionType::INBOUND: connType = "inbound"; break;
                case ConnectionType::OUTBOUND_FULL_RELAY: connType = "outbound-full-relay"; break;
                case ConnectionType::MANUAL: connType = "manual"; break;
                case ConnectionType::FEELER: connType = "feeler"; break;
                case ConnectionType::BLOCK_RELAY: connType = "block-relay-only"; break;
                case ConnectionType::ADDR_FETCH: connType = "addr-fetch"; break;
                default: connType = "unknown"; break;
            }
            peerObj["connection_type"] = connType;
            
            peers.push_back(JSONValue(std::move(peerObj)));
        }
    }
    
    return RPCResponse::Success(JSONValue(std::move(peers)), req.GetId());
}

RPCResponse cmd_getconnectioncount(const RPCRequest& req, const RPCContext& ctx,
                                   RPCCommandTable* table) {
    int64_t count = 0;
    
    network::NetworkManager* netman = table->GetNetworkManager();
    if (netman) {
        count = static_cast<int64_t>(netman->GetConnectionCount());
    }
    
    return RPCResponse::Success(JSONValue(count), req.GetId());
}

RPCResponse cmd_addnode(const RPCRequest& req, const RPCContext& ctx,
                        RPCCommandTable* table) {
    try {
        std::string node = GetRequiredParam<std::string>(req, size_t(0));
        std::string command = GetRequiredParam<std::string>(req, size_t(1));
        
        if (command != "add" && command != "remove" && command != "onetry") {
            return InvalidParams("Invalid command. Use: add, remove, onetry", req.GetId());
        }
        
        network::NetworkManager* netman = table->GetNetworkManager();
        if (!netman) {
            return RPCResponse::Error(-1, "Network manager not available", req.GetId());
        }
        
        bool success = false;
        if (command == "add") {
            success = netman->AddNode(node);
            if (!success) {
                return RPCResponse::Error(-23, "Node already added", req.GetId());
            }
        } else if (command == "remove") {
            success = netman->RemoveNode(node);
            if (!success) {
                return RPCResponse::Error(-24, "Node not found in added nodes", req.GetId());
            }
        } else if (command == "onetry") {
            success = netman->TryConnectNode(node);
            if (!success) {
                return RPCResponse::Error(-25, "Unable to connect to node", req.GetId());
            }
        }
        
        return RPCResponse::Success(JSONValue(nullptr), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_disconnectnode(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    try {
        // Can disconnect by address or by peer ID
        std::string address = GetOptionalParam<std::string>(req, size_t(0), "");
        int64_t nodeId = GetOptionalParam<int64_t>(req, "nodeid", int64_t(-1));
        
        if (address.empty() && nodeId == -1) {
            return InvalidParams("Must specify address or nodeid", req.GetId());
        }
        
        network::NetworkManager* netman = table->GetNetworkManager();
        if (!netman) {
            return RPCResponse::Error(-1, "Network manager not available", req.GetId());
        }
        
        bool success = false;
        if (!address.empty()) {
            success = netman->DisconnectNode(address);
        } else {
            success = netman->DisconnectNode(static_cast<Peer::Id>(nodeId));
        }
        
        if (!success) {
            return RPCResponse::Error(-29, "Node not found", req.GetId());
        }
        
        return RPCResponse::Success(JSONValue(nullptr), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getaddednodeinfo(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table) {
    std::string node = GetOptionalParam<std::string>(req, size_t(0), "");
    
    JSONValue::Array result;
    
    network::NetworkManager* netman = table->GetNetworkManager();
    if (netman) {
        if (!node.empty()) {
            // Get info for specific node
            auto info = netman->GetAddedNodeInfo(node);
            if (info) {
                JSONValue::Object nodeObj;
                nodeObj["addednode"] = info->address;
                nodeObj["connected"] = info->connected;
                
                JSONValue::Array addresses;
                for (const auto& [addr, connected] : info->addresses) {
                    JSONValue::Object addrObj;
                    addrObj["address"] = addr;
                    addrObj["connected"] = connected ? "outbound" : "false";
                    addresses.push_back(JSONValue(std::move(addrObj)));
                }
                nodeObj["addresses"] = JSONValue(std::move(addresses));
                
                result.push_back(JSONValue(std::move(nodeObj)));
            }
        } else {
            // Get info for all added nodes
            auto allInfo = netman->GetAddedNodeInfo();
            for (const auto& info : allInfo) {
                JSONValue::Object nodeObj;
                nodeObj["addednode"] = info.address;
                nodeObj["connected"] = info.connected;
                
                JSONValue::Array addresses;
                for (const auto& [addr, connected] : info.addresses) {
                    JSONValue::Object addrObj;
                    addrObj["address"] = addr;
                    addrObj["connected"] = connected ? "outbound" : "false";
                    addresses.push_back(JSONValue(std::move(addrObj)));
                }
                nodeObj["addresses"] = JSONValue(std::move(addresses));
                
                result.push_back(JSONValue(std::move(nodeObj)));
            }
        }
    }
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_setnetworkactive(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table) {
    try {
        bool state = GetRequiredParam<bool>(req, size_t(0));
        
        network::NetworkManager* netman = table->GetNetworkManager();
        if (netman) {
            netman->SetNetworkActive(state);
        }
        
        return RPCResponse::Success(JSONValue(state), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_ping(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table) {
    network::NetworkManager* netman = table->GetNetworkManager();
    if (netman) {
        netman->PingAll();
    }
    
    return RPCResponse::Success(JSONValue(nullptr), req.GetId());
}

// ============================================================================
// Wallet Command Implementations
// ============================================================================

RPCResponse cmd_getwalletinfo(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    JSONValue::Object result;
    result["walletname"] = wallet->GetName();
    result["walletversion"] = int64_t(1);
    
    // Get balance breakdown
    wallet::WalletBalance balance = wallet->GetBalance();
    result["balance"] = FormatAmount(balance.confirmed);
    result["unconfirmed_balance"] = FormatAmount(balance.unconfirmed);
    result["immature_balance"] = FormatAmount(balance.immature);
    
    // Get transaction count
    result["txcount"] = static_cast<int64_t>(wallet->GetTransactions().size());
    
    // Keypool info (simplified for HD wallet)
    result["keypoololdest"] = GetTime();
    result["keypoolsize"] = int64_t(1000);  // HD wallets have effectively unlimited keys
    result["keypoolsize_hd_internal"] = int64_t(1000);
    
    // Lock status
    result["unlocked_until"] = wallet->IsLocked() ? int64_t(0) : int64_t(0x7FFFFFFF);
    
    // Fee and settings
    result["paytxfee"] = static_cast<double>(wallet->GetConfig().defaultFeeRate) / COIN;
    result["private_keys_enabled"] = true;
    result["avoid_reuse"] = false;
    result["scanning"] = false;
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_getbalance(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    int64_t minconf = GetOptionalParam<int64_t>(req, size_t(0), int64_t(1));
    bool includeWatchOnly = GetOptionalParam<bool>(req, size_t(1), false);
    
    wallet::WalletBalance balance = wallet->GetBalance();
    
    Amount result = 0;
    if (minconf <= 0) {
        // Include unconfirmed
        result = balance.confirmed + balance.unconfirmed;
    } else {
        // Only confirmed (minconf >= 1)
        result = balance.confirmed;
    }
    
    if (includeWatchOnly) {
        result += balance.watchOnlyConfirmed;
        if (minconf <= 0) {
            result += balance.watchOnlyUnconfirmed;
        }
    }
    
    return RPCResponse::Success(FormatAmount(result), req.GetId());
}

RPCResponse cmd_getunconfirmedbalance(const RPCRequest& req, const RPCContext& ctx,
                                      RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    wallet::WalletBalance balance = wallet->GetBalance();
    
    return RPCResponse::Success(FormatAmount(balance.unconfirmed), req.GetId());
}

RPCResponse cmd_getnewaddress(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    if (wallet->IsLocked()) {
        return RPCResponse::Error(ErrorCode::WALLET_UNLOCK_NEEDED, 
                                 "Wallet is locked. Unlock first with walletpassphrase.", req.GetId());
    }
    
    std::string label = GetOptionalParam<std::string>(req, size_t(0), "");
    
    std::string address = wallet->GetNewAddress(label);
    if (address.empty()) {
        return RPCResponse::Error(-4, "Error generating address", req.GetId());
    }
    
    return RPCResponse::Success(JSONValue(address), req.GetId());
}

RPCResponse cmd_getaddressinfo(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    try {
        std::string address = GetRequiredParam<std::string>(req, size_t(0));
        
        JSONValue::Object result;
        result["address"] = address;
        result["isvalid"] = ValidateAddress(address);
        
        // Determine address type using the proper function
        AddressType addrType = GetAddressType(address);
        
        bool isWitness = (addrType == AddressType::P2WPKH || 
                          addrType == AddressType::P2WSH ||
                          addrType == AddressType::P2TR);
        bool isScript = (addrType == AddressType::P2SH || 
                         addrType == AddressType::P2WSH);
        bool isP2SH_P2WPKH = false;  // Will be set true if we detect wrapped witness
        
        wallet::Wallet* wallet = table->GetWallet();
        auto keystore = wallet ? wallet->GetKeyStore() : nullptr;
        
        // Handle P2SH addresses (may be P2SH-P2WPKH)
        if (addrType == AddressType::P2SH) {
            auto base58Data = DecodeBase58Check(address);
            if (base58Data.size() == 21) {
                Hash160 scriptHash;
                std::memcpy(scriptHash.data(), base58Data.data() + 1, 20);
                
                // Create P2SH scriptPubKey: OP_HASH160 <20-byte-hash> OP_EQUAL
                result["scriptPubKey"] = "a914" + FormatHex(scriptHash.data(), 20) + "87";
                result["script"] = "scripthash";
                
                // For P2SH-P2WPKH, the redeemScript is: OP_0 <20-byte-pubkeyhash>
                // If the wallet has a key whose P2WPKH scripthash matches, it's P2SH-P2WPKH
                if (keystore) {
                    auto keyHashes = keystore->GetKeyHashes();
                    for (const auto& keyHash : keyHashes) {
                        // Build P2WPKH witness program: 0x0014 + keyhash
                        std::vector<uint8_t> witnessProgram;
                        witnessProgram.push_back(0x00);  // OP_0
                        witnessProgram.push_back(0x14);  // Push 20 bytes
                        witnessProgram.insert(witnessProgram.end(), keyHash.begin(), keyHash.end());
                        
                        // Hash the witness program to get P2SH script hash
                        Hash160 computedScriptHash = ComputeHash160(witnessProgram.data(), witnessProgram.size());
                        
                        if (computedScriptHash == scriptHash) {
                            // This is a P2SH-P2WPKH address!
                            isP2SH_P2WPKH = true;
                            
                            // Report embedded witness info
                            JSONValue::Object embedded;
                            embedded["isscript"] = false;
                            embedded["iswitness"] = true;
                            embedded["witness_version"] = static_cast<int64_t>(0);
                            embedded["witness_program"] = FormatHex(keyHash.data(), 20);
                            embedded["scriptPubKey"] = "0014" + FormatHex(keyHash.data(), 20);
                            embedded["address"] = EncodeP2WPKH(keyHash, 
                                wallet ? wallet->GetConfig().testnet : false);
                            
                            // Get public key for embedded
                            auto pubkey = keystore->GetPublicKey(keyHash);
                            if (pubkey && pubkey->IsValid()) {
                                embedded["pubkey"] = pubkey->ToHex();
                            }
                            
                            result["embedded"] = JSONValue(std::move(embedded));
                            result["hex"] = FormatHex(witnessProgram.data(), witnessProgram.size());
                            
                            bool isMine = keystore->HaveKey(keyHash);
                            bool isWatchOnly = keystore->IsWatchOnly(keyHash);
                            result["ismine"] = isMine;
                            result["iswatchonly"] = isWatchOnly;
                            result["solvable"] = isMine || isWatchOnly;
                            
                            if (pubkey && pubkey->IsValid()) {
                                result["pubkey"] = pubkey->ToHex();
                                result["iscompressed"] = pubkey->IsCompressed();
                            }
                            
                            // HD derivation info
                            if (isMine) {
                                auto hdManager = wallet->GetHDKeyManager();
                                if (hdManager) {
                                    auto keyInfo = hdManager->FindKeyByHash(keyHash);
                                    if (keyInfo) {
                                        result["hdkeypath"] = keyInfo->path.ToString();
                                        result["hdseedid"] = FormatHex(keyInfo->keyHash.data(), 20);
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                
                if (!isP2SH_P2WPKH) {
                    // Regular P2SH (multisig or other script) - we don't have the redeemScript
                    result["ismine"] = false;
                    result["iswatchonly"] = false;
                    result["solvable"] = false;
                }
            }
        }
        // Handle native SegWit (Bech32) addresses
        else if (isWitness) {
            auto decoded = DecodeBech32(address);
            if (decoded && !std::get<2>(*decoded).empty()) {
                auto& witnessProgram = std::get<2>(*decoded);
                uint8_t version = std::get<1>(*decoded);
                
                result["witness_version"] = static_cast<int64_t>(version);
                result["witness_program"] = FormatHex(witnessProgram.data(), witnessProgram.size());
                
                if (witnessProgram.size() == 20) {
                    // P2WPKH - convert to key hash
                    Hash160 keyHash;
                    std::memcpy(keyHash.data(), witnessProgram.data(), 20);
                    result["scriptPubKey"] = "0014" + FormatHex(keyHash.data(), 20);
                    
                    if (keystore) {
                        bool isMine = keystore->HaveKey(keyHash);
                        bool isWatchOnly = keystore->IsWatchOnly(keyHash);
                        
                        result["ismine"] = isMine;
                        result["iswatchonly"] = isWatchOnly;
                        result["solvable"] = isMine || isWatchOnly;
                        
                        auto pubkey = keystore->GetPublicKey(keyHash);
                        if (pubkey && pubkey->IsValid()) {
                            result["pubkey"] = pubkey->ToHex();
                            result["iscompressed"] = pubkey->IsCompressed();
                        }
                        
                        // Look up in address book
                        if (wallet) {
                            auto entry = wallet->LookupAddress(address);
                            if (entry) {
                                result["label"] = entry->label;
                                result["purpose"] = entry->purpose;
                            }
                            
                            // HD derivation info
                            if (isMine) {
                                auto hdManager = wallet->GetHDKeyManager();
                                if (hdManager) {
                                    auto keyInfo = hdManager->FindKeyByHash(keyHash);
                                    if (keyInfo) {
                                        result["hdkeypath"] = keyInfo->path.ToString();
                                        result["hdseedid"] = FormatHex(keyInfo->keyHash.data(), 20);
                                    }
                                }
                            }
                        }
                    } else {
                        result["ismine"] = false;
                        result["iswatchonly"] = false;
                        result["solvable"] = false;
                    }
                } else if (witnessProgram.size() == 32) {
                    // P2WSH or P2TR
                    result["scriptPubKey"] = (version == 0 ? "0020" : "5120") + 
                        FormatHex(witnessProgram.data(), 32);
                    result["ismine"] = false;
                    result["iswatchonly"] = false;
                    result["solvable"] = false;
                }
            }
        }
        // Handle P2PKH (legacy) addresses
        else if (addrType == AddressType::P2PKH) {
            auto base58Data = DecodeBase58Check(address);
            if (base58Data.size() == 21) {
                Hash160 keyHash;
                std::memcpy(keyHash.data(), base58Data.data() + 1, 20);
                
                // P2PKH scriptPubKey: OP_DUP OP_HASH160 <hash> OP_EQUALVERIFY OP_CHECKSIG
                result["scriptPubKey"] = "76a914" + FormatHex(keyHash.data(), 20) + "88ac";
                
                if (keystore) {
                    bool isMine = keystore->HaveKey(keyHash);
                    bool isWatchOnly = keystore->IsWatchOnly(keyHash);
                    
                    result["ismine"] = isMine;
                    result["iswatchonly"] = isWatchOnly;
                    result["solvable"] = isMine || isWatchOnly;
                    
                    auto pubkey = keystore->GetPublicKey(keyHash);
                    if (pubkey && pubkey->IsValid()) {
                        result["pubkey"] = pubkey->ToHex();
                        result["iscompressed"] = pubkey->IsCompressed();
                    }
                    
                    if (wallet) {
                        auto entry = wallet->LookupAddress(address);
                        if (entry) {
                            result["label"] = entry->label;
                            result["purpose"] = entry->purpose;
                        }
                        
                        if (isMine) {
                            auto hdManager = wallet->GetHDKeyManager();
                            if (hdManager) {
                                auto keyInfo = hdManager->FindKeyByHash(keyHash);
                                if (keyInfo) {
                                    result["hdkeypath"] = keyInfo->path.ToString();
                                    result["hdseedid"] = FormatHex(keyInfo->keyHash.data(), 20);
                                }
                            }
                        }
                    }
                } else {
                    result["ismine"] = false;
                    result["iswatchonly"] = false;
                    result["solvable"] = false;
                }
            }
        }
        
        result["isscript"] = isScript || isP2SH_P2WPKH;
        result["iswitness"] = isWitness;
        
        // For P2SH-P2WPKH, also indicate it wraps witness
        if (isP2SH_P2WPKH) {
            result["ischange"] = false;  // Could be determined from HD path
            result["script"] = "witness_v0_keyhash";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_listaddresses(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    JSONValue::Array addresses;
    
    // Get all wallet addresses
    std::vector<std::string> walletAddresses = wallet->GetAddresses();
    for (const auto& addr : walletAddresses) {
        JSONValue::Object entry;
        entry["address"] = addr;
        
        // Look up label from address book
        auto bookEntry = wallet->LookupAddress(addr);
        if (bookEntry) {
            entry["label"] = bookEntry->label;
            entry["purpose"] = bookEntry->purpose;
        } else {
            entry["label"] = "";
            entry["purpose"] = "receive";
        }
        
        addresses.push_back(JSONValue(std::move(entry)));
    }
    
    return RPCResponse::Success(JSONValue(std::move(addresses)), req.GetId());
}

RPCResponse cmd_sendtoaddress(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    if (wallet->IsLocked()) {
        return RPCResponse::Error(ErrorCode::WALLET_UNLOCK_NEEDED, 
                                 "Wallet is locked. Unlock first with walletpassphrase.", req.GetId());
    }
    
    try {
        std::string address = GetRequiredParam<std::string>(req, size_t(0));
        Amount amount = ParseAmount(req.GetParam(size_t(1)));
        std::string comment = GetOptionalParam<std::string>(req, size_t(2), "");
        
        if (!ValidateAddress(address)) {
            return InvalidParams("Invalid address", req.GetId());
        }
        
        if (amount <= 0) {
            return InvalidParams("Amount must be positive", req.GetId());
        }
        
        // Check balance
        wallet::WalletBalance balance = wallet->GetBalance();
        if (balance.GetSpendable() < amount) {
            return RPCResponse::Error(-6, "Insufficient funds", req.GetId());
        }
        
        // Create and sign transaction
        wallet::BuildTxResult result = wallet->SendToAddress(address, amount);
        
        if (!result.success) {
            return RPCResponse::Error(-4, result.error, req.GetId());
        }
        
        // Convert to immutable transaction
        Transaction tx(result.tx);
        
        // Broadcast (if mempool is available)
        Mempool* mempool = table->GetMempool();
        if (mempool) {
            std::string errString;
            if (!mempool->AddTx(std::make_shared<Transaction>(tx), result.fee, 
                               wallet->GetChainHeight(), false, errString)) {
                return RPCResponse::Error(-25, "Transaction rejected: " + errString, req.GetId());
            }
        }
        
        // Process in wallet
        wallet->ProcessTransaction(std::make_shared<Transaction>(tx));
        
        // Return txid
        std::string txid = HashToHex(tx.GetHash());
        return RPCResponse::Success(JSONValue(txid), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_sendmany(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    if (wallet->IsLocked()) {
        return RPCResponse::Error(ErrorCode::WALLET_UNLOCK_NEEDED, 
                                 "Wallet is locked. Unlock first with walletpassphrase.", req.GetId());
    }
    
    try {
        const JSONValue& amounts = req.GetParam(size_t(0));
        if (!amounts.IsObject()) {
            return InvalidParams("Amounts must be an object", req.GetId());
        }
        
        // Build recipients list
        std::vector<wallet::Recipient> recipients;
        Amount totalAmount = 0;
        
        const JSONValue::Object& amountsObj = amounts.GetObject();
        for (const auto& [key, value] : amountsObj) {
            if (!ValidateAddress(key)) {
                return InvalidParams("Invalid address: " + key, req.GetId());
            }
            Amount amt = ParseAmount(value);
            if (amt <= 0) {
                return InvalidParams("Amount must be positive for address: " + key, req.GetId());
            }
            auto recipient = wallet::Recipient::FromAddress(key, amt);
            if (!recipient) {
                return InvalidParams("Failed to parse address: " + key, req.GetId());
            }
            recipients.push_back(*recipient);
            totalAmount += amt;
        }
        
        if (recipients.empty()) {
            return InvalidParams("No recipients specified", req.GetId());
        }
        
        // Check balance
        wallet::WalletBalance balance = wallet->GetBalance();
        if (balance.GetSpendable() < totalAmount) {
            return RPCResponse::Error(-6, "Insufficient funds", req.GetId());
        }
        
        // Create and sign transaction
        wallet::BuildTxResult result = wallet->SendToRecipients(recipients);
        
        if (!result.success) {
            return RPCResponse::Error(-4, result.error, req.GetId());
        }
        
        // Convert to immutable transaction
        Transaction tx(result.tx);
        
        // Broadcast
        Mempool* mempool = table->GetMempool();
        if (mempool) {
            std::string errString;
            if (!mempool->AddTx(std::make_shared<Transaction>(tx), result.fee, 
                               wallet->GetChainHeight(), false, errString)) {
                return RPCResponse::Error(-25, "Transaction rejected: " + errString, req.GetId());
            }
        }
        
        // Process in wallet
        wallet->ProcessTransaction(std::make_shared<Transaction>(tx));
        
        // Return txid
        std::string txid = HashToHex(tx.GetHash());
        return RPCResponse::Success(JSONValue(txid), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_listtransactions(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    int64_t count = GetOptionalParam<int64_t>(req, size_t(0), int64_t(10));
    int64_t skip = GetOptionalParam<int64_t>(req, size_t(1), int64_t(0));
    bool includeWatchOnly = GetOptionalParam<bool>(req, size_t(2), false);
    
    JSONValue::Array transactions;
    
    // Get wallet transactions
    std::vector<wallet::WalletTransaction> walletTxs = wallet->GetTransactions();
    
    // Sort by time (most recent first)
    std::sort(walletTxs.begin(), walletTxs.end(), 
              [](const wallet::WalletTransaction& a, const wallet::WalletTransaction& b) {
                  return a.timeReceived > b.timeReceived;
              });
    
    // Apply skip and count
    int64_t current = 0;
    for (const auto& wtx : walletTxs) {
        if (current < skip) {
            ++current;
            continue;
        }
        if (static_cast<int64_t>(transactions.size()) >= count) {
            break;
        }
        
        JSONValue::Object entry;
        entry["txid"] = HashToHex(wtx.GetHash());
        entry["confirmations"] = static_cast<int64_t>(wtx.GetDepth(wallet->GetChainHeight()));
        
        if (wtx.IsConfirmed()) {
            entry["blockhash"] = HashToHex(wtx.confirmation.blockHash);
            entry["blockheight"] = static_cast<int64_t>(wtx.confirmation.blockHeight);
            entry["blocktime"] = wtx.confirmation.blockTime;
        }
        
        entry["time"] = wtx.timeReceived;
        entry["timereceived"] = wtx.timeReceived;
        
        // Calculate amount (net change to wallet)
        Amount netAmount = wtx.GetNetAmount();
        entry["amount"] = FormatAmount(netAmount);
        entry["fee"] = FormatAmount(wtx.fee);
        
        // Category
        if (wtx.fromMe && netAmount < 0) {
            entry["category"] = "send";
        } else if (netAmount > 0) {
            entry["category"] = "receive";
        } else {
            entry["category"] = "internal";
        }
        
        if (!wtx.label.empty()) {
            entry["label"] = wtx.label;
        }
        
        transactions.push_back(JSONValue(std::move(entry)));
        ++current;
    }
    
    return RPCResponse::Success(JSONValue(std::move(transactions)), req.GetId());
}

RPCResponse cmd_listunspent(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    int64_t minconf = GetOptionalParam<int64_t>(req, size_t(0), int64_t(1));
    int64_t maxconf = GetOptionalParam<int64_t>(req, size_t(1), int64_t(9999999));
    
    JSONValue::Array utxos;
    
    // Get spendable outputs
    std::vector<wallet::WalletOutput> outputs = wallet->GetSpendableOutputs();
    int32_t chainHeight = wallet->GetChainHeight();
    
    for (const auto& output : outputs) {
        int32_t depth = output.GetDepth(chainHeight);
        
        // Filter by confirmation count
        if (depth < minconf || depth > maxconf) {
            continue;
        }
        
        JSONValue::Object entry;
        entry["txid"] = HashToHex(output.outpoint.hash);
        entry["vout"] = static_cast<int64_t>(output.outpoint.n);
        entry["address"] = wallet->GetAddress(output.keyHash);
        entry["amount"] = FormatAmount(output.GetValue());
        entry["confirmations"] = static_cast<int64_t>(depth);
        entry["spendable"] = output.IsSpendable(chainHeight);
        entry["solvable"] = true;
        entry["safe"] = (depth >= 1);  // Considered safe if confirmed
        
        if (!output.label.empty()) {
            entry["label"] = output.label;
        }
        
        utxos.push_back(JSONValue(std::move(entry)));
    }
    
    return RPCResponse::Success(JSONValue(std::move(utxos)), req.GetId());
}

// Helper: Create message hash for signing (Bitcoin-style message magic)
static Hash256 CreateMessageHash(const std::string& message) {
    // Standard Bitcoin message signing format:
    // Hash256(Hash256("\x18Bitcoin Signed Message:\n" + varint(len) + message))
    // We'll use "SHURIUM Signed Message:\n" as our magic
    const std::string magic = "\x17SHURIUM Signed Message:\n";  // 0x17 = 23 bytes
    
    std::vector<Byte> msgData;
    msgData.reserve(magic.size() + 9 + message.size());  // varint can be up to 9 bytes
    
    // Add magic prefix
    for (char c : magic) {
        msgData.push_back(static_cast<Byte>(c));
    }
    
    // Add message length as varint
    if (message.size() < 253) {
        msgData.push_back(static_cast<Byte>(message.size()));
    } else if (message.size() < 65536) {
        msgData.push_back(253);
        msgData.push_back(static_cast<Byte>(message.size() & 0xff));
        msgData.push_back(static_cast<Byte>((message.size() >> 8) & 0xff));
    } else {
        msgData.push_back(254);
        for (int i = 0; i < 4; ++i) {
            msgData.push_back(static_cast<Byte>((message.size() >> (i * 8)) & 0xff));
        }
    }
    
    // Add message
    for (char c : message) {
        msgData.push_back(static_cast<Byte>(c));
    }
    
    // Double SHA256
    return DoubleSHA256(msgData.data(), msgData.size());
}

// Helper: Convert address to key hash
static std::optional<Hash160> AddressToKeyHash(const std::string& address) {
    // Try decoding as Base58 (P2PKH)
    std::vector<uint8_t> decoded = DecodeBase58Check(address);
    if (decoded.size() == 21) {  // 1 byte version + 20 bytes hash
        Hash160 hash;
        std::copy(decoded.begin() + 1, decoded.end(), hash.begin());
        return hash;
    }
    
    // Try decoding as Bech32 (P2WPKH)
    // The DecodeAddress function returns scriptPubKey
    decoded = DecodeAddress(address);
    if (decoded.size() == 22 && decoded[0] == 0x00 && decoded[1] == 0x14) {
        // P2WPKH: OP_0 OP_PUSH20 <20 bytes>
        Hash160 hash;
        std::copy(decoded.begin() + 2, decoded.end(), hash.begin());
        return hash;
    }
    
    return std::nullopt;
}

// Helper: Encode bytes as hex string
static std::string BytesToHex(const std::vector<uint8_t>& bytes) {
    static const char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        result.push_back(hexChars[(b >> 4) & 0x0f]);
        result.push_back(hexChars[b & 0x0f]);
    }
    return result;
}

// Helper: Decode hex string to bytes
static std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned int byte;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> byte;
        result.push_back(static_cast<uint8_t>(byte));
    }
    return result;
}

RPCResponse cmd_signmessage(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    try {
        std::string address = GetRequiredParam<std::string>(req, size_t(0));
        std::string message = GetRequiredParam<std::string>(req, size_t(1));
        
        // Get keystore
        wallet::IKeyStore* keystore = wallet->GetKeyStore();
        if (!keystore) {
            return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Keystore not available", req.GetId());
        }
        
        // Check if wallet is locked
        if (keystore->IsLocked()) {
            return RPCResponse::Error(ErrorCode::WALLET_UNLOCK_NEEDED, "Wallet is locked", req.GetId());
        }
        
        // Get key hash from address
        auto keyHash = AddressToKeyHash(address);
        if (!keyHash) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Invalid address format", req.GetId());
        }
        
        // Get private key
        auto privKey = keystore->GetKey(*keyHash);
        if (!privKey) {
            return RPCResponse::Error(ErrorCode::WALLET_ERROR, 
                "Address not found in wallet", req.GetId());
        }
        
        // Create message hash
        Hash256 messageHash = CreateMessageHash(message);
        
        // Sign with compact/recoverable signature
        std::vector<uint8_t> signature = privKey->SignCompact(messageHash);
        if (signature.empty()) {
            return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Signing failed", req.GetId());
        }
        
        // Encode signature as hex (can be converted to base64 by client if needed)
        std::string hexSig = BytesToHex(signature);
        
        return RPCResponse::Success(JSONValue(hexSig), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_verifymessage(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    try {
        std::string address = GetRequiredParam<std::string>(req, size_t(0));
        std::string signatureStr = GetRequiredParam<std::string>(req, size_t(1));
        std::string message = GetRequiredParam<std::string>(req, size_t(2));
        
        // Decode hex signature
        std::vector<uint8_t> signature = HexToBytes(signatureStr);
        if (signature.size() != 65) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, 
                "Invalid signature size (expected 65 bytes / 130 hex chars)", req.GetId());
        }
        
        // Get key hash from address for comparison
        auto expectedKeyHash = AddressToKeyHash(address);
        if (!expectedKeyHash) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Invalid address format", req.GetId());
        }
        
        // Create message hash
        Hash256 messageHash = CreateMessageHash(message);
        
        // Recover public key from signature
        auto recoveredPubKey = PublicKey::RecoverCompact(messageHash, signature);
        if (!recoveredPubKey) {
            return RPCResponse::Success(JSONValue(false), req.GetId());
        }
        
        // Compute hash of recovered public key
        Hash160 recoveredKeyHash = recoveredPubKey->GetHash160();
        
        // Compare with expected
        bool valid = (recoveredKeyHash == *expectedKeyHash);
        
        return RPCResponse::Success(JSONValue(valid), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_dumpprivkey(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    try {
        std::string address = GetRequiredParam<std::string>(req, size_t(0));
        
        // Check if wallet is locked (encrypted wallets need unlock)
        if (wallet->IsLocked()) {
            return RPCResponse::Error(ErrorCode::WALLET_UNLOCK_NEEDED, 
                                     "Wallet is locked. Use walletpassphrase to unlock.", req.GetId());
        }
        
        // Decode the address to get the key hash
        auto decoded = DecodeBech32(address);
        if (!decoded || std::get<2>(*decoded).empty()) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, 
                                     "Invalid address format", req.GetId());
        }
        
        // The decoded data contains the witness program (key hash)
        // For P2WPKH (version 0), it's a 20-byte hash
        auto& witnessProgram = std::get<2>(*decoded);
        if (witnessProgram.size() != 20) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, 
                                     "Address is not a P2WPKH address", req.GetId());
        }
        
        // Convert to Hash160
        Hash160 keyHash;
        std::memcpy(keyHash.data(), witnessProgram.data(), 20);
        
        // Get the private key from wallet
        auto keystore = wallet->GetKeyStore();
        if (!keystore) {
            return RPCResponse::Error(ErrorCode::WALLET_ERROR, 
                                     "Keystore not available", req.GetId());
        }
        
        auto privKey = keystore->GetKey(keyHash);
        if (!privKey) {
            return RPCResponse::Error(ErrorCode::WALLET_ERROR, 
                                     "Private key not found for this address", req.GetId());
        }
        
        // Export as WIF (Wallet Import Format)
        std::string wif = privKey->ToWIF();
        
        return RPCResponse::Success(JSONValue(wif), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_importprivkey(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    try {
        std::string privkeyWif = GetRequiredParam<std::string>(req, size_t(0));
        std::string label = GetOptionalParam<std::string>(req, size_t(1), "");
        bool rescan = GetOptionalParam<bool>(req, size_t(2), true);
        
        // Check if wallet is locked (encrypted wallets need unlock to add keys)
        if (wallet->IsLocked()) {
            return RPCResponse::Error(ErrorCode::WALLET_UNLOCK_NEEDED, 
                                     "Wallet is locked. Use walletpassphrase to unlock.", req.GetId());
        }
        
        // Parse the WIF-encoded private key
        auto privKey = PrivateKey::FromWIF(privkeyWif);
        if (!privKey || !privKey->IsValid()) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, 
                                     "Invalid private key format. Use WIF format (starts with 'L', 'K', '5', or testnet 'c').", 
                                     req.GetId());
        }
        
        // Get the public key and address
        PublicKey pubKey = privKey->GetPublicKey();
        Hash160 keyHash = pubKey.GetHash160();
        
        // Check if key already exists
        auto keystore = wallet->GetKeyStore();
        if (!keystore) {
            return RPCResponse::Error(ErrorCode::WALLET_ERROR, 
                                     "Keystore not available", req.GetId());
        }
        
        if (keystore->HaveKey(keyHash)) {
            // Key already exists, just return the address
            std::string address = wallet->GetAddress(keyHash);
            return RPCResponse::Success(JSONValue(address), req.GetId());
        }
        
        // Add the private key to the keystore
        if (!keystore->AddKey(*privKey, label)) {
            return RPCResponse::Error(ErrorCode::WALLET_ERROR, 
                                     "Failed to add private key to wallet", req.GetId());
        }
        
        // Get the address for this key
        std::string address = wallet->GetAddress(keyHash);
        
        // Add to address book if label provided
        if (!label.empty()) {
            wallet->AddAddressBookEntry(address, label, "receive");
        }
        
        // Save the wallet
        wallet->Save();
        
        // Note: rescan parameter would trigger blockchain rescan to find 
        // transactions for this address. This requires chain sync infrastructure.
        // For now, we acknowledge the parameter but don't implement full rescan.
        (void)rescan;
        
        return RPCResponse::Success(JSONValue(address), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_walletlock(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    wallet->Lock();
    return RPCResponse::Success(JSONValue(nullptr), req.GetId());
}

RPCResponse cmd_walletpassphrase(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    try {
        std::string passphrase = GetRequiredParam<std::string>(req, size_t(0));
        int64_t timeout = GetRequiredParam<int64_t>(req, size_t(1));
        
        if (!wallet->Unlock(passphrase)) {
            return RPCResponse::Error(-14, "Error: The wallet passphrase entered was incorrect.", req.GetId());
        }
        
        // Note: timeout would require a background timer to auto-lock
        // For now, wallet stays unlocked until explicitly locked
        (void)timeout;  // Acknowledge unused parameter
        
        return RPCResponse::Success(JSONValue(nullptr), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_walletpassphrasechange(const RPCRequest& req, const RPCContext& ctx,
                                       RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    try {
        std::string oldpass = GetRequiredParam<std::string>(req, size_t(0));
        std::string newpass = GetRequiredParam<std::string>(req, size_t(1));
        
        if (!wallet->ChangePassword(oldpass, newpass)) {
            return RPCResponse::Error(-14, "Error: The wallet passphrase entered was incorrect.", req.GetId());
        }
        
        return RPCResponse::Success(JSONValue(nullptr), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_encryptwallet(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    try {
        std::string passphrase = GetRequiredParam<std::string>(req, size_t(0));
        
        if (passphrase.empty()) {
            return RPCResponse::Error(-1, "Passphrase cannot be empty", req.GetId());
        }
        
        if (passphrase.length() < 8) {
            return RPCResponse::Error(-1, "Passphrase must be at least 8 characters", req.GetId());
        }
        
        // Check if already encrypted
        if (wallet->IsEncrypted()) {
            return RPCResponse::Error(-15, "Wallet is already encrypted", req.GetId());
        }
        
        // Encrypt the wallet
        if (!wallet->EncryptWallet(passphrase)) {
            return RPCResponse::Error(-1, "Failed to encrypt wallet", req.GetId());
        }
        
        // Save the encrypted wallet
        if (!wallet->Save()) {
            return RPCResponse::Error(-1, "Failed to save encrypted wallet", req.GetId());
        }
        
        return RPCResponse::Success(JSONValue("Wallet encrypted; restart required"), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_backupwallet(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    try {
        std::string destination = GetRequiredParam<std::string>(req, size_t(0));
        
        if (!wallet->Save(destination)) {
            return RPCResponse::Error(-4, "Error: Unable to backup wallet to destination", req.GetId());
        }
        
        return RPCResponse::Success(JSONValue(nullptr), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_loadwallet(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table) {
    try {
        std::string filename = GetRequiredParam<std::string>(req, size_t(0));
        
        // Check if wallet already loaded
        if (table->GetWallet()) {
            return RPCResponse::Error(-4, "Wallet already loaded. Use unloadwallet first.", req.GetId());
        }
        
        // Construct wallet path using data directory if filename doesn't contain path separator
        std::string walletPath = filename;
        bool hasPathSeparator = (filename.find('/') != std::string::npos || 
                                 filename.find('\\') != std::string::npos);
        if (!hasPathSeparator && !table->GetDataDir().empty()) {
            // Add .dat extension if not present
            std::string walletFile = filename;
            if (walletFile.find(".dat") == std::string::npos) {
                walletFile += ".dat";
            }
            walletPath = JoinWalletPath(table->GetDataDir(), walletFile);
        }
        
        // Try to load wallet
        auto wallet = wallet::Wallet::Load(walletPath);
        if (!wallet) {
            return RPCResponse::Error(-4, "Unable to load wallet file: " + walletPath, req.GetId());
        }
        
        // Set wallet in command table
        table->SetWallet(std::move(wallet));
        
        JSONValue result;
        result["name"] = filename;
        result["path"] = walletPath;
        result["warning"] = "";
        
        return RPCResponse::Success(result, req.GetId());
    } catch (const std::exception& e) {
        return RPCResponse::Error(-4, std::string("Error loading wallet: ") + e.what(), req.GetId());
    }
}

RPCResponse cmd_createwallet(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    try {
        std::string walletName = GetRequiredParam<std::string>(req, size_t(0));
        std::string passphrase = GetOptionalParam<std::string>(req, size_t(1), "");
        
        // Check if wallet already loaded
        if (table->GetWallet()) {
            return RPCResponse::Error(-4, "Wallet already loaded. Use unloadwallet first.", req.GetId());
        }
        
        // Create wallet config
        wallet::Wallet::Config config;
        config.name = walletName;
        
        // Generate new wallet and get mnemonic
        std::string mnemonic;
        auto wallet = wallet::Wallet::GenerateWithMnemonic(mnemonic, passphrase, 
                                                           wallet::Mnemonic::Strength::Words24, config);
        if (!wallet) {
            return RPCResponse::Error(-4, "Failed to create wallet", req.GetId());
        }
        
        // Construct wallet path using data directory
        std::string walletFile = walletName + ".dat";
        std::string walletPath = walletFile;
        if (!table->GetDataDir().empty()) {
            walletPath = JoinWalletPath(table->GetDataDir(), walletFile);
        }
        
        // Save wallet
        if (!wallet->Save(walletPath)) {
            return RPCResponse::Error(-4, "Failed to save wallet to: " + walletPath, req.GetId());
        }
        
        // Set wallet in command table
        table->SetWallet(std::move(wallet));
        
        JSONValue::Object result;
        result["name"] = walletName;
        result["path"] = walletPath;
        result["mnemonic"] = mnemonic;
        result["warning"] = "IMPORTANT: Write down these 24 words and store them securely! "
                           "This is the ONLY way to recover your wallet. "
                           "Anyone with these words can access your funds.";
        if (passphrase.empty()) {
            result["encryption_warning"] = "Wallet created without encryption. Use encryptwallet to secure it.";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return RPCResponse::Error(-4, std::string("Error creating wallet: ") + e.what(), req.GetId());
    }
}

RPCResponse cmd_unloadwallet(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "No wallet loaded", req.GetId());
    }
    
    try {
        std::string walletName = wallet->GetName();
        
        // Clear wallet from command table
        table->SetWallet(nullptr);
        
        JSONValue result;
        result["warning"] = "";
        
        return RPCResponse::Success(result, req.GetId());
    } catch (const std::exception& e) {
        return RPCResponse::Error(-4, std::string("Error unloading wallet: ") + e.what(), req.GetId());
    }
}

RPCResponse cmd_restorewallet(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    try {
        std::string walletName = GetRequiredParam<std::string>(req, size_t(0));
        std::string mnemonic = GetRequiredParam<std::string>(req, size_t(1));
        std::string passphrase = GetOptionalParam<std::string>(req, size_t(2), "");
        
        // Check if wallet already loaded
        if (table->GetWallet()) {
            return RPCResponse::Error(-4, "Wallet already loaded. Use unloadwallet first.", req.GetId());
        }
        
        // Validate mnemonic (should be 24 words)
        int wordCount = 0;
        bool inWord = false;
        for (char c : mnemonic) {
            if (std::isspace(c)) {
                inWord = false;
            } else if (!inWord) {
                inWord = true;
                ++wordCount;
            }
        }
        if (wordCount != 24) {
            return RPCResponse::Error(-4, "Invalid mnemonic: expected 24 words, got " + 
                                      std::to_string(wordCount), req.GetId());
        }
        
        // Create wallet config
        wallet::Wallet::Config config;
        config.name = walletName;
        
        // Restore wallet from mnemonic
        auto wallet = wallet::Wallet::FromMnemonic(mnemonic, passphrase, "", config);
        if (!wallet) {
            return RPCResponse::Error(-4, "Failed to restore wallet from mnemonic. "
                                      "Please check that the recovery phrase is correct.", req.GetId());
        }
        
        // Construct wallet path using data directory
        std::string walletFile = walletName + ".dat";
        std::string walletPath = walletFile;
        if (!table->GetDataDir().empty()) {
            walletPath = JoinWalletPath(table->GetDataDir(), walletFile);
        }
        
        // Save wallet
        if (!wallet->Save(walletPath)) {
            return RPCResponse::Error(-4, "Failed to save restored wallet to: " + walletPath, req.GetId());
        }
        
        // Set wallet in command table
        table->SetWallet(std::move(wallet));
        
        JSONValue::Object result;
        result["name"] = walletName;
        result["path"] = walletPath;
        result["warning"] = "Wallet restored successfully. "
                           "Note: Imported keys from the original wallet (if any) are NOT recovered. "
                           "You may need to rescan the blockchain to find existing transactions.";
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return RPCResponse::Error(-4, std::string("Error restoring wallet: ") + e.what(), req.GetId());
    }
}


// ============================================================================
// Identity Command Implementations
// ============================================================================

// Helper: Convert identity status to string
static std::string IdentityStatusStr(identity::IdentityStatus status) {
    switch (status) {
        case identity::IdentityStatus::Pending: return "pending";
        case identity::IdentityStatus::Active: return "active";
        case identity::IdentityStatus::Suspended: return "suspended";
        case identity::IdentityStatus::Revoked: return "revoked";
        case identity::IdentityStatus::Expired: return "expired";
        default: return "unknown";
    }
}

// Helper: Convert Hash256 to hex string for identity IDs
static std::string IdentityIdToHex(const Hash256& id) {
    std::ostringstream oss;
    for (int i = 31; i >= 0; --i) {
        oss << std::hex << std::setfill('0') << std::setw(2) 
            << static_cast<unsigned int>(id[i]);
    }
    return oss.str();
}

// Helper: Parse hex string to Hash256 for identity IDs
static Hash256 HexToIdentityId(const std::string& hex) {
    Hash256 id;
    if (hex.size() != 64) return id;
    
    for (size_t i = 0; i < 32; ++i) {
        std::string byteStr = hex.substr(62 - i * 2, 2);
        id[i] = static_cast<Byte>(std::stoul(byteStr, nullptr, 16));
    }
    return id;
}

RPCResponse cmd_getidentityinfo(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table) {
    try {
        std::string identityIdHex = GetRequiredParam<std::string>(req, size_t(0));
        
        identity::IdentityManager* idMgr = table->GetIdentityManager();
        if (!idMgr) {
            return RPCResponse::Error(-32603, "Identity manager not available", req.GetId());
        }
        
        Hash256 identityId = HexToIdentityId(identityIdHex);
        auto record = idMgr->GetIdentityById(identityId);
        
        JSONValue::Object result;
        result["identityId"] = identityIdHex;
        
        if (record) {
            result["hasIdentity"] = true;
            result["status"] = IdentityStatusStr(record->status);
            result["verified"] = (record->status == identity::IdentityStatus::Active);
            result["treeIndex"] = static_cast<int64_t>(record->treeIndex);
            result["registrationHeight"] = static_cast<int64_t>(record->registrationHeight);
            result["registrationTime"] = record->registrationTime;
            result["lastUpdateHeight"] = static_cast<int64_t>(record->lastUpdateHeight);
            result["expirationHeight"] = static_cast<int64_t>(record->expirationHeight);
            result["canClaimUBI"] = record->CanClaimUBI(idMgr->GetCurrentEpoch());
            
            // Get commitment hash for reference
            result["commitmentHash"] = IdentityIdToHex(record->id);
        } else {
            result["hasIdentity"] = false;
            result["status"] = "not_found";
            result["verified"] = false;
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_createidentity(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    try {
        // Get optional commitment data (hex encoded)
        std::string commitmentHex = GetOptionalParam<std::string>(req, size_t(0), "");
        
        identity::IdentityManager* idMgr = table->GetIdentityManager();
        if (!idMgr) {
            return RPCResponse::Error(-32603, "Identity manager not available", req.GetId());
        }
        
        wallet::Wallet* wallet = table->GetWallet();
        if (!wallet) {
            return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
        }
        
        // Generate new identity secrets
        identity::IdentitySecrets secrets = identity::IdentitySecrets::Generate();
        
        // Create registration request
        identity::RegistrationRequest request;
        request.commitment = secrets.GetCommitment();
        request.timestamp = GetTime();
        
        // Register the identity
        auto record = idMgr->RegisterIdentity(request);
        
        if (!record) {
            return RPCResponse::Error(-32000, "Identity registration failed - commitment may already be registered", req.GetId());
        }
        
        // Encrypt and store secrets in wallet (for later UBI claims)
        std::array<Byte, 32> encryptionKey;
        // Derive encryption key from wallet's master key
        // For now, use a simple derivation from wallet address
        auto addresses = wallet->GetAddresses();
        if (!addresses.empty()) {
            const std::string& addr = addresses[0];
            std::copy_n(reinterpret_cast<const Byte*>(addr.data()), 
                       std::min(addr.size(), size_t(32)), 
                       encryptionKey.begin());
        }
        
        // Update secrets with tree index
        secrets.treeIndex = record->treeIndex;
        
        // Store encrypted secrets (in real impl, wallet would have dedicated identity storage)
        std::vector<Byte> encryptedSecrets = secrets.Encrypt(encryptionKey);
        
        JSONValue::Object result;
        result["identityId"] = IdentityIdToHex(record->id);
        result["status"] = IdentityStatusStr(record->status);
        result["treeIndex"] = static_cast<int64_t>(record->treeIndex);
        result["registrationHeight"] = static_cast<int64_t>(record->registrationHeight);
        result["message"] = "Identity created successfully. It will become active after activation delay.";
        
        // Clear secrets from memory
        secrets.Clear();
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_verifyidentity(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    try {
        std::string identityIdHex = GetRequiredParam<std::string>(req, size_t(0));
        std::string proofHex = GetRequiredParam<std::string>(req, size_t(1));
        
        identity::IdentityManager* idMgr = table->GetIdentityManager();
        if (!idMgr) {
            return RPCResponse::Error(-32603, "Identity manager not available", req.GetId());
        }
        
        Hash256 identityId = HexToIdentityId(identityIdHex);
        auto record = idMgr->GetIdentityById(identityId);
        
        JSONValue::Object result;
        result["identityId"] = identityIdHex;
        
        if (!record) {
            result["valid"] = false;
            result["message"] = "Identity not found";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        // Get membership proof for verification
        auto membershipProof = idMgr->GetMembershipProof(record->commitment);
        
        if (!membershipProof) {
            result["valid"] = false;
            result["message"] = "Could not generate membership proof";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        // Verify the membership proof
        bool valid = idMgr->VerifyMembershipProof(record->commitment, *membershipProof);
        
        result["valid"] = valid;
        result["status"] = IdentityStatusStr(record->status);
        result["message"] = valid ? "Identity verified successfully" : "Identity verification failed";
        result["isActive"] = record->IsActive();
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getidentitystatus(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table) {
    try {
        std::string identityIdHex = GetRequiredParam<std::string>(req, size_t(0));
        
        identity::IdentityManager* idMgr = table->GetIdentityManager();
        if (!idMgr) {
            return RPCResponse::Error(-32603, "Identity manager not available", req.GetId());
        }
        
        Hash256 identityId = HexToIdentityId(identityIdHex);
        auto record = idMgr->GetIdentityById(identityId);
        
        JSONValue::Object result;
        result["identityId"] = identityIdHex;
        
        if (record) {
            result["exists"] = true;
            result["status"] = IdentityStatusStr(record->status);
            result["isActive"] = record->IsActive();
            result["canClaimUBI"] = record->CanClaimUBI(idMgr->GetCurrentEpoch());
            result["treeIndex"] = static_cast<int64_t>(record->treeIndex);
            result["registrationHeight"] = static_cast<int64_t>(record->registrationHeight);
            
            // Get identity statistics
            auto stats = idMgr->GetStats();
            result["currentEpoch"] = static_cast<int64_t>(stats.currentEpoch);
            result["totalIdentities"] = static_cast<int64_t>(stats.totalIdentities);
            result["activeIdentities"] = static_cast<int64_t>(stats.activeIdentities);
        } else {
            result["exists"] = false;
            result["status"] = "not_found";
            result["isActive"] = false;
            result["canClaimUBI"] = false;
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_claimubi(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table) {
    try {
        std::string identityIdHex = GetRequiredParam<std::string>(req, size_t(0));
        std::string recipientAddress = GetOptionalParam<std::string>(req, size_t(1), "");
        
        identity::IdentityManager* idMgr = table->GetIdentityManager();
        if (!idMgr) {
            return RPCResponse::Error(-32603, "Identity manager not available", req.GetId());
        }
        
        economics::UBIDistributor* ubiDist = table->GetUBIDistributor();
        if (!ubiDist) {
            return RPCResponse::Error(-32603, "UBI distributor not available", req.GetId());
        }
        
        wallet::Wallet* wallet = table->GetWallet();
        if (!wallet) {
            return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
        }
        
        Hash256 identityId = HexToIdentityId(identityIdHex);
        auto record = idMgr->GetIdentityById(identityId);
        
        JSONValue::Object result;
        result["identityId"] = identityIdHex;
        
        if (!record) {
            result["success"] = false;
            result["amount"] = FormatAmount(0);
            result["message"] = "Identity not found";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        if (!record->IsActive()) {
            result["success"] = false;
            result["amount"] = FormatAmount(0);
            result["message"] = "Identity is not active (status: " + IdentityStatusStr(record->status) + ")";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        identity::EpochId currentEpoch = idMgr->GetCurrentEpoch();
        
        // Get recipient address
        Hash160 recipient;
        if (recipientAddress.empty()) {
            // Use wallet's default address
            auto addresses = wallet->GetAddresses();
            if (addresses.empty()) {
                result["success"] = false;
                result["amount"] = FormatAmount(0);
                result["message"] = "No recipient address available";
                return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
            }
            // Convert address string to Hash160 (simplified)
            recipientAddress = addresses[0];
        }
        
        // Parse recipient address to Hash160
        // Use proper address decoding (bech32 or base58)
        if (!recipientAddress.empty()) {
            auto keyHash = AddressToKeyHash(recipientAddress);
            if (keyHash) {
                recipient = *keyHash;
            } else {
                // Try decoding as raw scriptPubKey
                auto decoded = DecodeAddress(recipientAddress);
                if (decoded.size() >= 20) {
                    // Extract hash160 from scriptPubKey (P2WPKH: 0014 + 20 bytes)
                    if (decoded.size() == 22 && decoded[0] == 0x00 && decoded[1] == 0x14) {
                        std::memcpy(recipient.data(), decoded.data() + 2, 20);
                    }
                    // P2PKH: 76a914 + 20 bytes + 88ac
                    else if (decoded.size() == 25 && decoded[0] == 0x76) {
                        std::memcpy(recipient.data(), decoded.data() + 3, 20);
                    }
                } else {
                    result["success"] = false;
                    result["amount"] = FormatAmount(0);
                    result["message"] = "Invalid recipient address format";
                    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
                }
            }
        }
        
        // Use wallet to create the UBI claim
        auto [claimOpt, error] = wallet->CreateUBIClaim(currentEpoch, recipient);
        
        if (!claimOpt) {
            result["success"] = false;
            result["amount"] = FormatAmount(0);
            result["message"] = error.empty() ? "Failed to create claim" : error;
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        economics::UBIClaim claim = *claimOpt;
        
        // Get chain state for current height
        ChainState* chainState = table->GetChainState();
        int currentHeight = chainState ? static_cast<int>(chainState->GetHeight()) : 0;
        
        // Get identity tree root
        auto identityRoot = idMgr->GetIdentityRoot();
        Hash256 treeRootHash;
        // Convert FieldElement to Hash256 (simplified)
        std::memset(treeRootHash.data(), 0, 32);
        
        // Process the claim
        economics::ClaimStatus status = ubiDist->ProcessClaim(claim, treeRootHash, currentHeight);
        
        result["epoch"] = static_cast<int64_t>(currentEpoch);
        result["claimStatus"] = economics::ClaimStatusToString(status);
        
        if (status == economics::ClaimStatus::Valid) {
            result["success"] = true;
            result["amount"] = FormatAmount(claim.amount);
            result["message"] = "UBI claimed successfully";
            result["txid"] = ""; // Would be set if claim creates a transaction
        } else {
            result["success"] = false;
            result["amount"] = FormatAmount(0);
            
            // Provide helpful error messages
            switch (status) {
                case economics::ClaimStatus::DoubleClaim:
                    result["message"] = "Already claimed UBI for this epoch";
                    break;
                case economics::ClaimStatus::InvalidProof:
                    result["message"] = "Invalid claim proof";
                    break;
                case economics::ClaimStatus::IdentityNotFound:
                    result["message"] = "Identity not found in tree";
                    break;
                case economics::ClaimStatus::EpochExpired:
                    result["message"] = "Epoch claim window has expired";
                    break;
                case economics::ClaimStatus::EpochNotComplete:
                    result["message"] = "Epoch has not completed yet";
                    break;
                case economics::ClaimStatus::PoolEmpty:
                    result["message"] = "UBI pool is empty for this epoch";
                    break;
                default:
                    result["message"] = "Claim processing failed";
            }
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getubiinfo(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table) {
    try {
        std::string identityIdHex = GetRequiredParam<std::string>(req, size_t(0));
        
        identity::IdentityManager* idMgr = table->GetIdentityManager();
        if (!idMgr) {
            return RPCResponse::Error(-32603, "Identity manager not available", req.GetId());
        }
        
        economics::UBIDistributor* ubiDist = table->GetUBIDistributor();
        if (!ubiDist) {
            return RPCResponse::Error(-32603, "UBI distributor not available", req.GetId());
        }
        
        Hash256 identityId = HexToIdentityId(identityIdHex);
        auto record = idMgr->GetIdentityById(identityId);
        
        JSONValue::Object result;
        result["identityId"] = identityIdHex;
        
        identity::EpochId currentEpoch = ubiDist->GetCurrentEpoch();
        result["currentEpoch"] = static_cast<int64_t>(currentEpoch);
        
        if (!record) {
            result["eligible"] = false;
            result["reason"] = "Identity not found";
            result["pendingAmount"] = FormatAmount(0);
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        bool isEligible = record->IsActive() && record->CanClaimUBI(currentEpoch);
        result["eligible"] = isEligible;
        result["identityStatus"] = IdentityStatusStr(record->status);
        
        // Get current epoch pool info
        const economics::EpochUBIPool* pool = ubiDist->GetPool(currentEpoch);
        if (pool && pool->isFinalized) {
            result["amountPerPerson"] = FormatAmount(pool->amountPerPerson);
            result["pendingAmount"] = FormatAmount(isEligible ? pool->amountPerPerson : 0);
            result["poolTotal"] = FormatAmount(pool->totalPool);
            result["poolClaimed"] = FormatAmount(pool->amountClaimed);
            result["claimCount"] = static_cast<int64_t>(pool->claimCount);
            result["eligibleCount"] = static_cast<int64_t>(pool->eligibleCount);
            result["claimRate"] = pool->ClaimRate();
        } else {
            // Pool not finalized yet - show estimated amount
            auto stats = idMgr->GetStats();
            Amount estimated = ubiDist->GetAmountPerPerson(currentEpoch);
            result["amountPerPerson"] = FormatAmount(estimated);
            result["pendingAmount"] = FormatAmount(isEligible ? estimated : 0);
            result["poolFinalized"] = false;
            result["activeIdentities"] = static_cast<int64_t>(stats.activeIdentities);
        }
        
        // Get distribution stats
        result["totalDistributed"] = FormatAmount(ubiDist->GetTotalDistributed());
        result["totalClaimsAllTime"] = static_cast<int64_t>(ubiDist->GetTotalClaims());
        result["averageClaimRate"] = ubiDist->GetAverageClaimRate();
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getubihistory(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    try {
        std::string identityIdHex = GetRequiredParam<std::string>(req, size_t(0));
        int64_t count = GetOptionalParam<int64_t>(req, size_t(1), int64_t(10));
        
        identity::IdentityManager* idMgr = table->GetIdentityManager();
        economics::UBIDistributor* ubiDist = table->GetUBIDistributor();
        
        if (!idMgr || !ubiDist) {
            return RPCResponse::Error(-32603, "Identity/UBI systems not available", req.GetId());
        }
        
        Hash256 identityId = HexToIdentityId(identityIdHex);
        auto record = idMgr->GetIdentityById(identityId);
        
        JSONValue::Array history;
        
        if (!record) {
            // Return empty history for unknown identity
            return RPCResponse::Success(JSONValue(std::move(history)), req.GetId());
        }
        
        // Get historical epoch data
        identity::EpochId currentEpoch = ubiDist->GetCurrentEpoch();
        int64_t epochsToShow = std::min(count, static_cast<int64_t>(currentEpoch));
        
        for (int64_t i = 0; i < epochsToShow; ++i) {
            identity::EpochId epoch = currentEpoch - 1 - static_cast<identity::EpochId>(i);
            auto stats = ubiDist->GetEpochStats(epoch);
            
            JSONValue::Object entry;
            entry["epoch"] = static_cast<int64_t>(epoch);
            entry["poolSize"] = FormatAmount(stats.poolSize);
            entry["amountPerPerson"] = FormatAmount(stats.poolSize / std::max(1u, stats.eligibleCount));
            entry["distributed"] = FormatAmount(stats.distributed);
            entry["claimRate"] = stats.claimRate;
            entry["eligibleCount"] = static_cast<int64_t>(stats.eligibleCount);
            entry["claimCount"] = static_cast<int64_t>(stats.claimCount);
            
            // Note: Per-identity claim history would require additional tracking
            // For now, show epoch-level stats
            entry["type"] = "epoch_summary";
            
            history.push_back(JSONValue(std::move(entry)));
        }
        
        return RPCResponse::Success(JSONValue(std::move(history)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

// ============================================================================
// Staking Command Implementations
// ============================================================================

// Helper: Convert validator status to string
static std::string ValidatorStatusStr(staking::ValidatorStatus status) {
    switch (status) {
        case staking::ValidatorStatus::Pending: return "pending";
        case staking::ValidatorStatus::Active: return "active";
        case staking::ValidatorStatus::Inactive: return "inactive";
        case staking::ValidatorStatus::Jailed: return "jailed";
        case staking::ValidatorStatus::Tombstoned: return "tombstoned";
        case staking::ValidatorStatus::Unbonding: return "unbonding";
        default: return "unknown";
    }
}

// Helper: Convert delegation status to string
static std::string DelegationStatusStr(staking::DelegationStatus status) {
    switch (status) {
        case staking::DelegationStatus::Active: return "active";
        case staking::DelegationStatus::Unbonding: return "unbonding";
        case staking::DelegationStatus::Completed: return "completed";
        default: return "unknown";
    }
}

// Helper: Convert ValidatorId (Hash160) to hex string
static std::string ValidatorIdToHex(const staking::ValidatorId& id) {
    std::ostringstream oss;
    for (int i = 19; i >= 0; --i) {
        oss << std::hex << std::setfill('0') << std::setw(2) 
            << static_cast<unsigned int>(id[i]);
    }
    return oss.str();
}

// Helper: Parse hex string to ValidatorId (Hash160)
static staking::ValidatorId HexToValidatorId(const std::string& hex) {
    staking::ValidatorId id;
    if (hex.size() != 40) return id;
    for (size_t i = 0; i < 20; ++i) {
        std::string byteStr = hex.substr(38 - i * 2, 2);
        id[i] = static_cast<Byte>(std::stoul(byteStr, nullptr, 16));
    }
    return id;
}

// Helper: Convert DelegationId to hex
static std::string DelegationIdToHex(const staking::DelegationId& id) {
    return IdentityIdToHex(id);
}

// Helper: Parse hex to DelegationId
static staking::DelegationId HexToDelegationId(const std::string& hex) {
    return HexToIdentityId(hex);
}

RPCResponse cmd_getstakinginfo(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    JSONValue::Object result;
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        result["enabled"] = false;
        result["staking"] = false;
        result["totalStaked"] = FormatAmount(0);
        result["activeValidators"] = int64_t(0);
        result["message"] = "Staking engine not available";
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    }
    
    const staking::ValidatorSet& valSet = engine->GetValidatorSet();
    const staking::RewardDistributor& rewards = engine->GetRewardDistributor();
    
    auto activeValidators = valSet.GetActiveSet();
    size_t pendingCount = valSet.GetValidatorCount(staking::ValidatorStatus::Pending);
    size_t jailedCount = valSet.GetValidatorCount(staking::ValidatorStatus::Jailed);
    
    result["enabled"] = true;
    result["staking"] = !activeValidators.empty();
    result["totalStaked"] = FormatAmount(engine->GetTotalStaked());
    result["activeValidators"] = static_cast<int64_t>(activeValidators.size());
    result["pendingValidators"] = static_cast<int64_t>(pendingCount);
    result["jailedValidators"] = static_cast<int64_t>(jailedCount);
    result["networkAPY"] = static_cast<double>(engine->GetNetworkAPY()) / 100.0;
    result["totalRewardsDistributed"] = FormatAmount(rewards.GetTotalRewardsDistributed());
    result["currentEpoch"] = static_cast<int64_t>(rewards.GetCurrentEpoch());
    result["currentHeight"] = static_cast<int64_t>(engine->GetCurrentHeight());
    result["minValidatorStake"] = FormatAmount(staking::MIN_VALIDATOR_STAKE);
    result["minDelegationStake"] = FormatAmount(staking::MIN_DELEGATION_STAKE);
    result["unbondingPeriodBlocks"] = static_cast<int64_t>(staking::UNBONDING_PERIOD);
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_getvalidatorinfo(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table) {
    try {
        std::string validatorIdHex = GetRequiredParam<std::string>(req, size_t(0));
        
        staking::StakingEngine* engine = table->GetStakingEngine();
        if (!engine) {
            return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
        }
        
        staking::ValidatorId validatorId = HexToValidatorId(validatorIdHex);
        const staking::ValidatorSet& valSet = engine->GetValidatorSet();
        auto validator = valSet.GetValidator(validatorId);
        
        JSONValue::Object result;
        result["validatorId"] = validatorIdHex;
        
        if (!validator) {
            result["exists"] = false;
            result["message"] = "Validator not found";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        result["exists"] = true;
        result["moniker"] = validator->moniker;
        result["description"] = validator->description;
        result["status"] = ValidatorStatusStr(validator->status);
        result["isActive"] = valSet.IsActive(validatorId);
        result["selfStake"] = FormatAmount(validator->selfStake);
        result["delegatedStake"] = FormatAmount(validator->delegatedStake);
        result["totalStake"] = FormatAmount(validator->GetTotalStake());
        result["votingPower"] = static_cast<int64_t>(validator->GetVotingPower());
        result["commissionRate"] = static_cast<double>(validator->commissionRate) / 100.0;
        result["accumulatedRewards"] = FormatAmount(validator->accumulatedRewards);
        result["totalRewardsEarned"] = FormatAmount(validator->totalRewardsEarned);
        result["blocksProduced"] = static_cast<int64_t>(validator->blocksProduced);
        result["missedBlocksPercent"] = validator->GetMissedBlocksPercent();
        result["registrationHeight"] = static_cast<int64_t>(validator->registrationHeight);
        result["jailed"] = (validator->status == staking::ValidatorStatus::Jailed);
        result["slashCount"] = static_cast<int64_t>(validator->slashCount);
        result["totalSlashed"] = FormatAmount(validator->totalSlashed);
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_listvalidators(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    std::string statusFilter = GetOptionalParam<std::string>(req, size_t(0), "active");
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        // Return empty array when staking engine not available
        return RPCResponse::Success(JSONValue(JSONValue::Array()), req.GetId());
    }
    
    const staking::ValidatorSet& valSet = engine->GetValidatorSet();
    std::vector<staking::Validator> validators;
    
    if (statusFilter == "active") {
        validators = valSet.GetActiveSet();
    } else if (statusFilter == "pending") {
        validators = valSet.GetValidatorsByStatus(staking::ValidatorStatus::Pending);
    } else if (statusFilter == "jailed") {
        validators = valSet.GetValidatorsByStatus(staking::ValidatorStatus::Jailed);
    } else if (statusFilter == "all") {
        validators = valSet.GetActiveSet();
        auto pending = valSet.GetValidatorsByStatus(staking::ValidatorStatus::Pending);
        auto jailed = valSet.GetValidatorsByStatus(staking::ValidatorStatus::Jailed);
        validators.insert(validators.end(), pending.begin(), pending.end());
        validators.insert(validators.end(), jailed.begin(), jailed.end());
    } else {
        return InvalidParams("Invalid status filter. Use: active, pending, jailed, all", req.GetId());
    }
    
    JSONValue::Array result;
    for (const auto& val : validators) {
        JSONValue::Object entry;
        entry["validatorId"] = ValidatorIdToHex(val.id);
        entry["moniker"] = val.moniker;
        entry["status"] = ValidatorStatusStr(val.status);
        entry["totalStake"] = FormatAmount(val.GetTotalStake());
        entry["selfStake"] = FormatAmount(val.selfStake);
        entry["delegatedStake"] = FormatAmount(val.delegatedStake);
        entry["commissionRate"] = static_cast<double>(val.commissionRate) / 100.0;
        entry["votingPower"] = static_cast<int64_t>(val.GetVotingPower());
        entry["blocksProduced"] = static_cast<int64_t>(val.blocksProduced);
        entry["jailed"] = (val.status == staking::ValidatorStatus::Jailed);
        result.push_back(JSONValue(std::move(entry)));
    }
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_createvalidator(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
    }
    
    try {
        Amount amount = ParseAmount(req.GetParam(size_t(0)));
        int64_t commission = GetRequiredParam<int64_t>(req, size_t(1));
        std::string moniker = GetRequiredParam<std::string>(req, size_t(2));
        std::string description = GetOptionalParam<std::string>(req, size_t(3), "");
        
        if (amount < staking::MIN_VALIDATOR_STAKE) {
            return InvalidParams("Minimum validator stake is 100,000 NXS", req.GetId());
        }
        
        if (commission < 0 || commission > staking::MAX_COMMISSION_RATE) {
            return InvalidParams("Commission must be between 0 and 5000 basis points", req.GetId());
        }
        
        Amount balance = wallet->GetBalance().confirmed;
        if (balance < amount) {
            return RPCResponse::Error(-6, "Insufficient funds", req.GetId());
        }
        
        staking::Validator validator;
        auto addresses = wallet->GetAddresses();
        if (addresses.empty()) {
            return RPCResponse::Error(-4, "No addresses in wallet", req.GetId());
        }
        
        std::copy_n(reinterpret_cast<const Byte*>(addresses[0].data()),
                   std::min(addresses[0].size(), size_t(20)), validator.id.begin());
        validator.moniker = moniker;
        validator.description = description;
        validator.selfStake = amount;
        validator.commissionRate = static_cast<int>(commission);
        validator.status = staking::ValidatorStatus::Pending;
        
        std::vector<Byte> signature;
        bool success = engine->RegisterValidator(validator, signature);
        
        JSONValue::Object result;
        if (success) {
            result["success"] = true;
            result["validatorId"] = ValidatorIdToHex(validator.id);
            result["status"] = "pending";
            result["selfStake"] = FormatAmount(amount);
            result["commissionRate"] = static_cast<double>(commission) / 100.0;
            result["message"] = "Validator registered successfully";
        } else {
            result["success"] = false;
            result["message"] = "Validator registration failed";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_updatevalidator(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table) {
    if (!table->GetWallet()) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
    }
    
    try {
        std::string validatorIdHex = GetRequiredParam<std::string>(req, size_t(0));
        
        // Parse validator ID
        staking::ValidatorId validatorId = HexToValidatorId(validatorIdHex);
        
        // Get the validator set
        staking::ValidatorSet& valSet = engine->GetValidatorSet();
        auto validator = valSet.GetValidator(validatorId);
        if (!validator) {
            return RPCResponse::Error(-5, "Validator not found", req.GetId());
        }
        
        // Get optional update parameters (use existing values as defaults)
        std::string moniker = validator->moniker;
        std::string description = validator->description;
        int commissionRate = validator->commissionRate;
        
        // Check for optional parameters
        if (req.GetParams().IsArray() && req.GetParams().GetArray().size() > 1) {
            auto params = req.GetParams().GetArray();
            
            // Parameter 1: moniker (optional)
            if (params.size() > 1 && !params[1].IsNull() && params[1].IsString()) {
                moniker = params[1].GetString();
                if (moniker.length() > 64) {
                    return InvalidParams("Moniker too long (max 64 chars)", req.GetId());
                }
            }
            
            // Parameter 2: description (optional)
            if (params.size() > 2 && !params[2].IsNull() && params[2].IsString()) {
                description = params[2].GetString();
                if (description.length() > 256) {
                    return InvalidParams("Description too long (max 256 chars)", req.GetId());
                }
            }
            
            // Parameter 3: commission rate (optional, basis points)
            if (params.size() > 3 && !params[3].IsNull()) {
                commissionRate = static_cast<int>(params[3].GetInt());
                if (commissionRate < staking::MIN_COMMISSION_RATE || 
                    commissionRate > staking::MAX_COMMISSION_RATE) {
                    return InvalidParams("Commission must be 0-5000 basis points", req.GetId());
                }
                
                // Check commission change limit
                int change = std::abs(commissionRate - validator->commissionRate);
                if (change > staking::MAX_COMMISSION_CHANGE) {
                    return InvalidParams("Commission change exceeds maximum (500 basis points per update)", req.GetId());
                }
            }
        }
        
        // Create signature (would normally come from wallet signing)
        std::vector<Byte> signature;
        
        // Update the validator
        bool success = valSet.UpdateValidator(validatorId, moniker, description, commissionRate, signature);
        
        JSONValue::Object result;
        if (success) {
            result["success"] = true;
            result["validatorId"] = validatorIdHex;
            result["moniker"] = moniker;
            result["description"] = description;
            result["commissionRate"] = static_cast<double>(commissionRate) / 100.0;
            result["message"] = "Validator updated successfully";
        } else {
            result["success"] = false;
            result["message"] = "Failed to update validator";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_delegate(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
    }
    
    try {
        std::string validatorIdHex = GetRequiredParam<std::string>(req, size_t(0));
        Amount amount = ParseAmount(req.GetParam(size_t(1)));
        
        if (amount < staking::MIN_DELEGATION_STAKE) {
            return InvalidParams("Minimum delegation is 100 NXS", req.GetId());
        }
        
        Amount balance = wallet->GetBalance().confirmed;
        if (balance < amount) {
            return RPCResponse::Error(-6, "Insufficient funds", req.GetId());
        }
        
        staking::ValidatorId validatorId = HexToValidatorId(validatorIdHex);
        const staking::ValidatorSet& valSet = engine->GetValidatorSet();
        if (!valSet.ValidatorExists(validatorId)) {
            return RPCResponse::Error(-5, "Validator not found", req.GetId());
        }
        
        Hash160 delegator;
        auto addresses = wallet->GetAddresses();
        if (addresses.empty()) {
            return RPCResponse::Error(-4, "No addresses in wallet", req.GetId());
        }
        std::copy_n(reinterpret_cast<const Byte*>(addresses[0].data()),
                   std::min(addresses[0].size(), size_t(20)), delegator.begin());
        
        std::vector<Byte> signature;
        auto delegationId = engine->Delegate(delegator, validatorId, amount, signature);
        
        JSONValue::Object result;
        if (delegationId) {
            result["success"] = true;
            result["delegationId"] = DelegationIdToHex(*delegationId);
            result["validatorId"] = validatorIdHex;
            result["amount"] = FormatAmount(amount);
            result["status"] = "active";
            result["message"] = "Delegation created successfully";
        } else {
            result["success"] = false;
            result["message"] = "Delegation failed";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_undelegate(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
    }
    
    try {
        std::string delegationIdHex = GetRequiredParam<std::string>(req, size_t(0));
        Amount amount = ParseAmount(req.GetParam(size_t(1)));
        
        staking::DelegationId delegationId = HexToDelegationId(delegationIdHex);
        staking::StakingPool& pool = engine->GetStakingPool();
        
        auto delegation = pool.GetDelegation(delegationId);
        if (!delegation) {
            return RPCResponse::Error(-5, "Delegation not found", req.GetId());
        }
        
        if (amount > delegation->amount) {
            return InvalidParams("Cannot undelegate more than delegated amount", req.GetId());
        }
        
        std::vector<Byte> signature;
        bool success = pool.Undelegate(delegationId, amount, signature);
        
        JSONValue::Object result;
        if (success) {
            int currentHeight = engine->GetCurrentHeight();
            result["success"] = true;
            result["delegationId"] = delegationIdHex;
            result["amount"] = FormatAmount(amount);
            result["unbondingStart"] = static_cast<int64_t>(currentHeight);
            result["unbondingComplete"] = static_cast<int64_t>(currentHeight + staking::UNBONDING_PERIOD);
            result["status"] = "unbonding";
            result["message"] = "Undelegation started";
        } else {
            result["success"] = false;
            result["message"] = "Undelegation failed";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_listdelegations(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
    }
    
    Hash160 delegator;
    auto addresses = wallet->GetAddresses();
    if (addresses.empty()) {
        JSONValue::Object result;
        result["delegations"] = JSONValue::Array();
        result["totalDelegated"] = FormatAmount(0);
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    }
    std::copy_n(reinterpret_cast<const Byte*>(addresses[0].data()),
               std::min(addresses[0].size(), size_t(20)), delegator.begin());
    
    const staking::StakingPool& pool = engine->GetStakingPool();
    auto delegations = pool.GetDelegationsByDelegator(delegator);
    
    int currentHeight = engine->GetCurrentHeight();
    Amount totalDelegated = 0;
    Amount totalPendingRewards = 0;
    
    JSONValue::Array delegationList;
    for (const auto& del : delegations) {
        JSONValue::Object entry;
        entry["delegationId"] = DelegationIdToHex(del.id);
        entry["validatorId"] = ValidatorIdToHex(del.validatorId);
        entry["amount"] = FormatAmount(del.amount);
        entry["status"] = DelegationStatusStr(del.status);
        entry["pendingRewards"] = FormatAmount(del.pendingRewards);
        entry["totalRewardsClaimed"] = FormatAmount(del.totalRewardsClaimed);
        entry["creationHeight"] = static_cast<int64_t>(del.creationHeight);
        entry["canClaimRewards"] = del.CanClaimRewards(currentHeight);
        
        totalDelegated += del.amount;
        totalPendingRewards += del.pendingRewards;
        delegationList.push_back(JSONValue(std::move(entry)));
    }
    
    JSONValue::Object result;
    result["delegations"] = JSONValue(std::move(delegationList));
    result["totalDelegated"] = FormatAmount(totalDelegated);
    result["totalPendingRewards"] = FormatAmount(totalPendingRewards);
    result["delegationCount"] = static_cast<int64_t>(delegations.size());
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_claimrewards(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
    }
    
    std::string delegationIdHex = GetOptionalParam<std::string>(req, size_t(0), "");
    
    staking::StakingPool& pool = engine->GetStakingPool();
    int currentHeight = engine->GetCurrentHeight();
    Amount totalClaimed = 0;
    JSONValue::Array claimedList;
    
    if (!delegationIdHex.empty()) {
        staking::DelegationId delegationId = HexToDelegationId(delegationIdHex);
        auto delegation = pool.GetDelegation(delegationId);
        if (!delegation) {
            return RPCResponse::Error(-5, "Delegation not found", req.GetId());
        }
        if (!delegation->CanClaimRewards(currentHeight)) {
            JSONValue::Object result;
            result["success"] = false;
            result["message"] = "Cannot claim rewards yet (cooldown period)";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        std::vector<Byte> signature;
        Amount claimed = pool.ClaimRewards(delegationId, signature);
        if (claimed > 0) {
            JSONValue::Object entry;
            entry["delegationId"] = delegationIdHex;
            entry["amount"] = FormatAmount(claimed);
            claimedList.push_back(JSONValue(std::move(entry)));
            totalClaimed = claimed;
        }
    } else {
        Hash160 delegator;
        auto addresses = wallet->GetAddresses();
        if (!addresses.empty()) {
            std::copy_n(reinterpret_cast<const Byte*>(addresses[0].data()),
                       std::min(addresses[0].size(), size_t(20)), delegator.begin());
            auto delegations = pool.GetDelegationsByDelegator(delegator);
            for (const auto& del : delegations) {
                if (del.CanClaimRewards(currentHeight) && del.pendingRewards > 0) {
                    std::vector<Byte> signature;
                    Amount claimed = pool.ClaimRewards(del.id, signature);
                    if (claimed > 0) {
                        JSONValue::Object entry;
                        entry["delegationId"] = DelegationIdToHex(del.id);
                        entry["amount"] = FormatAmount(claimed);
                        claimedList.push_back(JSONValue(std::move(entry)));
                        totalClaimed += claimed;
                    }
                }
            }
        }
    }
    
    JSONValue::Object result;
    result["success"] = (totalClaimed > 0);
    result["totalClaimed"] = FormatAmount(totalClaimed);
    result["claimedDelegations"] = JSONValue(std::move(claimedList));
    result["message"] = (totalClaimed > 0) ? "Rewards claimed" : "No rewards to claim";
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_getpendingrewards(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
    }
    
    Hash160 delegator;
    auto addresses = wallet->GetAddresses();
    if (addresses.empty()) {
        JSONValue::Object result;
        result["total"] = FormatAmount(0);
        result["delegations"] = JSONValue::Array();
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    }
    std::copy_n(reinterpret_cast<const Byte*>(addresses[0].data()),
               std::min(addresses[0].size(), size_t(20)), delegator.begin());
    
    const staking::StakingPool& pool = engine->GetStakingPool();
    auto delegations = pool.GetDelegationsByDelegator(delegator);
    int currentHeight = engine->GetCurrentHeight();
    Amount totalPending = 0;
    JSONValue::Array rewardsList;
    
    for (const auto& del : delegations) {
        if (del.pendingRewards > 0) {
            JSONValue::Object entry;
            entry["delegationId"] = DelegationIdToHex(del.id);
            entry["validatorId"] = ValidatorIdToHex(del.validatorId);
            entry["pendingRewards"] = FormatAmount(del.pendingRewards);
            entry["canClaim"] = del.CanClaimRewards(currentHeight);
            entry["delegatedAmount"] = FormatAmount(del.amount);
            rewardsList.push_back(JSONValue(std::move(entry)));
            totalPending += del.pendingRewards;
        }
    }
    
    JSONValue::Object result;
    result["total"] = FormatAmount(totalPending);
    result["delegationCount"] = static_cast<int64_t>(rewardsList.size());
    result["delegations"] = JSONValue(std::move(rewardsList));
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_unjailvalidator(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    staking::StakingEngine* engine = table->GetStakingEngine();
    if (!engine) {
        return RPCResponse::Error(-32603, "Staking engine not available", req.GetId());
    }
    
    try {
        std::string validatorIdHex = GetRequiredParam<std::string>(req, size_t(0));
        
        staking::ValidatorId validatorId = HexToValidatorId(validatorIdHex);
        staking::ValidatorSet& valSet = engine->GetValidatorSet();
        
        auto validator = valSet.GetValidator(validatorId);
        if (!validator) {
            return RPCResponse::Error(-5, "Validator not found", req.GetId());
        }
        
        if (validator->status != staking::ValidatorStatus::Jailed) {
            return RPCResponse::Error(-32000, "Validator is not jailed", req.GetId());
        }
        
        int currentHeight = engine->GetCurrentHeight();
        if (!validator->IsJailExpired(currentHeight)) {
            int remaining = (validator->jailedHeight + staking::JAIL_DURATION) - currentHeight;
            return RPCResponse::Error(-32000, "Jail period not expired. " + 
                std::to_string(remaining) + " blocks remaining.", req.GetId());
        }
        
        std::vector<Byte> signature;
        bool success = valSet.UnjailValidator(validatorId, signature);
        
        JSONValue::Object result;
        if (success) {
            result["success"] = true;
            result["validatorId"] = validatorIdHex;
            result["status"] = "active";
            result["message"] = "Validator unjailed successfully";
        } else {
            result["success"] = false;
            result["message"] = "Unjail failed";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}


// ============================================================================
// Governance Command Implementations
// ============================================================================

// Helper: Convert proposal ID to hex
static std::string ProposalIdToHex(const governance::GovernanceProposalId& id) {
    return IdentityIdToHex(id);
}

// Helper: Parse hex to proposal ID
static governance::GovernanceProposalId HexToProposalId(const std::string& hex) {
    return HexToIdentityId(hex);
}

// Helper: Convert VoterId to hex
static std::string VoterIdToHex(const governance::VoterId& id) {
    return ValidatorIdToHex(id);
}

// Helper: Parse hex to VoterId
static governance::VoterId HexToVoterId(const std::string& hex) {
    return HexToValidatorId(hex);
}

// Helper: Convert proposal type to string
static std::string ProposalTypeStr(governance::ProposalType type) {
    switch (type) {
        case governance::ProposalType::Parameter: return "parameter";
        case governance::ProposalType::Protocol: return "protocol";
        case governance::ProposalType::Constitutional: return "constitutional";
        case governance::ProposalType::Emergency: return "emergency";
        case governance::ProposalType::Signal: return "signal";
        default: return "unknown";
    }
}

// Helper: Parse proposal type from string
static std::optional<governance::ProposalType> ParseProposalTypeStr(const std::string& str) {
    if (str == "parameter") return governance::ProposalType::Parameter;
    if (str == "protocol") return governance::ProposalType::Protocol;
    if (str == "constitutional") return governance::ProposalType::Constitutional;
    if (str == "emergency") return governance::ProposalType::Emergency;
    if (str == "signal") return governance::ProposalType::Signal;
    return std::nullopt;
}

// Helper: Convert governance status to string
static std::string GovernanceStatusStr(governance::GovernanceStatus status) {
    switch (status) {
        case governance::GovernanceStatus::Draft: return "draft";
        case governance::GovernanceStatus::Pending: return "pending";
        case governance::GovernanceStatus::Active: return "active";
        case governance::GovernanceStatus::Approved: return "approved";
        case governance::GovernanceStatus::Rejected: return "rejected";
        case governance::GovernanceStatus::QuorumFailed: return "quorum_failed";
        case governance::GovernanceStatus::Executed: return "executed";
        case governance::GovernanceStatus::ExecutionFailed: return "execution_failed";
        case governance::GovernanceStatus::Cancelled: return "cancelled";
        case governance::GovernanceStatus::Vetoed: return "vetoed";
        case governance::GovernanceStatus::Expired: return "expired";
        default: return "unknown";
    }
}

// Helper: Parse governance status from string
static std::optional<governance::GovernanceStatus> ParseGovernanceStatusStr(const std::string& str) {
    if (str == "draft") return governance::GovernanceStatus::Draft;
    if (str == "pending") return governance::GovernanceStatus::Pending;
    if (str == "active") return governance::GovernanceStatus::Active;
    if (str == "approved") return governance::GovernanceStatus::Approved;
    if (str == "rejected") return governance::GovernanceStatus::Rejected;
    if (str == "executed") return governance::GovernanceStatus::Executed;
    return std::nullopt;
}

// Helper: Convert vote choice to string
static std::string VoteChoiceStr(governance::VoteChoice choice) {
    switch (choice) {
        case governance::VoteChoice::Yes: return "yes";
        case governance::VoteChoice::No: return "no";
        case governance::VoteChoice::Abstain: return "abstain";
        case governance::VoteChoice::NoWithVeto: return "veto";
        default: return "unknown";
    }
}

// Helper: Parse vote choice from string
static std::optional<governance::VoteChoice> ParseVoteChoiceStr(const std::string& str) {
    if (str == "yes") return governance::VoteChoice::Yes;
    if (str == "no") return governance::VoteChoice::No;
    if (str == "abstain") return governance::VoteChoice::Abstain;
    if (str == "veto" || str == "no_with_veto") return governance::VoteChoice::NoWithVeto;
    return std::nullopt;
}

RPCResponse cmd_getgovernanceinfo(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table) {
    JSONValue::Object result;
    
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    if (!engine) {
        result["enabled"] = false;
        result["votingEnabled"] = false;
        result["message"] = "Governance engine not available";
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    }
    
    result["enabled"] = true;
    result["votingEnabled"] = true;
    result["currentHeight"] = static_cast<int64_t>(engine->GetCurrentHeight());
    result["totalProposals"] = static_cast<int64_t>(engine->GetTotalProposalCount());
    result["activeProposals"] = static_cast<int64_t>(engine->GetActiveProposalCount());
    
    const governance::VotingPowerTracker& tracker = engine->GetVotingPowerTracker();
    result["totalVotingPower"] = static_cast<int64_t>(tracker.GetTotalVotingPower());
    result["voterCount"] = static_cast<int64_t>(tracker.GetVoterCount());
    
    const governance::DelegationRegistry& delegations = engine->GetDelegations();
    result["activeDelegations"] = static_cast<int64_t>(delegations.GetActiveDelegationCount());
    
    // Thresholds
    JSONValue::Object thresholds;
    thresholds["parameterQuorum"] = static_cast<int64_t>(governance::PARAMETER_QUORUM);
    thresholds["parameterApproval"] = static_cast<int64_t>(governance::PARAMETER_APPROVAL_THRESHOLD);
    thresholds["protocolQuorum"] = static_cast<int64_t>(governance::PROTOCOL_QUORUM);
    thresholds["protocolApproval"] = static_cast<int64_t>(governance::PROTOCOL_APPROVAL_THRESHOLD);
    thresholds["constitutionalQuorum"] = static_cast<int64_t>(governance::CONSTITUTIONAL_QUORUM);
    thresholds["constitutionalApproval"] = static_cast<int64_t>(governance::CONSTITUTIONAL_APPROVAL_THRESHOLD);
    result["thresholds"] = JSONValue(std::move(thresholds));
    
    // Voting periods
    JSONValue::Object periods;
    periods["parameterVotingPeriod"] = static_cast<int64_t>(governance::PARAMETER_VOTING_PERIOD);
    periods["protocolVotingPeriod"] = static_cast<int64_t>(governance::PROTOCOL_VOTING_PERIOD);
    periods["constitutionalVotingPeriod"] = static_cast<int64_t>(governance::CONSTITUTIONAL_VOTING_PERIOD);
    result["votingPeriods"] = JSONValue(std::move(periods));
    
    // Constants
    result["minProposalStake"] = FormatAmount(governance::MIN_PROPOSAL_STAKE);
    result["minVotingStake"] = FormatAmount(governance::MIN_VOTING_STAKE);
    result["maxActiveProposalsPerUser"] = static_cast<int64_t>(governance::MAX_ACTIVE_PROPOSALS_PER_USER);
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_listproposals(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    std::string statusFilter = GetOptionalParam<std::string>(req, size_t(0), "active");
    int64_t limit = GetOptionalParam<int64_t>(req, size_t(1), int64_t(50));
    
    JSONValue::Array proposals;
    
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    if (!engine) {
        return RPCResponse::Success(JSONValue(std::move(proposals)), req.GetId());
    }
    
    // Parse status filter
    std::vector<governance::GovernanceProposal> proposalList;
    
    if (statusFilter == "all") {
        // Get all proposals from all statuses
        for (int s = static_cast<int>(governance::GovernanceStatus::Draft);
             s <= static_cast<int>(governance::GovernanceStatus::Expired); ++s) {
            auto status = static_cast<governance::GovernanceStatus>(s);
            auto batch = engine->GetProposalsByStatus(status);
            proposalList.insert(proposalList.end(), batch.begin(), batch.end());
        }
    } else {
        auto status = ParseGovernanceStatusStr(statusFilter);
        if (!status) {
            return InvalidParams("Invalid status filter. Use: draft, pending, active, approved, rejected, executed, all", req.GetId());
        }
        proposalList = engine->GetProposalsByStatus(*status);
    }
    
    // Sort by submit height (most recent first)
    std::sort(proposalList.begin(), proposalList.end(),
              [](const auto& a, const auto& b) {
                  return a.submitHeight > b.submitHeight;
              });
    
    // Apply limit
    if (static_cast<int>(proposalList.size()) > limit) {
        proposalList.resize(static_cast<size_t>(limit));
    }
    
    // Format response
    for (const auto& proposal : proposalList) {
        JSONValue::Object obj;
        obj["proposalId"] = ProposalIdToHex(proposal.id);
        obj["type"] = ProposalTypeStr(proposal.type);
        obj["status"] = GovernanceStatusStr(proposal.status);
        obj["title"] = proposal.title;
        obj["submitHeight"] = static_cast<int64_t>(proposal.submitHeight);
        obj["votingStartHeight"] = static_cast<int64_t>(proposal.votingStartHeight);
        obj["votingEndHeight"] = static_cast<int64_t>(proposal.votingEndHeight);
        obj["deposit"] = FormatAmount(proposal.deposit);
        
        // Vote tallies
        obj["votesYes"] = static_cast<int64_t>(proposal.votesYes);
        obj["votesNo"] = static_cast<int64_t>(proposal.votesNo);
        obj["votesAbstain"] = static_cast<int64_t>(proposal.votesAbstain);
        obj["votesVeto"] = static_cast<int64_t>(proposal.votesNoWithVeto);
        
        // Progress indicators
        obj["approvalPercent"] = proposal.GetApprovalPercent();
        obj["participationPercent"] = proposal.GetParticipationPercent();
        obj["hasQuorum"] = proposal.HasQuorum();
        obj["hasApproval"] = proposal.HasApproval();
        
        proposals.push_back(JSONValue(std::move(obj)));
    }
    
    return RPCResponse::Success(JSONValue(std::move(proposals)), req.GetId());
}

RPCResponse cmd_getproposal(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table) {
    try {
        std::string proposalIdHex = GetRequiredParam<std::string>(req, size_t(0));
        
        governance::GovernanceEngine* engine = table->GetGovernanceEngine();
        if (!engine) {
            return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Governance engine not available", req.GetId());
        }
        
        governance::GovernanceProposalId proposalId = HexToProposalId(proposalIdHex);
        auto proposal = engine->GetProposal(proposalId);
        if (!proposal) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Proposal not found", req.GetId());
        }
        
        JSONValue::Object result;
        result["proposalId"] = ProposalIdToHex(proposal->id);
        result["type"] = ProposalTypeStr(proposal->type);
        result["status"] = GovernanceStatusStr(proposal->status);
        result["title"] = proposal->title;
        result["description"] = proposal->description;
        result["proposer"] = proposal->proposer.ToHex();
        result["discussionUrl"] = proposal->discussionUrl;
        
        // Timing
        result["submitHeight"] = static_cast<int64_t>(proposal->submitHeight);
        result["votingStartHeight"] = static_cast<int64_t>(proposal->votingStartHeight);
        result["votingEndHeight"] = static_cast<int64_t>(proposal->votingEndHeight);
        result["executionHeight"] = static_cast<int64_t>(proposal->executionHeight);
        
        // Finances
        result["deposit"] = FormatAmount(proposal->deposit);
        
        // Vote tallies
        JSONValue::Object votes;
        votes["yes"] = static_cast<int64_t>(proposal->votesYes);
        votes["no"] = static_cast<int64_t>(proposal->votesNo);
        votes["abstain"] = static_cast<int64_t>(proposal->votesAbstain);
        votes["veto"] = static_cast<int64_t>(proposal->votesNoWithVeto);
        votes["total"] = static_cast<int64_t>(proposal->GetTotalVotes());
        result["votes"] = JSONValue(std::move(votes));
        
        // Voting power snapshot
        result["totalVotingPower"] = static_cast<int64_t>(proposal->totalVotingPower);
        
        // Progress indicators
        JSONValue::Object progress;
        progress["approvalPercent"] = proposal->GetApprovalPercent();
        progress["participationPercent"] = proposal->GetParticipationPercent();
        progress["quorumRequired"] = static_cast<int64_t>(proposal->GetQuorumRequirement());
        progress["approvalRequired"] = static_cast<int64_t>(proposal->GetApprovalThreshold());
        progress["hasQuorum"] = proposal->HasQuorum();
        progress["hasApproval"] = proposal->HasApproval();
        progress["isVetoed"] = proposal->IsVetoed();
        result["progress"] = JSONValue(std::move(progress));
        
        // Voting period info
        int currentHeight = engine->GetCurrentHeight();
        result["currentHeight"] = static_cast<int64_t>(currentHeight);
        result["isVotingActive"] = proposal->IsVotingActive(currentHeight);
        result["isReadyForExecution"] = proposal->IsReadyForExecution(currentHeight);
        
        // Type-specific payload
        JSONValue::Object payload;
        if (proposal->type == governance::ProposalType::Parameter) {
            auto* changes = std::get_if<std::vector<governance::ParameterChange>>(&proposal->payload);
            if (changes) {
                JSONValue::Array changesArray;
                for (const auto& change : *changes) {
                    JSONValue::Object changeObj;
                    changeObj["parameter"] = governance::GovernableParameterToString(change.parameter);
                    if (auto* intVal = std::get_if<int64_t>(&change.currentValue)) {
                        changeObj["currentValue"] = *intVal;
                    } else if (auto* strVal = std::get_if<std::string>(&change.currentValue)) {
                        changeObj["currentValue"] = *strVal;
                    }
                    if (auto* intVal = std::get_if<int64_t>(&change.newValue)) {
                        changeObj["newValue"] = *intVal;
                    } else if (auto* strVal = std::get_if<std::string>(&change.newValue)) {
                        changeObj["newValue"] = *strVal;
                    }
                    changesArray.push_back(JSONValue(std::move(changeObj)));
                }
                payload["changes"] = JSONValue(std::move(changesArray));
            }
        } else if (proposal->type == governance::ProposalType::Protocol) {
            auto* upgrade = std::get_if<governance::ProtocolUpgrade>(&proposal->payload);
            if (upgrade) {
                payload["newVersion"] = governance::ProtocolUpgrade::FormatVersion(upgrade->newVersion);
                payload["minClientVersion"] = governance::ProtocolUpgrade::FormatVersion(upgrade->minClientVersion);
                payload["activationHeight"] = static_cast<int64_t>(upgrade->activationHeight);
                payload["deadlineHeight"] = static_cast<int64_t>(upgrade->deadlineHeight);
                payload["codeReference"] = upgrade->codeReference;
                payload["changelogUrl"] = upgrade->changelogUrl;
            }
        } else if (proposal->type == governance::ProposalType::Constitutional) {
            auto* change = std::get_if<governance::ConstitutionalChange>(&proposal->payload);
            if (change) {
                payload["article"] = governance::ConstitutionalArticleToString(change->article);
                payload["currentText"] = change->currentText;
                payload["newText"] = change->newText;
                payload["rationale"] = change->rationale;
            }
        } else if (proposal->type == governance::ProposalType::Signal || 
                   proposal->type == governance::ProposalType::Emergency) {
            auto* text = std::get_if<std::string>(&proposal->payload);
            if (text) {
                payload["text"] = *text;
            }
        }
        result["payload"] = JSONValue(std::move(payload));
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_createproposal(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    if (!engine) {
        return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Governance engine not available", req.GetId());
    }
    
    try {
        std::string typeStr = GetRequiredParam<std::string>(req, size_t(0));
        std::string title = GetRequiredParam<std::string>(req, size_t(1));
        std::string description = GetRequiredParam<std::string>(req, size_t(2));
        Amount deposit = ParseAmount(req.GetParam(size_t(3)));
        
        // Validate deposit
        if (deposit < governance::MIN_PROPOSAL_STAKE) {
            double minDeposit = static_cast<double>(governance::MIN_PROPOSAL_STAKE) / COIN;
            return InvalidParams("Minimum proposal deposit is " + std::to_string(minDeposit) + " NXS", req.GetId());
        }
        
        // Validate proposal type
        auto proposalType = ParseProposalTypeStr(typeStr);
        if (!proposalType) {
            return InvalidParams("Invalid proposal type. Use: parameter, protocol, constitutional, emergency, signal", req.GetId());
        }
        
        // Check wallet balance
        if (wallet->GetBalance().confirmed < deposit) {
            return RPCResponse::Error(ErrorCode::WALLET_INSUFFICIENT_FUNDS, "Insufficient funds for proposal deposit", req.GetId());
        }
        
        // Build proposal
        governance::GovernanceProposal proposal;
        proposal.type = *proposalType;
        proposal.title = title;
        proposal.description = description;
        proposal.deposit = deposit;
        proposal.status = governance::GovernanceStatus::Draft;
        proposal.submitHeight = engine->GetCurrentHeight();
        
        // Get proposer public key from wallet address
        auto addresses = wallet->GetAddresses();
        if (addresses.empty()) {
            return RPCResponse::Error(-4, "No addresses in wallet", req.GetId());
        }
        // Create proposer identity from address
        std::vector<Byte> addressBytes(addresses[0].begin(), addresses[0].end());
        addressBytes.resize(33, 0); // Pad to compressed public key size
        proposal.proposer = PublicKey(addressBytes);
        
        // Optional discussion URL
        std::string discussionUrl = GetOptionalParam<std::string>(req, size_t(4), "");
        proposal.discussionUrl = discussionUrl;
        
        // Set type-specific payload
        if (*proposalType == governance::ProposalType::Signal || 
            *proposalType == governance::ProposalType::Emergency) {
            // Text payload (description is the payload)
            proposal.payload = description;
        } else if (*proposalType == governance::ProposalType::Parameter) {
            // Parameter changes - parse from optional param 5 (JSON array)
            std::vector<governance::ParameterChange> changes;
            // For now, empty changes - full parameter parsing would need JSON array
            proposal.payload = changes;
        } else if (*proposalType == governance::ProposalType::Protocol) {
            // Protocol upgrade - would need additional params
            governance::ProtocolUpgrade upgrade;
            proposal.payload = upgrade;
        } else if (*proposalType == governance::ProposalType::Constitutional) {
            // Constitutional change - would need additional params
            governance::ConstitutionalChange change;
            proposal.payload = change;
        }
        
        // Calculate proposal ID
        proposal.id = proposal.CalculateHash();
        
        // Create signature (in real implementation would sign with wallet key)
        std::vector<Byte> signature(64, 0); // Placeholder signature
        
        // Submit to governance engine
        auto proposalId = engine->SubmitProposal(proposal, signature);
        if (!proposalId) {
            return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Failed to submit proposal", req.GetId());
        }
        
        JSONValue::Object result;
        result["proposalId"] = ProposalIdToHex(*proposalId);
        result["type"] = ProposalTypeStr(proposal.type);
        result["title"] = proposal.title;
        result["deposit"] = FormatAmount(deposit);
        result["status"] = "pending";
        result["submitHeight"] = static_cast<int64_t>(proposal.submitHeight);
        result["votingStartHeight"] = static_cast<int64_t>(proposal.votingStartHeight);
        result["votingEndHeight"] = static_cast<int64_t>(proposal.votingEndHeight);
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_vote(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    if (!engine) {
        return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Governance engine not available", req.GetId());
    }
    
    try {
        std::string proposalIdHex = GetRequiredParam<std::string>(req, size_t(0));
        std::string choiceStr = GetRequiredParam<std::string>(req, size_t(1));
        std::string reason = GetOptionalParam<std::string>(req, size_t(2), "");
        
        // Parse vote choice
        auto voteChoice = ParseVoteChoiceStr(choiceStr);
        if (!voteChoice) {
            return InvalidParams("Invalid vote choice. Use: yes, no, abstain, veto", req.GetId());
        }
        
        // Parse proposal ID
        governance::GovernanceProposalId proposalId = HexToProposalId(proposalIdHex);
        
        // Check proposal exists and is active
        auto proposal = engine->GetProposal(proposalId);
        if (!proposal) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Proposal not found", req.GetId());
        }
        
        if (!proposal->IsVotingActive(engine->GetCurrentHeight())) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Voting is not currently active for this proposal", req.GetId());
        }
        
        // Get voter ID from wallet
        auto addresses = wallet->GetAddresses();
        if (addresses.empty()) {
            return RPCResponse::Error(-4, "No addresses in wallet", req.GetId());
        }
        
        governance::VoterId voterId;
        std::copy_n(reinterpret_cast<const Byte*>(addresses[0].data()),
                   std::min(addresses[0].size(), size_t(20)), voterId.begin());
        
        // Check voting power
        uint64_t votingPower = engine->GetEffectiveVotingPower(voterId, proposal->type);
        if (votingPower < static_cast<uint64_t>(governance::MIN_VOTING_STAKE)) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Insufficient voting power. Minimum stake required.", req.GetId());
        }
        
        // Build vote
        governance::Vote vote;
        vote.proposalId = proposalId;
        vote.voter = voterId;
        vote.choice = *voteChoice;
        vote.votingPower = votingPower;
        vote.voteHeight = engine->GetCurrentHeight();
        vote.reason = reason;
        
        // Cast vote
        bool success = engine->CastVote(vote);
        
        JSONValue::Object result;
        if (success) {
            result["success"] = true;
            result["proposalId"] = proposalIdHex;
            result["voter"] = VoterIdToHex(voterId);
            result["choice"] = VoteChoiceStr(*voteChoice);
            result["votingPower"] = static_cast<int64_t>(votingPower);
            result["voteHeight"] = static_cast<int64_t>(vote.voteHeight);
            
            // Return updated proposal status
            auto updatedProposal = engine->GetProposal(proposalId);
            if (updatedProposal) {
                result["currentApprovalPercent"] = updatedProposal->GetApprovalPercent();
                result["currentParticipationPercent"] = updatedProposal->GetParticipationPercent();
            }
        } else {
            result["success"] = false;
            result["message"] = "Failed to cast vote";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getvoteinfo(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table) {
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    if (!engine) {
        return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Governance engine not available", req.GetId());
    }
    
    try {
        std::string proposalIdHex = GetRequiredParam<std::string>(req, size_t(0));
        std::string voterHex = GetOptionalParam<std::string>(req, size_t(1), "");
        
        governance::GovernanceProposalId proposalId = HexToProposalId(proposalIdHex);
        
        // Check proposal exists
        auto proposal = engine->GetProposal(proposalId);
        if (!proposal) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Proposal not found", req.GetId());
        }
        
        JSONValue::Object result;
        result["proposalId"] = proposalIdHex;
        
        if (!voterHex.empty()) {
            // Get specific voter's vote
            governance::VoterId voterId = HexToVoterId(voterHex);
            result["voter"] = voterHex;
            
            auto vote = engine->GetVote(proposalId, voterId);
            if (vote) {
                result["hasVoted"] = true;
                result["choice"] = VoteChoiceStr(vote->choice);
                result["votingPower"] = static_cast<int64_t>(vote->votingPower);
                result["voteHeight"] = static_cast<int64_t>(vote->voteHeight);
                result["reason"] = vote->reason;
            } else {
                result["hasVoted"] = false;
                result["choice"] = "";
                result["votingPower"] = static_cast<int64_t>(engine->GetVotingPower(voterId));
            }
        } else {
            // Get all votes for proposal
            auto votes = engine->GetVotes(proposalId);
            
            JSONValue::Array votesArray;
            for (const auto& vote : votes) {
                JSONValue::Object voteObj;
                voteObj["voter"] = VoterIdToHex(vote.voter);
                voteObj["choice"] = VoteChoiceStr(vote.choice);
                voteObj["votingPower"] = static_cast<int64_t>(vote.votingPower);
                voteObj["voteHeight"] = static_cast<int64_t>(vote.voteHeight);
                if (!vote.reason.empty()) {
                    voteObj["reason"] = vote.reason;
                }
                votesArray.push_back(JSONValue(std::move(voteObj)));
            }
            
            result["totalVotes"] = static_cast<int64_t>(votes.size());
            result["votes"] = JSONValue(std::move(votesArray));
            
            // Vote breakdown
            JSONValue::Object breakdown;
            breakdown["yes"] = static_cast<int64_t>(proposal->votesYes);
            breakdown["no"] = static_cast<int64_t>(proposal->votesNo);
            breakdown["abstain"] = static_cast<int64_t>(proposal->votesAbstain);
            breakdown["veto"] = static_cast<int64_t>(proposal->votesNoWithVeto);
            result["breakdown"] = JSONValue(std::move(breakdown));
            
            result["approvalPercent"] = proposal->GetApprovalPercent();
            result["participationPercent"] = proposal->GetParticipationPercent();
            result["hasQuorum"] = proposal->HasQuorum();
            result["hasApproval"] = proposal->HasApproval();
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_delegatevote(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    if (!engine) {
        return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Governance engine not available", req.GetId());
    }
    
    try {
        std::string delegateHex = GetRequiredParam<std::string>(req, size_t(0));
        std::string scopeStr = GetOptionalParam<std::string>(req, size_t(1), "");
        int64_t expirationHeight = GetOptionalParam<int64_t>(req, size_t(2), int64_t(0));
        
        // Get keystore and check if unlocked
        auto keystore = wallet->GetKeyStore();
        if (!keystore) {
            return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Keystore not available", req.GetId());
        }
        if (keystore->IsLocked()) {
            return RPCResponse::Error(ErrorCode::WALLET_UNLOCK_NEEDED, "Wallet is locked", req.GetId());
        }
        
        // Get delegator address and key hash from wallet
        auto addresses = wallet->GetAddresses();
        if (addresses.empty()) {
            return RPCResponse::Error(-4, "No addresses in wallet", req.GetId());
        }
        
        // Get key hash from first address
        auto keyHash = AddressToKeyHash(addresses[0]);
        if (!keyHash) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Invalid wallet address", req.GetId());
        }
        
        // Get private key for signing
        auto privKey = keystore->GetKey(*keyHash);
        if (!privKey) {
            return RPCResponse::Error(ErrorCode::WALLET_ERROR, "Key not found in wallet", req.GetId());
        }
        
        governance::VoterId delegatorId;
        std::memcpy(delegatorId.data(), keyHash->data(), std::min(keyHash->size(), delegatorId.size()));
        
        governance::VoterId delegateId = HexToVoterId(delegateHex);
        
        // Check not delegating to self
        if (delegatorId == delegateId) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Cannot delegate to yourself", req.GetId());
        }
        
        // Build delegation
        governance::Delegation delegation;
        delegation.delegator = delegatorId;
        delegation.delegate = delegateId;
        delegation.creationHeight = engine->GetCurrentHeight();
        delegation.expirationHeight = expirationHeight;
        delegation.isActive = true;
        
        // Parse optional scope
        if (!scopeStr.empty()) {
            auto scope = ParseProposalTypeStr(scopeStr);
            if (scope) {
                delegation.scope = *scope;
            }
        }
        
        // Create message to sign: hash of delegation data
        DataStream ss;
        ss.Write(delegatorId.data(), delegatorId.size());
        ss.Write(delegateId.data(), delegateId.size());
        ser_writedata32(ss, static_cast<uint32_t>(delegation.creationHeight));
        ser_writedata32(ss, static_cast<uint32_t>(delegation.expirationHeight));
        ser_writedata8(ss, delegation.scope ? static_cast<uint8_t>(*delegation.scope) : 0xFF);
        
        Hash256 delegationHash = SHA256Hash(ss.data(), ss.size());
        
        // Sign the delegation hash
        auto signatureVec = privKey->SignCompact(delegationHash);
        if (signatureVec.empty()) {
            return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Failed to sign delegation", req.GetId());
        }
        
        // Convert to std::vector<Byte>
        std::vector<Byte> signature(signatureVec.begin(), signatureVec.end());
        
        // Register delegation
        bool success = engine->Delegate(delegation, signature);
        
        JSONValue::Object result;
        if (success) {
            result["success"] = true;
            result["delegator"] = VoterIdToHex(delegatorId);
            result["delegate"] = delegateHex;
            result["creationHeight"] = static_cast<int64_t>(delegation.creationHeight);
            if (expirationHeight > 0) {
                result["expirationHeight"] = static_cast<int64_t>(expirationHeight);
            }
            if (delegation.scope) {
                result["scope"] = ProposalTypeStr(*delegation.scope);
            } else {
                result["scope"] = "all";
            }
            result["signature"] = FormatHex(signature.data(), signature.size());
            result["message"] = "Voting power delegated successfully";
        } else {
            result["success"] = false;
            result["message"] = "Failed to delegate voting power";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_undelegatevote(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    wallet::Wallet* wallet = table->GetWallet();
    if (!wallet) {
        return RPCResponse::Error(ErrorCode::WALLET_NOT_FOUND, "Wallet not loaded", req.GetId());
    }
    
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    if (!engine) {
        return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Governance engine not available", req.GetId());
    }
    
    try {
        // Get keystore and check if unlocked
        auto keystore = wallet->GetKeyStore();
        if (!keystore) {
            return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Keystore not available", req.GetId());
        }
        if (keystore->IsLocked()) {
            return RPCResponse::Error(ErrorCode::WALLET_UNLOCK_NEEDED, "Wallet is locked", req.GetId());
        }
        
        // Get delegator address from wallet
        auto addresses = wallet->GetAddresses();
        if (addresses.empty()) {
            return RPCResponse::Error(-4, "No addresses in wallet", req.GetId());
        }
        
        // Get key hash from first address
        auto keyHash = AddressToKeyHash(addresses[0]);
        if (!keyHash) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "Invalid wallet address", req.GetId());
        }
        
        // Get private key for signing
        auto privKey = keystore->GetKey(*keyHash);
        if (!privKey) {
            return RPCResponse::Error(ErrorCode::WALLET_ERROR, "Key not found in wallet", req.GetId());
        }
        
        governance::VoterId delegatorId;
        std::memcpy(delegatorId.data(), keyHash->data(), std::min(keyHash->size(), delegatorId.size()));
        
        // Check existing delegation
        const governance::DelegationRegistry& delegations = engine->GetDelegations();
        auto existingDelegation = delegations.GetDelegation(delegatorId);
        if (!existingDelegation) {
            return RPCResponse::Error(ErrorCode::INVALID_PARAMS, "No active delegation found", req.GetId());
        }
        
        // Create message to sign: hash of revocation data
        DataStream ss;
        ss.Write(delegatorId.data(), delegatorId.size());
        ss.Write(existingDelegation->delegate.data(), existingDelegation->delegate.size());
        ser_writedata32(ss, static_cast<uint32_t>(engine->GetCurrentHeight()));  // Revocation height
        std::string action = "REVOKE_DELEGATION";
        ss.Write(reinterpret_cast<const uint8_t*>(action.data()), action.size());
        
        Hash256 revocationHash = SHA256Hash(ss.data(), ss.size());
        
        // Sign the revocation hash
        auto signatureVec = privKey->SignCompact(revocationHash);
        if (signatureVec.empty()) {
            return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, "Failed to sign revocation", req.GetId());
        }
        
        // Convert to std::vector<Byte>
        std::vector<Byte> signature(signatureVec.begin(), signatureVec.end());
        
        // Revoke delegation
        bool success = engine->RevokeDelegation(delegatorId, signature);
        
        JSONValue::Object result;
        if (success) {
            result["success"] = true;
            result["delegator"] = VoterIdToHex(delegatorId);
            result["previousDelegate"] = VoterIdToHex(existingDelegation->delegate);
            result["revokedAtHeight"] = static_cast<int64_t>(engine->GetCurrentHeight());
            result["signature"] = FormatHex(signature.data(), signature.size());
            result["message"] = "Delegation revoked successfully";
        } else {
            result["success"] = false;
            result["message"] = "Failed to revoke delegation";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getparameter(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    
    try {
        std::string name = GetRequiredParam<std::string>(req, size_t(0));
        
        JSONValue::Object result;
        result["name"] = name;
        
        // Try to parse as governable parameter
        auto param = governance::ParseGovernableParameter(name);
        
        if (param && engine) {
            // Get from parameter registry
            governance::ParameterValue value = engine->GetParameter(*param);
            
            if (auto* intVal = std::get_if<int64_t>(&value)) {
                result["value"] = *intVal;
                result["type"] = "integer";
            } else if (auto* strVal = std::get_if<std::string>(&value)) {
                result["value"] = *strVal;
                result["type"] = "string";
            }
            
            result["description"] = governance::GovernableParameterToString(*param);
            result["modifiable"] = true;
            
            // Get bounds if applicable
            auto minVal = governance::GetParameterMin(*param);
            auto maxVal = governance::GetParameterMax(*param);
            if (minVal) {
                result["minValue"] = *minVal;
            }
            if (maxVal) {
                result["maxValue"] = *maxVal;
            }
        } else {
            // Fallback to hardcoded defaults for common parameters
            result["modifiable"] = true;
            
            if (name == "min_transaction_fee" || name == "MinTransactionFee") {
                result["value"] = FormatAmount(1000);
                result["type"] = "amount";
                result["description"] = "Minimum transaction fee";
            } else if (name == "block_size_limit" || name == "BlockSizeLimit") {
                result["value"] = int64_t(4000000);
                result["type"] = "integer";
                result["description"] = "Maximum block size in bytes";
            } else if (name == "min_validator_stake") {
                result["value"] = FormatAmount(staking::MIN_VALIDATOR_STAKE);
                result["type"] = "amount";
                result["description"] = "Minimum stake to become a validator";
            } else if (name == "min_delegation_stake") {
                result["value"] = FormatAmount(staking::MIN_DELEGATION_STAKE);
                result["type"] = "amount";
                result["description"] = "Minimum delegation amount";
            } else if (name == "min_proposal_stake") {
                result["value"] = FormatAmount(governance::MIN_PROPOSAL_STAKE);
                result["type"] = "amount";
                result["description"] = "Minimum proposal deposit";
            } else if (name == "min_voting_stake") {
                result["value"] = FormatAmount(governance::MIN_VOTING_STAKE);
                result["type"] = "amount";
                result["description"] = "Minimum stake to vote";
            } else {
                result["value"] = JSONValue(nullptr);
                result["type"] = "unknown";
                result["description"] = "Parameter not found";
                result["modifiable"] = false;
            }
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_listparameters(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    governance::GovernanceEngine* engine = table->GetGovernanceEngine();
    
    JSONValue::Array parameters;
    
    // Helper to add parameters
    auto addParam = [&](const std::string& name, const JSONValue& value, 
                       const std::string& type, const std::string& desc,
                       bool modifiable = true) {
        JSONValue::Object param;
        param["name"] = name;
        param["value"] = value;
        param["type"] = type;
        param["description"] = desc;
        param["modifiable"] = modifiable;
        parameters.push_back(JSONValue(std::move(param)));
    };
    
    // Add parameters from ParameterRegistry if available
    if (engine) {
        const governance::ParameterRegistry& registry = engine->GetParameters();
        auto allParams = registry.GetAllParameters();
        
        for (const auto& [param, value] : allParams) {
            std::string name = governance::GovernableParameterToString(param);
            if (auto* intVal = std::get_if<int64_t>(&value)) {
                addParam(name, static_cast<int64_t>(*intVal), "integer", name);
            } else if (auto* strVal = std::get_if<std::string>(&value)) {
                addParam(name, *strVal, "string", name);
            }
        }
    }
    
    // Add staking parameters
    addParam("min_validator_stake", FormatAmount(staking::MIN_VALIDATOR_STAKE), "amount", 
             "Minimum stake to become a validator");
    addParam("min_delegation_stake", FormatAmount(staking::MIN_DELEGATION_STAKE), "amount", 
             "Minimum delegation amount");
    addParam("unbonding_period", static_cast<int64_t>(staking::UNBONDING_PERIOD), "integer", 
             "Unbonding period in blocks");
    addParam("max_validators", static_cast<int64_t>(staking::MAX_ACTIVE_VALIDATORS), "integer", 
             "Maximum active validators");
    
    // Add governance parameters
    addParam("min_proposal_stake", FormatAmount(governance::MIN_PROPOSAL_STAKE), "amount", 
             "Minimum proposal deposit");
    addParam("min_voting_stake", FormatAmount(governance::MIN_VOTING_STAKE), "amount", 
             "Minimum stake to vote");
    addParam("parameter_voting_period", static_cast<int64_t>(governance::PARAMETER_VOTING_PERIOD), "integer", 
             "Voting period for parameter changes (blocks)");
    addParam("protocol_voting_period", static_cast<int64_t>(governance::PROTOCOL_VOTING_PERIOD), "integer", 
             "Voting period for protocol upgrades (blocks)");
    addParam("constitutional_voting_period", static_cast<int64_t>(governance::CONSTITUTIONAL_VOTING_PERIOD), "integer", 
             "Voting period for constitutional changes (blocks)");
    
    return RPCResponse::Success(JSONValue(std::move(parameters)), req.GetId());
}

// ============================================================================
// Mining/PoUW Command Implementations
// ============================================================================

RPCResponse cmd_getmininginfo(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    JSONValue::Object result;
    
    // Default values
    result["blocks"] = int64_t(0);
    result["currentblocksize"] = int64_t(0);
    result["currentblockweight"] = int64_t(0);
    result["currentblocktx"] = int64_t(0);
    result["difficulty"] = 1.0;
    result["networkhashps"] = int64_t(0);
    result["pooledtx"] = int64_t(0);
    result["chain"] = "main";
    result["warnings"] = "";
    
    // Get real chain data if available
    ChainState* chainState = table->GetChainState();
    if (chainState) {
        BlockIndex* tip = chainState->GetTip();
        if (tip) {
            result["blocks"] = static_cast<int64_t>(tip->nHeight);
            result["difficulty"] = GetDifficultyFromBits(tip->nBits);
        }
    }
    
    // Get mempool data if available
    Mempool* mempool = table->GetMempool();
    if (mempool) {
        result["pooledtx"] = static_cast<int64_t>(mempool->Size());
    }
    
    // PoUW-specific fields (SHURIUM unique feature)
    result["pouw_enabled"] = true;
    result["active_problems"] = int64_t(0);
    result["solved_problems"] = int64_t(0);
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_getblocktemplate(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table) {
    // Check required components
    ChainState* chainState = table->GetChainState();
    if (!chainState) {
        return RPCError(-1, "Chain state not available", req.GetId());
    }
    
    Mempool* mempool = table->GetMempool();
    if (!mempool) {
        return RPCError(-1, "Mempool not available", req.GetId());
    }
    
    // Get consensus params (use mainnet as default)
    consensus::Params params = consensus::Params::Main();
    
    // Create block assembler and generate template
    miner::BlockAssemblerOptions options;
    miner::BlockAssembler assembler(*chainState, *mempool, params, options);
    
    // Create a default coinbase script (OP_TRUE for simplicity in mining pool)
    // In production, miner would provide their own script
    Script coinbaseScript;
    coinbaseScript.push_back(OP_TRUE);
    
    miner::BlockTemplate blockTemplate = assembler.CreateNewBlock(coinbaseScript);
    
    if (!blockTemplate.isValid) {
        return RPCError(-1, "Failed to create block template: " + blockTemplate.error, req.GetId());
    }
    
    // Build response in BIP22/BIP23 format
    JSONValue::Object result;
    
    result["version"] = static_cast<int64_t>(blockTemplate.block.nVersion);
    result["previousblockhash"] = BlockHashToHex(blockTemplate.block.hashPrevBlock);
    result["curtime"] = static_cast<int64_t>(blockTemplate.curTime);
    result["mintime"] = static_cast<int64_t>(blockTemplate.minTime);
    result["height"] = static_cast<int64_t>(blockTemplate.height);
    
    // Format nBits as hex string (8 hex chars for 32-bit value)
    std::stringstream bitsHex;
    bitsHex << std::hex << std::setfill('0') << std::setw(8) << blockTemplate.nBits;
    result["bits"] = bitsHex.str();
    
    result["target"] = miner::TargetToHex(blockTemplate.target);
    result["coinbasevalue"] = static_cast<int64_t>(blockTemplate.coinbaseValue);
    
    // Add transactions (excluding coinbase)
    JSONValue::Array txArray;
    for (size_t i = 1; i < blockTemplate.txInfo.size(); ++i) {
        const auto& txInfo = blockTemplate.txInfo[i];
        JSONValue::Object txObj;
        
        // Serialize transaction to hex
        DataStream ss;
        Serialize(ss, *txInfo.tx);
        txObj["data"] = FormatHex(ss.data(), ss.size());
        txObj["txid"] = HashToHex(txInfo.tx->GetHash());
        txObj["fee"] = static_cast<int64_t>(txInfo.fee);
        txObj["sigops"] = static_cast<int64_t>(txInfo.sigops);
        
        txArray.push_back(JSONValue(std::move(txObj)));
    }
    result["transactions"] = JSONValue(std::move(txArray));
    
    // Coinbase auxiliary data
    if (!blockTemplate.txInfo.empty()) {
        const auto& coinbaseTx = blockTemplate.txInfo[0].tx;
        DataStream ss;
        Serialize(ss, *coinbaseTx);
        result["coinbasetxn"] = FormatHex(ss.data(), ss.size());
    }
    
    // Mutable fields that miners can modify
    JSONValue::Array mutable_fields;
    mutable_fields.push_back(JSONValue("time"));
    mutable_fields.push_back(JSONValue("transactions"));
    mutable_fields.push_back(JSONValue("prevblock"));
    result["mutable"] = JSONValue(std::move(mutable_fields));
    
    // Capabilities
    JSONValue::Array capabilities;
    capabilities.push_back(JSONValue("proposal"));
    result["capabilities"] = JSONValue(std::move(capabilities));
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_submitblock(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table) {
    try {
        std::string hexdata = GetRequiredParam<std::string>(req, size_t(0));
        
        // Decode the block from hex
        auto blockOpt = miner::HexToBlock(hexdata);
        if (!blockOpt) {
            return RPCError(-22, "Block decode failed", req.GetId());
        }
        Block& block = *blockOpt;
        
        // Get chain state manager for full block processing
        ChainStateManager* chainManager = table->GetChainStateManager();
        if (!chainManager) {
            // Fall back to basic validation if no manager available
            ChainState* chainState = table->GetChainState();
            if (!chainState) {
                return RPCError(-1, "Chain state not available", req.GetId());
            }
            
            // Get consensus params for validation
            consensus::Params params = consensus::Params::Main();
            
            // Perform basic block validation only
            consensus::ValidationState state;
            if (!consensus::CheckBlock(block, state, params)) {
                std::string reason = state.GetRejectReason();
                if (!state.GetDebugMessage().empty()) {
                    reason += ": " + state.GetDebugMessage();
                }
                return RPCError(-25, "Block validation failed: " + reason, req.GetId());
            }
            
            // Without ChainStateManager, we can only validate, not connect
            return RPCError(-1, "Block validated but ChainStateManager not available for connection", req.GetId());
        }
        
        // Check for duplicate BEFORE processing
        BlockHash blockHash = block.GetHash();
        BlockIndex* existingIndex = chainManager->LookupBlockIndex(blockHash);
        if (existingIndex && existingIndex->IsValid(BlockStatus::VALID_TRANSACTIONS)) {
            // Block already exists and has been validated - it's a duplicate
            return RPCError(-27, "duplicate", req.GetId());
        }
        
        // Check if parent exists (to give better error for orphans)
        BlockIndex* parentIndex = chainManager->LookupBlockIndex(block.hashPrevBlock);
        if (!parentIndex) {
            return RPCError(-25, "Block's parent not found (orphan)", req.GetId());
        }
        
        // Use ChainStateManager for full block processing
        // ProcessNewBlock handles:
        // - Block header validation
        // - Block body validation (CheckBlock)
        // - BlockIndex creation
        // - Chain tip comparison and potential reorg
        // - UTXO set updates
        // - Activating the best chain
        bool accepted = chainManager->ProcessNewBlock(block);
        
        if (!accepted) {
            // Block was rejected after validation
            return RPCError(-25, "Block rejected", req.GetId());
        }
        
        // Block was accepted and connected
        // Relay to network peers
        MessageProcessor* msgproc = table->GetMessageProcessor();
        if (msgproc) {
            msgproc->RelayBlock(block.GetHash());
        }
        
        // Return null on success per BIP22
        return RPCResponse::Success(JSONValue(nullptr), req.GetId());
        
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_getwork(const RPCRequest& req, const RPCContext& ctx,
                        RPCCommandTable* table) {
    JSONValue::Object result;
    
    result["problemId"] = "0000000000000000000000000000000000000000000000000000000000000000";
    result["problemType"] = "optimization";
    result["difficulty"] = 1.0;
    result["data"] = "";
    result["target"] = "0000000000000000000000000000000000000000000000000000000000000000";
    result["expires"] = GetTime() + 600;
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_submitwork(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table) {
    try {
        std::string problemIdStr = GetRequiredParam<std::string>(req, size_t(0));
        std::string solutionHex = GetRequiredParam<std::string>(req, size_t(1));
        std::string solverAddress = GetOptionalParam<std::string>(req, size_t(2), "");
        
        // Access the marketplace
        auto& marketplace = marketplace::Marketplace::Instance();
        
        // Try to find the problem - first by numeric ID, then by hash
        const marketplace::Problem* problem = nullptr;
        uint64_t problemId = 0;
        
        // Try to parse as numeric ID
        try {
            problemId = std::stoull(problemIdStr);
            problem = marketplace.GetProblem(problemId);
        } catch (...) {
            // Not a numeric ID, try as hash
        }
        
        // If not found by ID, try by hash
        if (!problem && problemIdStr.length() == 64) {
            // Parse hex hash
            ProblemHash hash;
            for (size_t i = 0; i < 32 && i * 2 + 1 < problemIdStr.length(); ++i) {
                std::string byte = problemIdStr.substr(i * 2, 2);
                hash[i] = static_cast<uint8_t>(std::stoul(byte, nullptr, 16));
            }
            problem = marketplace.GetProblemByHash(hash);
            if (problem) {
                problemId = problem->GetId();
            }
        }
        
        JSONValue::Object result;
        
        // If problem not found, return accepted=false (not an error)
        if (!problem) {
            result["accepted"] = false;
            result["message"] = "Problem not found";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        // Check if problem is expired
        if (problem->IsExpired()) {
            result["accepted"] = false;
            result["message"] = "Problem has expired";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        // Check if problem is already solved
        if (problem->IsSolved()) {
            result["accepted"] = false;
            result["message"] = "Problem already solved";
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        // Decode solution from hex
        std::vector<uint8_t> solutionData;
        for (size_t i = 0; i + 1 < solutionHex.length(); i += 2) {
            std::string byte = solutionHex.substr(i, 2);
            try {
                solutionData.push_back(static_cast<uint8_t>(std::stoul(byte, nullptr, 16)));
            } catch (...) {
                // Non-hex data, use raw bytes
                solutionData.clear();
                solutionData.insert(solutionData.end(), solutionHex.begin(), solutionHex.end());
                break;
            }
        }
        
        // Create solution object
        marketplace::Solution solution(problemId);
        solution.SetProblemHash(problem->GetHash());
        solution.SetSubmissionTime(GetTime());
        
        // Set solver address (use wallet address if not specified)
        if (solverAddress.empty() && table->GetWallet()) {
            auto addresses = table->GetWallet()->GetAddresses();
            if (!addresses.empty()) {
                solverAddress = addresses[0];  // Use first wallet address
            }
        }
        solution.SetSolver(solverAddress);
        
        // Set solution data
        marketplace::SolutionData solData;
        solData.SetResult(std::move(solutionData));
        solData.ComputeResultHash();
        solution.SetData(std::move(solData));
        solution.ComputeHash();
        
        // Submit solution to marketplace
        auto solutionId = marketplace.SubmitSolution(std::move(solution));
        
        if (solutionId != marketplace::Solution::INVALID_ID) {
            result["accepted"] = true;
            result["solution_id"] = std::to_string(solutionId);
            result["problem_id"] = std::to_string(problemId);
            result["status"] = "pending_verification";
            result["message"] = "Solution submitted for verification";
            
            // Get the submitted solution for status
            const auto* submitted = marketplace.GetSolution(solutionId);
            if (submitted) {
                result["solver"] = submitted->GetSolver();
            }
        } else {
            result["accepted"] = false;
            result["message"] = "Solution submission failed";
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_listproblems(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table) {
    std::string statusFilter = GetOptionalParam<std::string>(req, size_t(0), "pending");
    std::string typeFilter = GetOptionalParam<std::string>(req, size_t(1), "");
    int64_t maxCount = GetOptionalParam<int64_t>(req, size_t(2), 100);
    
    // Limit max count
    if (maxCount <= 0) maxCount = 100;
    if (maxCount > 1000) maxCount = 1000;
    
    JSONValue::Array problems;
    
    // Access the marketplace
    auto& marketplace = marketplace::Marketplace::Instance();
    
    // Get problems based on filter
    std::vector<const marketplace::Problem*> problemList;
    
    if (statusFilter == "pending" || statusFilter == "all") {
        // Get pending problems
        auto pending = marketplace.GetPendingProblems(static_cast<size_t>(maxCount));
        problemList.insert(problemList.end(), pending.begin(), pending.end());
    }
    
    // Filter by type if specified
    if (!typeFilter.empty()) {
        auto requestedType = marketplace::ProblemTypeFromString(typeFilter);
        if (!requestedType) {
            return InvalidParams("Invalid problem type: " + typeFilter, req.GetId());
        }
        
        // Filter the list
        auto it = std::remove_if(problemList.begin(), problemList.end(),
            [requestedType](const marketplace::Problem* p) {
                return p->GetType() != *requestedType;
            });
        problemList.erase(it, problemList.end());
    }
    
    // Convert to JSON
    for (const auto* problem : problemList) {
        if (!problem) continue;
        
        JSONValue::Object obj;
        obj["id"] = std::to_string(problem->GetId());
        obj["hash"] = FormatHex(problem->GetHash().data(), problem->GetHash().size());
        obj["type"] = marketplace::ProblemTypeToString(problem->GetType());
        obj["status"] = problem->IsSolved() ? "solved" : 
                       (problem->IsExpired() ? "expired" : "pending");
        obj["creator"] = problem->GetCreator();
        obj["reward"] = FormatAmount(problem->GetReward());
        obj["reward_raw"] = static_cast<int64_t>(problem->GetReward());
        obj["bonus_reward"] = FormatAmount(problem->GetBonusReward());
        obj["created_at"] = problem->GetCreationTime();
        obj["deadline"] = problem->GetDeadline();
        obj["expires_in"] = std::max<int64_t>(0, problem->GetDeadline() - GetTime());
        
        if (problem->IsSolved()) {
            obj["solver"] = problem->GetSolver();
        }
        
        // Add difficulty info
        const auto& diff = problem->GetDifficulty();
        JSONValue::Object diffObj;
        diffObj["target"] = static_cast<int64_t>(diff.target);
        diffObj["estimated_time"] = static_cast<int64_t>(diff.estimatedTime);
        diffObj["min_memory"] = static_cast<int64_t>(diff.minMemory);
        diffObj["operations"] = static_cast<int64_t>(diff.operations);
        obj["difficulty"] = JSONValue(std::move(diffObj));
        
        problems.push_back(JSONValue(std::move(obj)));
    }
    
    return RPCResponse::Success(JSONValue(std::move(problems)), req.GetId());
}

RPCResponse cmd_getproblem(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table) {
    try {
        std::string problemIdStr = GetRequiredParam<std::string>(req, size_t(0));
        
        // Access the marketplace
        auto& marketplace = marketplace::Marketplace::Instance();
        
        // Try to find the problem - first by numeric ID, then by hash
        const marketplace::Problem* problem = nullptr;
        
        // Try to parse as numeric ID
        try {
            uint64_t problemId = std::stoull(problemIdStr);
            problem = marketplace.GetProblem(problemId);
        } catch (...) {
            // Not a numeric ID, try as hash
        }
        
        // If not found by ID, try by hash
        if (!problem && problemIdStr.length() == 64) {
            // Parse hex hash
            ProblemHash hash;
            for (size_t i = 0; i < 32 && i * 2 + 1 < problemIdStr.length(); ++i) {
                std::string byte = problemIdStr.substr(i * 2, 2);
                hash[i] = static_cast<uint8_t>(std::stoul(byte, nullptr, 16));
            }
            problem = marketplace.GetProblemByHash(hash);
        }
        
        JSONValue::Object result;
        
        // If problem not found, return stub response for compatibility
        if (!problem) {
            result["problemId"] = problemIdStr;
            result["type"] = "unknown";
            result["status"] = "not_found";
            result["difficulty"] = 0.0;
            result["reward"] = FormatAmount(0);
            result["created_at"] = int64_t(0);
            result["deadline"] = int64_t(0);
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
        
        result["id"] = std::to_string(problem->GetId());
        result["problemId"] = std::to_string(problem->GetId());  // Backward compatibility
        result["hash"] = FormatHex(problem->GetHash().data(), problem->GetHash().size());
        result["type"] = marketplace::ProblemTypeToString(problem->GetType());
        result["status"] = problem->IsSolved() ? "solved" : 
                          (problem->IsExpired() ? "expired" : "pending");
        result["creator"] = problem->GetCreator();
        result["reward"] = FormatAmount(problem->GetReward());
        result["reward_raw"] = static_cast<int64_t>(problem->GetReward());
        result["bonus_reward"] = FormatAmount(problem->GetBonusReward());
        result["created_at"] = problem->GetCreationTime();
        result["deadline"] = problem->GetDeadline();
        result["expires_in"] = std::max<int64_t>(0, problem->GetDeadline() - GetTime());
        
        if (problem->IsSolved()) {
            result["solver"] = problem->GetSolver();
        }
        
        // Add difficulty info
        const auto& diff = problem->GetDifficulty();
        JSONValue::Object diffObj;
        diffObj["target"] = static_cast<int64_t>(diff.target);
        diffObj["estimated_time"] = static_cast<int64_t>(diff.estimatedTime);
        diffObj["min_memory"] = static_cast<int64_t>(diff.minMemory);
        diffObj["operations"] = static_cast<int64_t>(diff.operations);
        result["difficulty"] = JSONValue(std::move(diffObj));
        
        // Add specification data
        const auto& spec = problem->GetSpec();
        JSONValue::Object specObj;
        specObj["type"] = marketplace::ProblemTypeToString(spec.GetType());
        specObj["version"] = static_cast<int64_t>(spec.GetVersion());
        specObj["description"] = spec.GetDescription();
        specObj["input_size"] = static_cast<int64_t>(spec.GetInputData().size());
        specObj["verification_size"] = static_cast<int64_t>(spec.GetVerificationData().size());
        specObj["parameters"] = spec.GetParameters();
        result["specification"] = JSONValue(std::move(specObj));
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

/**
 * Generate blocks to a specified address (regtest only).
 * This is used for testing and development.
 * 
 * @param nblocks Number of blocks to generate
 * @param address Address to send coinbase rewards to
 * @return Array of block hashes generated
 */
RPCResponse cmd_generatetoaddress(const RPCRequest& req, const RPCContext& ctx,
                                   RPCCommandTable* table) {
    try {
        // Get parameters
        int64_t nblocks = GetRequiredParam<int64_t>(req, size_t(0));
        std::string address = GetRequiredParam<std::string>(req, size_t(1));
        
        if (nblocks < 1 || nblocks > 10000) {
            return RPCError(-8, "Invalid nblocks value (must be 1-10000)", req.GetId());
        }
        
        // Validate address and create coinbase script
        Script coinbaseScript;
        
        // Check if it's a bech32 address (starts with shr1 or tshr1)
        if (address.substr(0, 4) == "shr1" || address.substr(0, 5) == "tshr1") {
            // Decode bech32 address
            auto decoded = DecodeBech32(address);
            if (!decoded) {
                return RPCError(-5, "Invalid bech32 address", req.GetId());
            }
            auto [hrp, version, program] = *decoded;
            
            // Create witness script (P2WPKH or P2WSH)
            coinbaseScript.push_back(version == 0 ? OP_0 : static_cast<uint8_t>(OP_1 + version - 1));
            coinbaseScript.push_back(static_cast<uint8_t>(program.size()));
            coinbaseScript.insert(coinbaseScript.end(), program.begin(), program.end());
        } else {
            // Assume base58 P2PKH address - create P2PKH script
            // For simplicity, just use OP_TRUE for now (miner gets the coins)
            coinbaseScript.push_back(OP_TRUE);
        }
        
        // Get required components
        ChainStateManager* chainManager = table->GetChainStateManager();
        if (!chainManager) {
            return RPCError(-1, "Chain state manager not available", req.GetId());
        }
        
        ChainState* chainState = table->GetChainState();
        if (!chainState) {
            return RPCError(-1, "Chain state not available", req.GetId());
        }
        
        Mempool* mempool = table->GetMempool();
        if (!mempool) {
            return RPCError(-1, "Mempool not available", req.GetId());
        }
        
        // Use regtest params (lowest difficulty)
        consensus::Params params = consensus::Params::RegTest();
        
        JSONValue::Array blockHashes;
        
        for (int64_t i = 0; i < nblocks; ++i) {
            // Create block template
            miner::BlockAssemblerOptions options;
            miner::BlockAssembler assembler(*chainState, *mempool, params, options);
            miner::BlockTemplate blockTemplate = assembler.CreateNewBlock(coinbaseScript);
            
            if (!blockTemplate.isValid) {
                return RPCError(-1, "Failed to create block template: " + blockTemplate.error, req.GetId());
            }
            
            Block& block = blockTemplate.block;
            
            // Mine the block (simple CPU mining for regtest)
            // Regtest has very low difficulty, so this should be fast
            uint32_t maxNonce = std::numeric_limits<uint32_t>::max();
            bool found = false;
            
            for (uint32_t nonce = 0; nonce < maxNonce && !found; ++nonce) {
                block.nNonce = nonce;
                BlockHash hash = block.GetHash();
                
                // Check if hash meets target using miner utility
                // For regtest, difficulty is 1, so almost any hash works
                if (miner::Miner::MeetsTarget(hash, blockTemplate.target)) {
                    found = true;
                }
                
                // Update time occasionally to keep block valid
                if (nonce % 1000000 == 0) {
                    block.nTime = std::max(block.nTime, static_cast<uint32_t>(GetTime()));
                }
            }
            
            if (!found) {
                return RPCError(-1, "Failed to mine block (nonce exhausted)", req.GetId());
            }
            
            // Submit the block
            bool accepted = chainManager->ProcessNewBlock(block);
            if (!accepted) {
                return RPCError(-1, "Block rejected by chain", req.GetId());
            }
            
            // Remove confirmed transactions from mempool
            if (mempool) {
                mempool->RemoveForBlock(block.vtx);
            }
            
            // Notify wallet about the new block so it can track coinbase outputs
            wallet::Wallet* wallet = table->GetWallet();
            if (wallet) {
                wallet->ProcessBlock(block, blockTemplate.height);
            }
            
            // Add to result array
            blockHashes.push_back(JSONValue(BlockHashToHex(block.GetHash())));
        }
        
        return RPCResponse::Success(JSONValue(std::move(blockHashes)), req.GetId());
        
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}


// ============================================================================
// Utility Command Implementations
// ============================================================================

RPCResponse cmd_help(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table) {
    std::string command = GetOptionalParam<std::string>(req, size_t(0), "");
    
    if (command.empty()) {
        // List all commands by category
        JSONValue::Object result;
        
        auto commands = table->GetAllCommands();
        std::map<std::string, JSONValue::Array> byCategory;
        
        for (const auto& cmd : commands) {
            JSONValue::Object cmdInfo;
            cmdInfo["name"] = cmd.name;
            cmdInfo["description"] = cmd.description;
            byCategory[cmd.category].push_back(JSONValue(std::move(cmdInfo)));
        }
        
        for (auto& [category, cmds] : byCategory) {
            result[category] = JSONValue(std::move(cmds));
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    }
    
    // Get help for specific command
    auto commands = table->GetAllCommands();
    for (const auto& cmd : commands) {
        if (cmd.name == command) {
            JSONValue::Object result;
            result["name"] = cmd.name;
            result["category"] = cmd.category;
            result["description"] = cmd.description;
            result["requiresAuth"] = cmd.requiresAuth;
            result["requiresWallet"] = cmd.requiresWallet;
            
            JSONValue::Array args;
            for (size_t i = 0; i < cmd.argNames.size(); ++i) {
                JSONValue::Object arg;
                arg["name"] = cmd.argNames[i];
                if (i < cmd.argDescriptions.size()) {
                    arg["description"] = cmd.argDescriptions[i];
                }
                args.push_back(JSONValue(std::move(arg)));
            }
            result["arguments"] = JSONValue(std::move(args));
            
            return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
        }
    }
    
    return RPCResponse::Error(ErrorCode::METHOD_NOT_FOUND, 
                             "Unknown command: " + command, req.GetId());
}

RPCResponse cmd_stop(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table) {
    // Signal server shutdown
    RequestShutdown();
    return RPCResponse::Success(JSONValue("SHURIUM server stopping"), req.GetId());
}

RPCResponse cmd_uptime(const RPCRequest& req, const RPCContext& ctx,
                       RPCCommandTable* table) {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - g_startTime).count();
    
    return RPCResponse::Success(JSONValue(static_cast<int64_t>(uptime)), req.GetId());
}

RPCResponse cmd_getmemoryinfo(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table) {
    JSONValue::Object result;
    
    // Basic memory info (platform-specific implementation would be needed for actual values)
    JSONValue::Object locked;
    locked["used"] = int64_t(0);
    locked["free"] = int64_t(0);
    locked["total"] = int64_t(0);
    locked["locked"] = int64_t(0);
    locked["chunks_used"] = int64_t(0);
    locked["chunks_free"] = int64_t(0);
    result["locked"] = JSONValue(std::move(locked));
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_logging(const RPCRequest& req, const RPCContext& ctx,
                        RPCCommandTable* table) {
    util::Logger& logger = util::Logger::Instance();
    
    // List of all known categories
    static const std::vector<std::string> allCategories = {
        util::LogCategory::DEFAULT,
        util::LogCategory::NET,
        util::LogCategory::MEMPOOL,
        util::LogCategory::VALIDATION,
        util::LogCategory::WALLET,
        util::LogCategory::RPC,
        util::LogCategory::CONSENSUS,
        util::LogCategory::MINING,
        util::LogCategory::IDENTITY,
        util::LogCategory::UBI,
        util::LogCategory::DB,
        util::LogCategory::LOCK,
        util::LogCategory::BENCH
    };
    
    // If parameters provided, update logging configuration
    if (req.HasParam(size_t(0)) || req.HasParam("include")) {
        // Get categories to include
        if (req.HasParam(size_t(0))) {
            const auto& includeParam = req.GetParams()[0];
            if (includeParam.IsArray()) {
                const auto& arr = includeParam.GetArray();
                for (const auto& item : arr) {
                    if (item.IsString()) {
                        std::string cat = item.GetString();
                        if (cat == "all" || cat == "1") {
                            logger.EnableAllCategories();
                        } else if (cat == "none" || cat == "0") {
                            logger.DisableAllCategories();
                        } else {
                            logger.EnableCategory(cat);
                        }
                    }
                }
            }
        }
        
        // Get categories to exclude
        if (req.HasParam(size_t(1))) {
            const auto& excludeParam = req.GetParams()[1];
            if (excludeParam.IsArray()) {
                const auto& arr = excludeParam.GetArray();
                for (const auto& item : arr) {
                    if (item.IsString()) {
                        std::string cat = item.GetString();
                        logger.DisableCategory(cat);
                    }
                }
            }
        }
    }
    
    // Build result with current enabled categories
    JSONValue::Array enabled;
    JSONValue::Array disabled;
    
    for (const auto& cat : allCategories) {
        if (logger.IsCategoryEnabled(cat)) {
            enabled.push_back(JSONValue(cat));
        } else {
            disabled.push_back(JSONValue(cat));
        }
    }
    
    JSONValue::Object result;
    result["enabled"] = JSONValue(std::move(enabled));
    result["disabled"] = JSONValue(std::move(disabled));
    
    return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
}

RPCResponse cmd_echo(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table) {
    // Simply return the parameters
    return RPCResponse::Success(req.GetParams(), req.GetId());
}

RPCResponse cmd_validateaddress(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table) {
    try {
        std::string address = GetRequiredParam<std::string>(req, size_t(0));
        
        JSONValue::Object result;
        result["isvalid"] = ValidateAddress(address);
        result["address"] = address;
        
        if (ValidateAddress(address)) {
            result["scriptPubKey"] = "";
            result["isscript"] = false;
            result["iswitness"] = false;
        }
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_createmultisig(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table) {
    try {
        int64_t nrequired = GetRequiredParam<int64_t>(req, size_t(0));
        const JSONValue& keys = req.GetParam(size_t(1));
        
        if (!keys.IsArray()) {
            return InvalidParams("Keys must be an array", req.GetId());
        }
        
        int64_t nkeys = static_cast<int64_t>(keys.Size());
        
        if (nrequired < 1 || nrequired > nkeys) {
            return InvalidParams("Invalid nrequired value", req.GetId());
        }
        
        if (nkeys > 16) {
            return InvalidParams("Maximum 16 keys allowed", req.GetId());
        }
        
        // Parse public keys
        std::vector<PublicKey> pubkeys;
        for (size_t i = 0; i < keys.Size(); ++i) {
            std::string keyStr = keys[i].GetString();
            
            // Try to parse as hex public key
            auto pubkey = PublicKey::FromHex(keyStr);
            if (!pubkey || !pubkey->IsValid()) {
                return InvalidParams("Invalid public key: " + keyStr, req.GetId());
            }
            pubkeys.push_back(*pubkey);
        }
        
        // Create multisig redeemScript: OP_n <pubkey1> ... <pubkeym> OP_m OP_CHECKMULTISIG
        Script redeemScript;
        redeemScript << Script::EncodeOP_N(static_cast<int>(nrequired));
        for (const auto& pubkey : pubkeys) {
            redeemScript << pubkey.ToVector();
        }
        redeemScript << Script::EncodeOP_N(static_cast<int>(nkeys));
        redeemScript << OP_CHECKMULTISIG;
        
        // Get script hash for P2SH address
        Hash160 scriptHash = ComputeHash160(redeemScript.data(), redeemScript.size());
        
        // Create P2SH address
        // For P2SH, we use a different version byte
        // Mainnet: 0x05 (starts with '3')
        // Testnet: 0xC4 (starts with '2')
        std::string address;
        
        // Check if we're on testnet based on wallet config or context
        wallet::Wallet* wallet = table->GetWallet();
        bool testnet = wallet ? wallet->GetConfig().testnet : false;
        
        // Encode as base58check P2SH address
        uint8_t version = testnet ? 0xC4 : 0x05;
        
        // Build address: version + scriptHash + checksum
        std::vector<uint8_t> addressData;
        addressData.push_back(version);
        addressData.insert(addressData.end(), scriptHash.begin(), scriptHash.end());
        
        // Add 4-byte checksum (double SHA256)
        Hash256 hash1 = SHA256Hash(addressData.data(), addressData.size());
        Hash256 hash2 = SHA256Hash(hash1.data(), hash1.size());
        addressData.insert(addressData.end(), hash2.begin(), hash2.begin() + 4);
        
        // Base58 encode
        address = EncodeBase58(addressData);
        
        // Convert redeemScript to hex
        std::string redeemScriptHex = FormatHex(redeemScript.data(), redeemScript.size());
        
        JSONValue::Object result;
        result["address"] = address;
        result["redeemScript"] = redeemScriptHex;
        
        return RPCResponse::Success(JSONValue(std::move(result)), req.GetId());
    } catch (const std::exception& e) {
        return InvalidParams(e.what(), req.GetId());
    }
}

RPCResponse cmd_estimatefee(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table) {
    int64_t nblocks = GetOptionalParam<int64_t>(req, size_t(0), int64_t(6));
    
    // Validate nblocks
    if (nblocks < 1) {
        nblocks = 1;
    } else if (nblocks > 1008) {
        nblocks = 1008;  // Cap at ~1 week
    }
    
    // Base fee rates (in NXS per kB)
    constexpr double kMinRelayFee = 0.00001;   // 1000 satoshis/kB
    constexpr double kHighPriorityFee = 0.0001;  // 10000 satoshis/kB
    constexpr double kMediumPriorityFee = 0.00005;  // 5000 satoshis/kB
    constexpr double kLowPriorityFee = 0.00002;  // 2000 satoshis/kB
    
    // Average block size for estimation (250KB average)
    constexpr size_t kAvgBlockSize = 250000;
    
    Mempool* mempool = table->GetMempool();
    
    if (!mempool || mempool->Size() == 0) {
        // Empty mempool - return minimum fee based on priority
        double feeRate = kMinRelayFee;
        if (nblocks <= 2) {
            feeRate = kHighPriorityFee;
        } else if (nblocks <= 6) {
            feeRate = kMediumPriorityFee;
        } else {
            feeRate = kLowPriorityFee;
        }
        return RPCResponse::Success(JSONValue(feeRate), req.GetId());
    }
    
    // Get all mempool transactions and their fee rates
    std::vector<TxMempoolInfo> txinfos = mempool->GetAllTxInfo();
    
    if (txinfos.empty()) {
        return RPCResponse::Success(JSONValue(kMinRelayFee), req.GetId());
    }
    
    // Sort transactions by fee rate (highest first - these get mined first)
    std::sort(txinfos.begin(), txinfos.end(), [](const TxMempoolInfo& a, const TxMempoolInfo& b) {
        return a.feeRate > b.feeRate;
    });
    
    // Estimate how many bytes can be confirmed in nblocks
    size_t targetBytes = static_cast<size_t>(nblocks) * kAvgBlockSize;
    
    // Find the fee rate at the target position
    size_t cumulativeSize = 0;
    Amount targetFeePerK = 0;
    
    for (const auto& info : txinfos) {
        cumulativeSize += info.vsize;
        targetFeePerK = info.feeRate.GetFeePerK();
        
        if (cumulativeSize >= targetBytes) {
            // We've reached the target - this fee rate would get confirmed in nblocks
            break;
        }
    }
    
    // Convert to NXS per kB (1 NXS = 100,000,000 satoshis)
    double feeRate = static_cast<double>(targetFeePerK) / 100000000.0;
    
    // Apply minimum fee and add small buffer for reliability
    if (feeRate < kMinRelayFee) {
        feeRate = kMinRelayFee;
    }
    
    // Add 10% buffer for fee estimation reliability
    feeRate *= 1.1;
    
    // Apply priority-based minimum thresholds
    if (nblocks <= 2 && feeRate < kHighPriorityFee) {
        feeRate = kHighPriorityFee;
    } else if (nblocks <= 6 && feeRate < kMediumPriorityFee) {
        feeRate = kMediumPriorityFee;
    } else if (feeRate < kLowPriorityFee) {
        feeRate = kLowPriorityFee;
    }
    
    return RPCResponse::Success(JSONValue(feeRate), req.GetId());
}

} // namespace rpc
} // namespace shurium
