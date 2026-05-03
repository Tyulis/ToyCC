#pragma once

#include <string>
#include <utility>
#include <type_traits>

#include "diagnostic.h"
#include "debug/dwarf.h"
#include "debug/encoder.h"

namespace toycc::debug {
    // Attribute emission
    inline std::string asm_expression(const std::string& value) {
        return value;
    }

    inline std::string asm_expression(std::integral auto value) {
        return std::to_string(value);
    }

    inline std::string asm_expression(bool value) {
        return std::to_string(static_cast<int>(value));
    }

    template <typename T> requires(std::is_enum_v<T>)
    std::string asm_expression(T value) {
        return std::to_string(std::to_underlying(value));
    }

    using AbbreviationKey = std::tuple<Tag, ChildDetermination, std::vector<std::pair<Attribute, Form>>>;

    // .debug_abbrev entry
    struct AbbreviationEntry {
        public:
            AbbreviationEntry(Tag tag, bool has_children);

            AbbreviationKey key() const;
            Encoder& emit(Encoder& encoder, size_t index) const;

            // Encode an attribute and add it to the abbreviation entry
            template <typename T> requires requires (T value) {{asm_expression(value)} -> std::same_as<std::string>;}
            AbbreviationEntry& add(Encoder& encoder, Attribute attribute, Form form, T value) {
                if (form == Form::DW_FORM_flag_present)
                    throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "DW_FORM_flag_present can only be used with literal boolean values");

                return add(attribute, form).encode(encoder, form, asm_expression(value));
            }

            AbbreviationEntry& add(Encoder& encoder, Attribute attribute, Form form, bool value);
            AbbreviationEntry& add(Encoder& encoder, Attribute attribute, Form form, const AssemblyData& value);
            AbbreviationEntry& add(Encoder& encoder, Attribute attribute, Form form, const std::string& expression);
            AbbreviationEntry& location(Encoder& encoder, size_t fileno, size_t line, size_t column);

        private:
            Tag tag;
            ChildDetermination has_children;
            std::vector<std::pair<Attribute, Form>> attributes;

            AbbreviationEntry& add(Attribute attribute, Form form);
            AbbreviationEntry& encode(Encoder& encoder, Form form, const std::string& expression);
    };
}
