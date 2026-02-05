cmake_minimum_required(VERSION 3.16)
project(dheap4_simd CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Enable Link-Time Optimization (IPO) for release builds when available.
include(CheckIPOSupported)
check_ipo_supported(RESULT DHEAP4_IPO_SUPPORTED OUTPUT DHEAP4_IPO_ERROR)
if(DHEAP4_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    message(STATUS "IPO/LTO not supported: ${DHEAP4_IPO_ERROR}")
endif()

# Enable NEON on ARM (default on Apple Silicon)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    message(STATUS "ARM64 detected, NEON enabled by default")
endif()

# Include directory
include_directories(${PROJECT_SOURCE_DIR}/include)

# Library
add_library(dheap4_simd STATIC src/dheap4_simd.cpp)

# Test executable
add_executable(test_dheap4 tests/test_dheap4.cpp)
target_link_libraries(test_dheap4 dheap4_simd)

# Benchmark executable
add_executable(bench_dheap4 bench/bench_dheap4.cpp)
target_link_libraries(bench_dheap4 dheap4_simd)

# Enable testing
enable_testing()
add_test(NAME dheap4_tests COMMAND test_dheap4)
