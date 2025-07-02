#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <atomic>

// Forward declaration for Mock Opus decoder (replace with real OpusDecoder in production)
namespace AudioReceiver { namespace Audio { namespace Mock {
    struct OpusDecoder;
}}}

namespace AudioReceiver {
namespace Audio {

/**
 * Opus audio decoder for low-latency audio streaming
 * Configured for CELT-only mode with 2.5ms frames (120 samples @ 48kHz)
 */
class OpusDecoder {
public:
    OpusDecoder(int sample_rate, int channels);
    ~OpusDecoder();

    // Non-copyable
    OpusDecoder(const OpusDecoder&) = delete;
    OpusDecoder& operator=(const OpusDecoder&) = delete;

    // Decode Opus-encoded audio data to PCM float samples
    std::vector<float> decode(const std::vector<uint8_t>& encoded_data);
    
    // Reset decoder state
    void reset();
    
    // Check if decoder is properly initialized
    bool is_initialized() const;
    
    // Get decoder configuration
    int get_sample_rate() const;
    int get_channels() const;
    int get_frame_size_samples() const; // Samples per channel
    int get_frame_size_bytes() const;   // Total bytes for all channels
    
    // Get statistics
    uint64_t get_frames_decoded() const;
    uint64_t get_decode_errors() const;

private:
    bool initialize();
    void cleanup();

    int sample_rate_;
    int channels_;
    int frame_size_samples_; // Per channel
    
    Mock::OpusDecoder* opus_decoder_;
    bool initialized_;
    
    // Statistics
    std::atomic<uint64_t> frames_decoded_;
    std::atomic<uint64_t> decode_errors_;
    
    // Constants based on PRD requirements
    static constexpr int SUPPORTED_SAMPLE_RATE = 48000;
    static constexpr double FRAME_DURATION_MS = 2.5; // 2.5ms frames
    static constexpr int MAX_FRAME_SIZE = 960;     // 120 samples * 2 channels * sizeof(float)
    static constexpr int MAX_PACKET_SIZE = 1500;   // Maximum Opus packet size
};

} // namespace Audio
} // namespace AudioReceiver