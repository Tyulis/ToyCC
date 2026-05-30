#include "arch/datamodel.h"
#include "ir/type_expressions.h"
#include "postprocessing/postprocessor.h"

// Rule : Block types (ARRAY, STRUCT, UNION) must always be accessed through a pointer, they shouldn't be actually loaded into variables / registers
namespace toycc::ir {
    static inline Operand index_zero(const CodeLocation& location) {
        return Constant {IntegerConstant(0), location, arch::DATAMODEL->offset_type};
    }

    void PostProcessor::split_blocks(std::shared_ptr<Scope> scope) {
        std::vector<Statement> original_statements = scope->statements;
        scope->statements.clear();

        for (Statement& statement : original_statements) {
            if (statement.block.get() != nullptr) {
                split_blocks(statement.block);
            } else {
                if (statement.output.has_value())
                    statement.output = split_operand_blocks(*statement.output, scope, statement.location);

                for (auto it = statement.inputs.begin(); it != statement.inputs.end(); it++)
                    *it = split_operand_blocks(*it, scope, statement.location);
            }

            scope->statements.push_back(statement);
        }
    }

    Operand PostProcessor::split_operand_blocks(Operand operand, std::shared_ptr<Scope> scope, const CodeLocation& location) {
        if (operand.indices.empty())
            return operand;

        // First, split block accesses recursively in indices
        for (Operand& index : operand.indices)
            split_operand_blocks(index, scope, location);

        // If a block type is at the top level, access it through a pointer instead
        // At lower levels it will always be accessed inside another block or through a pointer, so only the top level needs doing anything
        if (operand.base_type()->is_block()) {
            const Operand& index = operand.indices[0];
            std::shared_ptr<Type> member_type = operand.base_type()->dereference(index.as_index(), index.location);
            const Operand member = {operand.value, operand.location, {index}};
            std::shared_ptr<PointerType> member_pointer_type = PointerType::make(operand.location, member_type);
            std::shared_ptr<Declaration> member_pointer = declare_temporary(scope, member_pointer_type, operand.location);
            scope->add_statement(Statement::make_addressof(operand.location, member, member_pointer));

            std::vector<Operand> new_indices = {operand.indices.begin() + 1, operand.indices.end()};
            new_indices.insert(new_indices.begin(), index_zero(operand.location));
            operand = Operand {member_pointer, operand.location, new_indices};
        }

        return operand;
    }
}
