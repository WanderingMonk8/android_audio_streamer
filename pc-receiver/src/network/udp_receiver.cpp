#include "network/udp_receiver.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace AudioReceiver {
namespace Network {

#ifdef _WIN32
bool UdpReceiver::winsock_initialized_ = false;
int UdpReceiver::winsock_ref_count_ = 0;
#endif

UdpReceiver::UdpReceiver(uint16_t port) 
    : port_(port)
#ifdef _WIN32
    , socket_(INVALID_SOCKET)
#else
    , socket_(-1)
#endif
    , running_(false)
    , packets_received_(0)
    , packets_dropped_(0)
    , bytes_received_(0) {
}

UdpReceiver::~UdpReceiver() {
    stop();
    cleanup_socket();
}

bool UdpReceiver::start(PacketCallback packet_cb, ErrorCallback error_cb) {
    if (running_.load()) {
        return false;
    }
    
    packet_callback_ = packet_cb;
    error_callback_ = error_cb;
    
    if (!initialize_socket()) {
        return false;
    }
    
    running_.store(true);
    receive_thread_ = std::thread(&UdpReceiver::receive_loop, this);
    
    return true;
}

void UdpReceiver::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    
    cleanup_socket();
}

bool UdpReceiver::is_running() const {
    return running_.load();
}

uint64_t UdpReceiver::get_packets_received() const {
    return packets_received_.load();
}

uint64_t UdpReceiver::get_packets_dropped() const {
    return packets_dropped_.load();
}

uint64_t UdpReceiver::get_bytes_received() const {
    return bytes_received_.load();
}

bool UdpReceiver::initialize_socket() {
#ifdef _WIN32
    // Initialize Winsock if not already done
    if (!winsock_initialized_) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            if (error_callback_) {
                error_callback_("Failed to initialize Winsock: " + std::to_string(result));
            }
            return false;
        }
        winsock_initialized_ = true;
    }
    winsock_ref_count_++;
    
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        if (error_callback_) {
            error_callback_("Failed to create socket: " + std::to_string(WSAGetLastError()));
        }
        return false;
    }
#else
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        if (error_callback_) {
            error_callback_("Failed to create socket: " + std::string(strerror(errno)));
        }
        return false;
    }
#endif
    
    // Set socket options for low latency
#ifdef _WIN32
    DWORD timeout = 100; // 100ms timeout
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    
    // Set socket buffer sizes
    int buffer_size = 64 * 1024; // 64KB
    setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&buffer_size), sizeof(buffer_size));
#else
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    int buffer_size = 64 * 1024; // 64KB
    setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
#endif
    
    // Bind socket
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    int result = bind(socket_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (result != 0) {
#ifdef _WIN32
        if (error_callback_) {
            error_callback_("Failed to bind socket: " + std::to_string(WSAGetLastError()));
        }
#else
        if (error_callback_) {
            error_callback_("Failed to bind socket: " + std::string(strerror(errno)));
        }
#endif
        return false;
    }
    
    return true;
}

void UdpReceiver::cleanup_socket() {
#ifdef _WIN32
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    
    winsock_ref_count_--;
    if (winsock_ref_count_ <= 0 && winsock_initialized_) {
        WSACleanup();
        winsock_initialized_ = false;
        winsock_ref_count_ = 0;
    }
#else
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
#endif
}

void UdpReceiver::receive_loop() {
    uint8_t buffer[MAX_PACKET_SIZE];
    sockaddr_in sender_addr{};
    
    while (running_.load()) {
#ifdef _WIN32
        int addr_len = sizeof(sender_addr);
        int bytes_received = recvfrom(socket_, reinterpret_cast<char*>(buffer), MAX_PACKET_SIZE, 0,
                                     reinterpret_cast<sockaddr*>(&sender_addr), &addr_len);
        
        if (bytes_received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                continue; // Timeout is expected, continue loop
            }
            if (running_.load() && error_callback_) {
                error_callback_("Socket receive error: " + std::to_string(error));
            }
            break;
        }
#else
        socklen_t addr_len = sizeof(sender_addr);
        ssize_t bytes_received = recvfrom(socket_, buffer, MAX_PACKET_SIZE, 0,
                                         reinterpret_cast<sockaddr*>(&sender_addr), &addr_len);
        
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Timeout is expected, continue loop
            }
            if (running_.load() && error_callback_) {
                error_callback_("Socket receive error: " + std::string(strerror(errno)));
            }
            break;
        }
#endif
        
        if (bytes_received > 0) {
            bytes_received_.fetch_add(static_cast<uint64_t>(bytes_received));
            
            // Try to deserialize packet
            auto packet = AudioPacket::deserialize(buffer, static_cast<size_t>(bytes_received));
            if (packet) {
                packets_received_.fetch_add(1);
                if (packet_callback_) {
                    packet_callback_(std::move(packet));
                }
            } else {
                packets_dropped_.fetch_add(1);
                if (error_callback_) {
                    error_callback_("Received invalid packet, dropped");
                }
            }
        }
    }
}

} // namespace Network
} // namespace AudioReceiver