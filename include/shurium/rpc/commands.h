// SHURIUM - RPC Commands
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Defines all RPC commands for the SHURIUM node.
//
// Categories:
// - Blockchain: getblockchaininfo, getblock, getblockhash, etc.
// - Network: getnetworkinfo, getpeerinfo, addnode, etc.
// - Wallet: getbalance, sendtoaddress, listtransactions, etc.
// - Identity: createidentity, verifyidentity, claimubi, etc.
// - Staking: getstakinginfo, createvalidator, delegate, etc.
// - Governance: getproposals, vote, createproposal, etc.
// - Utility: help, stop, uptime, etc.

#ifndef SHURIUM_RPC_COMMANDS_H
#define SHURIUM_RPC_COMMANDS_H

#include <shurium/rpc/server.h>

#include <memory>
#include <string>
#include <vector>

// Forward declarations - classes are in the nexus namespace directly
namespace shurium {
    class ChainState;
    class ChainStateManager;
    class Mempool;
    class MessageProcessor;
    namespace db { class BlockDB; }
    namespace wallet { class Wallet; }
    namespace identity { class IdentityManager; }
    namespace economics { class UBIDistributor; }
    namespace staking { class StakingEngine; }
    namespace governance { class GovernanceEngine; }
    namespace network { class NetworkManager; }
    namespace miner { class Miner; }
}

namespace shurium {
namespace rpc {

// ============================================================================
// Command Categories
// ============================================================================

namespace Category {
    constexpr const char* BLOCKCHAIN = "Blockchain";
    constexpr const char* NETWORK = "Network";
    constexpr const char* WALLET = "Wallet";
    constexpr const char* IDENTITY = "Identity";
    constexpr const char* STAKING = "Staking";
    constexpr const char* GOVERNANCE = "Governance";
    constexpr const char* UTILITY = "Utility";
    constexpr const char* MINING = "Mining";
}

// ============================================================================
// RPC Command Table - Registration
// ============================================================================

/**
 * Manages the registration and context for RPC commands.
 */
class RPCCommandTable {
public:
    RPCCommandTable();
    ~RPCCommandTable();
    
    // === Context Setup ===
    
    /// Set chain state reference
    void SetChainState(std::shared_ptr<ChainState> chainState);
    
    /// Set chain state manager reference (for full block processing)
    void SetChainStateManager(std::shared_ptr<ChainStateManager> chainManager);
    
    /// Set mempool reference
    void SetMempool(std::shared_ptr<Mempool> mempool);
    
    /// Set wallet reference
    void SetWallet(std::shared_ptr<wallet::Wallet> wallet);
    
    /// Set identity manager reference
    void SetIdentityManager(std::shared_ptr<identity::IdentityManager> identity);
    
    /// Set UBI distributor reference
    void SetUBIDistributor(std::shared_ptr<economics::UBIDistributor> ubi);
    
    /// Set staking engine reference
    void SetStakingEngine(std::shared_ptr<staking::StakingEngine> staking);
    
    /// Set governance engine reference
    void SetGovernanceEngine(std::shared_ptr<governance::GovernanceEngine> governance);
    
    /// Set network manager reference
    void SetNetworkManager(std::shared_ptr<network::NetworkManager> network);
    
    /// Set message processor reference (for block/tx relay)
    void SetMessageProcessor(MessageProcessor* msgproc);
    
    /// Set block database reference (for raw block data)
    void SetBlockDB(std::shared_ptr<db::BlockDB> blockdb);
    
    /// Set data directory path (for wallet file paths)
    void SetDataDir(const std::string& dataDir);
    
    /// Set miner reference (for mining control)
    void SetMiner(miner::Miner* miner);
    
    // === Command Registration ===
    
    /// Register all commands with the server
    void RegisterCommands(RPCServer& server);
    
    /// Get all registered commands
    std::vector<RPCMethod> GetAllCommands() const;
    
    /// Get commands by category
    std::vector<RPCMethod> GetCommandsByCategory(const std::string& category) const;
    
    // === Context Access ===
    
    ChainState* GetChainState() const { return chainState_.get(); }
    ChainStateManager* GetChainStateManager() const { return chainManager_.get(); }
    Mempool* GetMempool() const { return mempool_.get(); }
    wallet::Wallet* GetWallet() const { return wallet_.get(); }
    identity::IdentityManager* GetIdentityManager() const { return identity_.get(); }
    economics::UBIDistributor* GetUBIDistributor() const { return ubiDistributor_.get(); }
    staking::StakingEngine* GetStakingEngine() const { return staking_.get(); }
    governance::GovernanceEngine* GetGovernanceEngine() const { return governance_.get(); }
    network::NetworkManager* GetNetworkManager() const { return network_.get(); }
    MessageProcessor* GetMessageProcessor() const { return msgproc_; }
    db::BlockDB* GetBlockDB() const { return blockdb_.get(); }
    const std::string& GetDataDir() const { return dataDir_; }
    miner::Miner* GetMiner() const { return miner_; }

private:
    // === Command Registration Helpers ===
    
    void RegisterBlockchainCommands();
    void RegisterNetworkCommands();
    void RegisterWalletCommands();
    void RegisterIdentityCommands();
    void RegisterStakingCommands();
    void RegisterGovernanceCommands();
    void RegisterUtilityCommands();
    void RegisterMiningCommands();
    
    std::vector<RPCMethod> commands_;
    
    // Context references
    std::shared_ptr<ChainState> chainState_;
    std::shared_ptr<ChainStateManager> chainManager_;
    std::shared_ptr<Mempool> mempool_;
    std::shared_ptr<wallet::Wallet> wallet_;
    std::shared_ptr<identity::IdentityManager> identity_;
    std::shared_ptr<economics::UBIDistributor> ubiDistributor_;
    std::shared_ptr<staking::StakingEngine> staking_;
    std::shared_ptr<governance::GovernanceEngine> governance_;
    std::shared_ptr<network::NetworkManager> network_;
    std::shared_ptr<db::BlockDB> blockdb_;
    MessageProcessor* msgproc_{nullptr};  // Not owned - raw pointer for optional ref
    std::string dataDir_;  // Data directory for wallet file paths
    miner::Miner* miner_{nullptr};  // Not owned - raw pointer for mining control
};

// ============================================================================
// Blockchain Commands
// ============================================================================

/// Get blockchain information
RPCResponse cmd_getblockchaininfo(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table);

/// Get best block hash
RPCResponse cmd_getbestblockhash(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table);

/// Get block count
RPCResponse cmd_getblockcount(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Get block by hash
RPCResponse cmd_getblock(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table);

/// Get block hash by height
RPCResponse cmd_getblockhash(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// Get block header
RPCResponse cmd_getblockheader(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get chain tips
RPCResponse cmd_getchaintips(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// Get difficulty
RPCResponse cmd_getdifficulty(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Get mempool info
RPCResponse cmd_getmempoolinfo(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get raw mempool
RPCResponse cmd_getrawmempool(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Get transaction
RPCResponse cmd_gettransaction(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get raw transaction
RPCResponse cmd_getrawtransaction(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table);

/// Decode raw transaction
RPCResponse cmd_decoderawtransaction(const RPCRequest& req, const RPCContext& ctx,
                                     RPCCommandTable* table);

/// Send raw transaction
RPCResponse cmd_sendrawtransaction(const RPCRequest& req, const RPCContext& ctx,
                                   RPCCommandTable* table);

// ============================================================================
// Network Commands
// ============================================================================

/// Get network info
RPCResponse cmd_getnetworkinfo(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get peer info
RPCResponse cmd_getpeerinfo(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

/// Get connection count
RPCResponse cmd_getconnectioncount(const RPCRequest& req, const RPCContext& ctx,
                                   RPCCommandTable* table);

/// Add node
RPCResponse cmd_addnode(const RPCRequest& req, const RPCContext& ctx,
                        RPCCommandTable* table);

/// Disconnect node
RPCResponse cmd_disconnectnode(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get added node info
RPCResponse cmd_getaddednodeinfo(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table);

/// Set network active
RPCResponse cmd_setnetworkactive(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table);

/// Ping all peers
RPCResponse cmd_ping(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table);

// ============================================================================
// Wallet Commands
// ============================================================================

/// Get wallet info
RPCResponse cmd_getwalletinfo(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Get balance
RPCResponse cmd_getbalance(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table);

/// Get unconfirmed balance
RPCResponse cmd_getunconfirmedbalance(const RPCRequest& req, const RPCContext& ctx,
                                      RPCCommandTable* table);

/// Get new address
RPCResponse cmd_getnewaddress(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Get address info
RPCResponse cmd_getaddressinfo(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// List addresses
RPCResponse cmd_listaddresses(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Send to address
RPCResponse cmd_sendtoaddress(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Send many
RPCResponse cmd_sendmany(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table);

/// Send from specific address
RPCResponse cmd_sendfrom(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table);

/// List transactions
RPCResponse cmd_listtransactions(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table);

/// List unspent
RPCResponse cmd_listunspent(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

/// Sign message
RPCResponse cmd_signmessage(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

/// Verify message
RPCResponse cmd_verifymessage(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Dump private key
RPCResponse cmd_dumpprivkey(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

/// Import private key
RPCResponse cmd_importprivkey(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Lock wallet
RPCResponse cmd_walletlock(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table);

/// Unlock wallet
RPCResponse cmd_walletpassphrase(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table);

/// Change wallet passphrase
RPCResponse cmd_walletpassphrasechange(const RPCRequest& req, const RPCContext& ctx,
                                       RPCCommandTable* table);

/// Encrypt wallet
RPCResponse cmd_encryptwallet(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Backup wallet
RPCResponse cmd_backupwallet(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// Load wallet from file
RPCResponse cmd_loadwallet(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table);

/// Create new wallet
RPCResponse cmd_createwallet(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// Unload current wallet
RPCResponse cmd_unloadwallet(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// Restore wallet from mnemonic phrase
RPCResponse cmd_restorewallet(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

// ============================================================================
// Identity Commands
// ============================================================================

/// Get identity info
RPCResponse cmd_getidentityinfo(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table);

/// Create identity
RPCResponse cmd_createidentity(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Verify identity
RPCResponse cmd_verifyidentity(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get identity status
RPCResponse cmd_getidentitystatus(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table);

/// Claim UBI
RPCResponse cmd_claimubi(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table);

/// Get UBI info
RPCResponse cmd_getubiinfo(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table);

/// Get UBI history
RPCResponse cmd_getubihistory(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

// ============================================================================
// Staking Commands
// ============================================================================

/// Get staking info
RPCResponse cmd_getstakinginfo(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get validator info
RPCResponse cmd_getvalidatorinfo(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table);

/// List validators
RPCResponse cmd_listvalidators(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Create validator
RPCResponse cmd_createvalidator(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table);

/// Update validator
RPCResponse cmd_updatevalidator(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table);

/// Delegate
RPCResponse cmd_delegate(const RPCRequest& req, const RPCContext& ctx,
                         RPCCommandTable* table);

/// Undelegate
RPCResponse cmd_undelegate(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table);

/// List delegations
RPCResponse cmd_listdelegations(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table);

/// Claim staking rewards
RPCResponse cmd_claimrewards(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// Get pending rewards
RPCResponse cmd_getpendingrewards(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table);

/// Unjail validator
RPCResponse cmd_unjailvalidator(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table);

// ============================================================================
// Governance Commands
// ============================================================================

/// Get governance info
RPCResponse cmd_getgovernanceinfo(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table);

/// List proposals
RPCResponse cmd_listproposals(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Get proposal
RPCResponse cmd_getproposal(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

/// Create proposal
RPCResponse cmd_createproposal(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Vote on proposal
RPCResponse cmd_vote(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table);

/// Get vote info
RPCResponse cmd_getvoteinfo(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

/// Delegate voting power
RPCResponse cmd_delegatevote(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// Undelegate voting power
RPCResponse cmd_undelegatevote(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get parameter value
RPCResponse cmd_getparameter(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// List parameters
RPCResponse cmd_listparameters(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

// ============================================================================
// Mining/PoUW Commands
// ============================================================================

/// Get mining info
RPCResponse cmd_getmininginfo(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Get block template
RPCResponse cmd_getblocktemplate(const RPCRequest& req, const RPCContext& ctx,
                                 RPCCommandTable* table);

/// Submit block
RPCResponse cmd_submitblock(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

/// Get work
RPCResponse cmd_getwork(const RPCRequest& req, const RPCContext& ctx,
                        RPCCommandTable* table);

/// Submit work solution
RPCResponse cmd_submitwork(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table);

/// List problems
RPCResponse cmd_listproblems(const RPCRequest& req, const RPCContext& ctx,
                             RPCCommandTable* table);

/// Get problem
RPCResponse cmd_getproblem(const RPCRequest& req, const RPCContext& ctx,
                           RPCCommandTable* table);

/// Create problem
RPCResponse cmd_createproblem(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Get marketplace info
RPCResponse cmd_getmarketplaceinfo(const RPCRequest& req, const RPCContext& ctx,
                                    RPCCommandTable* table);

/// Generate blocks to address (regtest only)
RPCResponse cmd_generatetoaddress(const RPCRequest& req, const RPCContext& ctx,
                                  RPCCommandTable* table);

/// Enable or disable mining (setgenerate)
RPCResponse cmd_setgenerate(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

// ============================================================================
// Utility Commands
// ============================================================================

/// Help
RPCResponse cmd_help(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table);

/// Stop server
RPCResponse cmd_stop(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table);

/// Get uptime
RPCResponse cmd_uptime(const RPCRequest& req, const RPCContext& ctx,
                       RPCCommandTable* table);

/// Get memory info
RPCResponse cmd_getmemoryinfo(const RPCRequest& req, const RPCContext& ctx,
                              RPCCommandTable* table);

/// Log level
RPCResponse cmd_logging(const RPCRequest& req, const RPCContext& ctx,
                        RPCCommandTable* table);

/// Echo (for testing)
RPCResponse cmd_echo(const RPCRequest& req, const RPCContext& ctx,
                     RPCCommandTable* table);

/// Validate address
RPCResponse cmd_validateaddress(const RPCRequest& req, const RPCContext& ctx,
                                RPCCommandTable* table);

/// Create multisig
RPCResponse cmd_createmultisig(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Estimate fee
RPCResponse cmd_estimatefee(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

// ============================================================================
// Fund Management Commands
// ============================================================================

/// Get information about all protocol funds
/// Returns: array of fund info objects
RPCResponse cmd_getfundinfo(const RPCRequest& req, const RPCContext& ctx,
                            RPCCommandTable* table);

/// Get balance for a specific fund
/// Params: fundtype (ubi, contribution, ecosystem, stability)
/// Returns: balance, totalReceived, totalSpent
RPCResponse cmd_getfundbalance(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// List recent transactions for a fund
/// Params: fundtype, count (optional), skip (optional)
/// Returns: array of transaction info
RPCResponse cmd_listfundtransactions(const RPCRequest& req, const RPCContext& ctx,
                                     RPCCommandTable* table);

/// Get fund addresses (shows multisig details)
/// Params: fundtype (optional, if omitted shows all)
/// Returns: fund address details including pubkeys
RPCResponse cmd_getfundaddress(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

/// Set a custom address for a fund (for organizations/governments)
/// Params: fundtype, address
/// Returns: success status and warning about persistence
RPCResponse cmd_setfundaddress(const RPCRequest& req, const RPCContext& ctx,
                               RPCCommandTable* table);

// ============================================================================
// Helper Functions
// ============================================================================

/// Parse amount from JSON value
Amount ParseAmount(const JSONValue& value);

/// Format amount for JSON output
JSONValue FormatAmount(Amount amount);

/// Parse address from string
std::string ParseAddress(const std::string& str);

/// Validate address format
bool ValidateAddress(const std::string& address);

/// Parse hex string
std::vector<Byte> ParseHex(const std::string& hex);

/// Format bytes as hex
std::string FormatHex(const std::vector<Byte>& bytes);
std::string FormatHex(const Byte* data, size_t len);

/// Get required parameter
template<typename T>
T GetRequiredParam(const RPCRequest& req, const std::string& name);

template<typename T>
T GetRequiredParam(const RPCRequest& req, size_t index);

/// Get optional parameter with default
template<typename T>
T GetOptionalParam(const RPCRequest& req, const std::string& name, const T& defaultValue);

template<typename T>
T GetOptionalParam(const RPCRequest& req, size_t index, const T& defaultValue);

} // namespace rpc
} // namespace shurium

#endif // SHURIUM_RPC_COMMANDS_H
