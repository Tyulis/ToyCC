#pragma once

#include <cstddef>

namespace toycc::arch {
    struct DataModel {
        virtual size_t bool_size() const = 0;
        virtual size_t bool_alignment() const = 0;

        virtual size_t char_size() const = 0;
        virtual size_t char_alignment() const = 0;
        virtual size_t short_size() const = 0;
        virtual size_t short_alignment() const = 0;
        virtual size_t int_size() const = 0;
        virtual size_t int_alignment() const = 0;
        virtual size_t long_size() const = 0;
        virtual size_t long_alignment() const = 0;
        virtual size_t long_long_size() const = 0;
        virtual size_t long_long_alignment() const = 0;

        virtual size_t float_size() const = 0;
        virtual size_t float_alignment() const = 0;
        virtual size_t double_size() const = 0;
        virtual size_t double_alignment() const = 0;
        virtual size_t long_double_size() const = 0;
        virtual size_t long_double_alignment() const = 0;

        virtual size_t pointer_size() const = 0;
        virtual size_t pointer_alignment() const = 0;
    };

    extern DataModel* DATAMODEL;
}
