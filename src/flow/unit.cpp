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
            globals[variable] = {};
            if (!(variable->storage & ir::INTERNAL_STORAGE) && variable->type->category != ir::TypeCategory::FUNCTION)
                global_decls.insert(variable);
        }

        for (const ir::Statement& statement : global_scope->statements) {
            switch (statement.tag) {
                case ir::StatementTag::FUNCTION: {
                    std::shared_ptr<ir::Declaration> function = statement.output->declaration();
                    procedures[function->name] = Procedure {statement, globals, unique_id};
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

                case ir::StatementTag::COPY: {
                    if (!statement.inputs[0].is_constant())
                        throw Diagnostic(DiagnosticLevel::ERROR, "Global initializers must be constants", statement.location);
                    if (!statement.output->is_variable())
                        throw Diagnostic(DiagnosticLevel::ERROR, "Global initializers must be assigned to global variables", statement.location);

                    auto found = globals.find(statement.output->declaration());
                    if (found == globals.end())
                        throw Diagnostic(DiagnosticLevel::ERROR, "Global initializers must be assigned to global variables", statement.location);

                    found->second = statement.inputs[0].constant();
                    break;
                }
                [[fallthrough]];

                default:
                    global_block->add_statement(statement, global_decls);
            }
        }
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
