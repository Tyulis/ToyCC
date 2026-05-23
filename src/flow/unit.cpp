#include "config.h"
#include "diagnostic.h"
#include "flow/unit.h"

namespace toycc::flow {
    // -------- TranslationUnit
    TranslationUnit::TranslationUnit(std::shared_ptr<ir::Scope> global_scope, std::string working_directory, std::string filename)
        : working_directory(working_directory), filename(filename), unique_id(std::make_shared<size_t>(0))
    {
        global_block = std::make_shared<BasicBlock> (BasicBlockType::INNER, unique_id);

        std::unordered_set<std::shared_ptr<ir::Declaration>> global_decls;
        for (std::shared_ptr<ir::Declaration> variable : global_scope->locals_list()) {
            globals[variable] = std::unexpected(ValueStatus::UNINITIALIZED);
            if (!(variable->storage & ir::INTERNAL_STORAGE) && variable->type->category != ir::TypeCategory::FUNCTION)
                global_decls.insert(variable);
        }

        for (const ir::Statement& statement : global_scope->statements) {
            switch (statement.tag) {
                case ir::StatementTag::FUNCTION: {
                    std::shared_ptr<ir::Declaration> function = statement.output->declaration();
                    procedures[function->name] = Procedure {statement, {}, unique_id};
                    break;
                }

                // The global scope must be all constexpr and a single basic block, so no jumps, no calls
                case ir::StatementTag::BLOCK:
                case ir::StatementTag::JUMP:
                case ir::StatementTag::JUMP_IF_TRUE:
                case ir::StatementTag::JUMP_IF_FALSE:
                case ir::StatementTag::RETURN:
                case ir::StatementTag::RETURN_VAL:
                case ir::StatementTag::MARKER:
                case ir::StatementTag::CALL:
                    throw Diagnostic(DiagnosticLevel::ERROR, std::format("{} can't be a global statement", statement.ir_code()), statement.location);

                default:
                    global_block->add_statement(statement, global_decls);
            }
        }

        // Use the constant folding algorithm to evaluate constant expressions in the global scope
        global_block->opt_constant_folding();
        for (const auto& [variable, value] : global_block->output_values())
            globals[variable] = value;

        // Check error conditions
        for (const auto& [variable, value] : globals) {
            if (value.has_value())
                continue;  // The global variable has a compile-time value

            switch (value.error()) {
                case ValueStatus::UNINITIALIZED:
                    if (variable->type->is_const())
                        throw Diagnostic(DiagnosticLevel::ERROR, std::format("Global constant `{}` has no value", variable->name), variable->location);
                    break;

                case ValueStatus::UNKNOWN:
                    throw Diagnostic(DiagnosticLevel::ERROR, std::format("Global variable `{}`'s initializer is not a constant expression", variable->name), variable->location);
            }
        }
    }


    // -------- Optimization passes
    // Apply the graph optimization passes enabled in the config
    void TranslationUnit::optimize() {
        // The split-intermediates pass is useful to optimize constant folding
        if (config::optimization::split_intermediates)
            opt_split_intermediates();
        if (config::optimization::constant_folding)
            opt_constant_folding();
    }


    std::string TranslationUnit::dot_graph() const {
        std::stringstream dot;
        dot << "digraph {\n";
        dot << "compound = true;\n";
        for (const auto& [name, procedure] : procedures)
            procedure.dot_subgraph(dot);
        dot << "}\n";
        return dot.str();
    }
}
