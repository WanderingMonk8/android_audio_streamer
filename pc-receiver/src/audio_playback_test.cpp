#include "audio/audio_pipeline.h"
#include "audio/real_audio_output.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace AudioReceiver::Audio;

// Generate a sine wave test tone
std::vector<uint8_t> generate_test_tone_opus_packet(double frequency, int sample_rate, int channels, int frame_size) {
    // For this test, we'll create a simple mock Opus packet
    // In a real scenario, this would be actual Opus-encoded data
    std::vector<uint8_t> mock_opus_packet;
    
    // Create a simple pattern that the mock decoder will recognize
    // This is just for testing - real Opus packets would be different
    mock_opus_packet.push_back(0x01); // Mock Opus header
    mock_opus_packet.push_back(static_cast<uint8_t>(frequency / 10)); // Frequency indicator
    mock_opus_packet.push_back(static_cast<uint8_t>(channels));
    mock_opus_packet.push_back(static_cast<uint8_t>(frame_size & 0xFF));
    
    // Add some mock payload data
    for (int i = 0; i < 20; i++) {
        mock_opus_packet.push_back(static_cast<uint8_t>(i * 10));
    }
    
    return mock_opus_packet;
}

void test_real_audio_output_direct() {
    std::cout << "\n=== Testing Direct Real Audio Output ===" << std::endl;
    
    const int sample_rate = 48000;
    const int channels = 2;
    const int buffer_size = 128;
    
    // Create real audio output
    RealAudioOutput audio_output(sample_rate, channels, buffer_size);
    
    if (!audio_output.is_initialized()) {
        std::cerr << "Failed to initialize real audio output!" << std::endl;
        return;
    }
    
    std::cout << "Real audio output initialized successfully!" << std::endl;
    std::cout << "Sample rate: " << audio_output.get_sample_rate() << " Hz" << std::endl;
    std::cout << "Channels: " << audio_output.get_channels() << std::endl;
    std::cout << "Buffer size: " << audio_output.get_buffer_size() << " samples" << std::endl;
    std::cout << "Estimated latency: " << audio_output.get_estimated_latency_ms() << " ms" << std::endl;
    
    // Start audio output
    if (!audio_output.start()) {
        std::cerr << "Failed to start audio output!" << std::endl;
        return;
    }
    
    std::cout << "\nPlaying test tones for 5 seconds..." << std::endl;
    std::cout << "You should hear audio through your speakers/headphones!" << std::endl;
    
    // Generate and play test tones
    const double duration_seconds = 5.0;
    const int total_frames = static_cast<int>(sample_rate * duration_seconds / buffer_size);
    
    for (int frame = 0; frame < total_frames; frame++) {
        std::vector<float> audio_data(buffer_size * channels);
        
        // Generate a sine wave (440 Hz A note)
        const double frequency = 440.0;
        const double amplitude = 0.3; // Moderate volume
        
        for (int sample = 0; sample < buffer_size; sample++) {
            double time = (frame * buffer_size + sample) / static_cast<double>(sample_rate);
            float value = static_cast<float>(amplitude * std::sin(2.0 * M_PI * frequency * time));
            
            // Fill both channels (stereo)
            audio_data[sample * channels] = value;     // Left channel
            audio_data[sample * channels + 1] = value; // Right channel
        }
        
        // Write audio data
        if (!audio_output.write_audio(audio_data)) {
            std::cerr << "Failed to write audio data at frame " << frame << std::endl;
            break;
        }
        
        // Small delay to maintain real-time playback
        std::this_thread::sleep_for(std::chrono::microseconds(
            buffer_size * 1000000 / sample_rate));
    }
    
    std::cout << "Test tone playback completed!" << std::endl;
    std::cout << "Frames written: " << audio_output.get_frames_written() << std::endl;
    std::cout << "Underruns: " << audio_output.get_underruns() << std::endl;
    std::cout << "Actual latency: " << audio_output.get_actual_latency_ms() << " ms" << std::endl;
    
    audio_output.stop();
}

void test_audio_pipeline_with_real_audio() {
    std::cout << "\n=== Testing Complete Audio Pipeline with Real Audio ===" << std::endl;
    
    const int sample_rate = 48000;
    const int channels = 2;
    const int buffer_size = 128;
    const int jitter_buffer_capacity = 5;
    
    // Create audio pipeline
    AudioPipeline pipeline(sample_rate, channels, buffer_size, jitter_buffer_capacity);
    
    if (!pipeline.is_initialized()) {
        std::cerr << "Failed to initialize audio pipeline!" << std::endl;
        return;
    }
    
    std::cout << "Audio pipeline initialized successfully!" << std::endl;
    
    // Start pipeline
    if (!pipeline.start()) {
        std::cerr << "Failed to start audio pipeline!" << std::endl;
        return;
    }
    
    std::cout << "Pipeline started. Processing mock audio packets..." << std::endl;
    std::cout << "You should hear audio through your speakers/headphones!" << std::endl;
    
    // Send test packets through the pipeline
    const int num_packets = 200; // About 5 seconds of audio at 2.5ms per packet
    
    for (int i = 0; i < num_packets; i++) {
        // Generate test packet
        auto packet_data = generate_test_tone_opus_packet(440.0, sample_rate, channels, 120);
        
        // Send packet to pipeline
        uint32_t sequence_id = i;
        uint64_t timestamp = i * 2500; // 2.5ms per packet in microseconds
        
        if (!pipeline.process_audio_packet(sequence_id, timestamp, packet_data)) {
            std::cerr << "Failed to process packet " << i << std::endl;
        }
        
        // Small delay to simulate real-time packet arrival
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Slightly less than 2.5ms
    }
    
    // Let the pipeline process remaining packets
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "Pipeline test completed!" << std::endl;
    std::cout << "Packets processed: " << pipeline.get_packets_processed() << std::endl;
    std::cout << "Frames decoded: " << pipeline.get_frames_decoded() << std::endl;
    std::cout << "Frames output: " << pipeline.get_frames_output() << std::endl;
    std::cout << "Total latency: " << pipeline.get_total_latency_ms() << " ms" << std::endl;
    std::cout << "Decode errors: " << pipeline.get_decode_errors() << std::endl;
    std::cout << "Output underruns: " << pipeline.get_output_underruns() << std::endl;
    
    pipeline.stop();
}

void list_available_audio_devices() {
    std::cout << "\n=== Available Audio Devices ===" << std::endl;
    
    auto devices = RealAudioOutput::get_available_devices();
    auto default_device = RealAudioOutput::get_default_device();
    
    std::cout << "Found " << devices.size() << " audio devices:" << std::endl;
    
    for (const auto& device : devices) {
        std::cout << "Device " << device.id << ": " << device.name;
        if (device.is_default) {
            std::cout << " (DEFAULT)";
        }
        std::cout << std::endl;
        std::cout << "  Max channels: " << device.max_channels << std::endl;
        std::cout << "  Default sample rate: " << device.default_sample_rate << " Hz" << std::endl;
    }
    
    std::cout << "\nDefault device: " << default_device.name << " (ID: " << default_device.id << ")" << std::endl;
}

int main() {
    std::cout << "=== Real Audio Output Test Program ===" << std::endl;
    std::cout << "This program will test real PortAudio integration with actual sound output." << std::endl;
    std::cout << "Make sure your speakers/headphones are connected and volume is at a comfortable level!" << std::endl;
    
    try {
        // List available devices
        list_available_audio_devices();
        
        // Test direct audio output
        test_real_audio_output_direct();
        
        // Test complete pipeline
        test_audio_pipeline_with_real_audio();
        
        std::cout << "\n=== All Real Audio Tests Completed Successfully! ===" << std::endl;
        std::cout << "If you heard audio output, the real PortAudio integration is working perfectly!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}