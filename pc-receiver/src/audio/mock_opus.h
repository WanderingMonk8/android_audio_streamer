#pragma once

#include <cstdint>

// Mock Opus constants and functions for compilation without libopus
#define OPUS_OK 0
#define OPUS_BAD_ARG -1
#define OPUS_INVALID_PACKET -4
#define OPUS_RESET_STATE 4028

namespace AudioReceiver { namespace Audio { namespace Mock {
    struct OpusDecoder {
        int sample_rate;
        int channels;
    };
    
    // Function declarations only - implementations in mock_opus.cpp
    OpusDecoder* opus_decoder_create(int sample_rate, int channels, int* error);
    void opus_decoder_destroy(OpusDecoder* decoder);
    int opus_decode_float(OpusDecoder* decoder, const unsigned char* data, int len,
                         float* pcm, int frame_size, int decode_fec);
    int opus_decoder_ctl(OpusDecoder* decoder, int request, ...);
    const char* opus_strerror(int error);
}}}