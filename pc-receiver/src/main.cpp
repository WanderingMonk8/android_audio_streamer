#include "network/udp_receiver.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

using namespace AudioReceiver::Network;

std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running.store(false);
}

int main(int argc, char* argv[]) {
    std::cout << "Audio Receiver - Network Layer Test" << std::endl;
    std::cout << "====================================" << std::endl;
    
    // Parse command line arguments
    uint16_t port = 12345;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    
    std::cout << "Listening on UDP port: " << port << std::endl;
    
    // Set up signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Create UDP receiver
    UdpReceiver receiver(port);
    
    // Statistics tracking
    uint64_t last_packet_count = 0;
    auto last_stats_time = std::chrono::steady_clock::now();
    
    // Set up callbacks
    auto packet_callback = [](std::unique_ptr<AudioPacket> packet) {
        std::cout << "Received packet - Seq: " << packet->sequence_id 
                  << ", Timestamp: " << packet->timestamp 
                  << ", Payload size: " << packet->payload_size << " bytes" << std::endl;
    };
    
    auto error_callback = [](const std::string& error) {
        std::cerr << "Error: " << error << std::endl;
    };
    
    // Start receiver
    if (!receiver.start(packet_callback, error_callback)) {
        std::cerr << "Failed to start UDP receiver" << std::endl;
        return 1;
    }
    
    std::cout << "UDP receiver started. Press Ctrl+C to stop." << std::endl;
    
    // Main loop - print statistics every 5 seconds
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 5) {
            uint64_t current_packets = receiver.get_packets_received();
            uint64_t packets_per_sec = (current_packets - last_packet_count) / 5;
            
            std::cout << "Stats - Packets: " << current_packets 
                      << ", Dropped: " << receiver.get_packets_dropped()
                      << ", Bytes: " << receiver.get_bytes_received()
                      << ", Rate: " << packets_per_sec << " pkt/s" << std::endl;
            
            last_packet_count = current_packets;
            last_stats_time = now;
        }
    }
    
    std::cout << "Stopping receiver..." << std::endl;
    receiver.stop();
    
    // Final statistics
    std::cout << "Final stats - Packets: " << receiver.get_packets_received()
              << ", Dropped: " << receiver.get_packets_dropped()
              << ", Bytes: " << receiver.get_bytes_received() << std::endl;
    
    std::cout << "Audio receiver stopped." << std::endl;
    return 0;
}