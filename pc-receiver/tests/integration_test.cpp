#include "network/udp_receiver.h"
#include "network/packet.h"
#include "network/qos_manager.h"
#include "network/network_monitor.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <fstream>
#include <algorithm>

using namespace AudioReceiver::Network;

/**
 * Integration test utilities for Android → PC communication
 * These tests validate that the PC receiver can handle Android packets correctly
 */

class IntegrationTestRunner {
public:
    IntegrationTestRunner(uint16_t port = 12345) : port_(port), packets_received_(0) {}
    
    /**
     * Test basic packet reception from Android
     */
    bool test_android_packet_reception(int duration_seconds = 30) {
        std::cout << "=== Testing Android Packet Reception ===" << std::endl;
        std::cout << "Listening on port " << port_ << " for " << duration_seconds << " seconds" << std::endl;
        std::cout << "Start Android app now..." << std::endl;
        
        UdpReceiver receiver(port_);
        packets_received_ = 0;
        received_packets_.clear();
        
        auto packet_callback = [this](std::unique_ptr<AudioPacket> packet) {
            packets_received_++;
            received_packets_.push_back(*packet);
            
            std::cout << "Received packet #" << packets_received_ 
                      << " - Seq: " << packet->sequence_id 
                      << ", Timestamp: " << packet->timestamp 
                      << ", Size: " << packet->payload_size << " bytes" << std::endl;
            
            // Validate packet integrity
            if (!packet->is_valid()) {
                std::cerr << "ERROR: Invalid packet received!" << std::endl;
            }
        };
        
        auto error_callback = [](const std::string& error) {
            std::cerr << "Network error: " << error << std::endl;
        };
        
        if (!receiver.start(packet_callback, error_callback)) {
            std::cerr << "Failed to start UDP receiver" << std::endl;
            return false;
        }
        
        // Run for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
        
        receiver.stop();
        
        std::cout << "\n=== Reception Test Results ===" << std::endl;
        std::cout << "Total packets received: " << packets_received_ << std::endl;
        std::cout << "Packets dropped: " << receiver.get_packets_dropped() << std::endl;
        std::cout << "Bytes received: " << receiver.get_bytes_received() << std::endl;
        
        if (packets_received_ > 0) {
            analyze_packet_sequence();
            return true;
        } else {
            std::cout << "No packets received. Check Android app and network connectivity." << std::endl;
            return false;
        }
    }
    
    /**
     * Test packet format compatibility
     */
    bool test_packet_format_compatibility() {
        std::cout << "\n=== Testing Packet Format Compatibility ===" << std::endl;
        
        // Create test packet data that matches Android format
        std::vector<uint8_t> test_data = {
            // Sequence ID (123, little endian)
            0x7B, 0x00, 0x00, 0x00,
            // Timestamp (456789, little endian)
            0x15, 0xF6, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
            // Payload size (5, little endian)
            0x05, 0x00, 0x00, 0x00,
            // Payload
            0x01, 0x02, 0x03, 0x04, 0x05
        };
        
        auto packet = AudioPacket::deserialize(test_data.data(), test_data.size());
        
        if (!packet) {
            std::cerr << "Failed to deserialize test packet" << std::endl;
            return false;
        }
        
        std::cout << "✓ Packet deserialization successful" << std::endl;
        std::cout << "  Sequence ID: " << packet->sequence_id << " (expected: 123)" << std::endl;
        std::cout << "  Timestamp: " << packet->timestamp << " (expected: 456789)" << std::endl;
        std::cout << "  Payload size: " << packet->payload_size << " (expected: 5)" << std::endl;
        
        // Verify values
        bool success = (packet->sequence_id == 123 && 
                       packet->timestamp == 456789 && 
                       packet->payload_size == 5 &&
                       packet->payload.size() == 5);
        
        if (success) {
            std::cout << "✓ Packet format compatibility confirmed" << std::endl;
        } else {
            std::cout << "✗ Packet format mismatch detected" << std::endl;
        }
        
        return success;
    }
    
    /**
     * Test network optimization features
     */
    bool test_network_optimization_features() {
        std::cout << "\n=== Testing Network Optimization Features ===" << std::endl;
        
        // Test QoS Manager
        QoSManager qos_manager;
        std::cout << "QoS support available: " << (qos_manager.is_qos_supported() ? "Yes" : "No") << std::endl;
        
        // Test Network Monitor
        NetworkMonitor network_monitor;
        
        // Simulate some network activity
        for (uint32_t i = 1; i <= 10; i++) {
            network_monitor.record_packet_sent(i, 1000);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            network_monitor.record_packet_received(i, 1000);
            network_monitor.record_rtt(5000 + (i % 3) * 1000); // 5-8ms RTT
        }
        
        auto metrics = network_monitor.get_metrics();
        std::cout << "Network quality: ";
        switch (metrics.quality) {
            case NetworkMetrics::NetworkQuality::EXCELLENT:
                std::cout << "EXCELLENT"; break;
            case NetworkMetrics::NetworkQuality::GOOD:
                std::cout << "GOOD"; break;
            case NetworkMetrics::NetworkQuality::FAIR:
                std::cout << "FAIR"; break;
            case NetworkMetrics::NetworkQuality::POOR:
                std::cout << "POOR"; break;
        }
        std::cout << std::endl;
        
        std::cout << "Recommended jitter buffer size: " 
                  << network_monitor.get_recommended_jitter_buffer_size() << " packets" << std::endl;
        std::cout << "Recommended FEC redundancy: " 
                  << network_monitor.get_recommended_fec_redundancy() << "%" << std::endl;
        
        return true;
    }
    
    /**
     * Measure packet processing latency
     */
    void measure_packet_processing_latency() {
        std::cout << "\n=== Measuring Packet Processing Latency ===" << std::endl;
        
        const int test_count = 1000;
        std::vector<double> latencies;
        latencies.reserve(test_count);
        
        // Create test packet
        std::vector<uint8_t> payload(480 * 4); // 10ms of audio at 48kHz
        AudioPacket test_packet(1, 0, payload);
        auto serialized = test_packet.serialize();
        
        for (int i = 0; i < test_count; i++) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Deserialize packet (simulating reception)
            auto packet = AudioPacket::deserialize(serialized.data(), serialized.size());
            
            auto end = std::chrono::high_resolution_clock::now();
            
            if (packet) {
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                latencies.push_back(duration.count());
            }
        }
        
        if (!latencies.empty()) {
            double sum = 0;
            double min_latency = latencies[0];
            double max_latency = latencies[0];
            
            for (double latency : latencies) {
                sum += latency;
                min_latency = std::min(min_latency, latency);
                max_latency = std::max(max_latency, latency);
            }
            
            double avg_latency = sum / latencies.size();
            
            std::cout << "Packet processing latency statistics:" << std::endl;
            std::cout << "  Average: " << avg_latency << " μs" << std::endl;
            std::cout << "  Minimum: " << min_latency << " μs" << std::endl;
            std::cout << "  Maximum: " << max_latency << " μs" << std::endl;
            std::cout << "  Samples: " << latencies.size() << std::endl;
        }
    }
    
    /**
     * Save test results to file
     */
    void save_test_results(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return;
        }
        
        file << "Android-PC Integration Test Results\n";
        file << "===================================\n\n";
        file << "Total packets received: " << packets_received_ << "\n";
        file << "Test timestamp: " << std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() << "\n\n";
        
        file << "Received packets:\n";
        for (size_t i = 0; i < received_packets_.size() && i < 100; i++) {
            const auto& packet = received_packets_[i];
            file << "Packet " << i << ": seq=" << packet.sequence_id 
                 << ", timestamp=" << packet.timestamp 
                 << ", size=" << packet.payload_size << "\n";
        }
        
        file.close();
        std::cout << "Test results saved to: " << filename << std::endl;
    }

private:
    uint16_t port_;
    std::atomic<int> packets_received_;
    std::vector<AudioPacket> received_packets_;
    
    void analyze_packet_sequence() {
        if (received_packets_.empty()) return;
        
        std::cout << "\n=== Packet Sequence Analysis ===" << std::endl;
        
        // Check for sequence gaps
        std::vector<uint32_t> sequence_ids;
        for (const auto& packet : received_packets_) {
            sequence_ids.push_back(packet.sequence_id);
        }
        
        std::sort(sequence_ids.begin(), sequence_ids.end());
        
        int gaps = 0;
        for (size_t i = 1; i < sequence_ids.size(); i++) {
            if (sequence_ids[i] != sequence_ids[i-1] + 1) {
                gaps++;
            }
        }
        
        std::cout << "Sequence gaps detected: " << gaps << std::endl;
        
        // Check timestamp progression
        bool timestamps_increasing = true;
        for (size_t i = 1; i < received_packets_.size(); i++) {
            if (received_packets_[i].timestamp <= received_packets_[i-1].timestamp) {
                timestamps_increasing = false;
                break;
            }
        }
        
        std::cout << "Timestamps increasing: " << (timestamps_increasing ? "Yes" : "No") << std::endl;
        
        // Calculate packet rate
        if (received_packets_.size() > 1) {
            auto first_time = received_packets_.front().timestamp;
            auto last_time = received_packets_.back().timestamp;
            auto duration_ns = last_time - first_time;
            auto duration_s = duration_ns / 1000000000.0;
            auto packet_rate = received_packets_.size() / duration_s;
            
            std::cout << "Average packet rate: " << packet_rate << " packets/second" << std::endl;
        }
    }
};

// Main integration test function
int main(int argc, char* argv[]) {
    std::cout << "Android-PC Integration Test Utility" << std::endl;
    std::cout << "===================================" << std::endl;
    
    uint16_t port = 12345;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    
    IntegrationTestRunner runner(port);
    
    // Run all tests
    bool all_passed = true;
    
    // Test 1: Packet format compatibility
    if (!runner.test_packet_format_compatibility()) {
        all_passed = false;
    }
    
    // Test 2: Network optimization features
    if (!runner.test_network_optimization_features()) {
        all_passed = false;
    }
    
    // Test 3: Packet processing latency
    runner.measure_packet_processing_latency();
    
    // Test 4: Android packet reception (interactive)
    std::cout << "\nReady for Android packet reception test." << std::endl;
    std::cout << "Press Enter to start listening for Android packets..." << std::endl;
    std::cin.get();
    
    if (runner.test_android_packet_reception(30)) {
        runner.save_test_results("integration_test_results.txt");
    } else {
        all_passed = false;
    }
    
    std::cout << "\n=== Integration Test Summary ===" << std::endl;
    std::cout << "Overall result: " << (all_passed ? "PASSED" : "FAILED") << std::endl;
    
    return all_passed ? 0 : 1;
}