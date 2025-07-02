#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace AudioReceiver {
namespace Network {

/**
 * Audio packet structure for UDP transmission
 * Format: [sequence_id(4)] [timestamp(8)] [payload_size(4)] [payload(variable)]
 */
struct AudioPacket {
    uint32_t sequence_id;
    uint64_t timestamp;
    uint32_t payload_size;
    std::vector<uint8_t> payload;

    AudioPacket() = default;
    AudioPacket(uint32_t seq_id, uint64_t ts, const std::vector<uint8_t>& data);
    
    // Serialize packet to bytes for transmission
    std::vector<uint8_t> serialize() const;
    
    // Deserialize bytes to packet
    static std::unique_ptr<AudioPacket> deserialize(const uint8_t* data, size_t size);
    
    // Get total packet size in bytes
    size_t total_size() const;
    
    // Validate packet integrity
    bool is_valid() const;
};

} // namespace Network
} // namespace AudioReceiver