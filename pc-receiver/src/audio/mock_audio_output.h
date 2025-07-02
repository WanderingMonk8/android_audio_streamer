#pragma once

#include "audio_output.h"
#include <vector>
#include <thread>
#include <atomic>

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
bool pa_initialize();
void pa_terminate();
std::vector<AudioDevice> pa_get_devices();
AudioDevice pa_get_default_device();
AudioStream* pa_open_stream(int sample_rate, int channels, int buffer_size, int device_id);
bool pa_start_stream(AudioStream* stream);
void pa_stop_stream(AudioStream* stream);
void pa_close_stream(AudioStream* stream);
bool pa_write_stream(AudioStream* stream, const float* data, int frames);
double pa_get_stream_latency(AudioStream* stream);

}}} // namespace AudioReceiver::Audio::Mock