#include "diagnostic.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/allocation.h"
#include "ir/flow.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_translation_unit(CodeOutput& output, const ir::TranslationUnit& unit) {
        for (std::shared_ptr<ir::Declaration> declaration : unit.globals) {
            if (declaration->type->category == ir::TypeCategory::FUNCTION)
                continue;

            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Global declarations other than functions are not implemented", declaration->location);
        }

        for (const auto& [name, procedure] : unit.procedures)
            generate_procedure(output, procedure, unit.globals);
    }

    void CodeGenerator::generate_procedure(CodeOutput& output, const ir::Procedure& procedure, const std::unordered_set<std::shared_ptr<ir::Declaration>>& globals) {
        // Generate the function symbol
        output.directive(std::format(".globl {}", procedure.declaration->name));
        output.directive(std::format(".type {}, @function", procedure.declaration->name));
        output.label(procedure.declaration->name);

        // Then the actual code
        StackFrame frame(procedure);

        ir::FlowGraph remaining_blocks = procedure.blocks;
        std::shared_ptr<ir::BasicBlock> current_block = procedure.entry_block;
        while (current_block != procedure.exit_block) {
            const ir::FlowGraph::EdgeSet transitions = remaining_blocks.out_edges(current_block);
            remaining_blocks.pop_node(current_block);

            frame.enter_block(current_block, remaining_blocks.nof_nodes() <= 1);
            generate_basic_block(frame, current_block, globals);

            // Only the exit block remains -> exit
            if (remaining_blocks.nof_nodes() <= 1)
                break;

            // Load the parameters for the first inner block
            if (current_block == procedure.entry_block)
                frame.load_parameters();

            // Always prioritize fallthrough
            std::shared_ptr<ir::BasicBlock> fallthrough = nullptr;
            std::shared_ptr<ir::BasicBlock> non_fallthrough = nullptr;

            for (const ir::FlowGraph::Edge& transition : transitions) {
                if (transition.attr == ir::FlowType::FALLTHROUGH) {
                    fallthrough = transition.exit;
                    break;
                } else if (transition.exit != procedure.exit_block) {
                    non_fallthrough = transition.exit;
                }
            }

            // This block links to yet non-generated blocks -> proceed with those
            if (fallthrough.get() != nullptr) {
                current_block = fallthrough;
                continue;
            } else if (non_fallthrough.get() != nullptr) {
                current_block = non_fallthrough;
                continue;
            }

            // No link to other blocks : find a source node regarding fallthrough.
            // Fallthrough links always make a directed acyclic graph so there's always one
            for (std::shared_ptr<ir::BasicBlock> block : remaining_blocks.nodes()) {
                if (block == procedure.exit_block)
                    continue;

                bool has_fallthrough_entry = false;
                for (const ir::FlowGraph::Edge& entry_transition : remaining_blocks.in_edges(block)) {
                    if (entry_transition.attr == ir::FlowType::FALLTHROUGH) {
                        has_fallthrough_entry = true;
                        break;
                    }
                }

                if (!has_fallthrough_entry) {
                    current_block = block;
                    break;
                }
            }
        }

        output << frame;
    }
}
