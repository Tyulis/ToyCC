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
    };

    struct Struct : public Type {
        std::vector<StructMember> members;
        std::optional<size_t> struct_alignment;

        Struct() = default;
        Struct(std::string name, CodeLocation location);
    };

    struct Union : public Type {
        std::vector<StructMember> members;

        Union() = default;
        Union(std::string name, CodeLocation location);
    };

    struct Enum : public Type {
        std::map<std::string, int> values;

        Enum() = default;
        Enum(std::string name, CodeLocation location);
    };
}
