# Android → PC Integration Test Plan

## Overview
This document outlines the comprehensive testing approach for validating the Android → PC audio streaming integration using the existing implementations.

## Test Objectives
1. **Validate packet format compatibility** between Android and PC
2. **Confirm end-to-end network transmission** works correctly
3. **Measure actual latency** in real network conditions
4. **Test network optimization features** (QoS, FEC, adaptive buffering)
5. **Identify any integration issues** before proceeding with enhancements

## Test Environment Setup

### PC Receiver Setup
```bash
# Build PC receiver
cd pc-receiver
mkdir build && cd build
cmake ..
make

# Run basic tests first
./network_tests
./audio_tests

# Start PC receiver (listens on port 12345 by default)
./audio_receiver
```

### Android Setup
- **Target IP**: Update `EncodingService.kt` with PC IP address
- **Network**: Ensure Android device and PC are on same WiFi network
- **Permissions**: Verify network permissions in AndroidManifest.xml

## Test Scenarios

### Phase 1: Basic Connectivity Tests

#### Test 1.1: PC Receiver Standalone
**Objective**: Verify PC receiver works independently
**Steps**:
1. Build and run PC receiver
2. Run network tests: `./network_tests`
3. Run audio tests: `./audio_tests`
4. Start receiver: `./audio_receiver`
5. Verify it listens on port 12345

**Expected Results**:
- All tests pass
- Receiver starts without errors
- Logs show "Listening on UDP port: 12345"

#### Test 1.2: Android Network Service Standalone
**Objective**: Verify Android network components work
**Steps**:
1. Run Android unit tests
2. Test NetworkService with mock data
3. Verify UdpSender creates packets correctly

**Expected Results**:
- All Android network tests pass
- Packets serialize correctly
- No network errors in logs

### Phase 2: Packet Format Validation

#### Test 2.1: Packet Serialization Compatibility
**Objective**: Confirm Android packets match PC expectations
**Steps**:
1. Create test packets on Android
2. Serialize and log packet bytes
3. Compare with PC packet format
4. Test deserialization on PC side

**Expected Results**:
- Packet format matches exactly
- PC can deserialize Android packets
- No format errors

#### Test 2.2: Cross-Platform Packet Exchange
**Objective**: Test actual packet transmission
**Steps**:
1. Start PC receiver
2. Send test packets from Android
3. Verify PC receives and processes packets
4. Check packet statistics on both sides

**Expected Results**:
- PC receives Android packets
- Sequence IDs match
- No packet corruption
- Statistics update correctly

### Phase 3: End-to-End Integration

#### Test 3.1: Mock Audio Data Transmission
**Objective**: Test complete pipeline with mock audio
**Steps**:
1. Start PC receiver with audio output
2. Generate mock audio data on Android
3. Encode and transmit to PC
4. Verify PC decodes and plays audio
5. Measure end-to-end latency

**Expected Results**:
- Audio packets transmitted successfully
- PC decodes audio without errors
- Audio output works (if PortAudio available)
- Latency < 50ms (initial target)

#### Test 3.2: Real Audio Capture Integration
**Objective**: Test with actual Android audio capture
**Steps**:
1. Enable Oboe audio capture on Android
2. Capture real audio (microphone or system)
3. Encode and stream to PC
4. Verify real-time audio streaming
5. Test for extended periods (5+ minutes)

**Expected Results**:
- Real audio captured and transmitted
- No audio dropouts or glitches
- Stable streaming for extended periods
- CPU usage acceptable on Android

### Phase 4: Network Optimization Testing

#### Test 4.1: QoS Validation
**Objective**: Test traffic prioritization
**Steps**:
1. Monitor network traffic with Wireshark
2. Verify DSCP marking on packets
3. Test under network congestion
4. Compare with/without QoS

**Expected Results**:
- Packets marked with traffic class
- Better performance under congestion
- Router prioritizes audio traffic

#### Test 4.2: Network Condition Simulation
**Objective**: Test adaptive features
**Steps**:
1. Simulate packet loss (1%, 5%, 10%)
2. Simulate network jitter
3. Test with limited bandwidth
4. Verify adaptive behavior

**Expected Results**:
- PC adapts jitter buffer size
- FEC recovery works (if implemented)
- Graceful degradation under poor conditions

### Phase 5: Performance Measurement

#### Test 5.1: Latency Measurement
**Objective**: Measure actual end-to-end latency
**Steps**:
1. Generate test tone on Android
2. Capture PC audio output
3. Measure time difference
4. Test under various conditions

**Expected Results**:
- Latency < 10ms (target)
- Consistent latency measurements
- Low jitter in latency

#### Test 5.2: Throughput and Resource Usage
**Objective**: Measure system performance
**Steps**:
1. Monitor CPU usage on both sides
2. Measure network bandwidth usage
3. Test battery usage on Android
4. Monitor memory usage

**Expected Results**:
- CPU usage < 20% on both sides
- Bandwidth usage reasonable
- Acceptable battery drain
- No memory leaks

## Test Implementation

### Android Integration Test
Create `AndroidPCIntegrationTest.kt`:
```kotlin
@Test
fun testEndToEndPacketTransmission() {
    // Test implementation
}

@Test
fun testLatencyMeasurement() {
    // Latency test implementation
}
```

### PC Test Utilities
Create `pc-receiver/tests/integration_test.cpp`:
```cpp
// Integration test utilities
void test_android_packet_reception();
void measure_packet_processing_latency();
```

## Success Criteria

### Functional Requirements
- [ ] PC receiver successfully receives Android packets
- [ ] Packet format 100% compatible
- [ ] Audio decoding works correctly
- [ ] No packet corruption or loss in ideal conditions

### Performance Requirements
- [ ] End-to-end latency < 10ms (target)
- [ ] Stable streaming for 30+ minutes
- [ ] CPU usage < 20% on both sides
- [ ] No audio dropouts under normal conditions

### Network Requirements
- [ ] QoS marking works correctly
- [ ] Graceful handling of network issues
- [ ] Adaptive behavior under poor conditions
- [ ] Recovery from network interruptions

## Test Execution Schedule

### Day 1: Environment Setup
- Build PC receiver
- Set up Android test environment
- Verify basic functionality

### Day 2: Basic Integration
- Test packet transmission
- Validate format compatibility
- Basic end-to-end testing

### Day 3: Performance Testing
- Latency measurement
- Network condition testing
- Optimization validation

## Troubleshooting Guide

### Common Issues
1. **PC receiver not receiving packets**
   - Check firewall settings
   - Verify IP address configuration
   - Test with telnet/nc

2. **Packet format errors**
   - Compare serialization output
   - Check endianness
   - Validate packet size

3. **Audio issues**
   - Verify codec compatibility
   - Check audio device availability
   - Test with mock audio first

4. **Network issues**
   - Test on same machine (localhost)
   - Check WiFi network configuration
   - Verify port accessibility

## Next Steps After Testing

Based on test results:
1. **If successful**: Proceed with minor optimizations (QoS enhancement)
2. **If issues found**: Address compatibility problems first
3. **Performance gaps**: Identify optimization opportunities
4. **Integration problems**: Fix before proceeding to next phase

## Test Artifacts

### Logs to Collect
- Android logcat output
- PC receiver console output
- Network packet captures (Wireshark)
- Performance metrics

### Metrics to Track
- Packet transmission success rate
- End-to-end latency measurements
- CPU and memory usage
- Network bandwidth utilization
- Audio quality metrics

---

**Ready to begin integration testing!**