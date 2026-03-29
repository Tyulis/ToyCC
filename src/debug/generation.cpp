#include <utility>

#include "diagnostic.h"
#include "debug/generation.h"
#include "debug/dwarf.h"

namespace toycc::debug {
    // -------- StringTable
    const std::string& StringTable::operator[] (const std::string& string) {
        auto it = labels.find(string);
        if (it == labels.end())
            return insert(string);
        else
            return it->second;
    }

    const std::string& StringTable::insert(const std::string& string) {
        std::string label = std::format(".WS{}", current_id++);
        labels[string] = label;

        assembly.label(label);
        assembly.directive(std::format(".string \"{}\"", string));
        return labels[string];
    }

    CodeOutput& StringTable::emit(CodeOutput& output) const {
        output << assembly;
        return output;
    }

    // -------- AttributeValue
    Encoder& AttributeValue::emit(Encoder& encoder) const {
        switch (form) {
            case Form::DW_FORM_addr:        encoder.offset(expression);  break;
            case Form::DW_FORM_data1:       encoder.int8  (expression);  break;
            case Form::DW_FORM_data2:       encoder.int16 (expression);  break;
            case Form::DW_FORM_data4:       encoder.int32 (expression);  break;
            case Form::DW_FORM_data8:       encoder.int64 (expression);  break;
            case Form::DW_FORM_strp:        encoder.offset(expression);  break;
            case Form::DW_FORM_sec_offset:  encoder.offset(expression);  break;

            case Form::DW_FORM_block2:
            case Form::DW_FORM_block4:
            case Form::DW_FORM_string:
            case Form::DW_FORM_block:
            case Form::DW_FORM_block1:
            case Form::DW_FORM_flag:
            case Form::DW_FORM_sdata:
            case Form::DW_FORM_udata:
            case Form::DW_FORM_ref_addr:
            case Form::DW_FORM_ref1:
            case Form::DW_FORM_ref2:
            case Form::DW_FORM_ref4:
            case Form::DW_FORM_ref8:
            case Form::DW_FORM_ref_udata:
            case Form::DW_FORM_indirect:
            case Form::DW_FORM_exprloc:
            case Form::DW_FORM_flag_present:
            case Form::DW_FORM_strx:
            case Form::DW_FORM_addrx:
            case Form::DW_FORM_ref_sup4:
            case Form::DW_FORM_strp_sup:
            case Form::DW_FORM_data16:
            case Form::DW_FORM_line_strp:
            case Form::DW_FORM_ref_sig8:
            case Form::DW_FORM_implicit_const:
            case Form::DW_FORM_loclistx:
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
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("DWARF form {} is not implemented", std::to_underlying(form)));
        }

        return encoder;
    }

    // -------- DebugInfoRecord
    AbbreviationKey DebugInfoRecord::abbrev_key() const {
        ChildDetermination has_children = (children.empty() ? ChildDetermination::DW_CHILDREN_no : ChildDetermination::DW_CHILDREN_yes);

        std::set<Attribute> keyset;
        for (const AttributeValue& value : values)
            keyset.insert(value.attribute);

        return {tag, has_children, keyset};
    }

    Encoder& DebugInfoRecord::emit(Encoder& encoder, const AbbreviationEntry& abbreviation) const {
        encoder.uleb128(abbreviation.index);
        for (const auto& [attribute, form] : abbreviation.attributes) {
            auto value = std::ranges::find_if(values, [attribute](const AttributeValue& value) { return value.attribute == attribute; });
            if (value == values.end())
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid abbreviation entry : attribute not found");

            value->emit(encoder);
        }

        return encoder;
    }

    // -------- AbbreviationEntry
    Encoder& AbbreviationEntry::emit(Encoder& encoder) const {
        // DWARF5 7.5.3
        encoder.uleb128(index).uleb128(tag).int8(has_children);
        for (const auto& [attribute, form] : attributes)
            encoder.uleb128(attribute).uleb128(form);
        encoder.uleb128(0).uleb128(0);
        return encoder;
    }
}
