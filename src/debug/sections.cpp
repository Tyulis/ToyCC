#include "debug/sections.h"
#include "code_location.h"
#include "debug/settings.h"

namespace toycc::debug {
    // -------- StringTable
    StringTable::StringTable(DWARFFormat format) : encoder(format) {}

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

        encoder.label(label);
        encoder.string(string);
        return labels[string];
    }

    void StringTable::emit(CodeOutput& output) const {
        output << encoder;
    }


    // -------- FileTable
    FileTable::FileTable() {
        filenos[BUILTIN_LOCATION.filename] = 0;
    }
    size_t FileTable::operator[] (const std::string& name) {
        if (name.empty())
            return 0;

        auto it = filenos.find(name);
        if (it == filenos.end()) {
            const size_t new_fileno = filenos.size();
            filenos[name] = new_fileno;
            return new_fileno;
        } else {
            return it->second;
        }
    }

    void FileTable::emit(CodeOutput& output) const {
        for (const auto& [filename, fileno] : filenos)
            output.debug(std::format(".file {} \"{}\"", fileno, filename));
    }


    // -------- DataSections
    DataSections::DataSections(DWARFFormat format) : strings(format), loclists(format), debuginfo(format), abbreviations(format), format(format) {
        debuginfo.header(CompilationUnitHeader {.debug_abbrev_label = BEGIN_DEBUG_ABBREV_LABEL});
    }

    size_t DataSections::offset(std::shared_ptr<DebugInfoEntry> entry) {
        auto it = entry_offsets.find(entry);
        if (it == entry_offsets.end())
            return encode(entry);
        else
            return it->second;
    }

    size_t DataSections::encode(std::shared_ptr<DebugInfoEntry> entry) {
        const size_t offset = debuginfo.length();
        entry_offsets[entry] = offset;  // Defense against recursive entries (that would call .offset() recursively) : set the offset in the map first

        Encoder entry_encoder(format);  // Separate encoder in case the `entry` has references to encode before itself
        AbbreviationEntry abbreviation = entry->emit(entry_encoder, *this);

        AbbreviationKey key = abbreviation.key();
        size_t abbreviation_index;
        if (!abbreviation_indices.contains(key)) {
            abbreviation_index = 1 + abbreviation_indices.size();  // Abbreviation 0 is reserved for null entries
            abbreviation_indices[key] = abbreviation_index;
            abbreviation.emit(abbreviations, abbreviation_index);  // Emit the abbreviation entry if it doesn't exist already
        } else {
            abbreviation_index = abbreviation_indices.at(key);
        }

        debuginfo.uleb128(abbreviation_index);
        debuginfo.insert(entry_encoder.encode());

        // Recursively emit the children entries
        if (!entry->children.empty()) {
            for (std::shared_ptr<DebugInfoEntry> child : entry->children)
                encode(child);

            debuginfo.uleb128(0);  // Insert a null entry to terminate the list of children
        }

        return offset;
    }
}
