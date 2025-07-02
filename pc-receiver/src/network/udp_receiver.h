#pragma once

#include "packet.h"
#include <functional>
#include <memory>
#include <atomic>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace AudioReceiver {
namespace Network {

/**
 * UDP receiver for audio packets
 * Implements low-latency packet reception with callback-based processing
 */
class UdpReceiver {
public:
    using PacketCallback = std::function<void(std::unique_ptr<AudioPacket>)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    UdpReceiver(uint16_t port);
    ~UdpReceiver();

    // Non-copyable
    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    // Start receiving packets
    bool start(PacketCallback packet_cb, ErrorCallback error_cb);
    
    // Stop receiving packets
    void stop();
    
    // Check if receiver is running
    bool is_running() const;
    
    // Get statistics
    uint64_t get_packets_received() const;
    uint64_t get_packets_dropped() const;
    uint64_t get_bytes_received() const;

private:
    void receive_loop();
    bool initialize_socket();
    void cleanup_socket();

    uint16_t port_;
    
#ifdef _WIN32
    SOCKET socket_;
    static bool winsock_initialized_;
    static int winsock_ref_count_;
#else
    int socket_;
#endif

    std::atomic<bool> running_;
    std::thread receive_thread_;
    
    PacketCallback packet_callback_;
    ErrorCallback error_callback_;
    
    // Statistics
    std::atomic<uint64_t> packets_received_;
    std::atomic<uint64_t> packets_dropped_;
    std::atomic<uint64_t> bytes_received_;
    
    static constexpr size_t MAX_PACKET_SIZE = 2048;
};

} // namespace Network
} // namespace AudioReceiver