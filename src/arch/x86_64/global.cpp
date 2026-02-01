#include "diagnostic.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/allocation.h"
#include "ir/declaration.h"
#include "ir/flow.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_translation_unit(CodeOutput& output, const ir::TranslationUnit& unit) {
        generate_global_declarations(output, unit.globals);

        output.directive(".text");  // Now that the data has been handled, all that remains is code
        for (const auto& [name, procedure] : unit.procedures)
            generate_procedure(output, procedure);
    }

    void CodeGenerator::generate_procedure(CodeOutput& output, const ir::Procedure& procedure) {
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
            generate_basic_block(frame, current_block);

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

    void CodeGenerator::generate_global_declarations(CodeOutput& output, const ir::GlobalMap& globals) {
        std::unordered_set<std::shared_ptr<ir::Declaration>> uninitialized_globals;
        ir::GlobalMap rw_globals;
        ir::GlobalMap ro_globals;

        for (const auto& [declaration, initializer] : globals) {
            if (declaration->storage & ir::StorageClass::EXTERN)
                continue;
            if (declaration->type->dequalify()->category == ir::TypeCategory::FUNCTION)
                continue;

            if (initializer.has_value()) {
                if (declaration->type->is_const())
                    ro_globals[declaration] = initializer;
                else
                    rw_globals[declaration] = initializer;
            } else {
                uninitialized_globals.insert(declaration);
            }
        }

        if (!uninitialized_globals.empty())
            generate_uninitialized_globals(output, uninitialized_globals);
        if (!rw_globals.empty())
            generate_readwrite_globals(output, rw_globals);
        if (!ro_globals.empty())
            generate_readonly_globals(output, ro_globals);
    }

    void CodeGenerator::generate_uninitialized_globals(CodeOutput& output, const std::unordered_set<std::shared_ptr<ir::Declaration>>& globals) {
        output.directive(".bss");
        for (std::shared_ptr<ir::Declaration> declaration : globals) {
            generate_global_declaration(output, declaration);
            output.directive(std::format(".zero {}", declaration->type->size(declaration->location)));
        }
    }

    void CodeGenerator::generate_readwrite_globals(CodeOutput&, const ir::GlobalMap&) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Global variables are not implemented");
    }

    void CodeGenerator::generate_readonly_globals(CodeOutput&, const ir::GlobalMap&) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Global constants are not implemented");
    }

    // Generate the symbol for a global declaration, do not fill it
    void CodeGenerator::generate_global_declaration(CodeOutput& output, std::shared_ptr<ir::Declaration> variable) {
        if (variable->storage & ir::StorageClass::EXTERN)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "`extern` declarations should not be generated", variable->location);

        if (!(variable->storage & ir::StorageClass::STATIC))
            output.directive(std::format(".globl {}", variable->name));

        output.directive(std::format(".type {}, @object", variable->name));
        output.directive(std::format(".size {}, {}", variable->name, variable->type->size(variable->location)));
        output.directive(std::format(".align {}", variable->type->alignment(variable->location)));
        output.label(variable->name);
    }
}
