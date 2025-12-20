#pragma once

#include <string>
#include <memory>
#include <optional>

#include "code_location.h"
#include "util/flags.hpp"

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

    constexpr size_t POINTER_SIZE = 8;

    enum class TypeQualifier {
        CONST = 0x01, VOLATILE = 0x02, RESTRICT = 0x04, ATOMIC = 0x08,
    };

    struct QualifiedType {
        std::shared_ptr<Type> base_type;
        Flags<TypeQualifier> qualifiers;
        int pointer_level = 0;
        int array_length = 1;
        std::optional<size_t> custom_alignment;

        size_t size() const;
        size_t alignment() const;
    };

    struct Typedef {
        std::string name;
        CodeLocation location;
        std::string target;
    };
}
