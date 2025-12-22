#include "ir/type.h"
#include "code_location.h"

namespace toycc::ir {
    Type::Type(TypeIdentifier identifier, CodeLocation location) : identifier(identifier), location(location) {}

    PrimitiveType::PrimitiveType(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment)
        : Type(TypeIdentifier {.category = TypeCategory::PRIMITIVE, .name = name}, CodeLocation{.filename = "<built-in>", .line = 0, .character = 0}),
          is_signed(is_signed), semantic(semantic), primitive_size(size), primitive_alignment(alignment) {}
}
