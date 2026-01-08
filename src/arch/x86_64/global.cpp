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
            std::shared_ptr<LocalBlock> fallthrough = nullptr;
            std::shared_ptr<LocalBlock> non_fallthrough = nullptr;
            bool goes_to_exit = false;

            for (const FlowGraph::Edge& transition : transitions) {
                if (visited.contains(transition.exit))
                    continue;

                if (transition.exit == procedure.exit_block)
                    goes_to_exit = true;

                if (transition.attr == FlowType::FALLTHROUGH) {
                    fallthrough = transition.exit;
                    break;
                } else {
                    non_fallthrough = transition.exit;
                }
            }

            if      (fallthrough.get()     != nullptr)  current_block = fallthrough;
            else if (non_fallthrough.get() != nullptr)  current_block = non_fallthrough;
            else if (goes_to_exit)                      break;
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Found a dead end local block");
        }

        output << frame;
    }

    // FIXME : Trivial implementation for now
    void CodeGenerator::generate_local_block(StackFrame& frame, std::shared_ptr<LocalBlock> block) {
        if (block->label.has_value() && block->label->name != frame.procedure.declaration->name)
            frame.output.label(block->label->name);

        // FIXME : For now use any topological sort, this should optimize for minimal graph cutwidth
        std::vector<std::shared_ptr<DependencyNode>> ordered_nodes = block->dependencies.topological_sort();
        for (std::shared_ptr<DependencyNode> node : ordered_nodes)
            if (node->is_statement())
                generate_statement(frame, node->statement());

        // FIXME : At least for now, move all output variables to memory at the end of the block
        for (std::shared_ptr<DependencyNode> output : block->dependencies.sinks()) {
            if (!output->is_value())
                continue;

            FlowGraph::NodeSet next_blocks = frame.procedure.blocks.next_nodes(block);
            const bool only_returns = (next_blocks.size() == 1) && (*next_blocks.begin() == frame.procedure.exit_block);

            if (frame.procedure.globals.contains(output->declaration()))
                move_variable(frame, output->declaration(), LOC::STATIC, ordered_nodes.back()->location());
            else if (only_returns)  // It's only useful to store local variables when that block doesn't unconditionally exit
                move_variable(frame, output->declaration(), LOC::STACK, ordered_nodes.back()->location());
        }
    }
}
