#pragma once

#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "output.h"
#include "debug/dwarf.h"
#include "debug/encoder.h"
#include "debug/generation.h"

namespace toycc::debug {
    class CompilationUnit {
        public:
            // RAII object to automatically release debug info stack entries
            struct EntryLifespan {
                ~EntryLifespan();
                CompilationUnit& unit;
            };

        public:
            CompilationUnit(std::string working_directory, std::string filename);

            // To emit debugging directives at the beginning and end of the .text section
            void begin_text(CodeOutput& assembly) const;
            void end_text(CodeOutput& assembly) const;

            size_t fileno(std::string filename);  // Get the fileno for the given filename, add it if it's not known yet
            void emit_filenos(CodeOutput& assembly);
            std::string string(std::string value);  // Add a string to the debug info, return a label to it

            void push(const DebugInfoEntry& entry);                // Push an entry as a node with children
            void append(const DebugInfoEntry& entry);              // Append an entry to the children list of the current node
            void pop();                                            // Pop the last entry with children
            EntryLifespan push_auto(const DebugInfoEntry& entry);  // Push an entry with children which gets automatically popped when it goes out of scope

            void emit_debug_sections(CodeOutput& assembly);

        private:
            std::unordered_map<std::string, size_t> filenos;  // File numbers for the `.loc` directives
            size_t current_fileno = 0;

            StringTable debug_str;
            AbbreviationMap abbreviations;

            Encoder debug_info;
            Encoder debug_abbrev;

            void emit_debug_entry(const DebugInfoEntry& entry, bool has_children);
            void emit_abbreviation_entry(AbbreviationMap& abbreviations, const DebugInfoEntry& entry, bool has_children);
    };
}
