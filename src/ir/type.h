#pragma once

#include <string>

#include "code_location.h"

namespace toycc::ir {
    enum class TypeCategory {
        PRIMITIVE, TYPEDEF, STRUCT, UNION, ENUM, BUILTIN,
    };

    struct TypeIdentifier {
        TypeCategory category;
        std::string name;

        std::string text() const;
        bool operator< (TypeIdentifier rhs) const;
    };

    struct Type {
        TypeIdentifier identifier;
        CodeLocation location;

        Type() = default;
        Type(TypeIdentifier identifier, CodeLocation location);
    };

    enum class PrimitiveSemantic {
        VOID, BOOL, INTEGER, FLOAT,
    };

    struct PrimitiveType : public Type {
        bool is_signed;
        PrimitiveSemantic semantic;
        size_t primitive_size;
        size_t primitive_alignment;

        PrimitiveType() = default;
        PrimitiveType(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment);
    };
}
