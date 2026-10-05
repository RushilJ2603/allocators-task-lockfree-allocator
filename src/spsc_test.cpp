#include "arena.h"
#include "spsc.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

int main() {
    // order, full and empty
    SpscQueue<int, 8> q;
    int v;
    assert(q.empty() && !q.pop(v));
    for (int i = 0; i < 8; i++) assert(q.push(i));
    assert(!q.push(99));
    for (int i = 0; i < 8; i++) { assert(q.pop(v) && v == i); }
    assert(q.empty());

    // wrap around more than once
    for (int round = 0; round < 50; round++) {
        for (int i = 0; i < 8; i++) assert(q.push(round * 8 + i));
        for (int i = 0; i < 8; i++) { assert(q.pop(v) && v == round * 8 + i); }
    }

    // two threads. every value has to arrive once, in order.
    constexpr int kCount = 2'000'000;
    SpscQueue<std::uint64_t, 1024> big;
    std::uint64_t sum = 0;
    bool ordered = true;

    auto t0 = std::chrono::steady_clock::now();
    std::thread consumer([&] {
        std::uint64_t expect = 0;
        for (int got = 0; got < kCount; ) {
            std::uint64_t x;
            if (big.pop(x)) {
                if (x != expect++) ordered = false;
                sum += x;
                got++;
            }
        }
    });
    for (std::uint64_t i = 0; i < kCount; i++) while (!big.push(i)) { }
    consumer.join();
    auto t1 = std::chrono::steady_clock::now();

    assert(ordered);
    assert(sum == (std::uint64_t)kCount * (kCount - 1) / 2);
    double secs = std::chrono::duration<double>(t1 - t0).count();
    std::printf("%d items, %.3fs, %.1f million/sec\n", kCount, secs, kCount / secs / 1e6);

    // what the queue is for: pointers out of the arena
    struct Reading { std::uint32_t id, value; };
    Arena arena(1 << 20);
    SpscQueue<Reading*, 256> ptrs;
    std::uint64_t total = 0;

    std::thread reader([&] {
        for (int got = 0; got < 10000; ) {
            Reading* r;
            if (ptrs.pop(r)) { total += r->value; got++; }
        }
    });
    for (int i = 0; i < 10000; i++) {
        Reading* r = arena.alloc_n<Reading>();
        r->id = i;
        r->value = i * 2;
        while (!ptrs.push(r)) { }
    }
    reader.join();
    assert(total == 10000ull * 9999);

    std::printf("spsc ok\n");
    return 0;
}
