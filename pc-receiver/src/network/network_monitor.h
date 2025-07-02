#pragma once

#include <cstdint>
#include <chrono>
#include <atomic>
#include <mutex>
#include <deque>

namespace AudioReceiver {
namespace Network {

/**
 * Network quality metrics for adaptive optimization
 */
struct NetworkMetrics {
    // Packet loss statistics
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t packets_lost = 0;
    double packet_loss_rate = 0.0; // Percentage (0.0 - 100.0)
    
    // Timing statistics (in microseconds)
    uint64_t min_rtt_us = 0;
    uint64_t max_rtt_us = 0;
    uint64_t avg_rtt_us = 0;
    uint64_t jitter_us = 0;
    
    // Throughput statistics
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    double throughput_mbps = 0.0;
    
    // Quality assessment
    enum class NetworkQuality {
        EXCELLENT,  // <1% loss, <5ms RTT, low jitter
        GOOD,       // 1-3% loss, 5-20ms RTT, moderate jitter
        FAIR,       // 3-10% loss, 20-50ms RTT, high jitter
        POOR        // >10% loss, >50ms RTT, very high jitter
    } quality = NetworkQuality::EXCELLENT;
    
    // Timestamp of last update
    std::chrono::steady_clock::time_point last_update;
};

/**
 * Network Quality Monitor for real-time network condition assessment
 * Tracks packet loss, RTT, jitter, and throughput for adaptive optimization
 * 
 * Features:
 * - Real-time packet loss detection
 * - RTT and jitter measurement
 * - Throughput monitoring
 * - Network quality classification
 * - Sliding window statistics
 * - Thread-safe operation
 */
class NetworkMonitor {
public:
    NetworkMonitor(size_t window_size = 100, 
                   std::chrono::milliseconds update_interval = std::chrono::milliseconds(1000));
    ~NetworkMonitor() = default;

    // Non-copyable
    NetworkMonitor(const NetworkMonitor&) = delete;
    NetworkMonitor& operator=(const NetworkMonitor&) = delete;

    /**
     * Record a sent packet
     * @param sequence_id Packet sequence number
     * @param size_bytes Packet size in bytes
     * @param timestamp Send timestamp (optional, uses current time if not provided)
     */
    void record_packet_sent(uint32_t sequence_id, size_t size_bytes, 
                           std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    /**
     * Record a received packet
     * @param sequence_id Packet sequence number
     * @param size_bytes Packet size in bytes
     * @param timestamp Receive timestamp (optional, uses current time if not provided)
     */
    void record_packet_received(uint32_t sequence_id, size_t size_bytes,
                               std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    /**
     * Record an RTT measurement
     * @param rtt_us Round-trip time in microseconds
     */
    void record_rtt(uint64_t rtt_us);

    /**
     * Get current network metrics
     * @return Current network quality metrics
     */
    NetworkMetrics get_metrics() const;

    /**
     * Get current network quality assessment
     * @return Network quality classification
     */
    NetworkMetrics::NetworkQuality get_network_quality() const;

    /**
     * Check if network conditions are suitable for high-quality audio
     * @return true if network can handle low-latency audio streaming
     */
    bool is_suitable_for_audio() const;

    /**
     * Get recommended jitter buffer size based on current conditions
     * @param min_size Minimum buffer size
     * @param max_size Maximum buffer size
     * @return Recommended buffer size in packets
     */
    size_t get_recommended_jitter_buffer_size(size_t min_size = 3, size_t max_size = 10) const;

    /**
     * Get recommended FEC redundancy level
     * @return Recommended redundancy percentage (0-50)
     */
    double get_recommended_fec_redundancy() const;

    /**
     * Reset all statistics
     */
    void reset();

    /**
     * Check if enough data has been collected for reliable metrics
     * @return true if metrics are statistically significant
     */
    bool has_sufficient_data() const;

private:
    struct PacketRecord {
        uint32_t sequence_id;
        size_t size_bytes;
        std::chrono::steady_clock::time_point timestamp;
        bool received;
    };

    // Configuration
    size_t window_size_;
    std::chrono::milliseconds update_interval_;
    
    // Thread safety
    mutable std::mutex metrics_mutex_;
    
    // Packet tracking
    std::deque<PacketRecord> sent_packets_;
    std::deque<PacketRecord> received_packets_;
    std::deque<uint64_t> rtt_samples_;
    
    // Current metrics (protected by mutex)
    NetworkMetrics current_metrics_;
    
    // Internal state
    uint32_t expected_sequence_id_;
    std::chrono::steady_clock::time_point last_update_;
    
    // Helper methods
    void update_metrics();
    void calculate_packet_loss();
    void calculate_jitter();
    void calculate_throughput();
    void classify_network_quality();
    void cleanup_old_records();
    
    // Constants for quality thresholds
    static constexpr double EXCELLENT_LOSS_THRESHOLD = 1.0;   // 1%
    static constexpr double GOOD_LOSS_THRESHOLD = 3.0;        // 3%
    static constexpr double FAIR_LOSS_THRESHOLD = 10.0;       // 10%
    
    static constexpr uint64_t EXCELLENT_RTT_THRESHOLD_US = 5000;   // 5ms
    static constexpr uint64_t GOOD_RTT_THRESHOLD_US = 20000;       // 20ms
    static constexpr uint64_t FAIR_RTT_THRESHOLD_US = 50000;       // 50ms
    
    static constexpr uint64_t EXCELLENT_JITTER_THRESHOLD_US = 1000; // 1ms
    static constexpr uint64_t GOOD_JITTER_THRESHOLD_US = 5000;      // 5ms
    static constexpr uint64_t FAIR_JITTER_THRESHOLD_US = 20000;     // 20ms
    
    static constexpr size_t MIN_SAMPLES_FOR_RELIABLE_METRICS = 10;
};

} // namespace Network
} // namespace AudioReceiver