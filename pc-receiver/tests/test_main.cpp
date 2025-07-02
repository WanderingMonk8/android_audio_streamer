#include <iostream>

// Test function declarations
void run_packet_tests();
void run_udp_receiver_tests();

int main() {
    std::cout << "Starting Audio Receiver Network Tests..." << std::endl;
    
    try {
        run_packet_tests();
        std::cout << std::endl;
        run_udp_receiver_tests();
        
        std::cout << std::endl;
        std::cout << "🎉 All tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}