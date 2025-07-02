#include "network/fec_decoder.h"
#include <algorithm>
#include <chrono>

namespace AudioReceiver {
namespace Network {

FECDecoder::FECDecoder(size_t max_recovery_distance, size_t buffer_size)
    : max_recovery_distance_(max_recovery_distance)
    , buffer_size_(std::min(buffer_size, MAX_BUFFER_SIZE)) {
}

FECRecoveryResult FECDecoder::process_packet(const std::vector<uint8_t>& packet_data) {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    
    FECRecoveryResult result;
    
    if (packet_data.size() < FECHeader::HEADER_SIZE) {
        return result; // Invalid packet
    }
    
    // Deserialize header
    auto header = FECHeader::deserialize(packet_data);
    
    // Extract payload data
    std::vector<uint8_t> payload_data(
        packet_data.begin() + FECHeader::HEADER_SIZE,
        packet_data.end()
    );
    
    // Create stored packet
    StoredPacket stored_packet;
    stored_packet.sequence_id = header.sequence_id;
    stored_packet.data = payload_data;
    stored_packet.packet_type = header.packet_type;
    stored_packet.redundant_sequence_id = header.redundant_sequence_id;
    stored_packet.timestamp_ms = get_current_timestamp_ms();
    stored_packet.is_primary_data = (header.packet_type == FECPacketType::PRIMARY);
    
    if (header.packet_type == FECPacketType::PRIMARY) {
        // Store primary packet
        primary_packets_[header.sequence_id] = stored_packet;
        stats_.primary_packets_received++;
        
        // Return the primary packet data immediately
        result.success = true;
        result.sequence_id = header.sequence_id;
        result.recovered_data = payload_data;
        result.was_recovered_from_redundancy = false;
        
    } else if (header.packet_type == FECPacketType::REDUNDANT) {
        // Store redundant packet
        redundant_packets_[header.redundant_sequence_id].push_back(stored_packet);
        stats_.redundant_packets_received++;
        
        // Don't return data for redundant packets directly
        result.success = false;
    }
    
    // Cleanup old packets
    cleanup_expired_packets(header.sequence_id);
    
    return result;
}

FECRecoveryResult FECDecoder::recover_packet(uint32_t sequence_id) {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    
    stats_.recovery_attempts++;
    
    // Check if we already have the primary packet
    auto primary_it = primary_packets_.find(sequence_id);
    if (primary_it != primary_packets_.end()) {
        FECRecoveryResult result;
        result.success = true;
        result.sequence_id = sequence_id;
        result.recovered_data = primary_it->second.data;
        result.was_recovered_from_redundancy = false;
        return result;
    }
    
    // Try to recover from redundancy
    auto result = try_recover_from_redundancy(sequence_id);
    update_recovery_stats(result);
    
    return result;
}

bool FECDecoder::can_recover_packet(uint32_t sequence_id) const {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    
    // Check if we have the primary packet
    if (primary_packets_.find(sequence_id) != primary_packets_.end()) {
        return true;
    }
    
    // Check if we have redundant packets for this sequence ID
    auto redundant_it = redundant_packets_.find(sequence_id);
    return (redundant_it != redundant_packets_.end() && !redundant_it->second.empty());
}

std::vector<uint32_t> FECDecoder::get_recoverable_packets() const {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    
    std::vector<uint32_t> recoverable;
    
    // Add all primary packets
    for (const auto& pair : primary_packets_) {
        recoverable.push_back(pair.first);
    }
    
    // Add all packets that have redundancy available
    for (const auto& pair : redundant_packets_) {
        if (!pair.second.empty()) {
            recoverable.push_back(pair.first);
        }
    }
    
    // Remove duplicates and sort
    std::sort(recoverable.begin(), recoverable.end());
    recoverable.erase(std::unique(recoverable.begin(), recoverable.end()), recoverable.end());
    
    return recoverable;
}

FECDecoder::FECDecodeStats FECDecoder::get_stats() const {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    
    FECDecodeStats stats = stats_;
    
    // Calculate recovery success rate
    if (stats.recovery_attempts > 0) {
        stats.recovery_success_rate = 
            (static_cast<double>(stats.packets_recovered) / 
             static_cast<double>(stats.recovery_attempts)) * 100.0;
    }
    
    return stats;
}

void FECDecoder::reset() {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    
    primary_packets_.clear();
    redundant_packets_.clear();
    recovery_timestamps_.clear();
    stats_ = FECDecodeStats{};
}

void FECDecoder::set_max_recovery_distance(size_t max_distance) {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    max_recovery_distance_ = max_distance;
}

void FECDecoder::cleanup_old_packets() {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    
    // Find the highest sequence ID to determine cleanup threshold
    uint32_t max_sequence_id = 0;
    for (const auto& pair : primary_packets_) {
        max_sequence_id = std::max(max_sequence_id, pair.first);
    }
    for (const auto& pair : redundant_packets_) {
        max_sequence_id = std::max(max_sequence_id, pair.first);
    }
    
    cleanup_expired_packets(max_sequence_id);
}

FECRecoveryResult FECDecoder::try_recover_from_redundancy(uint32_t sequence_id) {
    FECRecoveryResult result;
    
    auto redundant_it = redundant_packets_.find(sequence_id);
    if (redundant_it == redundant_packets_.end() || redundant_it->second.empty()) {
        stats_.recovery_failures++;
        return result;
    }
    
    // Use the first available redundant packet
    const auto& redundant_packet = redundant_it->second[0];
    
    result.success = true;
    result.sequence_id = sequence_id;
    result.recovered_data = redundant_packet.data;
    result.was_recovered_from_redundancy = true;
    result.redundant_packet_used = redundant_packet.sequence_id;
    
    // Calculate recovery delay
    if (redundant_packet.sequence_id > sequence_id) {
        result.recovery_delay_packets = redundant_packet.sequence_id - sequence_id;
        stats_.max_recovery_delay_packets = std::max(stats_.max_recovery_delay_packets, 
                                                    result.recovery_delay_packets);
    }
    
    stats_.packets_recovered++;
    
    // Record recovery timestamp for latency calculation
    recovery_timestamps_[sequence_id] = get_current_timestamp_ms();
    
    return result;
}

bool FECDecoder::is_packet_in_recovery_window(uint32_t sequence_id, uint32_t current_sequence_id) const {
    if (current_sequence_id < sequence_id) {
        return false; // Future packet
    }
    
    uint32_t distance = current_sequence_id - sequence_id;
    return distance <= max_recovery_distance_;
}

void FECDecoder::update_recovery_stats(const FECRecoveryResult& result) {
    if (!result.success) {
        return;
    }
    
    // Update average recovery delay
    if (result.was_recovered_from_redundancy && result.recovery_delay_packets > 0) {
        // Simple moving average for recovery delay
        static constexpr double ALPHA = 0.1; // Smoothing factor
        double new_delay_ms = result.recovery_delay_packets * 2.5; // Assume 2.5ms per packet
        
        if (stats_.average_recovery_delay_ms == 0.0) {
            stats_.average_recovery_delay_ms = new_delay_ms;
        } else {
            stats_.average_recovery_delay_ms = 
                (1.0 - ALPHA) * stats_.average_recovery_delay_ms + ALPHA * new_delay_ms;
        }
    }
}

void FECDecoder::cleanup_expired_packets(uint32_t current_sequence_id) {
    uint64_t current_time = get_current_timestamp_ms();
    
    // Remove expired primary packets
    auto primary_it = primary_packets_.begin();
    while (primary_it != primary_packets_.end()) {
        bool should_remove = false;
        
        // Remove if too old by time
        if (current_time - primary_it->second.timestamp_ms > PACKET_TIMEOUT_MS) {
            should_remove = true;
        }
        
        // Remove if outside recovery window
        if (!is_packet_in_recovery_window(primary_it->first, current_sequence_id)) {
            should_remove = true;
        }
        
        if (should_remove) {
            primary_it = primary_packets_.erase(primary_it);
        } else {
            ++primary_it;
        }
    }
    
    // Remove expired redundant packets
    auto redundant_it = redundant_packets_.begin();
    while (redundant_it != redundant_packets_.end()) {
        bool should_remove = false;
        
        // Remove if outside recovery window
        if (!is_packet_in_recovery_window(redundant_it->first, current_sequence_id)) {
            should_remove = true;
        }
        
        // Remove if all packets in the vector are too old
        if (!redundant_it->second.empty()) {
            bool all_expired = true;
            for (const auto& packet : redundant_it->second) {
                if (current_time - packet.timestamp_ms <= PACKET_TIMEOUT_MS) {
                    all_expired = false;
                    break;
                }
            }
            if (all_expired) {
                should_remove = true;
            }
        }
        
        if (should_remove) {
            redundant_it = redundant_packets_.erase(redundant_it);
        } else {
            ++redundant_it;
        }
    }
    
    // Cleanup recovery timestamps
    auto recovery_it = recovery_timestamps_.begin();
    while (recovery_it != recovery_timestamps_.end()) {
        if (current_time - recovery_it->second > PACKET_TIMEOUT_MS) {
            recovery_it = recovery_timestamps_.erase(recovery_it);
        } else {
            ++recovery_it;
        }
    }
}

uint64_t FECDecoder::get_current_timestamp_ms() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace Network
} // namespace AudioReceiver