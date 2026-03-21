#include "diagnostic.h"
#include "ir/postprocessor.h"
#include "ir/type_expressions.h"

namespace toycc::ir {
    // Process pointer dereferences and array indices to flatten multi-dimensional and dynamic indexing, and resolve all array indices to static offsets
    // At this point, indirections have been split up, so the only thing to do is computing flat offsets
    void PostProcessor::dereference(std::shared_ptr<Scope> scope) {
        std::vector<Statement> original_statements = scope->statements;
        scope->statements.clear();

        for (Statement& statement : original_statements) {
            if (statement.block.get() != nullptr) {
                dereference(statement.block);
            } else {
                if (statement.output.has_value())
                    statement.output = dereference_operand(*statement.output, scope);

                for (auto it = statement.inputs.begin(); it != statement.inputs.end(); it++)
                    *it = dereference_operand(*it, scope);
            }

            scope->statements.push_back(statement);
        }
    }

    // Convert multi-level indices to a flat offset, ex. a[0][1][2] of type int32_t[2][4][8] -> a[40]
    Operand PostProcessor::dereference_operand(Operand operand, std::shared_ptr<Scope> scope) {
        for (Operand& index : operand.indices)
            index = fully_dereference_operand(index, scope);

        if (operand.indices.empty())
            return operand;

        std::shared_ptr<Type> pointer_type = operand.base_type();
        Operand flat_offset = Constant {IntegerConstant(0), operand.location, offset_type};
        for (const Operand& index : operand.indices) {
            std::shared_ptr<Type> referenced_type = pointer_type->dereference(index.as_index(), index.location);
            Operand offset = make_offset(pointer_type, index, scope);
            flat_offset = merge_offsets(flat_offset, offset, scope);
            pointer_type = referenced_type;
        }

        if (flat_offset.is_constant()) {
            return Operand {operand.value, operand.location, {flat_offset}};
        } else {
            // Variable offset -> explicitely add it to the pointer before dereferencing
            std::shared_ptr<Declaration> offset_pointer = declare_temporary(scope, PointerType::make(anonymous_type(), operand.location, pointer_type), operand.location);
            scope->add_statement(Statement::make_binary_operation(operand.location, StatementTag::ADD, operand, flat_offset, offset_pointer));
            return Operand {offset_pointer, operand.location, {Constant {IntegerConstant(0), operand.location, offset_type}}};
        }
    }

    // Fully dereference an operand, i.e make an operand with the dereferenced value, but without any dereference remaining
    Operand PostProcessor::fully_dereference_operand(Operand operand, std::shared_ptr<Scope> scope) {
        if (!operand.is_dereference())
            return operand;

        Operand dereferenced = dereference_operand(operand, scope);

        // Emit a copy to a flat variable
        Operand result = declare_temporary(scope, dereferenced.type(), dereferenced.location);
        scope->add_statement(Statement::make_unary_operation(dereferenced.location, StatementTag::COPY, dereferenced, result));
        return result;
    }

    // Compute an operand for the pointer_type[index] offset
    Operand PostProcessor::make_offset(std::shared_ptr<Type> pointer_type, Operand index, std::shared_ptr<Scope> scope) {
        pointer_type = pointer_type->dequalify();
        switch (pointer_type->category) {
            case TypeCategory::POINTER:
            case TypeCategory::ARRAY:
                return make_pointer_offset(pointer_type, index, scope);

            case TypeCategory::STRUCT:
                return make_struct_offset(pointer_type, index, scope);

            case TypeCategory::UNION:
                return make_union_offset(pointer_type, index, scope);

            default: throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't dereference type `{}`", pointer_type->ir_code()), index.location);
        }
    }

    Operand PostProcessor::make_pointer_offset(std::shared_ptr<Type> pointer_type, Operand index, std::shared_ptr<Scope> scope) {
        std::shared_ptr<Type> referenced_type = pointer_type->dereference(index.as_index(), index.location);
        const size_t size = referenced_type->size(index.location);

        if (index.is_constant()) {
            if (index.constant().tag() != Constant::INTEGER)
                throw Diagnostic(DiagnosticLevel::ERROR, "Indices must be integers", index.location);

            // Fold constants immediately
            IntegerConstant offset = index.constant().integer() * size;
            return Constant {offset, index.location, offset_type};
        } else {
            // Insert a multiplication to convert the index to an offset
            Operand size_operand = Constant {IntegerConstant(size), index.location, offset_type};
            Operand offset = declare_temporary(scope, offset_type, index.location);
            scope->add_statement(Statement::make_binary_operation(index.location, StatementTag::MUL, index, size_operand, offset));
            return offset;
        }
    }

    Operand PostProcessor::make_struct_offset(std::shared_ptr<Type> pointer_type, Operand index, std::shared_ptr<Scope>) {
        if (!index.is_constant() || index.constant().tag() != Constant::INTEGER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Struct member indices must be constant integers", index.location);

        std::shared_ptr<StructType> struct_type = std::static_pointer_cast<StructType>(pointer_type);
        const size_t offset = struct_type->member_offset(index.as_index().value());
        return Constant {IntegerConstant(offset), index.location, offset_type};
    }

    Operand PostProcessor::make_union_offset(std::shared_ptr<Type>, Operand index, std::shared_ptr<Scope>) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Union member access is not implemented", index.location);
    }

    // Compute an operand for flat_offset + index * size
    Operand PostProcessor::merge_offsets(Operand flat_offset, Operand offset, std::shared_ptr<Scope> scope) {
        if (flat_offset.is_constant() && offset.is_constant()) {
            // Fold constants immediately
            IntegerConstant result = flat_offset.constant().integer() + offset.constant().integer();
            return Constant {result, offset.location, offset_type};
        } else {
            // For variable indices, insert an addition
            Operand result = declare_temporary(scope, offset_type, offset.location);
            scope->add_statement(Statement::make_binary_operation(offset.location, StatementTag::ADD, flat_offset, offset, result));
            return result;
        }
    }
}
