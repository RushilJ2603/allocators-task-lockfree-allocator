# Allocators task

Phases 1 and 2. Phase 3 and the bonus parts are not done yet.

Built on Linux, g++ 13.3.0, C++17.

```
make           # builds both test binaries
make check     # builds and runs them
make asan      # address and UB sanitizers
make tsan-check  # thread sanitizer
```

## What is done

- [x] Phase 1: arena allocator over mmap, aligned allocation, O(1) reset
- [x] Phase 2: lock-free SPSC ring buffer
- [x] Bonus: ThreadSanitizer in the build

## Phase 1: arena allocator

`src/arena.h`, `src/arena.cpp`.

One `mmap` for the whole pool, then `alloc` moves an offset forward and returns
a pointer into it. `MAP_ANONYMOUS` with a file descriptor of `-1` means there is
no file behind the mapping.

```cpp
void* alloc(std::size_t size, std::size_t alignment = alignof(std::max_align_t));
void reset();
```

Alignment rounds the offset up:

```cpp
std::size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);
```

The mask only works for powers of two, so `alloc` throws on anything else
instead of handing back a misaligned pointer.

There is no per-allocation header, which is where the zero fragmentation
requirement comes from. The test shows it by filling a 4096 byte arena with
exactly 64 allocations of 64 bytes.

Individual allocations cannot be freed. `reset()` sets the offset back to zero,
so it costs the same no matter how much was handed out:

```
$ ./arena_test
reset after 10 allocs:     0.95 ns
reset after 100000 allocs: 0.95 ns
arena ok
```

The timing loop does a `reset` and an `alloc` together, because `reset` alone
has no observable effect and the compiler removes the loop entirely. The first
version of this test printed 0.00 ns for that reason.

`alloc` returns `nullptr` when full rather than throwing, since it is on the
hot path, and it checks for space before touching the offset so a failed call
changes nothing.

## Phase 2: lock-free SPSC queue

`src/spsc.h`.

No mutex, condition variable or semaphore. The single producer, single consumer
restriction is what makes that possible: the producer is the only thread that
writes `head_` and the consumer is the only one that writes `tail_`.

The indices count up forever and are masked when used, so the capacity has to
be a power of two, and the occupancy is just `head_ - tail_` with no ambiguity
between full and empty.

### Memory ordering

```cpp
// producer
if (h - tail_.load(std::memory_order_acquire) == Capacity) return false;
slots_[h & (Capacity - 1)] = v;
head_.store(h + 1, std::memory_order_release);

// consumer
if (head_.load(std::memory_order_acquire) == t) return false;
out = slots_[t & (Capacity - 1)];
tail_.store(t + 1, std::memory_order_release);
```

The producer's `release` store cannot be reordered before the slot write, and
the consumer's `acquire` load pairs with it, so seeing the new index means
seeing the data. The same pair in reverse stops the producer overwriting a slot
the consumer is still reading. Each thread loads its own index with `relaxed`
because nothing else writes it.

On x86-64 this costs no instructions. Compiled with `-O2` the acquire and
release operations are plain `mov`, with no `mfence` or `lock`:

```
$ g++ -O2 -S ... | grep -c 'mfence\|lock \|xchg'
0
```

x86 already gives this ordering in hardware, so the annotations are there to
stop the compiler reordering. Using `relaxed` everywhere would probably appear
to work here and break on ARM.

`head_` and `tail_` are `alignas(64)` so they sit on different cache lines.
Sharing a line would mean every store by one thread invalidating the other
thread's copy, even though they touch different variables.

### Results

```
$ ./spsc_test
2000000 items, 0.027s, 75.1 million/sec
spsc ok
```

The threaded test checks that every value arrives exactly once and in order,
and that the sum of everything received matches, so a lost or duplicated item
shows up rather than passing quietly. The last part of the test pushes pointers
allocated from the arena, which is what the queue exists to carry.

## ThreadSanitizer

Functional tests prove very little for a lock-free structure, since a race can
sit there for a million iterations without showing. So the queue also runs
under TSan, with no warnings.

A zero only means something if the detector works, so it was checked against a
deliberately racy program in the same setup, which TSan caught immediately.

On recent kernels TSan dies at startup with `unexpected memory mapping` because
of ASLR. `setarch -R` disables ASLR for the process, which is what the
`tsan-check` target does.

## Problems along the way

The reset timing printed `0.00 ns`. `reset()` is a single assignment in the
header, so with the loop doing nothing else the compiler worked out the whole
thing had no observable effect and removed it. The loop now does a `reset` and
an `alloc` together and keeps the returned pointer, which it cannot delete.

ThreadSanitizer would not start, dying with
`FATAL: ThreadSanitizer: unexpected memory mapping`. This is a clash with how
ASLR is configured on recent kernels rather than anything in the code. Running
the binary under `setarch -R` disables ASLR for that process and it works, so
the `tsan-check` target does that.

`std::invalid_argument` would not compile in the test, with
`expected unqualified-id before '&' token` on the catch clause. The type was
not declared because `<stdexcept>` was missing, and the error points at the
`&` rather than the include.
