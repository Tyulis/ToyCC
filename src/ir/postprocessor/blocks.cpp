#include "arch/datamodel.h"
#include "ir/postprocessor.h"
#include "ir/type_expressions.h"

namespace toycc::ir {
    void PostProcessor::split_blocks(std::shared_ptr<Scope> scope) {
        std::vector<Statement> original_statements = scope->statements;
        scope->statements.clear();

        for (Statement& statement : original_statements) {
            if (statement.block.get() != nullptr) {
                split_blocks(statement.block);
            } else {
                if (statement.output.has_value())
                    statement.output = split_operand_blocks(*statement.output, scope);

                for (auto it = statement.inputs.begin(); it != statement.inputs.end(); it++)
                    *it = split_operand_blocks(*it, scope);
            }

            scope->statements.push_back(statement);
        }
    }

    Operand PostProcessor::split_operand_blocks(Operand operand, std::shared_ptr<Scope> scope) {
        // First, split block accesses recursively in indices
        for (Operand& index : operand.indices)
            split_operand_blocks(index, scope);

        if (operand.indices.empty())
            return operand;

        // Then split block accesses in the actual operand
        Operand block = operand;
        std::shared_ptr<Type> block_type = block.base_type();
        size_t top_level = 0;
        for (size_t level = 0; level < operand.indices.size(); level++) {
            const Operand& index = operand.indices[level];
            std::shared_ptr<Type> referenced_type = block_type->dereference(index.as_index(), index.location);

            switch (block_type->category) {
                case TypeCategory::ARRAY:
                case TypeCategory::STRUCT:
                case TypeCategory::UNION: {
                    std::shared_ptr<PointerType> pointer_type = PointerType::make(index.location, referenced_type);
                    std::shared_ptr<Declaration> member_pointer = declare_temporary(scope, pointer_type, index.location);
                    const Operand reference = Operand {block.value, block.location, {operand.indices.begin() + top_level, operand.indices.begin() + level + 1}};
                    scope->add_statement(Statement::make_unary_operation(operand.location, StatementTag::ADDRESSOF, reference, member_pointer));
                    std::vector<Operand> new_indices(operand.indices.begin() + level + 1, operand.indices.end());
                    new_indices.insert(new_indices.begin(), Constant {IntegerConstant(0), index.location, arch::DATAMODEL->offset_type});
                    block = Operand {member_pointer, operand.location, new_indices};
                    top_level = level;
                    break;
                }

                default:
                    break;
            }

            block_type = referenced_type;
        }

        return block;
    }
}
