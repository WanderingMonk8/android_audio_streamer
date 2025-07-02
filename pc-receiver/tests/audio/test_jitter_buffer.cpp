#include "audio/jitter_buffer.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace AudioReceiver::Audio;

void test_jitter_buffer_construction() {
    std::cout << "Testing Jitter buffer construction..." << std::endl;
    
    // Test valid construction
    JitterBuffer buffer(5, 120, 2); // 5 packets, 120 samples per frame, 2 channels
    assert(buffer.get_capacity() == 5);
    assert(buffer.get_frame_size() == 120);
    assert(buffer.get_channels() == 2);
    assert(buffer.is_empty());
    assert(!buffer.is_full());
    assert(buffer.get_size() == 0);
    
    std::cout << "✓ Jitter buffer construction test passed" << std::endl;
}

void test_jitter_buffer_invalid_params() {
    std::cout << "Testing Jitter buffer with invalid parameters..." << std::endl;
    
    // Test invalid capacity
    JitterBuffer buffer1(0, 120, 2);
    assert(!buffer1.is_initialized());
    
    JitterBuffer buffer2(100, 120, 2); // Too large
    assert(!buffer2.is_initialized());
    
    // Test invalid frame size
    JitterBuffer buffer3(5, 0, 2);
    assert(!buffer3.is_initialized());
    
    // Test invalid channels
    JitterBuffer buffer4(5, 120, 0);
    assert(!buffer4.is_initialized());
    
    std::cout << "✓ Invalid parameters test passed" << std::endl;
}

void test_jitter_buffer_add_packet() {
    std::cout << "Testing Jitter buffer add packet..." << std::endl;
    
    JitterBuffer buffer(3, 120, 2);
    assert(buffer.is_initialized());
    
    // Create test audio packets
    std::vector<float> audio1(240, 1.0f); // 120 samples * 2 channels
    std::vector<float> audio2(240, 2.0f);
    std::vector<float> audio3(240, 3.0f);
    
    // Add packets
    assert(buffer.add_packet(1, 1000, audio1));
    assert(buffer.get_size() == 1);
    assert(!buffer.is_empty());
    
    assert(buffer.add_packet(2, 2000, audio2));
    assert(buffer.get_size() == 2);
    
    assert(buffer.add_packet(3, 3000, audio3));
    assert(buffer.get_size() == 3);
    assert(buffer.is_full());
    
    std::cout << "✓ Jitter buffer add packet test passed" << std::endl;
}

void test_jitter_buffer_get_packet() {
    std::cout << "Testing Jitter buffer get packet..." << std::endl;
    
    JitterBuffer buffer(3, 120, 2);
    assert(buffer.is_initialized());
    
    // Try to get from empty buffer
    auto empty_packet = buffer.get_next_packet();
    assert(empty_packet == nullptr);
    
    // Add packets in order
    std::vector<float> audio1(240, 1.0f);
    std::vector<float> audio2(240, 2.0f);
    
    buffer.add_packet(1, 1000, audio1);
    buffer.add_packet(2, 2000, audio2);
    
    // Get packets in order
    auto packet1 = buffer.get_next_packet();
    assert(packet1 != nullptr);
    assert(packet1->sequence_id == 1);
    assert(packet1->timestamp == 1000);
    assert(packet1->audio_data.size() == 240);
    assert(packet1->audio_data[0] == 1.0f);
    
    auto packet2 = buffer.get_next_packet();
    assert(packet2 != nullptr);
    assert(packet2->sequence_id == 2);
    assert(packet2->timestamp == 2000);
    assert(packet2->audio_data[0] == 2.0f);
    
    // Buffer should be empty now
    assert(buffer.is_empty());
    auto empty_packet2 = buffer.get_next_packet();
    assert(empty_packet2 == nullptr);
    
    std::cout << "✓ Jitter buffer get packet test passed" << std::endl;
}

void test_jitter_buffer_out_of_order() {
    std::cout << "Testing Jitter buffer out-of-order packets..." << std::endl;
    
    JitterBuffer buffer(5, 120, 2);
    assert(buffer.is_initialized());
    
    std::vector<float> audio1(240, 1.0f);
    std::vector<float> audio2(240, 2.0f);
    std::vector<float> audio3(240, 3.0f);
    
    // Add packets out of order
    buffer.add_packet(3, 3000, audio3);
    buffer.add_packet(1, 1000, audio1);
    buffer.add_packet(2, 2000, audio2);
    
    // Should get packets in sequence order
    auto packet1 = buffer.get_next_packet();
    assert(packet1->sequence_id == 1);
    
    auto packet2 = buffer.get_next_packet();
    assert(packet2->sequence_id == 2);
    
    auto packet3 = buffer.get_next_packet();
    assert(packet3->sequence_id == 3);
    
    std::cout << "✓ Jitter buffer out-of-order test passed" << std::endl;
}

void test_jitter_buffer_overflow() {
    std::cout << "Testing Jitter buffer overflow handling..." << std::endl;
    
    JitterBuffer buffer(2, 120, 2); // Small buffer
    assert(buffer.is_initialized());
    
    std::vector<float> audio(240, 1.0f);
    
    // Fill buffer
    assert(buffer.add_packet(1, 1000, audio));
    assert(buffer.add_packet(2, 2000, audio));
    assert(buffer.is_full());
    
    // Try to add one more - should drop oldest
    assert(buffer.add_packet(3, 3000, audio));
    assert(buffer.get_size() == 2);
    assert(buffer.get_packets_dropped() == 1);
    
    // Should get packets 2 and 3 (packet 1 was dropped)
    auto packet = buffer.get_next_packet();
    assert(packet->sequence_id == 2);
    
    packet = buffer.get_next_packet();
    assert(packet->sequence_id == 3);
    
    std::cout << "✓ Jitter buffer overflow test passed" << std::endl;
}

void test_jitter_buffer_duplicate_packets() {
    std::cout << "Testing Jitter buffer duplicate packet handling..." << std::endl;
    
    JitterBuffer buffer(3, 120, 2);
    assert(buffer.is_initialized());
    
    std::vector<float> audio(240, 1.0f);
    
    // Add packet
    assert(buffer.add_packet(1, 1000, audio));
    assert(buffer.get_size() == 1);
    
    // Try to add duplicate - should be rejected
    assert(!buffer.add_packet(1, 1000, audio));
    assert(buffer.get_size() == 1);
    assert(buffer.get_duplicates_dropped() == 1);
    
    std::cout << "✓ Jitter buffer duplicate packet test passed" << std::endl;
}

void test_jitter_buffer_statistics() {
    std::cout << "Testing Jitter buffer statistics..." << std::endl;
    
    JitterBuffer buffer(3, 120, 2);
    assert(buffer.is_initialized());
    
    // Initial stats
    assert(buffer.get_packets_added() == 0);
    assert(buffer.get_packets_retrieved() == 0);
    assert(buffer.get_packets_dropped() == 0);
    assert(buffer.get_duplicates_dropped() == 0);
    
    std::vector<float> audio(240, 1.0f);
    
    // Add packets
    buffer.add_packet(1, 1000, audio);
    buffer.add_packet(2, 2000, audio);
    assert(buffer.get_packets_added() == 2);
    
    // Get packets
    buffer.get_next_packet();
    buffer.get_next_packet();
    assert(buffer.get_packets_retrieved() == 2);
    
    std::cout << "✓ Jitter buffer statistics test passed" << std::endl;
}

void test_jitter_buffer_clear() {
    std::cout << "Testing Jitter buffer clear..." << std::endl;
    
    JitterBuffer buffer(3, 120, 2);
    assert(buffer.is_initialized());
    
    std::vector<float> audio(240, 1.0f);
    
    // Add packets
    buffer.add_packet(1, 1000, audio);
    buffer.add_packet(2, 2000, audio);
    assert(buffer.get_size() == 2);
    
    // Clear buffer
    buffer.clear();
    assert(buffer.is_empty());
    assert(buffer.get_size() == 0);
    
    // Statistics should be reset
    assert(buffer.get_packets_added() == 0);
    assert(buffer.get_packets_retrieved() == 0);
    
    std::cout << "✓ Jitter buffer clear test passed" << std::endl;
}

void run_jitter_buffer_tests() {
    std::cout << "=== Running Jitter Buffer Tests ===" << std::endl;
    
    test_jitter_buffer_construction();
    test_jitter_buffer_invalid_params();
    test_jitter_buffer_add_packet();
    test_jitter_buffer_get_packet();
    test_jitter_buffer_out_of_order();
    test_jitter_buffer_overflow();
    test_jitter_buffer_duplicate_packets();
    test_jitter_buffer_statistics();
    test_jitter_buffer_clear();
    
    std::cout << "=== All Jitter Buffer Tests Passed ===" << std::endl;
}