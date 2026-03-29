#pragma once

#include <any>
#include <map>
#include <set>
#include <memory>
#include <vector>
#include <utility>

#include "output.h"
#include "debug/dwarf.h"
#include "debug/encoder.h"

namespace toycc::debug {
    // String table, encoded as the .debug_str section
    class StringTable {
        public:
            // Get the label associated to a string, insert it if it doesn't exist
            const std::string& operator[] (const std::string& string);
            CodeOutput& emit(CodeOutput& output) const;

        private:
            const std::string& insert(const std::string& string);

            size_t current_id = 0;
            std::map<std::string, std::string> labels;  // String -> label map
            CodeOutput assembly;
    };

    // Actual .debug_abbrev entry
    struct AbbreviationEntry {
        size_t index;
        Tag tag;
        ChildDetermination has_children;
        std::vector<std::pair<Attribute, Form>> attributes;

        Encoder& emit(Encoder& encoder) const;
    };

    using AbbreviationKey = std::tuple<Tag, ChildDetermination, std::set<Attribute>>;
    using AbbreviationMap = std::map<AbbreviationKey, AbbreviationEntry>;

    // Attribute emission
    inline std::string asm_expression(const std::string& value) {
        return value;
    }

    inline std::string asm_expression(std::integral auto value) {
        return std::to_string(value);
    }

    template <typename T> requires(std::is_enum_v<T>)
    std::string asm_expression(T value) {
        return std::to_string(std::to_underlying(value));
    }

    // Higher-level debug info record, that needs to be split into .debug_info and .debug_abbrev entries
    struct AttributeValue {
        Attribute attribute;
        Form form;
        std::string expression;

        template <typename T> requires requires (T value) {{asm_expression(value)} -> std::same_as<std::string>;}
        AttributeValue(Attribute attribute, Form form, T value) : attribute(attribute), form(form), expression(asm_expression(value)) {}

        Encoder& emit(Encoder& encoder) const;
    };

    struct DebugInfoRecord {
        Tag tag;
        std::vector<AttributeValue> values;
        std::vector<std::shared_ptr<DebugInfoRecord>> children;

        AbbreviationKey abbrev_key() const;
        Encoder& emit(Encoder& encoder, const AbbreviationEntry& abbreviation) const;
    };


    // Actual .debug_info entry
    struct DebugInfoEntry {
        size_t abbrev_index;
        std::map<Attribute, std::any> values;

        size_t emit(CodeOutput& assembly, const AbbreviationEntry& abbrev) const;
    };
}
