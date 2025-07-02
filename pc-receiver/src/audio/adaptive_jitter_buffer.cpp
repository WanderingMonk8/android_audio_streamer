#include "audio/adaptive_jitter_buffer.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace AudioReceiver {
namespace Audio {

AdaptiveJitterBuffer::AdaptiveJitterBuffer(int frame_size, int channels, 
                                         const AdaptiveJitterConfig& config)
    : config_(config)
    , frame_size_(frame_size)
    , channels_(channels) {
    
    // Validate configuration
    config_.min_capacity = std::max(1, config_.min_capacity);
    config_.max_capacity = std::max(config_.min_capacity, config_.max_capacity);
    config_.default_capacity = std::clamp(config_.default_capacity, 
                                         config_.min_capacity, 
                                         config_.max_capacity);
    config_.adaptation_rate = std::clamp(config_.adaptation_rate, 0.0, 1.0);
    
    // Create initial jitter buffer with default capacity
    jitter_buffer_ = std::make_unique<JitterBuffer>(config_.default_capacity, frame_size, channels);
    
    // Initialize adaptive state
    stats_.current_capacity = config_.default_capacity;
    stats_.target_capacity = config_.default_capacity;
    stats_.adaptation_factor = 1.0;
    stats_.current_network_quality = Network::NetworkMetrics::NetworkQuality::EXCELLENT;
    stats_.last_adaptation = std::chrono::steady_clock::now();
    stats_.last_update = std::chrono::steady_clock::now();
    
    capacity_history_.reserve(MAX_CAPACITY_HISTORY);
    utilization_history_.reserve(MAX_CAPACITY_HISTORY);
}

void AdaptiveJitterBuffer::set_network_monitor(std::shared_ptr<Network::NetworkMonitor> network_monitor) {
    std::lock_guard<std::mutex> lock(adaptive_mutex_);
    network_monitor_ = network_monitor;
}

bool AdaptiveJitterBuffer::add_packet(uint32_t sequence_id, uint64_t timestamp, 
                                     const std::vector<float>& audio_data) {
    // Check if adaptation is needed (non-blocking check)
    if (should_adapt()) {
        update_adaptation();
    }
    
    // Delegate to underlying jitter buffer
    bool result = jitter_buffer_->add_packet(sequence_id, timestamp, audio_data);
    
    // Update utilization statistics
    update_utilization_stats();
    
    return result;
}

std::unique_ptr<AudioPacket> AdaptiveJitterBuffer::get_next_packet() {
    auto packet = jitter_buffer_->get_next_packet();
    
    // Update utilization statistics
    update_utilization_stats();
    
    // Check for underrun
    if (!packet && !jitter_buffer_->is_empty()) {
        std::lock_guard<std::mutex> lock(adaptive_mutex_);
        stats_.underruns++;
    }
    
    return packet;
}

void AdaptiveJitterBuffer::update_adaptation() {
    std::lock_guard<std::mutex> lock(adaptive_mutex_);
    
    if (!network_monitor_) {
        return; // No network monitor available
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_update = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats_.last_update);
    
    // Check if enough time has passed since last update
    if (time_since_last_update < config_.update_interval) {
        return;
    }
    
    // Get current network metrics
    auto network_metrics = network_monitor_->get_metrics();
    stats_.current_network_quality = network_metrics.quality;
    stats_.current_packet_loss_rate = network_metrics.packet_loss_rate;
    stats_.current_rtt_us = network_metrics.avg_rtt_us;
    stats_.current_jitter_us = network_metrics.jitter_us;
    
    // Calculate target capacity based on network conditions
    int new_target_capacity = calculate_target_capacity();
    
    // Check if adaptation is needed
    if (new_target_capacity != stats_.target_capacity) {
        stats_.target_capacity = new_target_capacity;
        
        // Apply gradual adaptation
        double adaptation_factor = calculate_adaptation_factor();
        int current_capacity = stats_.current_capacity;
        int capacity_diff = stats_.target_capacity - current_capacity;
        
        // Apply adaptation rate
        int capacity_change = static_cast<int>(capacity_diff * config_.adaptation_rate * adaptation_factor);
        int new_capacity = current_capacity + capacity_change;
        
        // Ensure we're moving towards the target
        if (capacity_diff > 0 && capacity_change <= 0) {
            capacity_change = 1; // Minimum increase
        } else if (capacity_diff < 0 && capacity_change >= 0) {
            capacity_change = -1; // Minimum decrease
        }
        
        new_capacity = current_capacity + capacity_change;
        new_capacity = std::clamp(new_capacity, config_.min_capacity, config_.max_capacity);
        
        // Apply capacity change if significant enough
        if (new_capacity != current_capacity) {
            apply_capacity_change(new_capacity);
        }
    }
    
    update_statistics();
    stats_.last_update = now;
}

bool AdaptiveJitterBuffer::set_capacity(int capacity) {
    if (capacity < config_.min_capacity || capacity > config_.max_capacity) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(adaptive_mutex_);
    apply_capacity_change(capacity);
    return true;
}

AdaptiveStats AdaptiveJitterBuffer::get_adaptive_stats() const {
    std::lock_guard<std::mutex> lock(adaptive_mutex_);
    return stats_;
}

JitterBuffer* AdaptiveJitterBuffer::get_jitter_buffer() const {
    return jitter_buffer_.get();
}

void AdaptiveJitterBuffer::update_config(const AdaptiveJitterConfig& config) {
    std::lock_guard<std::mutex> lock(adaptive_mutex_);
    
    config_ = config;
    
    // Validate new configuration
    config_.min_capacity = std::max(1, config_.min_capacity);
    config_.max_capacity = std::max(config_.min_capacity, config_.max_capacity);
    config_.default_capacity = std::clamp(config_.default_capacity, 
                                         config_.min_capacity, 
                                         config_.max_capacity);
    config_.adaptation_rate = std::clamp(config_.adaptation_rate, 0.0, 1.0);
    
    // Adjust current capacity if it's outside new bounds
    if (stats_.current_capacity < config_.min_capacity) {
        apply_capacity_change(config_.min_capacity);
    } else if (stats_.current_capacity > config_.max_capacity) {
        apply_capacity_change(config_.max_capacity);
    }
}

AdaptiveJitterConfig AdaptiveJitterBuffer::get_config() const {
    std::lock_guard<std::mutex> lock(adaptive_mutex_);
    return config_;
}

void AdaptiveJitterBuffer::reset() {
    std::lock_guard<std::mutex> lock(adaptive_mutex_);
    
    jitter_buffer_->clear();
    
    // Reset adaptive state
    stats_ = AdaptiveStats{};
    stats_.current_capacity = config_.default_capacity;
    stats_.target_capacity = config_.default_capacity;
    stats_.adaptation_factor = 1.0;
    stats_.current_network_quality = Network::NetworkMetrics::NetworkQuality::EXCELLENT;
    stats_.last_adaptation = std::chrono::steady_clock::now();
    stats_.last_update = std::chrono::steady_clock::now();
    
    capacity_history_.clear();
    utilization_history_.clear();
    
    // Recreate jitter buffer with default capacity
    jitter_buffer_ = std::make_unique<JitterBuffer>(config_.default_capacity, frame_size_, channels_);
}

void AdaptiveJitterBuffer::clear() {
    jitter_buffer_->clear();
}

bool AdaptiveJitterBuffer::is_empty() const {
    return jitter_buffer_->is_empty();
}

bool AdaptiveJitterBuffer::is_full() const {
    return jitter_buffer_->is_full();
}

bool AdaptiveJitterBuffer::is_initialized() const {
    return jitter_buffer_->is_initialized();
}

int AdaptiveJitterBuffer::get_capacity() const {
    return jitter_buffer_->get_capacity();
}

int AdaptiveJitterBuffer::get_size() const {
    return jitter_buffer_->get_size();
}

int AdaptiveJitterBuffer::calculate_target_capacity() {
    if (!network_monitor_) {
        return config_.default_capacity;
    }
    
    // Get recommended buffer size from network monitor
    size_t recommended_size = network_monitor_->get_recommended_jitter_buffer_size(
        config_.min_capacity, config_.max_capacity);
    
    // Apply additional logic based on specific conditions
    int target_capacity = static_cast<int>(recommended_size);
    
    // Adjust based on packet loss rate
    if (stats_.current_packet_loss_rate > config_.packet_loss_threshold) {
        target_capacity += static_cast<int>(stats_.current_packet_loss_rate / 5.0); // +1 per 5% loss
    }
    
    // Adjust based on jitter
    if (stats_.current_jitter_us > config_.jitter_threshold_us) {
        target_capacity += 1;
    }
    
    // Adjust based on RTT
    if (stats_.current_rtt_us > config_.rtt_threshold_us) {
        target_capacity += 1;
    }
    
    return std::clamp(target_capacity, config_.min_capacity, config_.max_capacity);
}

double AdaptiveJitterBuffer::calculate_adaptation_factor() {
    // Base adaptation factor
    double factor = 1.0;
    
    // Adjust based on network quality
    switch (stats_.current_network_quality) {
        case Network::NetworkMetrics::NetworkQuality::EXCELLENT:
            factor = 1.2; // Adapt faster for excellent network
            break;
        case Network::NetworkMetrics::NetworkQuality::GOOD:
            factor = 1.0;
            break;
        case Network::NetworkMetrics::NetworkQuality::FAIR:
            factor = 0.8; // Adapt slower for unstable network
            break;
        case Network::NetworkMetrics::NetworkQuality::POOR:
            factor = 0.6; // Adapt very slowly for poor network
            break;
    }
    
    // Adjust based on stability
    if (!is_adaptation_stable()) {
        factor *= 0.5; // Slow down adaptation if unstable
    }
    
    return std::clamp(factor, MIN_ADAPTATION_FACTOR, MAX_ADAPTATION_FACTOR);
}

bool AdaptiveJitterBuffer::should_adapt() const {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_adaptation = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats_.last_adaptation);
    
    return time_since_last_adaptation >= MIN_ADAPTATION_INTERVAL;
}

void AdaptiveJitterBuffer::apply_capacity_change(int new_capacity) {
    if (new_capacity == stats_.current_capacity) {
        return;
    }
    
    // Migrate packets to new buffer
    migrate_packets_to_new_buffer(new_capacity);
    
    // Update statistics
    if (new_capacity > stats_.current_capacity) {
        stats_.capacity_increases++;
    } else {
        stats_.capacity_decreases++;
    }
    
    stats_.current_capacity = new_capacity;
    stats_.adaptations_count++;
    stats_.last_adaptation = std::chrono::steady_clock::now();
    
    // Update capacity history
    capacity_history_.push_back(new_capacity);
    if (capacity_history_.size() > MAX_CAPACITY_HISTORY) {
        capacity_history_.erase(capacity_history_.begin());
    }
}

void AdaptiveJitterBuffer::update_statistics() {
    // Update adaptation factor
    stats_.adaptation_factor = calculate_adaptation_factor();
}

void AdaptiveJitterBuffer::update_utilization_stats() {
    if (!jitter_buffer_) return;
    
    double utilization = static_cast<double>(jitter_buffer_->get_size()) / 
                        static_cast<double>(jitter_buffer_->get_capacity());
    
    std::lock_guard<std::mutex> lock(adaptive_mutex_);
    
    // Update utilization history
    utilization_history_.push_back(utilization);
    if (utilization_history_.size() > MAX_CAPACITY_HISTORY) {
        utilization_history_.erase(utilization_history_.begin());
    }
    
    // Calculate average utilization
    if (!utilization_history_.empty()) {
        stats_.average_buffer_utilization = 
            std::accumulate(utilization_history_.begin(), utilization_history_.end(), 0.0) / 
            utilization_history_.size();
    }
    
    // Check for overrun
    if (jitter_buffer_->is_full()) {
        stats_.overruns++;
    }
}

bool AdaptiveJitterBuffer::is_adaptation_stable() const {
    if (capacity_history_.size() < config_.stability_window) {
        return true; // Not enough data, assume stable
    }
    
    // Calculate variance in recent capacity changes
    auto recent_start = capacity_history_.end() - config_.stability_window;
    double mean = std::accumulate(recent_start, capacity_history_.end(), 0.0) / config_.stability_window;
    
    double variance = 0.0;
    for (auto it = recent_start; it != capacity_history_.end(); ++it) {
        double diff = *it - mean;
        variance += diff * diff;
    }
    variance /= config_.stability_window;
    
    double std_dev = std::sqrt(variance);
    double coefficient_of_variation = (mean > 0) ? (std_dev / mean) : 0.0;
    
    return coefficient_of_variation <= config_.stability_threshold;
}

void AdaptiveJitterBuffer::migrate_packets_to_new_buffer(int new_capacity) {
    // Extract all packets from current buffer
    std::vector<std::unique_ptr<AudioPacket>> packets;
    
    while (!jitter_buffer_->is_empty()) {
        auto packet = jitter_buffer_->get_next_packet();
        if (packet) {
            packets.push_back(std::move(packet));
        }
    }
    
    // Create new buffer with new capacity
    jitter_buffer_ = std::make_unique<JitterBuffer>(new_capacity, frame_size_, channels_);
    
    // Re-add packets to new buffer
    for (auto& packet : packets) {
        jitter_buffer_->add_packet(packet->sequence_id, packet->timestamp, packet->audio_data);
    }
}

Network::NetworkMetrics::NetworkQuality AdaptiveJitterBuffer::assess_network_quality() {
    if (!network_monitor_) {
        return Network::NetworkMetrics::NetworkQuality::EXCELLENT;
    }
    
    return network_monitor_->get_network_quality();
}

double AdaptiveJitterBuffer::get_network_adaptation_factor() {
    auto quality = assess_network_quality();
    
    switch (quality) {
        case Network::NetworkMetrics::NetworkQuality::EXCELLENT:
            return 0.8; // Smaller buffer for excellent network
        case Network::NetworkMetrics::NetworkQuality::GOOD:
            return 1.0; // Normal buffer size
        case Network::NetworkMetrics::NetworkQuality::FAIR:
            return 1.3; // Larger buffer for fair network
        case Network::NetworkMetrics::NetworkQuality::POOR:
            return 1.6; // Much larger buffer for poor network
        default:
            return 1.0;
    }
}

} // namespace Audio
} // namespace AudioReceiver