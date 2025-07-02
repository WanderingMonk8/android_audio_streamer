#include "network/network_monitor.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

namespace AudioReceiver {
namespace Network {

NetworkMonitor::NetworkMonitor(size_t window_size, std::chrono::milliseconds update_interval)
    : window_size_(window_size)
    , update_interval_(update_interval)
    , expected_sequence_id_(1)
    , last_update_(std::chrono::steady_clock::now()) {
    
    current_metrics_.last_update = last_update_;
}

void NetworkMonitor::record_packet_sent(uint32_t sequence_id, size_t size_bytes, 
                                       std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    PacketRecord record;
    record.sequence_id = sequence_id;
    record.size_bytes = size_bytes;
    record.timestamp = timestamp;
    record.received = false;
    
    sent_packets_.push_back(record);
    
    // Update metrics
    current_metrics_.packets_sent++;
    current_metrics_.bytes_sent += size_bytes;
    
    cleanup_old_records();
    update_metrics();
}

void NetworkMonitor::record_packet_received(uint32_t sequence_id, size_t size_bytes,
                                           std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    PacketRecord record;
    record.sequence_id = sequence_id;
    record.size_bytes = size_bytes;
    record.timestamp = timestamp;
    record.received = true;
    
    received_packets_.push_back(record);
    
    // Update metrics
    current_metrics_.packets_received++;
    current_metrics_.bytes_received += size_bytes;
    
    cleanup_old_records();
    update_metrics();
}

void NetworkMonitor::record_rtt(uint64_t rtt_us) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    rtt_samples_.push_back(rtt_us);
    
    // Keep only recent samples
    if (rtt_samples_.size() > window_size_) {
        rtt_samples_.pop_front();
    }
    
    update_metrics();
}

NetworkMetrics NetworkMonitor::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // Force update metrics for current state
    const_cast<NetworkMonitor*>(this)->calculate_packet_loss();
    const_cast<NetworkMonitor*>(this)->calculate_jitter();
    const_cast<NetworkMonitor*>(this)->classify_network_quality();
    
    
    return current_metrics_;
}

NetworkMetrics::NetworkQuality NetworkMonitor::get_network_quality() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return current_metrics_.quality;
}

bool NetworkMonitor::is_suitable_for_audio() const {
    auto quality = get_network_quality();
    return quality == NetworkMetrics::NetworkQuality::EXCELLENT || 
           quality == NetworkMetrics::NetworkQuality::GOOD;
}

size_t NetworkMonitor::get_recommended_jitter_buffer_size(size_t min_size, size_t max_size) const {
    auto quality = get_network_quality();
    auto metrics = get_metrics();
    
    size_t base_size = min_size;
    
    switch (quality) {
        case NetworkMetrics::NetworkQuality::EXCELLENT:
            base_size = min_size;
            break;
        case NetworkMetrics::NetworkQuality::GOOD:
            base_size = min_size + 1;
            break;
        case NetworkMetrics::NetworkQuality::FAIR:
            base_size = min_size + 3;
            break;
        case NetworkMetrics::NetworkQuality::POOR:
            base_size = max_size;
            break;
    }
    
    // Adjust based on jitter
    if (metrics.jitter_us > FAIR_JITTER_THRESHOLD_US) {
        base_size += 2;
    } else if (metrics.jitter_us > GOOD_JITTER_THRESHOLD_US) {
        base_size += 1;
    }
    
    return std::clamp(base_size, min_size, max_size);
}

double NetworkMonitor::get_recommended_fec_redundancy() const {
    auto quality = get_network_quality();
    auto metrics = get_metrics();
    
    double base_redundancy = 0.0;
    
    switch (quality) {
        case NetworkMetrics::NetworkQuality::EXCELLENT:
            base_redundancy = 5.0;  // 5% redundancy
            break;
        case NetworkMetrics::NetworkQuality::GOOD:
            base_redundancy = 10.0; // 10% redundancy
            break;
        case NetworkMetrics::NetworkQuality::FAIR:
            base_redundancy = 20.0; // 20% redundancy
            break;
        case NetworkMetrics::NetworkQuality::POOR:
            base_redundancy = 30.0; // 30% redundancy
            break;
    }
    
    // Adjust based on actual packet loss rate
    if (metrics.packet_loss_rate > 15.0) {
        base_redundancy += 10.0;
    } else if (metrics.packet_loss_rate > 5.0) {
        base_redundancy += 5.0;
    }
    
    return std::clamp(base_redundancy, 0.0, 50.0);
}

void NetworkMonitor::reset() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    sent_packets_.clear();
    received_packets_.clear();
    rtt_samples_.clear();
    
    current_metrics_ = NetworkMetrics{};
    current_metrics_.quality = NetworkMetrics::NetworkQuality::EXCELLENT;
    current_metrics_.last_update = std::chrono::steady_clock::now();
    
    expected_sequence_id_ = 1;
    last_update_ = std::chrono::steady_clock::now();
}

bool NetworkMonitor::has_sufficient_data() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return current_metrics_.packets_sent >= MIN_SAMPLES_FOR_RELIABLE_METRICS;
}

void NetworkMonitor::update_metrics() {
    // This method is called while holding the mutex
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_);
    
    if (time_since_update >= update_interval_) {
        calculate_packet_loss();
        calculate_jitter();
        calculate_throughput();
        classify_network_quality();
        
        current_metrics_.last_update = now;
        last_update_ = now;
    }
}

void NetworkMonitor::calculate_packet_loss() {
    if (current_metrics_.packets_sent == 0) {
        current_metrics_.packet_loss_rate = 0.0;
        current_metrics_.packets_lost = 0;
        return;
    }
    
    // Calculate packet loss based on sent vs received counts
    // This is more reliable than tracking individual packet states
    uint64_t lost_packets = current_metrics_.packets_sent - current_metrics_.packets_received;
    
    current_metrics_.packets_lost = lost_packets;
    current_metrics_.packet_loss_rate = 
        (static_cast<double>(lost_packets) / static_cast<double>(current_metrics_.packets_sent)) * 100.0;
}

void NetworkMonitor::calculate_jitter() {
    if (rtt_samples_.size() < 2) {
        current_metrics_.jitter_us = 0;
        return;
    }
    
    // Calculate RTT statistics
    auto min_it = std::min_element(rtt_samples_.begin(), rtt_samples_.end());
    auto max_it = std::max_element(rtt_samples_.begin(), rtt_samples_.end());
    
    current_metrics_.min_rtt_us = *min_it;
    current_metrics_.max_rtt_us = *max_it;
    
    uint64_t sum = std::accumulate(rtt_samples_.begin(), rtt_samples_.end(), 0ULL);
    current_metrics_.avg_rtt_us = sum / rtt_samples_.size();
    
    // Calculate jitter (standard deviation of RTT)
    double variance = 0.0;
    for (uint64_t rtt : rtt_samples_) {
        double diff = static_cast<double>(rtt) - static_cast<double>(current_metrics_.avg_rtt_us);
        variance += diff * diff;
    }
    variance /= rtt_samples_.size();
    
    current_metrics_.jitter_us = static_cast<uint64_t>(std::sqrt(variance));
}

void NetworkMonitor::calculate_throughput() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - current_metrics_.last_update);
    
    if (duration.count() > 0) {
        // Calculate throughput in Mbps
        double duration_seconds = static_cast<double>(duration.count()) / 1000000.0;
        double bits_sent = static_cast<double>(current_metrics_.bytes_sent) * 8.0;
        current_metrics_.throughput_mbps = bits_sent / duration_seconds / 1000000.0;
    }
}

void NetworkMonitor::classify_network_quality() {
    // Start with excellent and downgrade based on worst condition found
    NetworkMetrics::NetworkQuality quality = NetworkMetrics::NetworkQuality::EXCELLENT;
    
    // Check packet loss - if any condition is POOR, overall is POOR
    if (current_metrics_.packet_loss_rate > FAIR_LOSS_THRESHOLD) {
        quality = NetworkMetrics::NetworkQuality::POOR;
    } else if (current_metrics_.packet_loss_rate > GOOD_LOSS_THRESHOLD) {
        quality = NetworkMetrics::NetworkQuality::FAIR;
    } else if (current_metrics_.packet_loss_rate > EXCELLENT_LOSS_THRESHOLD) {
        quality = NetworkMetrics::NetworkQuality::GOOD;
    }
    
    // Check RTT - downgrade if worse than current
    NetworkMetrics::NetworkQuality rtt_quality = NetworkMetrics::NetworkQuality::EXCELLENT;
    if (current_metrics_.avg_rtt_us > FAIR_RTT_THRESHOLD_US) {
        rtt_quality = NetworkMetrics::NetworkQuality::POOR;
    } else if (current_metrics_.avg_rtt_us > GOOD_RTT_THRESHOLD_US) {
        rtt_quality = NetworkMetrics::NetworkQuality::FAIR;
    } else if (current_metrics_.avg_rtt_us > EXCELLENT_RTT_THRESHOLD_US) {
        rtt_quality = NetworkMetrics::NetworkQuality::GOOD;
    }
    
    // Take the worse of packet loss and RTT quality
    if (rtt_quality == NetworkMetrics::NetworkQuality::POOR || quality == NetworkMetrics::NetworkQuality::POOR) {
        quality = NetworkMetrics::NetworkQuality::POOR;
    } else if (rtt_quality == NetworkMetrics::NetworkQuality::FAIR || quality == NetworkMetrics::NetworkQuality::FAIR) {
        quality = NetworkMetrics::NetworkQuality::FAIR;
    } else if (rtt_quality == NetworkMetrics::NetworkQuality::GOOD || quality == NetworkMetrics::NetworkQuality::GOOD) {
        quality = NetworkMetrics::NetworkQuality::GOOD;
    }
    
    // Check jitter - downgrade if worse than current
    NetworkMetrics::NetworkQuality jitter_quality = NetworkMetrics::NetworkQuality::EXCELLENT;
    if (current_metrics_.jitter_us > FAIR_JITTER_THRESHOLD_US) {
        jitter_quality = NetworkMetrics::NetworkQuality::POOR;
    } else if (current_metrics_.jitter_us > GOOD_JITTER_THRESHOLD_US) {
        jitter_quality = NetworkMetrics::NetworkQuality::FAIR;
    } else if (current_metrics_.jitter_us > EXCELLENT_JITTER_THRESHOLD_US) {
        jitter_quality = NetworkMetrics::NetworkQuality::GOOD;
    }
    
    // Take the worse of current quality and jitter quality
    if (jitter_quality == NetworkMetrics::NetworkQuality::POOR || quality == NetworkMetrics::NetworkQuality::POOR) {
        quality = NetworkMetrics::NetworkQuality::POOR;
    } else if (jitter_quality == NetworkMetrics::NetworkQuality::FAIR || quality == NetworkMetrics::NetworkQuality::FAIR) {
        quality = NetworkMetrics::NetworkQuality::FAIR;
    } else if (jitter_quality == NetworkMetrics::NetworkQuality::GOOD || quality == NetworkMetrics::NetworkQuality::GOOD) {
        quality = NetworkMetrics::NetworkQuality::GOOD;
    }
    
    current_metrics_.quality = quality;
}

void NetworkMonitor::cleanup_old_records() {
    auto cutoff_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    
    // Remove old sent packets
    sent_packets_.erase(
        std::remove_if(sent_packets_.begin(), sent_packets_.end(),
                      [cutoff_time](const PacketRecord& record) {
                          return record.timestamp < cutoff_time;
                      }),
        sent_packets_.end());
    
    // Remove old received packets
    received_packets_.erase(
        std::remove_if(received_packets_.begin(), received_packets_.end(),
                      [cutoff_time](const PacketRecord& record) {
                          return record.timestamp < cutoff_time;
                      }),
        received_packets_.end());
    
    // Limit window size
    if (sent_packets_.size() > window_size_) {
        sent_packets_.erase(sent_packets_.begin(), 
                           sent_packets_.begin() + (sent_packets_.size() - window_size_));
    }
    
    if (received_packets_.size() > window_size_) {
        received_packets_.erase(received_packets_.begin(),
                               received_packets_.begin() + (received_packets_.size() - window_size_));
    }
}

} // namespace Network
} // namespace AudioReceiver