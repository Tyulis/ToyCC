#include "diagnostic.h"
#include "ir/generator.h"
#include "arch/x86_64.h"

namespace toycc::ir {
    // Return a declaration compatible with the `target` type specification. If necessary, emit an implicit cast and declare a new temporary with that target type
    std::shared_ptr<Declaration> Generator::emit_implicit_conversion(TypeSpecification target, std::shared_ptr<Declaration> source, CodeLocation location) {
        if (target.can_be_assigned_from(source->spec))  // Same type, no conversion necessary
            return source;

        const Flags<stmt::ConversionOperation> operation = implicit_conversion_operation(target, source->spec, location);  // Throws if no implicit conversion is available
        std::shared_ptr<Declaration> destination = declare_temporary(target, location);
        current_scope()->add_statement(std::make_shared<stmt::Conversion>(location, operation, destination, source));
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

    // Emit a copy statement from source to destination, adding an implicit cast if necessary
    void Generator::emit_copy(std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> source, CodeLocation location, bool initialize) {
        if (!initialize && (destination->spec.qualifiers & TypeQualifier::CONST))
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to assign a value to a constant after initialization", location);

        source = emit_implicit_conversion(destination->spec, source, location);
        current_scope()->add_statement(std::make_shared<stmt::Conversion>(location, Flags<stmt::ConversionOperation>{}, destination, source));
    }


    // Get the conversion operations to perform, or throw if no implicit conversion can be performed
    Flags<stmt::ConversionOperation> Generator::implicit_conversion_operation(TypeSpecification destination, TypeSpecification source, CodeLocation location) {
        if (destination.bitfield_length != source.bitfield_length)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfield conversions are not implemented", location);

        if (destination.is_array_type() && source.is_array_type()) {
            if (destination.element_type() != source.element_type())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between array types with incompatible element types", location);
            if (destination.array_spec != source.array_spec)
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible array dimensions", location);

            return {};  // In practice those are just pointers, nothing to do
        }

        else if (destination.is_function_type && source.is_function_type) {
            if (destination.return_type() != source.return_type())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between function types with incompatible return types", location);
            if (destination.parameters.size() != source.parameters.size())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible function types", location);

            for (size_t param = 0; param < destination.parameters.size(); param++) {
                try {
                    if (implicit_conversion_operation(destination.parameters[param].spec, source.parameters[param].spec, location))
                        throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between function types with incompatible parameter types", location);
                } catch (const Diagnostic&) {
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between function types with incompatible parameter types", location);
                }
            }

            return {};  // In practice those are just pointers, nothing to do
        }

        else if (destination.is_pointer_type() && source.is_pointer_type()) {
            if (destination.type.get() != source.type.get())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between pointers to incompatible types", location);

            if (destination.pointer_spec.size() != source.pointer_spec.size())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible pointer types", location);

            if (!destination.qualifiers.includes(source.qualifiers))
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a less cv-qualified pointer type", location);

            for (size_t level = 0; level < destination.pointer_spec.size() - 1; level++)
                if (!destination.pointer_spec[level].includes(source.pointer_spec[level]))
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a less cv-qualified pointer type", location);

            return {};  // Those are both pointers, nothing to do
        }

        else if (destination.is_object_type() && source.is_object_type()) {
            const TypeCategory destination_category = destination.type->identifier.category;
            const TypeCategory source_category = source.type->identifier.category;

            if (destination_category == TypeCategory::VOID || source_category == TypeCategory::VOID)
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't cast between void and non-void types", location);
            if (destination_category == TypeCategory::TYPEDEF || source_category == TypeCategory::TYPEDEF)
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted a cast with an unresolved typedef", location);
            if (destination_category == TypeCategory::BUILTIN || source_category == TypeCategory::BUILTIN)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Casting to or from built-in types is not supported", location);

            if (destination_category == TypeCategory::PRIMITIVE && source_category == TypeCategory::ENUM) {
                PrimitiveType& primitive_destination = static_cast<PrimitiveType&>(*destination.type);
                if (primitive_destination.semantic != PrimitiveSemantic::INTEGER)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast an enumerated value to a non-integer primitive type", location);
                if (!primitive_destination.is_signed)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast an enumerated value to an unsigned integer type", location);

                if (primitive_destination.primitive_size < toycc::arch::INT_SIZE)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely convert an enumerated value to an integer type smaller than int", location);
                else if (primitive_destination.primitive_size == toycc::arch::INT_SIZE)
                    return {};
                else
                    return stmt::ConversionOperation::INTEGER_SIZE_UP;
            }

            if (destination_category != source_category)  // Except enum -> int, implicit conversions across categories are forbidden
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible type categories", location);

            if (destination_category == TypeCategory::STRUCT && source_category == TypeCategory::STRUCT) {
                if (destination.type.get() != source.type.get())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't cast between incompatible structure types", location);
                return {};
            }

            if (destination_category == TypeCategory::UNION && source_category == TypeCategory::UNION) {
                if (destination.type.get() != source.type.get())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't cast between incompatible union types", location);
                return {};
            }

            if (destination_category == TypeCategory::ENUM && source_category == TypeCategory::ENUM) {
                if (destination.type.get() != source.type.get())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't cast between incompatible enum types", location);
                return {};
            }

            // Now, only primitive types remain
            PrimitiveType& destination_primitive = static_cast<PrimitiveType&>(*destination.type);
            PrimitiveType& source_primitive      = static_cast<PrimitiveType&>(*source.type);

            if (destination_primitive.semantic == PrimitiveSemantic::FLOAT) {
                if (source_primitive.semantic == PrimitiveSemantic::FLOAT) {
                    if (destination_primitive.primitive_size > source_primitive.primitive_size)
                        return stmt::ConversionOperation::FLOAT_SIZE_UP;
                    else if (destination_primitive.primitive_size == source_primitive.primitive_size)
                        return {};
                    else throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a smaller floating-point type", location);
                } else if (source_primitive.semantic == PrimitiveSemantic::INTEGER) {
                    return stmt::ConversionOperation::INT_TO_FLOAT;
                } else if (source_primitive.semantic == PrimitiveSemantic::BOOL) {
                    return stmt::ConversionOperation::BOOL_TO_FLOAT;
                }
            }

            else if (destination_primitive.semantic == PrimitiveSemantic::INTEGER) {
                if (source_primitive.semantic == PrimitiveSemantic::INTEGER) {
                    if (destination_primitive.is_signed == source_primitive.is_signed) {
                        if (destination_primitive.primitive_size > source_primitive.primitive_size)
                            return stmt::ConversionOperation::INTEGER_SIZE_UP;
                        else if (destination_primitive.primitive_size == source_primitive.primitive_size)
                            return {};
                        else if (destination_primitive.primitive_size < source_primitive.primitive_size)
                            throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a smaller integer type", location);
                    } else if (!destination_primitive.is_signed && destination_primitive.primitive_size > source_primitive.primitive_size) {
                        return stmt::ConversionOperation::INTEGER_SIZE_UP | stmt::ConversionOperation::SIGNED_TO_UNSIGNED;
                    } else if (destination_primitive.is_signed && destination_primitive.primitive_size > source_primitive.primitive_size) {
                        return stmt::ConversionOperation::INTEGER_SIZE_UP | stmt::ConversionOperation::UNSIGNED_TO_SIGNED;
                    } else {
                        throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast to a same-size or smaller type with different signedness", location);
                    }
                } else if (source_primitive.semantic == PrimitiveSemantic::BOOL) {
                    return stmt::ConversionOperation::BOOL_TO_INT;
                } else if (source_primitive.semantic == PrimitiveSemantic::FLOAT) {
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast floating-point types to integers", location);
                }
            }

            else if (destination_primitive.semantic == PrimitiveSemantic::BOOL) {
                if (source_primitive.semantic == PrimitiveSemantic::BOOL)
                    return {};
                else if (source_primitive.semantic == PrimitiveSemantic::INTEGER)
                    return stmt::ConversionOperation::INT_TO_BOOL;
                else if (source_primitive.semantic == PrimitiveSemantic::FLOAT)
                    return stmt::ConversionOperation::FLOAT_TO_BOOL;
            }
        }

        throw Diagnostic(DiagnosticLevel::ERROR, "Can't implicitely cast between incompatible type classes", location);
    }

    Flags<stmt::ConversionOperation> Generator::explicit_conversion_operation(TypeSpecification, TypeSpecification, CodeLocation location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Explicit conversions are not implemented", location);
    }
}
