#include "network/packet.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace AudioReceiver::Network;

void test_packet_construction() {
    std::cout << "Testing packet construction..." << std::endl;
    
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    AudioPacket packet(123, 456789, test_data);
    
    assert(packet.sequence_id == 123);
    assert(packet.timestamp == 456789);
    assert(packet.payload_size == 4);
    assert(packet.payload == test_data);
    assert(packet.is_valid());
    
    std::cout << "✓ Packet construction test passed" << std::endl;
}

void test_packet_serialization() {
    std::cout << "Testing packet serialization..." << std::endl;
    
    std::vector<uint8_t> test_data = {0xAA, 0xBB, 0xCC, 0xDD};
    AudioPacket packet(0x12345678, 0x123456789ABCDEF0, test_data);
    
    auto serialized = packet.serialize();
    
    // Check total size: 4 + 8 + 4 + 4 = 20 bytes
    assert(serialized.size() == 20);
    assert(packet.total_size() == 20);
    
    // Check sequence ID (little endian)
    assert(serialized[0] == 0x78);
    assert(serialized[1] == 0x56);
    assert(serialized[2] == 0x34);
    assert(serialized[3] == 0x12);
    
    // Check payload size
    assert(serialized[12] == 0x04);
    assert(serialized[13] == 0x00);
    assert(serialized[14] == 0x00);
    assert(serialized[15] == 0x00);
    
    // Check payload
    assert(serialized[16] == 0xAA);
    assert(serialized[17] == 0xBB);
    assert(serialized[18] == 0xCC);
    assert(serialized[19] == 0xDD);
    
    std::cout << "✓ Packet serialization test passed" << std::endl;
}

void test_packet_deserialization() {
    std::cout << "Testing packet deserialization..." << std::endl;
    
    // Create test packet
    std::vector<uint8_t> test_data = {0x11, 0x22, 0x33};
    AudioPacket original(0x87654321, 0xFEDCBA9876543210, test_data);
    
    // Serialize and deserialize
    auto serialized = original.serialize();
    auto deserialized = AudioPacket::deserialize(serialized.data(), serialized.size());
    
    assert(deserialized != nullptr);
    assert(deserialized->sequence_id == 0x87654321);
    assert(deserialized->timestamp == 0xFEDCBA9876543210);
    assert(deserialized->payload_size == 3);
    assert(deserialized->payload == test_data);
    assert(deserialized->is_valid());
    
    std::cout << "✓ Packet deserialization test passed" << std::endl;
}

void test_packet_invalid_data() {
    std::cout << "Testing packet with invalid data..." << std::endl;
    
    // Test with insufficient data
    uint8_t invalid_data[] = {0x01, 0x02, 0x03};
    auto packet = AudioPacket::deserialize(invalid_data, sizeof(invalid_data));
    assert(packet == nullptr);
    
    // Test with mismatched payload size
    uint8_t mismatched_data[] = {
        0x01, 0x00, 0x00, 0x00,  // sequence_id = 1
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timestamp = 2
        0x10, 0x00, 0x00, 0x00,  // payload_size = 16 (but only 2 bytes follow)
        0xAA, 0xBB
    };
    packet = AudioPacket::deserialize(mismatched_data, sizeof(mismatched_data));
    assert(packet == nullptr);
    
    std::cout << "✓ Invalid packet data test passed" << std::endl;
}

void test_empty_packet() {
    std::cout << "Testing empty packet..." << std::endl;
    
    std::vector<uint8_t> empty_data;
    AudioPacket packet(100, 200, empty_data);
    
    assert(packet.sequence_id == 100);
    assert(packet.timestamp == 200);
    assert(packet.payload_size == 0);
    assert(packet.payload.empty());
    assert(packet.is_valid());
    assert(packet.total_size() == 16); // 4 + 8 + 4 + 0
    
    // Test serialization/deserialization of empty packet
    auto serialized = packet.serialize();
    assert(serialized.size() == 16);
    
    auto deserialized = AudioPacket::deserialize(serialized.data(), serialized.size());
    assert(deserialized != nullptr);
    assert(deserialized->payload.empty());
    assert(deserialized->is_valid());
    
    std::cout << "✓ Empty packet test passed" << std::endl;
}

void run_packet_tests() {
    std::cout << "=== Running Packet Tests ===" << std::endl;
    
    test_packet_construction();
    test_packet_serialization();
    test_packet_deserialization();
    test_packet_invalid_data();
    test_empty_packet();
    
    std::cout << "=== All Packet Tests Passed ===" << std::endl;
}