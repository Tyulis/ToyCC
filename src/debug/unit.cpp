#include "debug/unit.h"
#include "debug/dwarf.h"
#include "debug/generation.h"

namespace toycc::debug {
    const static std::string PRODUCER_IDENTIFICATION = "ToyCC";

    const static std::string BEGIN_TEXT_LABEL = ".WL.text.begin";
    const static std::string END_TEXT_LABEL   = ".WL.text.end";
    const static std::string BEGIN_DEBUG_ABBREV_LABEL = ".WL.debug_abbrev.begin";

    CompilationUnit::CompilationUnit(std::string working_directory, std::string filename) {
        debug_info_root = push(Tag::DW_TAG_compile_unit, {
            {Attribute::DW_AT_low_pc,          Form::DW_FORM_addr,       BEGIN_TEXT_LABEL},                                         // DWARF5 3.1.1.1
            {Attribute::DW_AT_high_pc,         Form::DW_FORM_data8,      std::format("{}-{}", END_TEXT_LABEL, BEGIN_TEXT_LABEL)},   // DWARF5 3.1.1.1
            {Attribute::DW_AT_name,            Form::DW_FORM_strp,       debug_str[filename]},                                      // DWARF5 3.1.1.2
            {Attribute::DW_AT_comp_dir,        Form::DW_FORM_strp,       debug_str[working_directory]},                             // DWARF5 3.1.1.6
            {Attribute::DW_AT_language,        Form::DW_FORM_data1,      Language::DW_LANG_C11},                                    // DWARF5 3.1.1.3
            {Attribute::DW_AT_stmt_list,       Form::DW_FORM_sec_offset, 0 /* FIXME ? */},                                          // DWARF5 3.1.1.4
            {Attribute::DW_AT_producer,        Form::DW_FORM_strp,       debug_str[PRODUCER_IDENTIFICATION]},                       // DWARF5 3.1.1.7
            {Attribute::DW_AT_identifier_case, Form::DW_FORM_data1,      IdentifierCase::DW_ID_case_sensitive},                     // DWARF5 3.1.1.8
        });
    }

    void CompilationUnit::begin_text(CodeOutput& assembly) const {
        assembly.label(BEGIN_TEXT_LABEL);
    }

    void CompilationUnit::end_text(CodeOutput& assembly) const {
        assembly.label(END_TEXT_LABEL);
    }

    size_t CompilationUnit::fileno(std::string filename) {
        auto it = filenos.find(filename);
        if (it == filenos.end()) {
            size_t new_fileno = current_fileno++;
            filenos[filename] = new_fileno;
            return new_fileno;
        } else {
            return it->second;
        }
    }

    void CompilationUnit::emit_filenos(CodeOutput& assembly) {
        for (const auto& [filename, fileno] : filenos)
            assembly.debug(std::format(".file {} \"{}\"", fileno, filename));
    }

    std::shared_ptr<DebugInfoRecord> CompilationUnit::push(Tag tag, const std::vector<AttributeValue>& attributes) {
        std::shared_ptr<DebugInfoRecord> record = append(tag, attributes);
        debug_info_stack.push_back(record);
        return record;
    }

    std::shared_ptr<DebugInfoRecord> CompilationUnit::append(Tag tag, const std::vector<AttributeValue>& attributes) {
        std::shared_ptr<DebugInfoRecord> record = std::make_shared<DebugInfoRecord>(tag, attributes);
        if (!debug_info_stack.empty())
            debug_info_stack.back()->children.push_back(record);
        return record;
    }

    std::shared_ptr<DebugInfoRecord> CompilationUnit::pop() {
        std::shared_ptr<DebugInfoRecord> record = debug_info_stack.back();
        debug_info_stack.pop_back();
        return record;
    }

    void CompilationUnit::emit_debug_sections(CodeOutput& assembly) {
        Encoder debug_info;
        Encoder debug_abbrev;

        debug_info.header({.debug_abbrev_label = BEGIN_DEBUG_ABBREV_LABEL});

        AbbreviationMap abbreviations;
        emit_debug_record(debug_info, debug_abbrev, abbreviations, debug_info_root);

        // Fill the .debug_info section
        assembly.directive(".section .debug_info,\"\",@progbits");
        assembly.directive(".int 0xFFFFFFFF");  // Length field : set 64-bit DWARF
        assembly.directive(std::format(".quad {}", debug_info.length()));
        assembly << debug_info;

        // Fill the .debug_abbrev section
        assembly.directive(".section .debug_abbrev,\"\",@progbits");
        assembly.label(BEGIN_DEBUG_ABBREV_LABEL);
        assembly << debug_abbrev;

        // Fill the .debug_str section
        assembly.directive(".section .debug_str,\"\",@progbits");
        debug_str.emit(assembly);
    }

    void CompilationUnit::emit_debug_record(Encoder& debug_info, Encoder& debug_abbrev, AbbreviationMap& abbreviations, std::shared_ptr<DebugInfoRecord> record) {
        AbbreviationKey key = record->abbrev_key();

        auto it = abbreviations.find(key);
        if (it == abbreviations.end())
            emit_abbreviation_record(debug_abbrev, abbreviations, record);

        const AbbreviationEntry& abbreviation = abbreviations[key];
        record->emit(debug_info, abbreviation);

        // Emit children of this record
        if (!record->children.empty()) {
            for (std::shared_ptr<DebugInfoRecord> child : record->children)
                emit_debug_record(debug_info, debug_abbrev, abbreviations, child);

            // Emit a null record to terminate the list of children
            debug_info.uleb128(0);
        }
    }

    void CompilationUnit::emit_abbreviation_record(Encoder& debug_abbrev, AbbreviationMap& abbreviations, std::shared_ptr<DebugInfoRecord> record) {
        AbbreviationEntry abbreviation = {.index = 1 + abbreviations.size(),  // Abbreviation 0 is reserved for null record
                                          .tag = record->tag,
                                          .has_children = (record->children.empty() ? ChildDetermination::DW_CHILDREN_no : ChildDetermination::DW_CHILDREN_yes),
                                          .attributes = {}};

        for (const AttributeValue& attribute : record->values)
            abbreviation.attributes.push_back({attribute.attribute, attribute.form});

        abbreviations[record->abbrev_key()] = abbreviation;
        abbreviation.emit(debug_abbrev);
    }
}
