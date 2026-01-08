#include "ir/postprocessor.h"

namespace toycc::ir {
    // Reduce all types to their raw storage type
    void PostProcessor::detype(std::shared_ptr<Scope> scope) {
        // Step 1 : Detype statements
        for (Statement& statement : scope->statements) {
            // Recursively detype subblocks
            if (statement.block.get() != nullptr)
                detype(statement.block);

            // Detype all constants
            if (statement.output.has_value())
                detype_operand(statement.output.value());
            for (Operand& input : statement.inputs)
                detype_operand(input);
        }

        // Step 2 : Detype all declarations (after statements, we still need semantic pointer types for the index -> increment conversion)
        for (std::shared_ptr<Declaration> decl : scope->locals_list())
            decl->type = decl->type->storage_type();

        scope->clear_types();  // After that, we won't need to resolve any type names
    }

    void PostProcessor::detype_operand(Operand& operand) {
        if (operand.has_constant_base())
            operand.constant().type = operand.constant().type->storage_type();
        for (Operand& index : operand.indices)
            detype_operand(index);
    }
}
