#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

class TCPAudioReceiver {
public:
    TCPAudioReceiver(uint16_t port) : port_(port), running_(false) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    
    ~TCPAudioReceiver() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    bool start() {
        if (running_.load()) return false;
        
        // Create socket
#ifdef _WIN32
        server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket_ == INVALID_SOCKET) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
#else
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
#endif
        
        // Set socket options
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        
        // Bind socket
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);
        
        if (bind(server_socket_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to bind socket to port " << port_ << std::endl;
            return false;
        }
        
        // Listen
        if (listen(server_socket_, 5) < 0) {
            std::cerr << "Failed to listen on socket" << std::endl;
            return false;
        }
        
        running_.store(true);
        
        std::cout << "TCP Audio Receiver" << std::endl;
        std::cout << "==================" << std::endl;
        std::cout << "Listening on TCP port: " << port_ << std::endl;
        std::cout << "Waiting for Android connections..." << std::endl;
        
        // Start accepting connections
        accept_thread_ = std::thread(&TCPAudioReceiver::accept_loop, this);
        
        return true;
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false);
        
#ifdef _WIN32
        closesocket(server_socket_);
#else
        close(server_socket_);
#endif
        
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
    }
    
    void print_stats() {
        std::cout << "Stats - Connections: " << total_connections_ 
                  << ", Messages: " << total_messages_ 
                  << ", Bytes: " << total_bytes_ << std::endl;
    }

private:
    void accept_loop() {
        while (running_.load()) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            
#ifdef _WIN32
            SOCKET client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_len);
            if (client_socket == INVALID_SOCKET) {
                if (running_.load()) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }
#else
            int client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_len);
            if (client_socket < 0) {
                if (running_.load()) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }
#endif
            
            total_connections_++;
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            
            std::cout << "✓ Connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
            
            // Handle client in separate thread
            std::thread client_thread(&TCPAudioReceiver::handle_client, this, client_socket, std::string(client_ip));
            client_thread.detach();
        }
    }
    
    void handle_client(
#ifdef _WIN32
        SOCKET client_socket,
#else
        int client_socket,
#endif
        const std::string& client_ip) {
        
        char buffer[2048];
        
        while (running_.load()) {
#ifdef _WIN32
            int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;
#else
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;
#endif
            
            total_messages_++;
            total_bytes_ += bytes_received;
            
            std::cout << "Received from " << client_ip << ": " << bytes_received << " bytes" << std::endl;
            
            // Echo back to confirm receipt
            const char* response = "OK";
#ifdef _WIN32
            send(client_socket, response, strlen(response), 0);
#else
            send(client_socket, response, strlen(response), 0);
#endif
        }
        
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
        
        std::cout << "Connection from " << client_ip << " closed" << std::endl;
    }
    
    uint16_t port_;
    std::atomic<bool> running_;
    
#ifdef _WIN32
    SOCKET server_socket_;
#else
    int server_socket_;
#endif
    
    std::thread accept_thread_;
    
    std::atomic<int> total_connections_{0};
    std::atomic<int> total_messages_{0};
    std::atomic<int> total_bytes_{0};
};

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    
    TCPAudioReceiver receiver(port);
    
    if (!receiver.start()) {
        std::cerr << "Failed to start TCP receiver" << std::endl;
        return 1;
    }
    
    std::cout << "TCP receiver started. Press Ctrl+C to stop." << std::endl;
    
    // Print stats every 5 seconds
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        receiver.print_stats();
    }
    
    return 0;
}