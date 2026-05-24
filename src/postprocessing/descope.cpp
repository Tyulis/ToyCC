#include <memory>
#include <unordered_map>

#include "ir/statement.h"
#include "postprocessing/postprocessor.h"
#include "util/strings.h"

namespace toycc::ir {
    void PostProcessor::descope(std::shared_ptr<Scope> scope) {
        // Rename labels to make them globally unique
        std::unordered_map<std::string, Label> original_labels = scope->labels;
        std::unordered_map<std::string, std::string> label_renames;
        scope->labels.clear();

        for (auto [name, label] : original_labels) {
            if (label.type == LabelType::NAMED) {
                const std::string new_name = anonymous_label();
                label.name = new_name;
                label_renames[name] = new_name;
                scope->labels[new_name] = label;
            } else {
                scope->labels[name] = label;
            }
        }

        // Descope statements
        for (ssize_t position = 0; position < static_cast<ssize_t>(scope->statements.size()); position++) {
            // Rename all label references
            Statement& statement_ref = scope->statements[position];
            for (Operand& operand : statement_ref.inputs) {
                if (operand.base_tag() == Operand::CONSTANT_BASE && operand.constant().tag() == Constant::POINTER && label_renames.contains(operand.constant().pointer().label)) {
                    const PointerConstant pointer = operand.constant().pointer();
                    operand = ir::Constant {PointerConstant {label_renames[pointer.label], pointer.offset}, operand.constant().location, operand.constant().type};
                }
            }

            if (statement_ref.output.has_value() && statement_ref.output->base_tag() == Operand::CONSTANT_BASE) {
                const Constant& constant = statement_ref.output->constant();
                if (constant.tag() == Constant::POINTER && label_renames.contains(constant.pointer().label))
                    statement_ref.output = ir::Constant {PointerConstant{label_renames[constant.pointer().label], constant.pointer().offset}, constant.location, constant.type};
            }

            Statement statement = scope->statements[position];

            // Keep function scopes
            if (statement.tag == StatementTag::FUNCTION) {
                descope(statement.block);

                // Prefix and move static declarations into the global scope
                std::string scope_prefix = make_scope_prefix(statement.output->declaration()->name);  // Scope prefix to rename local variables but keep them identifiable

                std::vector<std::shared_ptr<Declaration>> function_locals(statement.block->locals_list().begin(), statement.block->locals_list().end());
                for (std::shared_ptr<Declaration> declaration : function_locals) {
                    if (declaration->storage & StorageClass::STATIC) {
                        declaration->name = std::format("{}{}", scope_prefix, ltrim(declaration->name, "."));
                        scope->add_local(statement.block->pop_local(declaration->name));
                    }
                }
            }

            // Integrate block scopes
            else if (statement.tag == StatementTag::BLOCK) {
                descope(statement.block);

                std::string scope_prefix = make_scope_prefix();  // Scope prefix to rename local variables but keep them identifiable

                // Move all statements of the block scope into the global scope
                std::ranges::copy(statement.block->statements, std::inserter(scope->statements, scope->statements.begin() + position + 1));

                // Move all declarations of the block scope into the global scope
                for (std::shared_ptr<Declaration> declaration : statement.block->locals_list()) {
                    declaration->name = std::format("{}{}", scope_prefix, ltrim(declaration->name, "."));
                    scope->add_local(declaration);
                }

                // Move all labels of the block scope into the global scope
                for (const auto& [name, label] : statement.block->labels)
                    scope->add_label(label);

                scope->statements.erase(scope->statements.begin() + position);  // Remove the block statement
                position -= 1;                                                  // Point the next iteration to the first statement of the moved scope
            }
        }
    }
}
