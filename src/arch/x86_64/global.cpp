#include <algorithm>

#include "diagnostic.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_translation_unit(CodeOutput& output, const TranslationUnit& unit) {
        for (const auto& [name, declaration] : unit.globals) {
            if (declaration->type->category == TypeCategory::FUNCTION)
                continue;

            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Global declarations other than functions are not implemented", declaration->location);
        }

        for (const auto& [name, procedure] : unit.procedures)
            generate_procedure(output, procedure);
    }

    void CodeGenerator::generate_procedure(CodeOutput& output, const Procedure& procedure) {
        // Generate the function symbol
        output.directive(std::format(".globl {}", procedure.declaration->name));
        output.directive(std::format(".type {}, @function", procedure.declaration->name));
        output.label(procedure.declaration->name);

        // Then the actual code
        StackFrame frame(procedure);
        std::shared_ptr<LocalBlock> current_block = procedure.entry_block;
        std::unordered_set<std::shared_ptr<LocalBlock>> visited;
        while (current_block != procedure.exit_block) {
            generate_local_block(frame, current_block);
            visited.insert(current_block);
            FlowGraph::EdgeSet transitions = procedure.blocks.out_edges(current_block);

            // Always prioritize fallthrough
            for (const FlowGraph::Edge& transition : transitions) {
                if (transition.attr == FlowType::FALLTHROUGH && !visited.contains(transition.exit)) {
                    current_block = transition.exit;
                    continue;
                }
            }

            // FIXME : What to do then ? For now take the first block
            for (const FlowGraph::Edge& transition : transitions) {
                if (!visited.contains(transition.exit)) {
                    current_block = transition.exit;
                    continue;
                }
            }
        }

        output << frame;
    }

    // FIXME : Trivial implementation for now
    void CodeGenerator::generate_local_block(StackFrame& frame, std::shared_ptr<LocalBlock> block) {
        std::vector<std::shared_ptr<Statement>> ordered_statements = block->statements.topological_sort();
        for (std::shared_ptr<Statement> statement : ordered_statements)
            generate_statement(frame, statement);
    }
}
