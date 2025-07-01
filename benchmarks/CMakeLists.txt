cmake_minimum_required(VERSION 3.16)
project(LockFreeMemoryPoolBenchmarks)

# Use the same C++ standard as the main project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options (same as main project)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

# Try to find Google Benchmark
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(benchmark QUIET benchmark)
endif()

# Fallback to find_package if pkg-config didn't work
if(NOT benchmark_FOUND)
    find_package(benchmark QUIET)
endif()

# Google Benchmark performance tests (if available)
if(benchmark_FOUND)
    add_executable(google_benchmark google_benchmark.cpp)
    
    # Link Google Benchmark
    if(TARGET benchmark::benchmark)
        target_link_libraries(google_benchmark benchmark::benchmark)
    else()
        target_link_libraries(google_benchmark ${benchmark_LIBRARIES})
        target_include_directories(google_benchmark PRIVATE ${benchmark_INCLUDE_DIRS})
    endif()
    
    # Link pthread for threading support on Unix systems
    if(UNIX)
        target_link_libraries(google_benchmark pthread)
    endif()
    
    message(STATUS "Google Benchmark found - google_benchmark target available")
    
    # Create a target to run Google Benchmark
    add_custom_target(run_google_benchmark
        COMMAND google_benchmark --benchmark_format=console
        DEPENDS google_benchmark
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running Google Benchmark performance tests"
    )
    
    # Create a comprehensive benchmark target with JSON output
    add_custom_target(benchmark_report
        COMMAND google_benchmark --benchmark_format=json --benchmark_out=benchmark_results.json
        COMMAND echo "Benchmark results saved to benchmark_results.json"
        DEPENDS google_benchmark
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running comprehensive benchmarks and saving JSON report"
    )
    
else()
    message(STATUS "Google Benchmark not found - install with: sudo apt-get install libbenchmark-dev")
    message(STATUS "Benchmarks will not be available")
    
    # Create a dummy target that explains how to install Google Benchmark
    add_custom_target(install_benchmark_help
        COMMAND echo "Google Benchmark is not installed."
        COMMAND echo "To install on Ubuntu/Debian: sudo apt-get install libbenchmark-dev"
        COMMAND echo "To install on macOS: brew install google-benchmark"
        COMMAND echo "After installation, reconfigure cmake to enable benchmarks."
        COMMENT "Google Benchmark installation help"
    )
endif()
