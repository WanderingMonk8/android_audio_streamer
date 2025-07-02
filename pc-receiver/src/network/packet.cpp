#include "network/packet.h"
#include <cstring>
#include <algorithm>

namespace AudioReceiver {
namespace Network {

AudioPacket::AudioPacket(uint32_t seq_id, uint64_t ts, const std::vector<uint8_t>& data)
    : sequence_id(seq_id), timestamp(ts), payload_size(static_cast<uint32_t>(data.size())), payload(data) {
}

std::vector<uint8_t> AudioPacket::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(total_size());
    
    // Serialize sequence_id (little endian)
    result.push_back(static_cast<uint8_t>(sequence_id & 0xFF));
    result.push_back(static_cast<uint8_t>((sequence_id >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>((sequence_id >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((sequence_id >> 24) & 0xFF));
    
    // Serialize timestamp (little endian)
    result.push_back(static_cast<uint8_t>(timestamp & 0xFF));
    result.push_back(static_cast<uint8_t>((timestamp >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>((timestamp >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((timestamp >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((timestamp >> 32) & 0xFF));
    result.push_back(static_cast<uint8_t>((timestamp >> 40) & 0xFF));
    result.push_back(static_cast<uint8_t>((timestamp >> 48) & 0xFF));
    result.push_back(static_cast<uint8_t>((timestamp >> 56) & 0xFF));
    
    // Serialize payload_size (little endian)
    result.push_back(static_cast<uint8_t>(payload_size & 0xFF));
    result.push_back(static_cast<uint8_t>((payload_size >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>((payload_size >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((payload_size >> 24) & 0xFF));
    
    // Serialize payload
    result.insert(result.end(), payload.begin(), payload.end());
    
    return result;
}

std::unique_ptr<AudioPacket> AudioPacket::deserialize(const uint8_t* data, size_t size) {
    // Minimum packet size: 4 + 8 + 4 = 16 bytes
    if (size < 16) {
        return nullptr;
    }
    
    auto packet = std::make_unique<AudioPacket>();
    
    // Deserialize sequence_id (little endian)
    packet->sequence_id = static_cast<uint32_t>(data[0]) |
                         (static_cast<uint32_t>(data[1]) << 8) |
                         (static_cast<uint32_t>(data[2]) << 16) |
                         (static_cast<uint32_t>(data[3]) << 24);
    
    // Deserialize timestamp (little endian)
    packet->timestamp = static_cast<uint64_t>(data[4]) |
                       (static_cast<uint64_t>(data[5]) << 8) |
                       (static_cast<uint64_t>(data[6]) << 16) |
                       (static_cast<uint64_t>(data[7]) << 24) |
                       (static_cast<uint64_t>(data[8]) << 32) |
                       (static_cast<uint64_t>(data[9]) << 40) |
                       (static_cast<uint64_t>(data[10]) << 48) |
                       (static_cast<uint64_t>(data[11]) << 56);
    
    // Deserialize payload_size (little endian)
    packet->payload_size = static_cast<uint32_t>(data[12]) |
                          (static_cast<uint32_t>(data[13]) << 8) |
                          (static_cast<uint32_t>(data[14]) << 16) |
                          (static_cast<uint32_t>(data[15]) << 24);
    
    // Check if we have enough data for the payload
    if (size < 16 + packet->payload_size) {
        return nullptr;
    }
    
    // Deserialize payload
    packet->payload.resize(packet->payload_size);
    if (packet->payload_size > 0) {
        std::memcpy(packet->payload.data(), data + 16, packet->payload_size);
    }
    
    return packet->is_valid() ? std::move(packet) : nullptr;
}

size_t AudioPacket::total_size() const {
    return 16 + payload_size; // 4 + 8 + 4 + payload_size
}

bool AudioPacket::is_valid() const {
    return payload_size == payload.size() && payload_size <= 65536; // Reasonable max size
}

} // namespace Network
} // namespace AudioReceiver