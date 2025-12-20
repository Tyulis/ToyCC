#include "ir/type.h"
#include "code_location.h"

namespace toycc::ir {
    Type::Type(std::string name, CodeLocation location) : name(name), location(location) {}

    PrimitiveType::PrimitiveType(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment)
        : Type(name, CodeLocation{.filename = "<built-in>", .line = 0, .character = 0}),
          is_signed(is_signed), semantic(semantic), primitive_size(size), primitive_alignment(alignment) {}

    size_t PrimitiveType::size() const {
        return primitive_size;
    }

    size_t PrimitiveType::alignment() const {
        return primitive_alignment;
    }


    size_t QualifiedType::size() const {
        if (spec.pointer_level > 0)
            return POINTER_SIZE;

        size_t array_length = 1;
        for (size_t dimension : spec.array_spec)
            array_length *= dimension;
        return array_length * base_type->size();
    }

    size_t QualifiedType::alignment() const {
        if (spec.custom_alignment.has_value())
            return spec.custom_alignment.value();
        else
            return base_type->alignment();
    }

}
