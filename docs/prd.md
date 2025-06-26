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

* Multi-threaded receiver implemented in C++ (or Python prototype)
* Use **libopus** for decoding
* Use **PortAudio**, **WASAPI**, or **ASIO** for low-latency playback
* Target playback buffer size: **64–128 samples**
* WASAPI exclusive mode or ALSA with isolated thread/core for playback
* ASIO support for Windows users with compatible hardware (optional)
* **Allow user to select desired output audio device** and preferred backend (WASAPI/ASIO) in the PC UI

### Performance Optimizations

* Use separate threads for capture, encode, transmit, receive, decode, and playback
* Lock-free, fixed-size ring buffers for inter-thread communication
* Real-time thread priorities on Android and PC (SCHED\_FIFO or THREAD\_PRIORITY\_AUDIO)
* Disable audio processing and battery optimizations on Android
* Support predictive clock drift compensation using lightweight resampling or frame dropping
* Use MMAP mode with Oboe for direct audio buffer access
* Zero-allocation, preallocated buffer strategy

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

* OS: Windows/Linux/macOS
* Language: C++ preferred, Python fallback
* Libraries: libopus, PortAudio, asio (optional)

---

## Latency Budget Breakdown

| Stage     | Target Latency |
| --------- | -------------- |
| Capture   | 0.5 ms         |
| Encode    | 1.0 ms         |
| UDP Send  | 0.2 ms         |
| Network   | 1.0 ms         |
| Decode    | 0.8 ms         |
| Playback  | 1.3 ms         |
| **Total** | **<5.0 ms**    |

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
* PC receiver binary (Windows + Linux)
* Complete source code with build scripts
* Performance validation report with latency benchmarks
* Markdown documentation (README, setup guide, troubleshooting)

---

## Timeline (Estimated)

| Week | Milestone                                 |
| ---- | ----------------------------------------- |
| 1    | Audio capture + encode pipeline (Android) |
| 2    | UDP networking + PC receiver MVP          |
| 3    | Playback, decode, and sync tuning         |
| 4    | UI polish + latency testing               |
| 5    | Cross-platform builds + documentation     |

---

## Risks & Mitigations

* **Android device limitations**: Use fallback modes and test on Pixel/Samsung
* **Wi-Fi jitter**: Use adaptive buffer and predictive compensation
* **CPU usage spikes**: Prioritize audio threads, use native code
* **Mic mode conflict**: Apply isolation layers between mic/system audio paths to ensure consistency
* **Thread interference**: Use real-time scheduling and core isolation to reduce OS-level contention

---

## Success Criteria

* End-to-end latency < 10 ms on 5 GHz Wi-Fi
* No packet drop or stutter in 10+ minute continuous sessions
* Seamless experience for end users with simple setup
* Smooth switching between system audio and microphone with no audible glitch
* Demonstrated total audio latency < 6 ms in controlled test conditions

---

## Authors

* Project Lead: \[Name TBD]
* Android Engineer: \[Name TBD]
* PC Engineer: \[Name TBD]
* Audio Consultant: \[Name TBD]

---
