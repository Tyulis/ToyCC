#include "diagnostic.h"
#include <variant>
#include "semantic/values.h"

namespace toycc::semantic {
    // -------- RValue
    RValue::RValue(std::shared_ptr<Declaration> declaration) : value(declaration) {}
    RValue::RValue(Constant value) : value(value) {}

    RValue::operator Operand() const {
        if (is_constant()) return {constant(),    location()};
        else               return {declaration(), location()};
    }

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

    std::shared_ptr<Declaration> RValue::declaration() const {
        if (!is_constant())  return std::get<std::shared_ptr<Declaration>>(value);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to access the declaration alternative of a constant rvalue", location());
    }

    std::optional<size_t> RValue::as_index() const {
        if (is_constant()) {
            if (constant().tag() == Constant::INTEGER)
                return static_cast<size_t>(constant().integer());
            else
                throw Diagnostic(DiagnosticLevel::ERROR, "Constant indices must be integers", location());
        } else {
            return {};
        }
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

    LValue::operator Operand() const {
        Operand base_operand(base);
        for (const RValue& index : indices)
            base_operand.indices.push_back(index);
        return base_operand;
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
            type = type->dereference(index.as_index(), location);
        return type;
    }


    // -------- Designation
    Designation::Tag Designation::tag() const {
        if (std::holds_alternative<std::monostate>(designation))
            return POSITIONAL;
        else if (std::holds_alternative<size_t>(designation))
            return INDEX;
        else if (std::holds_alternative<std::string>(designation))
            return NAME;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid designation type");
    }

    size_t Designation::index() const {
        return std::get<size_t>(designation);
    }

    std::string Designation::name() const {
        return std::get<std::string>(designation);
    }
}
