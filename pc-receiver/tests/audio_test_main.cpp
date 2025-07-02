#include <iostream>

// Test function declarations
void run_opus_decoder_tests();
void run_real_opus_decoder_tests();
void run_audio_output_tests();
void run_jitter_buffer_tests();
void run_audio_pipeline_tests();

int main() {
    std::cout << "Starting Audio Receiver Audio Tests..." << std::endl;
    
    try {
        run_opus_decoder_tests();
        std::cout << std::endl;
        run_real_opus_decoder_tests();
        std::cout << std::endl;
        run_audio_output_tests();
        std::cout << std::endl;
        run_jitter_buffer_tests();
        std::cout << std::endl;
        run_audio_pipeline_tests();
        
        std::cout << std::endl;
        std::cout << "All audio tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}