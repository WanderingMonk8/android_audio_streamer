#include "network/fec_encoder.h"
#include "network/fec_decoder.h"
#include <iostream>

// Forward declaration
void run_fec_tests();

int main() {
    std::cout << "Running FEC-only tests..." << std::endl;
    
    try {
        run_fec_tests();
        std::cout << "FEC tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FEC test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FEC test failed with unknown exception" << std::endl;
        return 1;
    }
}