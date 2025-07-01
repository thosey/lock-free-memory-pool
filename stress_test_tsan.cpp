/*
 * Intensive stress test for ThreadSanitizer validation
 * This test exercises the lock-free memory pool with high contention
 * to detect any potential data races or memory ordering issues.
 */

#include "src/LockFreeMemoryPool.h"
#include "src/LockFreeMemoryPoolStats.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace lfmemorypool;

struct TestObject {
    std::atomic<int> counter{0};
    char data[128];
    
    TestObject() {
        // Initialize with a recognizable pattern
        for (size_t i = 0; i < sizeof(data); ++i) {
            data[i] = static_cast<char>(i % 256);
        }
    }
    
    explicit TestObject(int initial_value) : counter(initial_value) {
        for (size_t i = 0; i < sizeof(data); ++i) {
            data[i] = static_cast<char>((i + initial_value) % 256);
        }
    }
};

// Define a global pool for stress testing
DEFINE_LOCKFREE_POOL(TestObject, 1000);

void stress_test_worker(int thread_id, int operations, std::atomic<long>& allocation_count, 
                       std::atomic<long>& deallocation_count, std::atomic<long>& operation_count) {
    std::random_device rd;
    std::mt19937 gen(rd() + thread_id);
    std::uniform_int_distribution<> op_dist(0, 99);
    std::uniform_int_distribution<> hold_dist(0, 10);
    
    std::vector<TestObject*> held_objects;
    held_objects.reserve(50);
    
    for (int i = 0; i < operations; ++i) {
        int op = op_dist(gen);
        
        if (op < 70 || held_objects.empty()) {  // 70% allocation, but always allocate if empty
            // Allocation
            TestObject* obj = lockfree_pool_alloc_fast<TestObject>(thread_id * 10000 + i);
            if (obj) {
                // Verify object state
                obj->counter.fetch_add(1, std::memory_order_acq_rel);
                held_objects.push_back(obj);
                allocation_count.fetch_add(1, std::memory_order_relaxed);
                
                // Occasionally validate the data
                if (i % 100 == 0) {
                    char expected = static_cast<char>((0 + thread_id * 10000 + i) % 256);
                    if (obj->data[0] != expected) {
                        std::cerr << "Data corruption detected in thread " << thread_id << std::endl;
                    }
                }
            }
        } else {
            // Deallocation
            if (!held_objects.empty()) {
                size_t idx = gen() % held_objects.size();
                TestObject* obj = held_objects[idx];
                
                // Final validation
                int final_count = obj->counter.load(std::memory_order_acquire);
                if (final_count < 1) {
                    std::cerr << "Counter validation failed in thread " << thread_id << std::endl;
                }
                
                lockfree_pool_free_fast(obj);
                held_objects.erase(held_objects.begin() + idx);
                deallocation_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
        
        // Occasionally hold objects for a bit to increase contention
        if (hold_dist(gen) == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        
        operation_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Clean up remaining objects
    for (auto* obj : held_objects) {
        lockfree_pool_free_fast(obj);
        deallocation_count.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    const int num_threads = std::thread::hardware_concurrency();
    const int operations_per_thread = 5000;
    
    std::cout << "Starting intensive lock-free stress test with ThreadSanitizer\n";
    std::cout << "Threads: " << num_threads << ", Operations per thread: " << operations_per_thread << std::endl;
    
    std::atomic<long> allocation_count{0};
    std::atomic<long> deallocation_count{0};
    std::atomic<long> operation_count{0};
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    // Start all worker threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(stress_test_worker, i, operations_per_thread, 
                           std::ref(allocation_count), std::ref(deallocation_count), 
                           std::ref(operation_count));
    }
    
    // Monitor progress
    auto monitor_thread = std::thread([&]() {
        while (operation_count.load(std::memory_order_relaxed) < num_threads * operations_per_thread) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            auto stats = lfmemorypool::stats::lockfree_pool_stats<TestObject>();
            std::cout << "Progress: " << operation_count.load() << "/" << (num_threads * operations_per_thread)
                      << " operations, Pool: " << stats.used_objects << " used, " 
                      << stats.free_objects << " free" << std::endl;
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    monitor_thread.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Final statistics
    auto final_stats = lfmemorypool::stats::lockfree_pool_stats<TestObject>();
    
    std::cout << "\n=== Stress Test Results ===\n";
    std::cout << "Duration: " << duration.count() << " ms\n";
    std::cout << "Total operations: " << operation_count.load() << "\n";
    std::cout << "Allocations: " << allocation_count.load() << "\n";
    std::cout << "Deallocations: " << deallocation_count.load() << "\n";
    std::cout << "Final pool state - Used: " << final_stats.used_objects 
              << ", Free: " << final_stats.free_objects 
              << ", Utilization: " << final_stats.utilization_percent << "%\n";
    
    if (final_stats.used_objects == 0) {
        std::cout << "SUCCESS: All objects properly returned to pool\n";
    } else {
        std::cout << "WARNING: " << final_stats.used_objects << " objects still allocated\n";
    }
    
    std::cout << "ThreadSanitizer validation complete - check for any race condition reports above\n";
    
    return 0;
}
