#include "mock_opus.h"

namespace AudioReceiver { namespace Audio { namespace Mock {
    
    OpusDecoder* opus_decoder_create(int sample_rate, int channels, int* error) {
        if (sample_rate != 48000 || (channels != 1 && channels != 2)) {
            *error = OPUS_BAD_ARG;
            return nullptr;
        }
        *error = OPUS_OK;
        return new OpusDecoder{sample_rate, channels};
    }
    
    void opus_decoder_destroy(OpusDecoder* decoder) {
        delete decoder;
    }
    
    int opus_decode_float(OpusDecoder* decoder, const unsigned char* data, int len,
                         float* pcm, int frame_size, int decode_fec) {
        (void)decode_fec;
        
        if (!pcm || frame_size <= 0) {
            return OPUS_BAD_ARG;
        }
        
        // Simulate invalid Opus data detection
        if (len > 0 && data) {
            if (len < 8 || (data[0] == 0x01 && data[1] == 0x02 && data[2] == 0x03 && data[3] == 0x04)) {
                return OPUS_INVALID_PACKET;
            }
        }
        
        // For valid-looking data or FEC, return silence
        if (len > 0 || decode_fec) {
            for (int i = 0; i < frame_size * decoder->channels; ++i) {
                pcm[i] = 0.0f;
            }
            return frame_size;
        }
        
        return OPUS_BAD_ARG;
    }
    
    int opus_decoder_ctl(OpusDecoder* decoder, int request, ...) {
        (void)decoder; (void)request;
        return OPUS_OK;
    }
    
    const char* opus_strerror(int error) {
        switch (error) {
            case OPUS_OK: return "OK";
            case OPUS_BAD_ARG: return "Bad argument";
            case OPUS_INVALID_PACKET: return "Invalid packet";
            default: return "Unknown error";
        }
    }
    
}}}