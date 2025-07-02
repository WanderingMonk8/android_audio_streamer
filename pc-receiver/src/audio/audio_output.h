#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <atomic>
#include <memory>

// Forward declaration for Mock PortAudio stream
namespace AudioReceiver { namespace Audio { namespace Mock {
    struct AudioStream;
}}}

namespace AudioReceiver {
namespace Audio {

/**
 * Audio device information
 */
struct AudioDevice {
    int id;
    std::string name;
    int max_channels;
    int default_sample_rate;
    bool is_default;
};

/**
 * Low-latency audio output using PortAudio/WASAPI
 * Configured for ultra-low latency playback (64-128 sample buffers)
 * Supports device selection and real-time latency measurement
 */
class AudioOutput {
public:
    AudioOutput(int sample_rate, int channels, int buffer_size, int device_id = -1);
    ~AudioOutput();

    // Non-copyable
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // Start/stop audio output
    bool start();
    void stop();
    
    // Write audio data (blocking)
    bool write_audio(const std::vector<float>& audio_data);
    
    // Check status
    bool is_initialized() const;
    bool is_running() const;
    
    // Get configuration
    int get_sample_rate() const;
    int get_channels() const;
    int get_buffer_size() const;
    int get_device_id() const;
    
    // Performance metrics
    uint64_t get_frames_written() const;
    uint64_t get_underruns() const;
    double get_estimated_latency_ms() const;
    
    // Device management
    static std::vector<AudioDevice> get_available_devices();
    static AudioDevice get_default_device();

private:
    bool initialize();
    void cleanup();
    
    // Configuration
    int sample_rate_;
    int channels_;
    int buffer_size_;
    int device_id_;
    
    // State
    Mock::AudioStream* audio_stream_;
    bool initialized_;
    std::atomic<bool> running_;
    
    // Statistics
    std::atomic<uint64_t> frames_written_;
    std::atomic<uint64_t> underruns_;
    
    // Constants based on PRD requirements
    static constexpr int SUPPORTED_SAMPLE_RATE = 48000;
    static constexpr int MIN_BUFFER_SIZE = 64;   // Minimum buffer size
    static constexpr int MAX_BUFFER_SIZE = 512;  // Maximum buffer size for low latency
    static constexpr double TARGET_LATENCY_MS = 2.0; // Target playback latency
};

} // namespace Audio
} // namespace AudioReceiver