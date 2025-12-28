#include "diagnostic.h"
#include "ir/generator.h"
#include "arch/x86_64.h"
#include "ir/statement.h"
#include "ir/type.h"

namespace toycc::ir {
    // Return a declaration compatible with the `target` type specification. If necessary, emit an implicit cast and declare a new temporary with that target type
    std::shared_ptr<Declaration> Generator::emit_implicit_conversion(TypeSpecification destination_spec, std::shared_ptr<Declaration> source, CodeLocation location) {
        const TypeSpecification& source_spec = source->spec;

        if (destination_spec.bitfield_length != source_spec.bitfield_length)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfield conversions are not implemented", location);

        if (destination_spec.is_object_type() && destination_spec.type->identifier.category == TypeCategory::PRIMITIVE) {
            const PrimitiveType& destination_primitive = static_cast<const PrimitiveType&>(*destination_spec.type);
            if (destination_primitive.semantic == PrimitiveSemantic::BOOL)
                return convert_to_boolean(source, location);
        }

        if (destination_spec.is_array_type() && source_spec.is_array_type())
            return emit_implicit_conversion_array(destination_spec, source, location);
        else if (destination_spec.is_function_type && source_spec.is_function_type)
            return emit_implicit_conversion_function(destination_spec, source, location);
        else if (destination_spec.is_pointer_type() && source_spec.is_pointer_type())
            return emit_implicit_conversion_pointer(destination_spec, source, location);
        else if (destination_spec.is_object_type() && source_spec.is_object_type())
            return emit_implicit_conversion_object(destination_spec, source, location);

        throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible type classes", location);
    }

    std::shared_ptr<Declaration> Generator::emit_implicit_conversion_array(TypeSpecification destination_spec, std::shared_ptr<Declaration> source, CodeLocation location) {
        const TypeSpecification& source_spec = source->spec;

        if (destination_spec.element_type() != source_spec.element_type())
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between array types with incompatible element types", location);
        if (destination_spec.array_spec != source_spec.array_spec)
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible array dimensions", location);

        return source;  // In practice those are just pointers, nothing to do
    }

    std::shared_ptr<Declaration> Generator::emit_implicit_conversion_function(TypeSpecification destination_spec, std::shared_ptr<Declaration> source, CodeLocation location) {
        const TypeSpecification& source_spec = source->spec;

        if (destination_spec.return_type() != source_spec.return_type())
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between function types with incompatible return types", location);
        if (destination_spec.parameters.size() != source_spec.parameters.size())
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible function types", location);

        for (size_t param = 0; param < destination_spec.parameters.size(); param++) {
            try {
                if (!destination_spec.parameters[param].spec.can_be_assigned_from(source_spec.parameters[param].spec))
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between function types with incompatible parameter types", location);
            } catch (const Diagnostic&) {
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between function types with incompatible parameter types", location);
            }
        }

        return source;  // In practice those are just pointers, nothing to do
    }

    std::shared_ptr<Declaration> Generator::emit_implicit_conversion_pointer(TypeSpecification destination_spec, std::shared_ptr<Declaration> source, CodeLocation location) {
        const TypeSpecification& source_spec = source->spec;

        if (destination_spec.type.get() != source_spec.type.get())
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between pointers to incompatible types", location);

        if (destination_spec.pointer_spec.size() != source_spec.pointer_spec.size())
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible pointer types", location);

        if (!destination_spec.qualifiers.includes(source_spec.qualifiers))
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a less cv-qualified pointer type", location);

        for (size_t level = 0; level < destination_spec.pointer_spec.size() - 1; level++)
            if (!destination_spec.pointer_spec[level].includes(source_spec.pointer_spec[level]))
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a less cv-qualified pointer type", location);

        return source;  // Those are both pointers, nothing to do
    }

    std::shared_ptr<Declaration> Generator::emit_implicit_conversion_object(TypeSpecification destination_spec, std::shared_ptr<Declaration> source, CodeLocation location) {
        const TypeSpecification& source_spec = source->spec;

        const TypeCategory destination_category = destination_spec.type->identifier.category;
        const TypeCategory source_category = source_spec.type->identifier.category;

        if (destination_category == TypeCategory::VOID || source_category == TypeCategory::VOID)
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't cast between void and non-void types", location);
        if (destination_category == TypeCategory::TYPEDEF || source_category == TypeCategory::TYPEDEF)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted a cast with an unresolved typedef", location);
        if (destination_category == TypeCategory::BUILTIN || source_category == TypeCategory::BUILTIN)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Casting to or from built-in types is not supported", location);

        if (destination_category == TypeCategory::PRIMITIVE && source_category == TypeCategory::ENUM) {
            PrimitiveType& primitive_destination = static_cast<PrimitiveType&>(*destination_spec.type);
            if (primitive_destination.semantic != PrimitiveSemantic::INTEGER)
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast an enumerated value to a non-integer primitive type", location);
            if (!primitive_destination.is_signed)
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast an enumerated value to an unsigned integer type", location);

            if (primitive_destination.primitive_size < toycc::arch::INT_SIZE)
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely convert an enumerated value to an integer type smaller than int", location);
            else if (primitive_destination.primitive_size == toycc::arch::INT_SIZE)
                return source;
            else {
                std::shared_ptr<Declaration> destination = declare_temporary(destination_spec, location);
                current_scope()->add_statement(std::make_shared<ir::stmt::Copy>(location, stmt::ConversionOperation::COPY, destination, source));  // Extend the size -> just copy the value
                return destination;
            }
        }

        if (destination_category != source_category)  // Except enum -> int, implicit conversions across categories are forbidden
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible type categories", location);

        if (destination_category == TypeCategory::STRUCT && source_category == TypeCategory::STRUCT) {
            if (destination_spec.type.get() != source_spec.type.get())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't cast between incompatible structure types", location);
            return source;
        }

        if (destination_category == TypeCategory::UNION && source_category == TypeCategory::UNION) {
            if (destination_spec.type.get() != source_spec.type.get())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't cast between incompatible union types", location);
            return source;
        }

        if (destination_category == TypeCategory::ENUM && source_category == TypeCategory::ENUM) {
            if (destination_spec.type.get() != source_spec.type.get())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't cast between incompatible enum types", location);
            return source;
        }

        // Now, only primitive types remain
        return emit_implicit_conversion_primitive(destination_spec, source, location);
    }

    std::shared_ptr<Declaration> Generator::emit_implicit_conversion_primitive(TypeSpecification destination_spec, std::shared_ptr<Declaration> source, CodeLocation location) {
        const PrimitiveType& destination_primitive = static_cast<const PrimitiveType&>(*destination_spec.type);
        const PrimitiveType& source_primitive      = static_cast<const PrimitiveType&>(*source->spec.type);
        std::optional<stmt::ConversionOperation> operation;

        switch (destination_primitive.semantic) {
            case PrimitiveSemantic::FLOAT:
                switch (source_primitive.semantic) {
                    case PrimitiveSemantic::FLOAT:
                        if (destination_primitive.primitive_size > source_primitive.primitive_size)
                            operation = stmt::ConversionOperation::FLOAT_TO_FLOAT;
                        else if (destination_primitive.primitive_size == source_primitive.primitive_size)
                            return source;  // Same format -> no conversion required
                        else throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a smaller floating-point type", location);
                        break;

                    case PrimitiveSemantic::INTEGER:
                        operation = stmt::ConversionOperation::INT_TO_FLOAT;
                        break;

                    case PrimitiveSemantic::BOOL:
                        operation = stmt::ConversionOperation::BOOL_TO_FLOAT;
                        break;
                }
                break;

            case PrimitiveSemantic::INTEGER: {
                switch (source_primitive.semantic) {
                    case PrimitiveSemantic::INTEGER:
                        if (destination_primitive.is_signed == source_primitive.is_signed) {
                            if (destination_primitive.primitive_size > source_primitive.primitive_size)
                                operation = stmt::ConversionOperation::COPY;
                            else if (destination_primitive.primitive_size == source_primitive.primitive_size)
                                return source;
                            else  // if (destination_primitive.primitive_size < source_primitive.primitive_size)
                                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a smaller integer type", location);
                        } else if (!destination_primitive.is_signed && destination_primitive.primitive_size > source_primitive.primitive_size) {
                            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Signed -> unsigned conversions are not implemented", location);
                        } else if (destination_primitive.is_signed && destination_primitive.primitive_size > source_primitive.primitive_size) {
                            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unsigned -> signed conversions are not implemented", location);
                        } else {
                            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a same-size or smaller type with different signedness", location);
                        }
                        break;

                    case PrimitiveSemantic::BOOL:
                        operation = stmt::ConversionOperation::BOOL_TO_INT;
                        break;

                    case PrimitiveSemantic::FLOAT:
                        throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast floating-point types to integers", location);
                }
                break;
            }

            case PrimitiveSemantic::BOOL:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Wrong path, this should have gone through convert_to_boolean", location);
        }

        // Factor simple conversions here
        if (!operation.has_value())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "If this conversion is not a simple COPY, it already should have returned", location);

        std::shared_ptr<Declaration> destination = declare_temporary(destination_spec, location);
        current_scope()->add_statement(std::make_shared<ir::stmt::Copy>(location, operation.value(), destination, source));
        return destination;
    }


    std::array<std::shared_ptr<Declaration>, 2> Generator::emit_arithmetic_conversion(std::shared_ptr<Declaration> left, std::shared_ptr<Declaration> right, CodeLocation location) {
        try {
            left = emit_implicit_conversion(right->spec, left, location);
        } catch (const Diagnostic& left_conversion_diagnostic) {
            try {
                right = emit_implicit_conversion(left->spec, right, location);
            } catch (const Diagnostic& right_conversion_diagnostic) {
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't perform any standard arithmetic conversions to make the operands compatible", location)
                .add_note(left_conversion_diagnostic).add_note(right_conversion_diagnostic);
            }
        }

        return {left, right};
    }

    // Copy source to destination, adding an implicit cast if necessary
    void Generator::emit_copy(std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> source, CodeLocation location, bool initialize) {
        if (!initialize && (destination->spec.qualifiers & TypeQualifier::CONST))
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to assign a value to a constant after initialization", location);

        source = emit_implicit_conversion(destination->spec, source, location);
        current_scope()->add_statement(std::make_shared<stmt::Copy>(location, stmt::ConversionOperation::COPY, destination, source));
    }

    std::shared_ptr<Declaration> Generator::convert_to_boolean(std::shared_ptr<Declaration> value, CodeLocation location) {
        const TypeSpecification& value_spec = value->spec;
        if (value_spec.is_array_type())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Truth values of array types are not implemented", location);
        else if (value_spec.is_function_type)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Truth values of function types are not implemented", location);
        else if (value_spec.is_pointer_type())
            return convert_pointer_to_boolean(value, location);
        else if (value_spec.is_object_type())
            return convert_object_to_boolean(value, location);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown type class in conversion to boolean", location);
    }

    std::shared_ptr<Declaration> Generator::convert_pointer_to_boolean(std::shared_ptr<Declaration> value, CodeLocation location) {
        std::shared_ptr<Declaration> result = declare_temporary_predicate(location);
        std::shared_ptr<Declaration> zero = declare_temporary(value->spec, location);
        current_scope()->add_statement(std::make_shared<stmt::LoadConst> (location, zero, static_cast<size_t>(0)));
        current_scope()->add_statement(std::make_shared<stmt::BinaryOp> (location, stmt::BinaryOperator::EQ, result, value, zero));
        return result;
    }

    std::shared_ptr<Declaration> Generator::convert_object_to_boolean(std::shared_ptr<Declaration> value, CodeLocation location) {
        switch (value->spec.type->identifier.category) {
            case TypeCategory::VOID:       throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to use void in conversion to boolean", location);
            case TypeCategory::TYPEDEF:    throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to use an unresolved typedef in conversion to boolean", location);
            case TypeCategory::PRIMITIVE:  return convert_primitive_to_boolean(value, location);
            case TypeCategory::STRUCT:     throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Converting a struct to boolean is not implemented", location);
            case TypeCategory::UNION:      throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Converting a union to boolean is not implemented", location);
            case TypeCategory::ENUM:       throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Converting an enum to boolean is not implemented", location);
            case TypeCategory::BUILTIN:    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Converting a builtin to boolean is not implemented", location);
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown object type category", location);
    }

    std::shared_ptr<Declaration> Generator::convert_primitive_to_boolean(std::shared_ptr<Declaration> value, CodeLocation location) {
        const PrimitiveType& primitive = static_cast<const PrimitiveType&>(*value->spec.type);

        if (primitive.semantic == PrimitiveSemantic::BOOL)
            return value;

        std::shared_ptr<Declaration> result = declare_temporary_predicate(location);
        std::shared_ptr<Declaration> zero = declare_temporary(value->spec, location);

        if (primitive.semantic == PrimitiveSemantic::INTEGER) {
            if (primitive.is_signed)  current_scope()->add_statement(std::make_shared<stmt::LoadConst> (location, zero, static_cast<ssize_t>(0)));
            else                      current_scope()->add_statement(std::make_shared<stmt::LoadConst> (location, zero, static_cast<size_t> (0)));
        } else if (primitive.semantic == PrimitiveSemantic::FLOAT) {
            current_scope()->add_statement(std::make_shared<stmt::LoadConst> (location, zero, static_cast<long double>(0)));
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown primitive type semantic", location);

        current_scope()->add_statement(std::make_shared<stmt::BinaryOp> (location, stmt::BinaryOperator::EQ, result, value, zero));
        return result;
    }
}
