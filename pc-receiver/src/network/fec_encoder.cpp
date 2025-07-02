#include "network/fec_encoder.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cmath>

namespace AudioReceiver {
namespace Network {

// FECHeader implementation
std::vector<uint8_t> FECHeader::serialize() const {
    std::vector<uint8_t> data(HEADER_SIZE);
    size_t offset = 0;
    
    // packet_type (1 byte)
    data[offset] = static_cast<uint8_t>(packet_type);
    offset += 1;
    
    // sequence_id (4 bytes, little endian)
    data[offset] = static_cast<uint8_t>(sequence_id & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((sequence_id >> 8) & 0xFF);
    data[offset + 2] = static_cast<uint8_t>((sequence_id >> 16) & 0xFF);
    data[offset + 3] = static_cast<uint8_t>((sequence_id >> 24) & 0xFF);
    offset += 4;
    
    // redundant_sequence_id (4 bytes, little endian)
    data[offset] = static_cast<uint8_t>(redundant_sequence_id & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((redundant_sequence_id >> 8) & 0xFF);
    data[offset + 2] = static_cast<uint8_t>((redundant_sequence_id >> 16) & 0xFF);
    data[offset + 3] = static_cast<uint8_t>((redundant_sequence_id >> 24) & 0xFF);
    offset += 4;
    
    // redundant_data_size (2 bytes, little endian)
    data[offset] = static_cast<uint8_t>(redundant_data_size & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((redundant_data_size >> 8) & 0xFF);
    offset += 2;
    
    // redundancy_level (1 byte)
    data[offset] = redundancy_level;
    offset += 1;
    
    // reserved (1 byte)
    data[offset] = reserved;
    
    return data;
}

FECHeader FECHeader::deserialize(const std::vector<uint8_t>& data) {
    FECHeader header{};
    
    if (data.size() < HEADER_SIZE) {
        return header; // Return default header if data is too small
    }
    
    size_t offset = 0;
    
    // packet_type (1 byte)
    header.packet_type = static_cast<FECPacketType>(data[offset]);
    offset += 1;
    
    // sequence_id (4 bytes, little endian)
    header.sequence_id = static_cast<uint32_t>(data[offset]) |
                        (static_cast<uint32_t>(data[offset + 1]) << 8) |
                        (static_cast<uint32_t>(data[offset + 2]) << 16) |
                        (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    
    // redundant_sequence_id (4 bytes, little endian)
    header.redundant_sequence_id = static_cast<uint32_t>(data[offset]) |
                                  (static_cast<uint32_t>(data[offset + 1]) << 8) |
                                  (static_cast<uint32_t>(data[offset + 2]) << 16) |
                                  (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    
    // redundant_data_size (2 bytes, little endian)
    header.redundant_data_size = static_cast<uint16_t>(data[offset]) |
                                (static_cast<uint16_t>(data[offset + 1]) << 8);
    offset += 2;
    
    // redundancy_level (1 byte)
    header.redundancy_level = data[offset];
    offset += 1;
    
    // reserved (1 byte)
    header.reserved = data[offset];
    
    return header;
}

// FECEncoder implementation
FECEncoder::FECEncoder(const FECConfig& config) : config_(config) {
    // Validate and clamp configuration values
    config_.redundancy_percentage = std::clamp(config_.redundancy_percentage, 
                                              MIN_REDUNDANCY_PERCENTAGE, 
                                              MAX_REDUNDANCY_PERCENTAGE);
    config_.window_size = std::min(config_.window_size, MAX_WINDOW_SIZE);
    
    stats_.current_redundancy_percentage = config_.redundancy_percentage;
}

std::vector<std::vector<uint8_t>> FECEncoder::encode_packet(uint32_t sequence_id, 
                                                           const std::vector<uint8_t>& audio_data) {
    std::vector<std::vector<uint8_t>> result;
    
    // Always create primary packet
    auto primary_packet = create_primary_packet(sequence_id, audio_data);
    result.push_back(primary_packet);
    
    // Update packet window
    update_packet_window(sequence_id, audio_data);
    
    // Generate redundant packets if we have enough history
    if (packet_window_.size() > 1) {
        auto redundant_packets = generate_redundant_packets(sequence_id);
        result.insert(result.end(), redundant_packets.begin(), redundant_packets.end());
    }
    
    // Update statistics
    stats_.primary_packets_encoded++;
    stats_.redundant_packets_generated += (result.size() - 1);
    
    // Update average redundancy
    if (stats_.primary_packets_encoded > 0) {
        stats_.average_redundancy_percentage = 
            (static_cast<double>(stats_.redundant_packets_generated) / 
             static_cast<double>(stats_.primary_packets_encoded)) * 100.0;
    }
    
    stats_.current_window_size = packet_window_.size();
    
    cleanup_old_packets();
    
    return result;
}

void FECEncoder::set_redundancy_level(double redundancy_percentage) {
    config_.redundancy_percentage = std::clamp(redundancy_percentage, 
                                              MIN_REDUNDANCY_PERCENTAGE, 
                                              MAX_REDUNDANCY_PERCENTAGE);
    stats_.current_redundancy_percentage = config_.redundancy_percentage;
}

FECConfig FECEncoder::get_config() const {
    return config_;
}

void FECEncoder::update_config(const FECConfig& config) {
    config_ = config;
    
    // Validate and clamp values
    config_.redundancy_percentage = std::clamp(config_.redundancy_percentage, 
                                              MIN_REDUNDANCY_PERCENTAGE, 
                                              MAX_REDUNDANCY_PERCENTAGE);
    config_.window_size = std::min(config_.window_size, MAX_WINDOW_SIZE);
    
    stats_.current_redundancy_percentage = config_.redundancy_percentage;
}

FECEncoder::FECStats FECEncoder::get_stats() const {
    return stats_;
}

void FECEncoder::reset() {
    packet_window_.clear();
    stats_ = FECStats{};
    stats_.current_redundancy_percentage = config_.redundancy_percentage;
}

std::vector<uint8_t> FECEncoder::create_primary_packet(uint32_t sequence_id, 
                                                      const std::vector<uint8_t>& audio_data) {
    FECHeader header;
    header.packet_type = FECPacketType::PRIMARY;
    header.sequence_id = sequence_id;
    header.redundant_sequence_id = 0;
    header.redundant_data_size = 0;
    header.redundancy_level = static_cast<uint8_t>(config_.redundancy_percentage);
    header.reserved = 0;
    
    auto header_data = header.serialize();
    
    std::vector<uint8_t> packet;
    packet.reserve(header_data.size() + audio_data.size());
    packet.insert(packet.end(), header_data.begin(), header_data.end());
    packet.insert(packet.end(), audio_data.begin(), audio_data.end());
    
    return packet;
}

std::vector<std::vector<uint8_t>> FECEncoder::generate_redundant_packets(uint32_t sequence_id) {
    std::vector<std::vector<uint8_t>> redundant_packets;
    
    size_t redundant_count = calculate_redundant_packet_count();
    
    // Generate redundant packets for previous packets in the window
    for (size_t i = 0; i < redundant_count && i < packet_window_.size() - 1; i++) {
        const auto& old_packet = packet_window_[packet_window_.size() - 2 - i];
        
        FECHeader header;
        header.packet_type = FECPacketType::REDUNDANT;
        header.sequence_id = sequence_id;
        header.redundant_sequence_id = old_packet.sequence_id;
        header.redundant_data_size = static_cast<uint16_t>(old_packet.data.size());
        header.redundancy_level = static_cast<uint8_t>(config_.redundancy_percentage);
        header.reserved = 0;
        
        auto header_data = header.serialize();
        
        std::vector<uint8_t> redundant_packet;
        redundant_packet.reserve(header_data.size() + old_packet.data.size());
        redundant_packet.insert(redundant_packet.end(), header_data.begin(), header_data.end());
        redundant_packet.insert(redundant_packet.end(), old_packet.data.begin(), old_packet.data.end());
        
        redundant_packets.push_back(redundant_packet);
    }
    
    return redundant_packets;
}

void FECEncoder::update_packet_window(uint32_t sequence_id, const std::vector<uint8_t>& audio_data) {
    PacketRecord record;
    record.sequence_id = sequence_id;
    record.data = audio_data;
    record.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    packet_window_.push_back(record);
    
    // Limit window size
    while (packet_window_.size() > config_.window_size) {
        packet_window_.pop_front();
    }
}

void FECEncoder::cleanup_old_packets() {
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Remove packets older than 1 second
    packet_window_.erase(
        std::remove_if(packet_window_.begin(), packet_window_.end(),
                      [now_ms](const PacketRecord& record) {
                          return (now_ms - record.timestamp_ms) > 1000;
                      }),
        packet_window_.end());
}

size_t FECEncoder::calculate_redundant_packet_count() const {
    if (config_.redundancy_percentage <= 0.0) {
        return 0;
    }
    
    // Calculate how many redundant packets to generate based on redundancy percentage
    // For example, 20% redundancy means 1 redundant packet for every 5 primary packets
    double redundant_ratio = config_.redundancy_percentage / 100.0;
    size_t max_redundant = static_cast<size_t>(std::ceil(redundant_ratio * config_.window_size));
    
    // Limit to available packets and recovery distance
    return std::min({max_redundant, packet_window_.size() - 1, config_.max_recovery_distance});
}

} // namespace Network
} // namespace AudioReceiver