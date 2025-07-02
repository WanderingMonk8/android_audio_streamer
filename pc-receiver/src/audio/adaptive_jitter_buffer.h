#pragma once

#include "jitter_buffer.h"
#include "network/network_monitor.h"
#include <memory>
#include <chrono>
#include <atomic>

namespace AudioReceiver {
namespace Audio {

/**
 * Adaptive jitter buffer configuration
 */
struct AdaptiveJitterConfig {
    int min_capacity = 3;           // Minimum buffer size (excellent network)
    int max_capacity = 10;          // Maximum buffer size (poor network)
    int default_capacity = 5;       // Default buffer size
    
    // Adaptation parameters
    double adaptation_rate = 0.1;   // How quickly to adapt (0.0-1.0)
    std::chrono::milliseconds update_interval{500}; // How often to check network conditions
    
    // Thresholds for adaptation
    double packet_loss_threshold = 5.0;     // % packet loss to trigger increase
    uint64_t jitter_threshold_us = 10000;   // 10ms jitter threshold
    uint64_t rtt_threshold_us = 50000;      // 50ms RTT threshold
    
    // Stability parameters
    int stability_window = 10;      // Number of updates to consider for stability
    double stability_threshold = 0.2; // Maximum change rate for stability
};

/**
 * Adaptive statistics for monitoring buffer behavior
 */
struct AdaptiveStats {
    // Current state
    int current_capacity = 0;
    int target_capacity = 0;
    double adaptation_factor = 1.0;  // Current adaptation multiplier
    
    // Network-based metrics
    Network::NetworkMetrics::NetworkQuality current_network_quality;
    double current_packet_loss_rate = 0.0;
    uint64_t current_rtt_us = 0;
    uint64_t current_jitter_us = 0;
    
    // Adaptation history
    uint64_t adaptations_count = 0;
    uint64_t capacity_increases = 0;
    uint64_t capacity_decreases = 0;
    
    // Performance metrics
    double average_buffer_utilization = 0.0;
    uint64_t underruns = 0;          // Times buffer became empty
    uint64_t overruns = 0;           // Times buffer became full
    
    // Timing
    std::chrono::steady_clock::time_point last_adaptation;
    std::chrono::steady_clock::time_point last_update;
};

/**
 * Adaptive Jitter Buffer
 * Dynamically adjusts buffer size based on real-time network conditions
 * 
 * Features:
 * - Network-aware buffer sizing using NetworkMonitor
 * - Smooth adaptation to prevent audio glitches
 * - Configurable adaptation parameters
 * - Comprehensive statistics and monitoring
 * - Integration with existing JitterBuffer
 * - Thread-safe operation
 */
class AdaptiveJitterBuffer {
public:
    AdaptiveJitterBuffer(int frame_size, int channels, 
                        const AdaptiveJitterConfig& config = AdaptiveJitterConfig{});
    ~AdaptiveJitterBuffer() = default;

    // Non-copyable
    AdaptiveJitterBuffer(const AdaptiveJitterBuffer&) = delete;
    AdaptiveJitterBuffer& operator=(const AdaptiveJitterBuffer&) = delete;

    /**
     * Set network monitor for adaptive behavior
     * @param network_monitor Shared pointer to network monitor
     */
    void set_network_monitor(std::shared_ptr<Network::NetworkMonitor> network_monitor);

    /**
     * Add audio packet to buffer (same interface as JitterBuffer)
     * @param sequence_id Packet sequence number
     * @param timestamp Packet timestamp
     * @param audio_data Audio data
     * @return true if packet was added successfully
     */
    bool add_packet(uint32_t sequence_id, uint64_t timestamp, const std::vector<float>& audio_data);

    /**
     * Get next packet in sequence order (same interface as JitterBuffer)
     * @return Next audio packet or nullptr if none available
     */
    std::unique_ptr<AudioPacket> get_next_packet();

    /**
     * Update buffer size based on current network conditions
     * Called automatically, but can be called manually for immediate adaptation
     */
    void update_adaptation();

    /**
     * Force buffer capacity to a specific size
     * @param capacity New buffer capacity
     * @return true if capacity was changed successfully
     */
    bool set_capacity(int capacity);

    /**
     * Get current adaptive statistics
     * @return Current adaptive statistics
     */
    AdaptiveStats get_adaptive_stats() const;

    /**
     * Get underlying jitter buffer statistics
     * @return Jitter buffer statistics
     */
    JitterBuffer* get_jitter_buffer() const;

    /**
     * Update adaptive configuration
     * @param config New configuration
     */
    void update_config(const AdaptiveJitterConfig& config);

    /**
     * Get current configuration
     * @return Current adaptive configuration
     */
    AdaptiveJitterConfig get_config() const;

    /**
     * Reset adaptive state and statistics
     */
    void reset();

    /**
     * Buffer management (delegated to underlying jitter buffer)
     */
    void clear();
    bool is_empty() const;
    bool is_full() const;
    bool is_initialized() const;
    int get_capacity() const;
    int get_size() const;

private:
    // Core components
    std::unique_ptr<JitterBuffer> jitter_buffer_;
    std::shared_ptr<Network::NetworkMonitor> network_monitor_;
    
    // Configuration
    AdaptiveJitterConfig config_;
    int frame_size_;
    int channels_;
    
    // Adaptive state
    mutable std::mutex adaptive_mutex_;
    AdaptiveStats stats_;
    
    // Adaptation history for stability
    std::vector<int> capacity_history_;
    std::vector<double> utilization_history_;
    
    // Helper methods
    int calculate_target_capacity();
    double calculate_adaptation_factor();
    bool should_adapt() const;
    void apply_capacity_change(int new_capacity);
    void update_statistics();
    void update_utilization_stats();
    bool is_adaptation_stable() const;
    void migrate_packets_to_new_buffer(int new_capacity);
    
    // Network condition analysis
    Network::NetworkMetrics::NetworkQuality assess_network_quality();
    double get_network_adaptation_factor();
    
    // Constants
    static constexpr double MIN_ADAPTATION_FACTOR = 0.5;
    static constexpr double MAX_ADAPTATION_FACTOR = 2.0;
    static constexpr int MAX_CAPACITY_HISTORY = 20;
    static constexpr std::chrono::milliseconds MIN_ADAPTATION_INTERVAL{100};
};

} // namespace Audio
} // namespace AudioReceiver