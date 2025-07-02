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

# Include directories
include_directories(src)

# Source files
set(NETWORK_SOURCES
    src/network/packet.cpp
    src/network/udp_receiver.cpp
)

set(AUDIO_SOURCES
    src/audio/opus_decoder.cpp
    src/audio/audio_output.cpp
    src/audio/jitter_buffer.cpp
)

# Create network library
add_library(network ${NETWORK_SOURCES})
target_link_libraries(network Threads::Threads)

if(WIN32)
    target_link_libraries(network ws2_32)
endif()

# Create audio library
add_library(audio ${AUDIO_SOURCES})

# Main executable
add_executable(audio_receiver src/main.cpp)
target_link_libraries(audio_receiver network audio)

# Enable testing
enable_testing()

# Test executable
add_executable(network_tests
    tests/network/test_packet.cpp
    tests/network/test_udp_receiver.cpp
    tests/test_main.cpp
)

target_link_libraries(network_tests network)

# Audio test executable
add_executable(audio_tests
    tests/audio/test_opus_decoder.cpp
    tests/audio/test_audio_output.cpp
    tests/audio/test_jitter_buffer.cpp
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