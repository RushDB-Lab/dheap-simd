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

option(DHEAP_FORCE_SCALAR "Force-disable SIMD path even if NEON is available" OFF)
set(DHEAP_SIMD_POLICY "HYBRID" CACHE STRING "SIMD policy: HYBRID, ALWAYS, NEVER")
set_property(CACHE DHEAP_SIMD_POLICY PROPERTY STRINGS HYBRID ALWAYS NEVER)
string(TOUPPER "${DHEAP_SIMD_POLICY}" DHEAP_SIMD_POLICY_UPPER)
if(DHEAP_SIMD_POLICY_UPPER STREQUAL "NEVER")
    set(DHEAP_SIMD_POLICY_VALUE 0)
elseif(DHEAP_SIMD_POLICY_UPPER STREQUAL "ALWAYS")
    set(DHEAP_SIMD_POLICY_VALUE 1)
elseif(DHEAP_SIMD_POLICY_UPPER STREQUAL "HYBRID")
    set(DHEAP_SIMD_POLICY_VALUE 2)
else()
    message(FATAL_ERROR "DHEAP_SIMD_POLICY must be one of: HYBRID, ALWAYS, NEVER")
endif()
if(DHEAP_FORCE_SCALAR)
    set(DHEAP_SIMD_POLICY_VALUE 0)
    message(STATUS "DHeap SIMD path: forced OFF by DHEAP_FORCE_SCALAR=ON")
else()
    message(STATUS "DHeap SIMD policy: ${DHEAP_SIMD_POLICY_UPPER}")
endif()

set(DHEAP_NODE_PAYLOAD_BYTES "0" CACHE STRING "Extra payload bytes per heap node (non-negative integer)")
if(NOT DHEAP_NODE_PAYLOAD_BYTES MATCHES "^[0-9]+$")
    message(FATAL_ERROR "DHEAP_NODE_PAYLOAD_BYTES must be a non-negative integer")
endif()
message(STATUS "DHeap node payload bytes: ${DHEAP_NODE_PAYLOAD_BYTES}")

set(DHEAP_ARITY "4" CACHE STRING "Heap arity d (integer >= 2)")
if(NOT DHEAP_ARITY MATCHES "^[0-9]+$")
    message(FATAL_ERROR "DHEAP_ARITY must be an integer >= 2")
endif()
if(DHEAP_ARITY LESS 2)
    message(FATAL_ERROR "DHEAP_ARITY must be >= 2")
endif()
message(STATUS "DHeap arity (d): ${DHEAP_ARITY}")

set(DHEAP_SIMD_BUILD_MIN_ARITY "8" CACHE STRING "HYBRID policy: minimum arity to enable SIMD in heapify path")
if(NOT DHEAP_SIMD_BUILD_MIN_ARITY MATCHES "^[0-9]+$")
    message(FATAL_ERROR "DHEAP_SIMD_BUILD_MIN_ARITY must be an integer >= 2")
endif()
if(DHEAP_SIMD_BUILD_MIN_ARITY LESS 2)
    message(FATAL_ERROR "DHEAP_SIMD_BUILD_MIN_ARITY must be >= 2")
endif()
message(STATUS "DHeap HYBRID: build-path SIMD min arity: ${DHEAP_SIMD_BUILD_MIN_ARITY}")

set(DHEAP_SIMD_POP_MIN_SIZE "4194304" CACHE STRING "HYBRID policy: minimum heap size to enable SIMD on pop root step")
if(NOT DHEAP_SIMD_POP_MIN_SIZE MATCHES "^[0-9]+$")
    message(FATAL_ERROR "DHEAP_SIMD_POP_MIN_SIZE must be a non-negative integer")
endif()
message(STATUS "DHeap HYBRID: pop-path SIMD min heap size: ${DHEAP_SIMD_POP_MIN_SIZE}")

# Include directory
include_directories(${PROJECT_SOURCE_DIR}/include)

# Library
add_library(dheap4_simd STATIC src/dheap4_simd.cpp)
target_compile_definitions(
    dheap4_simd
    PUBLIC
        DHEAP_NODE_PAYLOAD_BYTES=${DHEAP_NODE_PAYLOAD_BYTES}
        DHEAP_ARITY=${DHEAP_ARITY}
        DHEAP_SIMD_POLICY=${DHEAP_SIMD_POLICY_VALUE}
        DHEAP_SIMD_BUILD_MIN_ARITY=${DHEAP_SIMD_BUILD_MIN_ARITY}
        DHEAP_SIMD_POP_MIN_SIZE=${DHEAP_SIMD_POP_MIN_SIZE}
)
if(DHEAP_SIMD_POLICY_VALUE EQUAL 0)
    target_compile_definitions(dheap4_simd PUBLIC DHEAP4_SIMD_ENABLED=0)
endif()

# Test executable
add_executable(test_dheap4 tests/test_dheap4.cpp)
target_link_libraries(test_dheap4 dheap4_simd)

# Benchmark executable
add_executable(bench_dheap4 bench/bench_dheap4.cpp)
target_link_libraries(bench_dheap4 dheap4_simd)

# Enable testing
enable_testing()
add_test(NAME dheap4_tests COMMAND test_dheap4)
