#include "diagnostic.h"
#include "ir/postprocessor.h"
#include <variant>

namespace toycc::ir {
    // Reduce all types to their raw storage type
    void PostProcessor::detype(std::shared_ptr<Scope> scope) {
        // Step 1 : Detype statements
        std::vector<std::shared_ptr<Statement>> original_statements = scope->statements;
        scope->statements.clear();
        for (std::shared_ptr<Statement> statement : original_statements) {
            // Recursively detype subblocks
            if (statement->block.get() != nullptr)
                detype(statement->block);

            // Detype all constants
            std::vector<RValue*> rvalues;
            for (RValue& input : statement->inputs)  rvalues.push_back(&input);
            if (statement->lvalue_input.has_value()) {
                rvalues.push_back(&statement->lvalue_input->base);
                for (RValue& index : statement->lvalue_input->indices)
                    rvalues.push_back(&index);
            }
            if (statement->output.has_value()) {
                rvalues.push_back(&statement->output->base);
                for (RValue& index : statement->output->indices)
                    rvalues.push_back(&index);
            }

            for (RValue* rvalue : rvalues)
                if (rvalue->is_constant())
                    rvalue->constant().type = rvalue->constant().type->storage_type();

            // Convert array indices to pointer increments, using the newly detyped constants
            if (statement->lvalue_input.has_value())
                statement->lvalue_input = detype_lvalue(*statement->lvalue_input, scope);
            if (statement->output.has_value())
                statement->output = detype_lvalue(*statement->output, scope);

            scope->add_statement(statement);
        }

        // Step 2 : Detype all declarations (after statements, we still need semantic pointer types for the index -> increment conversion)
        for (std::shared_ptr<Declaration> decl : scope->locals_list())
            decl->type = decl->type->storage_type();

        scope->clear_types();  // After that, we won't need to resolve any type names
    }

    // Resolve the semantic array indices to increments in bytes relative to the pointer
    LValue PostProcessor::detype_lvalue(const LValue& original, std::shared_ptr<Scope> scope) {
        if (!original.is_dereference())  // Just an identifier, no problem
            return original;

        if (original.indices.size() > 1)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "All multi-level dereferences must be resolved before detyping", original.location);

        const RValue& base = original.base;
        const RValue& index = original.indices[0];

        std::shared_ptr<Type> referenced_type = base.type()->dereference(original.location);

        if (index.is_constant()) {
            // Just multiply the constant value
            const Constant& constant_index = index.constant();
            if (!std::holds_alternative<IntegerConstant>(constant_index.value))
                throw Diagnostic(DiagnosticLevel::ERROR, "Array indices must be integers", original.location);

            IntegerConstant increment = std::get<IntegerConstant>(constant_index.value) * referenced_type->size(original.location);
            return {base, original.location, {Constant {increment, constant_index.location, constant_index.type}}};
        } else {
            // Emit a multiplication for the index
            std::shared_ptr<Declaration> variable_index = index.declaration();
            std::shared_ptr<Declaration> increment = declare_temporary(scope, variable_index->type, variable_index->location);
            Constant value_size(IntegerConstant(referenced_type->size(original.location)), original.location, variable_index->type);
            scope->add_statement(Statement::make_binary_operation(original.location, StatementTag::MUL, variable_index, value_size, increment));
            return {base, original.location, {increment}};
        }
    }
}
