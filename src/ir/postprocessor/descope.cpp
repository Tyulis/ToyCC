#include <memory>

#include "ir/postprocessor.h"
#include "ir/statement.h"
#include "util/strings.h"

namespace toycc::ir {
    void PostProcessor::descope(std::shared_ptr<Scope> scope) {
        for (ssize_t position = 0; position < static_cast<ssize_t>(scope->statements.size()); position++) {
            std::shared_ptr<Statement> statement = scope->statements[position];

            // Keep function scopes
            if (statement->tag == StatementTag::FUNCTION) {
                descope(statement->block);

                // Prefix and move static declarations into the global scope
                std::string scope_prefix = make_scope_prefix(statement->output->base.declaration()->name);  // Scope prefix to rename local variables but keep them identifiable

                std::vector<std::shared_ptr<Declaration>> function_locals(statement->block->locals_list().begin(), statement->block->locals_list().end());
                for (std::shared_ptr<Declaration> declaration : function_locals) {
                    if (declaration->storage & StorageClass::STATIC) {
                        declaration->name = std::format("{}{}", scope_prefix, ltrim(declaration->name, "."));
                        scope->add_local(statement->block->pop_local(declaration->name));
                    }
                }
            }

            // Integrate block scopes
            else if (statement->tag == StatementTag::BLOCK) {
                descope(statement->block);

                std::string scope_prefix = make_scope_prefix();  // Scope prefix to rename local variables but keep them identifiable

                // Move all statements of the block scope into the global scope
                std::ranges::copy(statement->block->statements, std::inserter(scope->statements, scope->statements.begin() + position + 1));

                // Move all declarations of the block scope into the global scope
                for (std::shared_ptr<Declaration> declaration : statement->block->locals_list()) {
                    declaration->name = std::format("{}{}", scope_prefix, ltrim(declaration->name, "."));
                    scope->add_local(declaration);
                }

                // Move all labels of the block scope into the global scope
                for (std::shared_ptr<Label> label : statement->block->labels)
                    scope->add_label(label);

                scope->statements.erase(scope->statements.begin() + position);  // Remove the block statement
                position -= 1;                                                  // Point the next iteration to the first statement of the moved scope
            }
        }
    }
}
