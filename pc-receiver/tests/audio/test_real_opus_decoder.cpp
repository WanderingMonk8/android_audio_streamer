#include "audio/real_opus_decoder.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

using namespace AudioReceiver::Audio;

void test_real_opus_decoder_construction() {
    std::cout << "Testing Real Opus decoder construction..." << std::endl;
    
    // Test valid construction
    RealOpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    assert(decoder.get_sample_rate() == 48000);
    assert(decoder.get_channels() == 2);
    
    std::cout << "✓ Real Opus decoder construction test passed" << std::endl;
}

void test_real_opus_decoder_invalid_params() {
    std::cout << "Testing Real Opus decoder with invalid parameters..." << std::endl;
    
    // Test invalid sample rate
    RealOpusDecoder decoder1(44100, 2); // Should fail - only 48kHz supported
    assert(!decoder1.is_initialized());
    
    // Test invalid channels
    RealOpusDecoder decoder2(48000, 3); // Should fail - only mono/stereo supported
    assert(!decoder2.is_initialized());
    
    std::cout << "✓ Real Opus decoder invalid parameters test passed" << std::endl;
}

void test_real_opus_decoder_decode_empty() {
    std::cout << "Testing Real Opus decoder with empty data..." << std::endl;
    
    RealOpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Test empty input
    std::vector<uint8_t> empty_data;
    auto result = decoder.decode(empty_data);
    assert(result.empty());
    
    std::cout << "✓ Real Opus decoder empty data test passed" << std::endl;
}

void test_real_opus_decoder_decode_invalid() {
    std::cout << "Testing Real Opus decoder with invalid data..." << std::endl;
    
    RealOpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Test invalid Opus data
    std::vector<uint8_t> invalid_data = {0x01, 0x02, 0x03, 0x04};
    auto result = decoder.decode(invalid_data);
    assert(result.empty());
    
    // Check that error was counted
    assert(decoder.get_decode_errors() > 0);
    
    std::cout << "✓ Real Opus decoder invalid data test passed" << std::endl;
}

void test_real_opus_decoder_frame_size_calculation() {
    std::cout << "Testing Real Opus decoder frame size calculation..." << std::endl;
    
    RealOpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Test frame size for 2.5ms at 48kHz stereo
    // 48000 * 0.0025 = 120 samples per channel
    // 120 * 2 channels = 240 samples total
    // 240 * sizeof(float) = 960 bytes
    assert(decoder.get_frame_size_samples() == 120);
    assert(decoder.get_frame_size_bytes() == 960);
    
    std::cout << "✓ Real Opus decoder frame size calculation test passed" << std::endl;
}

void test_real_opus_decoder_reset() {
    std::cout << "Testing Real Opus decoder reset..." << std::endl;
    
    RealOpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Reset should not fail
    decoder.reset();
    assert(decoder.is_initialized());
    
    std::cout << "✓ Real Opus decoder reset test passed" << std::endl;
}

void test_real_opus_decoder_statistics() {
    std::cout << "Testing Real Opus decoder statistics..." << std::endl;
    
    RealOpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Initial stats should be zero
    assert(decoder.get_frames_decoded() == 0);
    assert(decoder.get_decode_errors() == 0);
    
    // Test invalid decode increments error count
    std::vector<uint8_t> invalid_data = {0xFF, 0xFF, 0xFF, 0xFF};
    decoder.decode(invalid_data);
    assert(decoder.get_decode_errors() == 1);
    
    std::cout << "✓ Real Opus decoder statistics test passed" << std::endl;
}

void test_real_opus_decoder_silence_generation() {
    std::cout << "Testing Real Opus decoder silence generation..." << std::endl;
    
    RealOpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Test silence generation for packet loss
    auto silence = decoder.generate_silence();
    assert(!silence.empty());
    assert(static_cast<int>(silence.size()) == decoder.get_frame_size_samples() * decoder.get_channels());
    
    // Verify it's actually silence (all zeros)
    for (float sample : silence) {
        assert(sample == 0.0f);
    }
    
    std::cout << "✓ Real Opus decoder silence generation test passed" << std::endl;
}

void test_real_opus_decoder_packet_loss_concealment() {
    std::cout << "Testing Real Opus decoder packet loss concealment..." << std::endl;
    
    RealOpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Test packet loss concealment (FEC)
    auto concealed = decoder.decode_with_fec();
    assert(!concealed.empty());
    assert(static_cast<int>(concealed.size()) == decoder.get_frame_size_samples() * decoder.get_channels());
    
    std::cout << "✓ Real Opus decoder packet loss concealment test passed" << std::endl;
}

void run_real_opus_decoder_tests() {
    std::cout << "=== Running Real Opus Decoder Tests ===" << std::endl;
    
    test_real_opus_decoder_construction();
    test_real_opus_decoder_invalid_params();
    test_real_opus_decoder_decode_empty();
    test_real_opus_decoder_decode_invalid();
    test_real_opus_decoder_frame_size_calculation();
    test_real_opus_decoder_reset();
    test_real_opus_decoder_statistics();
    test_real_opus_decoder_silence_generation();
    test_real_opus_decoder_packet_loss_concealment();
    
    std::cout << "=== All Real Opus Decoder Tests Passed ===" << std::endl;
}