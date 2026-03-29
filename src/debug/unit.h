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
            CompilationUnit(std::string working_directory, std::string filename);

            // To emit debugging directives at the beginning and end of the .text section
            void begin_text(CodeOutput& assembly) const;
            void end_text(CodeOutput& assembly) const;

            size_t fileno(std::string filename);  // Get the fileno for the given filename, add it if it's not known yet
            void emit_filenos(CodeOutput& assembly);

            std::shared_ptr<DebugInfoRecord> push(Tag tag, const std::vector<AttributeValue>& attributes);    // Push an entry as a node with children
            std::shared_ptr<DebugInfoRecord> append(Tag tag, const std::vector<AttributeValue>& attributes);  // Append an entry to the children list of the current node
            std::shared_ptr<DebugInfoRecord> pop();                                                           // Pop the last entry with children

            void emit_debug_sections(CodeOutput& assembly);

        private:
            std::unordered_map<std::string, size_t> filenos;  // File numbers for the `.loc` directives
            size_t current_fileno = 0;

            std::shared_ptr<DebugInfoRecord> debug_info_root;
            std::deque<std::shared_ptr<DebugInfoRecord>> debug_info_stack;
            StringTable debug_str;

            void emit_debug_record(Encoder& debug_info, Encoder& debug_abbrev, AbbreviationMap& abbreviations, std::shared_ptr<DebugInfoRecord> record);
            void emit_abbreviation_record(Encoder& debug_abbrev, AbbreviationMap& abbreviations, std::shared_ptr<DebugInfoRecord> record);
    };
}
