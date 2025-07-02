#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <atomic>

// Forward declaration for real Opus decoder
#ifdef HAVE_LIBOPUS
struct OpusDecoder;
#else
// Forward declaration for mock implementation
namespace AudioReceiver { namespace Audio { namespace Mock {
    struct OpusDecoder;
}}}
#endif

namespace AudioReceiver {
namespace Audio {

/**
 * Real Opus audio decoder using libopus
 * Configured for CELT-only mode with 2.5ms frames (120 samples @ 48kHz)
 * Supports packet loss concealment and Forward Error Correction (FEC)
 */
class RealOpusDecoder {
public:
    RealOpusDecoder(int sample_rate, int channels);
    ~RealOpusDecoder();

    // Non-copyable
    RealOpusDecoder(const RealOpusDecoder&) = delete;
    RealOpusDecoder& operator=(const RealOpusDecoder&) = delete;

    // Decode Opus-encoded audio data to PCM float samples
    std::vector<float> decode(const std::vector<uint8_t>& encoded_data);
    
    // Decode with Forward Error Correction for packet loss
    std::vector<float> decode_with_fec(const std::vector<uint8_t>& encoded_data = {});
    
    // Generate silence for missing packets
    std::vector<float> generate_silence();
    
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
    uint64_t get_fec_recoveries() const;

private:
    bool initialize();
    void cleanup();

    int sample_rate_;
    int channels_;
    int frame_size_samples_; // Per channel
    
#ifdef HAVE_LIBOPUS
    ::OpusDecoder* opus_decoder_;
#else
    Mock::OpusDecoder* opus_decoder_;
#endif
    bool initialized_;
    
    // Statistics
    std::atomic<uint64_t> frames_decoded_;
    std::atomic<uint64_t> decode_errors_;
    std::atomic<uint64_t> fec_recoveries_;
    
    // Constants based on PRD requirements
    static constexpr int SUPPORTED_SAMPLE_RATE = 48000;
    static constexpr double FRAME_DURATION_MS = 2.5; // 2.5ms frames
    static constexpr int MAX_FRAME_SIZE = 960;     // 120 samples * 2 channels * sizeof(float)
    static constexpr int MAX_PACKET_SIZE = 1500;   // Maximum Opus packet size
};

} // namespace Audio
} // namespace AudioReceiver