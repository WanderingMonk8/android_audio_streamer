#include "audio/jitter_buffer.h"
#include <iostream>
#include <algorithm>
#include <chrono>

namespace AudioReceiver {
namespace Audio {

JitterBuffer::JitterBuffer(int capacity, int frame_size, int channels)
    : capacity_(capacity)
    , frame_size_(frame_size)
    , channels_(channels)
    , initialized_(false)
    , next_expected_sequence_(0)
    , last_sequence_added_(0)
    , packets_added_(0)
    , packets_retrieved_(0)
    , packets_dropped_(0)
    , duplicates_dropped_(0)
    , last_packet_timestamp_(0)
    , jitter_sum_ms_(0.0)
    , jitter_count_(0)
    , max_sequence_gap_(0) {
    
    // Validate parameters according to PRD requirements
    if (capacity < MIN_CAPACITY || capacity > MAX_CAPACITY) {
        std::cerr << "Invalid buffer capacity: " << capacity 
                  << " (must be between " << MIN_CAPACITY 
                  << " and " << MAX_CAPACITY << ")" << std::endl;
        return;
    }
    
    if (frame_size < MIN_FRAME_SIZE || frame_size > MAX_FRAME_SIZE) {
        std::cerr << "Invalid frame size: " << frame_size 
                  << " (must be between " << MIN_FRAME_SIZE 
                  << " and " << MAX_FRAME_SIZE << ")" << std::endl;
        return;
    }
    
    if (channels < MIN_CHANNELS || channels > MAX_CHANNELS) {
        std::cerr << "Invalid channel count: " << channels 
                  << " (must be between " << MIN_CHANNELS 
                  << " and " << MAX_CHANNELS << ")" << std::endl;
        return;
    }
    
    initialized_ = true;
    
    std::cout << "Jitter buffer initialized - capacity: " << capacity_ 
              << ", frame size: " << frame_size_ << " samples, "
              << channels_ << " channels" << std::endl;
}

JitterBuffer::~JitterBuffer() {
    clear();
}

bool JitterBuffer::add_packet(uint32_t sequence_id, uint64_t timestamp, const std::vector<float>& audio_data) {
    if (!initialized_) {
        return false;
    }
    
    if (!validate_packet(sequence_id, audio_data)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    // Check for duplicate packets
    if (buffer_.find(sequence_id) != buffer_.end()) {
        duplicates_dropped_.fetch_add(1);
        return false;
    }
    
    // Handle buffer overflow
    if (static_cast<int>(buffer_.size()) >= capacity_) {
        drop_oldest_packet();
    }
    
    // Add packet to buffer
    auto packet = std::make_unique<AudioPacket>(sequence_id, timestamp, audio_data);
    buffer_[sequence_id] = std::move(packet);
    
    // Update statistics
    packets_added_.fetch_add(1);
    last_sequence_added_ = sequence_id;
    
    // Update jitter statistics
    update_jitter_stats(timestamp);
    
    // Track sequence gaps
    if (packets_added_.load() > 1) {
        uint32_t gap = (sequence_id > next_expected_sequence_) ? 
                       (sequence_id - next_expected_sequence_) : 0;
        max_sequence_gap_ = std::max(max_sequence_gap_, gap);
    }
    
    return true;
}

std::unique_ptr<AudioPacket> JitterBuffer::get_next_packet() {
    if (!initialized_) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (buffer_.empty()) {
        return nullptr;
    }
    
    // Find the packet with the smallest sequence ID
    auto it = buffer_.begin();
    uint32_t sequence_id = it->first;
    auto packet = std::move(it->second);
    buffer_.erase(it);
    
    // Update next expected sequence
    next_expected_sequence_ = sequence_id + 1;
    
    // Update statistics
    packets_retrieved_.fetch_add(1);
    
    return packet;
}

void JitterBuffer::clear() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    buffer_.clear();
    next_expected_sequence_ = 0;
    last_sequence_added_ = 0;
    
    // Reset statistics
    packets_added_.store(0);
    packets_retrieved_.store(0);
    packets_dropped_.store(0);
    duplicates_dropped_.store(0);
    
    // Reset jitter stats
    last_packet_timestamp_ = 0;
    jitter_sum_ms_ = 0.0;
    jitter_count_ = 0;
    max_sequence_gap_ = 0;
}

bool JitterBuffer::is_empty() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return buffer_.empty();
}

bool JitterBuffer::is_full() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return static_cast<int>(buffer_.size()) >= capacity_;
}

bool JitterBuffer::is_initialized() const {
    return initialized_;
}

int JitterBuffer::get_capacity() const {
    return capacity_;
}

int JitterBuffer::get_frame_size() const {
    return frame_size_;
}

int JitterBuffer::get_channels() const {
    return channels_;
}

int JitterBuffer::get_size() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return static_cast<int>(buffer_.size());
}

uint64_t JitterBuffer::get_packets_added() const {
    return packets_added_.load();
}

uint64_t JitterBuffer::get_packets_retrieved() const {
    return packets_retrieved_.load();
}

uint64_t JitterBuffer::get_packets_dropped() const {
    return packets_dropped_.load();
}

uint64_t JitterBuffer::get_duplicates_dropped() const {
    return duplicates_dropped_.load();
}

double JitterBuffer::get_average_jitter_ms() const {
    if (jitter_count_ == 0) {
        return 0.0;
    }
    return jitter_sum_ms_ / jitter_count_;
}

uint32_t JitterBuffer::get_max_sequence_gap() const {
    return max_sequence_gap_;
}

bool JitterBuffer::validate_packet(uint32_t sequence_id, const std::vector<float>& audio_data) {
    // Validate audio data size
    int expected_samples = frame_size_ * channels_;
    if (static_cast<int>(audio_data.size()) != expected_samples) {
        std::cerr << "Invalid audio data size: " << audio_data.size() 
                  << " (expected " << expected_samples << ")" << std::endl;
        return false;
    }
    
    // Sequence ID validation (basic sanity check)
    (void)sequence_id; // Suppress unused parameter warning for now
    
    return true;
}

void JitterBuffer::drop_oldest_packet() {
    // Buffer mutex should already be locked when this is called
    
    if (buffer_.empty()) {
        return;
    }
    
    // Find and remove the packet with the smallest sequence ID
    auto it = buffer_.begin();
    buffer_.erase(it);
    
    packets_dropped_.fetch_add(1);
}

void JitterBuffer::update_jitter_stats(uint64_t timestamp) {
    if (last_packet_timestamp_ != 0) {
        // Calculate inter-packet time difference
        uint64_t time_diff = (timestamp > last_packet_timestamp_) ? 
                            (timestamp - last_packet_timestamp_) : 
                            (last_packet_timestamp_ - timestamp);
        
        // Convert to milliseconds (assuming timestamp is in microseconds)
        double jitter_ms = time_diff / 1000.0;
        
        jitter_sum_ms_ += jitter_ms;
        jitter_count_++;
    }
    
    last_packet_timestamp_ = timestamp;
}

} // namespace Audio
} // namespace AudioReceiver