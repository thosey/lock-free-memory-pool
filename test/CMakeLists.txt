# Test CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # If GTest is not found, fetch it from GitHub
    include(FetchContent)
    
    message(STATUS "Google Test not found, fetching from GitHub...")
    
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0  # Use a stable release
        GIT_SHALLOW    TRUE
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
    
    # Create aliases to match find_package target names
    add_library(GTest::gtest ALIAS gtest)
    add_library(GTest::gtest_main ALIAS gtest_main)
    add_library(GTest::gmock ALIAS gmock)
    add_library(GTest::gmock_main ALIAS gmock_main)
endif()

# Create test executable
add_executable(lockfree_mempool_tests
    main.cpp
    testPool.cpp
)

# Link against the library and Google Test
target_link_libraries(lockfree_mempool_tests
    PRIVATE
    LockFreeMemoryPool
    GTest::gtest
    Threads::Threads
)

# Include directories
target_include_directories(lockfree_mempool_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Set compiler-specific options for tests
if(MSVC)
    target_compile_options(lockfree_mempool_tests PRIVATE /W4)
    # Disable some warnings for tests
    target_compile_options(lockfree_mempool_tests PRIVATE /wd4996)  # Disable deprecated warnings
else()
    target_compile_options(lockfree_mempool_tests PRIVATE -Wall -Wextra -Wpedantic)
    # Allow unused parameters in tests (common with Google Test)
    target_compile_options(lockfree_mempool_tests PRIVATE -Wno-unused-parameter)
endif()

# Add the test to CTest
add_test(
    NAME LockFreeMemoryPoolTests
    COMMAND lockfree_mempool_tests
)

# Set test properties
set_tests_properties(LockFreeMemoryPoolTests PROPERTIES
    TIMEOUT 60  # 60 seconds timeout
    LABELS "unit"
)

# Optional: Add custom target for running tests with verbose output
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS lockfree_mempool_tests
    COMMENT "Running all tests with verbose output"
)
