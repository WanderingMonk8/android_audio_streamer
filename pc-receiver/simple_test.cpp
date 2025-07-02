#include "src/network/packet.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace AudioReceiver::Network;

int main() {
    std::cout << "Running simple packet test..." << std::endl;
    
    // Test packet creation
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    AudioPacket packet(123, 456789, test_data);
    
    std::cout << "Created packet - Seq: " << packet.sequence_id 
              << ", Timestamp: " << packet.timestamp 
              << ", Payload size: " << packet.payload_size << std::endl;
    
    // Test serialization
    auto serialized = packet.serialize();
    std::cout << "Serialized packet size: " << serialized.size() << " bytes" << std::endl;
    
    // Test deserialization
    auto deserialized = AudioPacket::deserialize(serialized.data(), serialized.size());
    
    if (deserialized) {
        std::cout << "Deserialized packet - Seq: " << deserialized->sequence_id 
                  << ", Timestamp: " << deserialized->timestamp 
                  << ", Payload size: " << deserialized->payload_size << std::endl;
        
        // Verify data integrity
        assert(deserialized->sequence_id == 123);
        assert(deserialized->timestamp == 456789);
        assert(deserialized->payload_size == 4);
        assert(deserialized->payload == test_data);
        
        std::cout << "✓ All packet tests passed!" << std::endl;
    } else {
        std::cout << "❌ Deserialization failed!" << std::endl;
        return 1;
    }
    
    return 0;
}