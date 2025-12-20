#pragma once

#include <cstddef>

namespace toycc::arch {
    // Primitive type sizes
    constexpr size_t BOOL_SIZE = 1;
    constexpr size_t CHAR_SIZE = 1;
    constexpr size_t SHORT_SIZE = 2;
    constexpr size_t INT_SIZE = 4;
    constexpr size_t LONG_SIZE = 8;
    constexpr size_t LONG_LONG_SIZE = 8;

    constexpr size_t FLOAT_SIZE = 4;
    constexpr size_t DOUBLE_SIZE = 8;
    constexpr size_t LONG_DOUBLE_SIZE = 16;

    // Primitive type alignments
    constexpr size_t BOOL_ALIGNMENT = 1;
    constexpr size_t CHAR_ALIGNMENT = 1;
    constexpr size_t SHORT_ALIGNMENT = 2;
    constexpr size_t INT_ALIGNMENT = 4;
    constexpr size_t LONG_ALIGNMENT = 8;
    constexpr size_t LONG_LONG_ALIGNMENT = 8;

    constexpr size_t FLOAT_ALIGNMENT = 4;
    constexpr size_t DOUBLE_ALIGNMENT = 8;
    constexpr size_t LONG_DOUBLE_ALIGNMENT = 16;
}
