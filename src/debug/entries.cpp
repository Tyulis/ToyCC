#include "debug/settings.h"
#include "debug/entries.h"
#include "debug/dwarf.h"

namespace toycc::debug {
    // -------- DebugInfoEntry
    DebugInfoEntry::DebugInfoEntry(Tag tag) : tag(tag) {}

    AbbreviationEntry DebugInfoEntry::new_abbreviation() const {
        return AbbreviationEntry {tag, !children.empty()};
    }


    // -------- CompilationUnitEntry
    CompilationUnitEntry::CompilationUnitEntry(const std::string& start_expr, const std::string& length_expr, const std::string& file_name, const std::string& compilation_directory)
        : DebugInfoEntry(Tag::DW_TAG_compile_unit),
          start_expr(start_expr), length_expr(length_expr), file_name(file_name), compilation_directory(compilation_directory), producer(PRODUCER_IDENTIFICATION) {}

    AbbreviationEntry CompilationUnitEntry::emit(Encoder& encoder, DataSections& data) const {
        return new_abbreviation()
            .add(encoder, Attribute::DW_AT_low_pc,          Form::DW_FORM_addr,       start_expr)                           // DWARF5 3.1.1.1
            .add(encoder, Attribute::DW_AT_high_pc,         Form::DW_FORM_data8,      length_expr)                          // DWARF5 3.1.1.1
            .add(encoder, Attribute::DW_AT_name,            Form::DW_FORM_strp,       data.strings[file_name])              // DWARF5 3.1.1.2
            .add(encoder, Attribute::DW_AT_comp_dir,        Form::DW_FORM_strp,       data.strings[compilation_directory])  // DWARF5 3.1.1.6
            .add(encoder, Attribute::DW_AT_language,        Form::DW_FORM_data1,      language)                             // DWARF5 3.1.1.3
            .add(encoder, Attribute::DW_AT_stmt_list,       Form::DW_FORM_sec_offset, 0)                                    // DWARF5 3.1.1.4, required to access line number information
            .add(encoder, Attribute::DW_AT_producer,        Form::DW_FORM_strp,       data.strings[producer])               // DWARF5 3.1.1.7
            .add(encoder, Attribute::DW_AT_identifier_case, Form::DW_FORM_data1,      identifier_case)                      // DWARF5 3.1.1.8
            .add(encoder, Attribute::DW_AT_loclists_base,   Form::DW_FORM_sec_offset, data.loclists.base());                // DWARF5 3.1.1.16
    }

    // -------- TypeEntry
    TypeEntry::TypeEntry(Tag tag, CodeLocation code_location) : DebugInfoEntry(tag), code_location(code_location) {}


    // -------- MemberEntry
    MemberEntry::MemberEntry(const std::string& name, size_t bit_offset, std::shared_ptr<TypeEntry> type, CodeLocation code_location)
        : DebugInfoEntry(Tag::DW_TAG_member), name(name), bit_offset(bit_offset), type(type), code_location(code_location) {}

    AbbreviationEntry MemberEntry::emit(Encoder& encoder, DataSections& data) const {
        return new_abbreviation()
            .add(encoder, Attribute::DW_AT_name,            Form::DW_FORM_strp,  data.strings[name])
            .add(encoder, Attribute::DW_AT_data_bit_offset, Form::DW_FORM_data4, bit_offset)
            .add(encoder, Attribute::DW_AT_type,            Form::DW_FORM_ref8,  data.offset(type))
            .location(encoder, data.filenos[code_location.filename], code_location.line, code_location.character);
    }


    // -------- IntegerTypeEntry
    IntegerTypeEntry::IntegerTypeEntry(const std::string& name, bool is_signed, size_t size_bits, CodeLocation code_location)
        : TypeEntry(Tag::DW_TAG_base_type, code_location), name(name), is_signed(is_signed), size_bits(size_bits) {}

    AbbreviationEntry IntegerTypeEntry::emit(Encoder& encoder, DataSections& data) const {
        return new_abbreviation()
            .add(encoder, Attribute::DW_AT_name,     Form::DW_FORM_strp,  data.strings[name])
            .add(encoder, Attribute::DW_AT_encoding, Form::DW_FORM_data1, (is_signed ? BaseTypeEncoding::DW_ATE_signed : BaseTypeEncoding::DW_ATE_unsigned))
            .add(encoder, Attribute::DW_AT_bit_size, Form::DW_FORM_data1, size_bits)
            .location(encoder, data.filenos[code_location.filename], code_location.line, code_location.character);
    }


    // -------- PointerTypeEntry
    PointerTypeEntry::PointerTypeEntry(std::shared_ptr<TypeEntry> referenced_type, CodeLocation code_location)
        : TypeEntry(Tag::DW_TAG_pointer_type, code_location), referenced_type(referenced_type) {}

    AbbreviationEntry PointerTypeEntry::emit(Encoder& encoder, DataSections& data) const {
        AbbreviationEntry abbreviation = new_abbreviation()
            .location(encoder, data.filenos[code_location.filename], code_location.line, code_location.character);

        if (referenced_type.get() != nullptr)  // For void*, just don't encode the referenced type
            abbreviation.add(encoder, Attribute::DW_AT_type, Form::DW_FORM_ref8, data.offset(referenced_type));

        return abbreviation;
    }


    // -------- SubrangeEntry
    SubrangeEntry::SubrangeEntry(size_t lower_bound, std::optional<size_t> upper_bound)
        : DebugInfoEntry(Tag::DW_TAG_subrange_type), lower_bound(lower_bound), upper_bound(upper_bound) {}

    AbbreviationEntry SubrangeEntry::emit(Encoder& encoder, DataSections&) const {
        AbbreviationEntry abbreviation = new_abbreviation()
            .add(encoder, Attribute::DW_AT_lower_bound, Form::DW_FORM_data1, lower_bound);

        if (upper_bound.has_value())
            abbreviation.add(encoder, Attribute::DW_AT_upper_bound, Form::DW_FORM_udata, upper_bound.value());

        return abbreviation;
    }


    // -------- ArrayTypeEntry
    ArrayTypeEntry::ArrayTypeEntry(std::shared_ptr<TypeEntry> element_type, std::optional<size_t> size, CodeLocation code_location)
        : TypeEntry(Tag::DW_TAG_array_type, code_location), element_type(element_type)
    {
        children.emplace_back(std::make_shared<SubrangeEntry>(0, size));
    }

    AbbreviationEntry ArrayTypeEntry::emit(Encoder& encoder, DataSections& data) const {
        return new_abbreviation()
            .add(encoder, Attribute::DW_AT_type, Form::DW_FORM_ref8, data.offset(element_type))
            .location(encoder, data.filenos[code_location.filename], code_location.line, code_location.character);
    }


    // -------- StructTypeEntry
    StructTypeEntry::StructTypeEntry(size_t byte_size, std::optional<std::string> name, const std::vector<std::shared_ptr<MemberEntry>> members, CodeLocation code_location)
        : TypeEntry(Tag::DW_TAG_structure_type, code_location), byte_size(byte_size), name(name)
    {
        children.append_range(members);
    }

    AbbreviationEntry StructTypeEntry::emit(Encoder& encoder, DataSections& data) const {
        AbbreviationEntry abbreviation = new_abbreviation()
            .add(encoder, Attribute::DW_AT_byte_size, Form::DW_FORM_data8, byte_size)
            .location(encoder, data.filenos[code_location.filename], code_location.line, code_location.character);

        if (name.has_value())
            abbreviation.add(encoder, Attribute::DW_AT_name, Form::DW_FORM_strp, data.strings[name.value()]);

        return abbreviation;
    }


    // -------- SubprogramEntry
    SubprogramEntry::SubprogramEntry(const std::string& name, bool exported, const std::string& start_expr, const std::string& length_expr, const Expression& frame_base, std::shared_ptr<TypeEntry> return_type, CodeLocation code_location)
        : DebugInfoEntry(Tag::DW_TAG_subprogram),
          name(name), exported(exported), is_main(name == "main"), start_expr(start_expr), length_expr(length_expr),
          frame_base(frame_base.encode()), return_type(return_type), code_location(code_location) {}

    AbbreviationEntry SubprogramEntry::emit(Encoder& encoder, DataSections& data) const {
        AbbreviationEntry abbreviation = new_abbreviation()
            .add(encoder, Attribute::DW_AT_name,            Form::DW_FORM_strp,         data.strings[name])
            .add(encoder, Attribute::DW_AT_external,        Form::DW_FORM_flag_present, exported)
            .add(encoder, Attribute::DW_AT_main_subprogram, Form::DW_FORM_flag_present, is_main)
            .add(encoder, Attribute::DW_AT_low_pc,          Form::DW_FORM_addr,         start_expr)
            .add(encoder, Attribute::DW_AT_high_pc,         Form::DW_FORM_data8,        length_expr)
            .add(encoder, Attribute::DW_AT_frame_base,      Form::DW_FORM_exprloc,      frame_base)
            .location(encoder, data.filenos[code_location.filename], code_location.line, code_location.character);

        // Return type : only if the function actually returns something
        if (return_type.get() != nullptr)
            abbreviation.add(encoder, Attribute::DW_AT_type, Form::DW_FORM_ref8, data.offset(return_type));

        return abbreviation;
    }


    // -------- VariableEntry
    VariableEntry::VariableEntry(const std::string& name, bool exported, std::shared_ptr<TypeEntry> type, CodeLocation code_location)
        : DebugInfoEntry(Tag::DW_TAG_variable), name(name), exported(exported), type(type), code_location(code_location) {}

    AbbreviationEntry VariableEntry::emit(Encoder& encoder, DataSections& data) const {
        return new_abbreviation()
            .add(encoder, Attribute::DW_AT_name,     Form::DW_FORM_strp,     data.strings[name])
            .add(encoder, Attribute::DW_AT_external, Form::DW_FORM_flag,     exported)
            .add(encoder, Attribute::DW_AT_type,     Form::DW_FORM_ref8,     data.offset(type))
            .add(encoder, Attribute::DW_AT_location, Form::DW_FORM_loclistx, data.loclists.add(location))
            .location(encoder, data.filenos[code_location.filename], code_location.line, code_location.character);
    }
}
