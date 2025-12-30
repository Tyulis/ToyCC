#include <memory>

#include "ir/postprocessor.h"
#include "ir/statement.h"

namespace toycc::ir {
    void PostProcessor::descope(std::shared_ptr<Scope> scope) {
        for (ssize_t position = 0; position < static_cast<ssize_t>(scope->statements.size()); position++) {
            std::shared_ptr<Statement> statement = scope->statements[position];

            // Keep function scopes
            if (statement->tag == stmt::Tag::FUNCTION) {
                std::shared_ptr<stmt::Function> function = std::static_pointer_cast<stmt::Function>(statement);
                descope(function->scope);
            } else if (statement->tag == stmt::Tag::BLOCK) {
                std::shared_ptr<stmt::Block> block = std::static_pointer_cast<stmt::Block>(statement);
                descope(block->scope);

                // Scope prefix to rename local variables but keep them identifiable
                std::string scope_prefix;
                if (statement->tag == stmt::Tag::FUNCTION)
                    scope_prefix = make_scope_prefix(std::static_pointer_cast<stmt::Function>(block)->declaration->name);
                else
                    scope_prefix = make_scope_prefix();

                // Move all statements of the block scope into the global scope
                std::ranges::copy(block->scope->statements, std::inserter(scope->statements, scope->statements.begin() + position + 1));

                // Move all declarations of the block scope into the global scope
                std::vector<std::shared_ptr<Declaration>> block_locals(block->scope->locals_list().begin(), block->scope->locals_list().end());
                for (std::shared_ptr<Declaration> declaration : block_locals) {
                    declaration->name = std::format("{}.{}", scope_prefix, declaration->name);
                    scope->add_local(declaration);
                }

                // Move all labels of the block scope into the global scope
                std::ranges::copy(block->scope->labels, std::inserter(scope->labels, scope->labels.begin()));

                scope->statements.erase(scope->statements.begin() + position);  // Remove the block statement
                position -= 1;                                                  // Point the next iteration to the first statement of the moved scope
            }
        }
    }
}
