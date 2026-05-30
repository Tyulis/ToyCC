#include <format>
#include <sstream>

#include "diagnostic.h"
#include "code_location.h"
#include "ir/type.h"
#include "util/alignment.hpp"

namespace toycc::ir {
    constexpr static TypeTag to_tag(TypeCategory category) {
        switch (category) {
            case TypeCategory::VOID:      return TypeTag::DIRECT;
            case TypeCategory::BUILTIN:   return TypeTag::DIRECT;
            case TypeCategory::BOOL:      return TypeTag::DIRECT;
            case TypeCategory::INTEGER:   return TypeTag::DIRECT;
            case TypeCategory::FLOAT:     return TypeTag::DIRECT;
            case TypeCategory::LABEL:     return TypeTag::DIRECT;
            case TypeCategory::POINTER:   return TypeTag::DIRECT;
            case TypeCategory::ARRAY:     return TypeTag::DIRECT;
            case TypeCategory::STRUCT:    return TypeTag::STRUCT;
            case TypeCategory::UNION:     return TypeTag::UNION;
            case TypeCategory::ENUM:      return TypeTag::ENUM;
            case TypeCategory::FUNCTION:  return TypeTag::DIRECT;
            case TypeCategory::BITFIELD:  return TypeTag::DIRECT;
            case TypeCategory::ALIGNED:   return TypeTag::DIRECT;
            case TypeCategory::QUALIFIED: return TypeTag::DIRECT;
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category");
    }

    static std::string tag_repr(TypeTag tag) {
        switch (tag) {
            case TypeTag::DIRECT:   return "";
            case TypeTag::STRUCT:   return "struct ";
            case TypeTag::UNION:    return "union ";
            case TypeTag::ENUM:     return "enum ";
            case TypeTag::TYPEDEF:  return "";
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type tag");
    }

    static std::string category_repr(TypeCategory category) {
        switch (category) {
            case TypeCategory::VOID:      return "VOID";
            case TypeCategory::BUILTIN:   return "BUILTIN";
            case TypeCategory::BOOL:      return "BOOL";
            case TypeCategory::INTEGER:   return "INTEGER";
            case TypeCategory::FLOAT:     return "FLOAT";
            case TypeCategory::LABEL:     return "LABEL";
            case TypeCategory::POINTER:   return "POINTER";
            case TypeCategory::ARRAY:     return "ARRAY";
            case TypeCategory::STRUCT:    return "STRUCT";
            case TypeCategory::UNION:     return "UNION";
            case TypeCategory::ENUM:      return "ENUM";
            case TypeCategory::FUNCTION:  return "FUNCTION";
            case TypeCategory::BITFIELD:  return "BITFIELD";
            case TypeCategory::ALIGNED:   return "ALIGNED";
            case TypeCategory::QUALIFIED: return "QUALIFIED";
        }

        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category");
    }

    // -------- TypeIdentifier
    std::string TypeIdentifier::text() const {
        return std::format("{}{}", tag_repr(tag), name);
    }

    // -------- Type
    Type::Type(TypeCategory category, CodeLocation location) : category(category), location(location) {}

    bool Type::complete() const {
        return true;
    }

    bool Type::is_const() const {
        return false;
    }

    size_t Type::size(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't quary the size of type `{}`", text()), location);
    }

    size_t Type::alignment(CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't quary the alignment of type `{}`", text()), location);
    }

    bool Type::operator== (const Type& rhs) const {
        return category == rhs.category;
    }

    std::shared_ptr<Type> Type::dereference(std::optional<size_t>, CodeLocation location) const {
        throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't dereference type `{}`", text()), location);
    }

    std::shared_ptr<Type> Type::dequalify() const {
        return std::make_shared<Type> (*this);
    }

    TypeCategory Type::storage_category() const {
        return category;
    }

    TypeIdentifier Type::identifier() const {
        return {.tag = to_tag(category), .name = repr()};
    }

    bool Type::is_arithmetic() const {
        const TypeCategory dequalified_category = dequalify()->category;
        return dequalified_category == TypeCategory::BOOL || dequalified_category == TypeCategory::INTEGER || dequalified_category == TypeCategory::FLOAT;
    }

    bool Type::is_integral() const {
        const TypeCategory dequalified_category = dequalify()->category;
        return dequalified_category == TypeCategory::BOOL || dequalified_category == TypeCategory::INTEGER || dequalified_category == TypeCategory::ENUM;
    }

    bool Type::is_floating_point() const {
        const TypeCategory dequalified_category = dequalify()->category;
        return dequalified_category == TypeCategory::FLOAT;
    }

    bool Type::is_signed() const {
        return false;
    }

    bool Type::is_comparable() const {
        const TypeCategory dequalified_category = dequalify()->category;
        return dequalified_category == TypeCategory::BOOL || dequalified_category == TypeCategory::INTEGER || dequalified_category == TypeCategory::FLOAT ||
        dequalified_category == TypeCategory::POINTER || dequalified_category == TypeCategory::ENUM;
    }

    bool Type::has_truth_value() const {
        const TypeCategory dequalified_category = dequalify()->category;
        return dequalified_category == TypeCategory::BOOL || dequalified_category == TypeCategory::INTEGER || dequalified_category == TypeCategory::FLOAT ||
               dequalified_category == TypeCategory::POINTER || dequalified_category == TypeCategory::ENUM;
    }

    bool Type::is_compound() const {
        const TypeCategory dequalified_category = dequalify()->category;
        return dequalified_category == TypeCategory::STRUCT || dequalified_category == TypeCategory::UNION;
    }

    bool Type::is_block() const {
        const TypeCategory dequalified_category = dequalify()->category;
        return dequalified_category == TypeCategory::STRUCT || dequalified_category == TypeCategory::UNION || dequalified_category == TypeCategory::ARRAY;
    }

    std::string Type::repr() const {
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Can't represent type `{}`", text()), location);
    }

    std::string Type::ir_code(std::unordered_set<const Type*>) const {
        return category_repr(category);
    }

    std::string Type::text() const {
        return ir_code();
    }

    // -------- PrimitiveType
    PrimitiveType::PrimitiveType(TypeCategory category, std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits)
        : Type(category, location), name(name), size_bits(size_bits), alignment_bits(alignment_bits) {}

    size_t PrimitiveType::size(CodeLocation) const {
        return size_bits_to_bytes(size_bits);
    }

    size_t PrimitiveType::alignment(CodeLocation) const {
        return alignment_bits_to_bytes(alignment_bits);
    }

    std::string PrimitiveType::repr() const {
        return name;
    }

    std::string PrimitiveType::ir_code(std::unordered_set<const Type*>) const {
        std::stringstream code;
        code << category_repr(category) << "(" << size_bits;
        if (alignment_bits != size_bits)
            code << "|" << alignment_bits;
        code << ")";
        return code.str();
    }

    bool PrimitiveType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const PrimitiveType&>(rhs);
    }

    bool PrimitiveType::operator== (const PrimitiveType& rhs) const {
        return Type::operator== (rhs) && size_bits == rhs.size_bits && alignment_bits == rhs.alignment_bits;
    }

    // -------- BooleanType
    BooleanType::BooleanType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits)
        : PrimitiveType(TypeCategory::BOOL, name, location, size_bits, alignment_bits) {}

    bool BooleanType::is_signed() const {
        return false;
    }

    std::shared_ptr<Type> BooleanType::dequalify() const {
        return std::make_shared<BooleanType> (*this);
    }


    // -------- IntegerType
    IntegerType::IntegerType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits, bool is_signed)
        : PrimitiveType(TypeCategory::INTEGER, name, location, size_bits, alignment_bits), _signed(is_signed) {}

    std::string IntegerType::ir_code(std::unordered_set<const Type*>) const {
        return std::format("{} {}", (_signed? "SIGNED" : "UNSIGNED"), PrimitiveType::ir_code());
    }

    bool IntegerType::operator== (const Type& rhs) const {
        return (category == rhs.category) && *this == static_cast<const IntegerType&>(rhs);
    }

    bool IntegerType::operator== (const IntegerType& rhs) const {
        return PrimitiveType::operator== (rhs) && _signed == rhs._signed;
    }

    bool IntegerType::is_signed() const {
        return _signed;
    }

    std::shared_ptr<Type> IntegerType::dequalify() const {
        return std::make_shared<IntegerType> (*this);
    }


    // -------- FloatingPointType
    FloatingPointType::FloatingPointType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits)
        : PrimitiveType(TypeCategory::FLOAT, name, location, size_bits, alignment_bits) {}

    bool FloatingPointType::is_signed() const {
        return true;
    }

    std::shared_ptr<Type> FloatingPointType::dequalify() const {
        return std::make_shared<FloatingPointType> (*this);
    }
}
