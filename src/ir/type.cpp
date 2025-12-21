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
}
