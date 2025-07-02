#include "audio/audio_output.h"
#include "audio/mock_audio_output.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

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