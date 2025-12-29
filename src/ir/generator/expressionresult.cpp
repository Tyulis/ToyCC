#include "diagnostic.h"
#include "ir/generator.h"

namespace toycc::ir {
    Generator::ExpressionResult::ExpressionResult(CodeLocation location, std::shared_ptr<Declaration> result, bool is_lvalue, bool is_constexpr, Generator& generator)
        : location(location), result(result), is_lvalue(is_lvalue), is_constexpr(is_constexpr), generator(generator) {}

    std::shared_ptr<Declaration> Generator::ExpressionResult::load(CodeLocation location) {
        if (indices.empty())
            return result;

        // Otherwise emit a dereference
        std::shared_ptr<Declaration> destination = generator.declare_temporary(type(), location);
        generator.current_scope()->add_statement(std::make_shared<stmt::DerefLoad>(location, destination, lvalue()));
        return destination;
    }

    void Generator::ExpressionResult::store(std::shared_ptr<Declaration> source, CodeLocation location) {
        if (!is_lvalue)
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't store into an rvalue", location);

        std::shared_ptr<Declaration> stored_value = generator.emit_implicit_conversion(type(), source, location);

        if (indices.empty())
            generator.current_scope()->add_statement(std::make_shared<stmt::Copy>(location, stmt::ConversionOperation::COPY, result, stored_value));
        else
            generator.current_scope()->add_statement(std::make_shared<stmt::DerefStore>(location, lvalue(), stored_value));
    }

    Generator::ExpressionResult::~ExpressionResult() {
        if (!postfix_increments.empty())
            apply_postfix_operations();
    }

    void Generator::ExpressionResult::apply_postfix_operations() {
        // When the expression goes out of scope, apply the postfix operations
        // ++ and -- are only valid on pointer and integer lvalues
        if (!is_lvalue)
            throw Diagnostic(DiagnosticLevel::ERROR, "Postfix increment and decrement operations are only available on lvalues", location);

        switch (result->type->category) {
            case TypeCategory::POINTER:  return apply_pointer_postfix_operations();
            case TypeCategory::INTEGER:  return apply_integer_postfix_operations();
            default: throw Diagnostic(DiagnosticLevel::ERROR, "Postfix increment and decrement operations are only available on integer and pointer lvalues", location);
        }
    }

    void Generator::ExpressionResult::apply_pointer_postfix_operations() {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Pointer arithmetic is not implemented", location);
    }

    void Generator::ExpressionResult::apply_integer_postfix_operations() {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Integer postfix operations are not implemented", location);
    }

    std::shared_ptr<Type> Generator::ExpressionResult::type() const {
        std::shared_ptr<Type> type = result->type;
        for (std::shared_ptr<Declaration> index : indices)
            type = type->dereference(location);
        return type;
    }

    LValue Generator::ExpressionResult::lvalue() const {
        if (!is_lvalue)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to convert an rvalue expression result to an lvalue", location);
        return {.base_declaration = result, .location = location, .indices = indices};
    }

    // Wrap a simple declaration into an ExpressionResult
    std::shared_ptr<Generator::ExpressionResult> Generator::make_expression(std::shared_ptr<Declaration> declaration, bool is_lvalue, bool is_constexpr) {
        return std::make_shared<ExpressionResult>(declaration->location, declaration, is_lvalue, is_constexpr, *this);
    }
}
