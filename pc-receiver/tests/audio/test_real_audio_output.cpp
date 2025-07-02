#include "audio/real_audio_output.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace AudioReceiver::Audio;

void test_real_audio_output_construction() {
    std::cout << "Testing Real Audio output construction..." << std::endl;
    
    // Test valid construction
    RealAudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    assert(!output.is_running()); // Should not be running initially
    assert(output.get_sample_rate() == 48000);
    assert(output.get_channels() == 2);
    assert(output.get_buffer_size() == 128);
    assert(!output.is_exclusive_mode()); // Should default to false
    
    std::cout << "✓ Real Audio output construction test passed" << std::endl;
}

void test_real_audio_output_invalid_params() {
    std::cout << "Testing Real Audio output with invalid parameters..." << std::endl;
    
    // Test invalid sample rate
    RealAudioOutput output1(44100, 2, 128);
    assert(!output1.is_initialized());
    
    // Test invalid channels
    RealAudioOutput output2(48000, 3, 128);
    assert(!output2.is_initialized());
    
    // Test invalid buffer size
    RealAudioOutput output3(48000, 2, 32); // Too small
    assert(!output3.is_initialized());
    
    RealAudioOutput output4(48000, 2, 2048); // Too large
    assert(!output4.is_initialized());
    
    std::cout << "✓ Real Audio output invalid parameters test passed" << std::endl;
}

void test_real_audio_output_start_stop() {
    std::cout << "Testing Real Audio output start/stop..." << std::endl;
    
    RealAudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    assert(!output.is_running());
    
    // Test start
    bool started = output.start();
    assert(started);
    assert(output.is_running());
    
    // Test stop
    output.stop();
    assert(!output.is_running());
    
    // Test double start protection
    assert(output.start());
    assert(!output.start()); // Should fail on second start
    
    output.stop();
    
    std::cout << "✓ Real Audio output start/stop test passed" << std::endl;
}

void test_real_audio_output_write_audio() {
    std::cout << "Testing Real Audio output write audio..." << std::endl;
    
    RealAudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    
    // Test write without starting
    std::vector<float> audio_data(128 * 2, 0.5f); // 128 samples, 2 channels
    assert(!output.write_audio(audio_data)); // Should fail - not started
    
    // Start output
    assert(output.start());
    
    // Test valid write
    assert(output.write_audio(audio_data));
    assert(output.get_frames_written() == 128);
    
    // Test invalid data size
    std::vector<float> wrong_size(100, 0.5f);
    assert(!output.write_audio(wrong_size));
    
    // Test multiple writes
    assert(output.write_audio(audio_data));
    assert(output.get_frames_written() == 256);
    
    output.stop();
    
    std::cout << "✓ Real Audio output write audio test passed" << std::endl;
}

void test_real_audio_output_device_management() {
    std::cout << "Testing Real Audio output device management..." << std::endl;
    
    // Test device enumeration
    auto devices = RealAudioOutput::get_available_devices();
    assert(!devices.empty()); // Should have at least one device
    
    // Test default device
    auto default_device = RealAudioOutput::get_default_device();
    assert(default_device.id >= 0 || default_device.id == -1); // Valid device ID
    assert(!default_device.name.empty());
    assert(default_device.max_channels > 0);
    assert(default_device.default_sample_rate > 0);
    
    // Find default device in list
    bool found_default = false;
    for (const auto& device : devices) {
        if (device.is_default) {
            found_default = true;
            break;
        }
    }
    assert(found_default || devices.size() == 1); // Should have a default device
    
    std::cout << "✓ Real Audio output device management test passed" << std::endl;
}

void test_real_audio_output_latency_measurement() {
    std::cout << "Testing Real Audio output latency measurement..." << std::endl;
    
    RealAudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    
    // Test latency measurement before start
    double initial_latency = output.get_estimated_latency_ms();
    assert(initial_latency >= 0.0);
    
    // Start and test latency measurement
    assert(output.start());
    
    double running_latency = output.get_estimated_latency_ms();
    assert(running_latency > 0.0);
    assert(running_latency <= 20.0); // Should be reasonable for low-latency audio
    
    double actual_latency = output.get_actual_latency_ms();
    assert(actual_latency >= 0.0);
    
    output.stop();
    
    std::cout << "✓ Real Audio output latency measurement test passed" << std::endl;
}

void test_real_audio_output_exclusive_mode() {
    std::cout << "Testing Real Audio output exclusive mode..." << std::endl;
    
    RealAudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    assert(!output.is_exclusive_mode()); // Default should be false
    
    // Test setting exclusive mode before start
    assert(output.set_exclusive_mode(true));
    assert(output.is_exclusive_mode());
    
    assert(output.set_exclusive_mode(false));
    assert(!output.is_exclusive_mode());
    
    // Test setting exclusive mode while running
    assert(output.start());
    assert(!output.set_exclusive_mode(true)); // Should fail while running
    
    output.stop();
    
    std::cout << "✓ Real Audio output exclusive mode test passed" << std::endl;
}

void test_real_audio_output_performance_metrics() {
    std::cout << "Testing Real Audio output performance metrics..." << std::endl;
    
    RealAudioOutput output(48000, 2, 128);
    assert(output.is_initialized());
    
    // Initial metrics should be zero
    assert(output.get_frames_written() == 0);
    assert(output.get_underruns() == 0);
    
    assert(output.start());
    
    // Write some audio data
    std::vector<float> audio_data(128 * 2, 0.5f);
    for (int i = 0; i < 10; i++) {
        assert(output.write_audio(audio_data));
    }
    
    // Check metrics
    assert(output.get_frames_written() == 1280); // 10 * 128 frames
    // Underruns should be 0 for successful writes
    
    output.stop();
    
    std::cout << "✓ Real Audio output performance metrics test passed" << std::endl;
}

void test_real_audio_output_stress_test() {
    std::cout << "Testing Real Audio output stress test..." << std::endl;
    
    RealAudioOutput output(48000, 2, 64); // Smaller buffer for stress test
    assert(output.is_initialized());
    assert(output.start());
    
    // Rapid writes to test stability
    std::vector<float> audio_data(64 * 2, 0.1f);
    int successful_writes = 0;
    
    for (int i = 0; i < 100; i++) {
        if (output.write_audio(audio_data)) {
            successful_writes++;
        }
        
        // Small delay to simulate real-time audio
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    assert(successful_writes > 90); // Should succeed most of the time
    assert(output.get_frames_written() == static_cast<uint64_t>(successful_writes) * 64);
    
    output.stop();
    
    std::cout << "✓ Real Audio output stress test passed" << std::endl;
}

// Test runner
void run_real_audio_output_tests() {
    std::cout << "\n=== Running Real Audio Output Tests ===" << std::endl;
    
    test_real_audio_output_construction();
    test_real_audio_output_invalid_params();
    test_real_audio_output_start_stop();
    test_real_audio_output_write_audio();
    test_real_audio_output_device_management();
    test_real_audio_output_latency_measurement();
    test_real_audio_output_exclusive_mode();
    test_real_audio_output_performance_metrics();
    test_real_audio_output_stress_test();
    
    std::cout << "\n✅ All Real Audio Output tests passed!" << std::endl;
}