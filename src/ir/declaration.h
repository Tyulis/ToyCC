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
        AUTO         = 0x001,
        STATIC       = 0x002,
        EXTERN       = 0x004,
        REGISTER     = 0x008,
        THREAD_LOCAL = 0x010,
        TYPEDEF      = 0x020,

        PARAMETER    = 0x040,  // Function parameter
        TEMPORARY    = 0x080,  // Temporary variable internal to the IR
        ADDRESSED    = 0x100,  // Something requires the memory address of this variable
    };

    enum class TypeQualifier {
        CONST    = 0x01,
        VOLATILE = 0x02,
        RESTRICT = 0x04,
        ATOMIC   = 0x08,
    };

    enum class FunctionSpecifier {
        INLINE      = 0x01,
        NORETURN    = 0x02,
    };

    struct Declaration;

    struct TypeSpecification {
        std::shared_ptr<ir::Type> type;
        Flags<TypeQualifier> qualifiers;
        std::vector<Flags<TypeQualifier>> pointer_spec;
        std::vector<std::shared_ptr<Declaration>> array_spec;
        bool is_function_type = false;
        Flags<FunctionSpecifier> function_spec;
        std::vector<Declaration> parameters;
        std::optional<size_t> custom_alignment;
        std::optional<size_t> bitfield_length;

        void check(bool in_struct, CodeLocation location) const;
        TypeSpecification merge (TypeSpecification overriding, CodeLocation location) const;
        bool is_void() const;
        std::string ir_code() const;
        TypeSpecification return_type() const;

        bool operator== (const TypeSpecification& spec) const;
    };

    struct Declaration {
        std::string name;
        CodeLocation location;
        Flags<StorageClass> storage;
        TypeSpecification spec;

        void check(bool is_struct) const;
        std::string ir_code() const;

        bool operator== (const Declaration& decl) const;
    };
}
