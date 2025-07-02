#pragma once

#include "opus_decoder.h"
#include "audio_output.h"
#include "real_audio_output.h"
#include "jitter_buffer.h"
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>

namespace AudioReceiver {
namespace Audio {

/**
 * Complete audio processing pipeline
 * Integrates: UDP packets → Jitter Buffer → Opus Decoder → Audio Output
 * Provides end-to-end latency measurement and performance monitoring
 */
class AudioPipeline {
public:
    AudioPipeline(int sample_rate, int channels, int buffer_size, int jitter_buffer_capacity, int device_id = -1);
    ~AudioPipeline();

    // Non-copyable
    AudioPipeline(const AudioPipeline&) = delete;
    AudioPipeline& operator=(const AudioPipeline&) = delete;

    // Pipeline control
    bool start();
    void stop();
    bool is_running() const;
    bool is_initialized() const;
    
    // Process incoming audio packet
    bool process_audio_packet(uint32_t sequence_id, uint64_t timestamp, const std::vector<uint8_t>& encoded_data);
    
    // Configuration
    int get_sample_rate() const;
    int get_channels() const;
    int get_buffer_size() const;
    int get_jitter_buffer_capacity() const;
    int get_device_id() const;
    
    // Performance statistics
    uint64_t get_packets_processed() const;
    uint64_t get_frames_decoded() const;
    uint64_t get_frames_output() const;
    uint64_t get_decode_errors() const;
    uint64_t get_output_underruns() const;
    uint64_t get_jitter_buffer_drops() const;
    
    // Latency measurements (in milliseconds)
    double get_total_latency_ms() const;
    double get_decode_latency_ms() const;
    double get_output_latency_ms() const;
    double get_jitter_buffer_latency_ms() const;
    
    // Performance metrics
    double get_cpu_usage_percent() const;
    double get_average_processing_time_us() const;
    bool is_meeting_realtime_deadline() const;

private:
    // Pipeline processing thread
    void processing_thread();
    
    // Component processing methods
    bool process_jitter_buffer();
    bool process_decoder();
    bool process_audio_output();
    
    // Latency measurement
    void update_latency_measurements();
    uint64_t get_current_timestamp_us() const;
    
    // Configuration
    int sample_rate_;
    int channels_;
    int buffer_size_;
    int jitter_buffer_capacity_;
    int device_id_;
    bool initialized_;
    
    // Pipeline components
    std::unique_ptr<JitterBuffer> jitter_buffer_;
    std::unique_ptr<OpusDecoder> decoder_;
#ifdef HAVE_PORTAUDIO
    std::unique_ptr<RealAudioOutput> audio_output_;
#else
    std::unique_ptr<AudioOutput> audio_output_;
#endif
    
    // Threading
    std::atomic<bool> running_;
    std::thread processing_thread_;
    std::mutex pipeline_mutex_;
    std::condition_variable pipeline_cv_;
    
    // Packet queue for incoming packets
    std::queue<std::tuple<uint32_t, uint64_t, std::vector<uint8_t>>> packet_queue_;
    std::mutex packet_queue_mutex_;
    
    // Decoded audio buffer queue
    std::queue<std::vector<float>> decoded_audio_queue_;
    std::mutex decoded_audio_mutex_;
    
    // Statistics
    std::atomic<uint64_t> packets_processed_;
    std::atomic<uint64_t> frames_decoded_;
    std::atomic<uint64_t> frames_output_;
    std::atomic<uint64_t> decode_errors_;
    std::atomic<uint64_t> output_underruns_;
    
    // Latency tracking
    mutable std::mutex latency_mutex_;
    double total_latency_ms_;
    double decode_latency_ms_;
    double output_latency_ms_;
    double jitter_buffer_latency_ms_;
    
    // Performance tracking
    std::chrono::high_resolution_clock::time_point last_processing_time_;
    double total_processing_time_us_;
    uint64_t processing_count_;
    
    // Constants based on PRD requirements
    static constexpr int SUPPORTED_SAMPLE_RATE = 48000;
    static constexpr int MIN_BUFFER_SIZE = 64;
    static constexpr int MAX_BUFFER_SIZE = 512;
    static constexpr int MIN_JITTER_CAPACITY = 1;
    static constexpr int MAX_JITTER_CAPACITY = 20;
    static constexpr double TARGET_TOTAL_LATENCY_MS = 10.0; // PRD requirement
    static constexpr int PROCESSING_THREAD_SLEEP_US = 1000; // 1ms
};

} // namespace Audio
} // namespace AudioReceiver