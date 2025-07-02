#include "audio/audio_pipeline.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace AudioReceiver::Audio;

void test_audio_pipeline_construction() {
    std::cout << "Testing Audio pipeline construction..." << std::endl;
    
    // Test valid construction
    AudioPipeline pipeline(48000, 2, 128, 5); // 48kHz, stereo, 128 buffer, 5 packet jitter buffer
    assert(pipeline.is_initialized());
    assert(!pipeline.is_running());
    assert(pipeline.get_sample_rate() == 48000);
    assert(pipeline.get_channels() == 2);
    assert(pipeline.get_buffer_size() == 128);
    assert(pipeline.get_jitter_buffer_capacity() == 5);
    
    std::cout << "✓ Audio pipeline construction test passed" << std::endl;
}

void test_audio_pipeline_invalid_params() {
    std::cout << "Testing Audio pipeline with invalid parameters..." << std::endl;
    
    // Test invalid sample rate
    AudioPipeline pipeline1(44100, 2, 128, 5);
    assert(!pipeline1.is_initialized());
    
    // Test invalid channels
    AudioPipeline pipeline2(48000, 3, 128, 5);
    assert(!pipeline2.is_initialized());
    
    // Test invalid buffer size
    AudioPipeline pipeline3(48000, 2, 32, 5);
    assert(!pipeline3.is_initialized());
    
    // Test invalid jitter buffer capacity
    AudioPipeline pipeline4(48000, 2, 128, 0);
    assert(!pipeline4.is_initialized());
    
    std::cout << "✓ Invalid parameters test passed" << std::endl;
}

void test_audio_pipeline_start_stop() {
    std::cout << "Testing Audio pipeline start/stop..." << std::endl;
    
    AudioPipeline pipeline(48000, 2, 128, 5);
    assert(pipeline.is_initialized());
    assert(!pipeline.is_running());
    
    // Start pipeline
    bool started = pipeline.start();
    assert(started);
    assert(pipeline.is_running());
    
    // Give it a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop pipeline
    pipeline.stop();
    assert(!pipeline.is_running());
    
    std::cout << "✓ Audio pipeline start/stop test passed" << std::endl;
}

void test_audio_pipeline_process_packet() {
    std::cout << "Testing Audio pipeline packet processing..." << std::endl;
    
    AudioPipeline pipeline(48000, 2, 128, 5);
    assert(pipeline.is_initialized());
    
    // Start pipeline
    bool started = pipeline.start();
    assert(started);
    
    // Create mock encoded audio packet (simulate Opus data)
    std::vector<uint8_t> encoded_data(64, 0xAA); // Mock Opus packet
    
    // Process packet
    bool processed = pipeline.process_audio_packet(1, 1000, encoded_data);
    assert(processed);
    
    // Check statistics
    assert(pipeline.get_packets_processed() > 0);
    
    pipeline.stop();
    
    std::cout << "✓ Audio pipeline packet processing test passed" << std::endl;
}

void test_audio_pipeline_multiple_packets() {
    std::cout << "Testing Audio pipeline multiple packet processing..." << std::endl;
    
    AudioPipeline pipeline(48000, 2, 128, 5);
    assert(pipeline.is_initialized());
    
    // Start pipeline
    bool started = pipeline.start();
    assert(started);
    
    // Process multiple packets
    std::vector<uint8_t> encoded_data(64, 0xBB);
    
    for (uint32_t i = 1; i <= 3; ++i) {
        bool processed = pipeline.process_audio_packet(i, i * 1000, encoded_data);
        assert(processed);
    }
    
    // Give pipeline time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Check statistics
    assert(pipeline.get_packets_processed() == 3);
    assert(pipeline.get_frames_decoded() > 0);
    assert(pipeline.get_frames_output() > 0);
    
    pipeline.stop();
    
    std::cout << "✓ Audio pipeline multiple packets test passed" << std::endl;
}

void test_audio_pipeline_out_of_order_packets() {
    std::cout << "Testing Audio pipeline out-of-order packet handling..." << std::endl;
    
    AudioPipeline pipeline(48000, 2, 128, 5);
    assert(pipeline.is_initialized());
    
    // Start pipeline
    bool started = pipeline.start();
    assert(started);
    
    std::vector<uint8_t> encoded_data(64, 0xCC);
    
    // Send packets out of order
    pipeline.process_audio_packet(3, 3000, encoded_data);
    pipeline.process_audio_packet(1, 1000, encoded_data);
    pipeline.process_audio_packet(2, 2000, encoded_data);
    
    // Give pipeline time to process and reorder
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should have processed all packets despite out-of-order arrival
    assert(pipeline.get_packets_processed() == 3);
    
    pipeline.stop();
    
    std::cout << "✓ Audio pipeline out-of-order packets test passed" << std::endl;
}

void test_audio_pipeline_latency_measurement() {
    std::cout << "Testing Audio pipeline latency measurement..." << std::endl;
    
    AudioPipeline pipeline(48000, 2, 128, 5);
    assert(pipeline.is_initialized());
    
    // Start pipeline
    bool started = pipeline.start();
    assert(started);
    
    // Process some packets to get latency measurements
    std::vector<uint8_t> encoded_data(64, 0xDD);
    
    for (uint32_t i = 1; i <= 5; ++i) {
        pipeline.process_audio_packet(i, i * 1000, encoded_data);
    }
    
    // Give pipeline time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check latency measurements
    double total_latency = pipeline.get_total_latency_ms();
    double decode_latency = pipeline.get_decode_latency_ms();
    double output_latency = pipeline.get_output_latency_ms();
    double jitter_buffer_latency = pipeline.get_jitter_buffer_latency_ms();
    
    assert(total_latency > 0.0);
    assert(decode_latency >= 0.0);
    assert(output_latency > 0.0);
    assert(jitter_buffer_latency >= 0.0);
    
    // Total latency should be sum of components
    double expected_total = decode_latency + output_latency + jitter_buffer_latency;
    assert(std::abs(total_latency - expected_total) < 1.0); // Within 1ms tolerance
    
    pipeline.stop();
    
    std::cout << "✓ Audio pipeline latency measurement test passed" << std::endl;
}

void test_audio_pipeline_error_handling() {
    std::cout << "Testing Audio pipeline error handling..." << std::endl;
    
    AudioPipeline pipeline(48000, 2, 128, 5);
    assert(pipeline.is_initialized());
    
    // Start pipeline
    bool started = pipeline.start();
    assert(started);
    
    // Send invalid packet (empty data)
    std::vector<uint8_t> empty_data;
    bool processed = pipeline.process_audio_packet(1, 1000, empty_data);
    assert(!processed); // Should reject invalid packet
    
    // Send packet with invalid size - this will be accepted by process_audio_packet
    // but should fail during decoding, which increments decode errors
    std::vector<uint8_t> invalid_data(1, 0xFF);
    processed = pipeline.process_audio_packet(2, 2000, invalid_data);
    assert(processed); // Packet is accepted but will fail during decode
    
    // Give time for processing to occur and errors to be counted
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Check error statistics
    assert(pipeline.get_decode_errors() > 0);
    
    pipeline.stop();
    
    std::cout << "✓ Audio pipeline error handling test passed" << std::endl;
}

void test_audio_pipeline_statistics() {
    std::cout << "Testing Audio pipeline statistics..." << std::endl;
    
    AudioPipeline pipeline(48000, 2, 128, 5);
    assert(pipeline.is_initialized());
    
    // Initial statistics should be zero
    assert(pipeline.get_packets_processed() == 0);
    assert(pipeline.get_frames_decoded() == 0);
    assert(pipeline.get_frames_output() == 0);
    assert(pipeline.get_decode_errors() == 0);
    assert(pipeline.get_output_underruns() == 0);
    assert(pipeline.get_jitter_buffer_drops() == 0);
    
    // Start and process some packets
    pipeline.start();
    
    std::vector<uint8_t> encoded_data(64, 0xEE);
    pipeline.process_audio_packet(1, 1000, encoded_data);
    pipeline.process_audio_packet(2, 2000, encoded_data);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Statistics should be updated
    assert(pipeline.get_packets_processed() == 2);
    
    pipeline.stop();
    
    std::cout << "✓ Audio pipeline statistics test passed" << std::endl;
}

void test_audio_pipeline_performance() {
    std::cout << "Testing Audio pipeline performance..." << std::endl;
    
    AudioPipeline pipeline(48000, 2, 128, 5);
    assert(pipeline.is_initialized());
    
    // Start pipeline
    pipeline.start();
    
    // Measure processing time for multiple packets
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<uint8_t> encoded_data(64, 0xFF);
    const int num_packets = 10;
    
    for (int i = 1; i <= num_packets; ++i) {
        pipeline.process_audio_packet(i, i * 1000, encoded_data);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Processing should be fast (under 1ms per packet on average)
    double avg_processing_time_us = static_cast<double>(duration.count()) / num_packets;
    assert(avg_processing_time_us < 1000.0); // Less than 1ms per packet
    
    std::cout << "Average packet processing time: " << avg_processing_time_us << " μs" << std::endl;
    
    pipeline.stop();
    
    std::cout << "✓ Audio pipeline performance test passed" << std::endl;
}

void run_audio_pipeline_tests() {
    std::cout << "=== Running Audio Pipeline Tests ===" << std::endl;
    
    test_audio_pipeline_construction();
    test_audio_pipeline_invalid_params();
    test_audio_pipeline_start_stop();
    test_audio_pipeline_process_packet();
    test_audio_pipeline_multiple_packets();
    test_audio_pipeline_out_of_order_packets();
    test_audio_pipeline_latency_measurement();
    test_audio_pipeline_error_handling();
    test_audio_pipeline_statistics();
    test_audio_pipeline_performance();
    
    std::cout << "=== All Audio Pipeline Tests Passed ===" << std::endl;
}