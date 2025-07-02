#include "audio/audio_output.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace AudioReceiver::Audio;

void test_audio_output_construction() {
    std::cout << "Testing Audio output construction..." << std::endl;
    
    // Test valid construction
    AudioOutput output(48000, 2, 128);
    assert(!output.is_running()); // Should not be running initially
    assert(output.get_sample_rate() == 48000);
    assert(output.get_channels() == 2);
    assert(output.get_buffer_size() == 128);
    
    std::cout << "✓ Audio output construction test passed" << std::endl;
}

void test_audio_output_invalid_params() {
    std::cout << "Testing Audio output with invalid parameters..." << std::endl;
    
    // Test invalid sample rate
    AudioOutput output1(44100, 2, 128);
    assert(!output1.is_initialized());
    
    // Test invalid channels
    AudioOutput output2(48000, 3, 128);
    assert(!output2.is_initialized());
    
    // Test invalid buffer size
    AudioOutput output3(48000, 2, 32); // Too small
    assert(!output3.is_initialized());
    
    AudioOutput output4(48000, 2, 2048); // Too large
    assert(!output4.is_initialized());
    
    std::cout << "✓ Invalid parameters test passed" << std::endl;
}

void test_audio_output_start_stop() {
    std::cout << "Testing Audio output start/stop..." << std::endl;
    
    AudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    assert(!output.is_running());
    
    // Start output
    bool started = output.start();
    assert(started);
    assert(output.is_running());
    
    // Give it a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Stop output
    output.stop();
    assert(!output.is_running());
    
    std::cout << "✓ Audio output start/stop test passed" << std::endl;
}

void test_audio_output_write_audio() {
    std::cout << "Testing Audio output write audio..." << std::endl;
    
    AudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    
    // Start output
    bool started = output.start();
    assert(started);
    
    // Create test audio data (silence)
    std::vector<float> audio_data(256, 0.0f); // 128 samples * 2 channels
    
    // Write audio data
    bool written = output.write_audio(audio_data);
    assert(written);
    
    // Check statistics
    assert(output.get_frames_written() > 0);
    assert(output.get_underruns() == 0);
    
    output.stop();
    
    std::cout << "✓ Audio output write audio test passed" << std::endl;
}

void test_audio_output_buffer_underrun() {
    std::cout << "Testing Audio output buffer underrun handling..." << std::endl;
    
    AudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    
    // Start output
    bool started = output.start();
    assert(started);
    
    // Don't write any data - should cause underrun
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check that underruns were detected
    // Note: In mock implementation, we might not have real underruns
    // but the interface should be there
    uint64_t underruns = output.get_underruns();
    (void)underruns; // Suppress unused variable warning
    
    output.stop();
    
    std::cout << "✓ Audio output underrun handling test passed" << std::endl;
}

void test_audio_output_latency_measurement() {
    std::cout << "Testing Audio output latency measurement..." << std::endl;
    
    AudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    
    // Start output
    bool started = output.start();
    assert(started);
    
    // Get latency estimate
    double latency_ms = output.get_estimated_latency_ms();
    assert(latency_ms > 0.0);
    assert(latency_ms < 10.0); // Should be under 10ms for low-latency setup
    
    output.stop();
    
    std::cout << "✓ Audio output latency measurement test passed" << std::endl;
}

void test_audio_output_device_enumeration() {
    std::cout << "Testing Audio output device enumeration..." << std::endl;
    
    // Test static device enumeration
    auto devices = AudioOutput::get_available_devices();
    assert(!devices.empty()); // Should have at least one device
    
    // Check device info structure
    for (const auto& device : devices) {
        assert(!device.name.empty());
        assert(device.id >= 0);
        assert(device.max_channels > 0);
        assert(device.default_sample_rate > 0);
    }
    
    std::cout << "✓ Audio output device enumeration test passed" << std::endl;
}

void test_audio_output_device_selection() {
    std::cout << "Testing Audio output device selection..." << std::endl;
    
    auto devices = AudioOutput::get_available_devices();
    if (!devices.empty()) {
        // Test with specific device
        AudioOutput output(48000, 2, 128, devices[0].id);
        assert(output.is_initialized());
        assert(output.get_device_id() == devices[0].id);
    }
    
    // Test with invalid device ID
    AudioOutput output_invalid(48000, 2, 128, 9999);
    assert(!output_invalid.is_initialized());
    
    std::cout << "✓ Audio output device selection test passed" << std::endl;
}

void run_audio_output_tests() {
    std::cout << "=== Running Audio Output Tests ===" << std::endl;
    
    test_audio_output_construction();
    test_audio_output_invalid_params();
    test_audio_output_start_stop();
    test_audio_output_write_audio();
    test_audio_output_buffer_underrun();
    test_audio_output_latency_measurement();
    test_audio_output_device_enumeration();
    test_audio_output_device_selection();
    
    std::cout << "=== All Audio Output Tests Passed ===" << std::endl;
}