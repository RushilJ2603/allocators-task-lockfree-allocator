#include "arena.h"

#include <cassert>
#include <initializer_list>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

int main() {
    // alignment
    Arena a(1 << 20);
    for (std::size_t align : {1u, 8u, 64u, 128u}) {
        void* p = a.alloc(1, align);
        assert(p && reinterpret_cast<std::uintptr_t>(p) % align == 0);
    }

    // allocations must not overlap, so write a different byte into each one
    // and check nothing got trampled
    a.reset();
    std::uint8_t* blocks[100];
    for (int i = 0; i < 100; i++) {
        blocks[i] = static_cast<std::uint8_t*>(a.alloc(64, 16));
        assert(blocks[i]);
        std::memset(blocks[i], i, 64);
    }
    for (int i = 0; i < 100; i++)
        for (int b = 0; b < 64; b++) assert(blocks[i][b] == i);

    // no per-allocation header, so 64 x 64 bytes fills a 4096 byte arena exactly
    Arena small(4096);
    for (int i = 0; i < 64; i++) assert(small.alloc(64, 64));
    assert(small.used() == 4096);
    assert(small.alloc(1, 1) == nullptr);

    // reset reuses the same memory
    Arena r(1 << 16);
    void* first = r.alloc(128, 64);
    for (int i = 0; i < 50; i++) r.alloc(128, 64);
    r.reset();
    assert(r.used() == 0);
    assert(r.alloc(128, 64) == first);

    // reset should not care how much was allocated. Keeping the result of the
    // alloc stops the compiler deleting the loop, since reset on its own has
    // no visible effect.
    auto time_reset = [](int n) {
        Arena x(1 << 24);
        for (int i = 0; i < n; i++) x.alloc(64, 8);
        void* last = nullptr;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 100000; i++) { x.reset(); last = x.alloc(64, 8); }
        auto t1 = std::chrono::steady_clock::now();
        assert(last);
        return std::chrono::duration<double, std::nano>(t1 - t0).count() / 100000;
    };
    std::printf("reset after 10 allocs:     %.2f ns\n", time_reset(10));
    std::printf("reset after 100000 allocs: %.2f ns\n", time_reset(100000));

    std::printf("arena ok\n");
    return 0;
}
