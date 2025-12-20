#pragma once

#include <cstddef>

namespace toycc {
    constexpr inline size_t align_offset(size_t offset, size_t alignment) {
        return ((offset + (alignment - 1)) & ~(alignment - 1));
    }

    constexpr inline size_t previous_power_of_two(size_t x) {
        if (x == 0)
            return 0;

        x |= (x >> 1);
        x |= (x >> 2);
        x |= (x >> 4);
        x |= (x >> 8);
        x |= (x >> 16);
        x |= (x >> 32);
        return x - (x >> 1);
    }

    constexpr inline size_t next_power_of_two(size_t x) {
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x |= x >> 32;
        return x + 1;
    }
}
