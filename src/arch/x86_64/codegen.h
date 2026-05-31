#pragma once

#include "output.h"
#include "flow/block.h"
#include "flow/procedure.h"
#include "flow/unit.h"
#include "arch/codegen.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"
#include "debug/debuginfo.h"

namespace toycc::arch::x86_64 {
    class CodeGenerator : public toycc::arch::CodeGenerator {
        public:
            // -------- Exported methods -> arch/x86_64/exports.cpp
            CodeGenerator(const flow::TranslationUnit& unit);
            virtual void operator() (std::ostream& output) override;

        private:
            // -------- Global constructs -> arch/x86_64/global.cpp
            void generate_translation_unit(CodeOutput& output, const flow::TranslationUnit& unit);
            void generate_global_declarations(CodeOutput& output, const flow::ConstantMap& globals, debug::DebugInfo& debuginfo);
            void generate_uninitialized_globals(CodeOutput& output, const std::unordered_set<std::shared_ptr<ir::Declaration>>& globals, debug::DebugInfo& debuginfo);
            void generate_readwrite_globals(CodeOutput& output, const flow::ConstantMap& globals, debug::DebugInfo& debuginfo);
            void generate_readonly_globals(CodeOutput& output, const flow::ConstantMap& globals, debug::DebugInfo& debuginfo);
            void generate_global_declaration(CodeOutput& output, std::shared_ptr<ir::Declaration> variable, debug::DebugInfo& debuginfo);
            void generate_global_value(CodeOutput& output, const ir::Constant& value);
            void generate_aggregate_global_value(CodeOutput& output, const ir::Constant& value);
            void generate_procedure(CodeOutput& output, const flow::Procedure& procedure, debug::DebugInfo& debuginfo);

            // -------- Basic block generation -> arch/x86_64/block.cpp
            void generate_basic_block(StackFrame& frame, std::shared_ptr<flow::BasicBlock> block, debug::DebugInfo& debuginfo);
            void code_generation_iteration(StackFrame& frame, flow::DependencyGraph& graph, const std::vector<GroupMatch>& group_matches, debug::DebugInfo& debuginfo);

            std::vector<GroupMatch> find_entry_matches(const flow::DependencyGraph& graph, const std::vector<GroupMatch>& group_matches);
            const TranslationMatch& select_translation(const StackFrame& frame, const std::vector<TranslationMatch>& matches);
            void emit_location_info(StackFrame& frame, const TranslationMatch& match, debug::DebugInfo& debuginfo);
            void clear_processed_statements(StackFrame& frame, flow::DependencyGraph& graph, const GroupMatch& match);
            void clear_obsolete_matches(std::vector<GroupMatch>& group_matches, const flow::DependencyGraph& graph);
            void flush_globals(StackFrame& frame);
            void flush_locals(StackFrame& frame, const flow::Procedure& procedure, std::shared_ptr<flow::BasicBlock> current_block, bool is_fallthrough);

            // -------- Symbol management -> arch/x86_64/symbols.cpp
            size_t unique_id = 0;
            std::string anonymous_identifier();
    };
}
