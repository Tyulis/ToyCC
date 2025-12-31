#include "diagnostic.h"
#include "ir/generator.h"
#include <variant>

namespace toycc::ir {
    Generator::ExpressionResult::ExpressionResult(LValue result, CodeLocation location, Generator& generator)
        : result(result), location(location), generator(generator) {}

    Generator::ExpressionResult::ExpressionResult(RValue result, CodeLocation location, Generator& generator)
        : result(result), location(location), generator(generator) {}

    Generator::ExpressionResult::~ExpressionResult() {
        if (!postfix_increments.empty())
            apply_postfix_operations();
    }

    std::shared_ptr<Type> Generator::ExpressionResult::type() const {
        return std::visit([&](auto&& val) {return val.type();}, result);
    }

    bool Generator::ExpressionResult::is_lvalue() const {
        return std::holds_alternative<LValue>(result);
    }

    LValue Generator::ExpressionResult::lvalue() const {
        if (!is_lvalue())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to convert an rvalue expression result to an lvalue", location);
        return std::get<LValue>(result);
    }

    RValue Generator::ExpressionResult::load(CodeLocation location) const {
        if (std::holds_alternative<RValue>(result))
            return std::get<RValue>(result);

        LValue lvalue = std::get<LValue>(result);
        if (lvalue.indices.empty())
            return lvalue.base;

        // Otherwise emit a dereference
        std::shared_ptr<Declaration> destination = generator.declare_temporary(type(), location);
        generator.emit(Statement::make_load(location, lvalue, destination));
        return destination;
    }

    void Generator::ExpressionResult::store(RValue source, CodeLocation location) const {
        if (!is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't store into an rvalue", location);

        LValue lvalue = std::get<LValue>(result);
        RValue stored_value = generator.emit_implicit_conversion(lvalue.type(), source, location);
        generator.emit(Statement::make_unary_operation(location, StatementTag::COPY, stored_value, lvalue));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::ExpressionResult::dereference(RValue index, CodeLocation location) const {
        if (is_lvalue()) {
            LValue lvalue = std::get<LValue>(result);
            lvalue.indices.push_back(index);
            return generator.make_expression(lvalue, location);
        } else {
            RValue pointer = std::get<RValue>(result);
            LValue dereferenced(pointer, location, {generator.make_constant_zero(TypeCategory::INTEGER, location)});
            return generator.make_expression(dereferenced, location);
        }
    }

    void Generator::ExpressionResult::apply_postfix_operations() {
        // When the expression goes out of scope, apply the postfix operations
        // ++ and -- are only valid on pointer and integer lvalues
        if (!is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Postfix increment and decrement operations are only available on lvalues", location);

        LValue destination = std::get<LValue>(result);
        RValue left = load(location);

        for (int increment : postfix_increments) {
            Constant right = {.value = IntegerConstant(increment), .location = location, .type = generator.literal_integer_type};
            generator.emit(Statement::make_binary_operation(location, StatementTag::PLUS, left, right, destination));
        }
    }

    void Generator::ExpressionResult::apply_pointer_postfix_operations() {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Pointer arithmetic is not implemented", location);
    }

    void Generator::ExpressionResult::apply_integer_postfix_operations() {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Integer postfix operations are not implemented", location);
    }


    // Wrap a simple declaration into an ExpressionResult
    std::shared_ptr<Generator::ExpressionResult> Generator::make_expression(LValue lvalue, CodeLocation location) {
        return std::make_shared<ExpressionResult> (lvalue, location, *this);
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::make_expression(RValue rvalue, CodeLocation location) {
        return std::make_shared<ExpressionResult> (rvalue, location, *this);
    }
}
