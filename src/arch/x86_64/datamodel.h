#pragma once

#include "arch/datamodel.h"

namespace toycc::arch::x86_64 {
    struct DataModel : public toycc::arch::DataModel {
        virtual size_t bool_size() const override;
        virtual size_t bool_alignment() const override;

        virtual size_t char_size() const override;
        virtual size_t char_alignment() const override;
        virtual size_t short_size() const override;
        virtual size_t short_alignment() const override;
        virtual size_t int_size() const override;
        virtual size_t int_alignment() const override;
        virtual size_t long_size() const override;
        virtual size_t long_alignment() const override;
        virtual size_t long_long_size() const override;
        virtual size_t long_long_alignment() const override;

        virtual size_t float_size() const override;
        virtual size_t float_alignment() const override;
        virtual size_t double_size() const override;
        virtual size_t double_alignment() const override;
        virtual size_t long_double_size() const override;
        virtual size_t long_double_alignment() const override;

        virtual size_t pointer_size() const override;
        virtual size_t pointer_alignment() const override;
    };

    extern DataModel DATAMODEL;
}
