cmake_minimum_required(VERSION 3.18.1)

# Define the library
add_library(native-lib SHARED
    native-lib.cpp)

# Find and link Oboe
find_library(log-lib log)
find_library(oboe-lib oboe)

target_link_libraries(native-lib
    ${log-lib}
    ${oboe-lib})

# Include Oboe headers
target_include_directories(native-lib PRIVATE
    ${CMAKE_SOURCE_DIR}/src/main/cpp)