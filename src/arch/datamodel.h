#pragma once

#include <memory>
#include <cstddef>
#include <unordered_set>

#include "ir/type.h"
#include "ir/type_expressions.h"

namespace toycc::arch {
    struct DataModel {
        std::shared_ptr<ir::Type>              void_type;
        std::shared_ptr<ir::BooleanType>       boolean_type;

        std::shared_ptr<ir::IntegerType>       signed_char_type;
        std::shared_ptr<ir::IntegerType>       unsigned_char_type;
        std::shared_ptr<ir::IntegerType>       signed_short_type;
        std::shared_ptr<ir::IntegerType>       unsigned_short_type;
        std::shared_ptr<ir::IntegerType>       signed_int_type;
        std::shared_ptr<ir::IntegerType>       unsigned_int_type;
        std::shared_ptr<ir::IntegerType>       signed_long_type;
        std::shared_ptr<ir::IntegerType>       unsigned_long_type;
        std::shared_ptr<ir::IntegerType>       signed_long_long_type;
        std::shared_ptr<ir::IntegerType>       unsigned_long_long_type;

        std::shared_ptr<ir::FloatingPointType> float_type;
        std::shared_ptr<ir::FloatingPointType> double_type;
        std::shared_ptr<ir::FloatingPointType> long_double_type;

        std::shared_ptr<ir::Type>              label_type;
        std::shared_ptr<ir::Type>              literal_character_type;
        std::shared_ptr<ir::Type>              literal_integer_type;
        std::shared_ptr<ir::Type>              literal_floating_type;

        std::shared_ptr<ir::IntegerType>       size_type;
        std::shared_ptr<ir::IntegerType>       pointer_type;  // Underlying integer type for pointers
        std::shared_ptr<ir::IntegerType>       offset_type;   // Pointer offset type, e.g ptrdiff_t, same as pointer_type but signed
        std::shared_ptr<ir::Type>              enum_underlying_type;
        std::shared_ptr<ir::PointerType>       void_pointer_type;

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
