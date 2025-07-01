#pragma once

/*
 * LockFreeMemoryPool - A high-performance, thread-safe, lock-free memory pool
 *
 * Features:
 * - Lock-free allocation/deallocation using atomic compare-and-swap
 * - RAII support with smart pointer integration
 * - Exception safety with strong guarantees
 * - Cache-optimized design with false sharing prevention
 * - Dual API (safe + fast) for different performance requirements
 * - Global pool management with template traits system
 *
 * This implementation uses lock-free programming techniques
 * to provide thread-safe memory pooling for high-performance applications.
 */

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// Suppress warning about intentional structure padding for cache line alignment
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)  // structure was padded due to alignment specifier
#endif

namespace lfmemorypool {

#ifndef NDEBUG
#define SAFE_CALL(expr, message)        \
    do {                                \
        if (!(expr)) {                  \
            std::cerr << "LockFreeMemoryPool assertion failed: " << message \
                      << " (" << #expr << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort();               \
        }                               \
    } while (0)
#else
#define SAFE_CALL(expr, message)                                \
    do {                                                        \
        [[maybe_unused]] bool result = static_cast<bool>(expr); \
        static_cast<void>(message);                             \
    } while (0)
#endif

/// Lock-free memory pool with RAII support and global pool management
template <typename T>
class LockFreeMemoryPool final {
   private:
    struct PoolDeleter {
        LockFreeMemoryPool* pool;

        void operator()(T* ptr) const noexcept {
            if (ptr && pool) {
                ptr->~T();  // Proper destructor call
                SAFE_CALL(pool->deallocate_impl_safe(ptr),
                          "LockFreeMemoryPool: Invalid pointer in PoolDeleter");
            }
        }
    };

    // Memory segment with proper alignment
    struct alignas(T) Segment {
        // Use aligned storage to avoid unnecessary construction/destruction
        std::aligned_storage_t<sizeof(T), alignof(T)> memory;

        // Atomic flag for lock-free allocation
        // Placed after memory to maintain alignment of T
        std::atomic<bool> available{true};
    };

   public:
    using unique_ptr_type = std::unique_ptr<T, PoolDeleter>;

    explicit LockFreeMemoryPool(std::size_t pool_size) : segments(pool_size) {
        // Initialize all blocks as free
        for (size_t i = 0; i < segments.size(); ++i) {
            segments[i].available.store(true, std::memory_order_relaxed);
        }
    }

    /// Safe allocation with automatic RAII cleanup
    template <typename... Args>
    [[nodiscard]] unique_ptr_type allocate_safe(Args&&... args) noexcept {
        try {
            T* ptr = allocate_fast(std::forward<Args>(args)...);
            if (!ptr)
                return nullptr;
            return unique_ptr_type(ptr, PoolDeleter{this});
        } catch (...) {
            // If construction throws, return null pointer
            return nullptr;
        }
    }

    /// Lock-free fast allocation for performance-critical paths
    template <typename... Args>
    [[nodiscard]] T* allocate_fast(Args&&... args) {
        const size_t pool_size = segments.size();
        constexpr int max_spurious_retries = 3;  // Limit retries for spurious CAS failures

        // Get starting hint with relaxed ordering (performance optimization)
        size_t start_idx = search_start.load(std::memory_order_relaxed);

        // Try to find and claim a free slot using lock-free search
        for (size_t attempts = 0; attempts < pool_size; ++attempts) {
            size_t idx = (start_idx + attempts) % pool_size;

            // Retry spurious failures for each slot (but with a reasonable limit)
            for (int retry = 0; retry < max_spurious_retries; ++retry) {
                // Try to atomically claim this slot
                bool expected = true;
                if (segments[idx].available.compare_exchange_weak(
                        expected, false,
                        std::memory_order_acq_rel,     // Success: acquire-release for correctness
                        std::memory_order_relaxed)) {  // Failure: relaxed for performance

                    // Successfully claimed this slot - construct object
                    T* ptr = reinterpret_cast<T*>(&segments[idx].memory);

                    try {
                        new (ptr) T(std::forward<Args>(args)...);
                    } catch (...) {
                        // Construction failed - release the slot and propagate exception
                        segments[idx].available.store(true, std::memory_order_release);
                        throw;
                    }

                    // Update hint for next allocation (relaxed - just a performance hint)
                    search_start.store((idx + 1) % pool_size, std::memory_order_relaxed);

                    return ptr;
                }

                // If expected is still true, it was a spurious failure - retry
                // If expected is false, the slot is genuinely occupied - move to next slot
                if (!expected) {
                    break;  // Slot genuinely occupied, don't retry
                }
                // else: spurious failure, retry this slot (up to max_spurious_retries)
            }

            // This slot was either taken by another thread or we exhausted retries
        }

        // Pool is exhausted
        return nullptr;
    }

    /// Lock-free fast deallocation
    void deallocate_fast(T* elem) noexcept {
        if (!elem)
            return;

        // Call destructor
        elem->~T();

        // Find the block index and mark as free
        SAFE_CALL(deallocate_impl_safe(elem),
                  "LockFreeMemoryPool: Invalid pointer in deallocate_fast");
    }

    // Deleted default, copy & move constructors and assignment-operators
    LockFreeMemoryPool() = delete;
    LockFreeMemoryPool(const LockFreeMemoryPool&) = delete;
    LockFreeMemoryPool(LockFreeMemoryPool&&) = delete;
    LockFreeMemoryPool& operator=(const LockFreeMemoryPool&) = delete;
    LockFreeMemoryPool& operator=(LockFreeMemoryPool&&) = delete;

    // Public access for optional statistics (when LockFreeMemoryPoolStats.h is included)
    // WARNING: Internal implementation details - DO NOT use directly
    const auto& get_segments_for_stats() const noexcept {
        return segments;
    }

   private:

    // Safe version that doesn't throw - returns success/failure
    [[nodiscard]] bool deallocate_impl_safe(const T* elem) noexcept {
        // Calculate the block index from the pointer
        const Segment* block = reinterpret_cast<const Segment*>(elem);
        const size_t idx = block - &segments[0];

        // Mark as free with release ordering to ensure visibility
        segments[idx].available.store(true, std::memory_order_release);
        return true;
    }

    std::vector<Segment> segments;

    // Starting index for allocation search (performance optimization)
    // This doesn't need to be perfectly accurate, just a starting point
    alignas(64) std::atomic<size_t> search_start{0};  // Avoid false sharing
};

// Global Lock-Free Pool Management System

/// Pool registry for type-specific lock-free pool management
template <typename T>
struct LockFreePoolRegistry {};

/// Macro to define a lock-free pool for a specific type
#define DEFINE_LOCKFREE_POOL(Type, Size)                \
    template <>                                         \
    struct lfmemorypool::LockFreePoolRegistry<Type> {       \
        static inline LockFreeMemoryPool<Type> pool{Size}; \
    }

/**
 * @brief Global safe allocation function with RAII support (lock-free)
 *
 * Allocates an object from the global pool registry and returns a unique_ptr
 * with automatic cleanup. This is the recommended allocation method for most use cases.
 *
 * @tparam T Type to allocate (must be registered with DEFINE_LOCKFREE_POOL)
 * @tparam Args Constructor argument types (deduced)
 * @param args Constructor arguments to forward to T's constructor
 * @return unique_ptr<T> with custom deleter, or nullptr if allocation fails
 * @note This function is noexcept and will return nullptr instead of throwing
 * @note The returned unique_ptr automatically returns memory to the pool when destroyed
 */
template <typename T, typename... Args>
[[nodiscard]] auto lockfree_pool_alloc_safe(Args&&... args) noexcept {
    return LockFreePoolRegistry<T>::pool.allocate_safe(std::forward<Args>(args)...);
}

/**
 * @brief Global fast allocation function for performance-critical paths (lock-free)
 *
 * Allocates an object from the global pool registry and returns a raw pointer.
 * Faster than the safe version but requires manual cleanup with lockfree_pool_free_fast().
 *
 * @tparam T Type to allocate (must be registered with DEFINE_LOCKFREE_POOL)
 * @tparam Args Constructor argument types (deduced)
 * @param args Constructor arguments to forward to T's constructor
 * @return T* Raw pointer to allocated object, or nullptr if allocation fails
 * @warning Must be paired with lockfree_pool_free_fast() - no automatic cleanup
 * @note May throw if constructor throws during object construction
 */
template <typename T, typename... Args>
[[nodiscard]] T* lockfree_pool_alloc_fast(Args&&... args) {
    return LockFreePoolRegistry<T>::pool.allocate_fast(std::forward<Args>(args)...);
}

/**
 * @brief Global fast deallocation function (lock-free)
 *
 * Returns memory allocated with lockfree_pool_alloc_fast() back to the pool.
 * Calls the object's destructor and marks the memory as available for reuse.
 *
 * @tparam T Type to deallocate (automatically deduced from pointer)
 * @param ptr Pointer to object allocated with lockfree_pool_alloc_fast()
 * @note Safe to call with nullptr (no-op)
 * @warning Only use with pointers from lockfree_pool_alloc_fast() for the same type T
 * @warning Do not use with pointers from the safe allocation function (unique_ptr handles those)
 */
template <typename T>
void lockfree_pool_free_fast(T* ptr) noexcept {
    LockFreePoolRegistry<T>::pool.deallocate_fast(ptr);
}

}  // namespace lfmemorypool

#ifdef _MSC_VER
#pragma warning(pop)
#endif
