cmake_minimum_required(VERSION 3.18.1)

project("audio-capture-native")

# Define the library
add_library(
    native-lib
    SHARED
    native-lib.cpp)

# Find and link Oboe
find_library(log-lib log)
find_package(oboe REQUIRED CONFIG)

target_link_libraries(native-lib
    ${log-lib}
    oboe::oboe
    android)