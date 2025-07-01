#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include "../src/LockFreeMemoryPool.h"
#include "../src/LockFreeMemoryPoolStats.h"

using namespace lfmemorypool;

struct Foo {
    int value;
    std::string name;

    Foo() : value(0), name("default") {}
    Foo(int v, const std::string &n) : value(v), name(n) {}

    bool operator==(const Foo &other) const {
        return value == other.value && name == other.name;
    }
};

struct Bar {
    double data[10];
    int counter;

    Bar() : counter(0) {
        for (int i = 0; i < 10; ++i) {
            data[i] = i * 1.5;
        }
    }

    explicit Bar(int c) : counter(c) {
        for (int i = 0; i < 10; ++i) {
            data[i] = i * c * 2.0;
        }
    }
};

struct Baz {
    std::atomic<int> atomic_value;
    char buffer[64];

    Baz() : atomic_value(42) {
        memset(buffer, 0, sizeof(buffer));
    }

    explicit Baz(int val) : atomic_value(val) {
        snprintf(buffer, sizeof(buffer), "Baz_%d", val);
    }
};

// Define lock-free pools for our test types
DEFINE_LOCKFREE_POOL(Foo, 1000);
DEFINE_LOCKFREE_POOL(Bar, 500);
DEFINE_LOCKFREE_POOL(Baz, 750);

// Test fixtures for Google Test
class LockFreeMemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

class GlobalLockFreeMemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(LockFreeMemoryPoolTest, BasicAllocationDeallocation) {
    LockFreeMemoryPool<int> pool(10);

    int *ptr1 = pool.allocate_fast(42);
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(*ptr1, 42);

    int *ptr2 = pool.allocate_fast(100);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr2, 100);
    EXPECT_NE(ptr1, ptr2);

    pool.deallocate_fast(ptr1);
    pool.deallocate_fast(ptr2);
}

TEST_F(LockFreeMemoryPoolTest, SafeAllocationWithRAII) {
    LockFreeMemoryPool<Foo> pool(5);

    auto ptr1 = pool.allocate_safe(123, "test");
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(ptr1->value, 123);
    EXPECT_EQ(ptr1->name, "test");

    auto ptr2 = pool.allocate_safe(456, "another");
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2->value, 456);
    EXPECT_EQ(ptr2->name, "another");
}

TEST_F(LockFreeMemoryPoolTest, PoolExhaustion) {
    const size_t pool_size = 3;
    LockFreeMemoryPool<int> pool(pool_size);

    std::vector<decltype(pool.allocate_safe(0))> ptrs;

    // Allocate all available slots using safe allocation
    for (size_t i = 0; i < pool_size; ++i) {
        auto ptr = pool.allocate_safe(static_cast<int>(i));
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(*ptr, static_cast<int>(i));
        ptrs.push_back(std::move(ptr));
    }

    // Next allocation should fail
    auto overflow_ptr = pool.allocate_safe(999);
    EXPECT_EQ(overflow_ptr, nullptr);

    // Deallocate one and try again
    ptrs[0].reset();  // RAII cleanup

    auto new_ptr = pool.allocate_safe(1000);
    ASSERT_NE(new_ptr, nullptr);
    EXPECT_EQ(*new_ptr, 1000);
}

TEST_F(LockFreeMemoryPoolTest, PoolStatistics) {
    const size_t pool_size = 10;
    LockFreeMemoryPool<int> pool(pool_size);

    auto stats = lfmemorypool::stats::get_pool_stats(pool);
    EXPECT_EQ(stats.total_objects, pool_size);
    EXPECT_EQ(stats.free_objects, pool_size);
    EXPECT_EQ(stats.used_objects, 0);
    EXPECT_DOUBLE_EQ(stats.utilization_percent, 0.0);

    std::vector<decltype(pool.allocate_safe(0))> ptrs;
    for (int i = 0; i < 5; ++i) {
        auto ptr = pool.allocate_safe(i);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(*ptr, i);
        ptrs.push_back(std::move(ptr));
    }

    stats = lfmemorypool::stats::get_pool_stats(pool);
    EXPECT_EQ(stats.total_objects, pool_size);
    EXPECT_EQ(stats.free_objects, 5);
    EXPECT_EQ(stats.used_objects, 5);
    EXPECT_DOUBLE_EQ(stats.utilization_percent, 50.0);

}

// Global pool tests
TEST_F(GlobalLockFreeMemoryPoolTest, PoolAllocationFoo) {
    // Test global safe allocation
    auto foo1 = lockfree_pool_alloc_safe<Foo>(42, "global_test");
    ASSERT_NE(foo1, nullptr);
    EXPECT_EQ(foo1->value, 42);
    EXPECT_EQ(foo1->name, "global_test");

    // Test global fast allocation
    Foo *foo2 = lockfree_pool_alloc_fast<Foo>(100, "fast_alloc");
    ASSERT_NE(foo2, nullptr);
    EXPECT_EQ(foo2->value, 100);
    EXPECT_EQ(foo2->name, "fast_alloc");

    // Clean up fast allocation
    lockfree_pool_free_fast(foo2);

    // RAII cleanup happens automatically for foo1
}

TEST_F(GlobalLockFreeMemoryPoolTest, PoolAllocationBar) {
    auto bar1 = lockfree_pool_alloc_safe<Bar>(5);
    ASSERT_NE(bar1, nullptr);
    EXPECT_EQ(bar1->counter, 5);
    EXPECT_DOUBLE_EQ(bar1->data[0], 0.0);
    EXPECT_DOUBLE_EQ(bar1->data[1], 10.0);  // 1 * 5 * 2.0

    Bar *bar2 = lockfree_pool_alloc_fast<Bar>();
    ASSERT_NE(bar2, nullptr);
    EXPECT_EQ(bar2->counter, 0);
    EXPECT_DOUBLE_EQ(bar2->data[0], 0.0);
    EXPECT_DOUBLE_EQ(bar2->data[1], 1.5);

    lockfree_pool_free_fast(bar2);
}

TEST_F(GlobalLockFreeMemoryPoolTest, PoolAllocationBaz) {
    auto baz1 = lockfree_pool_alloc_safe<Baz>(123);
    ASSERT_NE(baz1, nullptr);
    EXPECT_EQ(baz1->atomic_value.load(), 123);
    EXPECT_STREQ(baz1->buffer, "Baz_123");

    Baz *baz2 = lockfree_pool_alloc_fast<Baz>();
    ASSERT_NE(baz2, nullptr);
    EXPECT_EQ(baz2->atomic_value.load(), 42);

    lockfree_pool_free_fast(baz2);
}

TEST_F(GlobalLockFreeMemoryPoolTest, PoolStatistics) {
    auto foo_stats = lfmemorypool::stats::lockfree_pool_stats<Foo>();
    EXPECT_EQ(foo_stats.total_objects, 1000);

    auto bar_stats = lfmemorypool::stats::lockfree_pool_stats<Bar>();
    EXPECT_EQ(bar_stats.total_objects, 500);

    auto baz_stats = lfmemorypool::stats::lockfree_pool_stats<Baz>();
    EXPECT_EQ(baz_stats.total_objects, 750);
}

// Multi-threading tests
TEST_F(LockFreeMemoryPoolTest, ConcurrentAllocationDeallocation) {
    const size_t pool_size = 1000;
    const size_t num_threads = 8;
    const size_t operations_per_thread = 100;

    LockFreeMemoryPool<int> pool(pool_size);
    std::atomic<int> successful_allocations{0};
    std::atomic<int> failed_allocations{0};

    std::vector<std::jthread> threads;

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back(
            [&pool, &successful_allocations, &failed_allocations, operations_per_thread, t]() {
                std::vector<int *> local_ptrs;

                // Allocation phase
                for (size_t i = 0; i < operations_per_thread; ++i) {
                    int *ptr = pool.allocate_fast(static_cast<int>(t * 1000 + i));
                    if (ptr) {
                        local_ptrs.push_back(ptr);
                        successful_allocations.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        failed_allocations.fetch_add(1, std::memory_order_relaxed);
                    }
                }

                // Small delay to simulate work
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                // Deallocation phase
                for (auto ptr : local_ptrs) {
                    pool.deallocate_fast(ptr);
                }
            });
    }

    // Wait for all threads to complete (jthread automatically joins in destructor)
    threads.clear();  // This will cause all jthreads to be destroyed and joined

    EXPECT_GT(successful_allocations.load(), 0);
    EXPECT_LE(successful_allocations.load(), pool_size);

    // Pool should be empty now
    auto stats = lfmemorypool::stats::get_pool_stats(pool);
    EXPECT_EQ(stats.used_objects, 0);
    EXPECT_EQ(stats.free_objects, pool_size);
}

TEST_F(GlobalLockFreeMemoryPoolTest, ConcurrentAllocationDeallocation) {
    const size_t num_threads = 4;
    const size_t operations_per_thread = 50;

    std::atomic<int> successful_operations{0};
    std::vector<std::jthread> threads;

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&successful_operations, operations_per_thread, t]() {
            for (size_t i = 0; i < operations_per_thread; ++i) {
                // Test concurrent access to different pool types
                auto foo = lockfree_pool_alloc_safe<Foo>(static_cast<int>(t * 100 + i),
                                                         "thread_" + std::to_string(t));

                if (foo) {
                    EXPECT_EQ(foo->value, static_cast<int>(t * 100 + i));
                    successful_operations.fetch_add(1, std::memory_order_relaxed);
                }

                // Mix of safe and fast allocations
                if (i % 2 == 0) {
                    Bar *bar = lockfree_pool_alloc_fast<Bar>(static_cast<int>(i));
                    if (bar) {
                        EXPECT_EQ(bar->counter, static_cast<int>(i));
                        lockfree_pool_free_fast(bar);
                        successful_operations.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    EXPECT_GT(successful_operations.load(), 0);
}

// Edge case tests
TEST_F(LockFreeMemoryPoolTest, NullPointerDeallocation) {
    LockFreeMemoryPool<Foo> pool(10);

    // Should not crash
    pool.deallocate_fast(nullptr);

    // Global function should also handle null
    lockfree_pool_free_fast<Foo>(nullptr);
}

TEST_F(LockFreeMemoryPoolTest, NullPointerHandling) {
    LockFreeMemoryPool<int> pool(5);

    // Test deallocating a nullptr (should be safe)
    pool.deallocate_fast(nullptr);

    // Pool should still be functional
    int *valid_ptr = pool.allocate_fast(100);
    ASSERT_NE(valid_ptr, nullptr);
    EXPECT_EQ(*valid_ptr, 100);

    pool.deallocate_fast(valid_ptr);
}

TEST_F(LockFreeMemoryPoolTest, ConstructorExceptionHandling) {
    struct ThrowingType {
        ThrowingType(bool should_throw) {
            if (should_throw) {
                throw std::runtime_error("Constructor failed");
            }
        }
    };

    LockFreeMemoryPool<ThrowingType> pool(5);

    // Normal construction should work
    auto ptr1 = pool.allocate_fast(false);
    ASSERT_NE(ptr1, nullptr);

    // Exception during construction should not leak memory
    EXPECT_THROW({ [[maybe_unused]] auto result = pool.allocate_fast(true); }, std::runtime_error);

    // Pool should still be usable
    auto ptr2 = pool.allocate_fast(false);
    ASSERT_NE(ptr2, nullptr);

    pool.deallocate_fast(ptr1);
    pool.deallocate_fast(ptr2);
}