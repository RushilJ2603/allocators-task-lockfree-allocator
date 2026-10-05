#include "arena.h"

#include <sys/mman.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

Arena::Arena(std::size_t capacity) {
    // MAP_ANONYMOUS with fd -1 means there's no file behind the mapping, so
    // the kernel just gives us zeroed pages.
    void* m = ::mmap(nullptr, capacity, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED)
        throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));

    base_ = static_cast<std::uint8_t*>(m);
    capacity_ = capacity;
}

Arena::~Arena() {
    if (base_) ::munmap(base_, capacity_);
}

void* Arena::alloc(std::size_t size, std::size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        throw std::invalid_argument("alignment must be a power of two");

    // round the offset up to the next multiple of alignment
    std::size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);

    if (aligned > capacity_ || size > capacity_ - aligned) return nullptr;

    offset_ = aligned + size;
    return base_ + aligned;
}
