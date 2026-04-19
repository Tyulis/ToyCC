#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "debug/dwarf.h"
#include "output.h"
#include "ir/flow.h"
#include "ir/type.h"
#include "ir/type_expressions.h"
#include "debug/encoder.h"
#include "debug/loclist.h"
#include "debug/debuginfo.h"

namespace toycc::debug {
    class CompilationUnit {
        public:
            // RAII object to automatically release debug info stack entries
            struct EntryLifespan {
                ~EntryLifespan();
                CompilationUnit& unit;
            };

        public:
            const DWARFFormat format;

            CompilationUnit(std::string working_directory, std::string filename, DWARFFormat format = DWARFFormat::DWARF32);

            // To emit debugging directives at the beginning and end of the .text section
            void begin_text(CodeOutput& assembly) const;
            void end_text(CodeOutput& assembly) const;

            // Manage inner objects
            size_t fileno(std::string filename);  // Get the fileno for the given filename, add it if it's not known yet
            std::string string(std::string value);  // Add a string to the debug info, return a label to it
            size_t type(std::shared_ptr<ir::Type> type_expression);  // Add a type to the debug info, return the reference to it
            size_t emit_location_list(const LocationList& loclist);  // Emit a location list, return its offset

            void push(const DebugInfoEntry& entry);                // Push an entry as a node with children
            void append(const DebugInfoEntry& entry);              // Append an entry to the children list of the current node
            void pop();                                            // Pop the last entry with children
            EntryLifespan push_auto(const DebugInfoEntry& entry);  // Push an entry with children which gets automatically popped when it goes out of scope

            void emit_filenos(CodeOutput& assembly);
            void emit_debug_sections(CodeOutput& assembly);

            // -------- Entry generation -> debug/generation.cpp
            DebugInfoEntry procedure(const ir::Procedure& procedure);
            DebugInfoEntry variable(std::shared_ptr<ir::Declaration> variable, const LocationList& loclist);

        private:
            std::unordered_map<std::string, size_t> filenos;  // File numbers for the `.loc` directives
            size_t current_fileno = 0;

            StringTable debug_str;
            AbbreviationMap abbreviations;
            std::unordered_map<std::shared_ptr<ir::Type>, size_t> types;  // Type -> offset in .debug_info

            LengthFieldEncoder debug_info;
            LengthFieldEncoder debug_loclists;
            Encoder debug_abbrev;

            void emit_debug_entry(const DebugInfoEntry& entry, bool has_children);
            void emit_abbreviation_entry(AbbreviationMap& abbreviations, const DebugInfoEntry& entry, bool has_children);

            // -------- Entry generation -> debug/generation.cpp
            void emit_type(std::shared_ptr<ir::Type> type_expression);
            void emit_integer_type(std::shared_ptr<ir::IntegerType> type_expression);
            void emit_pointer_type(std::shared_ptr<ir::PointerType> type_expression);
            void emit_array_type  (std::shared_ptr<ir::ArrayType>   type_expression);
            void emit_struct_type (std::shared_ptr<ir::StructType>  type_expression);

            void emit_member(const ir::Member& member, size_t bit_offset);
    };
}
