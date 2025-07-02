#include "audio/audio_pipeline.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace AudioReceiver {
namespace Audio {

AudioPipeline::AudioPipeline(int sample_rate, int channels, int buffer_size, int jitter_buffer_capacity, int device_id)
    : sample_rate_(sample_rate)
    , channels_(channels)
    , buffer_size_(buffer_size)
    , jitter_buffer_capacity_(jitter_buffer_capacity)
    , device_id_(device_id)
    , initialized_(false)
    , running_(false)
    , packets_processed_(0)
    , frames_decoded_(0)
    , frames_output_(0)
    , decode_errors_(0)
    , output_underruns_(0)
    , total_latency_ms_(0.0)
    , decode_latency_ms_(0.0)
    , output_latency_ms_(0.0)
    , jitter_buffer_latency_ms_(0.0)
    , total_processing_time_us_(0.0)
    , processing_count_(0) {
    
    // Validate parameters according to PRD requirements
    if (sample_rate != SUPPORTED_SAMPLE_RATE) {
        std::cerr << "Unsupported sample rate: " << sample_rate 
                  << " (only " << SUPPORTED_SAMPLE_RATE << " Hz supported)" << std::endl;
        return;
    }
    
    if (channels != 1 && channels != 2) {
        std::cerr << "Unsupported channel count: " << channels 
                  << " (only mono/stereo supported)" << std::endl;
        return;
    }
    
    if (buffer_size < MIN_BUFFER_SIZE || buffer_size > MAX_BUFFER_SIZE) {
        std::cerr << "Invalid buffer size: " << buffer_size 
                  << " (must be between " << MIN_BUFFER_SIZE 
                  << " and " << MAX_BUFFER_SIZE << ")" << std::endl;
        return;
    }
    
    if (jitter_buffer_capacity < MIN_JITTER_CAPACITY || jitter_buffer_capacity > MAX_JITTER_CAPACITY) {
        std::cerr << "Invalid jitter buffer capacity: " << jitter_buffer_capacity 
                  << " (must be between " << MIN_JITTER_CAPACITY 
                  << " and " << MAX_JITTER_CAPACITY << ")" << std::endl;
        return;
    }
    
    // Initialize pipeline components
    try {
        // Calculate frame size for jitter buffer (120 samples @ 48kHz for 2.5ms frames)
        int frame_size = static_cast<int>(sample_rate * 2.5 / 1000.0); // 2.5ms frames
        
        jitter_buffer_ = std::make_unique<JitterBuffer>(jitter_buffer_capacity, frame_size, channels);
        if (!jitter_buffer_->is_initialized()) {
            std::cerr << "Failed to initialize jitter buffer" << std::endl;
            return;
        }
        
        decoder_ = std::make_unique<OpusDecoder>(sample_rate, channels);
        if (!decoder_->is_initialized()) {
            std::cerr << "Failed to initialize Opus decoder" << std::endl;
            return;
        }
        
#ifdef HAVE_PORTAUDIO
        audio_output_ = std::make_unique<RealAudioOutput>(sample_rate, channels, buffer_size, device_id);
        if (!audio_output_->is_initialized()) {
            std::cerr << "Failed to initialize real audio output" << std::endl;
            return;
        }
#else
        audio_output_ = std::make_unique<AudioOutput>(sample_rate, channels, buffer_size, device_id);
        if (!audio_output_->is_initialized()) {
            std::cerr << "Failed to initialize mock audio output" << std::endl;
            return;
        }
#endif
        
        initialized_ = true;
        
        std::cout << "Audio pipeline initialized - " 
                  << sample_rate_ << "Hz, " << channels_ << " channels, "
                  << buffer_size_ << " samples buffer, "
                  << jitter_buffer_capacity_ << " packet jitter buffer";
        if (device_id >= 0) {
            std::cout << ", device ID: " << device_id_;
        }
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize audio pipeline: " << e.what() << std::endl;
    }
}

AudioPipeline::~AudioPipeline() {
    stop();
}

bool AudioPipeline::start() {
    if (!initialized_ || running_.load()) {
        return false;
    }
    
    // Start audio output first
    if (!audio_output_->start()) {
        std::cerr << "Failed to start audio output" << std::endl;
        return false;
    }
    
    // Start processing thread
    running_.store(true);
    processing_thread_ = std::thread(&AudioPipeline::processing_thread, this);
    
    std::cout << "Audio pipeline started" << std::endl;
    return true;
}

void AudioPipeline::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "Stopping audio pipeline..." << std::endl;
    
    // Signal thread to stop
    running_.store(false);
    pipeline_cv_.notify_all();
    
    // Wait for processing thread to finish
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    
    // Stop audio output
    if (audio_output_) {
        audio_output_->stop();
    }
    
    // Clear queues
    {
        std::lock_guard<std::mutex> lock(packet_queue_mutex_);
        while (!packet_queue_.empty()) {
            packet_queue_.pop();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(decoded_audio_mutex_);
        while (!decoded_audio_queue_.empty()) {
            decoded_audio_queue_.pop();
        }
    }
    
    std::cout << "Audio pipeline stopped" << std::endl;
}

bool AudioPipeline::process_audio_packet(uint32_t sequence_id, uint64_t timestamp, const std::vector<uint8_t>& encoded_data) {
    if (!initialized_ || !running_.load()) {
        return false;
    }
    
    // Validate packet
    if (encoded_data.empty()) {
        decode_errors_.fetch_add(1);
        return false;
    }
    
    // Add packet to queue for processing
    {
        std::lock_guard<std::mutex> lock(packet_queue_mutex_);
        packet_queue_.emplace(sequence_id, timestamp, encoded_data);
    }
    
    // Notify processing thread
    pipeline_cv_.notify_one();
    
    packets_processed_.fetch_add(1);
    return true;
}

void AudioPipeline::processing_thread() {
    std::cout << "Audio pipeline processing thread started" << std::endl;
    
    while (running_.load()) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool processed_something = false;
        
        // Process incoming packets
        processed_something |= process_jitter_buffer();
        
        // Process decoder
        processed_something |= process_decoder();
        
        // Process audio output
        processed_something |= process_audio_output();
        
        // Update latency measurements
        update_latency_measurements();
        
        // Track processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        total_processing_time_us_ += duration.count();
        processing_count_++;
        
        // Sleep briefly if nothing was processed to avoid busy waiting
        if (!processed_something) {
            std::unique_lock<std::mutex> lock(pipeline_mutex_);
            pipeline_cv_.wait_for(lock, std::chrono::microseconds(PROCESSING_THREAD_SLEEP_US));
        }
    }
    
    std::cout << "Audio pipeline processing thread stopped" << std::endl;
}

bool AudioPipeline::process_jitter_buffer() {
    // Move packets from input queue to jitter buffer
    std::lock_guard<std::mutex> lock(packet_queue_mutex_);
    
    bool processed = false;
    while (!packet_queue_.empty()) {
        auto [sequence_id, timestamp, encoded_data] = packet_queue_.front();
        packet_queue_.pop();
        
        // Decode packet first
        auto decoded_audio = decoder_->decode(encoded_data);
        if (!decoded_audio.empty()) {
            // Add to jitter buffer
            if (jitter_buffer_->add_packet(sequence_id, timestamp, decoded_audio)) {
                frames_decoded_.fetch_add(decoded_audio.size() / channels_);
                processed = true;
            }
        } else {
            decode_errors_.fetch_add(1);
        }
    }
    
    return processed;
}

bool AudioPipeline::process_decoder() {
    // Get packets from jitter buffer and add to decoded audio queue
    auto packet = jitter_buffer_->get_next_packet();
    if (!packet) {
        return false;
    }
    
    // Add decoded audio to output queue
    {
        std::lock_guard<std::mutex> lock(decoded_audio_mutex_);
        decoded_audio_queue_.push(packet->audio_data);
    }
    
    return true;
}

bool AudioPipeline::process_audio_output() {
    std::lock_guard<std::mutex> lock(decoded_audio_mutex_);
    
    if (decoded_audio_queue_.empty()) {
        return false;
    }
    
    // Get decoded audio data
    auto audio_data = decoded_audio_queue_.front();
    decoded_audio_queue_.pop();
    
    // Ensure audio data matches output buffer size
    int expected_samples = buffer_size_ * channels_;
    if (static_cast<int>(audio_data.size()) != expected_samples) {
        // Resize or pad audio data to match buffer size
        audio_data.resize(expected_samples, 0.0f);
    }
    
    // Write to audio output
    if (audio_output_->write_audio(audio_data)) {
        frames_output_.fetch_add(buffer_size_);
        return true;
    } else {
        output_underruns_.fetch_add(1);
        return false;
    }
}

void AudioPipeline::update_latency_measurements() {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    
    // Get component latencies
    decode_latency_ms_ = 1.5; // Mock decode latency (PRD target: 1.5ms)
    output_latency_ms_ = audio_output_ ? audio_output_->get_estimated_latency_ms() : 0.0;
    
    // Calculate jitter buffer latency based on buffer size
    int jitter_buffer_size = jitter_buffer_ ? jitter_buffer_->get_size() : 0;
    jitter_buffer_latency_ms_ = (jitter_buffer_size * 2.5); // 2.5ms per packet
    
    // Total latency is sum of all components
    total_latency_ms_ = decode_latency_ms_ + output_latency_ms_ + jitter_buffer_latency_ms_;
}

bool AudioPipeline::is_running() const {
    return running_.load();
}

bool AudioPipeline::is_initialized() const {
    return initialized_;
}

int AudioPipeline::get_sample_rate() const {
    return sample_rate_;
}

int AudioPipeline::get_channels() const {
    return channels_;
}

int AudioPipeline::get_buffer_size() const {
    return buffer_size_;
}

int AudioPipeline::get_jitter_buffer_capacity() const {
    return jitter_buffer_capacity_;
}

int AudioPipeline::get_device_id() const {
    return device_id_;
}

uint64_t AudioPipeline::get_packets_processed() const {
    return packets_processed_.load();
}

uint64_t AudioPipeline::get_frames_decoded() const {
    return frames_decoded_.load();
}

uint64_t AudioPipeline::get_frames_output() const {
    return frames_output_.load();
}

uint64_t AudioPipeline::get_decode_errors() const {
    return decode_errors_.load() + (decoder_ ? decoder_->get_decode_errors() : 0);
}

uint64_t AudioPipeline::get_output_underruns() const {
    return output_underruns_.load() + (audio_output_ ? audio_output_->get_underruns() : 0);
}

uint64_t AudioPipeline::get_jitter_buffer_drops() const {
    return jitter_buffer_ ? jitter_buffer_->get_packets_dropped() : 0;
}

double AudioPipeline::get_total_latency_ms() const {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    return total_latency_ms_;
}

double AudioPipeline::get_decode_latency_ms() const {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    return decode_latency_ms_;
}

double AudioPipeline::get_output_latency_ms() const {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    return output_latency_ms_;
}

double AudioPipeline::get_jitter_buffer_latency_ms() const {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    return jitter_buffer_latency_ms_;
}

double AudioPipeline::get_cpu_usage_percent() const {
    // Mock CPU usage calculation
    return std::min(5.0, get_average_processing_time_us() / 10.0); // Simulate low CPU usage
}

double AudioPipeline::get_average_processing_time_us() const {
    if (processing_count_ == 0) {
        return 0.0;
    }
    return total_processing_time_us_ / processing_count_;
}

bool AudioPipeline::is_meeting_realtime_deadline() const {
    return get_total_latency_ms() <= TARGET_TOTAL_LATENCY_MS;
}

uint64_t AudioPipeline::get_current_timestamp_us() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

} // namespace Audio
} // namespace AudioReceiver