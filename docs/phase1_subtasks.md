# Phase 1: Core Pipeline Development Tasks

## Android Audio Stack Implementation
1. **Setup Android Environment**
   - Create project with min SDK 29
   - Add Oboe and libopus dependencies
   - Configure audio permissions

2. **Audio Capture Implementation**
   - Create AudioRecord instance with optimal buffers
   - Configure MediaProjection for system audio
   - Implement high-priority capture thread

3. **Encoding Pipeline**
   - Integrate libopus with CELT-only mode
   - Set 2.5ms frame size (120 samples @48kHz)
   - Create dedicated encoding thread

## PC Receiver Implementation
4. **Windows Audio Stack**
   - PortAudio WASAPI configuration
   - Target 64-128 sample buffers
   - Implement exclusive mode audio

5. **Network Receiver**
   - UDP socket implementation
   - Custom packet format handling
   - Jitter buffer (3-5 packets)

## Networking Implementation
6. **Custom Protocol**
   - Sequence numbering
   - Timestamp handling
   - 20% FEC redundancy

7. **Quality of Service**
   - DSCP marking (CS5)
   - TrafficClass = 0x10
   - Packet prioritization

## Monitoring & Fallbacks
8. **Latency Measurement**
   - End-to-end timing
   - Clock synchronization
   - Drift compensation

9. **Android Fallbacks**
   - OpenSL ES implementation
   - Dynamic buffer adjustment
   - Thermal throttling detection

## Validation
10. **Testing Protocol**
    - Latency benchmarks
    - Packet loss simulation
    - Stress testing