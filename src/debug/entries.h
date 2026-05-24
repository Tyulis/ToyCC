#pragma once

#include <memory>
#include <vector>

#include "code_location.h"
#include "debug/abbreviations.h"
#include "debug/dwarf.h"
#include "debug/encoder.h"
#include "debug/loclists.h"
#include "debug/expression.h"

namespace toycc::debug {
    // -------- Entries in .debug_info
    class DataSections;

    struct DebugInfoEntry {
        public:
            Tag tag;
            std::vector<std::shared_ptr<DebugInfoEntry>> children;

            DebugInfoEntry(Tag tag);
            virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const = 0;  // Emit the attributes (not the abbreviation index)

        protected:
            AbbreviationEntry new_abbreviation() const;
    };

    struct CompilationUnitEntry : public DebugInfoEntry {
        std::string start_expr;
        std::string length_expr;
        std::string file_name;
        std::string compilation_directory;
        constexpr static Language language = Language::DW_LANG_C11;
        std::string producer;
        constexpr static IdentifierCase identifier_case = IdentifierCase::DW_ID_case_sensitive;

        CompilationUnitEntry(const std::string& start_expr, const std::string& length_expr, const std::string& file_name, const std::string& compilation_directory);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct SubrangeEntry : public DebugInfoEntry {
        size_t lower_bound;
        std::optional<size_t> upper_bound;

        SubrangeEntry(size_t lower_bound, std::optional<size_t> upper_bound);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct TypeEntry : public DebugInfoEntry {
        CodeLocation code_location;
        TypeEntry(Tag tag, CodeLocation code_location);
    };

    struct MemberEntry : public DebugInfoEntry {
        std::string name;
        size_t bit_offset;
        std::shared_ptr<TypeEntry> type;
        CodeLocation code_location;

        MemberEntry(const std::string& name, size_t bit_offset, std::shared_ptr<TypeEntry> type, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct BooleanTypeEntry : public TypeEntry {
        std::string name;
        size_t size_bits;

        BooleanTypeEntry(const std::string& name, size_t size_bits, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct IntegerTypeEntry : public TypeEntry {
        std::string name;
        bool is_signed;
        size_t size_bits;

        IntegerTypeEntry(const std::string& name, bool is_signed, size_t size_bits, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct PointerTypeEntry : public TypeEntry {
        std::shared_ptr<TypeEntry> referenced_type;

        PointerTypeEntry(std::shared_ptr<TypeEntry> referenced_type, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct ArrayTypeEntry : public TypeEntry {
        std::shared_ptr<TypeEntry> element_type;

        ArrayTypeEntry(std::shared_ptr<TypeEntry> element_type, std::optional<size_t> size, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct CompoundTypeEntry : public TypeEntry {
        size_t byte_size;
        std::optional<std::string> name;

        CompoundTypeEntry(Tag tag, size_t byte_size, std::optional<std::string> name, const std::vector<std::shared_ptr<MemberEntry>> members, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct QualifiedTypeEntry : public TypeEntry {
        std::shared_ptr<TypeEntry> element_type;

        QualifiedTypeEntry(Tag tag, std::shared_ptr<TypeEntry> element_type, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct SubprogramEntry : public DebugInfoEntry {
        std::string name;
        bool exported;
        bool is_main;
        std::string start_expr;
        std::string length_expr;
        AssemblyData frame_base;
        std::shared_ptr<TypeEntry> return_type;
        CodeLocation code_location;

        SubprogramEntry(const std::string& name, bool exported, const std::string& start_expr, const std::string& length_expr, const Expression& frame_base, std::shared_ptr<TypeEntry> return_type, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };

    struct VariableEntry : public DebugInfoEntry {
        std::string name;
        bool exported;
        std::shared_ptr<TypeEntry> type;
        LocationList location;
        CodeLocation code_location;

        VariableEntry(const std::string& name, bool exported, std::shared_ptr<TypeEntry> type, CodeLocation code_location);
        virtual AbbreviationEntry emit(Encoder& encoder, DataSections& data) const override;
    };
}

#include "debug/sections.h"

