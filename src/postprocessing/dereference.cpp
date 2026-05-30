#include "diagnostic.h"
#include "arch/datamodel.h"
#include "ir/type_expressions.h"
#include "postprocessing/postprocessor.h"

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
        for (Operand& index : operand.indices) {
            index = fully_dereference_operand(index, scope);  // Recursively dereference the index so it's only an integer afterwards
            index = convert_to_offset(index, scope);          // The final offset will be applied to a pointer, so the index must be converted to the offset type first
        }

        if (operand.indices.empty())
            return operand;

        std::shared_ptr<Type> pointer_type = operand.base_type();
        Operand flat_offset = Constant {IntegerConstant(0), operand.location, arch::DATAMODEL->offset_type};
        for (Operand index : operand.indices) {
            std::shared_ptr<Type> referenced_type = pointer_type->dereference(index.as_index(), index.location);
            Operand offset = make_offset(pointer_type, index, scope);
            flat_offset = merge_offsets(flat_offset, offset, scope);
            pointer_type = referenced_type;
        }
        std::shared_ptr<Type> referenced_type = pointer_type;

        std::shared_ptr<Type> new_pointer_type = PointerType::make(operand.location, referenced_type);
        if (flat_offset.tag() == Operand::CONSTANT) {
            if (operand.indices.size() <= 1)
                return Operand {operand.value, operand.location, {flat_offset}};

            // We need to set the right pointer type in case there were multiple indirection levels
            // Ex. POINTER -> STRUCT -> INTEGER should resolve to POINTER -> INTEGER directly since there's only one dereference level now
            switch (operand.base_tag()) {
                case Operand::CONSTANT_BASE:
                    return Operand {operand.constant().as(new_pointer_type), operand.location, {flat_offset}};

                case Operand::VARIABLE_BASE:
                    if (*operand.base_type() == *new_pointer_type) {
                        return Operand {operand.declaration(), operand.location, {flat_offset}};
                    } else {
                        std::shared_ptr<Declaration> pointer_copy = declare_temporary(scope, new_pointer_type, operand.location);
                        scope->add_statement(Statement::make_unary_operation(operand.location, StatementTag::COPY, operand.pointer(), pointer_copy));
                        return Operand {pointer_copy, operand.location, {flat_offset}};
                    }
            }
            __builtin_unreachable();
        } else {
            // Variable offset -> explicitely add it to the pointer before dereferencing
            std::shared_ptr<Declaration> offset_pointer = declare_temporary(scope, new_pointer_type, operand.location);
            scope->add_statement(Statement::make_binary_operation(operand.location, StatementTag::ADD, operand.pointer(), flat_offset, offset_pointer));
            return Operand {offset_pointer, operand.location, {Constant {IntegerConstant(0), operand.location, arch::DATAMODEL->offset_type}}};
        }
    }

    // Fully dereference an operand, i.e make an operand with the dereferenced value, but without any dereference remaining
    Operand PostProcessor::fully_dereference_operand(Operand operand, std::shared_ptr<Scope> scope) {
        if (operand.tag() != Operand::DEREFERENCE)
            return operand;

        Operand dereferenced = dereference_operand(operand, scope);

        // Emit a copy to a flat variable
        Operand result = declare_temporary(scope, dereferenced.type(), dereferenced.location);
        scope->add_statement(Statement::make_unary_operation(dereferenced.location, StatementTag::COPY, dereferenced, result));
        return result;
    }

    // Convert an index to the offset type if necessary
    Operand PostProcessor::convert_to_offset(Operand index, std::shared_ptr<Scope> scope) {
        std::shared_ptr<Type> index_type = index.type();
        if (*index_type == *arch::DATAMODEL->offset_type)
            return index;

        switch (index.tag()) {
            case Operand::CONSTANT: {
                if (index.constant().tag() != Constant::INTEGER)
                    throw Diagnostic(DiagnosticLevel::ERROR, "An index must resolve to an integer type", index.location);
                return Constant {index.constant().integer(), index.location, arch::DATAMODEL->offset_type};  // Just overwrite the type
            }

            case Operand::VARIABLE: {
                std::shared_ptr<Declaration> converted = declare_temporary(scope, arch::DATAMODEL->offset_type, index.location);

                switch (index_type->category) {
                    case TypeCategory::BOOL:  // Equivalent to a small unsigned type, zero-extend
                        scope->add_statement(Statement::make_unary_operation(index.location, StatementTag::ZERO_EXTEND, index, converted));
                        return converted;

                    case TypeCategory::INTEGER: {
                        if (index_type->is_signed())
                            scope->add_statement(Statement::make_unary_operation(index.location, StatementTag::SIGN_EXTEND, index, converted));
                        else
                            scope->add_statement(Statement::make_unary_operation(index.location, StatementTag::ZERO_EXTEND, index, converted));
                        return converted;
                    }

                    default:
                        throw Diagnostic(DiagnosticLevel::ERROR, "An index must resolve to an integer type", index.location);
                }
            }

            case Operand::DEREFERENCE:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The index should have been already dereferenced", index.location);
        }
        __builtin_unreachable();
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

        if (index.tag() == Operand::CONSTANT) {
            if (index.constant().tag() != Constant::INTEGER)
                throw Diagnostic(DiagnosticLevel::ERROR, "Indices must be integers", index.location);

            // Fold constants immediately
            IntegerConstant offset = index.constant().integer() * size;
            return Constant {offset, index.location, arch::DATAMODEL->offset_type};
        } else {
            // Insert a multiplication to convert the index to an offset
            Operand size_operand = Constant {IntegerConstant(size), index.location, arch::DATAMODEL->offset_type};
            Operand offset = declare_temporary(scope, arch::DATAMODEL->offset_type, index.location);
            scope->add_statement(Statement::make_binary_operation(index.location, StatementTag::MUL, index, size_operand, offset));
            return offset;
        }
    }

    Operand PostProcessor::make_struct_offset(std::shared_ptr<Type> pointer_type, Operand index, std::shared_ptr<Scope>) {
        if (index.tag() != Operand::CONSTANT || index.constant().tag() != Constant::INTEGER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Struct member indices must be constant integers", index.location);

        std::shared_ptr<StructType> struct_type = std::static_pointer_cast<StructType>(pointer_type);
        const size_t offset = struct_type->member_offset(index.as_index().value());
        return Constant {IntegerConstant(offset), index.location, arch::DATAMODEL->offset_type};
    }

    Operand PostProcessor::make_union_offset(std::shared_ptr<Type>, Operand index, std::shared_ptr<Scope>) {
        return Constant {IntegerConstant(0), index.location, arch::DATAMODEL->offset_type};
    }

    // Compute an operand for flat_offset + index * size
    Operand PostProcessor::merge_offsets(Operand flat_offset, Operand offset, std::shared_ptr<Scope> scope) {
        if (flat_offset.tag() == Operand::CONSTANT && offset.tag() == Operand::CONSTANT) {
            // Fold constants immediately
            IntegerConstant result = flat_offset.constant().integer() + offset.constant().integer();
            return Constant {result, offset.location, arch::DATAMODEL->offset_type};
        } else {
            // For variable indices, insert an addition
            Operand result = declare_temporary(scope, arch::DATAMODEL->offset_type, offset.location);
            scope->add_statement(Statement::make_binary_operation(offset.location, StatementTag::ADD, flat_offset, offset, result));
            return result;
        }
    }
}
