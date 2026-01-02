#include <format>

#include "diagnostic.h"
#include "ir/postprocessor.h"

namespace toycc::ir {
    // Process pointer dereferences and array indices to flatten multi-dimensional indexing
    void PostProcessor::dereference(std::shared_ptr<Scope> scope) {
        const std::vector<std::shared_ptr<Statement>> original_statements = scope->statements;
        scope->statements.clear();

        for (std::shared_ptr<Statement> statement : original_statements) {
            if (statement->block.get() != nullptr) {
                dereference(statement->block);
            } else {
                if (statement->output.has_value())
                    statement->output = dereference_lvalue(*statement->output, scope);

                if (statement->lvalue_input.has_value())
                    statement->lvalue_input = dereference_lvalue(*statement->lvalue_input, scope);
            }

            scope->statements.push_back(statement);
        }
    }

    LValue PostProcessor::dereference_lvalue(const LValue& original, std::shared_ptr<Scope> scope) {
        if (original.indices.size() <= 1)
            return original;

        LValue result = original;
        do {
            std::shared_ptr<Type> pointer_type = result.base.type()->dequalify();

            switch (pointer_type->category) {
                case TypeCategory::POINTER: {
                    std::shared_ptr<Type> referenced_type = pointer_type->dereference(original.location);
                    std::shared_ptr<Declaration> pointee = declare_temporary(scope, referenced_type, original.location);
                    LValue pointer(result.base, original.location, {result.indices[0]});
                    scope->add_statement(Statement::make_load(original.location, pointer, pointee));
                    result = LValue {pointee, original.location, {result.indices.begin() + 1, result.indices.end()}};
                    break;
                }

                case TypeCategory::ARRAY: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Multi-dimensional array dereferences are not implemented", original.location);
                default: throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't dereference object of type `{}`", pointer_type->text()), original.location);
            }
        } while (result.indices.size() > 1);

        return result;
    }
}
