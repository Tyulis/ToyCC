#pragma once

#include <string>

#include "output.h"
#include "ir/flow.h"
#include "ir/allocation.h"
#include "debug/debuginfo.h"
#include "gen/execmodel/x86_64/location.h"

namespace toycc::arch::x86_64 {
    using toycc::execmodel::x86_64::Location;

    using Allocation = ir::Allocation<Location>;

    // Stack frame object that automatically generates its frame push and pop code
    class StackFrame : public ir::StackFrame<Location> {
        using Parent = ir::StackFrame<Location>;

        public:
            StackFrame(const ir::Procedure& procedure, debug::DebugInfo& debuginfo);

            void move(std::shared_ptr<ir::Declaration> declaration, Location location);
            void copy(std::shared_ptr<ir::Declaration> declaration, Location location);
            void free(std::shared_ptr<ir::Declaration> declaration);

            std::unordered_set<Location> locate(const ir::Operand& operand) const;
            std::optional<Location> allocate(const std::unordered_set<Location>& locations) const;
            std::unordered_set<std::shared_ptr<ir::Declaration>> allocated_variables() const;

            bool is_free(Location location) const;
            bool any_free(const std::unordered_set<Location>& locations) const;

            std::shared_ptr<ir::Declaration> declare_intermediate(std::shared_ptr<ir::Type> type, CodeLocation code_location);
            void flush_intermediates();
            void load_parameters();
            void enter_block(std::shared_ptr<ir::BasicBlock> block, bool is_last);
            void end();

            void label(std::string name);
            void statement(std::string code);
            void directive(std::string code);
            void comment(std::string content);
            void debug(std::string content);
            void linebreak();
            std::string str() const;
            std::string dump() const;

            std::shared_ptr<ir::BasicBlock> current_block;
            bool is_last_block = false;

        private:
            std::string name;
            size_t unique_id = 0;
            std::unordered_set<std::shared_ptr<ir::Declaration>> intermediates;
            CodeOutput output;

            debug::DebugInfo& debuginfo;

            std::string dump_allocations() const;

            bool emit_instruction_label = false;
            size_t instruction_index = 0;
            std::string get_instruction_label() const;  // Only get the name of the current instruction label
            std::string use_instruction_label();        // Get the name and set `emit_instruction_label`

            bool is_debug_variable(std::shared_ptr<ir::Declaration> declaration);
            debug::AssemblyData debug_location(std::shared_ptr<ir::Declaration> declaration, Location location);
    };

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code);
}
