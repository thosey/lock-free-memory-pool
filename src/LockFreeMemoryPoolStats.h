#pragma once

/*
 * LockFreeMemoryPool Statistics - Pool monitoring and diagnostics
 * Include this header to enable statistics collection for the memory pool.
 */

namespace lfmemorypool {

// Forward declarations
template <typename T>
class LockFreeMemoryPool;

template <typename T>
struct LockFreePoolRegistry;

/// Statistics namespace containing pool monitoring and diagnostics functionality
namespace stats {

/// Pool statistics structure for monitoring and diagnostics
struct PoolStats {
    size_t total_objects;        ///< Total number of segments in the pool
    size_t free_objects;         ///< Number of available segments
    size_t used_objects;         ///< Number of occupied segments
    double utilization_percent;  ///< Percentage of pool utilization (0-100)
};

namespace detail {
// Implementation that accesses pool internals via public accessor
template <typename T>
PoolStats get_pool_stats_impl(const LockFreeMemoryPool<T>& pool) noexcept {
    size_t free_count = 0;
    const auto& segments = pool.get_segments_for_stats();
    const size_t total = segments.size();

    // Count free objects (snapshot - may be slightly inaccurate)
    for (const auto& segment : segments) {
        if (segment.available.load(std::memory_order_relaxed)) {
            ++free_count;
        }
    }

    size_t used = total - free_count;

    return PoolStats{total, free_count, used, total > 0 ? static_cast<double>(used) / total * 100.0 : 0.0};
}
}  // namespace detail

/// Get pool statistics for a specific pool instance
template <typename T>
PoolStats get_pool_stats(const LockFreeMemoryPool<T>& pool) noexcept {
    return detail::get_pool_stats_impl(pool);
}

/// Get lock-free pool statistics for a type (using global registry)
template <typename T>
PoolStats lockfree_pool_stats() noexcept {
    return detail::get_pool_stats_impl(LockFreePoolRegistry<T>::pool);
}

}  // namespace stats

}  // namespace lfmemorypool
