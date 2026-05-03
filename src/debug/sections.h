#pragma once

#include <map>
#include <string>
#include <memory>

#include "debug/dwarf.h"
#include "debug/encoder.h"
#include "debug/entries.h"
#include "debug/loclists.h"

namespace toycc::debug {
    // String table, encoded as the .debug_str section
    class StringTable {
        public:
            StringTable(DWARFFormat format);

            // Get the label associated to a string, insert it if it doesn't exist
            const std::string& operator[] (const std::string& string);
            void emit(CodeOutput& output) const;

        private:
            const std::string& insert(const std::string& string);

            size_t current_id = 0;
            std::map<std::string, std::string> labels;  // String -> label map
            Encoder encoder;
    };

    class FileTable {
        public:
            FileTable();
            size_t operator[] (const std::string& name);
            void emit(CodeOutput& output) const;

        private:
            std::unordered_map<std::string, size_t> filenos;  // File numbers for the `.loc` directives
    };

    class DataSections {
        public:
            StringTable strings;
            LocationListsSection loclists;
            FileTable filenos;
            LengthFieldEncoder debuginfo;
            Encoder abbreviations;

            DataSections(DWARFFormat format);
            size_t offset(std::shared_ptr<DebugInfoEntry> entry);  // Encode the entry if not already done, then return its offset in .debug_info
            size_t encode(std::shared_ptr<DebugInfoEntry> entry);  // Encode an entry, return its offset in .debug_info

        private:
            DWARFFormat format;

            std::unordered_map<std::shared_ptr<DebugInfoEntry>, size_t> entry_offsets;
            std::map<AbbreviationKey, size_t> abbreviation_indices;
    };
}
