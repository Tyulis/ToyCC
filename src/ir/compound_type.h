#pragma once

#include <map>
#include <vector>
#include <optional>

#include "code_location.h"
#include "ir/type.h"
#include "ir/declaration.h"

namespace toycc::ir {
    struct StructMember : public Identifier {
        std::optional<size_t> bitfield_length;

        size_t alignment() const;
    };

    struct Struct : public Type {
        std::vector<StructMember> members;
        std::optional<size_t> struct_alignment;

        Struct() = default;
        Struct(std::string name, CodeLocation location, std::optional<size_t> struct_alignment);

        virtual size_t size() const override;
        virtual size_t alignment() const override;
    };

    struct Enum : public Type {
        std::map<std::string, int> values;

        Enum() = default;
        Enum(std::string name, CodeLocation location);

        virtual size_t size() const override;
        virtual size_t alignment() const override;
    };

    struct Union : public Type {
        std::vector<Identifier> members;

        Union() = default;
        Union(std::string name, CodeLocation location);

        virtual size_t size() const override;
        virtual size_t alignment() const override;
    };
}
