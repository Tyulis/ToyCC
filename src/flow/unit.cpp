#include "diagnostic.h"
#include "flow/unit.h"

namespace toycc::flow {
    // -------- TranslationUnit
    TranslationUnit::TranslationUnit(std::shared_ptr<ir::Scope> global_scope, std::string working_directory, std::string filename)
        : working_directory(working_directory), filename(filename), unique_id(std::make_shared<size_t>(0))
    {
        // After descoping, only procedures and static declarations remain
        for (std::shared_ptr<ir::Declaration> declaration : global_scope->locals_list())
            globals[declaration] = {};

        for (const ir::Statement& statement : global_scope->statements) {
            switch (statement.tag) {
                case ir::StatementTag::FUNCTION: {
                    std::shared_ptr<ir::Declaration> function = statement.output->declaration();
                    procedures[function->name] = Procedure {statement, globals, unique_id};
                    break;
                }

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

                default:
                    throw Diagnostic(DiagnosticLevel::ERROR, std::format("{} can't be a global statement", statement.ir_code()), statement.location);
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
