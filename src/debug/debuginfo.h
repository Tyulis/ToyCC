#pragma once

#include <deque>
#include <memory>
#include <unordered_map>

#include "debug/dwarf.h"
#include "debug/entries.h"
#include "ir/flow.h"
#include "ir/type.h"
#include "ir/declaration.h"
#include "ir/type_expressions.h"

namespace toycc::debug {
    // Manager and generator for .debug_info entries
    class DebugInfo{
        public:
            DWARFFormat format;

            DebugInfo(std::string working_directory, std::string filename, DWARFFormat format = DWARFFormat::DWARF32);

            // RAII object to automatically release debug info stack entries
            struct EntryLifespan {
                ~EntryLifespan();
                DebugInfo& debuginfo;
            };

            // -------- Entry and stack management
            void push(std::shared_ptr<DebugInfoEntry> entry);                // Push an entry as a node with children
            void append(std::shared_ptr<DebugInfoEntry> entry);              // Append an entry to the children list of the current node
            void pop();                                                      // Pop the last entry with children
            EntryLifespan push_auto(std::shared_ptr<DebugInfoEntry> entry);  // Push an entry with children which gets automatically popped when it goes out of scope

            // -------- Generate and access entries
            std::shared_ptr<TypeEntry> type(std::shared_ptr<ir::Type> type);                     // Get the type entry for the given `type`
            std::shared_ptr<VariableEntry> variable(std::shared_ptr<ir::Declaration> variable);  // Get the variable entry for the given `variable`
            std::shared_ptr<SubprogramEntry> procedure(const ir::Procedure& procedure);          // Generate an entry for the given procedure

            // -------- Actual code emission
            void begin_text(CodeOutput& assembly) const;                             // Emit debugging directives after the beginning of the .text section
            void wrap_text (CodeOutput& assembly, const std::string& text_section);  // Emit all debug information around the complete `.text` section
            void end_text  (CodeOutput& assembly) const;                             // Emit debugging directives before the end of the .text section

            // -------- Helpers
            size_t fileno(const std::string& filename);  // Get the file number associated to this file name for line number information
            Expression expr() const;                     // Create a new expression compatible with this debug info

        private:
            struct TypeEqual {
                inline bool operator() (std::shared_ptr<ir::Type> left, std::shared_ptr<ir::Type> right) const {
                    return *left == *right;
                }
            };

            using TypeEntryMap = std::unordered_map<std::shared_ptr<ir::Type>, std::shared_ptr<TypeEntry>, std::hash<std::shared_ptr<ir::Type>>, TypeEqual>;

            DataSections data;
            std::deque<std::shared_ptr<DebugInfoEntry>> stack;
            TypeEntryMap types;
            std::unordered_map<std::shared_ptr<ir::Declaration>, std::shared_ptr<VariableEntry>> variables;

            std::shared_ptr<TypeEntry>         add_type_entry        (std::shared_ptr<ir::Type>        type_expression);
            std::shared_ptr<IntegerTypeEntry>  add_integer_type_entry(std::shared_ptr<ir::IntegerType> type_expression);
            std::shared_ptr<PointerTypeEntry>  add_pointer_type_entry(std::shared_ptr<ir::PointerType> type_expression);
            std::shared_ptr<ArrayTypeEntry>    add_array_type_entry  (std::shared_ptr<ir::ArrayType>   type_expression);
            std::shared_ptr<CompoundTypeEntry> add_struct_type_entry (std::shared_ptr<ir::StructType>  type_expression);
            std::shared_ptr<CompoundTypeEntry> add_union_type_entry  (std::shared_ptr<ir::UnionType>   type_expression);
    };
}
