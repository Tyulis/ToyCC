#include <filesystem>

#include "config.h"
#include "debug/expression.h"
#include "debug/unit.h"
#include "diagnostic.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/allocation.h"
#include "ir/declaration.h"
#include "ir/flow.h"
#include "util/strings.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_translation_unit(CodeOutput& output, const ir::TranslationUnit& unit) {
        debug::CompilationUnit debuginfo(unit.working_directory, unit.filename, config::debug::format);

        // Emit the data sections for global variables
        CodeOutput code;
        generate_global_declarations(code, unit.globals, debuginfo);

        // Emit the code section (.text)
        code.directive(".text");  // Now that the data has been handled, all that remains is code
        debuginfo.begin_text(code);
        for (const auto& [name, procedure] : unit.procedures)
            generate_procedure(code, procedure, debuginfo);
        debuginfo.end_text(code);

        // Wrap everything in debug info
        output.directive(std::format(".file \"{}\"", std::filesystem::path(unit.filename).filename().string()));
        debuginfo.emit_filenos(output);
        output << code.str();
        debuginfo.emit_debug_sections(output);
    }

    void CodeGenerator::generate_procedure(CodeOutput& output, const ir::Procedure& procedure, debug::CompilationUnit& debuginfo) {
        StackFrame frame(procedure, debuginfo);
        std::shared_ptr<ir::Declaration> declaration = procedure.declaration;
        auto debug_scope = debuginfo.push_auto(debuginfo.procedure(procedure));

        ir::FlowGraph remaining_blocks = procedure.blocks;
        std::shared_ptr<ir::BasicBlock> current_block = procedure.entry_block;
        while (current_block != procedure.exit_block) {
            const ir::FlowGraph::EdgeSet transitions = remaining_blocks.out_edges(current_block);
            remaining_blocks.pop_node(current_block);

            frame.enter_block(current_block, remaining_blocks.nof_nodes() <= 1);
            generate_basic_block(frame, current_block, debuginfo);

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

        frame.end();

        output << frame;
        output.label(procedure.end_label());
    }

    void CodeGenerator::generate_global_declarations(CodeOutput& output, const ir::GlobalMap& globals, debug::CompilationUnit& debuginfo) {
        std::unordered_set<std::shared_ptr<ir::Declaration>> uninitialized_globals;
        ir::GlobalMap rw_globals;
        ir::GlobalMap ro_globals;

        for (const auto& [declaration, initializer] : globals) {
            if (declaration->storage & ir::StorageClass::EXTERN)
                continue;
            if (declaration->type->storage_category() == ir::TypeCategory::FUNCTION)
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
            generate_uninitialized_globals(output, uninitialized_globals, debuginfo);
        if (!rw_globals.empty())
            generate_readwrite_globals(output, rw_globals, debuginfo);
        if (!ro_globals.empty())
            generate_readonly_globals(output, ro_globals, debuginfo);
    }

    void CodeGenerator::generate_uninitialized_globals(CodeOutput& output, const std::unordered_set<std::shared_ptr<ir::Declaration>>& globals, debug::CompilationUnit& debuginfo) {
        output.directive(".bss");
        for (std::shared_ptr<ir::Declaration> declaration : globals) {
            generate_global_declaration(output, declaration, debuginfo);
            output.directive(std::format(".zero {}", declaration->type->size(declaration->location)));
        }
    }

    void CodeGenerator::generate_readwrite_globals(CodeOutput& output, const ir::GlobalMap& globals, debug::CompilationUnit& debuginfo) {
        output.directive(".data");
        for (const auto& [declaration, value] : globals) {
            generate_global_declaration(output, declaration, debuginfo);
            generate_global_value(output, value.value());
        }
    }

    void CodeGenerator::generate_readonly_globals(CodeOutput& output, const ir::GlobalMap& globals, debug::CompilationUnit& debuginfo) {
        output.directive(".rodata");
        for (const auto& [declaration, value] : globals) {
            generate_global_declaration(output, declaration, debuginfo);
            generate_global_value(output, value.value());
        }
    }

    // Generate the symbol for a global declaration, do not fill it
    void CodeGenerator::generate_global_declaration(CodeOutput& output, std::shared_ptr<ir::Declaration> variable, debug::CompilationUnit& debuginfo) {
        if (variable->storage & ir::StorageClass::EXTERN)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "`extern` declarations should not be generated", variable->location);

        if (!(variable->storage & ir::StorageClass::STATIC))
            output.directive(std::format(".globl {}", variable->name));

        output.directive(std::format(".type {}, @object", variable->name));
        output.directive(std::format(".size {}, {}", variable->name, variable->type->size(variable->location)));
        output.directive(std::format(".align {}", variable->type->alignment(variable->location)));
        output.label(variable->name);

        debuginfo.append(debuginfo.variable(variable));
        debuginfo.loclists.set_default(variable, debuginfo.expr().address(variable->name).encode());
    }

    // Set the actual value of a global declaration
    void CodeGenerator::generate_global_value(CodeOutput& output, const ir::Constant& value) {
        switch (value.tag()) {
            case ir::Constant::INTEGER:
                switch (value.type->size(value.location)) {
                    case 1:  output.directive(".byte "  + dump(value.integer()));  break;
                    case 2:  output.directive(".short " + dump(value.integer()));  break;
                    case 4:  output.directive(".long "  + dump(value.integer()));  break;
                    case 8:  output.directive(".quad "  + dump(value.integer()));  break;
                    default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Global integers of size {} are not supported", value.type->size(value.location)), value.location);
                }
                break;

            case ir::Constant::FLOAT:
                switch (value.type->size(value.location)) {
                    case 4:  output.directive(".single " + dump(value.floating_point()));  break;
                    case 8:  output.directive(".double " + dump(value.floating_point()));  break;
                    default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Global floats of size {} are not supported", value.type->size(value.location)), value.location);
                }
                break;

            case ir::Constant::STRING:
                output.directive(std::format(".string \"{}\"", value.string()));
                break;
        }
    }
}
