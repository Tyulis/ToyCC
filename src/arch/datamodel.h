#pragma once

#include <memory>
#include <cstddef>
#include <unordered_set>

#include "ir/type.h"

namespace toycc::arch {
    struct DataModel {
        std::shared_ptr<ir::Type> void_type;
        std::shared_ptr<ir::Type> boolean_type;

        std::shared_ptr<ir::Type> signed_char_type;
        std::shared_ptr<ir::Type> unsigned_char_type;
        std::shared_ptr<ir::Type> signed_short_type;
        std::shared_ptr<ir::Type> unsigned_short_type;
        std::shared_ptr<ir::Type> signed_int_type;
        std::shared_ptr<ir::Type> unsigned_int_type;
        std::shared_ptr<ir::Type> signed_long_type;
        std::shared_ptr<ir::Type> unsigned_long_type;
        std::shared_ptr<ir::Type> signed_long_long_type;
        std::shared_ptr<ir::Type> unsigned_long_long_type;

        std::shared_ptr<ir::Type> float_type;
        std::shared_ptr<ir::Type> double_type;
        std::shared_ptr<ir::Type> long_double_type;

        std::shared_ptr<ir::Type> label_type;
        std::shared_ptr<ir::Type> literal_character_type;
        std::shared_ptr<ir::Type> literal_integer_type;
        std::shared_ptr<ir::Type> literal_floating_type;

        std::shared_ptr<ir::Type> size_type;
        std::shared_ptr<ir::Type> offset_type;
        std::shared_ptr<ir::Type> ptrdiff_type;
        std::shared_ptr<ir::Type> enum_underlying_type;
        std::shared_ptr<ir::Type> void_pointer_type;

        size_t pointer_size;
        size_t pointer_alignment;

        inline std::unordered_set<std::shared_ptr<ir::Type>> builtin_types() const {
            return {void_type, boolean_type, signed_char_type, unsigned_char_type, signed_short_type, unsigned_short_type,
                    signed_int_type, unsigned_int_type, signed_long_type, unsigned_long_type, signed_long_long_type, unsigned_long_long_type,
                    float_type, double_type, long_double_type};
        }
    };

    extern DataModel* DATAMODEL;
}
