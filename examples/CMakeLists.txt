cmake_minimum_required(VERSION 3.16)
project(LockFreeMemoryPoolExamples)

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

# Basic usage example
add_executable(basic_usage basic_usage.cpp)

# Link pthread for threading support on Unix systems
if(UNIX)
    target_link_libraries(basic_usage pthread)
endif()

# Create a target to run the examples
add_custom_target(run_examples 
    COMMAND basic_usage
    DEPENDS basic_usage
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running LockFreeMemoryPool examples"
)
