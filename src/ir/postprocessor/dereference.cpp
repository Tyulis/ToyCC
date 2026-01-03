#include <format>

#include "diagnostic.h"
#include "ir/postprocessor.h"

namespace toycc::ir {
    // Process pointer dereferences and array indices to flatten multi-dimensional and dynamic indexing, and resolve all array indices to static offsets
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
        if (original.indices.empty())
            return original;

        LValue result = original;

        do {
            std::shared_ptr<Type> pointer_type = result.base.type()->dequalify();
            if (pointer_type->category == TypeCategory::POINTER) {
                const auto [pointer, index] = resolve_index(result.base, result.indices[0], scope, original.location);
                result.base = pointer;
                result.indices[0] = index;
            } else throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't dereference object of type `{}`", pointer_type->text()), original.location);

            if (result.indices.size() > 1) {
                std::shared_ptr<Type> referenced_type = pointer_type->dereference(original.location);
                std::shared_ptr<Declaration> pointee = declare_temporary(scope, referenced_type, original.location);
                scope->add_statement(Statement::make_load(original.location, LValue {result.base, original.location, {result.indices[0]}}, pointee));
                result = LValue {pointee, original.location, {result.indices.begin() + 1, result.indices.end()}};
            } else break;
        } while (result.indices.size() > 1);

        return result;
    }

    std::tuple<RValue, RValue> PostProcessor::resolve_index(const RValue& pointer, const RValue& index, std::shared_ptr<Scope> scope, CodeLocation location) {
        std::shared_ptr<Type> pointer_type = pointer.type()->dequalify();
        std::shared_ptr<Type> referenced_type = pointer_type->dereference(location);

        // Resolve variable indices to static constants
        if (index.is_constant()) {
            const Constant& constant_index = index.constant();
            if (!constant_index.is_integer())
                throw Diagnostic(DiagnosticLevel::ERROR, "Array indices must be integers", location);

            IntegerConstant offset = constant_index.integer() * referenced_type->size(location);
            return {pointer, Constant {offset, constant_index.location, constant_index.type}};
        } else {
            // Emit a multiplication to get from the index to an offset
            std::shared_ptr<Declaration> variable_index = index.declaration();
            Constant value_size(IntegerConstant(referenced_type->size(location)), location, variable_index->type);
            std::shared_ptr<Declaration> offset = declare_temporary(scope, variable_index->type, variable_index->location);
            scope->add_statement(Statement::make_binary_operation(location, StatementTag::MUL, variable_index, value_size, offset));

            // Then apply that variable offset to the pointer
            std::shared_ptr<Declaration> offset_pointer = declare_temporary(scope, pointer_type, location);
            scope->add_statement(Statement::make_binary_operation(location, StatementTag::PLUS, pointer, offset, offset_pointer));

            // Now the lvalue is *(offset_pointer+0)
            return {offset_pointer, Constant {IntegerConstant(0), location, offset_type}};
        }
    }
}
