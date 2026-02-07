#include <format>

#include "diagnostic.h"
#include "ir/postprocessor.h"

namespace toycc::ir {
    // Process pointer dereferences and array indices to flatten multi-dimensional and dynamic indexing, and resolve all array indices to static offsets
    void PostProcessor::dereference(std::shared_ptr<Scope> scope) {
        std::vector<Statement> original_statements = scope->statements;
        scope->statements.clear();

        for (Statement& statement : original_statements) {
            if (statement.block.get() != nullptr) {
                dereference(statement.block);
            } else {
                if (statement.output.has_value())
                    statement.output = dereference_operand(*statement.output, scope);

                for (auto it = statement.inputs.begin(); it != statement.inputs.end(); it++)
                    *it = dereference_operand(*it, scope);
            }

            scope->statements.push_back(statement);
        }
    }

    Operand PostProcessor::dereference_operand(const Operand& original, std::shared_ptr<Scope> scope) {
        if (original.indices.empty())
            return original;

        Operand result = original;

        do {
            std::shared_ptr<Type> pointer_type = result.base_type()->dequalify();
            if (pointer_type->category == TypeCategory::POINTER || pointer_type->category == TypeCategory::ARRAY)
                result = resolve_first_index(result, scope);
            else throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't dereference object of type `{}`", pointer_type->text()), original.location);

            if (result.indices.size() > 1)
                result = dereference_first_index(result, scope);
        } while (result.indices.size() > 1);

        return result;
    }

    Operand PostProcessor::resolve_first_index(const Operand& original, std::shared_ptr<Scope> scope) {
        std::shared_ptr<Type> pointer_type = original.base_type()->dequalify();
        Operand index = dereference_operand(original.indices[0], scope);
        std::shared_ptr<Type> referenced_type = pointer_type->dereference(index.as_index(), original.location);

        // Fully dereference the index : after that, the index is either a constant or a variable
        if (index.is_dereference())
            index = dereference_first_index(index, scope);

        // Resolve variable indices to static constants
        Operand result = original;
        if (index.is_constant()) {
            const Constant& constant_index = index.constant();
            if (!constant_index.is_integer())
                throw Diagnostic(DiagnosticLevel::ERROR, "Array indices must be integers", original.location);

            IntegerConstant offset = constant_index.integer() * referenced_type->size(original.location);
            result.indices[0] = Constant {offset, constant_index.location, constant_index.type};
        } else if (index.is_variable()) {
            // Emit a multiplication to get from the index to an offset
            std::shared_ptr<Declaration> variable_index = index.declaration();
            Constant value_size(IntegerConstant(referenced_type->size(original.location)), original.location, variable_index->type);
            std::shared_ptr<Declaration> offset = declare_temporary(scope, variable_index->type, variable_index->location);
            scope->add_statement(Statement::make_binary_operation(original.location, StatementTag::MUL, variable_index, value_size, offset));

            // Then apply that variable offset to the pointer
            const Operand pointer = Operand {original.value, original.location, {original.indices[0]}};
            std::shared_ptr<Declaration> offset_pointer = declare_temporary(scope, pointer_type, original.location);
            scope->add_statement(Statement::make_binary_operation(original.location, StatementTag::ADD, pointer, offset, offset_pointer));

            // Now the lvalue is *(offset_pointer+0)
            std::vector<Operand> indices = {Constant {IntegerConstant(0), original.location, offset_type}};
            std::copy(original.indices.begin() + 1, original.indices.end(), std::back_inserter(indices));
            return {offset_pointer, original.location, indices, referenced_type};
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Labels are not supported as array indices", original.location);

        result.dereference_type = referenced_type;
        return result;
    }

    Operand PostProcessor::dereference_first_index(const Operand& operand, std::shared_ptr<Scope> scope) {
        const Operand& index = operand.indices[0];
        std::shared_ptr<Type> referenced_type = operand.base_type()->dereference(index.as_index(), operand.location);
        std::shared_ptr<Declaration> pointee = declare_temporary(scope, referenced_type, operand.location);
        const Operand reference = Operand {operand.value, operand.location, {index}, referenced_type};
        scope->add_statement(Statement::make_unary_operation(operand.location, StatementTag::COPY, reference, pointee));

        std::shared_ptr<Type> pointee_referenced_type = nullptr;
        if (pointee->type->category == TypeCategory::POINTER)
            pointee_referenced_type = pointee->type->dereference(index.as_index(), operand.location);
        return {pointee, operand.location, {operand.indices.begin() + 1, operand.indices.end()}, pointee_referenced_type};
    }
}
