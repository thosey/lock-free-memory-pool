# LockFreeMemoryPool Examples

This directory contains a focused example program demonstrating how to use the LockFreeMemoryPool library.

## Example

### basic_usage.cpp

A comprehensive example showing:

- **Basic allocation patterns** - Safe vs Fast allocation APIs
- **Thread safety** - Multiple threads allocating concurrently  
- **Pool exhaustion handling** - What happens when pool is full
- **Exception safety** - How the pool handles constructor failures
- **RAII integration** - Automatic cleanup with smart pointers

This single example covers all the essential concepts you need to understand the library.

## Building Examples

### From the main project directory:

```bash
# Configure with examples
cmake -B build -S . -DBUILD_EXAMPLES=ON

# Build everything including examples  
cmake --build build

# Run the basic usage example
./build/examples/basic_usage
```

### Or build examples independently:

```bash
cd examples
mkdir build && cd build
cmake ..
make
./basic_usage
```

## Example Output

The basic usage example demonstrates:

1. **Safe Allocation (RAII)**:
   ```cpp
   auto foo = lockfree_pool_alloc_safe<Foo>(1, "Alice", 3.14);
   // Automatic cleanup when 'foo' goes out of scope
   ```

2. **Fast Allocation (Manual)**:
   ```cpp
   Foo* foo = lockfree_pool_alloc_fast<Foo>(1, "Alice", 3.14);
   // Must manually call: lockfree_pool_free_fast(foo);
   ```

3. **Thread Safety**:
   - Multiple threads allocating simultaneously
   - No locks, no blocking, no data races

4. **Pool Management**:
   - Graceful handling of pool exhaustion
   - Slot reuse after deallocation

5. **Exception Safety**:
   - Constructor failures don't leak pool slots
   - Strong exception safety guarantees

## Key Takeaways

- **Use `allocate_safe()` by default** - RAII with automatic cleanup
- **Use `allocate_fast()` for hot paths** - Manual management for maximum performance
- **Pool exhaustion returns nullptr** - Always check return values
- **Thread-safe by design** - No additional synchronization needed
- **Exception-safe** - Failed constructors don't permanently consume slots

## Testing and Benchmarks

The library includes comprehensive testing and performance validation:

### Running Tests

```bash
# Build with tests (included by default)
cmake -B build -S .
cmake --build build

# Run the full test suite
cd build && ctest

# Or run tests directly
./build/test/lockfree_mempool_tests
```

The test suite covers:
- **Thread safety** - Concurrent allocation/deallocation scenarios
- **Exception safety** - Constructor failure handling
- **Pool exhaustion** - Graceful handling of resource limits
- **Memory corruption detection** - Bounds checking and invalid pointer detection
- **Edge cases** - Null pointer handling, double-free protection

### Running Benchmarks

```bash
# Build with Google Benchmark
cmake -B build -S . -DBUILD_BENCHMARKS=ON
cmake --build build

# Run performance benchmarks
./build/benchmarks/google_benchmark
```

The benchmarks measure:
- **Single-threaded performance** - Pool vs heap allocation speed
- **Multi-threaded scalability** - Performance under contention
- **Fragmentation resistance** - Consistent performance over time
- **Mixed workload patterns** - Real-world allocation scenarios

### Expected Test Results

- **All tests should pass** - 13 test cases covering core functionality
- **No memory leaks** - Validated with tools like Valgrind
- **Deterministic performance** - Consistent timing across runs
- **Thread safety validation** - No data races or corruption under stress

## Performance Notes

The example includes timing demonstrations showing:
- **Lock-free allocation** - No thread blocking
- **O(1) average allocation** - Thanks to search hint optimization  
- **Cache-friendly design** - Aligned structures prevent false sharing
- **Minimal overhead** - Direct memory reuse without heap allocation

### Typical Performance Results

Based on current benchmarks (single-threaded workloads):

```
Pool Allocation (Fast):   ~21-37ns per operation  (2-3x faster than heap)
Pool Allocation (Safe):   ~22-37ns per operation  (2-3x faster than heap)  
Heap Allocation (new/delete): ~64-120ns per operation
```

**Key performance insights:**
- **Pool significantly outperforms heap** - 2-3x faster allocation
- **Both pool APIs perform similarly** - safety doesn't cost performance
- **Performance scales well** - consistent advantage across different object counts
- **Lock-free design** - No blocking in multi-threaded scenarios
