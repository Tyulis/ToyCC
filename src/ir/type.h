#pragma once

#include <string>

#include "code_location.h"

namespace toycc::ir {
    struct Type {
        std::string name;
        CodeLocation location;

        Type() = default;
        Type(std::string name, CodeLocation location);

        virtual size_t size() const = 0;
        virtual size_t alignment() const = 0;
    };

    enum class PrimitiveSemantic {
        VOID, BOOL, INTEGER, FLOAT,
    };

    struct PrimitiveType : public Type {
        bool is_signed;
        PrimitiveSemantic semantic;
        size_t primitive_size;
        size_t primitive_alignment;

        PrimitiveType(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment);

        virtual size_t size() const override;
        virtual size_t alignment() const override;
    };
}
