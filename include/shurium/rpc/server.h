// SHURIUM - RPC Server
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// JSON-RPC 2.0 server for SHURIUM node communication.
//
// Features:
// - HTTP and Unix socket transports
// - Authentication support
// - Rate limiting
// - SSL/TLS support (optional)
// - Async request handling

#ifndef SHURIUM_RPC_SERVER_H
#define SHURIUM_RPC_SERVER_H

#include <shurium/core/types.h>
#include <shurium/util/threadpool.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace shurium {
namespace rpc {

// Forward declarations
class RPCServer;
class RPCRequest;
class RPCResponse;

// ============================================================================
// JSON Value - Simple JSON representation
// ============================================================================

/**
 * Represents a JSON value.
 * Supports: null, bool, int64, double, string, array, object
 */
class JSONValue {
public:
    enum class Type {
        Null,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object
    };
    
    using Array = std::vector<JSONValue>;
    using Object = std::map<std::string, JSONValue>;
    
    // Constructors
    JSONValue() : type_(Type::Null) {}
    JSONValue(std::nullptr_t) : type_(Type::Null) {}
    JSONValue(bool value) : type_(Type::Bool), boolValue_(value) {}
    JSONValue(int value) : type_(Type::Int), intValue_(value) {}
    JSONValue(int64_t value) : type_(Type::Int), intValue_(value) {}
    JSONValue(uint64_t value) : type_(Type::Int), intValue_(static_cast<int64_t>(value)) {}
    JSONValue(double value) : type_(Type::Double), doubleValue_(value) {}
    JSONValue(const char* value) : type_(Type::String), stringValue_(value) {}
    JSONValue(const std::string& value) : type_(Type::String), stringValue_(value) {}
    JSONValue(std::string&& value) : type_(Type::String), stringValue_(std::move(value)) {}
    JSONValue(const Array& value) : type_(Type::Array), arrayValue_(value) {}
    JSONValue(Array&& value) : type_(Type::Array), arrayValue_(std::move(value)) {}
    JSONValue(const Object& value) : type_(Type::Object), objectValue_(value) {}
    JSONValue(Object&& value) : type_(Type::Object), objectValue_(std::move(value)) {}
    
    // Type checking
    Type GetType() const { return type_; }
    bool IsNull() const { return type_ == Type::Null; }
    bool IsBool() const { return type_ == Type::Bool; }
    bool IsInt() const { return type_ == Type::Int; }
    bool IsDouble() const { return type_ == Type::Double; }
    bool IsNumber() const { return type_ == Type::Int || type_ == Type::Double; }
    bool IsString() const { return type_ == Type::String; }
    bool IsArray() const { return type_ == Type::Array; }
    bool IsObject() const { return type_ == Type::Object; }
    
    // Value getters (with defaults)
    bool GetBool(bool defaultValue = false) const;
    int64_t GetInt(int64_t defaultValue = 0) const;
    double GetDouble(double defaultValue = 0.0) const;
    const std::string& GetString(const std::string& defaultValue = "") const;
    const Array& GetArray() const;
    const Object& GetObject() const;
    
    // Object access
    bool HasKey(const std::string& key) const;
    const JSONValue& operator[](const std::string& key) const;
    JSONValue& operator[](const std::string& key);
    
    // Array access
    size_t Size() const;
    const JSONValue& operator[](size_t index) const;
    JSONValue& operator[](size_t index);
    void Push(const JSONValue& value);
    void Push(JSONValue&& value);
    
    // Serialization
    std::string ToJSON(bool pretty = false, int indent = 0) const;
    static JSONValue Parse(const std::string& json);
    static std::optional<JSONValue> TryParse(const std::string& json);
    
    /// Get a null value reference (for returning from accessors)
    static const JSONValue& Null() { return nullValue_; }

private:
    Type type_;
    bool boolValue_{false};
    int64_t intValue_{0};
    double doubleValue_{0.0};
    std::string stringValue_;
    Array arrayValue_;
    Object objectValue_;
    
    static const JSONValue nullValue_;
    static const Array emptyArray_;
    static const Object emptyObject_;
    static const std::string emptyString_;
};

// ============================================================================
// RPC Error Codes (JSON-RPC 2.0 standard + custom)
// ============================================================================

namespace ErrorCode {
    // Standard JSON-RPC 2.0 errors
    constexpr int PARSE_ERROR = -32700;
    constexpr int INVALID_REQUEST = -32600;
    constexpr int METHOD_NOT_FOUND = -32601;
    constexpr int INVALID_PARAMS = -32602;
    constexpr int INTERNAL_ERROR = -32603;
    
    // Server errors (-32000 to -32099)
    constexpr int SERVER_ERROR = -32000;
    constexpr int NOT_READY = -32001;
    constexpr int SHUTTING_DOWN = -32002;
    constexpr int RATE_LIMITED = -32003;
    
    // Authentication errors
    constexpr int UNAUTHORIZED = -32010;
    constexpr int FORBIDDEN = -32011;
    
    // SHURIUM-specific errors (-1 to -999)
    constexpr int WALLET_ERROR = -1;
    constexpr int WALLET_INSUFFICIENT_FUNDS = -2;
    constexpr int WALLET_KEYPOOL_RAN_OUT = -3;
    constexpr int WALLET_UNLOCK_NEEDED = -4;
    constexpr int WALLET_NOT_FOUND = -5;
    
    constexpr int NETWORK_ERROR = -10;
    constexpr int PEER_NOT_CONNECTED = -11;
    
    constexpr int BLOCK_NOT_FOUND = -20;
    constexpr int TX_NOT_FOUND = -21;
    constexpr int TX_REJECTED = -22;
    constexpr int TX_ALREADY_IN_CHAIN = -23;
    
    constexpr int IDENTITY_ERROR = -30;
    constexpr int IDENTITY_NOT_FOUND = -31;
    constexpr int IDENTITY_INVALID_PROOF = -32;
    
    constexpr int STAKING_ERROR = -40;
    constexpr int VALIDATOR_NOT_FOUND = -41;
    
    constexpr int GOVERNANCE_ERROR = -50;
    constexpr int PROPOSAL_NOT_FOUND = -51;
}

// ============================================================================
// RPC Request
// ============================================================================

/**
 * Represents a JSON-RPC 2.0 request.
 */
class RPCRequest {
public:
    RPCRequest() = default;
    RPCRequest(const std::string& method, const JSONValue& params = JSONValue(),
               const JSONValue& id = JSONValue());
    
    /// Get the method name
    const std::string& GetMethod() const { return method_; }
    
    /// Get parameters
    const JSONValue& GetParams() const { return params_; }
    
    /// Get request ID
    const JSONValue& GetId() const { return id_; }
    
    /// Check if this is a notification (no id)
    bool IsNotification() const { return id_.IsNull(); }
    
    /// Get a parameter by index (for array params)
    const JSONValue& GetParam(size_t index) const;
    
    /// Get a parameter by name (for object params)
    const JSONValue& GetParam(const std::string& name) const;
    
    /// Check if parameter exists
    bool HasParam(const std::string& name) const;
    bool HasParam(size_t index) const;
    
    /// Serialize to JSON
    std::string ToJSON() const;
    
    /// Parse from JSON
    static std::optional<RPCRequest> Parse(const std::string& json);
    
    /// Parse batch of requests
    static std::vector<RPCRequest> ParseBatch(const std::string& json);

private:
    std::string method_;
    JSONValue params_;
    JSONValue id_;
};

// ============================================================================
// RPC Response
// ============================================================================

/**
 * Represents a JSON-RPC 2.0 response.
 */
class RPCResponse {
public:
    /// Create success response
    static RPCResponse Success(const JSONValue& result, const JSONValue& id);
    
    /// Create error response
    static RPCResponse Error(int code, const std::string& message,
                            const JSONValue& id, const JSONValue& data = JSONValue());
    
    /// Check if response is an error
    bool IsError() const { return isError_; }
    
    /// Get result (for success responses)
    const JSONValue& GetResult() const { return result_; }
    
    /// Get error code
    int GetErrorCode() const { return errorCode_; }
    
    /// Get error message
    const std::string& GetErrorMessage() const { return errorMessage_; }
    
    /// Get error data
    const JSONValue& GetErrorData() const { return errorData_; }
    
    /// Get response ID
    const JSONValue& GetId() const { return id_; }
    
    /// Serialize to JSON
    std::string ToJSON() const;
    
    /// Serialize batch responses
    static std::string BatchToJSON(const std::vector<RPCResponse>& responses);

private:
    bool isError_{false};
    JSONValue result_;
    int errorCode_{0};
    std::string errorMessage_;
    JSONValue errorData_;
    JSONValue id_;
};

// ============================================================================
// RPC Method Handler
// ============================================================================

/**
 * Context passed to RPC method handlers.
 */
struct RPCContext {
    /// Client address (for logging/rate limiting)
    std::string clientAddress;
    
    /// Authenticated username (empty if not authenticated)
    std::string username;
    
    /// Is connection from localhost?
    bool isLocal{false};
    
    /// Additional metadata
    std::map<std::string, std::string> metadata;
};

/// RPC method handler function type
using RPCHandler = std::function<RPCResponse(const RPCRequest&, const RPCContext&)>;

/**
 * RPC method registration info.
 */
struct RPCMethod {
    std::string name;
    std::string category;
    std::string description;
    RPCHandler handler;
    bool requiresAuth{false};
    bool requiresWallet{false};
    std::vector<std::string> argNames;
    std::vector<std::string> argDescriptions;
};

// ============================================================================
// RPC Server Configuration
// ============================================================================

/**
 * RPC server configuration.
 */
struct RPCServerConfig {
    /// HTTP bind address
    std::string bindAddress{"127.0.0.1"};
    
    /// HTTP port
    uint16_t port{8332};
    
    /// Unix socket path (empty to disable)
    std::string unixSocketPath;
    
    /// Enable SSL/TLS
    bool enableSSL{false};
    
    /// SSL certificate file
    std::string sslCertFile;
    
    /// SSL key file
    std::string sslKeyFile;
    
    /// RPC username (empty for no auth)
    std::string rpcUser;
    
    /// RPC password
    std::string rpcPassword;
    
    /// Allow connections from non-localhost
    bool allowRemote{false};
    
    /// Max concurrent connections
    size_t maxConnections{128};
    
    /// Request timeout (seconds)
    int requestTimeout{30};
    
    /// Enable rate limiting
    bool enableRateLimiting{true};
    
    /// Max requests per minute per client
    size_t maxRequestsPerMinute{600};
    
    /// Thread pool size
    size_t threadPoolSize{4};
    
    /// Max request body size (bytes)
    size_t maxRequestSize{10 * 1024 * 1024};  // 10 MB
};

// ============================================================================
// RPC Server
// ============================================================================

/**
 * JSON-RPC 2.0 server.
 */
class RPCServer {
public:
    RPCServer();
    explicit RPCServer(const RPCServerConfig& config);
    ~RPCServer();
    
    // Non-copyable
    RPCServer(const RPCServer&) = delete;
    RPCServer& operator=(const RPCServer&) = delete;
    
    // === Configuration ===
    
    /// Set configuration
    void SetConfig(const RPCServerConfig& config);
    
    /// Get configuration
    const RPCServerConfig& GetConfig() const { return config_; }
    
    // === Server Control ===
    
    /// Start the server
    bool Start();
    
    /// Stop the server
    void Stop();
    
    /// Check if server is running
    bool IsRunning() const { return running_.load(); }
    
    /// Wait for server to stop
    void Wait();
    
    // === Method Registration ===
    
    /// Register an RPC method
    void RegisterMethod(const RPCMethod& method);
    
    /// Unregister an RPC method
    void UnregisterMethod(const std::string& name);
    
    /// Check if method exists
    bool HasMethod(const std::string& name) const;
    
    /// Get registered method info
    std::optional<RPCMethod> GetMethod(const std::string& name) const;
    
    /// Get all registered methods
    std::vector<RPCMethod> GetMethods() const;
    
    /// Get methods by category
    std::vector<RPCMethod> GetMethodsByCategory(const std::string& category) const;
    
    // === Request Handling ===
    
    /// Process a single request (for testing or internal use)
    RPCResponse HandleRequest(const RPCRequest& request, const RPCContext& context);
    
    /// Process raw JSON request
    std::string HandleRawRequest(const std::string& json, const RPCContext& context);
    
    // === Statistics ===
    
    /// Get total requests handled
    uint64_t GetTotalRequests() const { return totalRequests_.load(); }
    
    /// Get total errors
    uint64_t GetTotalErrors() const { return totalErrors_.load(); }
    
    /// Get active connections
    size_t GetActiveConnections() const { return activeConnections_.load(); }
    
    /// Get uptime in seconds
    int64_t GetUptime() const;

private:
    // === Internal Methods ===
    
    /// HTTP server thread
    void HTTPServerThread();
    
    /// Handle HTTP connection
    void HandleConnection(int clientSocket);
    
    /// Parse HTTP request
    bool ParseHTTPRequest(const std::string& raw, std::string& body, 
                         std::map<std::string, std::string>& headers);
    
    /// Build HTTP response
    std::string BuildHTTPResponse(int statusCode, const std::string& body,
                                 const std::string& contentType = "application/json");
    
    /// Authenticate request
    bool Authenticate(const std::map<std::string, std::string>& headers,
                     std::string& username);
    
    /// Check rate limit
    bool CheckRateLimit(const std::string& clientAddress);
    
    /// Clean up rate limit data
    void CleanupRateLimits();
    
    RPCServerConfig config_;
    std::atomic<bool> running_{false};
    
    // Method registry
    std::map<std::string, RPCMethod> methods_;
    mutable std::mutex methodsMutex_;
    
    // Server threads
    std::thread httpThread_;
    int serverSocket_{-1};
    
    // Statistics
    std::atomic<uint64_t> totalRequests_{0};
    std::atomic<uint64_t> totalErrors_{0};
    std::atomic<size_t> activeConnections_{0};
    std::chrono::steady_clock::time_point startTime_;
    
    // Rate limiting
    struct RateLimitEntry {
        std::chrono::steady_clock::time_point windowStart;
        size_t requestCount{0};
    };
    std::unordered_map<std::string, RateLimitEntry> rateLimits_;
    std::mutex rateLimitMutex_;
    
    // Thread pool for handling connections
    std::unique_ptr<util::ThreadPool> threadPool_;
};

// ============================================================================
// Helper Functions
// ============================================================================

/// Create a successful RPC response
inline RPCResponse RPCSuccess(const JSONValue& result, const JSONValue& id) {
    return RPCResponse::Success(result, id);
}

/// Create an error RPC response
inline RPCResponse RPCError(int code, const std::string& message, const JSONValue& id) {
    return RPCResponse::Error(code, message, id);
}

/// Create parse error response
inline RPCResponse ParseError(const JSONValue& id = JSONValue()) {
    return RPCResponse::Error(ErrorCode::PARSE_ERROR, "Parse error", id);
}

/// Create invalid request error response
inline RPCResponse InvalidRequest(const JSONValue& id = JSONValue()) {
    return RPCResponse::Error(ErrorCode::INVALID_REQUEST, "Invalid Request", id);
}

/// Create method not found error response
inline RPCResponse MethodNotFound(const std::string& method, const JSONValue& id) {
    return RPCResponse::Error(ErrorCode::METHOD_NOT_FOUND, 
                             "Method not found: " + method, id);
}

/// Create invalid params error response
inline RPCResponse InvalidParams(const std::string& message, const JSONValue& id) {
    return RPCResponse::Error(ErrorCode::INVALID_PARAMS, message, id);
}

/// Create internal error response
inline RPCResponse InternalError(const std::string& message, const JSONValue& id) {
    return RPCResponse::Error(ErrorCode::INTERNAL_ERROR, message, id);
}

// ============================================================================
// Security Functions
// ============================================================================

/**
 * Generate a cryptographically secure random password for RPC authentication.
 * Uses OpenSSL RAND_bytes when available, falls back to std::random_device.
 * 
 * @param length Number of random bytes (output will be 2x this in hex chars)
 * @return Hex-encoded random string suitable for use as RPC password
 */
std::string GenerateRPCPassword(size_t length = 32);

/**
 * Generate a random username for RPC authentication.
 * 
 * @param prefix Optional prefix for the username
 * @return Username string
 */
std::string GenerateRPCUsername(const std::string& prefix = "nexus");

/**
 * Generate both username and password for RPC authentication and write to a cookie file.
 * The cookie file format is: username:password
 * 
 * @param cookiePath Path to write the cookie file
 * @return true if successful, false on error
 */
bool GenerateRPCCookie(const std::string& cookiePath);

} // namespace rpc
} // namespace shurium

#endif // SHURIUM_RPC_SERVER_H
