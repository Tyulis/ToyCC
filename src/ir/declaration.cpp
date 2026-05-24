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

    std::string Member::ir_code(std::unordered_set<const Type*> parents) const {
        return std::format("{} {}", type->ir_code(parents), name);
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

        if (!type->complete())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't declare `{}` with incomplete type `{}`", name, type->repr()), location);
    }

    std::string Declaration::ir_code() const {
        return std::format("{}{}{}", storage_classes_repr(storage), function_specifiers_repr(function_spec), Member::ir_code());
    }


    // -------- PointerConstant
    bool PointerConstant::operator== (const PointerConstant& rhs) const {
        return rhs.label == rhs.label && offset == rhs.offset;
    }

    std::ostream& operator<< (std::ostream& stream, const PointerConstant& pointer) {
        stream << "&" << pointer.label;
        if (pointer.offset != 0)
            stream << "+" << pointer.offset;
        return stream;
    }

    // -------- Constant
    Constant::Tag Constant::tag() const {
        if (std::holds_alternative<IntegerConstant>(value))
            return Constant::INTEGER;
        else if (std::holds_alternative<FloatingPointConstant>(value))
            return Constant::FLOAT;
        else if (std::holds_alternative<PointerConstant>(value))
            return Constant::POINTER;
        else if (std::holds_alternative<std::string>(value))
            return Constant::STRING;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown constant tag", location);
    }

    IntegerConstant Constant::integer() const {
        return std::get<IntegerConstant>(value);
    }

    FloatingPointConstant Constant::floating_point() const {
        return std::get<FloatingPointConstant>(value);
    }

    PointerConstant Constant::pointer() const {
        return std::get<PointerConstant>(value);
    }

    std::string Constant::string() const {
        return std::get<std::string>(value);
    }


    Constant Constant::as(std::shared_ptr<Type> new_type) const {
        std::shared_ptr<Type> new_unqualified = new_type->dequalify();
        const Constant::Tag value_tag = tag();
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

            case TypeCategory::LABEL:
                if (value_tag == Constant::POINTER)
                    return {.value = value, .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-pointer constants to jump targets", location);

            case TypeCategory::POINTER:
                if (value_tag == Constant::STRING || value_tag == Constant::POINTER)
                    return {.value = value, .location = location, .type = new_type};
            [[fallthrough]];

            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::ENUM:
                if (value_tag == Constant::INTEGER)
                    return {.value = value, .location = location, .type = new_type};
                else if (value_tag == Constant::FLOAT)
                    return {.value = IntegerConstant(floating_point()), .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-arithmetic constants to integers", location);

            case TypeCategory::FLOAT:
                if (value_tag == Constant::INTEGER)
                    return {.value = FloatingPointConstant(integer()), .location = location, .type = new_type};
                else if (value_tag == Constant::FLOAT)
                    return {.value = value, .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-arithmetic constants to integers", location);

            case TypeCategory::ARRAY:
                if (value_tag == Constant::STRING)
                    return {.value = value, .location = location, .type = new_type};
                else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert non-string literals to array types", location);
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown type category", location);
    }

    bool Constant::operator== (const Constant& rhs) const {
        return value == rhs.value;
    }

    std::string Constant::ir_code() const {
        std::stringstream code;
        switch (tag()) {
            case Constant::INTEGER:  code << integer();              break;
            case Constant::FLOAT:    code << floating_point();       break;
            case Constant::POINTER:  code << pointer();              break;
            case Constant::STRING:   code << string();               break;
        }
        return code.str();
    }

    // -------- Operand
    Operand::Operand(const Constant& constant, std::vector<Operand> indices) : value(constant), location(constant.location), indices(indices) {}
    Operand::Operand(const Constant& constant, CodeLocation location, std::vector<Operand> indices) : value(constant), location(location), indices(indices) {}
    Operand::Operand(std::shared_ptr<Declaration> declaration, std::vector<Operand> indices) : value(declaration), location(declaration->location), indices(indices) {}
    Operand::Operand(std::shared_ptr<Declaration> declaration, CodeLocation location, std::vector<Operand> indices) : value(declaration), location(location), indices(indices) {}
    Operand::Operand(std::variant<std::shared_ptr<Declaration>, Constant> value, CodeLocation location, std::vector<Operand> indices)
            : value(value), location(location), indices(indices) {}

    Operand::BaseTag Operand::base_tag() const {
        if (std::holds_alternative<Constant>(value))
            return Operand::CONSTANT_BASE;
        else if (std::holds_alternative<std::shared_ptr<Declaration>>(value))
            return Operand::VARIABLE_BASE;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown operand tag", location);
    }

    Operand::Tag Operand::tag() const {
        if (!indices.empty())
            return Operand::DEREFERENCE;

        switch (base_tag()) {
            case Operand::CONSTANT_BASE:  return Operand::CONSTANT;
            case Operand::VARIABLE_BASE:  return Operand::VARIABLE;
        }
        __builtin_unreachable();
    }

    std::shared_ptr<Type> Operand::base_type() const {
        switch (base_tag()) {
            case Operand::CONSTANT_BASE:  return std::get<Constant>(value).type;
            case Operand::VARIABLE_BASE:  return std::get<std::shared_ptr<Declaration>>(value)->type;
        }
        __builtin_unreachable();
    }

    std::shared_ptr<Type> Operand::type() const {
        std::shared_ptr<Type> type = base_type();
        for (const Operand& index : indices)
            type = type->dereference(index.as_index(), location);
        return type;
    }

    Constant Operand::constant() const {
        if (base_tag() == Operand::CONSTANT_BASE)  return std::get<Constant>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the constant alternative of a non-constant operand", location);
    }

    Constant& Operand::constant() {
        if (base_tag() == Operand::CONSTANT_BASE)  return std::get<Constant>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the constant alternative of a non-constant operand", location);
    }

    std::shared_ptr<Declaration> Operand::declaration() const {
        if (base_tag() == Operand::VARIABLE_BASE)  return std::get<std::shared_ptr<Declaration>>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the declaration alternative of a constant operand", location);
    }

    // Get the operand without the dereferencing indices
    Operand Operand::pointer() const {
        return Operand {value, location, {}};
    }

    std::optional<size_t> Operand::as_index() const {
        switch (tag()) {
            case Operand::CONSTANT:
                if (constant().tag() == Constant::INTEGER)
                    return static_cast<size_t>(constant().integer());
                else
                    throw Diagnostic(DiagnosticLevel::ERROR, "Constant indices must be integers", location);

            case Operand::VARIABLE:
            case Operand::DEREFERENCE:
                return {};
        }
        __builtin_unreachable();
    }

    bool Operand::operator== (const Operand& rhs) const {
        if (indices != rhs.indices)
            return false;

        switch (base_tag()) {
            case Operand::CONSTANT_BASE:  return constant() == rhs.constant();
            case Operand::VARIABLE_BASE:  return declaration() == rhs.declaration();
        }
        __builtin_unreachable();
    }

    std::string Operand::ir_code() const {
        std::stringstream code;

        switch (base_tag()) {
            case Operand::CONSTANT_BASE:  code << constant().ir_code();  break;
            case Operand::VARIABLE_BASE:  code << declaration()->name;   break;
        }

        for (const Operand& index : indices)
            code << "[" << index.ir_code() << "]";

        return code.str();
    }
}
