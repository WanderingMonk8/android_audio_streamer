cmake_minimum_required(VERSION 3.16)
project(AudioReceiver VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific settings
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

# Try to find Opus library
if(PkgConfig_FOUND)
    pkg_check_modules(OPUS opus)
    if(OPUS_FOUND)
        add_definitions(-DHAVE_LIBOPUS)
        message(STATUS "Found libopus: ${OPUS_VERSION}")
    else()
        message(STATUS "libopus not found, using mock implementation")
    endif()
else()
    message(STATUS "PkgConfig not found, using mock implementation for Opus")
endif()

# Try to find PortAudio library
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        add_definitions(-DHAVE_PORTAUDIO)
        message(STATUS "Found PortAudio: ${PORTAUDIO_VERSION}")
    else()
        message(STATUS "PortAudio not found, using mock implementation")
    endif()
else()
    message(STATUS "PkgConfig not found, using mock implementation for PortAudio")
endif()

# Include directories
include_directories(src)
if(OPUS_FOUND)
    include_directories(${OPUS_INCLUDE_DIRS})
endif()
if(PORTAUDIO_FOUND)
    include_directories(${PORTAUDIO_INCLUDE_DIRS})
endif()

# Source files
set(NETWORK_SOURCES
    src/network/packet.cpp
    src/network/udp_receiver.cpp
    src/network/qos_manager.cpp
    src/network/network_monitor.cpp
    src/network/fec_encoder.cpp
    src/network/fec_decoder.cpp
)

set(AUDIO_SOURCES
    src/audio/mock_opus.cpp
    src/audio/opus_decoder.cpp
    src/audio/real_opus_decoder.cpp
    src/audio/mock_audio_output.cpp
    src/audio/audio_output.cpp
    src/audio/real_audio_output.cpp
    src/audio/jitter_buffer.cpp
    src/audio/adaptive_jitter_buffer.cpp
    src/audio/audio_pipeline.cpp
)

# Create network library
add_library(network ${NETWORK_SOURCES})
target_link_libraries(network Threads::Threads)

if(WIN32)
    target_link_libraries(network ws2_32)
endif()

# Create audio library
add_library(audio ${AUDIO_SOURCES})
target_link_libraries(audio network)  # Link with network library for NetworkMonitor
if(OPUS_FOUND)
    target_link_libraries(audio ${OPUS_LIBRARIES})
    target_compile_options(audio PRIVATE ${OPUS_CFLAGS_OTHER})
endif()
if(PORTAUDIO_FOUND)
    target_link_libraries(audio ${PORTAUDIO_LIBRARIES})
    target_compile_options(audio PRIVATE ${PORTAUDIO_CFLAGS_OTHER})
endif()

# Main executable
add_executable(audio_receiver src/main.cpp)
target_link_libraries(audio_receiver network audio)

# Audio playback test executable
add_executable(audio_playback_test src/audio_playback_test.cpp)
target_link_libraries(audio_playback_test audio)

# FEC-only test executable
add_executable(test_fec_only src/test_fec_only.cpp tests/network/test_fec.cpp)
target_link_libraries(test_fec_only network)

# Debug FEC header executable
add_executable(debug_fec_header src/debug_fec_header.cpp)
target_link_libraries(debug_fec_header network)

# Enable testing
enable_testing()

# Test executable
add_executable(network_tests
    tests/network/test_packet.cpp
    tests/network/test_udp_receiver.cpp
    tests/network/test_qos_manager.cpp
    tests/network/test_network_monitor.cpp
    tests/network/test_fec.cpp
    tests/test_main.cpp
)

target_link_libraries(network_tests network)

# Audio test executable
add_executable(audio_tests
    tests/audio/test_opus_decoder.cpp
    tests/audio/test_real_opus_decoder.cpp
    tests/audio/test_audio_output.cpp
    tests/audio/test_real_audio_output.cpp
    tests/audio/test_jitter_buffer.cpp
    tests/audio/test_adaptive_jitter_buffer.cpp
    tests/audio/test_audio_pipeline.cpp
    tests/audio_test_main.cpp
)

target_link_libraries(audio_tests audio)

# Add tests
add_test(NAME NetworkTests COMMAND network_tests)
add_test(NAME AudioTests COMMAND audio_tests)

# Compiler-specific options
if(MSVC)
    target_compile_options(audio_receiver PRIVATE /W4)
    target_compile_options(network PRIVATE /W4)
    target_compile_options(audio PRIVATE /W4)
    target_compile_options(network_tests PRIVATE /W4)
    target_compile_options(audio_tests PRIVATE /W4)
else()
    target_compile_options(audio_receiver PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(network PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(audio PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(network_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(audio_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()