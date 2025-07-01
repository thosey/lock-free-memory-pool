/**
 * @file basic_usage.cpp
 * @brief Basic usage examples for LockFreeMemoryPool
 * @defgroup examples Examples
 * @brief LockFreeMemoryPool usage examples and demonstrations
 * @{
 */

#include <chrono>
#include <cstdlib>
#include <cstring>  // For std::memset
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>
#include "../src/LockFreeMemoryPool.h"

using namespace lfmemorypool;

namespace examples {
/**
 * @brief Example type for demonstration purposes
 * @details This struct represents a realistic object with fixed-size data that would benefit from
 * pool allocation
 * @ingroup examples
 */
struct Foo {
    static constexpr size_t DATA_SIZE = 64;  // Fixed-size array for realistic object size

    int id;
    std::string name;
    double value;
    char data[DATA_SIZE];  // Fixed-size array - all memory comes from the pool

    Foo() : id(0), name("foo"), value(0.0) {
        std::memset(data, 0, sizeof(data));
        std::cout << "Foo() default constructor\n";
    }

    Foo(int i, const std::string& n, double v) : id(i), name(n), value(v) {
        std::memset(data, i % 256, sizeof(data));  // Fill with some pattern
        std::cout << "Foo(" << id << ", " << name << ", " << value << ") constructed\n";
    }

    ~Foo() {
        std::cout << "Foo(" << id << ", " << name << ") destroyed\n";
    }

    void do_work() {
        std::cout << "Foo " << id << " (" << name << ") doing work with value " << value << "\n";
        // Access the data array to show it's being used
        data[0] = static_cast<char>(id % 256);
    }

    // Method to demonstrate that this is a realistic, substantial object
    void process_data() {
        // Simulate some work with the fixed-size data
        for (size_t i = 0; i < DATA_SIZE; ++i) {
            data[i] = static_cast<char>((data[i] + id) % 256);
        }
        value += 1.0;
        std::cout << "Foo " << id << " processed data, new value: " << value << "\n";
    }

    size_t calculate_checksum() const {
        size_t sum = 0;
        for (size_t i = 0; i < DATA_SIZE; ++i) {
            sum += static_cast<unsigned char>(data[i]);
        }
        return sum + static_cast<size_t>(id) + static_cast<size_t>(value);
    }
};

// A simpler type for different use cases
struct SimpleCounter {
    int count;
    char padding[60];  // Make it a reasonable size for pooling

    SimpleCounter() : count(0) {
        std::memset(padding, 0, sizeof(padding));
        std::cout << "SimpleCounter() created\n";
    }

    explicit SimpleCounter(int initial) : count(initial) {
        std::memset(padding, initial % 256, sizeof(padding));
        std::cout << "SimpleCounter(" << initial << ") created\n";
    }

    ~SimpleCounter() {
        std::cout << "SimpleCounter(" << count << ") destroyed\n";
    }

    void increment() {
        count++;
        std::cout << "Counter incremented to " << count << "\n";
    }

    void add(int value) {
        count += value;
        std::cout << "Counter increased by " << value << " to " << count << "\n";
    }
};

}  // namespace examples
// Define global pools for our example types (must be at global scope)
DEFINE_LOCKFREE_POOL(examples::Foo, 1000);           // Pool of 1000 Foo objects
DEFINE_LOCKFREE_POOL(examples::SimpleCounter, 500);  // Pool of 500 SimpleCounter objects

namespace examples {
void demonstrateBasicUsage() {
    std::cout << "\n=== Basic Usage Demo ===\n";

    // Safe allocation with RAII (recommended for most use cases)
    std::cout << "\n--- Safe Allocation (RAII) ---\n";
    {
        auto foo1 = lockfree_pool_alloc_safe<Foo>(1, "Alice", 3.14);
        auto foo2 = lockfree_pool_alloc_safe<Foo>(2, "Bob", 2.71);

        if (foo1 && foo2) {
            foo1->do_work();
            foo2->do_work();
            foo1->process_data();
            std::cout << "Foo1 checksum: " << foo1->calculate_checksum() << "\n";
        }

        auto counter1 = lockfree_pool_alloc_safe<SimpleCounter>(20);
        if (counter1) {
            counter1->increment();
            counter1->add(5);
        }

        std::cout << "Objects will be automatically cleaned up when going out of scope...\n";
    }
    std::cout << "Scope exited - objects destroyed and returned to pool\n";

    // Fast allocation for performance-critical paths
    std::cout << "\n--- Fast Allocation (Manual Management) ---\n";
    Foo* foo3 = lockfree_pool_alloc_fast<Foo>(3, "Charlie", 1.41);
    SimpleCounter* counter2 = lockfree_pool_alloc_fast<SimpleCounter>(15);

    if (foo3) {
        foo3->do_work();
        foo3->process_data();
        lockfree_pool_free_fast(foo3);  // Must manually free
    }

    if (counter2) {
        counter2->increment();
        counter2->add(10);
        lockfree_pool_free_fast(counter2);  // Must manually free
    }
}

void demonstrateThreadSafety() {
    std::cout << "\n=== Thread Safety Demo ===\n";

    constexpr int NUM_THREADS = 4;
    constexpr int ALLOCATIONS_PER_THREAD = 10;

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);

            for (int i = 0; i < ALLOCATIONS_PER_THREAD; ++i) {
                // Mix of safe and fast allocations
                if (i % 2 == 0) {
                    // Safe allocation
                    auto foo = lockfree_pool_alloc_safe<Foo>(
                        t * 100 + i, "Thread" + std::to_string(t), dis(gen) / 10.0);
                    if (foo) {
                        foo->do_work();
                        // Automatic cleanup when foo goes out of scope
                    }
                } else {
                    // Fast allocation
                    Foo* foo = lockfree_pool_alloc_fast<Foo>(
                        t * 100 + i, "FastThread" + std::to_string(t), dis(gen) / 10.0);
                    if (foo) {
                        foo->do_work();
                        lockfree_pool_free_fast(foo);
                    }
                }

                // Small delay to interleave operations
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "All threads completed successfully!\n";
}

void demonstratePoolExhaustion() {
    std::cout << "\n=== Pool Exhaustion Demo ===\n";

    // Create a small local pool for demonstration
    LockFreeMemoryPool<Foo> smallPool(3);  // Only 3 slots

    std::cout << "Created pool with only 3 slots\n";

    // Allocate all slots
    auto foo1 = smallPool.allocate_safe(1, "First", 1.0);
    auto foo2 = smallPool.allocate_safe(2, "Second", 2.0);
    auto foo3 = smallPool.allocate_safe(3, "Third", 3.0);

    std::cout << "Allocated 3 objects successfully\n";

    // Try to allocate one more (should fail)
    auto foo4 = smallPool.allocate_safe(4, "Fourth", 4.0);
    if (!foo4) {
        std::cout << "Fourth allocation failed - pool exhausted (this is expected)\n";
    }

    // Free one slot and try again
    foo2.reset();  // Free the second object
    std::cout << "Freed one object\n";

    auto foo5 = smallPool.allocate_safe(5, "Fifth", 5.0);
    if (foo5) {
        std::cout << "Successfully allocated after freeing a slot\n";
        foo5->do_work();
    }
}

void demonstrateExceptionSafety() {
    std::cout << "\n=== Exception Safety Demo ===\n";

    // Type that throws in constructor sometimes
    struct ThrowingType {
        int value;

        ThrowingType(int v) : value(v) {
            if (v == 666) {  // Evil number throws
                throw std::runtime_error("Constructor failed!");
            }
            std::cout << "ThrowingType(" << v << ") constructed\n";
        }

        ~ThrowingType() {
            std::cout << "ThrowingType(" << value << ") destroyed\n";
        }
    };

    // Define pool for throwing type
    LockFreeMemoryPool<ThrowingType> throwingPool(10);

    // Successful allocation
    try {
        auto obj1 = throwingPool.allocate_safe(42);
        std::cout << "Successfully allocated ThrowingType(42)\n";
    } catch (const std::exception& e) {
        std::cout << "Unexpected exception: " << e.what() << "\n";
    }

    // Allocation that throws
    try {
        auto obj2 = throwingPool.allocate_safe(666);  // This will throw
        std::cout << "This should not print\n";
    } catch (const std::exception& e) {
        std::cout << "Expected exception caught: " << e.what() << "\n";
        std::cout << "Pool slot was automatically released due to exception safety\n";
    }

    // Verify pool is still usable
    try {
        auto obj3 = throwingPool.allocate_safe(123);
        std::cout << "Pool still works after exception - allocated ThrowingType(123)\n";
    } catch (const std::exception& e) {
        std::cout << "Unexpected exception: " << e.what() << "\n";
    }
}

void demonstratePoolVsHeapPerformance() {
    std::cout << "\n=== Pool vs Heap Performance Comparison ===\n";

    const int NUM_ITERATIONS = 1000;

    // Demonstrate pool allocation performance
    std::cout << "\n--- Pool Allocation Performance ---\n";
    auto start = std::chrono::high_resolution_clock::now();

    {
        std::vector<decltype(lockfree_pool_alloc_safe<Foo>(0, "", 0.0))> poolObjects;
        poolObjects.reserve(NUM_ITERATIONS);

        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            auto obj = lockfree_pool_alloc_safe<Foo>(i, "Pool" + std::to_string(i), i * 1.5);
            if (obj) {
                obj->process_data();  // Do some work
                poolObjects.push_back(std::move(obj));
            }
        }

        // Objects are automatically freed when vector goes out of scope
    }

    auto poolTime = std::chrono::high_resolution_clock::now() - start;

    // Demonstrate heap allocation performance
    std::cout << "\n--- Heap Allocation Performance ---\n";
    start = std::chrono::high_resolution_clock::now();

    {
        std::vector<std::unique_ptr<Foo>> heapObjects;
        heapObjects.reserve(NUM_ITERATIONS);

        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            auto obj = std::make_unique<Foo>(i, "Heap" + std::to_string(i), i * 1.5);
            obj->process_data();  // Do some work
            heapObjects.push_back(std::move(obj));
        }

        // Objects are automatically freed when vector goes out of scope
    }

    auto heapTime = std::chrono::high_resolution_clock::now() - start;

    // Report results
    auto poolMs = std::chrono::duration_cast<std::chrono::microseconds>(poolTime).count();
    auto heapMs = std::chrono::duration_cast<std::chrono::microseconds>(heapTime).count();

    std::cout << "\n--- Performance Results ---\n";
    std::cout << "Pool allocation: " << poolMs << " microseconds\n";
    std::cout << "Heap allocation: " << heapMs << " microseconds\n";

    if (poolMs < heapMs) {
        std::cout << "Pool allocation was " << (static_cast<double>(heapMs) / poolMs)
                  << "x faster than heap allocation!\n";
    } else {
        std::cout << "Heap allocation was " << (static_cast<double>(poolMs) / heapMs)
                  << "x faster than pool allocation.\n";
    }

    std::cout << "\nNote: Results may vary depending on system load and compiler optimizations.\n";
    std::cout << "For precise benchmarking, see the benchmarks/ directory.\n";
}

}  // namespace examples

int main() {
    std::cout << "LockFreeMemoryPool Example Usage\n";
    std::cout << "=============================\n";

    try {
        examples::demonstrateBasicUsage();
        examples::demonstrateThreadSafety();
        examples::demonstratePoolExhaustion();
        examples::demonstrateExceptionSafety();
        examples::demonstratePoolVsHeapPerformance();

        std::cout << "\n=== Example Complete ===\n";
        std::cout << "All demonstrations completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}


