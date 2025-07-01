/**
 * @file google_benchmark.cpp
 * @brief Comprehensive performance benchmarks for LockFreeMemoryPool
 * @defgroup benchmarks Benchmarks
 * @brief Performance benchmarks and measurement utilities
 * @details This group contains comprehensive benchmarks comparing pool vs heap allocation,
 * fragmentation patterns, and multi-threaded performance characteristics.
 * Users can use these as templates for their own performance testing.
 * 
 * The benchmarks cover:
 * - Basic allocation/deallocation performance comparison
 * - Memory fragmentation impact measurement  
 * - Multi-threaded scalability testing
 * - Mixed allocation pattern analysis
 * - Real-world scenario simulations
 * @{
 */

#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <functional>
#include "../src/LockFreeMemoryPool.h"

using namespace lfmemorypool;

/**
 * @brief Test object for performance benchmarking
 * @details Represents a realistic object with mixed data types and reasonable size
 * for meaningful performance comparisons between heap and pool allocation.
 * @ingroup benchmarks
 */
struct TestObject {
    static constexpr size_t DATA_SIZE = 256;
    static constexpr size_t NUMBERS_SIZE = 20;
    
    int id;
    double value;
    char data[DATA_SIZE];
    std::string name;
    int numbers[NUMBERS_SIZE];
    
    TestObject() : id(0), value(0.0), name("default") {
    }
    
    TestObject(int i, double v, const std::string& n) 
        : id(i), value(v), name(n) {
    }
    
    ~TestObject() = default;
    
    // Method to prevent optimization from eliminating object usage
    int do_work() const {
        return id + static_cast<int>(value) + static_cast<int>(data[0]) + numbers[0];
    }
};

// Define global pools for our test objects
DEFINE_LOCKFREE_POOL(TestObject, 100000);

/**
 * @brief Allocation strategy interface for parameterized benchmarks
 * @details Defines a common interface for different allocation strategies,
 * ensuring fair comparison by using identical benchmark code paths.
 * @ingroup benchmarks
 */
struct AllocationStrategy {
    std::function<TestObject*(int, double, const std::string&)> allocate;
    std::function<void(TestObject*)> deallocate;
    std::string name;
    
    AllocationStrategy(
        std::function<TestObject*(int, double, const std::string&)> alloc_fn,
        std::function<void(TestObject*)> dealloc_fn,
        const std::string& strategy_name)
        : allocate(std::move(alloc_fn)), deallocate(std::move(dealloc_fn)), name(strategy_name) {}
};

/**
 * @brief Create allocation strategies for benchmarking
 * @details Factory function that returns all allocation strategies to be tested.
 * This ensures consistent testing across all allocation methods.
 * @ingroup benchmarks
 */
static std::vector<AllocationStrategy> CreateAllocationStrategies() {
    std::vector<AllocationStrategy> strategies;
    
    // Heap allocation strategy
    strategies.emplace_back(
        [](int id, double value, const std::string& name) -> TestObject* {
            return new TestObject(id, value, name);
        },
        [](TestObject* obj) {
            delete obj;
        },
        "Heap"
    );
    
    // Pool fast allocation strategy
    strategies.emplace_back(
        [](int id, double value, const std::string& name) -> TestObject* {
            return lockfree_pool_alloc_fast<TestObject>(id, value, name);
        },
        [](TestObject* obj) {
            lockfree_pool_free_fast(obj);
        },
        "PoolFast"
    );
    
    return strategies;
}

/**
 * @brief Parameterized allocation benchmark
 * @details Generic benchmark that works with any allocation strategy.
 * This ensures fair comparison by using identical code paths for all allocation methods.
 * @ingroup benchmarks
 */
static void BM_ParameterizedAllocation(benchmark::State& state, AllocationStrategy strategy) {
    const int num_objects = state.range(0);
    
    for (auto _ : state) {
        // Fill array with allocated objects
        std::vector<TestObject*> objects;
        objects.reserve(num_objects);
        
        for (int i = 0; i < num_objects; ++i) {
            TestObject* obj = strategy.allocate(i, i * 1.5, strategy.name + "_obj");
            if (obj) {
                objects.push_back(obj);
            }
        }
        
        // Do some work to prevent optimization
        int sum = 0;
        for (TestObject* obj : objects) {
            if (obj) {
                sum += obj->do_work();
            }
        }
        benchmark::DoNotOptimize(sum);
        
        // Clean up all objects
        for (TestObject* obj : objects) {
            if (obj) {
                strategy.deallocate(obj);
            }
        }
    }
    
    // Set the number of items processed per iteration
    state.SetItemsProcessed(state.iterations() * num_objects);
    
    // Set custom counters
    state.counters["objects_per_sec"] = benchmark::Counter(
        num_objects * state.iterations(), benchmark::Counter::kIsRate);
    state.counters["ns_per_object"] = benchmark::Counter(
        state.iterations() * num_objects, benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}

/**
 * @brief Parameterized fragmentation benchmark
 * @details Tests memory fragmentation impact with alternating allocation/deallocation patterns.
 * Uses the same code path for all allocation strategies to ensure fair comparison.
 * @ingroup benchmarks
 */
static void BM_ParameterizedFragmentation(benchmark::State& state, AllocationStrategy strategy) {
    const int objects_per_cycle = 50;
    const int cycles = state.range(0);
    
    for (auto _ : state) {
        std::vector<TestObject*> objects;
        objects.reserve(objects_per_cycle);
        
        for (int cycle = 0; cycle < cycles; ++cycle) {
            // Allocate many objects
            for (int i = 0; i < objects_per_cycle; ++i) {
                TestObject* obj = strategy.allocate(i, i * 1.5, "frag");
                objects.push_back(obj);
            }
            
            // Free every other object (create fragmentation)
            for (int i = 1; i < objects_per_cycle; i += 2) {
                if (objects[i]) {
                    strategy.deallocate(objects[i]);
                    objects[i] = nullptr;
                }
            }
            
            // Allocate new objects (test fragmentation handling)
            for (int i = 1; i < objects_per_cycle; i += 2) {
                objects[i] = strategy.allocate(i + 1000, i * 2.5, "refrag");
            }
            
            // Free all for next cycle
            for (TestObject* obj : objects) {
                if (obj) {
                    strategy.deallocate(obj);
                }
            }
            objects.clear();
        }
    }
    
    const int total_ops = cycles * objects_per_cycle * 2; // alloc + free
    state.SetItemsProcessed(state.iterations() * total_ops);
    state.counters["fragmentation_cycles"] = cycles;
}

/**
 * @brief Pool allocation benchmark (safe/RAII) - kept separate for RAII comparison
 * @details Measures pool allocation performance using the safe RAII interface.
 * Uses lockfree_pool_alloc_safe() which returns smart pointers with automatic cleanup.
 * This benchmark is kept separate since RAII can't be easily parameterized with raw pointers.
 * @ingroup benchmarks
 */
static void BM_PoolAllocationSafe(benchmark::State& state) {
    const int num_objects = state.range(0);
    
    for (auto _ : state) {
        // Fill array with pool-allocated objects
        std::vector<decltype(lockfree_pool_alloc_safe<TestObject>())> objects;
        objects.reserve(num_objects);
        
        for (int i = 0; i < num_objects; ++i) {
            auto obj = lockfree_pool_alloc_safe<TestObject>(i, i * 1.5, "pool_safe");
            if (obj) {
                objects.emplace_back(std::move(obj));
            }
        }
        
        // Do some work to prevent optimization
        int sum = 0;
        for (const auto& obj : objects) {
            if (obj) {
                sum += obj->do_work();
            }
        }
        benchmark::DoNotOptimize(sum);
        
        // Objects automatically returned to pool when vector goes out of scope
    }
    
    state.SetItemsProcessed(state.iterations() * num_objects);
    state.counters["objects_per_sec"] = benchmark::Counter(
        num_objects * state.iterations(), benchmark::Counter::kIsRate);
    state.counters["ns_per_object"] = benchmark::Counter(
        state.iterations() * num_objects, benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}

/**
 * @brief Parameterized mixed allocation pattern benchmark
 * @details Tests realistic allocation patterns with random allocation/deallocation timing.
 * Uses the same code path for all allocation strategies to ensure fair comparison.
 * @ingroup benchmarks
 */
static void BM_ParameterizedMixedPattern(benchmark::State& state, AllocationStrategy strategy) {
    const int total_operations = state.range(0);
    
    for (auto _ : state) {
        std::vector<TestObject*> live_objects;
        live_objects.reserve(1000);
        int total_work = 0;
        
        // Use a fixed seed for reproducible results
        std::mt19937 gen(42);
        std::uniform_int_distribution<> pattern_dis(0, 2);
        
        for (int i = 0; i < total_operations; ++i) {
            int pattern = pattern_dis(gen);
            
            if (pattern == 0 || live_objects.empty()) {
                // Allocate new object
                TestObject* obj = strategy.allocate(i, i * 1.1, "mixed");
                if (obj) {
                    live_objects.push_back(obj);
                }
            } else if (pattern == 1 && !live_objects.empty()) {
                // Free random object
                size_t idx = gen() % live_objects.size();
                strategy.deallocate(live_objects[idx]);
                live_objects.erase(live_objects.begin() + idx);
            } else {
                // Do work on random object
                if (!live_objects.empty()) {
                    size_t idx = gen() % live_objects.size();
                    total_work += live_objects[idx]->do_work();
                }
            }
        }
        
        // Clean up remaining objects
        for (TestObject* obj : live_objects) {
            strategy.deallocate(obj);
        }
        
        benchmark::DoNotOptimize(total_work);
    }
    
    state.SetItemsProcessed(state.iterations() * total_operations);
    state.counters["operations_per_sec"] = benchmark::Counter(
        total_operations * state.iterations(), benchmark::Counter::kIsRate);
}

// Register parameterized benchmarks for all allocation strategies
static void RegisterParameterizedBenchmarks() {
    auto strategies = CreateAllocationStrategies();
    
    for (const auto& strategy : strategies) {
        // Basic allocation benchmarks
        benchmark::RegisterBenchmark(
            ("BM_Allocation_" + strategy.name).c_str(),
            [strategy](benchmark::State& state) { BM_ParameterizedAllocation(state, strategy); }
        )->Range(1000, 100000)->Unit(benchmark::kMicrosecond);
        
        // Fragmentation benchmarks
        benchmark::RegisterBenchmark(
            ("BM_Fragmentation_" + strategy.name).c_str(),
            [strategy](benchmark::State& state) { BM_ParameterizedFragmentation(state, strategy); }
        )->Range(100, 2000)->Unit(benchmark::kMicrosecond);
        
        // Mixed pattern benchmarks
        benchmark::RegisterBenchmark(
            ("BM_MixedPattern_" + strategy.name).c_str(),
            [strategy](benchmark::State& state) { BM_ParameterizedMixedPattern(state, strategy); }
        )->Range(10000, 100000)->Unit(benchmark::kMicrosecond);
        
        // Multi-threaded benchmarks
        benchmark::RegisterBenchmark(
            ("BM_Allocation_" + strategy.name + "_2T").c_str(),
            [strategy](benchmark::State& state) { BM_ParameterizedAllocation(state, strategy); }
        )->Range(1000, 100000)->Threads(2)->Unit(benchmark::kMicrosecond);
        
        benchmark::RegisterBenchmark(
            ("BM_Allocation_" + strategy.name + "_4T").c_str(),
            [strategy](benchmark::State& state) { BM_ParameterizedAllocation(state, strategy); }
        )->Range(1000, 100000)->Threads(4)->Unit(benchmark::kMicrosecond);
        
        benchmark::RegisterBenchmark(
            ("BM_Allocation_" + strategy.name + "_8T").c_str(),
            [strategy](benchmark::State& state) { BM_ParameterizedAllocation(state, strategy); }
        )->Range(1000, 100000)->Threads(8)->Unit(benchmark::kMicrosecond);
    }
}

BENCHMARK(BM_PoolAllocationSafe)->Range(1000, 100000)->Unit(benchmark::kMicrosecond);

int main(int argc, char** argv) {
    RegisterParameterizedBenchmarks();
    benchmark::Initialize(&argc, argv);
    return benchmark::RunSpecifiedBenchmarks();
}

/** @} */ // end of benchmarks group
