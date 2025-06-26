# Program Flowchart: Android-to-PC Audio Streaming App

## Overview
This flowchart illustrates the end-to-end workflow of the Android-to-PC audio streaming application, based on the provided PRD. It includes key stages such as audio capture, encoding, transmission, reception, decoding, and playback.

## Flowchart
```mermaid
graph TD
    A[Start] --> B[Select Audio Source: System or Microphone]
    B --> C[Audio Capture]
    C --> D["Audio Encoding: Select Codec (Opus, PCM, etc.)"]
    D --> E[UDP Transmission]
    E --> F[PC Reception]
    F --> G[Audio Decoding]
    G --> H[Playback via Output Device]
    H --> I[End]