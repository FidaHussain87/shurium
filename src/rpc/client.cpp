// SHURIUM - RPC Client Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/rpc/client.h>
#include <shurium/util/logging.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>

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
    #include <sys/un.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <poll.h>
    using socket_t = int;
    #define INVALID_SOCKET_VALUE (-1)
    #define CLOSE_SOCKET close
#endif

namespace shurium {
namespace rpc {

// ============================================================================
// RPCClient Implementation
// ============================================================================

RPCClient::RPCClient() = default;

RPCClient::RPCClient(const RPCClientConfig& config) : config_(config) {}

RPCClient::~RPCClient() {
    Disconnect();
}

void RPCClient::SetConfig(const RPCClientConfig& config) {
    config_ = config;
}

bool RPCClient::Connect() {
    std::lock_guard<std::mutex> lock(socketMutex_);
    return CreateConnection();
}

void RPCClient::Disconnect() {
    std::lock_guard<std::mutex> lock(socketMutex_);
    CloseConnection();
}

bool RPCClient::IsConnected() const {
    return socket_ != INVALID_SOCKET_VALUE;
}

bool RPCClient::Reconnect() {
    Disconnect();
    return Connect();
}

bool RPCClient::CreateConnection() {
    if (socket_ != INVALID_SOCKET_VALUE) {
        return true;  // Already connected
    }
    
    // Resolve host
    struct addrinfo hints, *result;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    std::string portStr = std::to_string(config_.port);
    
    int status = getaddrinfo(config_.host.c_str(), portStr.c_str(), &hints, &result);
    if (status != 0) {
        lastError_ = "Failed to resolve host: " + config_.host;
        lastErrorCode_ = ErrorCode::NETWORK_ERROR;
        return false;
    }
    
    // Try each address
    for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
        socket_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_ == INVALID_SOCKET_VALUE) continue;
        
        // Set timeout
#ifdef _WIN32
        DWORD timeout = config_.connectTimeout * 1000;
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, 
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = config_.connectTimeout;
        tv.tv_usec = 0;
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
        
        if (connect(socket_, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) {
            freeaddrinfo(result);
            return true;
        }
        
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET_VALUE;
    }
    
    freeaddrinfo(result);
    lastError_ = "Failed to connect to " + config_.host + ":" + portStr;
    lastErrorCode_ = ErrorCode::NETWORK_ERROR;
    return false;
}

void RPCClient::CloseConnection() {
    if (socket_ != INVALID_SOCKET_VALUE) {
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET_VALUE;
    }
}

RPCResponse RPCClient::Call(const std::string& method, const JSONValue& params) {
    auto start = std::chrono::steady_clock::now();
    ++totalCalls_;
    
    // Create request
    RPCRequest req(method, params, JSONValue(GenerateId()));
    std::string requestJson = req.ToJSON();
    
    // Build and send HTTP request
    std::string httpRequest = BuildHTTPRequest(requestJson);
    
    // Connect if needed
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        if (!IsConnected() && !CreateConnection()) {
            ++totalErrors_;
            return RPCResponse::Error(ErrorCode::NETWORK_ERROR, lastError_, req.GetId());
        }
    }
    
    // Send and receive
    std::string response;
    try {
        response = SendRequest(httpRequest);
    } catch (const std::exception& e) {
        ++totalErrors_;
        lastError_ = e.what();
        lastErrorCode_ = ErrorCode::NETWORK_ERROR;
        
        // Try reconnecting
        if (config_.autoReconnect) {
            Disconnect();
        }
        
        return RPCResponse::Error(ErrorCode::NETWORK_ERROR, lastError_, req.GetId());
    }
    
    // Parse HTTP response
    std::string body;
    int statusCode;
    if (!ParseHTTPResponse(response, body, statusCode)) {
        ++totalErrors_;
        lastError_ = "Invalid HTTP response";
        lastErrorCode_ = ErrorCode::NETWORK_ERROR;
        return RPCResponse::Error(ErrorCode::NETWORK_ERROR, lastError_, req.GetId());
    }
    
    // Parse JSON-RPC response
    auto parsed = JSONValue::TryParse(body);
    if (!parsed) {
        ++totalErrors_;
        return RPCResponse::Error(ErrorCode::PARSE_ERROR, "Invalid JSON response", req.GetId());
    }
    
    // Build response
    if (parsed->HasKey("error") && !(*parsed)["error"].IsNull()) {
        const auto& err = (*parsed)["error"];
        int code = static_cast<int>(err["code"].GetInt(ErrorCode::INTERNAL_ERROR));
        std::string message = err["message"].GetString("Unknown error");
        ++totalErrors_;
        lastErrorCode_ = code;
        lastError_ = message;
        return RPCResponse::Error(code, message, (*parsed)["id"], err["data"]);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    totalResponseTime_ += duration;
    
    return RPCResponse::Success((*parsed)["result"], (*parsed)["id"]);
}

std::vector<RPCResponse> RPCClient::BatchCall(const std::vector<RPCRequest>& requests) {
    std::vector<RPCResponse> responses;
    
    if (requests.empty()) return responses;
    
    // Build batch JSON
    JSONValue::Array arr;
    for (const auto& req : requests) {
        arr.push_back(JSONValue::Parse(req.ToJSON()));
    }
    
    std::string requestJson = JSONValue(arr).ToJSON();
    std::string httpRequest = BuildHTTPRequest(requestJson);
    
    // Connect if needed
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        if (!IsConnected() && !CreateConnection()) {
            for (const auto& req : requests) {
                responses.push_back(RPCResponse::Error(
                    ErrorCode::NETWORK_ERROR, lastError_, req.GetId()));
            }
            return responses;
        }
    }
    
    // Send and receive
    std::string response;
    try {
        response = SendRequest(httpRequest);
    } catch (const std::exception& e) {
        for (const auto& req : requests) {
            responses.push_back(RPCResponse::Error(
                ErrorCode::NETWORK_ERROR, e.what(), req.GetId()));
        }
        return responses;
    }
    
    // Parse HTTP response
    std::string body;
    int statusCode;
    if (!ParseHTTPResponse(response, body, statusCode)) {
        for (const auto& req : requests) {
            responses.push_back(RPCResponse::Error(
                ErrorCode::NETWORK_ERROR, "Invalid HTTP response", req.GetId()));
        }
        return responses;
    }
    
    // Parse JSON array response
    auto parsed = JSONValue::TryParse(body);
    if (!parsed || !parsed->IsArray()) {
        for (const auto& req : requests) {
            responses.push_back(RPCResponse::Error(
                ErrorCode::PARSE_ERROR, "Invalid batch response", req.GetId()));
        }
        return responses;
    }
    
    for (size_t i = 0; i < parsed->Size(); ++i) {
        const auto& item = (*parsed)[i];
        if (item.HasKey("error") && !item["error"].IsNull()) {
            const auto& err = item["error"];
            responses.push_back(RPCResponse::Error(
                static_cast<int>(err["code"].GetInt()),
                err["message"].GetString(),
                item["id"],
                err["data"]));
        } else {
            responses.push_back(RPCResponse::Success(item["result"], item["id"]));
        }
    }
    
    return responses;
}

std::future<RPCResponse> RPCClient::CallAsync(const std::string& method,
                                              const JSONValue& params) {
    return std::async(std::launch::async, [this, method, params]() {
        return Call(method, params);
    });
}

std::future<std::vector<RPCResponse>> RPCClient::BatchCallAsync(
    const std::vector<RPCRequest>& requests) {
    return std::async(std::launch::async, [this, requests]() {
        return BatchCall(requests);
    });
}

JSONValue RPCClient::CallForResult(const std::string& method, const JSONValue& params) {
    RPCResponse resp = Call(method, params);
    if (resp.IsError()) {
        throw std::runtime_error(resp.GetErrorMessage());
    }
    return resp.GetResult();
}

bool RPCClient::CallSuccess(const std::string& method, const JSONValue& params) {
    RPCResponse resp = Call(method, params);
    return !resp.IsError();
}

double RPCClient::GetAverageResponseTime() const {
    uint64_t calls = totalCalls_.load();
    if (calls == 0) return 0.0;
    return static_cast<double>(totalResponseTime_.load()) / static_cast<double>(calls);
}

std::string RPCClient::SendRequest(const std::string& request) {
    std::lock_guard<std::mutex> lock(socketMutex_);
    
    if (socket_ == INVALID_SOCKET_VALUE) {
        throw std::runtime_error("Not connected");
    }
    
    // Send request
    ssize_t totalSent = 0;
    while (totalSent < static_cast<ssize_t>(request.size())) {
        ssize_t sent = send(socket_, request.c_str() + totalSent,
                          static_cast<int>(request.size() - totalSent), 0);
        if (sent <= 0) {
            CloseConnection();
            throw std::runtime_error("Send failed");
        }
        totalSent += sent;
    }
    
    // Receive response
    std::string response;
    char buffer[4096];
    
    while (true) {
        ssize_t received = recv(socket_, buffer, sizeof(buffer) - 1, 0);
        if (received < 0) {
            CloseConnection();
            throw std::runtime_error("Receive failed");
        }
        if (received == 0) break;
        
        buffer[received] = '\0';
        response += buffer;
        
        // Check if we have complete response (Content-Length or chunked)
        size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            std::string headers = response.substr(0, headerEnd);
            std::string lowerHeaders = headers;
            std::transform(lowerHeaders.begin(), lowerHeaders.end(), lowerHeaders.begin(), ::tolower);
            
            // Check for chunked transfer encoding
            if (lowerHeaders.find("transfer-encoding: chunked") != std::string::npos) {
                // For chunked encoding, look for final chunk (0\r\n\r\n)
                size_t bodyStart = headerEnd + 4;
                std::string body = response.substr(bodyStart);
                // The final chunk is "0\r\n" followed by optional trailers and "\r\n"
                if (body.find("0\r\n\r\n") != std::string::npos || 
                    body.find("\r\n0\r\n\r\n") != std::string::npos) {
                    break;  // Complete chunked response
                }
            } else {
                // Find Content-Length
                size_t clPos = lowerHeaders.find("content-length:");
                if (clPos != std::string::npos) {
                    size_t clEnd = headers.find("\r\n", clPos);
                    std::string clStr = headers.substr(clPos + 15, clEnd - clPos - 15);
                    // Trim whitespace
                    while (!clStr.empty() && clStr.front() == ' ') clStr.erase(0, 1);
                    size_t contentLength = std::stoul(clStr);
                    size_t bodyStart = headerEnd + 4;
                    if (response.size() >= bodyStart + contentLength) {
                        break;  // Complete response
                    }
                } else {
                    // No Content-Length and not chunked, wait for connection close
                    break;
                }
            }
        }
    }
    
    // Close connection after each request (HTTP/1.0 style for simplicity)
    CloseConnection();
    
    return response;
}

std::string RPCClient::BuildHTTPRequest(const std::string& body) {
    std::ostringstream ss;
    
    ss << "POST / HTTP/1.1\r\n";
    ss << "Host: " << config_.host << ":" << config_.port << "\r\n";
    ss << "Content-Type: application/json\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: close\r\n";
    
    // Add authentication if configured
    if (!config_.rpcUser.empty()) {
        // Simple base64 encode
        static const char base64_chars[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string credentials = config_.rpcUser + ":" + config_.rpcPassword;
        std::string encoded;
        
        int val = 0, valb = -6;
        for (unsigned char c : credentials) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) {
            encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        while (encoded.size() % 4) {
            encoded.push_back('=');
        }
        
        ss << "Authorization: Basic " << encoded << "\r\n";
    }
    
    ss << "\r\n";
    ss << body;
    
    return ss.str();
}

bool RPCClient::ParseHTTPResponse(const std::string& raw, std::string& body, int& statusCode) {
    // Find status line
    size_t statusEnd = raw.find("\r\n");
    if (statusEnd == std::string::npos) return false;
    
    std::string statusLine = raw.substr(0, statusEnd);
    
    // Parse status code (e.g., "HTTP/1.1 200 OK")
    if (statusLine.size() < 12) return false;
    
    size_t codeStart = statusLine.find(' ');
    if (codeStart == std::string::npos) return false;
    
    try {
        statusCode = std::stoi(statusLine.substr(codeStart + 1, 3));
    } catch (...) {
        return false;
    }
    
    // Find headers section
    size_t headersEnd = raw.find("\r\n\r\n");
    size_t bodyStart;
    if (headersEnd == std::string::npos) {
        headersEnd = raw.find("\n\n");
        if (headersEnd == std::string::npos) return false;
        bodyStart = headersEnd + 2;
    } else {
        bodyStart = headersEnd + 4;
    }
    
    std::string headers = raw.substr(0, headersEnd);
    std::string lowerHeaders = headers;
    std::transform(lowerHeaders.begin(), lowerHeaders.end(), lowerHeaders.begin(), ::tolower);
    
    // Check for chunked transfer encoding
    if (lowerHeaders.find("transfer-encoding: chunked") != std::string::npos) {
        // Decode chunked body
        body.clear();
        std::string rawBody = raw.substr(bodyStart);
        size_t pos = 0;
        
        while (pos < rawBody.size()) {
            // Read chunk size (hex)
            size_t lineEnd = rawBody.find("\r\n", pos);
            if (lineEnd == std::string::npos) break;
            
            std::string sizeStr = rawBody.substr(pos, lineEnd - pos);
            // Remove any chunk extensions (after ;)
            size_t extPos = sizeStr.find(';');
            if (extPos != std::string::npos) {
                sizeStr = sizeStr.substr(0, extPos);
            }
            
            size_t chunkSize;
            try {
                chunkSize = std::stoul(sizeStr, nullptr, 16);
            } catch (...) {
                break;
            }
            
            if (chunkSize == 0) {
                // Final chunk - we're done
                break;
            }
            
            // Read chunk data
            pos = lineEnd + 2;
            if (pos + chunkSize > rawBody.size()) break;
            body += rawBody.substr(pos, chunkSize);
            pos += chunkSize;
            
            // Skip trailing \r\n
            if (pos + 2 <= rawBody.size() && rawBody.substr(pos, 2) == "\r\n") {
                pos += 2;
            }
        }
    } else {
        // Regular body
        body = raw.substr(bodyStart);
    }
    
    return true;
}

int64_t RPCClient::GenerateId() {
    return nextId_++;
}

// ============================================================================
// RPCCLIParser Implementation
// ============================================================================

RPCCLIParser::RPCCLIParser() = default;

bool RPCCLIParser::Parse(int argc, char* argv[]) {
    args_.clear();
    method_.clear();
    error_.clear();
    wantsHelp_ = false;
    wantsVersion_ = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            wantsHelp_ = true;
            return true;
        }
        if (arg == "-v" || arg == "--version") {
            wantsVersion_ = true;
            return true;
        }
        
        if (arg.substr(0, 2) == "--") {
            // Long option
            size_t eqPos = arg.find('=');
            std::string key, value;
            
            if (eqPos != std::string::npos) {
                key = arg.substr(2, eqPos - 2);
                value = arg.substr(eqPos + 1);
            } else {
                key = arg.substr(2);
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    value = argv[++i];
                }
            }
            
            if (key == "rpcconnect") host = value;
            else if (key == "rpcport") port = static_cast<uint16_t>(std::stoi(value));
            else if (key == "rpcuser") rpcUser = value;
            else if (key == "rpcpassword") rpcPassword = value;
            else if (key == "datadir") dataDir = value;
            else if (key == "stdin") useStdin = true;
            else if (key == "raw") prettyPrint = false;
        }
        else if (arg[0] == '-') {
            // Short option
            char opt = arg[1];
            std::string value;
            
            if (arg.size() > 2) {
                value = arg.substr(2);
            } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                value = argv[++i];
            }
            
            switch (opt) {
                case 'c': host = value; break;
                case 'p': port = static_cast<uint16_t>(std::stoi(value)); break;
                case 'u': rpcUser = value; break;
                case 'P': rpcPassword = value; break;
                case 'd': dataDir = value; break;
                default:
                    error_ = "Unknown option: -" + std::string(1, opt);
                    return false;
            }
        }
        else {
            // Positional argument
            if (method_.empty()) {
                method_ = arg;
            } else {
                args_.push_back(arg);
            }
        }
    }
    
    if (method_.empty() && !wantsHelp_ && !wantsVersion_) {
        error_ = "No method specified";
        return false;
    }
    
    return true;
}

JSONValue RPCCLIParser::GetParams() const {
    if (args_.empty()) return JSONValue();
    
    JSONValue::Array params;
    for (const auto& arg : args_) {
        params.push_back(ParseArg(arg));
    }
    return JSONValue(params);
}

RPCClientConfig RPCCLIParser::GetClientConfig() const {
    RPCClientConfig config;
    config.host = host;
    config.port = port;
    config.rpcUser = rpcUser;
    config.rpcPassword = rpcPassword;
    return config;
}

std::string RPCCLIParser::GetHelpText() const {
    return R"(SHURIUM RPC Client

Usage: shurium-cli [options] <method> [params...]

Options:
  -h, --help            Show this help message
  -v, --version         Show version information
  -c, --rpcconnect=HOST Connect to RPC server at HOST (default: 127.0.0.1)
  -p, --rpcport=PORT    Connect to RPC server on PORT (default: 8332)
  -u, --rpcuser=USER    RPC username
  -P, --rpcpassword=PW  RPC password
  -d, --datadir=DIR     Data directory
      --stdin           Read params from stdin
      --raw             Output raw JSON (no pretty printing)

Examples:
  shurium-cli help
  shurium-cli getblockchaininfo
  shurium-cli getblock "00000000..."
  shurium-cli sendtoaddress "NXS1..." 10.5
)";
}

JSONValue RPCCLIParser::ParseArg(const std::string& arg) const {
    // Try to parse as JSON
    auto parsed = JSONValue::TryParse(arg);
    if (parsed) return *parsed;
    
    // Check for boolean
    if (arg == "true") return JSONValue(true);
    if (arg == "false") return JSONValue(false);
    if (arg == "null") return JSONValue();
    
    // Check for number
    try {
        if (arg.find('.') != std::string::npos || 
            arg.find('e') != std::string::npos ||
            arg.find('E') != std::string::npos) {
            return JSONValue(std::stod(arg));
        }
        return JSONValue(std::stoll(arg));
    } catch (...) {
        // Not a number
    }
    
    // Return as string
    return JSONValue(arg);
}

// ============================================================================
// RPCResultFormatter Implementation
// ============================================================================

std::string RPCResultFormatter::FormatAsText(const JSONValue& value, int indent) {
    std::ostringstream ss;
    std::string pad(indent * 2, ' ');
    
    switch (value.GetType()) {
        case JSONValue::Type::Null:
            ss << pad << "(null)";
            break;
            
        case JSONValue::Type::Bool:
            ss << pad << (value.GetBool() ? "true" : "false");
            break;
            
        case JSONValue::Type::Int:
            ss << pad << value.GetInt();
            break;
            
        case JSONValue::Type::Double:
            ss << pad << value.GetDouble();
            break;
            
        case JSONValue::Type::String:
            ss << pad << value.GetString();
            break;
            
        case JSONValue::Type::Array:
            for (size_t i = 0; i < value.Size(); ++i) {
                ss << FormatAsText(value[i], indent);
                if (i + 1 < value.Size()) ss << "\n";
            }
            break;
            
        case JSONValue::Type::Object:
            for (const auto& [key, val] : value.GetObject()) {
                ss << pad << key << ": ";
                if (val.IsObject() || val.IsArray()) {
                    ss << "\n" << FormatAsText(val, indent + 1);
                } else {
                    ss << FormatAsText(val, 0);
                }
                ss << "\n";
            }
            break;
    }
    
    return ss.str();
}

std::string RPCResultFormatter::FormatAsJSON(const JSONValue& value, bool pretty) {
    return value.ToJSON(pretty);
}

std::string RPCResultFormatter::FormatError(int code, const std::string& message) {
    std::ostringstream ss;
    ss << "error: " << message << " (code " << code << ")";
    return ss.str();
}

std::string RPCResultFormatter::FormatMethodHelp(const RPCMethod& method) {
    std::ostringstream ss;
    
    ss << method.name;
    for (const auto& arg : method.argNames) {
        ss << " <" << arg << ">";
    }
    ss << "\n\n";
    ss << method.description << "\n\n";
    
    if (!method.argNames.empty()) {
        ss << "Arguments:\n";
        for (size_t i = 0; i < method.argNames.size(); ++i) {
            ss << "  " << (i + 1) << ". " << method.argNames[i];
            if (i < method.argDescriptions.size()) {
                ss << " - " << method.argDescriptions[i];
            }
            ss << "\n";
        }
    }
    
    return ss.str();
}

std::string RPCResultFormatter::FormatMethodList(const std::vector<RPCMethod>& methods) {
    std::ostringstream ss;
    
    // Group by category
    std::map<std::string, std::vector<const RPCMethod*>> byCategory;
    for (const auto& method : methods) {
        byCategory[method.category].push_back(&method);
    }
    
    for (const auto& [category, categoryMethods] : byCategory) {
        ss << "== " << category << " ==\n";
        for (const auto* method : categoryMethods) {
            ss << "  " << method->name << "\n";
        }
        ss << "\n";
    }
    
    return ss.str();
}

} // namespace rpc
} // namespace shurium
