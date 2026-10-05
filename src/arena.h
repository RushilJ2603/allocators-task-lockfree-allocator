// Arena (bump) allocator. One mmap up front, then allocation is just moving an
// offset forward. No per-allocation header, so nothing is wasted except the
// padding an alignment request needs.
//
// Individual allocations can't be freed. reset() drops everything at once.

#ifndef ARENA_H
#define ARENA_H

#include <cstddef>
#include <cstdint>

class Arena {
public:
    explicit Arena(std::size_t capacity);
    ~Arena();
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    // nullptr when full, rather than throwing, since this is on the hot path.
    // alignment must be a power of two.
    void* alloc(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    template <class T>
    T* alloc_n(std::size_t count = 1) {
        return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }

    void reset() { offset_ = 0; }

    std::size_t capacity() const { return capacity_; }
    std::size_t used() const { return offset_; }

private:
    std::uint8_t* base_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t offset_ = 0;
};

#endif
