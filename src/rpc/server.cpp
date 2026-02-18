// SHURIUM - RPC Server Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/rpc/server.h>
#include <shurium/util/logging.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

#ifdef SHURIUM_USE_OPENSSL
#include <openssl/crypto.h>
#include <openssl/rand.h>
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define CLOSE_SOCKET closesocket
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/stat.h>
    #include <sys/un.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    using socket_t = int;
    #define INVALID_SOCKET_VALUE (-1)
    #define CLOSE_SOCKET close
#endif

namespace shurium {
namespace rpc {

// ============================================================================
// JSONValue Static Members
// ============================================================================

const JSONValue JSONValue::nullValue_;
const JSONValue::Array JSONValue::emptyArray_;
const JSONValue::Object JSONValue::emptyObject_;
const std::string JSONValue::emptyString_;

// ============================================================================
// Security Helper Functions
// ============================================================================

/**
 * Constant-time string comparison to prevent timing attacks.
 * Returns true if strings are equal, false otherwise.
 * Time taken is proportional to the length of the first string,
 * regardless of where the strings differ.
 */
static bool ConstantTimeCompare(const std::string& a, const std::string& b) {
    // Use OpenSSL's CRYPTO_memcmp if available (guaranteed constant-time)
#ifdef SHURIUM_USE_OPENSSL
    if (a.size() != b.size()) {
        // Still do a dummy comparison to avoid length-based timing leaks
        volatile unsigned char dummy = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            dummy |= a[i];
        }
        (void)dummy;
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
#else
    // Software constant-time comparison
    if (a.size() != b.size()) {
        // Still do a dummy comparison to avoid length-based timing leaks
        volatile unsigned char dummy = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            dummy |= a[i];
        }
        (void)dummy;
        return false;
    }
    
    volatile unsigned char result = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        result |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return result == 0;
#endif
}

/**
 * Generate a cryptographically secure random hex string.
 * Used for generating RPC authentication cookies.
 */
static std::string GenerateSecureHex(size_t bytes) {
    std::vector<unsigned char> buffer(bytes);
    
#ifdef SHURIUM_USE_OPENSSL
    if (RAND_bytes(buffer.data(), static_cast<int>(bytes)) != 1) {
        // Fallback if OpenSSL RAND fails
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<unsigned int> dist(0, 255);
        for (size_t i = 0; i < bytes; ++i) {
            buffer[i] = static_cast<unsigned char>(dist(gen));
        }
    }
#else
    // Use std::random_device (may not be cryptographically secure on all platforms)
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    for (size_t i = 0; i < bytes; ++i) {
        buffer[i] = static_cast<unsigned char>(dist(gen));
    }
#endif
    
    // Convert to hex
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char byte : buffer) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

// ============================================================================
// Login Attempt Tracking (Brute-Force Protection)
// ============================================================================

struct LoginAttemptEntry {
    std::chrono::steady_clock::time_point firstAttempt;
    std::chrono::steady_clock::time_point lastAttempt;
    size_t failedAttempts{0};
    bool locked{false};
    std::chrono::steady_clock::time_point lockUntil;
};

// Global login attempt tracker (thread-safe via mutex in RPCServer)
static std::unordered_map<std::string, LoginAttemptEntry> g_loginAttempts;
static std::mutex g_loginAttemptsMutex;

// Configuration for brute-force protection
constexpr size_t MAX_FAILED_ATTEMPTS = 5;              // Lock after 5 failed attempts
constexpr int LOCKOUT_DURATION_SECONDS = 300;          // 5 minute lockout
constexpr int ATTEMPT_WINDOW_SECONDS = 60;             // Count failures within 1 minute window

/**
 * Check if an IP address is currently locked out due to failed login attempts.
 */
static bool IsLockedOut(const std::string& clientAddress) {
    std::lock_guard<std::mutex> lock(g_loginAttemptsMutex);
    
    auto it = g_loginAttempts.find(clientAddress);
    if (it == g_loginAttempts.end()) {
        return false;
    }
    
    auto& entry = it->second;
    if (!entry.locked) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now >= entry.lockUntil) {
        // Lockout expired, reset
        entry.locked = false;
        entry.failedAttempts = 0;
        return false;
    }
    
    return true;
}

/**
 * Record a failed login attempt. Returns true if account is now locked.
 */
static bool RecordFailedLogin(const std::string& clientAddress) {
    std::lock_guard<std::mutex> lock(g_loginAttemptsMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto& entry = g_loginAttempts[clientAddress];
    
    // Check if we should reset the attempt window
    auto windowDuration = std::chrono::seconds(ATTEMPT_WINDOW_SECONDS);
    if (entry.failedAttempts > 0 && (now - entry.firstAttempt) >= windowDuration) {
        // Window expired, start fresh
        entry.failedAttempts = 0;
        entry.firstAttempt = now;
    }
    
    if (entry.failedAttempts == 0) {
        entry.firstAttempt = now;
    }
    
    entry.lastAttempt = now;
    entry.failedAttempts++;
    
    if (entry.failedAttempts >= MAX_FAILED_ATTEMPTS) {
        entry.locked = true;
        entry.lockUntil = now + std::chrono::seconds(LOCKOUT_DURATION_SECONDS);
        LOG_WARN(util::LogCategory::RPC) << "RPC: IP " << clientAddress 
            << " locked out for " << LOCKOUT_DURATION_SECONDS 
            << " seconds due to " << entry.failedAttempts << " failed login attempts";
        return true;
    }
    
    return false;
}

/**
 * Record a successful login (resets failed attempt counter).
 */
static void RecordSuccessfulLogin(const std::string& clientAddress) {
    std::lock_guard<std::mutex> lock(g_loginAttemptsMutex);
    g_loginAttempts.erase(clientAddress);
}

/**
 * Clean up old login attempt entries to prevent memory growth.
 */
static void CleanupLoginAttempts() {
    std::lock_guard<std::mutex> lock(g_loginAttemptsMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto maxAge = std::chrono::seconds(LOCKOUT_DURATION_SECONDS * 2);
    
    for (auto it = g_loginAttempts.begin(); it != g_loginAttempts.end();) {
        if ((now - it->second.lastAttempt) >= maxAge) {
            it = g_loginAttempts.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// JSONValue Implementation
// ============================================================================

bool JSONValue::GetBool(bool defaultValue) const {
    if (type_ == Type::Bool) return boolValue_;
    return defaultValue;
}

int64_t JSONValue::GetInt(int64_t defaultValue) const {
    if (type_ == Type::Int) return intValue_;
    if (type_ == Type::Double) return static_cast<int64_t>(doubleValue_);
    return defaultValue;
}

double JSONValue::GetDouble(double defaultValue) const {
    if (type_ == Type::Double) return doubleValue_;
    if (type_ == Type::Int) return static_cast<double>(intValue_);
    return defaultValue;
}

const std::string& JSONValue::GetString(const std::string& defaultValue) const {
    if (type_ == Type::String) return stringValue_;
    return defaultValue;
}

const JSONValue::Array& JSONValue::GetArray() const {
    if (type_ == Type::Array) return arrayValue_;
    return emptyArray_;
}

const JSONValue::Object& JSONValue::GetObject() const {
    if (type_ == Type::Object) return objectValue_;
    return emptyObject_;
}

bool JSONValue::HasKey(const std::string& key) const {
    if (type_ != Type::Object) return false;
    return objectValue_.count(key) > 0;
}

const JSONValue& JSONValue::operator[](const std::string& key) const {
    if (type_ != Type::Object) return nullValue_;
    auto it = objectValue_.find(key);
    if (it == objectValue_.end()) return nullValue_;
    return it->second;
}

JSONValue& JSONValue::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        objectValue_.clear();
    }
    return objectValue_[key];
}

size_t JSONValue::Size() const {
    if (type_ == Type::Array) return arrayValue_.size();
    if (type_ == Type::Object) return objectValue_.size();
    return 0;
}

const JSONValue& JSONValue::operator[](size_t index) const {
    if (type_ != Type::Array || index >= arrayValue_.size()) return nullValue_;
    return arrayValue_[index];
}

JSONValue& JSONValue::operator[](size_t index) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        arrayValue_.clear();
    }
    if (index >= arrayValue_.size()) {
        arrayValue_.resize(index + 1);
    }
    return arrayValue_[index];
}

void JSONValue::Push(const JSONValue& value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        arrayValue_.clear();
    }
    arrayValue_.push_back(value);
}

void JSONValue::Push(JSONValue&& value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        arrayValue_.clear();
    }
    arrayValue_.push_back(std::move(value));
}

std::string JSONValue::ToJSON(bool pretty, int indent) const {
    std::ostringstream ss;
    std::string indentStr(indent * 2, ' ');
    std::string childIndent((indent + 1) * 2, ' ');
    
    switch (type_) {
        case Type::Null:
            ss << "null";
            break;
            
        case Type::Bool:
            ss << (boolValue_ ? "true" : "false");
            break;
            
        case Type::Int:
            ss << intValue_;
            break;
            
        case Type::Double:
            ss << std::setprecision(15) << doubleValue_;
            break;
            
        case Type::String: {
            ss << '"';
            for (char c : stringValue_) {
                switch (c) {
                    case '"': ss << "\\\""; break;
                    case '\\': ss << "\\\\"; break;
                    case '\b': ss << "\\b"; break;
                    case '\f': ss << "\\f"; break;
                    case '\n': ss << "\\n"; break;
                    case '\r': ss << "\\r"; break;
                    case '\t': ss << "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 32) {
                            ss << "\\u" << std::hex << std::setw(4) 
                               << std::setfill('0') << static_cast<int>(c);
                        } else {
                            ss << c;
                        }
                }
            }
            ss << '"';
            break;
        }
        
        case Type::Array: {
            if (arrayValue_.empty()) {
                ss << "[]";
            } else if (pretty) {
                ss << "[\n";
                for (size_t i = 0; i < arrayValue_.size(); ++i) {
                    ss << childIndent << arrayValue_[i].ToJSON(true, indent + 1);
                    if (i + 1 < arrayValue_.size()) ss << ",";
                    ss << "\n";
                }
                ss << indentStr << "]";
            } else {
                ss << "[";
                for (size_t i = 0; i < arrayValue_.size(); ++i) {
                    if (i > 0) ss << ",";
                    ss << arrayValue_[i].ToJSON(false, 0);
                }
                ss << "]";
            }
            break;
        }
        
        case Type::Object: {
            if (objectValue_.empty()) {
                ss << "{}";
            } else if (pretty) {
                ss << "{\n";
                size_t i = 0;
                for (const auto& [key, value] : objectValue_) {
                    ss << childIndent << '"' << key << "\": " 
                       << value.ToJSON(true, indent + 1);
                    if (++i < objectValue_.size()) ss << ",";
                    ss << "\n";
                }
                ss << indentStr << "}";
            } else {
                ss << "{";
                size_t i = 0;
                for (const auto& [key, value] : objectValue_) {
                    if (i++ > 0) ss << ",";
                    ss << '"' << key << "\":" << value.ToJSON(false, 0);
                }
                ss << "}";
            }
            break;
        }
    }
    
    return ss.str();
}

// Simple JSON parser
JSONValue JSONValue::Parse(const std::string& json) {
    auto result = TryParse(json);
    if (!result) {
        throw std::runtime_error("JSON parse error");
    }
    return std::move(*result);
}

std::optional<JSONValue> JSONValue::TryParse(const std::string& json) {
    size_t pos = 0;
    
    auto skipWhitespace = [&]() {
        while (pos < json.size() && std::isspace(json[pos])) ++pos;
    };
    
    std::function<std::optional<JSONValue>()> parseValue;
    
    auto parseString = [&]() -> std::optional<std::string> {
        if (pos >= json.size() || json[pos] != '"') return std::nullopt;
        ++pos;
        
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') {
                if (++pos >= json.size()) return std::nullopt;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (pos + 4 >= json.size()) return std::nullopt;
                        // Simplified: just skip \uXXXX
                        pos += 4;
                        result += '?';
                        break;
                    }
                    default: return std::nullopt;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        if (pos >= json.size()) return std::nullopt;
        ++pos;  // Skip closing quote
        return result;
    };
    
    auto parseNumber = [&]() -> std::optional<JSONValue> {
        size_t start = pos;
        bool isFloat = false;
        
        if (json[pos] == '-') ++pos;
        
        while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        
        if (pos < json.size() && json[pos] == '.') {
            isFloat = true;
            ++pos;
            while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        }
        
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            isFloat = true;
            ++pos;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) ++pos;
            while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        }
        
        std::string numStr = json.substr(start, pos - start);
        try {
            if (isFloat) {
                return JSONValue(std::stod(numStr));
            } else {
                return JSONValue(std::stoll(numStr));
            }
        } catch (...) {
            return std::nullopt;
        }
    };
    
    auto parseArray = [&]() -> std::optional<JSONValue> {
        if (pos >= json.size() || json[pos] != '[') return std::nullopt;
        ++pos;
        
        Array arr;
        skipWhitespace();
        
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return JSONValue(arr);
        }
        
        while (true) {
            skipWhitespace();
            auto val = parseValue();
            if (!val) return std::nullopt;
            arr.push_back(std::move(*val));
            
            skipWhitespace();
            if (pos >= json.size()) return std::nullopt;
            
            if (json[pos] == ']') {
                ++pos;
                return JSONValue(std::move(arr));
            }
            if (json[pos] != ',') return std::nullopt;
            ++pos;
        }
    };
    
    auto parseObject = [&]() -> std::optional<JSONValue> {
        if (pos >= json.size() || json[pos] != '{') return std::nullopt;
        ++pos;
        
        Object obj;
        skipWhitespace();
        
        if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return JSONValue(obj);
        }
        
        while (true) {
            skipWhitespace();
            auto key = parseString();
            if (!key) return std::nullopt;
            
            skipWhitespace();
            if (pos >= json.size() || json[pos] != ':') return std::nullopt;
            ++pos;
            
            skipWhitespace();
            auto val = parseValue();
            if (!val) return std::nullopt;
            obj[*key] = std::move(*val);
            
            skipWhitespace();
            if (pos >= json.size()) return std::nullopt;
            
            if (json[pos] == '}') {
                ++pos;
                return JSONValue(std::move(obj));
            }
            if (json[pos] != ',') return std::nullopt;
            ++pos;
        }
    };
    
    parseValue = [&]() -> std::optional<JSONValue> {
        skipWhitespace();
        if (pos >= json.size()) return std::nullopt;
        
        char c = json[pos];
        
        if (c == 'n' && json.substr(pos, 4) == "null") {
            pos += 4;
            return JSONValue();
        }
        if (c == 't' && json.substr(pos, 4) == "true") {
            pos += 4;
            return JSONValue(true);
        }
        if (c == 'f' && json.substr(pos, 5) == "false") {
            pos += 5;
            return JSONValue(false);
        }
        if (c == '"') {
            auto str = parseString();
            if (!str) return std::nullopt;
            return JSONValue(*str);
        }
        if (c == '[') return parseArray();
        if (c == '{') return parseObject();
        if (c == '-' || std::isdigit(c)) return parseNumber();
        
        return std::nullopt;
    };
    
    auto result = parseValue();
    if (!result) return std::nullopt;
    
    skipWhitespace();
    if (pos != json.size()) return std::nullopt;  // Extra characters
    
    return result;
}

// ============================================================================
// RPCRequest Implementation
// ============================================================================

RPCRequest::RPCRequest(const std::string& method, const JSONValue& params,
                       const JSONValue& id)
    : method_(method), params_(params), id_(id) {}

const JSONValue& RPCRequest::GetParam(size_t index) const {
    if (params_.IsArray()) {
        return params_[index];
    }
    return JSONValue::Null();
}

const JSONValue& RPCRequest::GetParam(const std::string& name) const {
    if (params_.IsObject()) {
        return params_[name];
    }
    return JSONValue::Null();
}

bool RPCRequest::HasParam(const std::string& name) const {
    return params_.IsObject() && params_.HasKey(name);
}

bool RPCRequest::HasParam(size_t index) const {
    return params_.IsArray() && index < params_.Size();
}

std::string RPCRequest::ToJSON() const {
    JSONValue::Object obj;
    obj["jsonrpc"] = "2.0";
    obj["method"] = method_;
    if (!params_.IsNull()) {
        obj["params"] = params_;
    }
    if (!id_.IsNull()) {
        obj["id"] = id_;
    }
    return JSONValue(obj).ToJSON();
}

std::optional<RPCRequest> RPCRequest::Parse(const std::string& json) {
    auto parsed = JSONValue::TryParse(json);
    if (!parsed || !parsed->IsObject()) return std::nullopt;
    
    const auto& obj = *parsed;
    
    // Check JSON-RPC version
    if (!obj["jsonrpc"].IsString() || obj["jsonrpc"].GetString() != "2.0") {
        return std::nullopt;
    }
    
    // Method is required
    if (!obj["method"].IsString()) return std::nullopt;
    
    RPCRequest req;
    req.method_ = obj["method"].GetString();
    req.params_ = obj["params"];
    req.id_ = obj["id"];
    
    return req;
}

std::vector<RPCRequest> RPCRequest::ParseBatch(const std::string& json) {
    std::vector<RPCRequest> results;
    
    auto parsed = JSONValue::TryParse(json);
    if (!parsed) return results;
    
    if (parsed->IsArray()) {
        for (size_t i = 0; i < parsed->Size(); ++i) {
            const auto& item = (*parsed)[i];
            if (item.IsObject()) {
                auto req = Parse(item.ToJSON());
                if (req) {
                    results.push_back(std::move(*req));
                }
            }
        }
    } else if (parsed->IsObject()) {
        auto req = Parse(json);
        if (req) {
            results.push_back(std::move(*req));
        }
    }
    
    return results;
}

// ============================================================================
// RPCResponse Implementation
// ============================================================================

RPCResponse RPCResponse::Success(const JSONValue& result, const JSONValue& id) {
    RPCResponse resp;
    resp.isError_ = false;
    resp.result_ = result;
    resp.id_ = id;
    return resp;
}

RPCResponse RPCResponse::Error(int code, const std::string& message,
                               const JSONValue& id, const JSONValue& data) {
    RPCResponse resp;
    resp.isError_ = true;
    resp.errorCode_ = code;
    resp.errorMessage_ = message;
    resp.errorData_ = data;
    resp.id_ = id;
    return resp;
}

std::string RPCResponse::ToJSON() const {
    JSONValue::Object obj;
    obj["jsonrpc"] = "2.0";
    
    if (isError_) {
        JSONValue::Object error;
        error["code"] = errorCode_;
        error["message"] = errorMessage_;
        if (!errorData_.IsNull()) {
            error["data"] = errorData_;
        }
        obj["error"] = error;
    } else {
        obj["result"] = result_;
    }
    
    obj["id"] = id_;
    
    return JSONValue(obj).ToJSON();
}

std::string RPCResponse::BatchToJSON(const std::vector<RPCResponse>& responses) {
    if (responses.empty()) return "[]";
    if (responses.size() == 1) return responses[0].ToJSON();
    
    JSONValue::Array arr;
    for (const auto& resp : responses) {
        arr.push_back(JSONValue::Parse(resp.ToJSON()));
    }
    return JSONValue(arr).ToJSON();
}

// ============================================================================
// RPCServer Implementation
// ============================================================================

RPCServer::RPCServer() {
    startTime_ = std::chrono::steady_clock::now();
}

RPCServer::RPCServer(const RPCServerConfig& config) : config_(config) {
    startTime_ = std::chrono::steady_clock::now();
}

RPCServer::~RPCServer() {
    Stop();
}

void RPCServer::SetConfig(const RPCServerConfig& config) {
    config_ = config;
}

bool RPCServer::Start() {
    if (running_.load()) return true;
    
    // Create socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == INVALID_SOCKET_VALUE) {
        LOG_ERROR(util::LogCategory::RPC) << "Failed to create socket";
        return false;
    }
    
    // Set socket options
    int opt = 1;
#ifdef _WIN32
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    // Bind
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    
    if (config_.bindAddress == "0.0.0.0" || config_.bindAddress.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, config_.bindAddress.c_str(), &addr.sin_addr);
    }
    
    if (bind(serverSocket_, reinterpret_cast<struct sockaddr*>(&addr), 
             sizeof(addr)) < 0) {
        LOG_ERROR(util::LogCategory::RPC) << "Failed to bind to " 
            << config_.bindAddress << ":" << config_.port;
        CLOSE_SOCKET(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }
    
    // Listen
    if (listen(serverSocket_, static_cast<int>(config_.maxConnections)) < 0) {
        LOG_ERROR(util::LogCategory::RPC) << "Failed to listen on socket";
        CLOSE_SOCKET(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }
    
    // Create thread pool for handling connections
    util::ThreadPool::Config poolConfig;
    poolConfig.numThreads = config_.threadPoolSize > 0 ? config_.threadPoolSize : 4;
    poolConfig.maxQueueSize = config_.maxConnections;
    poolConfig.name = "rpc";
    poolConfig.startImmediately = true;
    threadPool_ = std::make_unique<util::ThreadPool>(poolConfig);
    
    running_.store(true);
    startTime_ = std::chrono::steady_clock::now();
    
    // Start server thread
    httpThread_ = std::thread(&RPCServer::HTTPServerThread, this);
    
    LOG_INFO(util::LogCategory::RPC) << "RPC server started on " 
        << config_.bindAddress << ":" << config_.port
        << " with " << poolConfig.numThreads << " worker threads";
    
    return true;
}

void RPCServer::Stop() {
    if (!running_.load()) return;
    
    running_.store(false);
    
    // Close server socket to interrupt accept()
    if (serverSocket_ != INVALID_SOCKET_VALUE) {
        CLOSE_SOCKET(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
    }
    
    // Wait for HTTP thread to finish
    if (httpThread_.joinable()) {
        httpThread_.join();
    }
    
    // Shutdown thread pool (waits for pending tasks to complete)
    if (threadPool_) {
        threadPool_->Stop();
        threadPool_->Wait();
        threadPool_.reset();
    }
    
    LOG_INFO(util::LogCategory::RPC) << "RPC server stopped";
}

void RPCServer::Wait() {
    if (httpThread_.joinable()) {
        httpThread_.join();
    }
}

void RPCServer::RegisterMethod(const RPCMethod& method) {
    std::lock_guard<std::mutex> lock(methodsMutex_);
    methods_[method.name] = method;
}

void RPCServer::UnregisterMethod(const std::string& name) {
    std::lock_guard<std::mutex> lock(methodsMutex_);
    methods_.erase(name);
}

bool RPCServer::HasMethod(const std::string& name) const {
    std::lock_guard<std::mutex> lock(methodsMutex_);
    return methods_.count(name) > 0;
}

std::optional<RPCMethod> RPCServer::GetMethod(const std::string& name) const {
    std::lock_guard<std::mutex> lock(methodsMutex_);
    auto it = methods_.find(name);
    if (it == methods_.end()) return std::nullopt;
    return it->second;
}

std::vector<RPCMethod> RPCServer::GetMethods() const {
    std::lock_guard<std::mutex> lock(methodsMutex_);
    std::vector<RPCMethod> result;
    for (const auto& [name, method] : methods_) {
        result.push_back(method);
    }
    return result;
}

std::vector<RPCMethod> RPCServer::GetMethodsByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(methodsMutex_);
    std::vector<RPCMethod> result;
    for (const auto& [name, method] : methods_) {
        if (method.category == category) {
            result.push_back(method);
        }
    }
    return result;
}

RPCResponse RPCServer::HandleRequest(const RPCRequest& request, const RPCContext& context) {
    ++totalRequests_;
    
    // Find method
    std::optional<RPCMethod> method;
    {
        std::lock_guard<std::mutex> lock(methodsMutex_);
        auto it = methods_.find(request.GetMethod());
        if (it != methods_.end()) {
            method = it->second;
        }
    }
    
    if (!method) {
        ++totalErrors_;
        return MethodNotFound(request.GetMethod(), request.GetId());
    }
    
    // Check authentication
    if (method->requiresAuth && context.username.empty()) {
        ++totalErrors_;
        return RPCResponse::Error(ErrorCode::UNAUTHORIZED, 
                                 "Authentication required", request.GetId());
    }
    
    // Call handler
    try {
        return method->handler(request, context);
    } catch (const std::exception& e) {
        ++totalErrors_;
        return InternalError(e.what(), request.GetId());
    }
}

std::string RPCServer::HandleRawRequest(const std::string& json, const RPCContext& context) {
    // Check rate limit
    if (config_.enableRateLimiting && !CheckRateLimit(context.clientAddress)) {
        return RPCResponse::Error(ErrorCode::RATE_LIMITED, 
                                 "Rate limit exceeded", JSONValue()).ToJSON();
    }
    
    // Try to parse as batch
    auto parsed = JSONValue::TryParse(json);
    if (!parsed) {
        return ParseError().ToJSON();
    }
    
    if (parsed->IsArray()) {
        // Batch request
        std::vector<RPCResponse> responses;
        for (size_t i = 0; i < parsed->Size(); ++i) {
            const auto& item = (*parsed)[i];
            auto req = RPCRequest::Parse(item.ToJSON());
            if (req) {
                if (!req->IsNotification()) {
                    responses.push_back(HandleRequest(*req, context));
                } else {
                    HandleRequest(*req, context);  // Process but don't respond
                }
            } else {
                responses.push_back(InvalidRequest());
            }
        }
        return RPCResponse::BatchToJSON(responses);
    } else {
        // Single request
        auto req = RPCRequest::Parse(json);
        if (!req) {
            return InvalidRequest().ToJSON();
        }
        if (req->IsNotification()) {
            HandleRequest(*req, context);
            return "";  // No response for notifications
        }
        return HandleRequest(*req, context).ToJSON();
    }
}

int64_t RPCServer::GetUptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - startTime_).count();
}

void RPCServer::HTTPServerThread() {
    while (running_.load()) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket_, 
                                  reinterpret_cast<struct sockaddr*>(&clientAddr),
                                  &clientLen);
        
        if (clientSocket < 0) {
            if (running_.load()) {
                LOG_WARN(util::LogCategory::RPC) << "Accept failed";
            }
            continue;
        }
        
        // Submit connection handling to thread pool
        if (threadPool_ && !threadPool_->TrySubmit([this, clientSocket]() {
            HandleConnection(clientSocket);
        })) {
            // Thread pool queue is full - reject the connection
            LOG_WARN(util::LogCategory::RPC) << "Thread pool queue full, rejecting connection";
            std::string response = BuildHTTPResponse(503, 
                R"({"jsonrpc":"2.0","error":{"code":-32010,"message":"Server busy"},"id":null})",
                "application/json");
            send(clientSocket, response.c_str(), response.size(), 0);
            CLOSE_SOCKET(clientSocket);
        }
    }
}

void RPCServer::HandleConnection(int clientSocket) {
    ++activeConnections_;
    
    // Read request
    std::string buffer;
    buffer.resize(config_.maxRequestSize);
    
    ssize_t bytesRead = recv(clientSocket, buffer.data(), buffer.size() - 1, 0);
    if (bytesRead <= 0) {
        CLOSE_SOCKET(clientSocket);
        --activeConnections_;
        return;
    }
    buffer.resize(bytesRead);
    
    // Parse HTTP request
    std::string body;
    std::map<std::string, std::string> headers;
    if (!ParseHTTPRequest(buffer, body, headers)) {
        std::string response = BuildHTTPResponse(400, "Bad Request", "text/plain");
        send(clientSocket, response.c_str(), response.size(), 0);
        CLOSE_SOCKET(clientSocket);
        --activeConnections_;
        return;
    }
    
    // Build context
    RPCContext ctx;
    char addrStr[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    getpeername(clientSocket, reinterpret_cast<struct sockaddr*>(&addr), &addrLen);
    inet_ntop(AF_INET, &addr.sin_addr, addrStr, sizeof(addrStr));
    ctx.clientAddress = addrStr;
    ctx.isLocal = (ctx.clientAddress == "127.0.0.1" || ctx.clientAddress == "::1");
    
    // Check authentication
    if (!config_.rpcUser.empty()) {
        // Check if this IP is locked out due to too many failed attempts
        if (IsLockedOut(ctx.clientAddress)) {
            LOG_WARN(util::LogCategory::RPC) << "RPC: Rejected request from locked out IP: " 
                << ctx.clientAddress;
            std::string response = "HTTP/1.1 403 Forbidden\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(
                          strlen(R"({"jsonrpc":"2.0","error":{"code":-32011,"message":"Too many failed login attempts. Try again later."},"id":null})")) + 
                      "\r\n\r\n"
                      R"({"jsonrpc":"2.0","error":{"code":-32011,"message":"Too many failed login attempts. Try again later."},"id":null})";
            send(clientSocket, response.c_str(), response.size(), 0);
            CLOSE_SOCKET(clientSocket);
            --activeConnections_;
            return;
        }
        
        if (!Authenticate(headers, ctx.username)) {
            // Record the failed login attempt
            bool nowLocked = RecordFailedLogin(ctx.clientAddress);
            
            std::string errorMsg = nowLocked 
                ? R"({"jsonrpc":"2.0","error":{"code":-32011,"message":"Account locked due to too many failed attempts"},"id":null})"
                : R"({"jsonrpc":"2.0","error":{"code":-32011,"message":"Unauthorized"},"id":null})";
            
            std::string response = "HTTP/1.1 401 Unauthorized\r\n"
                      "WWW-Authenticate: Basic realm=\"SHURIUM RPC\"\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(errorMsg.size()) + 
                      "\r\n\r\n" + errorMsg;
            send(clientSocket, response.c_str(), response.size(), 0);
            CLOSE_SOCKET(clientSocket);
            --activeConnections_;
            return;
        }
        
        // Successful authentication - clear any failed attempt history
        RecordSuccessfulLogin(ctx.clientAddress);
    } else {
        // No authentication configured - allow local connections as "local" user
        // This enables all RPC commands to work in development/local mode
        if (ctx.isLocal) {
            ctx.username = "__local__";
        }
    }
    
    // Handle RPC request
    std::string result = HandleRawRequest(body, ctx);
    
    // Send response
    std::string response = BuildHTTPResponse(200, result);
    send(clientSocket, response.c_str(), response.size(), 0);
    
    CLOSE_SOCKET(clientSocket);
    --activeConnections_;
}

bool RPCServer::ParseHTTPRequest(const std::string& raw, std::string& body,
                                 std::map<std::string, std::string>& headers) {
    // Find header/body separator
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        headerEnd = raw.find("\n\n");
        if (headerEnd == std::string::npos) return false;
        body = raw.substr(headerEnd + 2);
    } else {
        body = raw.substr(headerEnd + 4);
    }
    
    std::string headerSection = raw.substr(0, headerEnd);
    
    // Parse headers
    std::istringstream stream(headerSection);
    std::string line;
    
    // First line is request line
    if (!std::getline(stream, line)) return false;
    
    // Parse subsequent header lines
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // Trim whitespace
            while (!value.empty() && value.front() == ' ') value.erase(0, 1);
            while (!value.empty() && value.back() == ' ') value.pop_back();
            // Convert key to lowercase for case-insensitive lookup
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            headers[key] = value;
        }
    }
    
    return true;
}

std::string RPCServer::BuildHTTPResponse(int statusCode, const std::string& body,
                                        const std::string& contentType) {
    std::ostringstream ss;
    
    ss << "HTTP/1.1 " << statusCode << " ";
    switch (statusCode) {
        case 200: ss << "OK"; break;
        case 400: ss << "Bad Request"; break;
        case 401: ss << "Unauthorized"; break;
        case 403: ss << "Forbidden"; break;
        case 404: ss << "Not Found"; break;
        case 500: ss << "Internal Server Error"; break;
        default: ss << "Unknown"; break;
    }
    ss << "\r\n";
    
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: close\r\n";
    ss << "\r\n";
    ss << body;
    
    return ss.str();
}

bool RPCServer::Authenticate(const std::map<std::string, std::string>& headers,
                            std::string& username) {
    auto it = headers.find("authorization");
    if (it == headers.end()) return false;
    
    const std::string& auth = it->second;
    if (auth.substr(0, 6) != "Basic ") return false;
    
    // Decode base64 credentials
    std::string encoded = auth.substr(6);
    
    // Standard base64 decoding
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string decoded;
    decoded.reserve(encoded.size() * 3 / 4);  // Pre-allocate
    
    int val = 0, bits = -8;
    for (char c : encoded) {
        if (c == '=') break;  // Padding signals end of data
        if (std::isspace(static_cast<unsigned char>(c))) continue;  // Skip whitespace
        size_t pos = base64_chars.find(c);
        if (pos == std::string::npos) {
            // Invalid base64 character - reject the entire input
            return false;
        }
        val = (val << 6) + static_cast<int>(pos);
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    
    // Parse user:password
    size_t colonPos = decoded.find(':');
    if (colonPos == std::string::npos) return false;
    
    std::string user = decoded.substr(0, colonPos);
    std::string pass = decoded.substr(colonPos + 1);
    
    // Use constant-time comparison to prevent timing attacks
    // Both username and password must match
    bool userMatch = ConstantTimeCompare(user, config_.rpcUser);
    bool passMatch = ConstantTimeCompare(pass, config_.rpcPassword);
    
    if (userMatch && passMatch) {
        username = user;
        return true;
    }
    
    return false;
}

bool RPCServer::CheckRateLimit(const std::string& clientAddress) {
    std::lock_guard<std::mutex> lock(rateLimitMutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto& entry = rateLimits_[clientAddress];
    
    // Check if window has expired (1 minute)
    auto windowDuration = std::chrono::minutes(1);
    if (now - entry.windowStart >= windowDuration) {
        entry.windowStart = now;
        entry.requestCount = 0;
    }
    
    if (entry.requestCount >= config_.maxRequestsPerMinute) {
        return false;
    }
    
    ++entry.requestCount;
    return true;
}

void RPCServer::CleanupRateLimits() {
    std::lock_guard<std::mutex> lock(rateLimitMutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto windowDuration = std::chrono::minutes(5);
    
    for (auto it = rateLimits_.begin(); it != rateLimits_.end();) {
        if (now - it->second.windowStart >= windowDuration) {
            it = rateLimits_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Also cleanup old login attempt entries
    CleanupLoginAttempts();
}

// ============================================================================
// Security Public Functions
// ============================================================================

std::string GenerateRPCPassword(size_t length) {
    return GenerateSecureHex(length);
}

std::string GenerateRPCUsername(const std::string& prefix) {
    // Generate a short random suffix
    std::string suffix = GenerateSecureHex(4);
    return prefix + "_" + suffix;
}

bool GenerateRPCCookie(const std::string& cookiePath) {
    try {
        std::string username = GenerateRPCUsername("__cookie__");
        std::string password = GenerateRPCPassword(32);
        std::string cookie = username + ":" + password;
        
        // Write to file with secure permissions
        std::ofstream file(cookiePath, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            LOG_ERROR(util::LogCategory::RPC) << "Failed to create RPC cookie file: " 
                << cookiePath;
            return false;
        }
        
        file << cookie;
        file.close();
        
        // Set secure file permissions (Unix only)
#ifndef _WIN32
        if (chmod(cookiePath.c_str(), 0600) != 0) {
            LOG_WARN(util::LogCategory::RPC) << "Failed to set permissions on cookie file";
        }
#endif
        
        LOG_INFO(util::LogCategory::RPC) << "Generated RPC authentication cookie: " 
            << cookiePath;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(util::LogCategory::RPC) << "Failed to generate RPC cookie: " << e.what();
        return false;
    }
}

} // namespace rpc
} // namespace shurium
