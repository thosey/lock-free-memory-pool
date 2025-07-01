# Lock Free Memory Pool

A high-performance, thread-safe, lock-free memory pool implementation in C++20.

## Features

- **Lock-free allocation/deallocation** - Uses atomic operations with proper memory ordering for thread safety
- **RAII support** - Automatic memory management with custom deleters and smart pointer integration
- **Global pool management** - Easy-to-use macros for defining type-specific pools
- **Header-only library** - Easy integration, just include the header
- **Cross-platform** - Works on Windows, Linux, and macOS  
- **Exception safe** - Proper cleanup even when constructors throw
- **Performance optimized** - Cache-line alignment, search hints, and memory ordering optimizations

## Quick Start

### 1. Include the header
```cpp
#include "LockFreeMemoryPool.h"
using namespace lfmemorypool;
```

### 2. Define global pools for your types
```cpp
// Define pools for your types (must be at global scope, not inside namespaces)
DEFINE_LOCKFREE_POOL(MyClass, 10000);  // Pool of 10,000 MyClass objects
DEFINE_LOCKFREE_POOL(AnotherType, 5000);
```

**Important:** The `DEFINE_LOCKFREE_POOL` macro must be invoked at global scope (outside of any namespace). However, the types themselves can be in namespaces:

```cpp
namespace myapp {
    struct MyType { /* ... */ };
}

// Macro must be at global scope
DEFINE_LOCKFREE_POOL(myapp::MyType, 1000);

namespace myapp {
    void use_pool() {
        // Usage code can be inside namespaces
        auto obj = lfmemorypool::lockfree_pool_alloc_safe<MyType>();
    }
}
```

### 3. Use the pools
```cpp
// Safe allocation with RAII (recommended)
auto obj1 = lockfree_pool_alloc_safe<MyClass>(constructor_args...);
// obj1 is automatically cleaned up when it goes out of scope

// Fast allocation for performance-critical paths
MyClass* obj2 = lockfree_pool_alloc_fast<MyClass>(constructor_args...);
// Must manually call lockfree_pool_free_fast(obj2) when done
lockfree_pool_free_fast(obj2);
```

## Building

### Prerequisites

- **C++20 or later** compiler (GCC 10+, Clang 10+, MSVC 2019 16.11+)
- **CMake 3.16+**
- **Google Test** (automatically downloaded if not found)

### Build Instructions

#### Linux/macOS:
```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Run tests
ctest --verbose
# or
make run_tests

# Optional: Build and run examples
cmake .. -DBUILD_EXAMPLES=ON
make
./examples/basic_usage
```

#### Windows (PowerShell):
```powershell
# Create build directory
mkdir build
cd build

# Configure (Visual Studio)
cmake ..

# Build
cmake --build . --config Release

# Run tests
ctest --verbose -C Release
```

### Build Options

- `BUILD_TESTS=ON/OFF` - Build tests (default: ON)
- `BUILD_EXAMPLES=ON/OFF` - Build usage examples (default: OFF)
- `BUILD_BENCHMARKS=ON/OFF` - Build performance benchmarks (default: OFF)
- `ENABLE_TSAN=ON/OFF` - Enable ThreadSanitizer for lock-free validation (default: OFF)
- `ENABLE_ASAN=ON/OFF` - Enable AddressSanitizer for memory error detection (default: OFF)
- `CMAKE_BUILD_TYPE=Debug/Release` - Build configuration (default: Release)

Example:
```bash
cmake -DBUILD_EXAMPLES=ON -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release ..
```

### Testing with ThreadSanitizer

**Important**: ThreadSanitizer is essential for validating lock-free code across different CPU architectures. While the code works on Intel x86/x64 with strong memory ordering, TSan helps catch potential issues on ARM, RISC-V, and other architectures with weaker memory models.

```bash
# Build with ThreadSanitizer enabled
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..
make -j$(nproc)

# Run tests with TSan - will detect data races and memory ordering issues
./build/test/lockfree_mempool_tests

# For thorough testing, also run concurrent tests multiple times
for i in {1..10}; do ./build/test/lockfree_mempool_tests --gtest_filter="*Concurrent*"; done
```

**Note**: ThreadSanitizer has significant performance overhead and should only be used during development and testing, not in production builds.

## API Reference

### Individual Pool Usage
```cpp
// Create a pool
LockFreeMemoryPool<MyType> pool(1000);

// Safe allocation with RAII
auto obj = pool.allocate_safe(constructor_args...);

// Fast allocation
MyType* ptr = pool.allocate_fast(constructor_args...);
pool.deallocate_fast(ptr);

// Get statistics
auto stats = lfmemorypool::stats::get_pool_stats(pool);
std::cout << "Pool utilization: " << stats.utilization_percent << "%" << std::endl;
```

### Global Pool Functions
```cpp
// After defining pools with DEFINE_LOCKFREE_POOL
auto obj = lockfree_pool_alloc_safe<MyType>(args...);
MyType* ptr = lockfree_pool_alloc_fast<MyType>(args...);
lockfree_pool_free_fast(ptr);
auto stats = lfmemorypool::stats::lockfree_pool_stats<MyType>();
```

## Pool Statistics

The library provides comprehensive pool monitoring through the `lfmemorypool::stats` namespace. Include `LockFreeMemoryPoolStats.h` to enable statistics collection:

```cpp
#include "LockFreeMemoryPoolStats.h"

// Pool instance statistics
lfmemorypool::LockFreeMemoryPool<MyType> pool{1000};
auto stats = lfmemorypool::stats::get_pool_stats(pool);

// Global pool statistics
auto global_stats = lfmemorypool::stats::lockfree_pool_stats<MyType>();

// Statistics structure
struct PoolStats {
    size_t total_objects;        // Total pool capacity
    size_t free_objects;         // Available objects
    size_t used_objects;         // Currently allocated objects  
    double utilization_percent;  // Usage percentage (0-100)
};

std::cout << "Pool utilization: " << stats.utilization_percent << "%" << std::endl;
std::cout << "Free objects: " << stats.free_objects << "/" << stats.total_objects << std::endl;
```

## Performance Characteristics

- **O(n) allocation** in worst case, but typically O(1) with good hint system
- **O(1) deallocation**
- **Lock-free** - No blocking between threads
- **Memory efficient** - No fragmentation, predictable memory usage

## Thread Safety

The memory pool is fully thread-safe:
- Multiple threads can allocate simultaneously
- Multiple threads can deallocate simultaneously  
- Allocation and deallocation can happen concurrently
- Uses atomic operations with proper memory ordering

## Error Handling and Edge Cases

### Pool Exhaustion
When the pool is exhausted (all objects are allocated):
- `allocate_safe()` returns `nullptr` (no exceptions thrown)
- `lockfree_pool_alloc_safe()` returns `nullptr` (no exceptions thrown)
- `allocate_fast()` returns `nullptr` (no exceptions thrown)
- `lockfree_pool_alloc_fast()` returns `nullptr` (no exceptions thrown)

```cpp
// Safe handling of pool exhaustion
auto obj = lockfree_pool_alloc_safe<MyType>();
if (!obj) {
    // Pool is exhausted - handle gracefully
    std::cerr << "Pool exhausted, falling back to heap allocation\n";
    auto heap_obj = std::make_unique<MyType>();
    // ... continue with heap_obj
}
```

### Constructor Exceptions
If object construction throws an exception:
- Memory is automatically returned to the pool
- Exception is propagated to the caller
- Pool remains in a consistent state

```cpp
struct ThrowingType {
    ThrowingType() { throw std::runtime_error("Construction failed"); }
};

try {
    auto obj = lockfree_pool_alloc_safe<ThrowingType>();
    // obj will be nullptr - exception was caught internally
} catch (const std::exception& e) {
    // This won't be reached for safe allocation
    // Fast allocation would propagate the exception
}
```

### Invalid Pointer Handling
- `deallocate_fast()` and `lockfree_pool_free_fast()` are safe with `nullptr`
- **Performance Note**: For maximum speed, the library does not validate that pointers belong to the pool
- Passing invalid pointers results in undefined behavior - users must ensure correct usage
- Debug builds may catch some invalid usage through assertions

```cpp
// Safe usage patterns
MyType* obj = lockfree_pool_alloc_fast<MyType>();
if (obj) {
    // ... use obj
    lockfree_pool_free_fast(obj);  // Safe
}

lockfree_pool_free_fast(nullptr);  // Safe - no-op

// INCORRECT - undefined behavior:
// MyType external_obj;
// lockfree_pool_free_fast(&external_obj);  // DON'T DO THIS!
```

### Memory Ordering Guarantees
- All atomic operations use appropriate memory ordering
- `acquire-release` semantics for allocation/deallocation synchronization
- `relaxed` ordering for performance hints (search start position)
- See code comments for detailed memory ordering rationale

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

## Performance Benchmarking

This project includes comprehensive performance benchmarks using **Google Benchmark**, a popular C++ benchmarking framework. The benchmarks provide detailed performance measurements comparing heap allocation to pool allocation.

### Quick Performance Test
```bash
# Install Google Benchmark (Ubuntu/Debian)
sudo apt install libbenchmark-dev

# Build and run
cmake -B build -DBUILD_BENCHMARKS=ON
cmake --build build
make -C build run_google_benchmark
```

The Google Benchmark suite includes:
- **Allocation Benchmarks**: Heap vs Pool (safe/fast) across different object counts
- **Multi-threaded Benchmarks**: Concurrent allocation performance
- **Fragmentation Tests**: Realistic allocation/deallocation patterns
- **Mixed Workloads**: Combined allocation patterns

For installation and detailed usage, see [benchmarks/README.md](benchmarks/README.md).

## Examples

See the [`examples/`](examples/) directory for comprehensive usage demonstrations:

- **[basic_usage.cpp](examples/basic_usage.cpp)** - Complete example showing safe/fast allocation, thread safety, pool exhaustion handling, and exception safety
- **[Examples README](examples/README.md)** - Detailed explanation of all example programs

Quick example run:
```bash
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build
./build/examples/basic_usage
```
