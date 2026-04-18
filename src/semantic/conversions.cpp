#include "diagnostic.h"
#include "ir/declaration.h"
#include "ir/type_expressions.h"
#include "ir/statement.h"
#include "ir/type.h"
#include "arch/datamodel.h"
#include "semantic/analyzer.h"

namespace toycc::semantic {
    SemanticAnalyzer::ConversionValidity SemanticAnalyzer::get_conversion_validity(std::shared_ptr<Type> destination, std::shared_ptr<Type> source) {
        // Those shouldn't be converted at all
        if (destination->category == TypeCategory::VOID || source->category == TypeCategory::VOID)
            return ConversionValidity::INVALID;
        if (destination->category == TypeCategory::BUILTIN || source->category == TypeCategory::BUILTIN)
            return ConversionValidity::INVALID;

        // Exact same type -> OK for coercion. From now on, types are not equal
        if (*destination == *source)
            return ConversionValidity::IMPLICIT;

        // Get modifiers out of the way
        if (destination->category == TypeCategory::QUALIFIED)
            return get_conversion_validity(static_cast<const QualifiedType&>(*destination).underlying_type, source);
        if (source->category == TypeCategory::QUALIFIED)
            return get_conversion_validity(destination, static_cast<const QualifiedType&>(*source).underlying_type);
        if (destination->category == TypeCategory::ALIGNED)
            return get_conversion_validity(static_cast<const AlignedType&>(*destination).underlying_type, source);
        if (source->category == TypeCategory::ALIGNED)
            return get_conversion_validity(destination, static_cast<const AlignedType&>(*source).underlying_type);

        if (destination->category == TypeCategory::BITFIELD || source->category == TypeCategory::BITFIELD)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfield conversions are not implemented");

        // Those are only valid when both types are equal
        if (destination->category == TypeCategory::FUNCTION || source->category == TypeCategory::FUNCTION)
            return ConversionValidity::INVALID;
        if (destination->category == TypeCategory::STRUCT || source->category == TypeCategory::STRUCT)
            return ConversionValidity::INVALID;
        if (destination->category == TypeCategory::UNION || source->category == TypeCategory::UNION)
            return ConversionValidity::INVALID;

        if (destination->category == TypeCategory::ENUM) {
            if      (source->category == TypeCategory::INTEGER)  return ConversionValidity::EXPLICIT;
            else if (source->category == TypeCategory::BOOL)     return ConversionValidity::EXPLICIT;
            else return ConversionValidity::INVALID;
        }

        if (destination->category == TypeCategory::ARRAY) {
            if (source->category == TypeCategory::POINTER)  return ConversionValidity::EXPLICIT;
            else return ConversionValidity::INVALID;
        }

        if (destination->category == TypeCategory::POINTER) {
            if      (source->category == TypeCategory::POINTER)  return ConversionValidity::EXPLICIT;  // Only implicit when it's the same pointer type
            else if (source->category == TypeCategory::INTEGER)  return ConversionValidity::EXPLICIT;
            else if (source->category == TypeCategory::ARRAY) {
                std::shared_ptr<PointerType> destination_pointer = std::static_pointer_cast<PointerType> (destination);
                std::shared_ptr<ArrayType> source_array = std::static_pointer_cast<ArrayType> (source);

                if (*source_array->element_type == *destination_pointer->referenced_type)
                    return ConversionValidity::IMPLICIT;
                else
                    return ConversionValidity::INVALID;
            } else return ConversionValidity::INVALID;
        }

        else if (destination->category == TypeCategory::FLOAT) {
            if      (source->category == TypeCategory::BOOL)     return ConversionValidity::IMPLICIT;
            else if (source->category == TypeCategory::INTEGER)  return ConversionValidity::IMPLICIT;
            else if (source->category == TypeCategory::FLOAT) {
                std::shared_ptr<FloatingPointType> destination_float = std::static_pointer_cast<FloatingPointType>(destination);
                std::shared_ptr<FloatingPointType> source_float      = std::static_pointer_cast<FloatingPointType>(destination);
                if (destination_float->size_bits >= source_float->size_bits)  return ConversionValidity::IMPLICIT;
                else                                                          return ConversionValidity::EXPLICIT;
            }
            else return ConversionValidity::INVALID;
        }

        else if (destination->category == TypeCategory::INTEGER) {
            if      (source->category == TypeCategory::BOOL)   return ConversionValidity::IMPLICIT;
            else if (source->category == TypeCategory::FLOAT)  return ConversionValidity::EXPLICIT;
            else if (source->category == TypeCategory::INTEGER) {
                std::shared_ptr<IntegerType> destination_int = std::static_pointer_cast<IntegerType>(destination);
                std::shared_ptr<IntegerType> source_int      = std::static_pointer_cast<IntegerType>(source);

                if      (destination_int->size_bits > source_int->size_bits)    return ConversionValidity::IMPLICIT;
                else if (destination_int->size_bits < source_int->size_bits)    return ConversionValidity::EXPLICIT;
                else if (destination_int->is_signed && !source_int->is_signed)  return ConversionValidity::IMPLICIT;
                else                                                            return ConversionValidity::EXPLICIT;
            }
            else return ConversionValidity::INVALID;
        }

        else if (destination->category == TypeCategory::BOOL) {
            if      (source->category == TypeCategory::ARRAY)    return ConversionValidity::IMPLICIT;
            else if (source->category == TypeCategory::POINTER)  return ConversionValidity::IMPLICIT;
            else if (source->category == TypeCategory::FLOAT)    return ConversionValidity::IMPLICIT;
            else if (source->category == TypeCategory::INTEGER)  return ConversionValidity::IMPLICIT;
            else if (source->category == TypeCategory::BOOL)     return ConversionValidity::IMPLICIT;
            else return ConversionValidity::INVALID;
        }

        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown conversion case");  // Everything should have returned by now
    }

    // Return a declaration compatible with the `target` type specification. If necessary, emit an implicit cast and declare a new temporary with that target type
    Operand SemanticAnalyzer::emit_implicit_conversion(std::shared_ptr<Type> destination_type, Operand source, CodeLocation location) {
        const ConversionValidity validity = get_conversion_validity(destination_type, source.type());
        switch (validity) {
            case ConversionValidity::INVALID:
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't convert type `{}` to `{}`", source.type()->text(), destination_type->text()), location);
            case ConversionValidity::EXPLICIT:
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Conversion from `{}` to `{}` can't be implicit", source.type()->text(), destination_type->text()), location);
            case ConversionValidity::IMPLICIT:
                return emit_conversion(destination_type, source, location);
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown conversion validity", location);
    }

    // Main entry point. Internals won't recheck the validity of the conversion
    Operand SemanticAnalyzer::emit_conversion(std::shared_ptr<Type> destination_type, Operand source, CodeLocation location) {
        const ConversionValidity validity = get_conversion_validity(destination_type, source.type());
        if (validity == ConversionValidity::INVALID)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't convert from `{}` to `{}`", source.type()->text(), destination_type->text()));

        return emit_conversion(destination_type, source.type(), source, location, make_temporary_generator(destination_type, location));
    }

    Operand SemanticAnalyzer::emit_conversion(std::shared_ptr<Type> destination_type, std::shared_ptr<Type> source_type, Operand source,
                                      CodeLocation location, SemanticAnalyzer::TemporaryGenerator destination_generator)
    {
        // Qualifiers were already checked, they are irrelevant in conversions -> remove them
        if (destination_type->category == TypeCategory::QUALIFIED)
            destination_type = std::static_pointer_cast<QualifiedType>(destination_type)->underlying_type;
        if (source_type->category == TypeCategory::QUALIFIED)
            source_type = std::static_pointer_cast<QualifiedType>(source_type)->underlying_type;

        // Exact same type -> nothing to do
        if (destination_type == source_type)
            return source;

        if (destination_type->category == TypeCategory::ALIGNED || source_type->category == TypeCategory::ALIGNED) {
            std::shared_ptr<Type> destination_unqualified = destination_type, source_unqualified = source_type;
            if (destination_type->category == TypeCategory::ALIGNED)
                destination_unqualified = std::static_pointer_cast<AlignedType>(destination_type)->underlying_type;
            if (source_type->category == TypeCategory::ALIGNED)
                source_unqualified = std::static_pointer_cast<AlignedType>(source_type)->underlying_type;

            Operand destination = emit_conversion(destination_unqualified, source_unqualified, source, location, destination_generator);

            // No copy was emitted, but one is required to realign the source object
            if (!source.is_constant() && destination == source && destination_type->alignment(location) > source_type->alignment(location)) {
                std::shared_ptr<Declaration> conversion_result = destination_generator();
                emit(Statement::make_unary_operation(location, StatementTag::COPY, source, conversion_result));
                destination = conversion_result;
            }
            return destination;
        }

        if (destination_type->category == TypeCategory::BITFIELD || source_type->category == TypeCategory::BITFIELD)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfield conversions are not implemented");

        switch (destination_type->category) {
            case TypeCategory::BOOL:    return emit_conversion_to_bool   (std::static_pointer_cast<BooleanType>      (destination_type), source_type, source, location, destination_generator);
            case TypeCategory::INTEGER: return emit_conversion_to_integer(std::static_pointer_cast<IntegerType>      (destination_type), source_type, source, location, destination_generator);
            case TypeCategory::FLOAT:   return emit_conversion_to_float  (std::static_pointer_cast<FloatingPointType>(destination_type), source_type, source, location, destination_generator);
            case TypeCategory::POINTER: return emit_conversion_to_pointer(                                            destination_type,  source_type, source, location, destination_generator);
            case TypeCategory::ARRAY:   return emit_conversion_to_pointer(                                            destination_type,  source_type, source, location, destination_generator);
            case TypeCategory::ENUM:    return emit_conversion_to_enum   (std::static_pointer_cast<EnumType>         (destination_type), source_type, source, location, destination_generator);
            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown conversion case : `{}` to `{}`", destination_type->text(), source_type->text()), location);
        }
    }

    Operand SemanticAnalyzer::emit_conversion_to_bool(std::shared_ptr<BooleanType> destination_type, std::shared_ptr<Type> source_type, Operand source,
                                              CodeLocation location, SemanticAnalyzer::TemporaryGenerator destination_generator)
    {
        switch (source_type->category) {
            case TypeCategory::BOOL:
                return source;

            case TypeCategory::POINTER:
            case TypeCategory::INTEGER:
            case TypeCategory::FLOAT: {
                std::shared_ptr<Declaration> destination = destination_generator();
                emit(Statement::make_binary_operation(location, StatementTag::NE, source, make_constant_zero(source_type->category, location), destination));
                return destination;
            }

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown conversion case : `{}` to `{}`", destination_type->text(), source_type->text()), location);
        }
    }

    Operand SemanticAnalyzer::emit_conversion_to_integer(std::shared_ptr<IntegerType> destination_type, std::shared_ptr<Type> source_type, Operand source,
                                                 CodeLocation location, SemanticAnalyzer::TemporaryGenerator destination_generator)
    {
        switch (source_type->category) {
            case TypeCategory::BOOL:
                if (destination_type->size(location) == source_type->size(location) && destination_type->alignment(location) >= source_type->alignment(location))
                    return source;
                else if (destination_type->size(location) >= source_type->size(location))
                    return emit_copy_conversion(destination_type, source, location, destination_generator, StatementTag::SIGN_EXTEND);
                else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unsupported boolean -> integer conversion", location);

            case TypeCategory::INTEGER: {
                std::shared_ptr<IntegerType> source_integer = std::static_pointer_cast<IntegerType>(source_type);
                if (destination_type->is_signed == source_integer->is_signed) {
                    if (destination_type->size_bits > source_integer->size_bits)
                        return emit_copy_conversion(destination_type, source, location, destination_generator, StatementTag::SIGN_EXTEND);
                    else if (destination_type->size_bits == source_integer->size_bits)
                        return source;
                    else  // if (destination_primitive.primitive_size < source_primitive.primitive_size)
                        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Narrowing integer conversions are not implemented", location);
                } else if (destination_type->is_signed && !source_integer->is_signed) {  // unsigned to signed
                    // 6.3.1.3.1 : If the source value can be represented by the destination type if the destination type has at least one more bit, so that conversion is okay
                    // 6.3.1.3.3 : If the destination type is signed but the source value can't be represented in it, the result is implementation-defined
                    //             Here, go through the same operation, whatever happens happens
                    // Unsigned values are always positive regardless of the "sign" bit, so zero-extend
                    if (destination_type->size_bits >= source_integer->size_bits)
                        return emit_copy_conversion(destination_type, source, location, destination_generator, StatementTag::ZERO_EXTEND);
                    else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Narrowing integer conversions are not implemented", location);
                } else /* if (!destination_type->is_signed && source_integer->is_signed) */ {  // signed to unsigned
                    // 6.3.1.3.1 : If the source value can be represented by the destination type if the destination type has at least one more bit, so that conversion is okay
                    // 6.3.1.3.2 : If the destination type is unsigned but the source value can't be represented in it, wrap around
                    // Ex. 8(-120) + 256 -> 8(136) <=> 0b10001000 -> nothing to do
                    // Ex. 16(-1000) + 1024 = 8(24) <=> 0b[11111100]00011000 -> nothing to do
                    // Ex. 8(+120) -> 16(120) <=> 0b[00000000]01111000 -> sign-extend
                    // Ex. 8(-120) + 65536 -> 16(65416) <=> 0b[11111111]10001000 -> sign-extend
                    if (destination_type->size_bits >= source_integer->size_bits)
                        return emit_copy_conversion(destination_type, source, location, destination_generator, StatementTag::SIGN_EXTEND);
                    else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Narrowing integer conversions are not implemented", location);
                }
            }

            case TypeCategory::FLOAT:
                return emit_copy_conversion(destination_type, source, location, destination_generator, StatementTag::FLOAT_TO_INT);

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown conversion case : `{}` to `{}`", destination_type->text(), source_type->text()), location);
        }
    }

    Operand SemanticAnalyzer::emit_conversion_to_float(std::shared_ptr<FloatingPointType> destination_type, std::shared_ptr<Type> source_type, Operand source,
                                               CodeLocation location, SemanticAnalyzer::TemporaryGenerator destination_generator)
    {
        switch (source_type->category) {
            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
                return emit_copy_conversion(destination_type, source, location, destination_generator, StatementTag::INT_TO_FLOAT);

            case TypeCategory::FLOAT: {
                std::shared_ptr<FloatingPointType> source_float = std::static_pointer_cast<FloatingPointType>(source_type);
                if (source_float->size_bits == destination_type->size_bits) {
                    if (source_float->alignment_bits == destination_type->alignment_bits)
                        return source;
                    else
                        return emit_copy_conversion(destination_type, source, location, destination_generator);
                } else {
                    return emit_copy_conversion(destination_type, source, location, destination_generator, StatementTag::FLOAT_TO_FLOAT);
                }
            }

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown conversion case : `{}` to `{}`", destination_type->text(), source_type->text()), location);
        }
    }

    Operand SemanticAnalyzer::emit_conversion_to_pointer(std::shared_ptr<Type> destination_type, std::shared_ptr<Type> source_type, Operand source,
                                                 CodeLocation location, SemanticAnalyzer::TemporaryGenerator destination_generator)
    {
        switch (source_type->category) {
            case TypeCategory::ARRAY: {
                // Point to the beginning of the array
                std::shared_ptr<Declaration> pointer = destination_generator();
                source.indices.push_back(make_constant_zero(TypeCategory::INTEGER, pointer->location));
                emit(Statement::make_addressof(pointer->location, source, pointer));
                return pointer;
            }

            case TypeCategory::POINTER:
                return source;

            case TypeCategory::INTEGER: {
                if (source_type->size(location) == destination_type->size(location))
                    return source;
                else
                    return emit_copy_conversion(destination_type, source, location, destination_generator);
            }

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown conversion case : `{}` to `{}`", destination_type->text(), source_type->text()), location);
        }
    }

    Operand SemanticAnalyzer::emit_conversion_to_enum(std::shared_ptr<EnumType> destination_type, std::shared_ptr<Type> source_type, Operand source,
                                              CodeLocation location, SemanticAnalyzer::TemporaryGenerator destination_generator)
    {
        if (destination_type->underlying_type->category != TypeCategory::INTEGER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid enum underlying type", location);
        return emit_conversion_to_integer(std::static_pointer_cast<IntegerType>(destination_type->underlying_type), source_type, source, location, destination_generator);
    }


    Operand SemanticAnalyzer::emit_copy_conversion(std::shared_ptr<Type> destination_type, Operand source, CodeLocation location, TemporaryGenerator destination_generator, StatementTag op) {
        if (source.is_constant()) {
            // Don't emit copies for constant expressions, just give another type expression to the constant
            return source.constant().as(destination_type);
        } else {
            std::shared_ptr<Declaration> destination = destination_generator();
            emit(Statement::make_unary_operation(location, op, source, destination));
            return destination;
        }
    }

    std::array<Operand, 2> SemanticAnalyzer::emit_arithmetic_conversion(Operand left, Operand right, CodeLocation location) {
        try {
            left = emit_implicit_conversion(right.type(), left, location);
        } catch (const Diagnostic& left_conversion_diagnostic) {
            try {
                right = emit_implicit_conversion(left.type(), right, location);
            } catch (const Diagnostic& right_conversion_diagnostic) {
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't perform any standard arithmetic conversions to make the operands compatible", location)
                      .add_note(left_conversion_diagnostic).add_note(right_conversion_diagnostic);
            }
        }

        return {left, right};
    }

    // Copy source to destination, adding an implicit cast if necessary
    void SemanticAnalyzer::emit_copy(Operand destination, Operand source, CodeLocation location, bool initialize) {
        if (!initialize && destination.type()->is_const())
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to assign a value to a constant after initialization", location);

        source = emit_implicit_conversion(destination.type(), source, location);
        emit(Statement::make_unary_operation(location, StatementTag::COPY, source, destination));
    }

    RValue SemanticAnalyzer::make_constant_zero(TypeCategory category, CodeLocation location) {
        switch (category) {
            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::POINTER:
                return Constant {IntegerConstant(0), location, arch::DATAMODEL->literal_integer_type};

            case TypeCategory::FLOAT:
                return Constant {FloatingPointConstant(0.0), location, arch::DATAMODEL->literal_floating_type};

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category for a constant zero", location);
        }
    }

    RValue SemanticAnalyzer::make_constant_zero(std::shared_ptr<Type> type, CodeLocation location) {
        switch (type->category) {
            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::POINTER:
                return Constant {IntegerConstant(0), location, type};

            case TypeCategory::FLOAT:
                return Constant {FloatingPointConstant(0.0), location, type};

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category for a constant zero", location);
        }
    }

    RValue SemanticAnalyzer::make_constant_one(TypeCategory category, CodeLocation location) {
        switch (category) {
            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::POINTER:
                return Constant {IntegerConstant(1), location, arch::DATAMODEL->literal_integer_type};

            case TypeCategory::FLOAT:
                return Constant {FloatingPointConstant(1.0), location, arch::DATAMODEL->literal_floating_type};

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category for a constant one", location);
        }
    }

    RValue SemanticAnalyzer::make_constant_one(std::shared_ptr<Type> type, CodeLocation location) {
        switch (type->category) {
            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::POINTER:
                return Constant {IntegerConstant(1), location, type};

            case TypeCategory::FLOAT:
                return Constant {FloatingPointConstant(1.0), location, type};

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category for a constant one", location);
        }
    }
}
