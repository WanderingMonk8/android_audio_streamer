#include "network/udp_receiver.h"
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

using namespace AudioReceiver::Network;

class TestUdpSender {
public:
    TestUdpSender(uint16_t target_port) : target_port_(target_port) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
        socket_ = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    }
    
    ~TestUdpSender() {
#ifdef _WIN32
        closesocket(socket_);
        WSACleanup();
#else
        close(socket_);
#endif
    }
    
    bool send_packet(const AudioPacket& packet) {
        auto data = packet.serialize();
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(target_port_);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        
        int result = sendto(socket_, reinterpret_cast<const char*>(data.data()), 
                           static_cast<int>(data.size()), 0, 
                           reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        
        return result > 0;
    }

private:
    uint16_t target_port_;
#ifdef _WIN32
    SOCKET socket_;
#else
    int socket_;
#endif
};

void test_udp_receiver_construction() {
    std::cout << "Testing UDP receiver construction..." << std::endl;
    
    UdpReceiver receiver(12345);
    assert(!receiver.is_running());
    assert(receiver.get_packets_received() == 0);
    assert(receiver.get_packets_dropped() == 0);
    assert(receiver.get_bytes_received() == 0);
    
    std::cout << "✓ UDP receiver construction test passed" << std::endl;
}

void test_udp_receiver_start_stop() {
    std::cout << "Testing UDP receiver start/stop..." << std::endl;
    
    UdpReceiver receiver(12346);
    
    std::atomic<int> packets_received{0};
    std::atomic<int> errors_received{0};
    
    auto packet_cb = [&packets_received](std::unique_ptr<AudioPacket> packet) {
        packets_received++;
        // Verify packet is valid
        assert(packet != nullptr);
        assert(packet->is_valid());
    };
    
    auto error_cb = [&errors_received](const std::string& error) {
        errors_received++;
        std::cout << "Error: " << error << std::endl;
    };
    
    // Start receiver
    bool started = receiver.start(packet_cb, error_cb);
    assert(started);
    assert(receiver.is_running());
    
    // Give it a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop receiver
    receiver.stop();
    assert(!receiver.is_running());
    
    std::cout << "✓ UDP receiver start/stop test passed" << std::endl;
}

void test_udp_packet_reception() {
    std::cout << "Testing UDP packet reception..." << std::endl;
    
    const uint16_t test_port = 12347;
    UdpReceiver receiver(test_port);
    
    std::atomic<int> packets_received{0};
    std::vector<std::unique_ptr<AudioPacket>> received_packets;
    
    auto packet_cb = [&packets_received, &received_packets](std::unique_ptr<AudioPacket> packet) {
        packets_received++;
        received_packets.push_back(std::move(packet));
    };
    
    auto error_cb = [](const std::string& error) {
        std::cout << "Error: " << error << std::endl;
    };
    
    // Start receiver
    bool started = receiver.start(packet_cb, error_cb);
    assert(started);
    
    // Give receiver time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create test sender
    TestUdpSender sender(test_port);
    
    // Send test packets
    std::vector<uint8_t> test_data1 = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> test_data2 = {0x05, 0x06, 0x07, 0x08, 0x09};
    
    AudioPacket packet1(1, 1000, test_data1);
    AudioPacket packet2(2, 2000, test_data2);
    
    assert(sender.send_packet(packet1));
    assert(sender.send_packet(packet2));
    
    // Wait for packets to be received
    auto start_time = std::chrono::steady_clock::now();
    while (packets_received < 2 && 
           std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Stop receiver
    receiver.stop();
    
    // Verify packets were received
    assert(packets_received == 2);
    assert(received_packets.size() == 2);
    assert(receiver.get_packets_received() == 2);
    assert(receiver.get_bytes_received() > 0);
    
    // Verify packet contents
    bool found_packet1 = false, found_packet2 = false;
    for (const auto& packet : received_packets) {
        if (packet->sequence_id == 1 && packet->timestamp == 1000) {
            assert(packet->payload == test_data1);
            found_packet1 = true;
        } else if (packet->sequence_id == 2 && packet->timestamp == 2000) {
            assert(packet->payload == test_data2);
            found_packet2 = true;
        }
    }
    assert(found_packet1 && found_packet2);
    
    std::cout << "✓ UDP packet reception test passed" << std::endl;
}

void test_udp_invalid_packet_handling() {
    std::cout << "Testing UDP invalid packet handling..." << std::endl;
    
    const uint16_t test_port = 12348;
    UdpReceiver receiver(test_port);
    
    std::atomic<int> packets_received{0};
    std::atomic<int> errors_received{0};
    
    auto packet_cb = [&packets_received](std::unique_ptr<AudioPacket> packet) {
        packets_received++;
        assert(packet != nullptr);
        assert(packet->is_valid());
    };
    
    auto error_cb = [&errors_received](const std::string& error) {
        errors_received++;
        std::cout << "Expected error: " << error << std::endl;
    };
    
    // Start receiver
    bool started = receiver.start(packet_cb, error_cb);
    assert(started);
    
    // Give receiver time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Send invalid data directly via socket
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET test_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    int test_socket = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(test_port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    // Send invalid packet (too short)
    uint8_t invalid_data[] = {0x01, 0x02, 0x03};
    sendto(test_socket, reinterpret_cast<const char*>(invalid_data), sizeof(invalid_data), 0,
           reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    
    // Send valid packet to ensure receiver is still working
    std::vector<uint8_t> test_data = {0xAA, 0xBB};
    AudioPacket valid_packet(123, 456, test_data);
    auto valid_data = valid_packet.serialize();
    sendto(test_socket, reinterpret_cast<const char*>(valid_data.data()), 
           static_cast<int>(valid_data.size()), 0,
           reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    
#ifdef _WIN32
    closesocket(test_socket);
    WSACleanup();
#else
    close(test_socket);
#endif
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Stop receiver
    receiver.stop();
    
    // Should have received 1 valid packet, invalid packet should be dropped
    assert(packets_received == 1);
    
    std::cout << "✓ UDP invalid packet handling test passed" << std::endl;
}

void run_udp_receiver_tests() {
    std::cout << "=== Running UDP Receiver Tests ===" << std::endl;
    
    test_udp_receiver_construction();
    test_udp_receiver_start_stop();
    test_udp_packet_reception();
    test_udp_invalid_packet_handling();
    
    std::cout << "=== All UDP Receiver Tests Passed ===" << std::endl;
}