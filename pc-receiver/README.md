# Audio Receiver - PC Component

Ultra-low latency audio receiver for Windows PC, designed to receive and play audio streams from Android devices.

## Architecture

This PC receiver implements the network layer of the audio streaming pipeline with the following components:

### Phase 1: Network Layer (Current Implementation)
- **UDP Packet Receiver**: Low-latency UDP socket implementation
- **Packet Structure**: Custom binary format with sequence ID, timestamp, and payload
- **Multi-threaded Design**: Separate thread for network reception
- **Error Handling**: Robust packet validation and error reporting
- **Statistics**: Real-time monitoring of packet reception rates

### Packet Format
```
[sequence_id(4)] [timestamp(8)] [payload_size(4)] [payload(variable)]
```
- All fields are little-endian
- Maximum packet size: 2048 bytes
- Payload contains Opus-encoded audio data

## Building

### Prerequisites
- CMake 3.16 or higher
- C++17 compatible compiler (Visual Studio 2019+, GCC 8+, Clang 7+)
- Windows 10+ (for WASAPI support in future phases)

### Build Instructions

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
cmake --build . --config Release

# Run tests
ctest --output-on-failure
```

### Visual Studio
```bash
# Generate Visual Studio solution
cmake -G "Visual Studio 16 2019" -A x64 ..

# Open AudioReceiver.sln in Visual Studio
```

## Usage

### Running the Receiver
```bash
# Default port (12345)
./audio_receiver

# Custom port
./audio_receiver 8080
```

### Running Tests
```bash
# Run all tests
./network_tests

# Or with CTest
ctest --verbose
```

## Testing

The implementation follows Test-Driven Development (TDD) with comprehensive unit tests:

- **Packet Tests**: Serialization, deserialization, validation
- **UDP Receiver Tests**: Socket operations, packet reception, error handling
- **Integration Tests**: End-to-end packet flow

## Performance Targets

- **Network Latency**: < 3.0ms (target from PRD)
- **Packet Processing**: < 0.5ms per packet
- **Memory Usage**: < 10MB for network layer
- **CPU Usage**: < 5% on modern CPUs

## Future Phases

### Phase 2: Audio Decoding
- Opus decoder integration
- PCM buffer management
- Audio format conversion

### Phase 3: Audio Output
- PortAudio integration
- WASAPI exclusive mode
- ASIO support for pro hardware
- Device selection and configuration

### Phase 4: Advanced Features
- Adaptive jitter buffering
- Clock synchronization
- Latency measurement
- QoS optimization

## Development

### Code Style
- C++17 standard
- RAII principles
- Exception safety
- Clear naming conventions

### Testing Strategy
- Unit tests for all components
- Integration tests for data flow
- Performance benchmarks
- Memory leak detection

### Debugging
- Comprehensive logging
- Statistics collection
- Error reporting
- Network packet inspection

## Integration with Android

The PC receiver is designed to work with the Android audio capture application:

1. Android captures system audio via Oboe
2. Android encodes audio with Opus
3. Android sends UDP packets to PC
4. PC receives and validates packets
5. PC decodes and plays audio (future phases)

## License

Part of the Ultra-Low Latency Android-to-PC Audio Streaming project.