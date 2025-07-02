#pragma once

#include <vector>
#include <cstdint>
#include <deque>
#include <memory>

namespace AudioReceiver {
namespace Network {

/**
 * Forward Error Correction (FEC) configuration
 */
struct FECConfig {
    double redundancy_percentage = 20.0;  // Percentage of redundancy (0-50%)
    size_t max_recovery_distance = 5;     // Maximum packets that can be recovered
    size_t window_size = 10;              // Sliding window size for redundancy
    bool adaptive_redundancy = true;      // Enable adaptive redundancy based on network conditions
};

/**
 * FEC packet types for identification
 */
enum class FECPacketType : uint8_t {
    PRIMARY = 0x01,    // Original audio packet
    REDUNDANT = 0x02   // Redundancy packet containing previous data
};

/**
 * FEC packet header for redundancy information
 */
struct FECHeader {
    FECPacketType packet_type;
    uint32_t sequence_id;
    uint32_t redundant_sequence_id;  // For redundant packets: which packet this recovers
    uint16_t redundant_data_size;    // Size of redundant data
    uint8_t redundancy_level;        // Redundancy level (0-255)
    uint8_t reserved;                // Reserved for future use
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static FECHeader deserialize(const std::vector<uint8_t>& data);
    static constexpr size_t HEADER_SIZE = 13; // bytes
};

/**
 * Forward Error Correction Encoder
 * Implements redundancy-based FEC for audio packet recovery
 * 
 * Features:
 * - Adaptive redundancy based on network conditions
 * - Sliding window redundancy generation
 * - Configurable redundancy levels (5-50%)
 * - Integration with Network Monitor for adaptive behavior
 * - Thread-safe operation
 */
class FECEncoder {
public:
    explicit FECEncoder(const FECConfig& config = FECConfig{});
    ~FECEncoder() = default;

    // Non-copyable
    FECEncoder(const FECEncoder&) = delete;
    FECEncoder& operator=(const FECEncoder&) = delete;

    /**
     * Encode a primary audio packet with FEC
     * @param sequence_id Packet sequence number
     * @param audio_data Original audio data
     * @return Vector of packets (primary + redundant packets)
     */
    std::vector<std::vector<uint8_t>> encode_packet(uint32_t sequence_id, 
                                                   const std::vector<uint8_t>& audio_data);

    /**
     * Update redundancy level based on network conditions
     * @param redundancy_percentage New redundancy level (0-50%)
     */
    void set_redundancy_level(double redundancy_percentage);

    /**
     * Get current redundancy configuration
     * @return Current FEC configuration
     */
    FECConfig get_config() const;

    /**
     * Update FEC configuration
     * @param config New FEC configuration
     */
    void update_config(const FECConfig& config);

    /**
     * Get statistics about FEC encoding
     */
    struct FECStats {
        uint64_t primary_packets_encoded = 0;
        uint64_t redundant_packets_generated = 0;
        double current_redundancy_percentage = 0.0;
        double average_redundancy_percentage = 0.0;
        size_t current_window_size = 0;
    };

    /**
     * Get FEC encoding statistics
     * @return Current FEC statistics
     */
    FECStats get_stats() const;

    /**
     * Reset FEC encoder state and statistics
     */
    void reset();

private:
    struct PacketRecord {
        uint32_t sequence_id;
        std::vector<uint8_t> data;
        size_t timestamp_ms;
    };

    FECConfig config_;
    std::deque<PacketRecord> packet_window_;
    FECStats stats_;
    
    // Helper methods
    std::vector<uint8_t> create_primary_packet(uint32_t sequence_id, 
                                              const std::vector<uint8_t>& audio_data);
    std::vector<std::vector<uint8_t>> generate_redundant_packets(uint32_t sequence_id);
    void update_packet_window(uint32_t sequence_id, const std::vector<uint8_t>& audio_data);
    void cleanup_old_packets();
    size_t calculate_redundant_packet_count() const;
    
    // Constants
    static constexpr double MIN_REDUNDANCY_PERCENTAGE = 0.0;
    static constexpr double MAX_REDUNDANCY_PERCENTAGE = 50.0;
    static constexpr size_t MAX_WINDOW_SIZE = 20;
    static constexpr size_t MAX_PACKET_SIZE = 1500;
};

} // namespace Network
} // namespace AudioReceiver