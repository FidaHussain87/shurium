// SHURIUM - Connection Management Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/network/connection.h>
#include <shurium/network/peer.h>
#include <shurium/core/types.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

// Platform-specific includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace shurium {

// ============================================================================
// Platform Abstraction
// ============================================================================

namespace {

#ifdef _WIN32
// Windows socket initialization
class WinsockInit {
public:
    WinsockInit() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WinsockInit() { WSACleanup(); }
};
static WinsockInit winsockInit;

inline int GetLastSocketError() { return WSAGetLastError(); }
inline void CloseSocket(SocketHandle s) { closesocket(s); }
inline bool WouldBlock(int err) { return err == WSAEWOULDBLOCK; }
inline bool InProgress(int err) { return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS; }

#else
inline int GetLastSocketError() { return errno; }
inline void CloseSocket(SocketHandle s) { close(s); }
inline bool WouldBlock(int err) { return err == EAGAIN || err == EWOULDBLOCK; }
inline bool InProgress(int err) { return err == EINPROGRESS; }
#endif

bool SetNonBlocking(SocketHandle socket) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) >= 0;
#endif
}

bool SetReuseAddr(SocketHandle socket) {
    int opt = 1;
    return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, 
                      reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0;
}

bool SetNoDelay(SocketHandle socket, bool enable) {
    int opt = enable ? 1 : 0;
    return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                      reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0;
}

bool SetKeepAlive(SocketHandle socket, bool enable, int idle, int interval, int count) {
    int opt = enable ? 1 : 0;
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0) {
        return false;
    }
    
    if (!enable) return true;
    
#ifdef __linux__
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#elif defined(__APPLE__)
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));
    // macOS doesn't support per-socket interval/count
    (void)interval;
    (void)count;
#else
    (void)idle;
    (void)interval;
    (void)count;
#endif
    return true;
}

} // anonymous namespace

// ============================================================================
// Connection Implementation
// ============================================================================

Connection::Connection(const NetService& addr, const ConnectionOptions& opts)
    : remoteAddr_(addr), options_(opts) {
    sendBuffer_.reserve(opts.writeBufferSize);
    recvBuffer_.reserve(opts.readBufferSize);
}

Connection::Connection(SocketHandle socket, const NetService& addr, const ConnectionOptions& opts)
    : socket_(socket), remoteAddr_(addr), options_(opts) {
    sendBuffer_.reserve(opts.writeBufferSize);
    recvBuffer_.reserve(opts.readBufferSize);
    state_ = ConnState::CONNECTED;
    connectTime_ = std::chrono::steady_clock::now();
    lastActivity_ = connectTime_;
}

Connection::~Connection() {
    Close(false);
}

std::unique_ptr<Connection> Connection::Create(const NetService& addr,
                                                const ConnectionOptions& opts) {
    return std::unique_ptr<Connection>(new Connection(addr, opts));
}

std::unique_ptr<Connection> Connection::FromSocket(SocketHandle socket,
                                                    const NetService& remoteAddr,
                                                    const ConnectionOptions& opts) {
    auto conn = std::unique_ptr<Connection>(new Connection(socket, remoteAddr, opts));
    conn->ConfigureSocket();
    return conn;
}

bool Connection::Connect() {
    if (state_ != ConnState::DISCONNECTED) {
        return false;
    }
    
    // Create socket
    int family = remoteAddr_.IsIPv6() ? AF_INET6 : AF_INET;
    socket_ = socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET_HANDLE) {
        return false;
    }
    
    if (!ConfigureSocket()) {
        CloseSocket(socket_);
        socket_ = INVALID_SOCKET_HANDLE;
        return false;
    }
    
    // Build address structure
    struct sockaddr_storage addr;
    socklen_t addrLen;
    std::memset(&addr, 0, sizeof(addr));
    
    if (remoteAddr_.IsIPv6()) {
        auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(remoteAddr_.GetPort());
        auto bytes = remoteAddr_.GetBytes();
        std::memcpy(&addr6->sin6_addr, bytes.data(), 16);
        addrLen = sizeof(struct sockaddr_in6);
    } else {
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(remoteAddr_.GetPort());
        auto bytes = remoteAddr_.GetBytes();
        // IPv4 is stored as-is in NetAddress (4 bytes)
        uint32_t ip4 = 0;
        if (bytes.size() >= 4) {
            ip4 = (static_cast<uint32_t>(bytes[0]) << 24) |
                  (static_cast<uint32_t>(bytes[1]) << 16) |
                  (static_cast<uint32_t>(bytes[2]) << 8) |
                  static_cast<uint32_t>(bytes[3]);
        }
        addr4->sin_addr.s_addr = htonl(ip4);
        addrLen = sizeof(struct sockaddr_in);
    }
    
    SetState(ConnState::CONNECTING);
    
    // Initiate async connect
    int result = ::connect(socket_, reinterpret_cast<struct sockaddr*>(&addr), addrLen);
    if (result == 0) {
        // Immediate success (unlikely for non-blocking)
        OnConnectComplete(true);
        return true;
    }
    
    int err = GetLastSocketError();
    if (InProgress(err)) {
        // Connection in progress - will be notified when writable
        return true;
    }
    
    // Connection failed
    OnConnectComplete(false);
    return false;
}

void Connection::Close(bool graceful) {
    if (socket_ == INVALID_SOCKET_HANDLE) {
        return;
    }
    
    if (graceful && state_ == ConnState::CONNECTED) {
        SetState(ConnState::CLOSING);
#ifndef _WIN32
        shutdown(socket_, SHUT_WR);
#else
        shutdown(socket_, SD_SEND);
#endif
    }
    
    CloseSocket(socket_);
    socket_ = INVALID_SOCKET_HANDLE;
    SetState(ConnState::DISCONNECTED);
}

NetService Connection::GetLocalAddress() const {
    if (socket_ == INVALID_SOCKET_HANDLE) {
        return NetService();
    }
    
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    if (getsockname(socket_, reinterpret_cast<struct sockaddr*>(&addr), &addrLen) != 0) {
        return NetService();
    }
    
    if (addr.ss_family == AF_INET6) {
        auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        std::array<uint8_t, 16> bytes;
        std::memcpy(bytes.data(), &addr6->sin6_addr, 16);
        return NetService(NetAddress(bytes), ntohs(addr6->sin6_port));
    } else {
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        uint32_t ip = ntohl(addr4->sin_addr.s_addr);
        std::array<uint8_t, 4> ipv4 = {
            static_cast<uint8_t>((ip >> 24) & 0xff),
            static_cast<uint8_t>((ip >> 16) & 0xff),
            static_cast<uint8_t>((ip >> 8) & 0xff),
            static_cast<uint8_t>(ip & 0xff)
        };
        return NetService(NetAddress(ipv4), ntohs(addr4->sin_port));
    }
}

bool Connection::ConfigureSocket() {
    if (!SetNonBlocking(socket_)) return false;
    if (!SetReuseAddr(socket_)) return false;
    
    if (options_.noDelay) {
        SetNoDelay(socket_, true);
    }
    
    if (options_.keepAlive) {
        SetKeepAlive(socket_, true, options_.keepAliveIdle,
                     options_.keepAliveInterval, options_.keepAliveCount);
    }
    
    if (options_.recvBufSize > 0) {
        setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&options_.recvBufSize),
                   sizeof(options_.recvBufSize));
    }
    
    if (options_.sendBufSize > 0) {
        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                   reinterpret_cast<const char*>(&options_.sendBufSize),
                   sizeof(options_.sendBufSize));
    }
    
    return true;
}

void Connection::SetState(ConnState newState) {
    ConnState oldState = state_.exchange(newState);
    if (oldState != newState && eventCallback_) {
        switch (newState) {
            case ConnState::CONNECTED:
                eventCallback_(*this, ConnEvent::CONNECTED);
                break;
            case ConnState::DISCONNECTED:
                eventCallback_(*this, ConnEvent::DISCONNECTED);
                break;
            case ConnState::ERROR:
                eventCallback_(*this, ConnEvent::ERROR);
                break;
            default:
                break;
        }
    }
}

size_t Connection::Send(const uint8_t* data, size_t len) {
    if (state_ != ConnState::CONNECTED && state_ != ConnState::CONNECTING) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    size_t space = options_.writeBufferSize - sendBuffer_.size();
    size_t toQueue = std::min(len, space);
    
    sendBuffer_.insert(sendBuffer_.end(), data, data + toQueue);
    return toQueue;
}

size_t Connection::Recv(uint8_t* buffer, size_t maxLen) {
    std::lock_guard<std::mutex> lock(recvMutex_);
    
    size_t toRead = std::min(maxLen, recvBuffer_.size());
    if (toRead > 0) {
        std::memcpy(buffer, recvBuffer_.data(), toRead);
        recvBuffer_.erase(recvBuffer_.begin(), recvBuffer_.begin() + toRead);
    }
    return toRead;
}

std::vector<uint8_t> Connection::RecvAll() {
    std::lock_guard<std::mutex> lock(recvMutex_);
    std::vector<uint8_t> result = std::move(recvBuffer_);
    recvBuffer_.clear();
    return result;
}

size_t Connection::GetSendBufferSize() const {
    std::lock_guard<std::mutex> lock(sendMutex_);
    return sendBuffer_.size() - sendOffset_;
}

size_t Connection::GetRecvBufferSize() const {
    std::lock_guard<std::mutex> lock(recvMutex_);
    return recvBuffer_.size();
}

void Connection::OnReadable() {
    if (socket_ == INVALID_SOCKET_HANDLE) return;
    
    uint8_t buffer[8192];
    while (true) {
        ssize_t n = recv(socket_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
        if (n > 0) {
            {
                std::lock_guard<std::mutex> lock(recvMutex_);
                recvBuffer_.insert(recvBuffer_.end(), buffer, buffer + n);
            }
            bytesRecv_ += n;
            lastActivity_ = std::chrono::steady_clock::now();
            
            if (dataCallback_) {
                dataCallback_(*this, buffer, n);
            }
        } else if (n == 0) {
            // Connection closed by peer
            Close(false);
            return;
        } else {
            int err = GetLastSocketError();
            if (WouldBlock(err)) {
                // No more data available
                break;
            }
            // Real error
            OnError(err);
            return;
        }
    }
    
    if (eventCallback_) {
        eventCallback_(*this, ConnEvent::DATA_RECEIVED);
    }
}

void Connection::OnWritable() {
    if (socket_ == INVALID_SOCKET_HANDLE) return;
    
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    while (sendOffset_ < sendBuffer_.size()) {
        size_t remaining = sendBuffer_.size() - sendOffset_;
        ssize_t n = send(socket_, 
                         reinterpret_cast<const char*>(sendBuffer_.data() + sendOffset_),
                         remaining, 0);
        if (n > 0) {
            sendOffset_ += n;
            bytesSent_ += n;
            lastActivity_ = std::chrono::steady_clock::now();
        } else if (n < 0) {
            int err = GetLastSocketError();
            if (WouldBlock(err)) {
                break;
            }
            OnError(err);
            return;
        }
    }
    
    // Compact buffer if we've sent a lot
    if (sendOffset_ > sendBuffer_.size() / 2) {
        sendBuffer_.erase(sendBuffer_.begin(), sendBuffer_.begin() + sendOffset_);
        sendOffset_ = 0;
    }
    
    if (eventCallback_) {
        eventCallback_(*this, ConnEvent::DATA_SENT);
    }
}

void Connection::OnError(int errorCode) {
    SetState(ConnState::ERROR);
    if (errorCallback_) {
        errorCallback_(*this, errorCode, "Socket error");
    }
    Close(false);
}

void Connection::OnConnectComplete(bool success) {
    if (success) {
        SetState(ConnState::CONNECTED);
        connectTime_ = std::chrono::steady_clock::now();
        lastActivity_ = connectTime_;
    } else {
        SetState(ConnState::ERROR);
        if (errorCallback_) {
            errorCallback_(*this, GetLastSocketError(), "Connect failed");
        }
        Close(false);
    }
}

// ============================================================================
// Listener Implementation
// ============================================================================

Listener::Listener(const NetAddress& addr, uint16_t port)
    : bindAddr_(addr), port_(port) {
}

Listener::~Listener() {
    Stop();
}

std::unique_ptr<Listener> Listener::Create(const NetAddress& bindAddr, uint16_t port) {
    return std::unique_ptr<Listener>(new Listener(bindAddr, port));
}

std::unique_ptr<Listener> Listener::Create(uint16_t port) {
    return Create(NetAddress(), port);
}

bool Listener::Start(int backlog) {
    if (listening_) return true;
    
    int family = bindAddr_.IsIPv6() ? AF_INET6 : AF_INET;
    socket_ = socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET_HANDLE) {
        return false;
    }
    
    SetReuseAddr(socket_);
    SetNonBlocking(socket_);
    
    struct sockaddr_storage addr;
    socklen_t addrLen;
    std::memset(&addr, 0, sizeof(addr));
    
    if (family == AF_INET6) {
        auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port_);
        addr6->sin6_addr = in6addr_any;
        addrLen = sizeof(struct sockaddr_in6);
    } else {
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port_);
        addr4->sin_addr.s_addr = INADDR_ANY;
        addrLen = sizeof(struct sockaddr_in);
    }
    
    if (bind(socket_, reinterpret_cast<struct sockaddr*>(&addr), addrLen) != 0) {
        CloseSocket(socket_);
        socket_ = INVALID_SOCKET_HANDLE;
        return false;
    }
    
    if (listen(socket_, backlog) != 0) {
        CloseSocket(socket_);
        socket_ = INVALID_SOCKET_HANDLE;
        return false;
    }
    
    listening_ = true;
    return true;
}

void Listener::Stop() {
    if (socket_ != INVALID_SOCKET_HANDLE) {
        CloseSocket(socket_);
        socket_ = INVALID_SOCKET_HANDLE;
    }
    listening_ = false;
}

NetService Listener::GetListenAddress() const {
    return NetService(bindAddr_, port_);
}

void Listener::OnAccept() {
    if (!listening_ || socket_ == INVALID_SOCKET_HANDLE) return;
    
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    
    SocketHandle clientSocket = accept(socket_, 
                                        reinterpret_cast<struct sockaddr*>(&addr),
                                        &addrLen);
    if (clientSocket == INVALID_SOCKET_HANDLE) {
        return;
    }
    
    // Extract remote address
    NetService remoteAddr;
    if (addr.ss_family == AF_INET6) {
        auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        std::array<uint8_t, 16> bytes;
        std::memcpy(bytes.data(), &addr6->sin6_addr, 16);
        remoteAddr = NetService(NetAddress(bytes), ntohs(addr6->sin6_port));
    } else {
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        uint32_t ip = ntohl(addr4->sin_addr.s_addr);
        std::array<uint8_t, 4> ipv4 = {
            static_cast<uint8_t>((ip >> 24) & 0xff),
            static_cast<uint8_t>((ip >> 16) & 0xff),
            static_cast<uint8_t>((ip >> 8) & 0xff),
            static_cast<uint8_t>(ip & 0xff)
        };
        remoteAddr = NetService(NetAddress(ipv4), ntohs(addr4->sin_port));
    }
    
    auto conn = Connection::FromSocket(clientSocket, remoteAddr);
    if (acceptCallback_) {
        acceptCallback_(std::move(conn));
    }
}

// ============================================================================
// EventLoop Implementation (using poll)
// ============================================================================

struct EventLoop::Impl {
    std::vector<struct pollfd> pollFds;
    std::unordered_map<SocketHandle, Connection*> connections;
    std::unordered_map<SocketHandle, Listener*> listeners;
};

EventLoop::EventLoop() : impl_(std::make_unique<Impl>()) {
    // Create wake-up pipe/socket pair
#ifdef _WIN32
    // On Windows, use loopback socket pair
    // Simplified: actual implementation would create socketpair
#else
    int fds[2];
    if (pipe(fds) == 0) {
        wakeupRecv_ = fds[0];
        wakeupSend_ = fds[1];
        SetNonBlocking(wakeupRecv_);
        SetNonBlocking(wakeupSend_);
    }
#endif
}

EventLoop::~EventLoop() {
    Stop();
    if (wakeupRecv_ != INVALID_SOCKET_HANDLE) CloseSocket(wakeupRecv_);
    if (wakeupSend_ != INVALID_SOCKET_HANDLE) CloseSocket(wakeupSend_);
}

void EventLoop::Start() {
    if (running_.exchange(true)) return;
    eventThread_ = std::thread(&EventLoop::Run, this);
}

void EventLoop::Stop() {
    if (!running_.exchange(false)) return;
    
    // Wake up the event loop
    if (wakeupSend_ != INVALID_SOCKET_HANDLE) {
        char c = 0;
        send(wakeupSend_, &c, 1, 0);
    }
    
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
}

void EventLoop::AddConnection(Connection* conn) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    impl_->connections[conn->GetSocket()] = conn;
}

void EventLoop::RemoveConnection(Connection* conn) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    impl_->connections.erase(conn->GetSocket());
}

void EventLoop::AddListener(Listener* listener) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    impl_->listeners[listener->GetSocket()] = listener;
}

void EventLoop::RemoveListener(Listener* listener) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    impl_->listeners.erase(listener->GetSocket());
}

void EventLoop::Post(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        postedCallbacks_.push(std::move(callback));
    }
    
    // Wake up event loop
    if (wakeupSend_ != INVALID_SOCKET_HANDLE) {
        char c = 0;
        send(wakeupSend_, &c, 1, 0);
    }
}

void EventLoop::Poll(int timeoutMs) {
    ProcessEvents(timeoutMs);
    ProcessPostedCallbacks();
}

void EventLoop::Run() {
    while (running_) {
        Poll(100);  // 100ms timeout for periodic wake
    }
}

void EventLoop::ProcessEvents(int timeoutMs) {
    impl_->pollFds.clear();
    
    // Add wake-up socket
    if (wakeupRecv_ != INVALID_SOCKET_HANDLE) {
        struct pollfd pfd;
        pfd.fd = wakeupRecv_;
        pfd.events = POLLIN;
        pfd.revents = 0;
        impl_->pollFds.push_back(pfd);
    }
    
    // Add listeners
    for (auto& [sock, listener] : impl_->listeners) {
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;
        pfd.revents = 0;
        impl_->pollFds.push_back(pfd);
    }
    
    // Add connections
    for (auto& [sock, conn] : impl_->connections) {
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;
        if (conn->HasPendingData() || conn->GetState() == ConnState::CONNECTING) {
            pfd.events |= POLLOUT;
        }
        pfd.revents = 0;
        impl_->pollFds.push_back(pfd);
    }
    
    if (impl_->pollFds.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        return;
    }
    
    int result = poll(impl_->pollFds.data(), impl_->pollFds.size(), timeoutMs);
    if (result <= 0) return;
    
    for (auto& pfd : impl_->pollFds) {
        if (pfd.revents == 0) continue;
        
        // Wake-up socket
        if (pfd.fd == wakeupRecv_) {
            char buf[64];
            while (recv(wakeupRecv_, buf, sizeof(buf), 0) > 0) {}
            continue;
        }
        
        // Listener
        auto listenerIt = impl_->listeners.find(pfd.fd);
        if (listenerIt != impl_->listeners.end()) {
            if (pfd.revents & POLLIN) {
                listenerIt->second->OnAccept();
            }
            continue;
        }
        
        // Connection
        auto connIt = impl_->connections.find(pfd.fd);
        if (connIt != impl_->connections.end()) {
            Connection* conn = connIt->second;
            
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                conn->OnError(0);
                continue;
            }
            
            if (conn->GetState() == ConnState::CONNECTING && (pfd.revents & POLLOUT)) {
                // Check if connect succeeded
                int err = 0;
                socklen_t errLen = sizeof(err);
                getsockopt(pfd.fd, SOL_SOCKET, SO_ERROR, 
                          reinterpret_cast<char*>(&err), &errLen);
                conn->OnConnectComplete(err == 0);
                continue;
            }
            
            if (pfd.revents & POLLIN) {
                conn->OnReadable();
            }
            if (pfd.revents & POLLOUT) {
                conn->OnWritable();
            }
        }
    }
}

void EventLoop::ProcessPostedCallbacks() {
    std::queue<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        std::swap(callbacks, postedCallbacks_);
    }
    
    while (!callbacks.empty()) {
        callbacks.front()();
        callbacks.pop();
    }
}

// ============================================================================
// ConnectionManager Implementation
// ============================================================================

ConnectionManager::ConnectionManager(const Options& opts)
    : options_(opts)
    , eventLoop_(std::make_unique<EventLoop>()) {
}

ConnectionManager::~ConnectionManager() {
    Stop();
}

bool ConnectionManager::Start() {
    if (running_.exchange(true)) return true;
    
    eventLoop_->Start();
    
    if (options_.acceptInbound) {
        listener_ = Listener::Create(options_.listenPort);
        if (listener_ && listener_->Start()) {
            listener_->SetAcceptCallback([this](std::unique_ptr<Connection> conn) {
                AcceptConnection(std::move(conn));
            });
            eventLoop_->AddListener(listener_.get());
        }
    }
    
    return true;
}

void ConnectionManager::Stop() {
    if (!running_.exchange(false)) return;
    
    DisconnectAll(DisconnectReason::MANUALLY_REQUESTED);
    
    if (listener_) {
        eventLoop_->RemoveListener(listener_.get());
        listener_->Stop();
        listener_.reset();
    }
    
    eventLoop_->Stop();
}

Peer::Id ConnectionManager::ConnectTo(const NetService& addr, ConnectionType type) {
    if (!running_) return -1;
    if (!CanAcceptConnection(false)) return -1;
    if (IsBanned(static_cast<const NetAddress&>(addr))) return -1;
    
    Peer::Id id = AllocatePeerId();
    auto conn = Connection::Create(addr);
    auto peer = (type == ConnectionType::INBOUND) 
                ? Peer::CreateInbound(id, addr)
                : Peer::CreateOutbound(id, addr, type);
    
    if (!conn->Connect()) {
        return -1;
    }
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        peers_[id] = std::move(peer);
        connections_[id] = std::move(conn);
        eventLoop_->AddConnection(connections_[id].get());
    }
    
    if (newPeerCallback_) {
        newPeerCallback_(peers_[id]);
    }
    
    return id;
}

void ConnectionManager::DisconnectPeer(Peer::Id id, DisconnectReason reason) {
    std::shared_ptr<Peer> peer;
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto peerIt = peers_.find(id);
        if (peerIt == peers_.end()) return;
        peer = peerIt->second;
        peer->Disconnect(reason);
        
        auto connIt = connections_.find(id);
        if (connIt != connections_.end()) {
            eventLoop_->RemoveConnection(connIt->second.get());
            connIt->second->Close();
            connections_.erase(connIt);
        }
        peers_.erase(peerIt);
    }
    
    HandlePeerDisconnect(id, reason);
}

void ConnectionManager::DisconnectAll(DisconnectReason reason) {
    std::vector<Peer::Id> ids;
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        for (auto& [id, peer] : peers_) {
            ids.push_back(id);
        }
    }
    
    for (auto id : ids) {
        DisconnectPeer(id, reason);
    }
}

void ConnectionManager::Ban(const NetAddress& addr, int64_t banTimeSeconds) {
    std::lock_guard<std::mutex> lock(banMutex_);
    banList_[addr] = GetTime() + banTimeSeconds;
}

void ConnectionManager::Unban(const NetAddress& addr) {
    std::lock_guard<std::mutex> lock(banMutex_);
    banList_.erase(addr);
}

bool ConnectionManager::IsBanned(const NetAddress& addr) const {
    std::lock_guard<std::mutex> lock(banMutex_);
    auto it = banList_.find(addr);
    if (it == banList_.end()) return false;
    return GetTime() < it->second;
}

std::shared_ptr<Peer> ConnectionManager::GetPeer(Peer::Id id) const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    auto it = peers_.find(id);
    return (it != peers_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Peer>> ConnectionManager::GetAllPeers() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    std::vector<std::shared_ptr<Peer>> result;
    result.reserve(peers_.size());
    for (auto& [id, peer] : peers_) {
        result.push_back(peer);
    }
    return result;
}

std::vector<std::shared_ptr<Peer>> ConnectionManager::GetPeersByType(ConnectionType type) const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    std::vector<std::shared_ptr<Peer>> result;
    for (auto& [id, peer] : peers_) {
        if (peer->GetConnectionType() == type) {
            result.push_back(peer);
        }
    }
    return result;
}

size_t ConnectionManager::GetPeerCount() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return peers_.size();
}

size_t ConnectionManager::GetInboundCount() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    size_t count = 0;
    for (auto& [id, peer] : peers_) {
        if (peer->IsInbound()) ++count;
    }
    return count;
}

size_t ConnectionManager::GetOutboundCount() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    size_t count = 0;
    for (auto& [id, peer] : peers_) {
        if (peer->IsOutbound()) ++count;
    }
    return count;
}

uint64_t ConnectionManager::GetTotalBytesSent() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    uint64_t total = 0;
    for (auto& [id, conn] : connections_) {
        total += conn->GetBytesSent();
    }
    return total;
}

uint64_t ConnectionManager::GetTotalBytesRecv() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    uint64_t total = 0;
    for (auto& [id, conn] : connections_) {
        total += conn->GetBytesRecv();
    }
    return total;
}

void ConnectionManager::AcceptConnection(std::unique_ptr<Connection> conn) {
    if (!CanAcceptConnection(true)) {
        conn->Close();
        return;
    }
    
    if (IsBanned(static_cast<const NetAddress&>(conn->GetRemoteAddress()))) {
        conn->Close();
        return;
    }
    
    Peer::Id id = AllocatePeerId();
    auto peer = Peer::CreateInbound(id, conn->GetRemoteAddress());
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        peers_[id] = std::move(peer);
        connections_[id] = std::move(conn);
        eventLoop_->AddConnection(connections_[id].get());
    }
    
    if (newPeerCallback_) {
        newPeerCallback_(peers_[id]);
    }
}

void ConnectionManager::HandlePeerDisconnect(Peer::Id id, DisconnectReason reason) {
    if (disconnectedCallback_) {
        disconnectedCallback_(id, reason);
    }
}

Peer::Id ConnectionManager::AllocatePeerId() {
    return nextPeerId_++;
}

bool ConnectionManager::CanAcceptConnection(bool inbound) const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    if (peers_.size() >= options_.maxConnections) return false;
    
    if (inbound) {
        size_t inboundCount = 0;
        for (auto& [id, peer] : peers_) {
            if (peer->IsInbound()) ++inboundCount;
        }
        return inboundCount < options_.maxInbound;
    }
    
    return true;
}

} // namespace shurium
