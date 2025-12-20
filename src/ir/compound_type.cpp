#include <format>

#include "ir/compound_type.h"
#include "arch/x86_64.h"
#include "util/alignment.hpp"

namespace toycc::ir {
    constexpr size_t BITFIELD_UNIT_BITS = 64;

    // ------------ StructMember
    size_t StructMember::alignment() const {
        if (bitfield_length.has_value())
            return 1;
        else
            return this->type.alignment();
    }

    // ------------ Struct
    Struct::Struct(std::string name, CodeLocation location, std::optional<size_t> struct_alignment)
        : Type(std::format("struct {}", name), location), struct_alignment(struct_alignment) {}

    size_t Struct::size() const {
        size_t total = 0;
        std::optional<size_t> current_bitfield_size;
        for (const StructMember& member : members) {
            if (member.bitfield_length.has_value()) {
                if (!current_bitfield_size.has_value()) {
                    if (current_bitfield_size.value() + member.bitfield_length.value() > BITFIELD_UNIT_BITS) {
                        size_t unit_size = next_power_of_two(current_bitfield_size.value());
                        total = align_offset(total, unit_size) + unit_size;
                    } else {
                        current_bitfield_size.value() += member.bitfield_length.value();
                    }
                } else {
                    current_bitfield_size = member.bitfield_length.value();
                }
            } else {
                if (current_bitfield_size.has_value()) {
                    size_t unit_size = next_power_of_two(current_bitfield_size.value());
                    total = align_offset(total, unit_size) + unit_size;
                    current_bitfield_size.reset();
                } else {
                    total = align_offset(total, member.type.alignment()) + member.type.size();
                }
            }
        }

        return total;
    }

    size_t Struct::alignment() const {
        if (struct_alignment.has_value())
            return struct_alignment.value();
        else if (members.size() == 0)
            return 1;
        else
            return members[0].alignment();
    }


    // ------------ Enum
    Enum::Enum(std::string name, CodeLocation location) : Type(std::format("enum {}", name), location) {}

    size_t Enum::size() const {
        return toycc::arch::INT_SIZE;
    }

    size_t Enum::alignment() const {
        return toycc::arch::INT_ALIGNMENT;
    }


    // ------------ Union
    Union::Union(std::string name, CodeLocation location) : Type(std::format("union {}", name), location) {}

    size_t Union::size() const {
        size_t max_size = 0;
        for (const Identifier& member : members)
            max_size = std::max(max_size, member.type.size());
        return max_size;
    }

    size_t Union::alignment() const {
        size_t max_alignment = 0;
        for (const Identifier& member : members)
            max_alignment = std::max(max_alignment, member.type.alignment());
        return max_alignment;
    }
}
