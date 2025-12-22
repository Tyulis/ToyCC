#include <format>

#include "diagnostic.h"
#include "ir/type.h"
#include "code_location.h"

namespace toycc::ir {
    std::string TypeIdentifier::text() const {
        switch (category) {
            case TypeCategory::PRIMITIVE:
            case TypeCategory::TYPEDEF:
            case TypeCategory::BUILTIN:
                return name;
            case TypeCategory::STRUCT:
                return std::format("struct {}", name);
            case TypeCategory::UNION:
                return std::format("union {}", name);
            case TypeCategory::ENUM:
                return std::format("enum {}", name);
        }

        throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Invalid type category");
    }

    bool TypeIdentifier::operator< (TypeIdentifier rhs) const {
        if (category != rhs.category)
            return category < rhs.category;
        return name < rhs.name;
    }

    Type::Type(TypeIdentifier identifier, CodeLocation location) : identifier(identifier), location(location) {}

    PrimitiveType::PrimitiveType(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment)
        : Type(TypeIdentifier {.category = TypeCategory::PRIMITIVE, .name = name}, CodeLocation{.filename = "<built-in>", .line = 0, .character = 0}),
          is_signed(is_signed), semantic(semantic), primitive_size(size), primitive_alignment(alignment) {}
}
