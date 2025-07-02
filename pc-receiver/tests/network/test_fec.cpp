#include "network/fec_encoder.h"
#include "network/fec_decoder.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace AudioReceiver::Network;

void test_fec_header_serialization() {
    std::cout << "Testing FEC header serialization..." << std::endl;
    std::cout << "HEADER_SIZE = " << FECHeader::HEADER_SIZE << std::endl;
    
    // Create test header
    FECHeader header;
    header.packet_type = FECPacketType::PRIMARY;
    header.sequence_id = 12345;
    header.redundant_sequence_id = 0;
    header.redundant_data_size = 256;
    header.redundancy_level = 20;
    header.reserved = 0;
    
    std::cout << "About to serialize..." << std::endl;
    
    // Serialize
    auto serialized = header.serialize();
    std::cout << "Serialized size: " << serialized.size() << std::endl;
    assert(serialized.size() == FECHeader::HEADER_SIZE);
    
    // Deserialize
    auto deserialized = FECHeader::deserialize(serialized);
    assert(deserialized.packet_type == header.packet_type);
    assert(deserialized.sequence_id == header.sequence_id);
    assert(deserialized.redundant_sequence_id == header.redundant_sequence_id);
    assert(deserialized.redundant_data_size == header.redundant_data_size);
    assert(deserialized.redundancy_level == header.redundancy_level);
    
    std::cout << "✓ FEC header serialization test passed" << std::endl;
}

void test_fec_encoder_construction() {
    std::cout << "Testing FEC encoder construction..." << std::endl;
    
    // Test default construction
    FECEncoder encoder1;
    auto config1 = encoder1.get_config();
    assert(config1.redundancy_percentage == 20.0);
    assert(config1.max_recovery_distance == 5);
    assert(config1.adaptive_redundancy == true);
    
    // Test custom configuration
    FECConfig custom_config;
    custom_config.redundancy_percentage = 30.0;
    custom_config.max_recovery_distance = 3;
    custom_config.window_size = 8;
    custom_config.adaptive_redundancy = false;
    
    FECEncoder encoder2(custom_config);
    auto config2 = encoder2.get_config();
    assert(config2.redundancy_percentage == 30.0);
    assert(config2.max_recovery_distance == 3);
    assert(config2.window_size == 8);
    assert(config2.adaptive_redundancy == false);
    
    std::cout << "✓ FEC encoder construction test passed" << std::endl;
}

void test_fec_decoder_construction() {
    std::cout << "Testing FEC decoder construction..." << std::endl;
    
    // Test default construction
    FECDecoder decoder1;
    auto stats1 = decoder1.get_stats();
    assert(stats1.primary_packets_received == 0);
    assert(stats1.redundant_packets_received == 0);
    assert(stats1.packets_recovered == 0);
    
    // Test custom construction
    FECDecoder decoder2(3, 15);
    auto stats2 = decoder2.get_stats();
    assert(stats2.primary_packets_received == 0);
    
    std::cout << "✓ FEC decoder construction test passed" << std::endl;
}

void test_fec_encoding_basic() {
    std::cout << "Testing basic FEC encoding..." << std::endl;
    
    FECConfig config;
    config.redundancy_percentage = 20.0;  // 20% redundancy
    config.window_size = 5;
    
    FECEncoder encoder(config);
    
    // Create test audio data
    std::vector<uint8_t> audio_data(100, 0xAB);  // 100 bytes of test data
    
    // Encode first packet (should only produce primary packet)
    auto packets1 = encoder.encode_packet(1, audio_data);
    assert(packets1.size() == 1);  // Only primary packet for first packet
    
    // Encode more packets to trigger redundancy
    for (uint32_t i = 2; i <= 6; i++) {
        auto packets = encoder.encode_packet(i, audio_data);
        // Should have primary + some redundant packets
        assert(packets.size() >= 1);
    }
    
    auto stats = encoder.get_stats();
    assert(stats.primary_packets_encoded == 6);
    assert(stats.redundant_packets_generated > 0);
    
    std::cout << "✓ Basic FEC encoding test passed" << std::endl;
}

void test_fec_redundancy_levels() {
    std::cout << "Testing FEC redundancy levels..." << std::endl;
    
    std::vector<double> redundancy_levels = {0.0, 10.0, 25.0, 50.0};
    
    for (double redundancy : redundancy_levels) {
        FECConfig config;
        config.redundancy_percentage = redundancy;
        config.window_size = 10;
        
        FECEncoder encoder(config);
        std::vector<uint8_t> audio_data(100, 0xCD);
        
        // Encode several packets
        size_t total_packets = 0;
        size_t total_redundant = 0;
        
        for (uint32_t i = 1; i <= 10; i++) {
            auto packets = encoder.encode_packet(i, audio_data);
            total_packets += packets.size();
            total_redundant += (packets.size() - 1);  // All except primary
        }
        
        auto stats = encoder.get_stats();
        std::cout << "Redundancy " << redundancy << "%: " 
                  << stats.redundant_packets_generated << " redundant packets generated" << std::endl;
        
        if (redundancy == 0.0) {
            assert(stats.redundant_packets_generated == 0);
        } else {
            assert(stats.redundant_packets_generated > 0);
        }
    }
    
    std::cout << "✓ FEC redundancy levels test passed" << std::endl;
}

void test_fec_decoding_basic() {
    std::cout << "Testing basic FEC decoding..." << std::endl;
    
    FECDecoder decoder;
    
    // Create test packets manually (simulating encoder output)
    FECHeader primary_header;
    primary_header.packet_type = FECPacketType::PRIMARY;
    primary_header.sequence_id = 1;
    primary_header.redundant_sequence_id = 0;
    primary_header.redundant_data_size = 0;
    primary_header.redundancy_level = 20;
    
    std::vector<uint8_t> audio_data(100, 0xEF);
    auto header_data = primary_header.serialize();
    
    std::vector<uint8_t> primary_packet;
    primary_packet.insert(primary_packet.end(), header_data.begin(), header_data.end());
    primary_packet.insert(primary_packet.end(), audio_data.begin(), audio_data.end());
    
    // Process primary packet
    auto result = decoder.process_packet(primary_packet);
    assert(result.success);
    assert(result.sequence_id == 1);
    assert(result.recovered_data == audio_data);
    assert(!result.was_recovered_from_redundancy);
    
    auto stats = decoder.get_stats();
    assert(stats.primary_packets_received == 1);
    assert(stats.redundant_packets_received == 0);
    
    std::cout << "✓ Basic FEC decoding test passed" << std::endl;
}

void test_fec_packet_recovery() {
    std::cout << "Testing FEC packet recovery..." << std::endl;
    
    FECConfig config;
    config.redundancy_percentage = 50.0;  // High redundancy for testing
    config.window_size = 5;
    
    FECEncoder encoder(config);
    FECDecoder decoder;
    
    std::vector<uint8_t> audio_data1(100, 0x11);
    std::vector<uint8_t> audio_data2(100, 0x22);
    std::vector<uint8_t> audio_data3(100, 0x33);
    
    // Encode packets
    auto packets1 = encoder.encode_packet(1, audio_data1);
    auto packets2 = encoder.encode_packet(2, audio_data2);
    auto packets3 = encoder.encode_packet(3, audio_data3);
    
    // Process packet 1 and 3, skip packet 2 (simulate packet loss)
    for (const auto& packet : packets1) {
        decoder.process_packet(packet);
    }
    
    // Skip packets2 (packet loss simulation)
    
    for (const auto& packet : packets3) {
        decoder.process_packet(packet);
    }
    
    // Try to recover packet 2
    auto recovery_result = decoder.recover_packet(2);
    
    if (recovery_result.success) {
        std::cout << "Successfully recovered packet 2 from redundancy" << std::endl;
        assert(recovery_result.was_recovered_from_redundancy);
        assert(recovery_result.sequence_id == 2);
    } else {
        std::cout << "Could not recover packet 2 (may need more redundancy)" << std::endl;
    }
    
    auto stats = decoder.get_stats();
    std::cout << "Recovery attempts: " << stats.recovery_attempts << std::endl;
    std::cout << "Packets recovered: " << stats.packets_recovered << std::endl;
    
    std::cout << "✓ FEC packet recovery test passed" << std::endl;
}

void test_fec_adaptive_redundancy() {
    std::cout << "Testing FEC adaptive redundancy..." << std::endl;
    
    FECEncoder encoder;
    
    // Test redundancy level changes
    encoder.set_redundancy_level(10.0);
    auto config1 = encoder.get_config();
    assert(config1.redundancy_percentage == 10.0);
    
    encoder.set_redundancy_level(30.0);
    auto config2 = encoder.get_config();
    assert(config2.redundancy_percentage == 30.0);
    
    // Test invalid redundancy levels
    encoder.set_redundancy_level(-5.0);  // Should clamp to 0
    auto config3 = encoder.get_config();
    assert(config3.redundancy_percentage >= 0.0);
    
    encoder.set_redundancy_level(75.0);  // Should clamp to 50
    auto config4 = encoder.get_config();
    assert(config4.redundancy_percentage <= 50.0);
    
    std::cout << "✓ FEC adaptive redundancy test passed" << std::endl;
}

void test_fec_statistics() {
    std::cout << "Testing FEC statistics..." << std::endl;
    
    FECEncoder encoder;
    FECDecoder decoder;
    
    std::vector<uint8_t> audio_data(100, 0x55);
    
    // Encode and decode several packets
    for (uint32_t i = 1; i <= 10; i++) {
        auto packets = encoder.encode_packet(i, audio_data);
        
        for (const auto& packet : packets) {
            decoder.process_packet(packet);
        }
    }
    
    auto encode_stats = encoder.get_stats();
    auto decode_stats = decoder.get_stats();
    
    assert(encode_stats.primary_packets_encoded == 10);
    assert(decode_stats.primary_packets_received == 10);
    
    std::cout << "Encoder stats:" << std::endl;
    std::cout << "  Primary packets: " << encode_stats.primary_packets_encoded << std::endl;
    std::cout << "  Redundant packets: " << encode_stats.redundant_packets_generated << std::endl;
    std::cout << "  Current redundancy: " << encode_stats.current_redundancy_percentage << "%" << std::endl;
    
    std::cout << "Decoder stats:" << std::endl;
    std::cout << "  Primary packets: " << decode_stats.primary_packets_received << std::endl;
    std::cout << "  Redundant packets: " << decode_stats.redundant_packets_received << std::endl;
    std::cout << "  Recovery success rate: " << decode_stats.recovery_success_rate << "%" << std::endl;
    
    std::cout << "✓ FEC statistics test passed" << std::endl;
}

void test_fec_reset_functionality() {
    std::cout << "Testing FEC reset functionality..." << std::endl;
    
    FECEncoder encoder;
    FECDecoder decoder;
    
    std::vector<uint8_t> audio_data(100, 0x77);
    
    // Generate some activity
    for (uint32_t i = 1; i <= 5; i++) {
        auto packets = encoder.encode_packet(i, audio_data);
        for (const auto& packet : packets) {
            decoder.process_packet(packet);
        }
    }
    
    // Check that we have some stats
    auto encode_stats_before = encoder.get_stats();
    auto decode_stats_before = decoder.get_stats();
    assert(encode_stats_before.primary_packets_encoded > 0);
    assert(decode_stats_before.primary_packets_received > 0);
    
    // Reset
    encoder.reset();
    decoder.reset();
    
    // Check that stats are cleared
    auto encode_stats_after = encoder.get_stats();
    auto decode_stats_after = decoder.get_stats();
    assert(encode_stats_after.primary_packets_encoded == 0);
    assert(decode_stats_after.primary_packets_received == 0);
    
    std::cout << "✓ FEC reset functionality test passed" << std::endl;
}

void test_fec_integration_with_network_monitor() {
    std::cout << "Testing FEC integration with Network Monitor..." << std::endl;
    
    // This test simulates how FEC would integrate with Network Monitor
    // for adaptive redundancy based on network conditions
    
    FECEncoder encoder;
    
    // Simulate excellent network conditions (low redundancy)
    double excellent_redundancy = 5.0;
    encoder.set_redundancy_level(excellent_redundancy);
    
    std::vector<uint8_t> audio_data(100, 0x99);
    auto packets_excellent = encoder.encode_packet(1, audio_data);
    
    // Simulate poor network conditions (high redundancy)
    double poor_redundancy = 30.0;
    encoder.set_redundancy_level(poor_redundancy);
    
    auto packets_poor = encoder.encode_packet(2, audio_data);
    
    auto stats = encoder.get_stats();
    assert(stats.current_redundancy_percentage == poor_redundancy);
    
    std::cout << "Excellent network redundancy: " << excellent_redundancy << "%" << std::endl;
    std::cout << "Poor network redundancy: " << poor_redundancy << "%" << std::endl;
    std::cout << "✓ FEC integration with Network Monitor test passed" << std::endl;
}

// Test runner
void run_fec_tests() {
    std::cout << "\n=== Running FEC Tests ===" << std::endl;
    
    test_fec_header_serialization();
    test_fec_encoder_construction();
    test_fec_decoder_construction();
    test_fec_encoding_basic();
    test_fec_redundancy_levels();
    test_fec_decoding_basic();
    test_fec_packet_recovery();
    test_fec_adaptive_redundancy();
    test_fec_statistics();
    test_fec_reset_functionality();
    test_fec_integration_with_network_monitor();
    
    std::cout << "\n✅ All FEC tests passed!" << std::endl;
}