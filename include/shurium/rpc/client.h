// SHURIUM - RPC Client
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// JSON-RPC 2.0 client for communicating with SHURIUM nodes.
//
// Features:
// - HTTP and Unix socket transports
// - Connection pooling
// - Automatic reconnection
// - Batch requests
// - Async requests

#ifndef SHURIUM_RPC_CLIENT_H
#define SHURIUM_RPC_CLIENT_H

#include <shurium/rpc/server.h>

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <type_traits>
#include <vector>

namespace shurium {
namespace rpc {

// ============================================================================
// RPC Client Configuration
// ============================================================================

/**
 * RPC client configuration.
 */
struct RPCClientConfig {
    /// Server hostname or IP
    std::string host{"127.0.0.1"};
    
    /// Server port
    uint16_t port{8332};
    
    /// Unix socket path (if set, overrides host/port)
    std::string unixSocketPath;
    
    /// Use SSL/TLS
    bool useSSL{false};
    
    /// RPC username
    std::string rpcUser;
    
    /// RPC password
    std::string rpcPassword;
    
    /// Connection timeout (seconds)
    int connectTimeout{5};
    
    /// Request timeout (seconds)
    int requestTimeout{30};
    
    /// Enable automatic reconnection
    bool autoReconnect{true};
    
    /// Max reconnection attempts (0 = unlimited)
    size_t maxReconnectAttempts{10};
    
    /// Reconnection delay (milliseconds)
    int reconnectDelay{1000};
    
    /// Max requests per batch
    size_t maxBatchSize{100};
};

// ============================================================================
// RPC Client
// ============================================================================

/**
 * JSON-RPC 2.0 client.
 */
class RPCClient {
public:
    RPCClient();
    explicit RPCClient(const RPCClientConfig& config);
    ~RPCClient();
    
    // Non-copyable
    RPCClient(const RPCClient&) = delete;
    RPCClient& operator=(const RPCClient&) = delete;
    
    // === Configuration ===
    
    /// Set configuration
    void SetConfig(const RPCClientConfig& config);
    
    /// Get configuration
    const RPCClientConfig& GetConfig() const { return config_; }
    
    // === Connection ===
    
    /// Connect to server
    bool Connect();
    
    /// Disconnect from server
    void Disconnect();
    
    /// Check if connected
    bool IsConnected() const;
    
    /// Reconnect to server
    bool Reconnect();
    
    // === Synchronous Calls ===
    
    /// Call an RPC method
    RPCResponse Call(const std::string& method, const JSONValue& params = JSONValue());
    
    /// Call with positional arguments (variadic template)
    /// Note: This template is disabled when passing a single JSONValue to avoid ambiguity
    template<typename T, typename... Args, 
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, JSONValue> || sizeof...(Args) != 0>>
    RPCResponse Call(const std::string& method, T&& first, Args&&... rest) {
        JSONValue::Array params;
        AddParams(params, std::forward<T>(first), std::forward<Args>(rest)...);
        return Call(method, JSONValue(params));
    }
    
    /// Send batch request
    std::vector<RPCResponse> BatchCall(const std::vector<RPCRequest>& requests);
    
    // === Asynchronous Calls ===
    
    /// Async call
    std::future<RPCResponse> CallAsync(const std::string& method, 
                                       const JSONValue& params = JSONValue());
    
    /// Async batch call
    std::future<std::vector<RPCResponse>> BatchCallAsync(
        const std::vector<RPCRequest>& requests);
    
    // === Convenience Methods ===
    
    /// Call and get result (throws on error)
    JSONValue CallForResult(const std::string& method, 
                           const JSONValue& params = JSONValue());
    
    /// Call and check success
    bool CallSuccess(const std::string& method, 
                    const JSONValue& params = JSONValue());
    
    // === Error Handling ===
    
    /// Get last error message
    const std::string& GetLastError() const { return lastError_; }
    
    /// Get last error code
    int GetLastErrorCode() const { return lastErrorCode_; }
    
    // === Statistics ===
    
    /// Get total calls made
    uint64_t GetTotalCalls() const { return totalCalls_; }
    
    /// Get total errors
    uint64_t GetTotalErrors() const { return totalErrors_; }
    
    /// Get average response time (milliseconds)
    double GetAverageResponseTime() const;

private:
    // === Internal Methods ===
    
    /// Send raw request and receive response
    std::string SendRequest(const std::string& json);
    
    /// Build HTTP request
    std::string BuildHTTPRequest(const std::string& body);
    
    /// Parse HTTP response
    bool ParseHTTPResponse(const std::string& raw, std::string& body, int& statusCode);
    
    /// Create socket connection
    bool CreateConnection();
    
    /// Close socket
    void CloseConnection();
    
    /// Generate request ID
    int64_t GenerateId();
    
    /// Helper to add params
    void AddParams(JSONValue::Array& params) {}
    
    template<typename T, typename... Rest>
    void AddParams(JSONValue::Array& params, T&& first, Rest&&... rest) {
        params.push_back(JSONValue(std::forward<T>(first)));
        AddParams(params, std::forward<Rest>(rest)...);
    }
    
    RPCClientConfig config_;
    int socket_{-1};
    std::mutex socketMutex_;
    
    std::string lastError_;
    int lastErrorCode_{0};
    
    std::atomic<int64_t> nextId_{1};
    std::atomic<uint64_t> totalCalls_{0};
    std::atomic<uint64_t> totalErrors_{0};
    std::atomic<int64_t> totalResponseTime_{0};
};

// ============================================================================
// CLI Helper - Command-line interface utilities
// ============================================================================

/**
 * Parses command-line arguments for RPC calls.
 */
class RPCCLIParser {
public:
    RPCCLIParser();
    
    /// Parse command line arguments
    bool Parse(int argc, char* argv[]);
    
    /// Get method name
    const std::string& GetMethod() const { return method_; }
    
    /// Get parameters as JSON
    JSONValue GetParams() const;
    
    /// Get RPC client config from args
    RPCClientConfig GetClientConfig() const;
    
    /// Check if help was requested
    bool WantsHelp() const { return wantsHelp_; }
    
    /// Get help text
    std::string GetHelpText() const;
    
    /// Check if version was requested
    bool WantsVersion() const { return wantsVersion_; }
    
    /// Get error message
    const std::string& GetError() const { return error_; }
    
    // Options
    std::string host{"127.0.0.1"};
    uint16_t port{8332};
    std::string rpcUser;
    std::string rpcPassword;
    std::string dataDir;
    bool useStdin{false};
    bool prettyPrint{true};

private:
    /// Parse a JSON argument
    JSONValue ParseArg(const std::string& arg) const;
    
    std::string method_;
    std::vector<std::string> args_;
    bool wantsHelp_{false};
    bool wantsVersion_{false};
    std::string error_;
};

/**
 * Formats RPC results for CLI output.
 */
class RPCResultFormatter {
public:
    /// Format result as human-readable text
    static std::string FormatAsText(const JSONValue& value, int indent = 0);
    
    /// Format result as JSON (optionally pretty-printed)
    static std::string FormatAsJSON(const JSONValue& value, bool pretty = true);
    
    /// Format error for display
    static std::string FormatError(int code, const std::string& message);
    
    /// Format help for a method
    static std::string FormatMethodHelp(const RPCMethod& method);
    
    /// Format list of methods
    static std::string FormatMethodList(const std::vector<RPCMethod>& methods);
};

} // namespace rpc
} // namespace shurium

#endif // SHURIUM_RPC_CLIENT_H
