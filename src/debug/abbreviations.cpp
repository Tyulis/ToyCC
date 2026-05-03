#include "debug/abbreviations.h"
#include "debug/dwarf.h"

namespace toycc::debug {
    // -------- AbbreviationEntry
    AbbreviationEntry::AbbreviationEntry(Tag tag, bool has_children) : tag(tag), has_children(has_children ? ChildDetermination::DW_CHILDREN_yes : ChildDetermination::DW_CHILDREN_no) {}

    Encoder& AbbreviationEntry::emit(Encoder& encoder, size_t index) const {
        // DWARF5 7.5.3
        encoder.uleb128(index).uleb128(tag).int8(has_children);
        for (const auto& [attribute, form] : attributes)
            encoder.uleb128(attribute).uleb128(form);
        encoder.uleb128(0).uleb128(0);
        return encoder;
    }

    AbbreviationKey AbbreviationEntry::key() const {
        return {tag, has_children, attributes};
    }

    AbbreviationEntry& AbbreviationEntry::add(Encoder& encoder, Attribute attribute, Form form, bool value) {
        if (form == Form::DW_FORM_flag_present && !value)
            return *this;  // Not emitted at all when false
        return add(attribute, form).encode(encoder, form, std::to_string(static_cast<int>(value)));
    }

    AbbreviationEntry& AbbreviationEntry::add(Encoder& encoder, Attribute attribute, Form form, const std::string& expression) {
        if (form == Form::DW_FORM_flag_present)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "DW_FORM_flag_present can only be used with literal boolean values");
        return add(attribute, form).encode(encoder, form, expression);
    }

    AbbreviationEntry& AbbreviationEntry::add(Encoder& encoder, Attribute attribute, Form form, const AssemblyData& value) {
        if (form == Form::DW_FORM_flag_present)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "DW_FORM_flag_present can only be used with literal boolean values");

        encoder.insert(value);
        return add(attribute, form);
    }

    AbbreviationEntry& AbbreviationEntry::location(Encoder& encoder, size_t fileno, size_t line, size_t column) {
        if (fileno != 0)  add(encoder, Attribute::DW_AT_decl_file,   Form::DW_FORM_udata, fileno);
        if (line   != 0)  add(encoder, Attribute::DW_AT_decl_line,   Form::DW_FORM_udata, line);
        if (column != 0)  add(encoder, Attribute::DW_AT_decl_column, Form::DW_FORM_udata, column);
        return *this;
    }

    AbbreviationEntry& AbbreviationEntry::encode(Encoder& encoder, Form form, const std::string& expression) {
        switch (form) {
            case Form::DW_FORM_addr:        encoder.address(expression);  break;
            case Form::DW_FORM_data1:       encoder.int8   (expression);  break;
            case Form::DW_FORM_data2:       encoder.int16  (expression);  break;
            case Form::DW_FORM_data4:       encoder.int32  (expression);  break;
            case Form::DW_FORM_data8:       encoder.int64  (expression);  break;
            case Form::DW_FORM_sdata:       encoder.sleb128(expression);  break;
            case Form::DW_FORM_udata:       encoder.uleb128(expression);  break;
            case Form::DW_FORM_strp:        encoder.offset (expression);  break;
            case Form::DW_FORM_sec_offset:  encoder.offset (expression);  break;
            case Form::DW_FORM_flag:        encoder.int8   (expression);  break;
            case Form::DW_FORM_flag_present:                              break;  // Only present if true, no actual value
            case Form::DW_FORM_ref1:        encoder.int8   (expression);  break;
            case Form::DW_FORM_ref2:        encoder.int16  (expression);  break;
            case Form::DW_FORM_ref4:        encoder.int32  (expression);  break;
            case Form::DW_FORM_ref8:        encoder.int64  (expression);  break;
            case Form::DW_FORM_loclistx:    encoder.uleb128(expression);  break;

            case Form::DW_FORM_block2:
            case Form::DW_FORM_block4:
            case Form::DW_FORM_string:
            case Form::DW_FORM_block:
            case Form::DW_FORM_block1:
            case Form::DW_FORM_ref_addr:
            case Form::DW_FORM_ref_udata:
            case Form::DW_FORM_indirect:
            case Form::DW_FORM_strx:
            case Form::DW_FORM_addrx:
            case Form::DW_FORM_ref_sup4:
            case Form::DW_FORM_strp_sup:
            case Form::DW_FORM_data16:
            case Form::DW_FORM_line_strp:
            case Form::DW_FORM_ref_sig8:
            case Form::DW_FORM_implicit_const:
            case Form::DW_FORM_rnglistx:
            case Form::DW_FORM_ref_sup8:
            case Form::DW_FORM_strx1:
            case Form::DW_FORM_strx2:
            case Form::DW_FORM_strx3:
            case Form::DW_FORM_strx4:
            case Form::DW_FORM_addrx1:
            case Form::DW_FORM_addrx2:
            case Form::DW_FORM_addrx3:
            case Form::DW_FORM_addrx4:
            case Form::DW_FORM_exprloc:
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("DWARF form {} is not implemented", std::to_underlying(form)));
        }

        return *this;
    }

    AbbreviationEntry& AbbreviationEntry::add(Attribute attribute, Form form) {
        attributes.emplace_back(attribute, form);
        return *this;
    }
}
