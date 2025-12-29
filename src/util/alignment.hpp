#pragma once

#include <cstddef>

namespace toycc {
    constexpr inline size_t is_power_of_two(size_t value) {
        if (value == 0)
            return false;
        return (value & (value - 1)) == 0;
    }

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

    constexpr inline size_t size_bits_to_bytes(size_t size_bits) {
        const size_t remainder = size_bits & 7;
        if (remainder == 0)  return size_bits;
        else                 return size_bits + (8 - remainder);
    }

    constexpr inline size_t alignment_bits_to_bytes(size_t alignment_bits) {
        return next_power_of_two(size_bits_to_bytes(alignment_bits));
    }
}
