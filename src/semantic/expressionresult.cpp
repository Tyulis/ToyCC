#include <variant>

#include "diagnostic.h"
#include "semantic/analyzer.h"
#include "semantic/values.h"

namespace toycc::semantic {
    // -------- ExpressionResult
    SemanticAnalyzer::ExpressionResult::ExpressionResult(LValue result, CodeLocation location, SemanticAnalyzer& analyzer)
        : result(result), location(location), analyzer(analyzer) {}

    SemanticAnalyzer::ExpressionResult::ExpressionResult(RValue result, CodeLocation location, SemanticAnalyzer& analyzer)
        : result(result), location(location), analyzer(analyzer) {}

    SemanticAnalyzer::ExpressionResult::~ExpressionResult() {
        if (!postfix_increments.empty())
            apply_postfix_operations();
    }

    std::shared_ptr<Type> SemanticAnalyzer::ExpressionResult::type() const {
        return std::visit([&](auto&& val) {return val.type();}, result);
    }

    bool SemanticAnalyzer::ExpressionResult::is_lvalue() const {
        return std::holds_alternative<LValue>(result);
    }

    LValue SemanticAnalyzer::ExpressionResult::lvalue() const {
        if (!is_lvalue())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to convert an rvalue expression result to an lvalue", location);
        return std::get<LValue>(result);
    }

    RValue SemanticAnalyzer::ExpressionResult::rvalue() const {
        if (is_lvalue())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to convert an lvalue expression result to an rvalue", location);
        return std::get<RValue>(result);
    }

    Operand SemanticAnalyzer::ExpressionResult::operand() const {
        if (is_lvalue())  return lvalue();
        else              return rvalue();
    }

    RValue SemanticAnalyzer::ExpressionResult::base() const {
        if (is_lvalue())  return lvalue().base;
        else              return rvalue();
    }

    std::vector<RValue>SemanticAnalyzer::ExpressionResult::indices() const {
        if (is_lvalue())  return lvalue().indices;
        else              return {};
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::ExpressionResult::dereference(RValue index, CodeLocation location) const {
        if (is_lvalue()) {
            LValue lvalue = std::get<LValue>(result);
            lvalue.indices.push_back(index);
            return analyzer.make_expression(lvalue, location);
        } else {
            RValue pointer = std::get<RValue>(result);
            LValue dereferenced(pointer, location, {analyzer.make_constant_zero(TypeCategory::INTEGER, location)});
            return analyzer.make_expression(dereferenced, location);
        }
    }

    void SemanticAnalyzer::ExpressionResult::apply_postfix_operations() {
        // When the expression goes out of scope, apply the postfix operations
        // ++ and -- are only valid on pointer and integer lvalues
        if (!is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Postfix increment and decrement operations are only available on lvalues", location);

        Operand value = operand();
        std::shared_ptr<Type> value_type = value.type()->dequalify();

        IntegerConstant factor = 0;
        switch (value_type->category) {
            case TypeCategory::INTEGER:
                factor = 1;
                break;

            case TypeCategory::POINTER:
                factor = value_type->dereference({}, location)->size(location);
                break;

            default: throw Diagnostic(DiagnosticLevel::ERROR, "Postfix increment and decrement operators are only available on integer and pointer operands", location);
        }

        for (int increment : postfix_increments) {
            Constant right = {.value = IntegerConstant(increment * factor), .location = location, .type = analyzer.literal_integer_type};
            analyzer.emit(Statement::make_binary_operation(location, StatementTag::ADD, value, right, value));
        }
    }

    // Wrap a simple declaration into an ExpressionResult
    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::make_expression(LValue lvalue, CodeLocation location) {
        return std::make_shared<ExpressionResult> (lvalue, location, *this);
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::make_expression(RValue rvalue, CodeLocation location) {
        return std::make_shared<ExpressionResult> (rvalue, location, *this);
    }
}
