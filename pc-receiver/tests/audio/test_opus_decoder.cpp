#include "audio/opus_decoder.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

using namespace AudioReceiver::Audio;

void test_opus_decoder_construction() {
    std::cout << "Testing Opus decoder construction..." << std::endl;
    
    // Test valid construction
    OpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    assert(decoder.get_sample_rate() == 48000);
    assert(decoder.get_channels() == 2);
    
    std::cout << "✓ Opus decoder construction test passed" << std::endl;
}

void test_opus_decoder_invalid_params() {
    std::cout << "Testing Opus decoder with invalid parameters..." << std::endl;
    
    // Test invalid sample rate
    OpusDecoder decoder1(44100, 2); // Should fail - only 48kHz supported
    assert(!decoder1.is_initialized());
    
    // Test invalid channels
    OpusDecoder decoder2(48000, 3); // Should fail - only mono/stereo supported
    assert(!decoder2.is_initialized());
    
    std::cout << "✓ Invalid parameters test passed" << std::endl;
}

void test_opus_decoder_decode_empty() {
    std::cout << "Testing Opus decoder with empty data..." << std::endl;
    
    OpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Test empty input
    std::vector<uint8_t> empty_data;
    auto result = decoder.decode(empty_data);
    assert(result.empty());
    
    std::cout << "✓ Empty data decode test passed" << std::endl;
}

void test_opus_decoder_decode_invalid() {
    std::cout << "Testing Opus decoder with invalid data..." << std::endl;
    
    OpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Test invalid Opus data
    std::vector<uint8_t> invalid_data = {0x01, 0x02, 0x03, 0x04};
    auto result = decoder.decode(invalid_data);
    assert(result.empty());
    
    std::cout << "✓ Invalid data decode test passed" << std::endl;
}

void test_opus_decoder_frame_size_calculation() {
    std::cout << "Testing Opus decoder frame size calculation..." << std::endl;
    
    OpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Test frame size for 2.5ms at 48kHz stereo
    // 48000 * 0.0025 = 120 samples per channel
    // 120 * 2 channels = 240 samples total
    // 240 * sizeof(float) = 960 bytes
    assert(decoder.get_frame_size_samples() == 120);
    assert(decoder.get_frame_size_bytes() == 960);
    
    std::cout << "✓ Frame size calculation test passed" << std::endl;
}

void test_opus_decoder_reset() {
    std::cout << "Testing Opus decoder reset..." << std::endl;
    
    OpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Reset should not fail
    decoder.reset();
    assert(decoder.is_initialized());
    
    std::cout << "✓ Decoder reset test passed" << std::endl;
}

void test_opus_decoder_statistics() {
    std::cout << "Testing Opus decoder statistics..." << std::endl;
    
    OpusDecoder decoder(48000, 2);
    assert(decoder.is_initialized());
    
    // Initial stats should be zero
    assert(decoder.get_frames_decoded() == 0);
    assert(decoder.get_decode_errors() == 0);
    
    // Test invalid decode increments error count
    std::vector<uint8_t> invalid_data = {0xFF, 0xFF, 0xFF, 0xFF};
    decoder.decode(invalid_data);
    assert(decoder.get_decode_errors() == 1);
    
    std::cout << "✓ Decoder statistics test passed" << std::endl;
}

void run_opus_decoder_tests() {
    std::cout << "=== Running Opus Decoder Tests ===" << std::endl;
    
    test_opus_decoder_construction();
    test_opus_decoder_invalid_params();
    test_opus_decoder_decode_empty();
    test_opus_decoder_decode_invalid();
    test_opus_decoder_frame_size_calculation();
    test_opus_decoder_reset();
    test_opus_decoder_statistics();
    
    std::cout << "=== All Opus Decoder Tests Passed ===" << std::endl;
}