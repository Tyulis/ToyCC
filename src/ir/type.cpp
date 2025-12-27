#include <format>

#include "diagnostic.h"
#include "ir/type.h"
#include "code_location.h"

namespace toycc::ir {
    static std::string category_repr(TypeCategory category) {
        switch (category) {
            case TypeCategory::VOID:       return "VOID";
            case TypeCategory::PRIMITIVE:  return "PRIMITIVE";
            case TypeCategory::TYPEDEF:    return "TYPEDEF";
            case TypeCategory::BUILTIN:    return "BUILTIN";
            case TypeCategory::STRUCT:     return "STRUCT";
            case TypeCategory::UNION:      return "UNION";
            case TypeCategory::ENUM:       return "ENUM";
        }

        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category");
    }

    std::string TypeIdentifier::text() const {
        switch (category) {
            case TypeCategory::VOID:
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

        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category");
    }

    std::string TypeIdentifier::ir_code() const {
        return std::format("{} {}", category_repr(category), name);
    }

    bool TypeIdentifier::operator< (TypeIdentifier rhs) const {
        if (category != rhs.category)
            return category < rhs.category;
        return name < rhs.name;
    }

    Type::Type(TypeIdentifier identifier, CodeLocation location) : identifier(identifier), location(location) {}
    std::string Type::ir_code() const {
        return std::format("#type {}", identifier.ir_code());
    }


    static std::string semantic_repr(PrimitiveSemantic semantic) {
        switch (semantic) {
            case PrimitiveSemantic::BOOL:    return "BOOL";
            case PrimitiveSemantic::FLOAT:   return "FLOAT";
            case PrimitiveSemantic::INTEGER: return "INTEGER";
        }

        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid primitive semantic");
    }

    PrimitiveType::PrimitiveType(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment, size_t conversion_rank)
        : Type(TypeIdentifier {.category = TypeCategory::PRIMITIVE, .name = name}, CodeLocation{.filename = "<built-in>", .line = 0, .character = 0}),
          is_signed(is_signed), semantic(semantic), primitive_size(size), primitive_alignment(alignment), conversion_rank(conversion_rank) {}

    std::string PrimitiveType::ir_code() const {
        return std::format("#type {} : {} {} size {} alignment {}",
                           identifier.ir_code(), (is_signed? "signed" : "unsigned"), semantic_repr(semantic), primitive_size, primitive_alignment);
    }
}
