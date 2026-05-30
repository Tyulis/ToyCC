#include <ranges>

#include "diagnostic.h"
#include "postprocessing/postprocessor.h"

// Rule : dereferences must be offsets from a pointer, not indirections
// So after this, all dereferences are POINTER[...][...]
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

    Operand PostProcessor::split_operand_indirections(Operand operand, std::shared_ptr<Scope> scope) {
        // First, split indirections recursively in indices
        for (Operand& index : operand.indices)
            split_operand_indirections(index, scope);

        if (operand.indices.size() <= 1)
            return operand;

        // Then split indirections : if any inner type is a pointer, split it
        return split_indirection_chains(operand, scope);
    }

    Operand PostProcessor::split_indirection_chains(Operand dereference, std::shared_ptr<Scope> scope) {
        // At this point, the top-level pointer is always a POINTER type
        if (dereference.base_type()->category != TypeCategory::POINTER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid condition : the top-level pointer should be a POINTER type", dereference.location);

        std::shared_ptr<Type> pointer_type = dereference.base_type();
        for (const auto& [level, index] : std::ranges::enumerate_view(dereference.indices)) {
            std::shared_ptr<Type> referenced_type = pointer_type->dereference(index.as_index(), index.location);
            if (referenced_type->category == TypeCategory::POINTER) {
                std::shared_ptr<Declaration> intermediate_pointer = declare_temporary(scope, referenced_type, index.location);
                const Operand intermediate_dereference = {dereference.value, dereference.location, {dereference.indices.begin(), dereference.indices.begin() + level + 1}};
                scope->add_statement(Statement::make_unary_operation(dereference.location, StatementTag::COPY, intermediate_dereference, intermediate_pointer));
                Operand lower_pointer = {intermediate_pointer, dereference.location, {dereference.indices.begin() + level + 1, dereference.indices.end()}};
                return split_indirection_chains(lower_pointer, scope);
            }
            pointer_type = referenced_type;
        }

        return dereference;
    }
}
