#include "mock_audio_output.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

namespace AudioReceiver { namespace Audio { namespace Mock {

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

}}} // namespace AudioReceiver::Audio::Mock