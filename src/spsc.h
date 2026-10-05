// Single producer, single consumer ring buffer. No locks: only the producer
// writes head_, only the consumer writes tail_, so neither index has two
// writers.

#ifndef SPSC_H
#define SPSC_H

#include <atomic>
#include <cstddef>
#include <type_traits>

template <class T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "capacity must be a power of two");
    static_assert(std::is_trivially_copyable<T>::value, "T is copied as raw bytes");

public:
    bool push(const T& v) {
        std::size_t h = head_.load(std::memory_order_relaxed);
        if (h - tail_.load(std::memory_order_acquire) == Capacity) return false;
        slots_[h & (Capacity - 1)] = v;
        // release pairs with the consumer's acquire, so it cannot see the new
        // index without also seeing the slot written above
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& out) {
        std::size_t t = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == t) return false;
        out = slots_[t & (Capacity - 1)];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    std::size_t size() const {
        return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
    }

private:
    // Indices count up forever and get masked on use, so size is head - tail
    // and full is never confused with empty. Separate cache lines, otherwise
    // the two threads invalidate each other's line on every store.
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) T slots_[Capacity];
};

#endif
