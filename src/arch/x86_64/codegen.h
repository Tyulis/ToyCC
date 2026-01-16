#pragma once

#include "ir/flow.h"
#include "arch/codegen.h"
#include "arch/x86_64/output.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    class CodeGenerator : public toycc::arch::CodeGenerator {
        public:
            // -------- Exported methods -> arch/x86_64/exports.cpp
            CodeGenerator(const ir::TranslationUnit& unit);
            virtual void operator() (std::ostream& output) override;

        private:
            // -------- Global constructs -> arch/x86_64/global.cpp
            void generate_translation_unit(CodeOutput& output, const ir::TranslationUnit& unit);
            void generate_procedure(CodeOutput& output, const ir::Procedure& procedure, const std::unordered_set<std::shared_ptr<ir::Declaration>>& globals);

            // -------- Basic block generation -> arch/x86_64/block.cpp
            void generate_basic_block(StackFrame& frame, std::shared_ptr<ir::BasicBlock> block, const std::unordered_set<std::shared_ptr<ir::Declaration>>& globals);
            void code_generation_iteration(StackFrame& frame, ir::DependencyGraph& graph, const std::vector<GroupMatch>& group_matches);

            std::vector<GroupMatch> find_entry_matches(const ir::DependencyGraph& graph, const std::vector<GroupMatch>& group_matches);
            const TranslationMatch& select_translation(const std::vector<TranslationMatch>& matches);
            void clear_processed_statements(ir::DependencyGraph& graph, const GroupMatch& match);
            void clear_obsolete_matches(std::vector<GroupMatch>& group_matches, const ir::DependencyGraph& graph);

            // -------- Symbol management -> arch/x86_64/symbols.cpp
            size_t unique_id = 0;
            std::string anonymous_identifier();
    };
}
