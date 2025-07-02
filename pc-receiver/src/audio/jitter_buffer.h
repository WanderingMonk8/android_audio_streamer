#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <map>
#include <cstdint>

namespace AudioReceiver {
namespace Audio {

/**
 * Audio packet for jitter buffer
 */
struct AudioPacket {
    uint32_t sequence_id;
    uint64_t timestamp;
    std::vector<float> audio_data;
    
    AudioPacket(uint32_t seq_id, uint64_t ts, const std::vector<float>& data)
        : sequence_id(seq_id), timestamp(ts), audio_data(data) {}
};

/**
 * Jitter buffer for audio packets
 * Handles packet reordering, duplicate detection, and overflow management
 * Configured for 3-5 packet capacity as specified in PRD
 */
class JitterBuffer {
public:
    JitterBuffer(int capacity, int frame_size, int channels);
    ~JitterBuffer();

    // Non-copyable
    JitterBuffer(const JitterBuffer&) = delete;
    JitterBuffer& operator=(const JitterBuffer&) = delete;

    // Add audio packet to buffer
    bool add_packet(uint32_t sequence_id, uint64_t timestamp, const std::vector<float>& audio_data);
    
    // Get next packet in sequence order
    std::unique_ptr<AudioPacket> get_next_packet();
    
    // Buffer management
    void clear();
    bool is_empty() const;
    bool is_full() const;
    bool is_initialized() const;
    
    // Configuration
    int get_capacity() const;
    int get_frame_size() const;
    int get_channels() const;
    int get_size() const;
    
    // Statistics
    uint64_t get_packets_added() const;
    uint64_t get_packets_retrieved() const;
    uint64_t get_packets_dropped() const;
    uint64_t get_duplicates_dropped() const;
    
    // Performance metrics
    double get_average_jitter_ms() const;
    uint32_t get_max_sequence_gap() const;

private:
    bool validate_packet(uint32_t sequence_id, const std::vector<float>& audio_data);
    void drop_oldest_packet();
    void update_jitter_stats(uint64_t timestamp);

    // Configuration
    int capacity_;
    int frame_size_;  // Samples per channel
    int channels_;
    bool initialized_;
    
    // Buffer storage (sequence_id -> packet)
    std::map<uint32_t, std::unique_ptr<AudioPacket>> buffer_;
    mutable std::mutex buffer_mutex_;
    
    // Sequence tracking
    uint32_t next_expected_sequence_;
    uint32_t last_sequence_added_;
    
    // Statistics
    std::atomic<uint64_t> packets_added_;
    std::atomic<uint64_t> packets_retrieved_;
    std::atomic<uint64_t> packets_dropped_;
    std::atomic<uint64_t> duplicates_dropped_;
    
    // Jitter measurement
    uint64_t last_packet_timestamp_;
    double jitter_sum_ms_;
    uint64_t jitter_count_;
    uint32_t max_sequence_gap_;
    
    // Constants based on PRD requirements
    static constexpr int MIN_CAPACITY = 1;
    static constexpr int MAX_CAPACITY = 20;  // Reasonable upper limit
    static constexpr int MIN_FRAME_SIZE = 64;
    static constexpr int MAX_FRAME_SIZE = 1024;
    static constexpr int MIN_CHANNELS = 1;
    static constexpr int MAX_CHANNELS = 2;
};

} // namespace Audio
} // namespace AudioReceiver