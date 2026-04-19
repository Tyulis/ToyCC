#include "debug/unit.h"
#include "debug/dwarf.h"
#include "debug/debuginfo.h"
#include "ir/type.h"

namespace toycc::debug {
    const static std::string PRODUCER_IDENTIFICATION = "ToyCC";

    const static std::string BEGIN_TEXT_LABEL = ".LWL.text.begin";
    const static std::string END_TEXT_LABEL   = ".LWL.text.end";
    const static std::string BEGIN_DEBUG_ABBREV_LABEL = ".LWL.debug_abbrev.begin";

    CompilationUnit::EntryLifespan::~EntryLifespan() {
        unit.pop();
    }

    CompilationUnit::CompilationUnit(std::string working_directory, std::string filename, DWARFFormat format)
        : format(format), debug_info(format), debug_loclists(format), debug_abbrev(format)
    {
        // Emit the compilation unit headers
        debug_info.header(CompilationUnitHeader {.debug_abbrev_label = BEGIN_DEBUG_ABBREV_LABEL});
        debug_loclists.header(LocationListHeader {});

        // Emit the compilation unit root entry
        push(DebugInfoEntry(Tag::DW_TAG_compile_unit)
            .add(Attribute::DW_AT_low_pc,          Form::DW_FORM_addr,       BEGIN_TEXT_LABEL)                                         // DWARF5 3.1.1.1
            .add(Attribute::DW_AT_high_pc,         Form::DW_FORM_data8,      std::format("{}-{}", END_TEXT_LABEL, BEGIN_TEXT_LABEL))   // DWARF5 3.1.1.1
            .add(Attribute::DW_AT_name,            Form::DW_FORM_strp,       debug_str[filename])                                      // DWARF5 3.1.1.2
            .add(Attribute::DW_AT_comp_dir,        Form::DW_FORM_strp,       debug_str[working_directory])                             // DWARF5 3.1.1.6
            .add(Attribute::DW_AT_language,        Form::DW_FORM_data1,      Language::DW_LANG_C11)                                    // DWARF5 3.1.1.3
            .add(Attribute::DW_AT_stmt_list,       Form::DW_FORM_sec_offset, 0 /* FIXME ? */)                                          // DWARF5 3.1.1.4
            .add(Attribute::DW_AT_producer,        Form::DW_FORM_strp,       debug_str[PRODUCER_IDENTIFICATION])                       // DWARF5 3.1.1.7
            .add(Attribute::DW_AT_identifier_case, Form::DW_FORM_data1,      IdentifierCase::DW_ID_case_sensitive));                   // DWARF5 3.1.1.8
    }

    void CompilationUnit::begin_text(CodeOutput& assembly) const {
        assembly.label(BEGIN_TEXT_LABEL);
    }

    void CompilationUnit::end_text(CodeOutput& assembly) const {
        assembly.label(END_TEXT_LABEL);
    }

    // -------- Debug info tables management and query
    size_t CompilationUnit::fileno(std::string filename) {
        if (filename.empty())
            filename = "<unknown>";

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

    std::string CompilationUnit::string(std::string value) {
        return debug_str[value];
    }

    size_t CompilationUnit::type(std::shared_ptr<ir::Type> type) {
        auto it = types.find(type);
        if (it != types.end())
            return it->second;

        const size_t offset = debug_info.length();
        types[type] = offset;
        emit_type(type);
        return offset;
    }

    size_t CompilationUnit::emit_location_list(const LocationList& loclist) {
        const size_t offset = debug_loclists.length();
        debug_loclists.insert(loclist.encode());
        return offset;
    }

    // -------- Debug info entries
    void CompilationUnit::push(const DebugInfoEntry& entry) {
        emit_debug_entry(entry, true);
    }

    CompilationUnit::EntryLifespan CompilationUnit::push_auto(const DebugInfoEntry& entry) {
        push(entry);
        return {*this};
    }

    void CompilationUnit::append(const DebugInfoEntry& entry) {
        emit_debug_entry(entry, false);
    }

    void CompilationUnit::pop() {
        debug_info.uleb128(0);  // Emit a null record to terminate the list of children
    }

    void CompilationUnit::emit_debug_sections(CodeOutput& assembly) {
        pop();  // Pop the compilation unit
        debug_abbrev.uleb128(0);  // The .debug_abbrev section must be null-terminated

        // Fill the .debug_info section
        assembly.directive(".section .debug_info,\"\",@progbits");
        assembly << debug_info;

        // Fill the .debug_abbrev section
        assembly.directive(".section .debug_abbrev,\"\",@progbits");
        assembly.label(BEGIN_DEBUG_ABBREV_LABEL);
        assembly << debug_abbrev;

        // Fill the .debug_loclists section
        assembly.directive(".section .debug_loclists,\"\",@progbits");
        assembly << debug_loclists;

        // Fill the .debug_str section
        assembly.directive(".section .debug_str,\"\",@progbits");
        debug_str.emit(assembly);
    }

    // -------- Debug info emission internals
    void CompilationUnit::emit_debug_entry(const DebugInfoEntry& entry, bool has_children) {
        AbbreviationKey key = entry.abbrev_key(has_children);

        auto it = abbreviations.find(key);
        if (it == abbreviations.end())
            emit_abbreviation_entry(abbreviations, entry, has_children);

        const AbbreviationEntry& abbreviation = abbreviations[key];
        entry.emit(debug_info, abbreviation);
    }

    void CompilationUnit::emit_abbreviation_entry(AbbreviationMap& abbreviations, const DebugInfoEntry& entry, bool has_children) {
        AbbreviationEntry abbreviation = {.index = 1 + abbreviations.size(),  // Abbreviation 0 is reserved for null record
                                          .tag = entry.tag,
                                          .has_children = (has_children ? ChildDetermination::DW_CHILDREN_yes : ChildDetermination::DW_CHILDREN_no),
                                          .attributes = {}};

        for (const AttributeValue& attribute : entry.values)
            abbreviation.attributes.push_back({attribute.attribute, attribute.form});

        abbreviations[entry.abbrev_key(has_children)] = abbreviation;
        abbreviation.emit(debug_abbrev);
    }
}
