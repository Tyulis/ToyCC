#include "diagnostic.h"
#include "ir/postprocessor.h"

namespace toycc::ir {
    void PostProcessor::split_indirections(std::shared_ptr<Scope> scope) {
        std::vector<Statement> original_statements = scope->statements;
        scope->statements.clear();

        for (Statement& statement : original_statements) {
            if (statement.block.get() != nullptr) {
                split_indirections(statement.block);
            } else {
                if (statement.output.has_value())
                    statement.output = split_operand_indirections(*statement.output, scope);

                for (auto it = statement.inputs.begin(); it != statement.inputs.end(); it++)
                    *it = split_operand_indirections(*it, scope);
            }

            scope->statements.push_back(statement);
        }
    }

    Operand PostProcessor::split_operand_indirections(const Operand& operand, std::shared_ptr<Scope> scope) {
        if (operand.indices.size() <= 1)
            return operand;

        Operand pointer = operand;
        std::shared_ptr<Type> pointer_type = pointer.base_type();
        size_t top_level = 0;
        for (size_t level = 0; level < operand.indices.size(); level++) {
            const Operand& index = operand.indices[level];

            switch (pointer_type->category) {
                case TypeCategory::ARRAY:
                case TypeCategory::STRUCT:
                case TypeCategory::UNION:
                    break;  // No indirection at this level, indices are still from the same pointer

                case TypeCategory::POINTER: {
                    std::shared_ptr<Type> referenced_type = pointer_type->dereference(index.as_index(), index.location);
                    std::shared_ptr<Declaration> pointee = declare_temporary(scope, referenced_type, index.location);
                    const Operand reference = Operand {operand.value, operand.location, {operand.indices.begin() + top_level, operand.indices.begin() + level}, referenced_type};
                    scope->add_statement(Statement::make_unary_operation(operand.location, StatementTag::COPY, reference, pointee));
                    pointer = Operand {pointee, operand.location, {operand.indices.begin() + level + 1, operand.indices.end()}};
                    top_level = level + 1;
                    break;
                }

                default: throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` can't be dereferenced", pointer_type->ir_code()), index.location);
            }

            pointer_type = pointer_type->dereference(index.as_index(), index.location);
        }

        return pointer;
    }
}
