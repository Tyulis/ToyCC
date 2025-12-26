#pragma once

#include <map>
#include <vector>
#include <optional>

#include "code_location.h"
#include "ir/type.h"
#include "ir/declaration.h"

namespace toycc::ir {
    struct StructMember {
        std::string name;
        CodeLocation location;
        TypeSpecification spec;

        std::string ir_code() const;
    };

    struct CompoundType : public Type {  // Struct or union
        std::vector<StructMember> members;
        std::optional<size_t> struct_alignment;
        bool is_complete = false;

        CompoundType() = default;
        CompoundType(TypeIdentifier identifier, CodeLocation location);

        virtual std::string ir_code() const override;
    };

    struct Enum : public Type {
        std::map<std::string, int> values;

        Enum() = default;
        Enum(std::string name, CodeLocation location);

        virtual std::string ir_code() const override;
    };
}
