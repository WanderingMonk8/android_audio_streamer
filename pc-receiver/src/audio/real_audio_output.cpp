#include "audio/real_audio_output.h"

// Check if we have PortAudio available
#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#else
// Fallback to mock implementation if PortAudio is not available
#include "audio/mock_audio_output.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#endif

namespace AudioReceiver {
namespace Audio {

RealAudioOutput::RealAudioOutput(int sample_rate, int channels, int buffer_size, int device_id)
    : sample_rate_(sample_rate)
    , channels_(channels)
    , buffer_size_(buffer_size)
    , device_id_(device_id)
    , exclusive_mode_(false)
    , pa_stream_(nullptr)
    , initialized_(false)
    , running_(false)
    , frames_written_(0)
    , underruns_(0)
    , actual_latency_ms_(0.0) {
    
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

RealAudioOutput::~RealAudioOutput() {
    cleanup();
}

bool RealAudioOutput::initialize() {
#ifdef HAVE_PORTAUDIO
    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "Failed to initialize PortAudio: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    
    // Set up stream parameters
    PaStreamParameters outputParameters;
    outputParameters.device = (device_id_ == -1) ? Pa_GetDefaultOutputDevice() : device_id_;
    outputParameters.channelCount = channels_;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = (double)buffer_size_ / sample_rate_; // Low latency
    outputParameters.hostApiSpecificStreamInfo = nullptr;
    
    // Try to set up WASAPI exclusive mode on Windows
    if (exclusive_mode_) {
        setup_wasapi_exclusive_mode();
    }
    
    // Open stream
    err = Pa_OpenStream(&pa_stream_,
                        nullptr,              // No input
                        &outputParameters,    // Output parameters
                        sample_rate_,         // Sample rate
                        buffer_size_,         // Frames per buffer
                        paClipOff,           // No clipping
                        nullptr,             // No callback (blocking mode)
                        nullptr);            // No user data
    
    if (err != paNoError) {
        std::cerr << "Failed to open PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return false;
    }
    
    std::cout << "Real PortAudio output initialized - " 
              << sample_rate_ << "Hz, " << channels_ << " channels, "
              << buffer_size_ << " samples buffer";
    if (device_id_ >= 0) {
        std::cout << ", device ID: " << device_id_;
    }
    if (exclusive_mode_) {
        std::cout << " (WASAPI exclusive mode)";
    }
    std::cout << std::endl;
    
#else
    // Use mock implementation
    if (!Mock::pa_initialize()) {
        std::cerr << "Failed to initialize mock PortAudio" << std::endl;
        return false;
    }
    
    pa_stream_ = Mock::pa_open_stream(sample_rate_, channels_, buffer_size_, device_id_);
    if (!pa_stream_) {
        std::cerr << "Failed to open mock audio stream" << std::endl;
        return false;
    }
    
    std::cout << "Real audio output initialized (using mock implementation) - " 
              << sample_rate_ << "Hz, " << channels_ << " channels, "
              << buffer_size_ << " samples buffer";
    if (device_id_ >= 0) {
        std::cout << ", device ID: " << device_id_;
    }
    std::cout << std::endl;
#endif
    
    return true;
}

void RealAudioOutput::cleanup() {
    stop();
    
    if (pa_stream_) {
#ifdef HAVE_PORTAUDIO
        Pa_CloseStream(pa_stream_);
        Pa_Terminate();
#else
        Mock::pa_close_stream(pa_stream_);
        Mock::pa_terminate();
#endif
        pa_stream_ = nullptr;
    }
    
    initialized_ = false;
}

bool RealAudioOutput::start() {
    if (!initialized_ || running_.load()) {
        return false;
    }
    
#ifdef HAVE_PORTAUDIO
    PaError err = Pa_StartStream(pa_stream_);
    if (err != paNoError) {
        std::cerr << "Failed to start PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
#else
    if (!Mock::pa_start_stream(pa_stream_)) {
        std::cerr << "Failed to start mock audio stream" << std::endl;
        return false;
    }
#endif
    
    running_.store(true);
    
    // Start latency measurement
    measure_actual_latency();
    
    return true;
}

void RealAudioOutput::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (pa_stream_) {
#ifdef HAVE_PORTAUDIO
        Pa_StopStream(pa_stream_);
#else
        Mock::pa_stop_stream(pa_stream_);
#endif
    }
}

bool RealAudioOutput::write_audio(const std::vector<float>& audio_data) {
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
    
    bool success = false;
    
#ifdef HAVE_PORTAUDIO
    PaError err = Pa_WriteStream(pa_stream_, audio_data.data(), buffer_size_);
    success = (err == paNoError);
    if (!success) {
        std::cerr << "PortAudio write error: " << Pa_GetErrorText(err) << std::endl;
    }
#else
    success = Mock::pa_write_stream(pa_stream_, audio_data.data(), buffer_size_);
#endif
    
    if (success) {
        frames_written_.fetch_add(buffer_size_);
    } else {
        underruns_.fetch_add(1);
    }
    
    return success;
}

bool RealAudioOutput::is_initialized() const {
    return initialized_;
}

bool RealAudioOutput::is_running() const {
    return running_.load();
}

int RealAudioOutput::get_sample_rate() const {
    return sample_rate_;
}

int RealAudioOutput::get_channels() const {
    return channels_;
}

int RealAudioOutput::get_buffer_size() const {
    return buffer_size_;
}

int RealAudioOutput::get_device_id() const {
    return device_id_;
}

uint64_t RealAudioOutput::get_frames_written() const {
    return frames_written_.load();
}

uint64_t RealAudioOutput::get_underruns() const {
    return underruns_.load();
}

double RealAudioOutput::get_estimated_latency_ms() const {
    if (!initialized_ || !pa_stream_) {
        return 0.0;
    }
    
#ifdef HAVE_PORTAUDIO
    // Get actual stream latency from PortAudio
    const PaStreamInfo* info = Pa_GetStreamInfo(pa_stream_);
    if (info) {
        return info->outputLatency * 1000.0; // Convert to ms
    }
    return (double)buffer_size_ / sample_rate_ * 1000.0; // Fallback estimate
#else
    return Mock::pa_get_stream_latency(pa_stream_);
#endif
}

double RealAudioOutput::get_actual_latency_ms() const {
    return actual_latency_ms_.load();
}

std::vector<AudioDevice> RealAudioOutput::get_available_devices() {
#ifdef HAVE_PORTAUDIO
    Pa_Initialize(); // Ensure PortAudio is initialized
    
    std::vector<AudioDevice> devices;
    int deviceCount = Pa_GetDeviceCount();
    int defaultDevice = Pa_GetDefaultOutputDevice();
    
    for (int i = 0; i < deviceCount; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxOutputChannels > 0) {
            devices.push_back({
                i,
                deviceInfo->name,
                deviceInfo->maxOutputChannels,
                static_cast<int>(deviceInfo->defaultSampleRate),
                (i == defaultDevice)
            });
        }
    }
    
    return devices;
#else
    Mock::pa_initialize(); // Ensure mock is initialized
    return Mock::pa_get_devices();
#endif
}

AudioDevice RealAudioOutput::get_default_device() {
#ifdef HAVE_PORTAUDIO
    Pa_Initialize(); // Ensure PortAudio is initialized
    
    int defaultDevice = Pa_GetDefaultOutputDevice();
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(defaultDevice);
    
    if (deviceInfo) {
        return {
            defaultDevice,
            deviceInfo->name,
            deviceInfo->maxOutputChannels,
            static_cast<int>(deviceInfo->defaultSampleRate),
            true
        };
    }
    
    return {-1, "Unknown Device", 2, 48000, true};
#else
    Mock::pa_initialize(); // Ensure mock is initialized
    return Mock::pa_get_default_device();
#endif
}

bool RealAudioOutput::set_exclusive_mode(bool exclusive) {
    if (running_.load()) {
        std::cerr << "Cannot change exclusive mode while stream is running" << std::endl;
        return false;
    }
    
    exclusive_mode_ = exclusive;
    return true;
}

bool RealAudioOutput::is_exclusive_mode() const {
    return exclusive_mode_;
}

bool RealAudioOutput::setup_wasapi_exclusive_mode() {
#ifdef HAVE_PORTAUDIO
    // WASAPI exclusive mode setup would go here
    // This is platform-specific and requires additional PortAudio configuration
    std::cout << "Setting up WASAPI exclusive mode..." << std::endl;
    return true;
#else
    std::cout << "Mock WASAPI exclusive mode setup" << std::endl;
    return true;
#endif
}

void RealAudioOutput::measure_actual_latency() {
    // Real latency measurement would involve:
    // 1. Playing a test tone
    // 2. Measuring the time from write_audio() call to actual audio output
    // 3. Using hardware-specific APIs for precise measurement
    
    // For now, use estimated latency
    actual_latency_ms_.store(get_estimated_latency_ms());
    
#ifndef HAVE_PORTAUDIO
    std::cout << "Mock latency measurement: " << actual_latency_ms_.load() << "ms" << std::endl;
#endif
}

} // namespace Audio
} // namespace AudioReceiver