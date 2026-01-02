#include <set>
#include <sstream>
#include <variant>

#include "diagnostic.h"
#include "ir/declaration.h"

namespace toycc::ir {
    static std::string function_specifiers_repr(Flags<FunctionSpecifier> specifiers) {
        if (!specifiers)
            return "";

        std::stringstream repr;
        if (specifiers & FunctionSpecifier::INLINE)    repr << "inline ";
        if (specifiers & FunctionSpecifier::NORETURN)  repr << "noreturn ";
        return repr.str();
    }

    static std::string storage_classes_repr(Flags<StorageClass> storage) {
        if (!storage)
            return "";

        std::stringstream repr;
        if (storage & StorageClass::AUTO)          repr << "auto ";
        if (storage & StorageClass::STATIC)        repr << "static ";
        if (storage & StorageClass::EXTERN)        repr << "extern ";
        if (storage & StorageClass::REGISTER)      repr << "register ";
        if (storage & StorageClass::THREAD_LOCAL)  repr << "thread_local ";
        if (storage & StorageClass::TYPEDEF)       repr << "typedef ";
        if (storage & StorageClass::PARAMETER)     repr << "parameter ";
        if (storage & StorageClass::TEMPORARY)     repr << "temporary ";
        if (storage & StorageClass::ADDRESSED)     repr << "addressed ";
        return repr.str();
    }

    // -------- Member
    Member::Member(std::string name, std::shared_ptr<Type> type, CodeLocation location) : name(name), type(type), location(location) {}

    Member Member::to_storage_type() const {
        return {name, type->storage_type(), location};
    }

    std::string Member::ir_code() const {
        return std::format("{} {}", type->ir_code(), name);
    }

    // -------- Declaration
    Declaration::Declaration(Member member, Flags<StorageClass> storage, Flags<FunctionSpecifier> function_spec)
        : Member(member), storage(storage), function_spec(function_spec) {}

    Declaration::Declaration(std::string name, std::shared_ptr<Type> type, CodeLocation location, Flags<StorageClass> storage, Flags<FunctionSpecifier> function_spec)
        : Member(name, type, location), storage(storage), function_spec(function_spec) {}

    // Throw diagnostics if the declaration's semantics are inconsistent
    static const std::set<Flags<StorageClass>> VALID_STORAGE_CLASSES = {StorageClass::AUTO, StorageClass::STATIC, StorageClass::EXTERN, StorageClass::REGISTER, StorageClass::TYPEDEF,
        StorageClass::THREAD_LOCAL | StorageClass::STATIC, StorageClass::THREAD_LOCAL | StorageClass::EXTERN};
    void Declaration::check() const {
        if (name.empty())
            throw Diagnostic(DiagnosticLevel::ERROR, "Unnamed declaration", location);

        if (!VALID_STORAGE_CLASSES.contains(storage))
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid combination of storage class specifiers `{}`", storage_classes_repr(storage)), location);

        if (storage == StorageClass::TYPEDEF && function_spec)
            throw Diagnostic(DiagnosticLevel::ERROR, "Typedef declaration can't have function specifiers", location);
    }

    std::string Declaration::ir_code() const {
        return std::format("#decl {}{}{}", storage_classes_repr(storage), function_specifiers_repr(function_spec), Member::ir_code());
    }

    // -------- Constant
    Constant Constant::as(std::shared_ptr<Type> new_type) const {
        std::shared_ptr<Type> new_unqualified = new_type->dequalify();
        switch (new_unqualified->category) {
            case TypeCategory::VOID:
            case TypeCategory::BUILTIN:
            case TypeCategory::STRUCT:
            case TypeCategory::UNION:
            case TypeCategory::FUNCTION:
            case TypeCategory::BITFIELD:
            case TypeCategory::QUALIFIED:
            case TypeCategory::ALIGNED:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid type for a constant : `{}`", new_type->text()), location);

            case TypeCategory::POINTER:
                if (std::holds_alternative<std::string>(value))
                    return {.value = value, .location = location, .type = new_type};
                [[fallthrough]];

            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::ENUM:
                if (std::holds_alternative<IntegerConstant>(value))
                    return {.value = value, .location = location, .type = new_type};
                else if (std::holds_alternative<FloatingPointConstant>(value))
                    return {.value = IntegerConstant(std::get<FloatingPointConstant>(value)), .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-arithmetic constants to integers", location);

            case TypeCategory::FLOAT:
                if (std::holds_alternative<IntegerConstant>(value))
                    return {.value = FloatingPointConstant(std::get<IntegerConstant>(value)), .location = location, .type = new_type};
                else if (std::holds_alternative<FloatingPointConstant>(value))
                    return {.value = value, .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-arithmetic constants to integers", location);

            case TypeCategory::ARRAY:
                if (std::holds_alternative<std::string>(value))
                    return {.value = value, .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-string literals to array types", location);
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown type category", location);
    }

    bool Constant::operator== (const Constant& rhs) const {
        if (std::holds_alternative<IntegerConstant>(value) && std::holds_alternative<IntegerConstant>(rhs.value))
            return std::get<IntegerConstant>(value) == std::get<IntegerConstant>(rhs.value);
        else if (std::holds_alternative<FloatingPointConstant>(value) && std::holds_alternative<FloatingPointConstant>(rhs.value))
            return std::get<FloatingPointConstant>(value) == std::get<FloatingPointConstant>(rhs.value);
        else if (std::holds_alternative<std::string>(value) && std::holds_alternative<std::string>(rhs.value))
            return std::get<std::string>(value) == std::get<std::string>(rhs.value);
        else return false;
    }

    std::string Constant::ir_code() const {
        std::stringstream code;
        if (std::holds_alternative<IntegerConstant>(value))
            code << std::get<IntegerConstant>(value);
        else if (std::holds_alternative<FloatingPointConstant>(value))
            code << std::get<FloatingPointConstant>(value);
        else if (std::holds_alternative<std::string>(value))
            code << "\"" << std::get<IntegerConstant>(value) << "\"";
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown constant category", location);

        return code.str();
    }

    // -------- RValue
    RValue::RValue(std::shared_ptr<Declaration> declaration) : value(declaration) {}
    RValue::RValue(Constant value) : value(value) {}

    bool RValue::is_constant() const {
        return !std::holds_alternative<std::shared_ptr<Declaration>>(value);
    }

    CodeLocation RValue::location() const {
        if (is_constant())  return std::get<Constant>(value).location;
        else                return std::get<std::shared_ptr<Declaration>>(value)->location;
    }

    std::shared_ptr<Type> RValue::type() const {
        if (is_constant())  return std::get<Constant>(value).type;
        else                return std::get<std::shared_ptr<Declaration>>(value)->type;
    }

    Constant RValue::constant() const {
        if (is_constant())  return std::get<Constant>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the constant alternative of a non-constant rvalue", location());
    }

    Constant& RValue::constant() {
        if (is_constant())  return std::get<Constant>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the constant alternative of a non-constant rvalue", location());
    }

    std::shared_ptr<Declaration> RValue::declaration() const {
        if (!is_constant())  return std::get<std::shared_ptr<Declaration>>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the declaration alternative of a constant rvalue", location());
    }

    bool RValue::operator== (const RValue& rhs) const {
        if (is_constant() && rhs.is_constant())
            return std::get<Constant>(value) == std::get<Constant>(rhs.value);
        else if (!is_constant() && !rhs.is_constant())
            return std::get<std::shared_ptr<Declaration>>(value).get() == std::get<std::shared_ptr<Declaration>>(rhs.value).get();
        else return false;
    }

    std::string RValue::ir_code() const {
        if (is_constant())  return std::get<Constant>(value).ir_code();
        else                return std::get<std::shared_ptr<Declaration>>(value)->name;
    }

    // -------- LValue
    LValue::LValue(std::shared_ptr<Declaration> declaration) : base(declaration), location(declaration->location) {}
    LValue::LValue(RValue base, CodeLocation location, std::vector<RValue> indices) : base(base), location(location), indices(indices) {
        if (base.is_constant() && indices.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A constant can't be an lvalue without some form of dereferencing", location);
    }

    bool LValue::is_dereference() const {
        return !indices.empty();
    }

    std::string LValue::ir_code() const {
        std::stringstream code;
        code << base.ir_code();
        for (RValue index : indices)
            code << "[" << index.ir_code() << "]";
        return code.str();
    }

    std::shared_ptr<Type> LValue::type() const {
        std::shared_ptr<Type> type = base.type();
        for (RValue index : indices)
            type = type->dereference(location);
        return type;
    }
}
