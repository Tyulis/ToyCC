#include <sstream>

#include "diagnostic.h"
#include "code_location.h"
#include "ir/type.h"
#include "ir/type_expressions.h"
#include "arch/x86_64.h"
#include "util/alignment.hpp"


namespace toycc::ir {
    std::string type_qualifiers_repr(Flags<TypeQualifier> qualifiers) {
        if (!qualifiers)
            return "";

        std::stringstream repr;
        if (qualifiers & TypeQualifier::CONST)     repr << "const ";
        if (qualifiers & TypeQualifier::VOLATILE)  repr << "volatile ";
        if (qualifiers & TypeQualifier::RESTRICT)  repr << "restrict ";
        if (qualifiers & TypeQualifier::ATOMIC)    repr << "atomic ";
        return repr.str();
    }

    // -------- PointerType
    PointerType::PointerType(std::string name, CodeLocation location, std::shared_ptr<Type> referenced_type)
        : Type(TypeCategory::POINTER, name, location), referenced_type(referenced_type) {}

    std::shared_ptr<PointerType> PointerType::make(std::string name, CodeLocation location, std::shared_ptr<Type> referenced_type) {
        if (referenced_type->category == TypeCategory::BITFIELD)
            throw Diagnostic(DiagnosticLevel::ERROR, "Pointers to bitfield types are not allowed", location);
        return std::make_shared<PointerType> (PointerType {name, location, referenced_type});
    }

    size_t PointerType::size(CodeLocation) const {
        return toycc::arch::POINTER_SIZE;
    }

    size_t PointerType::alignment(CodeLocation) const {
        return toycc::arch::POINTER_ALIGNMENT;
    }

    bool PointerType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const PointerType&>(rhs);
    }

    bool PointerType::operator== (const PointerType& rhs) const {
        return Type::operator== (rhs) && *referenced_type == *rhs.referenced_type;
    }

    std::shared_ptr<Type> PointerType::dereference(CodeLocation) const {
        return referenced_type;
    }

    std::string PointerType::ir_code() const {
        return std::format("{}*", referenced_type->ir_code());
    }

    // -------- ArrayType
    ArrayType::ArrayType(std::string name, CodeLocation location, std::shared_ptr<Type> element_type, std::shared_ptr<Declaration> length)
        : Type(TypeCategory::ARRAY, name, location), element_type(element_type), length(length) {}

    std::shared_ptr<ArrayType> ArrayType::make(std::string name, CodeLocation location, std::shared_ptr<Type> element_type, std::shared_ptr<Declaration> length) {
        if (element_type->category == TypeCategory::VOID)
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't make arrays of void", location);
        return std::make_shared<ArrayType> (ArrayType {name, location, element_type, length});
    }


    size_t ArrayType::size(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Array types size and alignment are not implemented", location);
    }

    size_t ArrayType::alignment(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Array types size and alignment are not implemented", location);
    }

    bool ArrayType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const ArrayType&>(rhs);
    }

    bool ArrayType::operator== (const ArrayType& rhs) const {
        return Type::operator== (rhs) && *element_type == *rhs.element_type && length == rhs.length;  // FIXME : Evaluate lengths
    }

    std::shared_ptr<Type> ArrayType::dereference(CodeLocation) const {
        return element_type;
    }

    std::string ArrayType::ir_code() const {
        return std::format("{}[{}]", element_type->ir_code(), length->name);
    }

    // -------- CompoundType
    CompoundType::CompoundType(TypeCategory category, std::string name, CodeLocation location, bool is_complete, std::vector<Member> members)
        : Type(category, name, location), is_complete(is_complete), members(members) {}

    bool CompoundType::complete() const {
        return is_complete;
    }

    bool CompoundType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const CompoundType&>(rhs);
    }

    bool CompoundType::operator== (const CompoundType& rhs) const {
        if (Type::operator== (rhs) && is_complete && rhs.is_complete && members.size() == rhs.members.size()) {
            for (size_t member = 0; member < members.size(); member++)
                if (*members[member].type != *rhs.members[member].type)
                    return false;
            return true;
        } else {
            return false;
        }
    }

    std::string CompoundType::ir_code() const {
        std::stringstream code;
        code << Type::ir_code();
        if (is_complete) {
            code << "{\n";
            for (const Member& member : members)
                code << member.ir_code() << ";\n";
            code << "}";
        }
        return code.str();
    }

    // -------- StructType
    StructType::StructType(std::string name, CodeLocation location, bool is_complete, std::vector<Member> members)
        : CompoundType(TypeCategory::STRUCT, name, location, is_complete, members) {}

    std::shared_ptr<StructType> StructType::make(std::string name, CodeLocation location, bool is_complete, std::vector<Member> members) {
        if (!is_complete && !members.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Incomplete structure type can't have members", location);
        return std::make_shared<StructType> (StructType {name, location, is_complete, members});
    }

    size_t StructType::size(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Struct type size computation is not implemented", location);
    }

    size_t StructType::alignment(CodeLocation location) const {
        if (!is_complete)
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to query the alignment of an incomplete type", location);
        if (members.empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Alignment of empty structures is not implemented", location);
        return members[0].type->alignment(location);
    }

    // -------- UnionType
    UnionType::UnionType(std::string name, CodeLocation location, bool is_complete, std::vector<Member> members)
        : CompoundType(TypeCategory::UNION, name, location, is_complete, members) {}

    std::shared_ptr<UnionType> UnionType::make(std::string name, CodeLocation location, bool is_complete, std::vector<Member> members) {
        if (!is_complete && !members.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Incomplete union type can't have members", location);
        return std::make_shared<UnionType> (UnionType {name, location, is_complete, members});
    }

    size_t UnionType::size(CodeLocation location) const {
        size_t union_size = 0;
        for (const Member& member : members)
            union_size = std::max(union_size, member.type->size(location));
        return union_size;
    }

    size_t UnionType::alignment(CodeLocation location) const {
        if (!is_complete)
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to query the alignment of an incomplete type", location);
        if (members.empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Alignment of empty unions is not implemented", location);

        size_t union_alignment = 0;
        for (const Member& member : members)
            union_alignment = std::max(union_alignment, member.type->alignment(location));
        return union_alignment;
    }

    // -------- EnumType
    EnumType::EnumType(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, std::unordered_map<std::string, ssize_t> values)
        : Type(TypeCategory::ENUM, name, location), underlying_type(underlying_type), values(values) {}

    std::shared_ptr<EnumType> EnumType::make(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, std::unordered_map<std::string, ssize_t> values) {
        if (underlying_type->category != TypeCategory::INTEGER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The underlying type of an enum type must be an integer type", location);
        return std::make_shared<EnumType> (EnumType {name, location, underlying_type, values});
    }

    bool EnumType::complete() const {
        return !values.empty();
    }

    size_t EnumType::size(CodeLocation location) const {
        return underlying_type->size(location);
    }

    size_t EnumType::alignment(CodeLocation location) const {
        return underlying_type->alignment(location);
    }

    bool EnumType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const EnumType&>(rhs);
    }

    bool EnumType::operator== (const EnumType& rhs) const {
        return Type::operator== (rhs) && *underlying_type == *rhs.underlying_type && values == rhs.values;
    }

    std::string EnumType::ir_code() const {
        std::stringstream code;
        code << Type::ir_code() << "{\n";
        for (std::pair<std::string, ssize_t> value : values)
            code << value.first << " = " << value.second << ",\n";
        code << "}";
        return code.str();
    }

    // -------- FunctionType
    FunctionType::FunctionType(std::string name, CodeLocation location, std::shared_ptr<Type> return_type, std::vector<Member> parameters)
        : Type(TypeCategory::FUNCTION, name, location), return_type(return_type), parameters(parameters) {}

    std::shared_ptr<FunctionType> FunctionType::make(std::string name, CodeLocation location, std::shared_ptr<Type> return_type, std::vector<Member> parameters) {
        std::vector<Member> actual_parameters;
        for (const Member& parameter : parameters) {
            if (parameter.type->category == TypeCategory::VOID) {
                if (parameters.size() > 1)
                    throw Diagnostic(DiagnosticLevel::ERROR, "A function with `void` parameter list can't have other parameters", location);
            } else {
                actual_parameters.push_back(parameter);
            }
        }

        if (return_type->category == TypeCategory::ARRAY)
            throw Diagnostic(DiagnosticLevel::ERROR, "A function can't return an array type", location);

        return std::make_shared<FunctionType> (FunctionType {name, location, return_type, parameters});
    }

    size_t FunctionType::size(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::ERROR, "Can't query the size of a function type", location);
    }

    size_t FunctionType::alignment(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::ERROR, "Can't query the alignment of a function type", location);
    }

    bool FunctionType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const FunctionType&>(rhs);
    }

    bool FunctionType::operator== (const FunctionType& rhs) const {
        if (Type::operator== (rhs) && *return_type == *rhs.return_type && parameters.size() == rhs.parameters.size()) {
            for (size_t param = 0; param < parameters.size(); param++)
                if (*parameters[param].type != *rhs.parameters[param].type)
                    return false;
            return true;
        } else {
            return false;
        }
    }

    std::string FunctionType::ir_code() const {
        std::stringstream code;
        code << return_type->ir_code() << " " << Type::ir_code();
        if (parameters.size() == 0) {
            code << "()";
        } else {
            code << "(\n";
            for (const Member& parameter : parameters)
                code << "    " << parameter.ir_code() << ",\n";
            code << ")";
        }
        return code.str();
    }

    // -------- TypeModifier

    TypeModifier::TypeModifier(TypeCategory category, std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type)
        : Type(category, name, location), underlying_type(underlying_type) {}

    bool TypeModifier::is_const() const {
        return underlying_type->is_const();
    }

    size_t TypeModifier::size(CodeLocation location) const {
        return underlying_type->size(location);
    }

    size_t TypeModifier::alignment(CodeLocation) const {
        return underlying_type->alignment(location);
    }

    bool TypeModifier::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const TypeModifier&>(rhs);
    }

    bool TypeModifier::operator== (const TypeModifier& rhs) const {
        return Type::operator== (rhs) && *underlying_type == *rhs.underlying_type;
    }

    std::shared_ptr<Type> TypeModifier::dereference(CodeLocation) const {
        return underlying_type->dereference(location);
    }

    std::shared_ptr<Type> TypeModifier::dequalify() const {
        return underlying_type->dequalify();
    }

    // -------- BitfieldType
    BitfieldType::BitfieldType(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, size_t size_bits)
        : TypeModifier(TypeCategory::BITFIELD, name, location, underlying_type), size_bits(size_bits) {}

    std::shared_ptr<BitfieldType> BitfieldType::make(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, size_t size_bits) {
        if (underlying_type->category != TypeCategory::BOOL || underlying_type->category != TypeCategory::INTEGER)
            throw Diagnostic(DiagnosticLevel::ERROR, "A bitfield type must be built upon a boolean or integer type", location);

        return std::make_shared<BitfieldType> (BitfieldType {name, location, underlying_type, size_bits});
    }

    size_t BitfieldType::size(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::ERROR, "Can't query the size of a bitfield type", location);
    }

    size_t BitfieldType::alignment(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::ERROR, "Can't query the alignment of a bitfield type", location);
    }

    bool BitfieldType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const BitfieldType&>(rhs);
    }

    bool BitfieldType::operator== (const BitfieldType& rhs) const {
        return TypeModifier::operator== (rhs) && size_bits == rhs.size_bits;
    }

    std::string BitfieldType::ir_code() const {
        return std::format("{}:{}", underlying_type->ir_code(), size_bits);
    }

    // -------- AlignedType
    AlignedType::AlignedType(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, size_t alignment_bits)
        : TypeModifier(TypeCategory::ALIGNED, name, location, underlying_type), alignment_bits(alignment_bits) {}

    std::shared_ptr<Type> AlignedType::make(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, size_t alignment_bits) {
        if (!is_power_of_two(alignment_bits))
            throw Diagnostic(DiagnosticLevel::ERROR, "Alignment boundaries must be powers of two", location);

        switch (underlying_type->category) {
            case TypeCategory::VOID:     throw Diagnostic(DiagnosticLevel::ERROR, "Can't align a void type", location);
            case TypeCategory::BUILTIN:  throw Diagnostic(DiagnosticLevel::ERROR, "Built-in types' memory layout are unspecified and can't be manually aligned", location);

            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::FLOAT: {
                if (alignment_bits < underlying_type->alignment(location))
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't manually align a type to a smaller boundary than its natural alignment", location);

                // Simplify the type expression : AlignedType(PrimitiveType) -> PrimitiveType with a different alignment
                const PrimitiveType& underlying_primitive = static_cast<const PrimitiveType&>(*underlying_type);
                return std::make_shared<PrimitiveType> (underlying_primitive.category, name, location, underlying_primitive.size_bits, alignment_bits);
            }

            case TypeCategory::ARRAY:  // Is that even possible ?
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Aligning array types is not implemented", location);

            case TypeCategory::ENUM: {
                // Simplify the type expression : AlignedType(EnumType) -> EnumType(PrimitiveType with a different alignment)
                const EnumType& underlying_enum = static_cast<const EnumType&> (*underlying_type);
                if (underlying_enum.underlying_type->category != TypeCategory::INTEGER)
                    throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Enum types can't have anything other than integers as their underlying type", location);
                std::shared_ptr<Type> aligned_primitive = AlignedType::make({}, location, underlying_enum.underlying_type, alignment_bits);
                return EnumType::make(name, location, aligned_primitive, underlying_enum.values);
            }

            case TypeCategory::FUNCTION:  throw Diagnostic(DiagnosticLevel::ERROR, "Can't manually align a function type", location);
            case TypeCategory::BITFIELD:  throw Diagnostic(DiagnosticLevel::ERROR, "Can't manually align a bitfield type", location);

            case TypeCategory::QUALIFIED: {
                // Simplify the type expression : AlignedType(QualifiedType(x)) = QualifiedType(AlignedType(x))
                // AlignedType simplifies better than QualifiedType, so keep QualifiedType on top
                const QualifiedType& qualified_type = static_cast<const QualifiedType&> (*underlying_type);
                std::shared_ptr<Type> aligned_unqualified_type = AlignedType::make({}, location, qualified_type.underlying_type, alignment_bits);
                return QualifiedType::make(name, location, aligned_unqualified_type, qualified_type.qualifiers);
            }

            case TypeCategory::ALIGNED: {
                // Simplify the type expression : AlignedType(A1, AlignedType(A2, x)) = AlignedType(A1, x), only valid when A1 >= A2
                const AlignedType& inner_aligned = static_cast<const AlignedType&> (*underlying_type);
                if (alignment_bits < inner_aligned.alignment_bits)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Can't manually align a type to a smaller boundary than its existing alignment", location);
                return std::make_shared<AlignedType> (AlignedType {name, location, inner_aligned.underlying_type, alignment_bits});
            }

            // Otherwise, generate a normal type expression
            case TypeCategory::POINTER:
            case TypeCategory::STRUCT:
            case TypeCategory::UNION:
                return std::make_shared<AlignedType> (AlignedType {name, location, underlying_type, alignment_bits});
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category", location);
    }

    size_t AlignedType::alignment(CodeLocation) const {
        return alignment_bits_to_bytes(alignment_bits);
    }

    bool AlignedType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const AlignedType&>(rhs);
    }

    bool AlignedType::operator== (const AlignedType& rhs) const {
        return TypeModifier::operator== (rhs) && alignment_bits == rhs.alignment_bits;
    }

    std::string AlignedType::ir_code() const {
        return std::format("{}|{}", underlying_type->ir_code(), alignment_bits);
    }

    // -------- QualifiedType
    QualifiedType::QualifiedType(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, Flags<TypeQualifier> qualifiers)
        : TypeModifier(TypeCategory::QUALIFIED, name, location, underlying_type), qualifiers(qualifiers) {}

    std::shared_ptr<QualifiedType> QualifiedType::make(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, Flags<TypeQualifier> qualifiers) {
        switch (underlying_type->category) {
            case TypeCategory::BUILTIN:  // Not sure about that case, block for now
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Qualifying built-in types is not implemented", location);
            case TypeCategory::ARRAY:  // Is that even possible ?
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Qualifying array types is not implemented", location);
            case TypeCategory::FUNCTION:
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't qualify a function type", location);

            case TypeCategory::QUALIFIED: {  // Simplify the type expression : QualifiedType(Q1, QualifiedType(Q2, x)) = QualifiedType(Q1 | Q2, x)
                const QualifiedType& underlying_qualified = static_cast<const QualifiedType&>(*underlying_type);
                return std::make_shared<QualifiedType> (QualifiedType {name, location, underlying_qualified.underlying_type, qualifiers | underlying_qualified.qualifiers});
            }

            // Otherwise, generate a normal type expression
            case TypeCategory::VOID:  // Allowed for pointers (e.g const void*)
            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::FLOAT:
            case TypeCategory::POINTER:
            case TypeCategory::STRUCT:
            case TypeCategory::UNION:
            case TypeCategory::ENUM:
            case TypeCategory::BITFIELD:
            case TypeCategory::ALIGNED:  // AlignedType(QualifiedType(x)) simplifies to QualifiedType(AlignedType(x)), don't simplify here to keep QualifiedType on top
                return std::make_shared<QualifiedType> (QualifiedType {name, location, underlying_type, qualifiers});
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category", location);
    }

    bool QualifiedType::is_const() const {
        return qualifiers & TypeQualifier::CONST;
    }

    bool QualifiedType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const QualifiedType&>(rhs);
    }

    bool QualifiedType::operator== (const QualifiedType& rhs) const {
        return TypeModifier::operator== (rhs) && qualifiers == rhs.qualifiers;
    }

    std::string QualifiedType::ir_code() const {
        return std::format("{}{}", type_qualifiers_repr(qualifiers), underlying_type->ir_code());
    }
}
