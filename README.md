Ultra-Low Latency Android-to-PC Audio Streaming App

## Overview
Design and implement a mobile application for Android that captures system audio and streams it to a PC over Wi-Fi with a total end-to-end latency of under 10 milliseconds. Enables real-time audio transfer for gaming, music production, live performance monitoring, and communication.

## Key Features

### Audio Capture
- System audio capture via MediaProjection API (Android 10+)
- Optional microphone mode with switchable input sources
- Noise cancellation options:
  - Built-in Android NoiseSuppressor
  - RNNoise for higher quality (with increased CPU usage)

### Audio Encoding
- Selectable codecs (Opus, PCM, AAC) with latency estimates
- Default: libopus in VoIP mode (CELT-only)
- Recommended frame size: 2.5ms (120 samples @48kHz)

### Networking
- Raw UDP sockets with custom binary packet format
- Optional Wi-Fi Direct to bypass router latency
- TrafficClass = 0x10 (Low Delay)
- 20% FEC redundancy + jitter buffer (3-5 packets)

### PC Receiver
- Multi-threaded C++ implementation (Windows)
- libopus decoding + PortAudio playback
- WASAPI/ASIO support for low-latency output
- Target buffer size: 64-128 samples

### Performance Optimizations
- Dedicated threads with real-time priorities
- Lock-free ring buffers between stages
- Oboe's AAudio with MMAP when available
- Adaptive buffer sizing (64-256 samples)
- NTP alignment + drift compensation

## Technical Requirements

### Android
- Minimum SDK: 29 (Android 10)
- Kotlin (UI) + C++ (audio stack)
- Libraries: Oboe, libopus, NDK sockets

### PC Receiver
- Windows only
- C++ preferred (Python fallback)
- Libraries: libopus, PortAudio

## Installation

### Android
1. Clone repo and open in Android Studio
2. Build and run on Android 10+ device

### PC Receiver (C++ Version)
1. Build with CMake (requires libopus + PortAudio)
2. Run binary and select audio device/backend

### PC Receiver (Python Prototype)
$ pip install pyaudio opuslib
$ python receiver.py

## Usage

### Android
1. Launch app and select audio source
2. Configure noise cancellation
3. Select codec and view latency
4. Enter PC IP manually or via QR
5. Press Start

### PC
1. Launch receiver application
2. Select output device/backend
3. Start listening for stream

## Revised Latency Budget

| Stage     | Target Latency | Notes |
| --------- | -------------- | ----- |
| Capture   | 1.2 ms         | Device-dependent |
| Encode    | 3.0 ms         | Opus CELT 5ms frames |
| Network   | 3.0 ms         | 5GHz Wi-Fi with QoS |
| Decode    | 1.5 ms         | libopus optimized |
| Playback  | 2.0 ms         | WASAPI/ASIO exclusive |
| **Total** | **<10.7 ms**   | Achievable on flagship devices |

## Performance Tips
- Use 5GHz Wi-Fi or Wi-Fi Direct
- Enable real-time thread priorities
- Use exclusive audio modes (WASAPI/ASIO)
- Monitor CPU/memory usage
- Consider thermal throttling effects

## Stretch Goals
- Automatic PC discovery via mDNS
- Adaptive jitter buffer tuning
- Stereo/multichannel support
- Remote control (volume, pause)
- Voice activity detection

## Troubleshooting
- No audio? Check firewall and IP matching
- Lag/stutter? Reduce codec complexity/buffer size
- High CPU? Disable RNNoise
- Clock drift? Enable NTP sync

## Risks & Mitigations
- **Device Fragmentation**: Tiered performance targets + testing matrix
- **Network Instability**: Adaptive jitter buffer + FEC
- **Clock Drift**: NTP+PTP sync + speed adjustment
- **Battery Optimization**: Foreground service + user education

## Success Criteria
- <10ms latency on 5GHz Wi-Fi
- No packet drop in 10+ minute sessions
- <6ms latency in controlled tests
- Smooth source switching without glitches

## Contributors
- Project Lead: Ken L
- Android Engineer: Ken L
- PC Engineer: Ken L
- Audio Consultant: Ken L

## License
MIT License

## Acknowledgements
- Google Oboe Library
- Xiph.org Opus Codec
- PortAudio Team
- RNNoise by Xiph.org
