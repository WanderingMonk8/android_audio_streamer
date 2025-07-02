#include "audio/opus_decoder.h"
#include "audio/mock_opus.h"
#include <iostream>
#include <cstring>

namespace AudioReceiver {
namespace Audio {

OpusDecoder::OpusDecoder(int sample_rate, int channels)
    : sample_rate_(sample_rate)
    , channels_(channels)
    , frame_size_samples_(0)
    , opus_decoder_(nullptr)
    , initialized_(false)
    , frames_decoded_(0)
    , decode_errors_(0) {
    
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
    
    // Calculate frame size for 2.5ms at 48kHz
    frame_size_samples_ = static_cast<int>(sample_rate * FRAME_DURATION_MS / 1000.0);
    
    initialized_ = initialize();
}

OpusDecoder::~OpusDecoder() {
    cleanup();
}

bool OpusDecoder::initialize() {
    int error;
    
    // Create Opus decoder
    opus_decoder_ = Mock::opus_decoder_create(sample_rate_, channels_, &error);
    if (error != OPUS_OK || opus_decoder_ == nullptr) {
        std::cerr << "Failed to create Opus decoder: " << Mock::opus_strerror(error) << std::endl;
        return false;
    }
    
    // Configure for low-latency CELT mode
    // Note: CELT-only mode is configured on the encoder side
    // The decoder automatically handles CELT frames
    
    std::cout << "Opus decoder initialized - " 
              << sample_rate_ << "Hz, " << channels_ << " channels, "
              << frame_size_samples_ << " samples/frame" << std::endl;
    
    return true;
}

void OpusDecoder::cleanup() {
    if (opus_decoder_) {
        Mock::opus_decoder_destroy(opus_decoder_);
        opus_decoder_ = nullptr;
    }
    initialized_ = false;
}

std::vector<float> OpusDecoder::decode(const std::vector<uint8_t>& encoded_data) {
    if (!initialized_ || encoded_data.empty()) {
        if (!encoded_data.empty()) {
            decode_errors_.fetch_add(1);
        }
        return {};
    }
    
    // Prepare output buffer
    std::vector<float> pcm_output(frame_size_samples_ * channels_);
    
    // Decode Opus frame
    int decoded_samples = Mock::opus_decode_float(
        opus_decoder_,
        encoded_data.data(),
        static_cast<int>(encoded_data.size()),
        pcm_output.data(),
        frame_size_samples_,
        0 // No FEC for now
    );
    
    if (decoded_samples < 0) {
        std::cerr << "Opus decode error: " << Mock::opus_strerror(decoded_samples) << std::endl;
        decode_errors_.fetch_add(1);
        return {};
    }
    
    // Resize output to actual decoded samples
    pcm_output.resize(decoded_samples * channels_);
    frames_decoded_.fetch_add(1);
    
    return pcm_output;
}

void OpusDecoder::reset() {
    if (initialized_ && opus_decoder_) {
        int result = Mock::opus_decoder_ctl(opus_decoder_, OPUS_RESET_STATE);
        if (result != OPUS_OK) {
            std::cerr << "Failed to reset Opus decoder: " << Mock::opus_strerror(result) << std::endl;
        }
    }
}

bool OpusDecoder::is_initialized() const {
    return initialized_;
}

int OpusDecoder::get_sample_rate() const {
    return sample_rate_;
}

int OpusDecoder::get_channels() const {
    return channels_;
}

int OpusDecoder::get_frame_size_samples() const {
    return frame_size_samples_;
}

int OpusDecoder::get_frame_size_bytes() const {
    return frame_size_samples_ * channels_ * static_cast<int>(sizeof(float));
}

uint64_t OpusDecoder::get_frames_decoded() const {
    return frames_decoded_.load();
}

uint64_t OpusDecoder::get_decode_errors() const {
    return decode_errors_.load();
}

} // namespace Audio
} // namespace AudioReceiver