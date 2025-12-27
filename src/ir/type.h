#pragma once

#include <string>

#include "code_location.h"

namespace toycc::ir {
    enum class PrimitiveConversionRank : size_t {
        BOOL        =  0,
        CHAR        = 10,
        SHORT       = 20,
        INT         = 30,
        LONG        = 40,
        LONG_LONG   = 50,
        FLOAT       = 60,
        DOUBLE      = 70,
        LONG_DOUBLE = 80,
    };

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

        PrimitiveType() = default;
        PrimitiveType(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment);

        virtual std::string ir_code() const override;
    };
}
