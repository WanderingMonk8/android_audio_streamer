#include "network/fec_encoder.h"
#include <iostream>

using namespace AudioReceiver::Network;

int main() {
    std::cout << "Debug FEC Header..." << std::endl;
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
    
    try {
        auto serialized = header.serialize();
        std::cout << "Serialized size: " << serialized.size() << std::endl;
        std::cout << "Expected size: " << FECHeader::HEADER_SIZE << std::endl;
        
        if (serialized.size() == FECHeader::HEADER_SIZE) {
            std::cout << "Size matches!" << std::endl;
            
            // Try to deserialize
            auto deserialized = FECHeader::deserialize(serialized);
            std::cout << "Deserialized successfully!" << std::endl;
            std::cout << "packet_type: " << static_cast<int>(deserialized.packet_type) << std::endl;
            std::cout << "sequence_id: " << deserialized.sequence_id << std::endl;
        } else {
            std::cout << "Size mismatch!" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}