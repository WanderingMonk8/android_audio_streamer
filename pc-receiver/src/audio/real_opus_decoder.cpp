#include "audio/real_opus_decoder.h"

// Check if we have libopus available
#ifdef HAVE_LIBOPUS
#include <opus/opus.h>
#else
// Fallback to mock implementation if libopus is not available
#include "audio/mock_opus.h"
#include <iostream>
#include <cstring>

// Use mock functions when libopus is not available
using AudioReceiver::Audio::Mock::opus_decoder_create;
using AudioReceiver::Audio::Mock::opus_decoder_destroy;
using AudioReceiver::Audio::Mock::opus_decode_float;
using AudioReceiver::Audio::Mock::opus_decoder_ctl;
using AudioReceiver::Audio::Mock::opus_strerror;
#endif

namespace AudioReceiver {
namespace Audio {

RealOpusDecoder::RealOpusDecoder(int sample_rate, int channels)
    : sample_rate_(sample_rate)
    , channels_(channels)
    , frame_size_samples_(0)
    , opus_decoder_(nullptr)
    , initialized_(false)
    , frames_decoded_(0)
    , decode_errors_(0)
    , fec_recoveries_(0) {
    
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

RealOpusDecoder::~RealOpusDecoder() {
    cleanup();
}

bool RealOpusDecoder::initialize() {
    int error;
    
    // Create Opus decoder
    opus_decoder_ = opus_decoder_create(sample_rate_, channels_, &error);
    if (error != OPUS_OK || opus_decoder_ == nullptr) {
        std::cerr << "Failed to create Opus decoder: " << opus_strerror(error) << std::endl;
        return false;
    }
    
    // Configure for low-latency CELT mode
    // Note: CELT-only mode is configured on the encoder side
    // The decoder automatically handles CELT frames
    
#ifndef HAVE_LIBOPUS
    std::cout << "Real Opus decoder initialized (using mock implementation) - " 
              << sample_rate_ << "Hz, " << channels_ << " channels, "
              << frame_size_samples_ << " samples/frame" << std::endl;
#else
    std::cout << "Real Opus decoder initialized (using libopus) - " 
              << sample_rate_ << "Hz, " << channels_ << " channels, "
              << frame_size_samples_ << " samples/frame" << std::endl;
#endif
    
    return true;
}

void RealOpusDecoder::cleanup() {
    if (opus_decoder_) {
        opus_decoder_destroy(opus_decoder_);
        opus_decoder_ = nullptr;
    }
    initialized_ = false;
}

std::vector<float> RealOpusDecoder::decode(const std::vector<uint8_t>& encoded_data) {
    if (!initialized_ || encoded_data.empty()) {
        if (!encoded_data.empty()) {
            decode_errors_.fetch_add(1);
        }
        return {};
    }
    
    // Prepare output buffer
    std::vector<float> pcm_output(frame_size_samples_ * channels_);
    
    // Decode Opus frame
    int decoded_samples = opus_decode_float(
        opus_decoder_,
        encoded_data.data(),
        static_cast<int>(encoded_data.size()),
        pcm_output.data(),
        frame_size_samples_,
        0 // No FEC for regular decode
    );
    
    if (decoded_samples < 0) {
        std::cerr << "Opus decode error: " << opus_strerror(decoded_samples) << std::endl;
        decode_errors_.fetch_add(1);
        return {};
    }
    
    // Resize output to actual decoded samples
    pcm_output.resize(decoded_samples * channels_);
    frames_decoded_.fetch_add(1);
    
    return pcm_output;
}

std::vector<float> RealOpusDecoder::decode_with_fec(const std::vector<uint8_t>& encoded_data) {
    if (!initialized_) {
        return {};
    }
    
    // Prepare output buffer
    std::vector<float> pcm_output(frame_size_samples_ * channels_);
    
    int decoded_samples;
    
    if (encoded_data.empty()) {
        // Use FEC to recover from packet loss
        decoded_samples = opus_decode_float(
            opus_decoder_,
            nullptr,
            0,
            pcm_output.data(),
            frame_size_samples_,
            1 // Enable FEC
        );
        
        if (decoded_samples > 0) {
            fec_recoveries_.fetch_add(1);
        }
    } else {
        // Regular decode with FEC enabled
        decoded_samples = opus_decode_float(
            opus_decoder_,
            encoded_data.data(),
            static_cast<int>(encoded_data.size()),
            pcm_output.data(),
            frame_size_samples_,
            1 // Enable FEC
        );
    }
    
    if (decoded_samples < 0) {
        std::cerr << "Opus FEC decode error: " << opus_strerror(decoded_samples) << std::endl;
        decode_errors_.fetch_add(1);
        return {};
    }
    
    // Resize output to actual decoded samples
    pcm_output.resize(decoded_samples * channels_);
    frames_decoded_.fetch_add(1);
    
    return pcm_output;
}

std::vector<float> RealOpusDecoder::generate_silence() {
    if (!initialized_) {
        return {};
    }
    
    // Generate silence frame
    std::vector<float> silence(frame_size_samples_ * channels_, 0.0f);
    return silence;
}

void RealOpusDecoder::reset() {
    if (initialized_ && opus_decoder_) {
        int result = opus_decoder_ctl(opus_decoder_, OPUS_RESET_STATE);
        if (result != OPUS_OK) {
            std::cerr << "Failed to reset Opus decoder: " << opus_strerror(result) << std::endl;
        }
    }
}

bool RealOpusDecoder::is_initialized() const {
    return initialized_;
}

int RealOpusDecoder::get_sample_rate() const {
    return sample_rate_;
}

int RealOpusDecoder::get_channels() const {
    return channels_;
}

int RealOpusDecoder::get_frame_size_samples() const {
    return frame_size_samples_;
}

int RealOpusDecoder::get_frame_size_bytes() const {
    return frame_size_samples_ * channels_ * static_cast<int>(sizeof(float));
}

uint64_t RealOpusDecoder::get_frames_decoded() const {
    return frames_decoded_.load();
}

uint64_t RealOpusDecoder::get_decode_errors() const {
    return decode_errors_.load();
}

uint64_t RealOpusDecoder::get_fec_recoveries() const {
    return fec_recoveries_.load();
}

} // namespace Audio
} // namespace AudioReceiver