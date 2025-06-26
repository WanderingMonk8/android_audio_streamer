Ultra-Low Latency Android-to-PC Audio Streaming App

Overview:
This project enables ultra-low latency audio streaming from an Android device to a PC over Wi-Fi. It supports system audio and microphone input, with configurable codec and playback options, targeting total latency under 10 milliseconds.

Features:

* System Audio and Microphone Capture on Android (Android 10+)
* Optional noise cancellation for microphone input (built-in or RNNoise)
* Selectable audio codecs (Opus, PCM, AAC) with estimated latency displayed
* UDP-based Wi-Fi streaming with optional Wi-Fi Direct mode
* PC playback via WASAPI or ASIO with device selection support (Windows only)
* Low-latency pipeline with threading, lock-free buffers, and real-time scheduling

Requirements:
Android:

* Android 10+
* Microphone and media capture permissions
* Foreground service permissions for background operation

PC:

* Windows only
* libopus, PortAudio (with ASIO support for low-latency playback)
* Python 3 or C++ compiler (depending on receiver version)

Installation:
Android:

1. Clone the repo and open the Android project in Android Studio.
2. Build and run the app on a device running Android 10 or later.

PC Receiver (Python Prototype):
\$ pip install pyaudio opuslib
\$ python receiver.py

PC Receiver (C++ Version for Windows):

1. Build using CMake with dependencies: libopus, PortAudio (enable WASAPI/ASIO backends)
2. Run binary and select desired audio device/backend (WASAPI or ASIO)

Usage:
Android:

1. Launch the app and select System Audio or Microphone Mode
2. Optionally enable Noise Cancellation
3. Select codec and view estimated latency
4. Enter the PC’s IP address manually or via QR pairing
5. Press Start to begin streaming

PC:

1. Launch the receiver application
2. Select desired audio output device and backend (WASAPI/ASIO)
3. Start listening for incoming stream

Performance Tips:

* Use 5 GHz Wi-Fi or Wi-Fi Direct for best latency
* Enable real-time thread priorities (especially for C++ version)
* Use exclusive audio modes (WASAPI or ASIO)

Troubleshooting:

* No audio on PC? Check firewall and ensure matching IP.
* Lag or stutter? Reduce codec complexity, buffer size, or switch Wi-Fi channel.
* High CPU usage? Disable RNNoise and use built-in suppressor.

Contributors:

* Android Developer: Ken L
* PC Developer: Ken L
* Audio Consultant: Ken L

License:
MIT License

Acknowledgements:

* Google Oboe Library
* Xiph.org Opus Codec
* PortAudio Development Team
* RNNoise by Xiph.org
