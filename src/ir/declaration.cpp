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
        for (StorageClass item : storage) {
            switch (item) {
                case StorageClass::AUTO:          repr << "auto ";          break;
                case StorageClass::STATIC:        repr << "static ";        break;
                case StorageClass::EXTERN:        repr << "extern ";        break;
                case StorageClass::REGISTER:      repr << "register ";      break;
                case StorageClass::THREAD_LOCAL:  repr << "thread_local ";  break;
                case StorageClass::TYPEDEF:       repr << "typedef ";       break;
                case StorageClass::PARAMETER:     repr << "parameter ";     break;
                case StorageClass::TEMPORARY:     repr << "temporary ";     break;
                case StorageClass::INTERMEDIATE:  repr << "intermediate ";  break;
                case StorageClass::GLOBAL:        repr << "global ";        break;
            }
        }

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
        return std::format("{}{}{}", storage_classes_repr(storage), function_specifiers_repr(function_spec), Member::ir_code());
    }


    // -------- Constant
    bool Constant::is_integer() const {
        return std::holds_alternative<IntegerConstant>(value);
    }

    bool Constant::is_floating_point() const {
        return std::holds_alternative<FloatingPointConstant>(value);
    }

    bool Constant::is_string() const {
        return std::holds_alternative<std::string>(value);
    }

    IntegerConstant Constant::integer() const {
        return std::get<IntegerConstant>(value);
    }

    FloatingPointConstant Constant::floating_point() const {
        return std::get<FloatingPointConstant>(value);
    }

    std::string Constant::string() const {
        return std::get<std::string>(value);
    }


    Constant Constant::as(std::shared_ptr<Type> new_type) const {
        std::shared_ptr<Type> new_unqualified = new_type->dequalify();
        switch (new_unqualified->category) {
            case TypeCategory::VOID:
            case TypeCategory::BUILTIN:
            case TypeCategory::LABEL:
            case TypeCategory::STRUCT:
            case TypeCategory::UNION:
            case TypeCategory::FUNCTION:
            case TypeCategory::BITFIELD:
            case TypeCategory::QUALIFIED:
            case TypeCategory::ALIGNED:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid type for a constant : `{}`", new_type->text()), location);

            case TypeCategory::POINTER:
                if (is_string())
                    return {.value = value, .location = location, .type = new_type};
            [[fallthrough]];

            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::ENUM:
                if (is_integer())
                    return {.value = value, .location = location, .type = new_type};
                else if (is_floating_point())
                    return {.value = IntegerConstant(floating_point()), .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-arithmetic constants to integers", location);

            case TypeCategory::FLOAT:
                if (is_integer())
                    return {.value = FloatingPointConstant(integer()), .location = location, .type = new_type};
                else if (is_floating_point())
                    return {.value = value, .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-arithmetic constants to integers", location);

            case TypeCategory::ARRAY:
                if (is_string())
                    return {.value = value, .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-string literals to array types", location);
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown type category", location);
    }

    bool Constant::operator== (const Constant& rhs) const {
        if (is_integer() && rhs.is_integer())
            return integer() == rhs.integer();
        else if (is_floating_point() && rhs.is_floating_point())
            return floating_point() == rhs.floating_point();
        else if (is_string() && rhs.is_string())
            return string() == rhs.string();
        else return false;
    }

    std::string Constant::ir_code() const {
        std::stringstream code;
        if (is_integer())
            code << integer();
        else if (is_floating_point())
            code << floating_point();
        else if (is_string())
            code << "\"" << string() << "\"";
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown constant category", location);

        return code.str();
    }

    // -------- Operand
    Operand::Operand(const Constant& constant, std::vector<Operand> indices) : value(constant), location(constant.location), indices(indices) {}
    Operand::Operand(const Constant& constant, CodeLocation location, std::vector<Operand> indices) : value(constant), location(location), indices(indices) {}
    Operand::Operand(std::shared_ptr<Declaration> declaration, std::vector<Operand> indices) : value(declaration), location(declaration->location), indices(indices) {}
    Operand::Operand(std::shared_ptr<Declaration> declaration, CodeLocation location, std::vector<Operand> indices) : value(declaration), location(location), indices(indices) {}
    Operand::Operand(std::string label, CodeLocation location, std::vector<Operand> indices) : value(label), location(location), indices(indices) {}
    Operand::Operand(std::variant<std::shared_ptr<Declaration>, Constant, std::string> value, CodeLocation location, std::vector<Operand> indices)
            : value(value), location(location), indices(indices) {}
    Operand::Operand(std::variant<std::shared_ptr<Declaration>, Constant, std::string> value, CodeLocation location, std::vector<Operand> indices, std::shared_ptr<Type> dereference_type)
            : value(value), location(location), indices(indices), dereference_type(dereference_type) {}

    bool Operand::is_label() const {
        return has_label_base() && !is_dereference();
    }

    bool Operand::is_constant() const {
        return has_constant_base() && !is_dereference();
    }

    bool Operand::is_variable() const {
        return has_variable_base() && !is_dereference();
    }

    bool Operand::is_dereference() const {
        return !indices.empty();
    }

    bool Operand::has_label_base() const {
        return std::holds_alternative<std::string>(value);
    }

    bool Operand::has_constant_base() const {
        return std::holds_alternative<Constant>(value);
    }

    bool Operand::has_variable_base() const {
        return std::holds_alternative<std::shared_ptr<Declaration>>(value);
    }

    std::shared_ptr<Type> Operand::base_type() const {
        if      (has_constant_base())  return std::get<Constant>(value).type;
        else if (has_variable_base())  return std::get<std::shared_ptr<Declaration>>(value)->type;
        else                           return std::make_shared<Type>(TypeCategory::LABEL, ".Tlabel", location);
    }

    std::shared_ptr<Type> Operand::type() const {
        if (!indices.empty() && dereference_type.get() != nullptr)
            return dereference_type;

        std::shared_ptr<Type> type = base_type();
        for (auto it = indices.begin(); it != indices.end(); it++)
            type = type->dereference(location);
        return type;
    }

    std::string Operand::label() const {
        if (has_label_base())  return std::get<std::string>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the label alternative of a non-label operand", location);
    }

    Constant Operand::constant() const {
        if (has_constant_base())  return std::get<Constant>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the constant alternative of a non-constant operand", location);
    }

    Constant& Operand::constant() {
        if (has_constant_base())  return std::get<Constant>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the constant alternative of a non-constant operand", location);
    }

    std::shared_ptr<Declaration> Operand::declaration() const {
        if (has_variable_base())  return std::get<std::shared_ptr<Declaration>>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the declaration alternative of a constant operand", location);
    }

    // Get the operand without the dereferencing indices
    Operand Operand::pointer() const {
        return Operand {value, location, {}};
    }

    bool Operand::operator== (const Operand& rhs) const {
        if (indices != rhs.indices)
            return false;

        if (has_constant_base() && rhs.has_constant_base())
            return constant() == rhs.constant();
        else if (has_variable_base() && rhs.has_variable_base())
            return declaration() == rhs.declaration();
        else if (has_label_base() && rhs.has_label_base())
            return label() == rhs.label();
        else return false;
    }

    std::string Operand::ir_code() const {
        std::stringstream code;
        if      (has_constant_base())  code << constant().ir_code();
        else if (has_variable_base())  code << declaration()->name;
        else                           code << label();

        for (const Operand& index : indices)
            code << "[" << index.ir_code() << "]";

        if (dereference_type.get() != nullptr)
            code << ":" << dereference_type->ir_code();
        return code.str();
    }
}
