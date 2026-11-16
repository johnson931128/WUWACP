
cmake_minimum_required(VERSION 3.20)

project(WUWACP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emulator core library
add_library(wuwacp_core
    src/cpu/hart.cpp
)

target_include_directories(wuwacp_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Main executable
add_executable(wuwacp
    src/main.cpp
)

target_link_libraries(wuwacp
    PRIVATE wuwacp_core
)

# Unit tests
enable_testing()

add_executable(cpu_state_test
    tests/cpu_state_test.cpp
)

target_link_libraries(cpu_state_test
    PRIVATE wuwacp_core
)

add_test(
    NAME cpu_state
    COMMAND cpu_state_test
)
