#include "ir/postprocessor.h"

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

            scope->add_statement(statement);
        }

        // Step 2 : Detype all declarations (after statements, we still need semantic pointer types for the index -> increment conversion)
        for (std::shared_ptr<Declaration> decl : scope->locals_list())
            decl->type = decl->type->storage_type();

        scope->clear_types();  // After that, we won't need to resolve any type names
    }
}
