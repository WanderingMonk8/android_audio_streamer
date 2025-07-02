#include "audio/audio_output.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

namespace AudioReceiver { namespace Audio { namespace Mock {
    struct AudioStream {
        int sample_rate;
        int channels;
        int buffer_size;
        int device_id;
        bool running;
        std::thread playback_thread;
        std::atomic<bool> should_stop;
        
        AudioStream(int sr, int ch, int bs, int dev_id) 
            : sample_rate(sr), channels(ch), buffer_size(bs), device_id(dev_id)
            , running(false), should_stop(false) {}
    };
    
    // Mock PortAudio functions
    bool pa_initialize() {
        static bool initialized = false;
        if (!initialized) {
            std::cout << "Mock PortAudio initialized" << std::endl;
            initialized = true;
        }
        return true;
    }
    
    void pa_terminate() {
        std::cout << "Mock PortAudio terminated" << std::endl;
    }
    
    std::vector<AudioDevice> pa_get_devices() {
        return {
            {0, "Default Audio Device", 2, 48000, true},
            {1, "Mock WASAPI Device", 2, 48000, false},
            {2, "Mock DirectSound Device", 2, 48000, false}
        };
    }
    
    AudioDevice pa_get_default_device() {
        return {0, "Default Audio Device", 2, 48000, true};
    }
    
    AudioStream* pa_open_stream(int sample_rate, int channels, int buffer_size, int device_id) {
        if (sample_rate != 48000 || channels < 1 || channels > 2 || 
            buffer_size < 64 || buffer_size > 512) {
            return nullptr;
        }
        
        // Validate device ID
        auto devices = pa_get_devices();
        bool valid_device = (device_id == -1); // Default device
        for (const auto& dev : devices) {
            if (dev.id == device_id) {
                valid_device = true;
                break;
            }
        }
        
        if (!valid_device) {
            return nullptr;
        }
        
        return new AudioStream(sample_rate, channels, buffer_size, device_id);
    }
    
    bool pa_start_stream(AudioStream* stream) {
        if (!stream || stream->running) {
            return false;
        }
        
        stream->running = true;
        stream->should_stop.store(false);
        
        // Start mock playback thread
        stream->playback_thread = std::thread([stream]() {
            std::cout << "Mock audio playback started - " 
                      << stream->sample_rate << "Hz, " << stream->channels << " channels, "
                      << stream->buffer_size << " samples buffer" << std::endl;
            
            while (!stream->should_stop.load()) {
                // Simulate audio buffer consumption
                std::this_thread::sleep_for(std::chrono::microseconds(
                    stream->buffer_size * 1000000 / stream->sample_rate));
            }
            
            std::cout << "Mock audio playback stopped" << std::endl;
        });
        
        return true;
    }
    
    void pa_stop_stream(AudioStream* stream) {
        if (!stream || !stream->running) {
            return;
        }
        
        stream->should_stop.store(true);
        if (stream->playback_thread.joinable()) {
            stream->playback_thread.join();
        }
        stream->running = false;
    }
    
    void pa_close_stream(AudioStream* stream) {
        if (stream) {
            pa_stop_stream(stream);
            delete stream;
        }
    }
    
    bool pa_write_stream(AudioStream* stream, const float* data, int frames) {
        if (!stream || !stream->running || !data || frames <= 0) {
            return false;
        }
        
        // Mock write - just simulate the time it takes
        (void)data; // Suppress unused parameter warning
        
        // Simulate write latency
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        
        return true;
    }
    
    double pa_get_stream_latency(AudioStream* stream) {
        if (!stream) {
            return 0.0;
        }
        
        // Calculate estimated latency based on buffer size
        return (double)stream->buffer_size / stream->sample_rate * 1000.0; // Convert to ms
    }
}}}

namespace AudioReceiver {
namespace Audio {

AudioOutput::AudioOutput(int sample_rate, int channels, int buffer_size, int device_id)
    : sample_rate_(sample_rate)
    , channels_(channels)
    , buffer_size_(buffer_size)
    , device_id_(device_id)
    , audio_stream_(nullptr)
    , initialized_(false)
    , running_(false)
    , frames_written_(0)
    , underruns_(0) {
    
    // Validate parameters according to PRD requirements
    if (sample_rate != SUPPORTED_SAMPLE_RATE) {
        std::cerr << "Unsupported sample rate: " << sample_rate 
                  << " (only " << SUPPORTED_SAMPLE_RATE << " Hz supported)" << std::endl;
        return;
    }
    
    if (channels != 1 && channels != 2) {
        std::cerr << "Unsupported channel count: " << channels 
                  << " (only mono/stereo supported)" << std::endl;
        return;
    }
    
    if (buffer_size < MIN_BUFFER_SIZE || buffer_size > MAX_BUFFER_SIZE) {
        std::cerr << "Invalid buffer size: " << buffer_size 
                  << " (must be between " << MIN_BUFFER_SIZE 
                  << " and " << MAX_BUFFER_SIZE << ")" << std::endl;
        return;
    }
    
    initialized_ = initialize();
}

AudioOutput::~AudioOutput() {
    cleanup();
}

bool AudioOutput::initialize() {
    // Initialize PortAudio
    if (!Mock::pa_initialize()) {
        std::cerr << "Failed to initialize PortAudio" << std::endl;
        return false;
    }
    
    // Open audio stream
    audio_stream_ = Mock::pa_open_stream(sample_rate_, channels_, buffer_size_, device_id_);
    if (!audio_stream_) {
        std::cerr << "Failed to open audio stream" << std::endl;
        return false;
    }
    
    std::cout << "Audio output initialized - " 
              << sample_rate_ << "Hz, " << channels_ << " channels, "
              << buffer_size_ << " samples buffer";
    if (device_id_ >= 0) {
        std::cout << ", device ID: " << device_id_;
    }
    std::cout << std::endl;
    
    return true;
}

void AudioOutput::cleanup() {
    stop();
    
    if (audio_stream_) {
        Mock::pa_close_stream(audio_stream_);
        audio_stream_ = nullptr;
    }
    
    Mock::pa_terminate();
    initialized_ = false;
}

bool AudioOutput::start() {
    if (!initialized_ || running_.load()) {
        return false;
    }
    
    if (!Mock::pa_start_stream(audio_stream_)) {
        std::cerr << "Failed to start audio stream" << std::endl;
        return false;
    }
    
    running_.store(true);
    return true;
}

void AudioOutput::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (audio_stream_) {
        Mock::pa_stop_stream(audio_stream_);
    }
}

bool AudioOutput::write_audio(const std::vector<float>& audio_data) {
    if (!initialized_ || !running_.load()) {
        return false;
    }
    
    // Validate audio data size
    int expected_samples = buffer_size_ * channels_;
    if (static_cast<int>(audio_data.size()) != expected_samples) {
        std::cerr << "Invalid audio data size: " << audio_data.size() 
                  << " (expected " << expected_samples << ")" << std::endl;
        return false;
    }
    
    // Write audio data
    bool success = Mock::pa_write_stream(audio_stream_, audio_data.data(), buffer_size_);
    if (success) {
        frames_written_.fetch_add(buffer_size_);
    } else {
        underruns_.fetch_add(1);
    }
    
    return success;
}

bool AudioOutput::is_initialized() const {
    return initialized_;
}

bool AudioOutput::is_running() const {
    return running_.load();
}

int AudioOutput::get_sample_rate() const {
    return sample_rate_;
}

int AudioOutput::get_channels() const {
    return channels_;
}

int AudioOutput::get_buffer_size() const {
    return buffer_size_;
}

int AudioOutput::get_device_id() const {
    return device_id_;
}

uint64_t AudioOutput::get_frames_written() const {
    return frames_written_.load();
}

uint64_t AudioOutput::get_underruns() const {
    return underruns_.load();
}

double AudioOutput::get_estimated_latency_ms() const {
    if (!initialized_ || !audio_stream_) {
        return 0.0;
    }
    
    return Mock::pa_get_stream_latency(audio_stream_);
}

std::vector<AudioDevice> AudioOutput::get_available_devices() {
    Mock::pa_initialize(); // Ensure PortAudio is initialized
    return Mock::pa_get_devices();
}

AudioDevice AudioOutput::get_default_device() {
    Mock::pa_initialize(); // Ensure PortAudio is initialized
    return Mock::pa_get_default_device();
}

} // namespace Audio
} // namespace AudioReceiver