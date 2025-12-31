#include "ir/postprocessor.h"

namespace toycc::ir {
    // Reduce all types to their raw storage type
    void PostProcessor::detype(std::shared_ptr<Scope> scope) {
        for (std::shared_ptr<Declaration> decl : scope->locals_list())
            decl->type = decl->type->storage_type();

        for (std::shared_ptr<Statement> statement : scope->statements)
            if (statement->block.get() != nullptr)
                detype(statement->block);

        scope->clear_types();  // After that, we won't need to resolve any type names
    }
}
