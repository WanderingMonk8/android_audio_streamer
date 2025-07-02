#pragma once

#include "audio_output.h"  // For AudioDevice struct
#include <vector>
#include <string>
#include <cstdint>
#include <atomic>
#include <memory>

// Forward declaration for real PortAudio stream
#ifdef HAVE_PORTAUDIO
typedef void PaStream;
typedef struct PaDeviceInfo PaDeviceInfo;
#else
// Forward declaration for mock implementation
namespace AudioReceiver { namespace Audio { namespace Mock {
    struct AudioStream;
}}}
#endif

namespace AudioReceiver {
namespace Audio {

/**
 * Real low-latency audio output using PortAudio/WASAPI
 * Configured for ultra-low latency playback (64-512 sample buffers)
 * Supports device selection, WASAPI exclusive mode, and real-time latency measurement
 */
class RealAudioOutput {
public:
    RealAudioOutput(int sample_rate, int channels, int buffer_size, int device_id = -1);
    ~RealAudioOutput();

    // Non-copyable
    RealAudioOutput(const RealAudioOutput&) = delete;
    RealAudioOutput& operator=(const RealAudioOutput&) = delete;

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
    double get_actual_latency_ms() const;  // Real hardware latency measurement
    
    // Device management
    static std::vector<AudioDevice> get_available_devices();
    static AudioDevice get_default_device();
    
    // WASAPI-specific features (Windows only)
    bool set_exclusive_mode(bool exclusive);
    bool is_exclusive_mode() const;

private:
    bool initialize();
    void cleanup();
    bool setup_wasapi_exclusive_mode();
    void measure_actual_latency();
    
    // Configuration
    int sample_rate_;
    int channels_;
    int buffer_size_;
    int device_id_;
    bool exclusive_mode_;
    
    // State
#ifdef HAVE_PORTAUDIO
    PaStream* pa_stream_;
#else
    Mock::AudioStream* pa_stream_;
#endif
    bool initialized_;
    std::atomic<bool> running_;
    
    // Statistics
    std::atomic<uint64_t> frames_written_;
    std::atomic<uint64_t> underruns_;
    std::atomic<double> actual_latency_ms_;
    
    // Constants based on PRD requirements
    static constexpr int SUPPORTED_SAMPLE_RATE = 48000;
    static constexpr int MIN_BUFFER_SIZE = 64;   // Minimum buffer size
    static constexpr int MAX_BUFFER_SIZE = 512;  // Maximum buffer size for low latency
    static constexpr double TARGET_LATENCY_MS = 2.0; // Target playback latency
    static constexpr double MAX_ACCEPTABLE_LATENCY_MS = 5.0; // Maximum acceptable latency
};

} // namespace Audio
} // namespace AudioReceiver