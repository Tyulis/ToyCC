#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "code_location.h"
#include "ir/type.h"
#include "util/flags.hpp"

namespace toycc::ir {
    enum class StorageClass {
        AUTO         = 0x01,
        STATIC       = 0x02,
        EXTERN       = 0x04,
        REGISTER     = 0x08,
        THREAD_LOCAL = 0x10,
        TYPEDEF      = 0x20,
    };

    enum class TypeQualifier {
        CONST    = 0x01,
        VOLATILE = 0x02,
        RESTRICT = 0x04,
        ATOMIC   = 0x08,
    };

    enum class FunctionSpecifier {
        INLINE   = 0x01,
        NORETURN = 0x02,
    };

    struct TypeSpecification;

    struct FunctionPrototype{
        std::vector<TypeSpecification> return_type;  // That's in a vector just for circular dependency issues
        std::vector<TypeSpecification> parameters;
    };

    struct TypeSpecification {
        std::shared_ptr<ir::Type> type;
        Flags<TypeQualifier> qualifiers;
        std::vector<Flags<TypeQualifier>> pointer_spec;
        std::vector<size_t> array_spec;
        Flags<FunctionSpecifier> function_spec;
        std::optional<FunctionPrototype> prototype;
        std::optional<size_t> custom_alignment;
        std::optional<size_t> bitfield_length;

        void check(bool in_struct, CodeLocation location) const;
        TypeSpecification merge (TypeSpecification overriding, CodeLocation location) const;
    };

    struct Declaration {
        std::string name;
        CodeLocation location;
        Flags<StorageClass> storage;
        TypeSpecification spec;

        void check(bool is_struct) const;
    };
}
