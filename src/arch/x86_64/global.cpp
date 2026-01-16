#include "diagnostic.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/allocation.h"

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
        std::shared_ptr<ir::BasicBlock> current_block = procedure.entry_block;
        std::unordered_set<std::shared_ptr<ir::BasicBlock>> visited;
        while (current_block != procedure.exit_block) {
            generate_basic_block(frame, current_block, globals);
            visited.insert(current_block);
            ir::FlowGraph::EdgeSet transitions = procedure.blocks.out_edges(current_block);

            // Always prioritize fallthrough
            std::shared_ptr<ir::BasicBlock> fallthrough = nullptr;
            std::shared_ptr<ir::BasicBlock> non_fallthrough = nullptr;
            bool goes_to_exit = false;

            for (const ir::FlowGraph::Edge& transition : transitions) {
                if (visited.contains(transition.exit))
                    continue;

                if (transition.exit == procedure.exit_block)
                    goes_to_exit = true;

                if (transition.attr == ir::FlowType::FALLTHROUGH) {
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
}
