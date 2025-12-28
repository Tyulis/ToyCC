#include "diagnostic.h"
#include "ir/generator.h"
#include "arch/x86_64.h"
#include "ir/statement.h"

namespace toycc::ir {
    // Return a declaration compatible with the `target` type specification. If necessary, emit an implicit cast and declare a new temporary with that target type
    std::shared_ptr<Declaration> Generator::emit_implicit_conversion(TypeSpecification destination_spec, std::shared_ptr<Declaration> source, CodeLocation location) {
        const TypeSpecification& source_spec = source->spec;

        if (destination_spec.bitfield_length != source_spec.bitfield_length)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfield conversions are not implemented", location);

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

            case PrimitiveSemantic::BOOL: {
                switch (source_primitive.semantic) {
                    case PrimitiveSemantic::BOOL:
                        return source;
                    case PrimitiveSemantic::INTEGER:
                        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Int -> bool conversions are not implemented", location);
                    case PrimitiveSemantic::FLOAT:
                        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Float -> bool conversions are not implemented", location);;
                }
                break;
            }
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
}
