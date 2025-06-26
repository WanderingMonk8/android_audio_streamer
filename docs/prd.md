# Product Requirements Document (PRD)

## Project Title

Ultra-Low Latency Android-to-PC Audio Streaming App

---

## Objective

Design and implement a mobile application for Android that captures system audio and streams it to a PC over Wi-Fi with a total end-to-end latency of under **10 milliseconds**. This application aims to enable real-time audio transfer for use cases such as gaming, music production, live performance monitoring, and communication.

---

## Key Features

### Audio Capture

* Capture internal (system) audio on Android using `MediaProjection` API (Android 10+)
* Use `AudioPlaybackCaptureConfiguration` for non-microphone sources
* Implement native `AudioRecord` via NDK or Oboe for lowest latency
* **Optional Microphone Mode:**

  * Switch to microphone input via `AudioRecord` with minimal buffer
  * Maintain ultra-low latency settings identical to system audio path
  * Allow user to toggle between mic and system audio in real-time
  * **Optional noise cancellation**:

    * Use built-in Android `NoiseSuppressor` API
    * Advanced option to enable `RNNoise` for higher quality (with additional CPU usage)
    * Include toggle in Android UI with performance impact warnings

### Audio Encoding

* Provide users with selectable audio codecs (e.g., Opus, PCM, AAC)
* Each codec displays its **estimated encoding latency** and bandwidth usage in the UI
* Default to **libopus in VoIP mode, CELT-only** for lowest delay
* Recommended frame size: **2.5 ms** (120 samples @ 48kHz)
* Display codec options alongside their performance trade-offs (latency, CPU usage, quality)

### Networking

* Use **raw UDP** sockets
* Custom binary packet format (sequence ID + timestamp + payload)
* Optional support for **Wi-Fi Direct** to bypass router latency
* Set `TrafficClass = 0x10` (Low Delay)
* Micro-batching option: allow atomic packets of 5 ms when needed

### PC Receiver

* Multi-threaded receiver implemented in C++ (Windows only)
* Use **libopus** for decoding
* Use **PortAudio** with **WASAPI** or **ASIO** for low-latency playback
* Target playback buffer size: **64–128 samples**
* WASAPI exclusive mode or ASIO for direct audio path on Windows
* **Allow user to select desired output audio device** and preferred backend (WASAPI/ASIO) in the PC UI

### Performance Optimizations

* Pipeline Architecture:
  - Dedicated threads for each stage (capture, encode, etc.)
  - Lock-free ring buffers between stages
  - Real-time priorities (SCHED_FIFO/THREAD_PRIORITY_AUDIO)

* Android Specific:
  - Use Oboe's AAudio with MMAP when available
  - Disable battery optimizations via DISABLE_KEYGUARD
  - Fallback to OpenSL ES when needed

* PC Specific:
  - WASAPI exclusive mode by default
  - Optional ASIO support for pro hardware
  - Adaptive buffer sizing (64-256 samples)

* Network:
  - DSCP marking (CS5) for QoS
  - 20% FEC redundancy
  - Jitter buffer (3-5 packets)

* Clock Sync:
  - NTP alignment during handshake
  - Per-packet drift compensation
  - Speed adjustment with phase vocoder

* Monitoring:
  - Real-time latency stats
  - Packet loss metrics
  - CPU/memory usage tracking

### UI / UX

* Simple start/stop UI on Android
* Toggle between **System Audio** and **Microphone Mode**
* QR code pairing option
* **Manual IP address entry available on both Android and PC UI**
* Codec selection menu with **real-time latency estimates**
* Display real-time latency stats (avg, min, max)

---

## Technical Requirements

### Android

* Minimum SDK: 29 (Android 10)
* Language: Kotlin (frontend), C++ (audio stack)
* Libraries: Oboe, libopus, NDK socket APIs

### PC Receiver

* OS: Windows only
* Language: C++ preferred, Python fallback
* Libraries: libopus, PortAudio, asio (optional)

---

## Revised Latency Budget (Realistic Targets)

| Stage     | Target Latency | Notes |
| --------- | -------------- | ----- |
| Capture   | 1.2 ms         | Device-dependent (Oboe MMAP) |
| Encode    | 3.0 ms         | Opus CELT 5ms frames |
| Network   | 3.0 ms         | 5GHz Wi-Fi with QoS |
| Decode    | 1.5 ms         | libopus optimized |
| Playback  | 2.0 ms         | WASAPI/ASIO exclusive |
| **Total** | **<10.7 ms**   | Achievable on flagship devices |

---

## Stretch Goals

* Automatic PC discovery via mDNS
* Adaptive jitter buffer tuning
* Support for stereo and multichannel audio
* Support for remote control (volume, pause)
* Voice activity detection for mic mode

---

## Deliverables

* Android APK (debug + release builds)
* PC receiver binary (Windows only)
* Complete source code with build scripts
* Performance validation report with latency benchmarks
* Markdown documentation (README, setup guide, troubleshooting)

---

## Timeline (Estimated)

| Week | Milestone                                 |
| ---- | ----------------------------------------- |
| 1-2  | Core pipeline (WASAPI, Oboe AAudio)       |
| 3-4  | Adaptive buffers + network optimization  |
| 5-6  | Advanced features (ASIO, OpenSL ES)      |
| 7-8  | Performance tuning + stress testing      |
| 9    | Final documentation + release packaging   |

---

## Risks & Mitigations

* **Android Device Fragmentation**:
  - Tiered performance targets
  - Extensive device testing matrix
  - Fallback audio paths

* **Network Instability**:
  - Adaptive jitter buffer
  - FEC with 20% redundancy
  - DSCP QoS marking

* **Clock Drift**:
  - Hybrid NTP+PTP sync
  - Speed-adjusting resampler
  - Continuous drift measurement

* **PC Audio Compatibility**:
  - WASAPI as default
  - ASIO as optional
  - Comprehensive audio backend testing

* **Battery Optimization Interference**:
  - DISABLE_KEYGUARD permission
  - Foreground service with sticky notification
  - User education about power settings

* **Thermal Throttling**:
  - CPU usage monitoring
  - Dynamic quality adjustment
  - Cooling period recommendations

---

## Success Criteria

* End-to-end latency < 10 ms on 5 GHz Wi-Fi
* No packet drop or stutter in 10+ minute continuous sessions
* Seamless experience for end users with simple setup
* Smooth switching between system audio and microphone with no audible glitch
* Demonstrated total audio latency < 6 ms in controlled test conditions

---

## Authors

* Project Lead: \Ken L
* Android Engineer: \Ken L
* PC Engineer: \Ken L
* Audio Consultant: \Ken L

---
