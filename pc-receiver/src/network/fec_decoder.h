#pragma once

#include "fec_encoder.h"
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

namespace AudioReceiver {
namespace Network {

/**
 * FEC recovery result for decoded packets
 */
struct FECRecoveryResult {
    bool success = false;
    uint32_t sequence_id = 0;
    std::vector<uint8_t> recovered_data;
    bool was_recovered_from_redundancy = false;
    
    // Recovery statistics
    uint32_t redundant_packet_used = 0;  // Which redundant packet was used for recovery
    size_t recovery_delay_packets = 0;   // How many packets later the recovery happened
};

/**
 * Forward Error Correction Decoder
 * Recovers lost packets using redundancy information
 * 
 * Features:
 * - Automatic packet loss detection and recovery
 * - Multiple redundancy level support
 * - Recovery statistics and monitoring
 * - Integration with jitter buffer for seamless operation
 * - Thread-safe operation
 */
class FECDecoder {
public:
    explicit FECDecoder(size_t max_recovery_distance = 5, size_t buffer_size = 20);
    ~FECDecoder() = default;

    // Non-copyable
    FECDecoder(const FECDecoder&) = delete;
    FECDecoder& operator=(const FECDecoder&) = delete;

    /**
     * Process an incoming FEC packet (primary or redundant)
     * @param packet_data Raw packet data with FEC header
     * @return Recovery result (may contain recovered packet)
     */
    FECRecoveryResult process_packet(const std::vector<uint8_t>& packet_data);

    /**
     * Attempt to recover a specific missing packet
     * @param sequence_id Sequence ID of the missing packet
     * @return Recovery result
     */
    FECRecoveryResult recover_packet(uint32_t sequence_id);

    /**
     * Check if a packet can be recovered using available redundancy
     * @param sequence_id Sequence ID to check
     * @return true if packet can be recovered
     */
    bool can_recover_packet(uint32_t sequence_id) const;

    /**
     * Get list of packets that can be recovered
     * @return Vector of sequence IDs that can be recovered
     */
    std::vector<uint32_t> get_recoverable_packets() const;

    /**
     * FEC decoder statistics
     */
    struct FECDecodeStats {
        uint64_t primary_packets_received = 0;
        uint64_t redundant_packets_received = 0;
        uint64_t packets_recovered = 0;
        uint64_t recovery_attempts = 0;
        uint64_t recovery_failures = 0;
        double recovery_success_rate = 0.0;
        uint64_t packets_lost_unrecoverable = 0;
        
        // Timing statistics
        double average_recovery_delay_ms = 0.0;
        uint64_t max_recovery_delay_packets = 0;
    };

    /**
     * Get FEC decoding statistics
     * @return Current decode statistics
     */
    FECDecodeStats get_stats() const;

    /**
     * Reset decoder state and statistics
     */
    void reset();

    /**
     * Set maximum recovery distance
     * @param max_distance Maximum number of packets that can be recovered
     */
    void set_max_recovery_distance(size_t max_distance);

    /**
     * Cleanup old packets beyond recovery window
     */
    void cleanup_old_packets();

private:
    struct StoredPacket {
        uint32_t sequence_id;
        std::vector<uint8_t> data;
        FECPacketType packet_type;
        uint32_t redundant_sequence_id;  // For redundant packets
        uint64_t timestamp_ms;
        bool is_primary_data;
    };

    size_t max_recovery_distance_;
    size_t buffer_size_;
    
    // Thread safety
    mutable std::mutex decoder_mutex_;
    
    // Packet storage
    std::map<uint32_t, StoredPacket> primary_packets_;
    std::map<uint32_t, std::vector<StoredPacket>> redundant_packets_;  // Key: redundant_sequence_id
    
    // Statistics
    FECDecodeStats stats_;
    
    // Recovery tracking
    std::map<uint32_t, uint64_t> recovery_timestamps_;
    
    // Helper methods
    FECRecoveryResult try_recover_from_redundancy(uint32_t sequence_id);
    bool is_packet_in_recovery_window(uint32_t sequence_id, uint32_t current_sequence_id) const;
    void update_recovery_stats(const FECRecoveryResult& result);
    void cleanup_expired_packets(uint32_t current_sequence_id);
    uint64_t get_current_timestamp_ms() const;
    
    // Constants
    static constexpr size_t MAX_BUFFER_SIZE = 100;
    static constexpr uint64_t PACKET_TIMEOUT_MS = 1000;  // 1 second timeout for recovery
};

} // namespace Network
} // namespace AudioReceiver