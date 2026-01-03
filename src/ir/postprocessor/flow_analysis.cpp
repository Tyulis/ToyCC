#include "ir/postprocessor.h"

namespace toycc::ir {
    TranslationUnit PostProcessor::analyse_flow(std::shared_ptr<Scope> global_scope) {
        TranslationUnit unit;

        // After descoping, only procedures and static declarations remain
        for (std::shared_ptr<Declaration> declaration : global_scope->locals_list())
            unit.globals[declaration->name] = declaration;

        for (std::shared_ptr<Statement> statement : global_scope->statements) {
            if (statement->tag == StatementTag::FUNCTION) {
                std::shared_ptr<Declaration> function = statement->output->declaration();
                unit.procedures.emplace(function->name, statement);
            }
        }

        return unit;
    }
}
