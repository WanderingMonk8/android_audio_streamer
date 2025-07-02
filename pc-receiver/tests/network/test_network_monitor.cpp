#include "network/network_monitor.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

using namespace AudioReceiver::Network;

void test_network_monitor_construction() {
    std::cout << "Testing Network Monitor construction..." << std::endl;
    
    // Test default construction
    NetworkMonitor monitor1;
    auto metrics = monitor1.get_metrics();
    assert(metrics.packets_sent == 0);
    assert(metrics.packets_received == 0);
    assert(metrics.packet_loss_rate == 0.0);
    assert(metrics.quality == NetworkMetrics::NetworkQuality::EXCELLENT);
    
    // Test custom construction
    NetworkMonitor monitor2(50, std::chrono::milliseconds(500));
    auto metrics2 = monitor2.get_metrics();
    assert(metrics2.packets_sent == 0);
    
    std::cout << "✓ Network Monitor construction test passed" << std::endl;
}

void test_packet_tracking() {
    std::cout << "Testing packet tracking..." << std::endl;
    
    NetworkMonitor monitor;
    
    // Record some sent packets
    monitor.record_packet_sent(1, 100);
    monitor.record_packet_sent(2, 100);
    monitor.record_packet_sent(3, 100);
    
    auto metrics = monitor.get_metrics();
    assert(metrics.packets_sent == 3);
    assert(metrics.bytes_sent == 300);
    
    // Record some received packets
    monitor.record_packet_received(1, 100);
    monitor.record_packet_received(2, 100);
    // Skip packet 3 (simulate packet loss)
    
    metrics = monitor.get_metrics();
    assert(metrics.packets_received == 2);
    assert(metrics.bytes_received == 200);
    assert(metrics.packets_lost == 1);
    assert(metrics.packet_loss_rate > 0.0);
    
    std::cout << "✓ Packet tracking test passed" << std::endl;
}

void test_packet_loss_calculation() {
    std::cout << "Testing packet loss calculation..." << std::endl;
    
    NetworkMonitor monitor;
    
    // Send 10 packets, receive 8 (20% loss)
    for (uint32_t i = 1; i <= 10; i++) {
        monitor.record_packet_sent(i, 100);
    }
    
    for (uint32_t i = 1; i <= 8; i++) {
        monitor.record_packet_received(i, 100);
    }
    
    auto metrics = monitor.get_metrics();
    assert(metrics.packets_sent == 10);
    assert(metrics.packets_received == 8);
    assert(metrics.packets_lost == 2);
    
    // Check packet loss rate (should be 20%)
    double expected_loss_rate = 20.0;
    assert(std::abs(metrics.packet_loss_rate - expected_loss_rate) < 1.0);
    
    std::cout << "Packet loss rate: " << metrics.packet_loss_rate << "%" << std::endl;
    std::cout << "✓ Packet loss calculation test passed" << std::endl;
}

void test_rtt_measurement() {
    std::cout << "Testing RTT measurement..." << std::endl;
    
    NetworkMonitor monitor;
    
    // Record some RTT samples
    monitor.record_rtt(5000);  // 5ms
    monitor.record_rtt(10000); // 10ms
    monitor.record_rtt(15000); // 15ms
    monitor.record_rtt(8000);  // 8ms
    
    auto metrics = monitor.get_metrics();
    assert(metrics.min_rtt_us == 5000);
    assert(metrics.max_rtt_us == 15000);
    
    // Average should be around 9.5ms (9500us)
    assert(metrics.avg_rtt_us >= 8000 && metrics.avg_rtt_us <= 11000);
    
    std::cout << "Min RTT: " << metrics.min_rtt_us << "us" << std::endl;
    std::cout << "Max RTT: " << metrics.max_rtt_us << "us" << std::endl;
    std::cout << "Avg RTT: " << metrics.avg_rtt_us << "us" << std::endl;
    std::cout << "Jitter: " << metrics.jitter_us << "us" << std::endl;
    
    std::cout << "✓ RTT measurement test passed" << std::endl;
}

void test_network_quality_classification() {
    std::cout << "Testing network quality classification..." << std::endl;
    
    // Test excellent network conditions
    {
        NetworkMonitor monitor;
        
        // Send 100 packets, receive all (0% loss)
        for (uint32_t i = 1; i <= 100; i++) {
            monitor.record_packet_sent(i, 100);
            monitor.record_packet_received(i, 100);
        }
        
        // Low RTT
        for (int i = 0; i < 20; i++) {
            monitor.record_rtt(3000); // 3ms
        }
        
        auto quality = monitor.get_network_quality();
        assert(quality == NetworkMetrics::NetworkQuality::EXCELLENT);
        assert(monitor.is_suitable_for_audio());
        
        std::cout << "Excellent network quality detected" << std::endl;
    }
    
    // Test poor network conditions
    {
        NetworkMonitor monitor;
        
        // Send 100 packets, receive 80 (20% loss)
        for (uint32_t i = 1; i <= 100; i++) {
            monitor.record_packet_sent(i, 100);
            if (i <= 80) {
                monitor.record_packet_received(i, 100);
            }
        }
        
        // High RTT
        for (int i = 0; i < 20; i++) {
            monitor.record_rtt(100000); // 100ms
        }
        
        auto metrics = monitor.get_metrics();
        auto quality = monitor.get_network_quality();
        
        std::cout << "Poor network test - Packet loss: " << metrics.packet_loss_rate 
                  << "%, RTT: " << metrics.avg_rtt_us << "us, Quality: ";
        switch(quality) {
            case NetworkMetrics::NetworkQuality::EXCELLENT: std::cout << "EXCELLENT"; break;
            case NetworkMetrics::NetworkQuality::GOOD: std::cout << "GOOD"; break;
            case NetworkMetrics::NetworkQuality::FAIR: std::cout << "FAIR"; break;
            case NetworkMetrics::NetworkQuality::POOR: std::cout << "POOR"; break;
        }
        std::cout << std::endl;
        
        assert(quality == NetworkMetrics::NetworkQuality::POOR);
        assert(!monitor.is_suitable_for_audio());
        
        std::cout << "Poor network quality detected" << std::endl;
    }
    
    std::cout << "✓ Network quality classification test passed" << std::endl;
}

void test_adaptive_recommendations() {
    std::cout << "Testing adaptive recommendations..." << std::endl;
    
    // Test excellent network - should recommend minimal buffering
    {
        NetworkMonitor monitor;
        
        // Perfect network conditions
        for (uint32_t i = 1; i <= 50; i++) {
            monitor.record_packet_sent(i, 100);
            monitor.record_packet_received(i, 100);
        }
        
        for (int i = 0; i < 20; i++) {
            monitor.record_rtt(2000); // 2ms
        }
        
        size_t jitter_buffer_size = monitor.get_recommended_jitter_buffer_size();
        double fec_redundancy = monitor.get_recommended_fec_redundancy();
        
        assert(jitter_buffer_size <= 5); // Should recommend small buffer
        assert(fec_redundancy <= 10.0);  // Should recommend low redundancy
        
        std::cout << "Excellent network - Jitter buffer: " << jitter_buffer_size 
                  << ", FEC: " << fec_redundancy << "%" << std::endl;
    }
    
    // Test poor network - should recommend more buffering
    {
        NetworkMonitor monitor;
        
        // Poor network conditions
        for (uint32_t i = 1; i <= 50; i++) {
            monitor.record_packet_sent(i, 100);
            if (i % 5 != 0) { // 20% packet loss
                monitor.record_packet_received(i, 100);
            }
        }
        
        for (int i = 0; i < 20; i++) {
            monitor.record_rtt(80000); // 80ms
        }
        
        // Update metrics to ensure quality classification is current
        auto metrics = monitor.get_metrics();
        auto quality = monitor.get_network_quality();
        
        size_t jitter_buffer_size = monitor.get_recommended_jitter_buffer_size();
        double fec_redundancy = monitor.get_recommended_fec_redundancy();
        
        assert(jitter_buffer_size >= 7); // Should recommend larger buffer
        assert(fec_redundancy >= 15.0);  // Should recommend higher redundancy
        
        std::cout << "Poor network - Quality: ";
        switch(quality) {
            case NetworkMetrics::NetworkQuality::EXCELLENT: std::cout << "EXCELLENT"; break;
            case NetworkMetrics::NetworkQuality::GOOD: std::cout << "GOOD"; break;
            case NetworkMetrics::NetworkQuality::FAIR: std::cout << "FAIR"; break;
            case NetworkMetrics::NetworkQuality::POOR: std::cout << "POOR"; break;
        }
        std::cout << ", Packet loss: " << metrics.packet_loss_rate << "%, RTT: " << metrics.avg_rtt_us << "us" << std::endl;
        std::cout << "Poor network - Jitter buffer: " << jitter_buffer_size 
                  << ", FEC: " << fec_redundancy << "%" << std::endl;
    }
    
    std::cout << "✓ Adaptive recommendations test passed" << std::endl;
}

void test_throughput_calculation() {
    std::cout << "Testing throughput calculation..." << std::endl;
    
    NetworkMonitor monitor;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Send data over time to test throughput calculation
    for (uint32_t i = 1; i <= 10; i++) {
        monitor.record_packet_sent(i, 1000); // 1KB packets
        monitor.record_packet_received(i, 1000);
        
        // Small delay to simulate real timing
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto metrics = monitor.get_metrics();
    assert(metrics.bytes_sent == 10000);
    assert(metrics.bytes_received == 10000);
    
    // Throughput should be calculated (exact value depends on timing)
    std::cout << "Throughput: " << metrics.throughput_mbps << " Mbps" << std::endl;
    
    std::cout << "✓ Throughput calculation test passed" << std::endl;
}

void test_reset_functionality() {
    std::cout << "Testing reset functionality..." << std::endl;
    
    NetworkMonitor monitor;
    
    // Add some data
    monitor.record_packet_sent(1, 100);
    monitor.record_packet_received(1, 100);
    monitor.record_rtt(5000);
    
    auto metrics_before = monitor.get_metrics();
    assert(metrics_before.packets_sent > 0);
    
    // Reset
    monitor.reset();
    
    auto metrics_after = monitor.get_metrics();
    assert(metrics_after.packets_sent == 0);
    assert(metrics_after.packets_received == 0);
    assert(metrics_after.packet_loss_rate == 0.0);
    
    std::cout << "✓ Reset functionality test passed" << std::endl;
}

void test_thread_safety() {
    std::cout << "Testing thread safety..." << std::endl;
    
    NetworkMonitor monitor;
    std::atomic<bool> stop_flag{false};
    
    // Thread 1: Send packets
    std::thread sender([&monitor, &stop_flag]() {
        uint32_t seq = 1;
        while (!stop_flag.load()) {
            monitor.record_packet_sent(seq++, 100);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Thread 2: Receive packets
    std::thread receiver([&monitor, &stop_flag]() {
        uint32_t seq = 1;
        while (!stop_flag.load()) {
            monitor.record_packet_received(seq++, 100);
            std::this_thread::sleep_for(std::chrono::microseconds(150));
        }
    });
    
    // Thread 3: Read metrics
    std::thread reader([&monitor, &stop_flag]() {
        while (!stop_flag.load()) {
            auto metrics = monitor.get_metrics();
            (void)metrics; // Suppress unused variable warning
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_flag.store(true);
    
    sender.join();
    receiver.join();
    reader.join();
    
    // Verify we have some data
    auto final_metrics = monitor.get_metrics();
    assert(final_metrics.packets_sent > 0);
    
    std::cout << "✓ Thread safety test passed" << std::endl;
}

// Test runner
void run_network_monitor_tests() {
    std::cout << "\n=== Running Network Monitor Tests ===" << std::endl;
    
    test_network_monitor_construction();
    test_packet_tracking();
    test_packet_loss_calculation();
    test_rtt_measurement();
    test_network_quality_classification();
    test_adaptive_recommendations();
    test_throughput_calculation();
    test_reset_functionality();
    test_thread_safety();
    
    std::cout << "\n✅ All Network Monitor tests passed!" << std::endl;
}