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
        for (size_t level = 0; level < operand.indices.size() - 1; level++) {
            const Operand& index = operand.indices[level];
            std::shared_ptr<Type> referenced_type = pointer_type->dereference(index.as_index(), index.location);

            if (referenced_type->category == TypeCategory::POINTER) {
                std::shared_ptr<Declaration> pointee = declare_temporary(scope, referenced_type, index.location);
                const Operand reference = Operand {pointer.value, pointer.location, {operand.indices.begin() + top_level, operand.indices.begin() + level + 1}};
                scope->add_statement(Statement::make_unary_operation(operand.location, StatementTag::COPY, reference, pointee));
                pointer = Operand {pointee, operand.location, {operand.indices.begin() + level + 1, operand.indices.end()}};
                top_level = level + 1;
                break;
            }

            pointer_type = referenced_type;
        }

        return pointer;
    }
}
