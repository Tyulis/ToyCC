#include <filesystem>

#include "config.h"
#include "debug/expression.h"
#include "debug/debuginfo.h"
#include "diagnostic.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/allocation.h"
#include "ir/declaration.h"
#include "ir/flow.h"
#include "util/strings.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_translation_unit(CodeOutput& output, const ir::TranslationUnit& unit) {
        debug::DebugInfo debuginfo(unit.working_directory, unit.filename, config::debug::format);

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
        debuginfo.wrap_text(output, code.str());
    }

    void CodeGenerator::generate_procedure(CodeOutput& output, const ir::Procedure& procedure, debug::DebugInfo& debuginfo) {
        StackFrame frame(procedure, debuginfo);
        std::shared_ptr<ir::Declaration> declaration = procedure.declaration;
        auto debug_scope = debuginfo.push_auto(debuginfo.procedure(procedure));

        ir::FlowGraph remaining_blocks = procedure.blocks;
        std::shared_ptr<ir::BasicBlock> current_block = procedure.entry_block;
        while (current_block != procedure.exit_block) {
            const ir::FlowGraph::Edge next_transition = next_block_transition(procedure, remaining_blocks, current_block);
            remaining_blocks.pop_node(current_block);

            frame.enter_block(current_block, remaining_blocks.nof_nodes() <= 1);
            generate_basic_block(frame, current_block, debuginfo);

            // Load the parameters for the first inner block
            if (current_block == procedure.entry_block)
                frame.load_parameters();

            if (next_transition.exit != procedure.exit_block)
                flush_locals(frame);

            current_block = next_transition.exit;
        }

        frame.end();

        output << frame;
        output.label(procedure.end_label());
    }

    // Get the next block transition from the `current_block`
    // Must be called *before* popping the current block from the `remaining_blocks` graph
    ir::FlowGraph::Edge CodeGenerator::next_block_transition(const ir::Procedure& procedure, const ir::FlowGraph& remaining_blocks, std::shared_ptr<ir::BasicBlock> current_block) {
        // Always prioritize fallthrough
        std::optional<ir::FlowGraph::Edge> fallthrough = {};
        std::optional<ir::FlowGraph::Edge> non_fallthrough = {};

        for (const ir::FlowGraph::Edge& transition : remaining_blocks.out_edges(current_block)) {
            if (transition.attr == ir::FlowType::FALLTHROUGH) {
                fallthrough = transition;
                break;
            } else if (transition.exit != procedure.exit_block) {
                non_fallthrough = transition;
            }
        }

        // This block links to yet non-generated blocks -> proceed with those
        if (fallthrough.has_value())
            return fallthrough.value();
        else if (non_fallthrough.has_value())
            return non_fallthrough.value();

        // No link to other blocks : find a source node regarding fallthrough.
        // At least for now, to keep the semantics of conditional jumps, fallthrough links must always be represented by sequential basic blocks in the generated code
        // Fallthrough links are the sequential order of the original code, so they make a directed acyclic subgraph of the flow graph
        // So there is always a source node in that subgraph
        for (std::shared_ptr<ir::BasicBlock> block : remaining_blocks.nodes()) {
            if (block == procedure.exit_block || block == current_block)
                continue;

            bool has_fallthrough_entry = false;
            for (const ir::FlowGraph::Edge& entry_transition : remaining_blocks.in_edges(block)) {
                if (entry_transition.attr == ir::FlowType::FALLTHROUGH) {
                    has_fallthrough_entry = true;
                    break;
                }
            }

            // No fallthrough entry transition -> this is a source node of the fallthrough DAG
            if (!has_fallthrough_entry)
                return {current_block, block, ir::FlowType::UNRELATED};
        }

        // No other fallthrough paths remain -> this must be the end, transition to the exit block
        if (remaining_blocks.nof_nodes() == 2) {  // {current_block, exit_block}
            // Defensive check of the remaining blocks before blindly returning the exit block
            bool found_current_block = false, found_exit_block = false;
            for (std::shared_ptr<ir::BasicBlock> block : remaining_blocks.nodes()) {
                if      (block == current_block)         found_current_block = true;
                else if (block == procedure.exit_block)  found_exit_block    = true;
            }

            if (!found_current_block || !found_exit_block)
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The last basic block in the flow graph is not the exit block");

            return {current_block, procedure.exit_block, ir::FlowType::UNRELATED};
        }

        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The fallthrough subgraph is not a DAG");
    }

    void CodeGenerator::generate_global_declarations(CodeOutput& output, const ir::GlobalMap& globals, debug::DebugInfo& debuginfo) {
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

    void CodeGenerator::generate_uninitialized_globals(CodeOutput& output, const std::unordered_set<std::shared_ptr<ir::Declaration>>& globals, debug::DebugInfo& debuginfo) {
        output.directive(".bss");
        for (std::shared_ptr<ir::Declaration> declaration : globals) {
            generate_global_declaration(output, declaration, debuginfo);
            output.directive(std::format(".zero {}", declaration->type->size(declaration->location)));
        }
    }

    void CodeGenerator::generate_readwrite_globals(CodeOutput& output, const ir::GlobalMap& globals, debug::DebugInfo& debuginfo) {
        output.directive(".data");
        for (const auto& [declaration, value] : globals) {
            generate_global_declaration(output, declaration, debuginfo);
            generate_global_value(output, value.value());
        }
    }

    void CodeGenerator::generate_readonly_globals(CodeOutput& output, const ir::GlobalMap& globals, debug::DebugInfo& debuginfo) {
        output.directive(".rodata");
        for (const auto& [declaration, value] : globals) {
            generate_global_declaration(output, declaration, debuginfo);
            generate_global_value(output, value.value());
        }
    }

    // Generate the symbol for a global declaration, do not fill it
    void CodeGenerator::generate_global_declaration(CodeOutput& output, std::shared_ptr<ir::Declaration> variable, debug::DebugInfo& debuginfo) {
        if (variable->storage & ir::StorageClass::EXTERN)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "`extern` declarations should not be generated", variable->location);

        if (!(variable->storage & ir::StorageClass::STATIC))
            output.directive(std::format(".globl {}", variable->name));

        output.directive(std::format(".type {}, @object", variable->name));
        output.directive(std::format(".size {}, {}", variable->name, variable->type->size(variable->location)));
        output.directive(std::format(".align {}", variable->type->alignment(variable->location)));
        output.label(variable->name);

        debuginfo.append(debuginfo.variable(variable));
        debuginfo.variable(variable)->location.set_default(debug::Expression(debuginfo.format).address(variable->name).encode());
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
