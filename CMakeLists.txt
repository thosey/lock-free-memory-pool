cmake_minimum_required(VERSION 3.16)
project(LockFreeMemoryPool VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Build options
option(BUILD_EXAMPLES "Build example programs" OFF)
option(BUILD_DOCUMENTATION "Build API documentation with Doxygen" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer for lock-free code validation" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection" OFF)

# Compiler-specific options
if(MSVC)
    # MSVC specific flags
    add_compile_options(/W4)
    # Don't manually add /O2 - let VS handle optimization flags automatically
else()
    # GCC/Clang flags
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
    
    # Sanitizer support for debugging lock-free code
    if(ENABLE_TSAN)
        add_compile_options(-fsanitize=thread -g)
        add_link_options(-fsanitize=thread)
        message(STATUS "ThreadSanitizer enabled for lock-free code validation")
    elseif(ENABLE_ASAN)
        add_compile_options(-fsanitize=address -g)
        add_link_options(-fsanitize=address)
        message(STATUS "AddressSanitizer enabled for memory error detection")
    endif()
endif()

# Header-only library target
add_library(LockFreeMemoryPool INTERFACE)

# Specify include directories for the library
target_include_directories(LockFreeMemoryPool INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Require C++20
target_compile_features(LockFreeMemoryPool INTERFACE cxx_std_20)

# Find required system libraries
find_package(Threads REQUIRED)
target_link_libraries(LockFreeMemoryPool INTERFACE Threads::Threads)

# Option to build tests
option(BUILD_TESTS "Build tests" ON)

# Option to build examples
option(BUILD_EXAMPLES "Build examples" OFF)

# Option to build benchmarks
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Add examples subdirectory if requested
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Add benchmarks subdirectory if requested
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Add documentation if requested
if(BUILD_DOCUMENTATION)
    add_subdirectory(docs)
endif()

# Installation rules (optional)
include(GNUInstallDirs)

install(TARGETS LockFreeMemoryPool
    EXPORT LockFreeMemoryPoolTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES src/LockFreeMemoryPool.h
    src/LockFreeMemoryPoolStats.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export configuration
install(EXPORT LockFreeMemoryPoolTargets
    FILE LockFreeMemoryPoolTargets.cmake
    NAMESPACE LockFreeMemoryPool::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LockFreeMemoryPool
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    LockFreeMemoryPoolConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LockFreeMemoryPoolConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/LockFreeMemoryPoolConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LockFreeMemoryPool
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/LockFreeMemoryPoolConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/LockFreeMemoryPoolConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LockFreeMemoryPool
)
