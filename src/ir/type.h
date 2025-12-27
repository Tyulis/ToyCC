#pragma once

#include <string>

#include "code_location.h"

namespace toycc::ir {
    enum class TypeCategory {
        VOID, PRIMITIVE, TYPEDEF, STRUCT, UNION, ENUM, BUILTIN,
    };

    struct TypeIdentifier {
        TypeCategory category;
        std::string name;

        std::string text() const;
        std::string ir_code() const;
        bool operator< (TypeIdentifier rhs) const;
    };

    struct Type {
        TypeIdentifier identifier;
        CodeLocation location;

        Type() = default;
        Type(TypeIdentifier identifier, CodeLocation location);

        virtual std::string ir_code() const;
    };

    enum class PrimitiveSemantic {
        BOOL, INTEGER, FLOAT,
    };

    struct PrimitiveType : public Type {
        bool is_signed;
        PrimitiveSemantic semantic;
        size_t primitive_size;
        size_t primitive_alignment;
        size_t conversion_rank;

        PrimitiveType() = default;
        PrimitiveType(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment, size_t conversion_rank);

        virtual std::string ir_code() const override;
    };
}
