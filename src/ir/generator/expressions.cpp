#include "diagnostic.h"
#include "ir/type_expressions.h"
#include "ir/generator.h"
#include "ir/statement.h"
#include <memory>

namespace toycc::ir {
    // ------------ Expressions

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_initializer(CParser::InitializerContext* context) {
        if (context->LeftBrace() || context->RightBrace())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Initializer lists are not implemented", locate(context));
        else if (context->assignmentExpression())
            return decode_assignment_expression(context->assignmentExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown initializer `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_expression(CParser::ExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result;
        for (CParser::AssignmentExpressionContext* expression : context->assignmentExpression())
            result = decode_assignment_expression(expression);
        return result;  // In a comma-separated list of expressions, return the last one
    }


    std::shared_ptr<Generator::ExpressionResult> Generator::decode_assignment_expression(CParser::AssignmentExpressionContext* context) {
        const CodeLocation location = locate(context);

        if (context->conditionalExpression())
            return decode_conditional_expression(context->conditionalExpression());
        else if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences are not supported as assignment expressions", location);

        const std::optional<stmt::BinaryOperator> op = decode_assignment_operator(context->assignmentOperator());
        std::shared_ptr<Generator::ExpressionResult> destination = decode_unary_expression(context->unaryExpression());
        if (!destination->is_lvalue)
            throw Diagnostic(DiagnosticLevel::ERROR, "Assignment destination must be an lvalue");
        std::shared_ptr<ExpressionResult> source = decode_assignment_expression(context->assignmentExpression());

        if (op.has_value())
            source = emit_binary_operation(op.value(), destination, source, location);

        destination->store(source->load(location), location);
        return source;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_conditional_expression(CParser::ConditionalExpressionContext* context) {
        std::shared_ptr<ExpressionResult> predicate = decode_logical_or_expression(context->logicalOrExpression());

        if (!context->Question())
            return predicate;

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Conditional expressions are not implemented", locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_logical_or_expression(CParser::LogicalOrExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_logical_and_expression(context->logicalAndExpression()[0]);
        if (context->logicalAndExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Logical OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_logical_and_expression(CParser::LogicalAndExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_inclusive_or_expression(context->inclusiveOrExpression()[0]);
        if (context->inclusiveOrExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Logical AND expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_inclusive_or_expression(CParser::InclusiveOrExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_exclusive_or_expression(context->exclusiveOrExpression()[0]);
        if (context->exclusiveOrExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inclusive OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_exclusive_or_expression(CParser::ExclusiveOrExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_and_expression(context->andExpression()[0]);
        if (context->andExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Exclusive OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_and_expression(CParser::AndExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_equality_expression(context->equalityExpression()[0]);
        if (context->equalityExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "AND expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_equality_expression(CParser::EqualityExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_relational_expression(context->relationalExpression()[0]);
        if (context->relationalExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Equality expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_relational_expression(CParser::RelationalExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_shift_expression(context->shiftExpression()[0]);
        if (context->shiftExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Relational expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_shift_expression(CParser::ShiftExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_additive_expression(context->additiveExpression()[0]);
        if (context->additiveExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Shift expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_additive_expression(CParser::AdditiveExpressionContext* context) {
        const std::vector<CParser::MultiplicativeExpressionContext*> operands = context->multiplicativeExpression();
        const std::vector<CParser::AdditiveOperatorContext*> operators = context->additiveOperator();

        std::shared_ptr<ExpressionResult> left = decode_multiplicative_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_multiplicative_expression(operands[operation_index + 1]);
            if (left->type()->is_arithmetic() && right->type()->is_arithmetic())
                left = emit_binary_operation(decode_additive_operator(operators[operation_index]), left, right, location);
            else
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-arithmetic additive expressions are not implemented");
        }
        return left;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_multiplicative_expression(CParser::MultiplicativeExpressionContext* context) {
        const std::vector<CParser::CastExpressionContext*> operands = context->castExpression();
        const std::vector<CParser::MultiplicativeOperatorContext*> operators = context->multiplicativeOperator();

        std::shared_ptr<ExpressionResult> left = decode_cast_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_cast_expression(operands[operation_index + 1]);
            if (left->type()->is_arithmetic() && right->type()->is_arithmetic())
                left = emit_binary_operation(decode_multiplicative_operator(operators[operation_index]), left, right, location);
            else
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-arithmetic multiplicative expressions are not implemented");
        }
        return left;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_cast_expression(CParser::CastExpressionContext* context) {
        if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences as cast expressions are not implemented", locate(context));
        else if (context->typeName())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Cast expressions are not implemented");
        else if (context->unaryExpression())
            return decode_unary_expression(context->unaryExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown cast expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_expression(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        if (!context->PlusPlus().empty() || !context->MinusMinus().empty() || !context->Sizeof().empty() || context->Alignof() || context->AndAnd())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary expressions are not implemented", locate(context));
        else if (context->postfixExpression())
            return decode_postfix_expression(context->postfixExpression());
        else if (context->unaryOperator() && context->castExpression())
            return decode_unary_operation(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown unary expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_operation(CParser::UnaryExpressionContext* context) {
        if      (context->unaryOperator()->And())    return decode_unary_addressof(context);
        else if (context->unaryOperator()->Star())   return decode_unary_dereference(context);
        else if (context->unaryOperator()->Plus())   return decode_unary_plus(context);
        else if (context->unaryOperator()->Minus())  return decode_unary_minus(context);
        else if (context->unaryOperator()->Tilde())  return decode_unary_bitwise_not(context);
        else if (context->unaryOperator()->Not())    return decode_unary_logical_not(context);
        else if (context->unaryOperator()->KW__extension__())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Extension unary operators are not supported", locate(context->unaryOperator()));
        else if (context->unaryOperator()->KW__real__() || context->unaryOperator()->KW__imag__())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Complex unary operators are not supported", locate(context->unaryOperator()));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown unary operator `{}`", context->getText()), locate(context->unaryOperator()));
    }


    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_addressof(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        std::shared_ptr<ExpressionResult> operand = decode_cast_expression(context->castExpression());
        if (!operand->is_lvalue)
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't take the address of an rvalue", locate(context));

        std::shared_ptr<Type> pointer_type = PointerType::make(anonymous_type(), location, operand->type());
        std::shared_ptr<Declaration> result = declare_temporary(pointer_type, location);
        current_scope()->add_statement(std::make_shared<stmt::AddressOf>(location, result, operand->lvalue()));
        return make_expression(result, true, false);
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_dereference(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        std::shared_ptr<ExpressionResult> operand = decode_cast_expression(context->castExpression());
        std::shared_ptr<Type> pointer_type = operand->type();
        if (pointer_type->category != TypeCategory::POINTER)
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to dereference a non-pointer object", location);

        std::shared_ptr<ExpressionResult> result = std::make_shared<ExpressionResult>(*operand);
        result->indices.insert(result->indices.cbegin(), nullptr);  // nullptr is a shortcut for a simple dereference here
        result->is_lvalue = true;
        result->is_constexpr = false;
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_plus(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary plus operations are not implemented", locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_minus(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary minus operations are not implemented", locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_bitwise_not(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary bitwise not operations are not implemented", locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_logical_not(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary logical not operations are not implemented", locate(context));
    }


    std::shared_ptr<Generator::ExpressionResult> Generator::decode_postfix_expression(CParser::PostfixExpressionContext* context) {
        if (!context->primaryExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inline initializer lists are not implemented");

        std::shared_ptr<ExpressionResult> result = decode_primary_expression(context->primaryExpression());
        for (CParser::PostfixOperatorContext* postfix : context->postfixOperator()) {
            const CodeLocation location = locate(postfix);
            if (postfix->LeftBracket() || postfix->RightBracket())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Array indexing is not implemented", location);
            else if (postfix->LeftParen() || postfix->RightParen())
                result = decode_function_call(result->load(location), postfix);
            else if (postfix->Dot() || postfix->Arrow())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Member access is not implemented", location);
            else if (postfix->PlusPlus())
                result->postfix_increments.push_back(1);
            else if (postfix->MinusMinus())
                result->postfix_increments.push_back(-1);
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown postfix operator `{}`", postfix->getText()));
        }
        return result;
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_primary_expression(CParser::PrimaryExpressionContext* context) {
        const CodeLocation location = locate(context);
        if (context->Identifier())
            return make_expression(resolve(context->Identifier()->getText(), location), true, false);  // FIXME : if the named variable is constexpr, this may also be constexpr ?
        else if (context->Constant())
            return make_expression(decode_constant(context->Constant()), false, true);
        else if (!context->StringLiteral().empty())
            return make_expression(decode_string_literal(context->StringLiteral()), false, false);  // The actual type is a pointer, which can't be constexpr. Is that a problem ?'
        else if (context->LeftParen() && context->expression() && context->RightParen())
            return decode_expression(context->expression());
        else if (context->genericSelection())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Generics are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Unknown primary expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_function_call(std::shared_ptr<Declaration> function, CParser::PostfixOperatorContext* call) {
        if (function->type->category != TypeCategory::FUNCTION)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't call object of type `{}` as a function", function->type->text()), locate(call));
        std::shared_ptr<FunctionType> function_type = std::static_pointer_cast<FunctionType> (function->type);

        std::vector<std::shared_ptr<Declaration>> parameters;
        if (call->argumentExpressionList()) {
            std::vector<CParser::AssignmentExpressionContext*> parameter_expressions = call->argumentExpressionList()->assignmentExpression();
            if (parameter_expressions.size() != function_type->parameters.size())
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid number of arguments : found {}, expected {}", parameter_expressions.size(), function_type->parameters.size()), locate(call));

            for (size_t param = 0; param < parameter_expressions.size(); param++) {
                const CodeLocation param_location = locate(parameter_expressions[param]);
                std::shared_ptr<ExpressionResult> expression_result = decode_assignment_expression(parameter_expressions[param]);
                std::shared_ptr<Declaration> parameter = emit_implicit_conversion(function_type->parameters[param].type, expression_result->load(param_location), param_location);
                parameters.push_back(parameter);
            }
        } else {
            if (!function_type->parameters.empty())
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid number of arguments : found 0, expected {}", function_type->parameters.size()), locate(call));
        }

        std::shared_ptr<Declaration> destination = declare_temporary(function_type->return_type, locate(call));
        current_scope()->add_statement(std::make_shared<stmt::Call>(locate(call), destination, function, parameters));
        return make_expression(destination, false, false);  // FIXME : May be constexpr when the function is inline ?
    }


    std::optional<stmt::BinaryOperator> Generator::decode_assignment_operator(CParser::AssignmentOperatorContext* context) {
        if      (context->Assign())            return {};
        else if (context->StarAssign())        return stmt::BinaryOperator::MUL;
        else if (context->DivAssign())         return stmt::BinaryOperator::DIV;
        else if (context->ModAssign())         return stmt::BinaryOperator::MOD;
        else if (context->PlusAssign())        return stmt::BinaryOperator::PLUS;
        else if (context->MinusAssign())       return stmt::BinaryOperator::MINUS;
        else if (context->LeftShiftAssign())   return stmt::BinaryOperator::LSHIFT;
        else if (context->RightShiftAssign())  return stmt::BinaryOperator::RSHIFT;
        else if (context->AndAssign())         return stmt::BinaryOperator::BITWISE_AND;
        else if (context->XorAssign())         return stmt::BinaryOperator::BITWISE_XOR;
        else if (context->OrAssign())          return stmt::BinaryOperator::BITWISE_OR;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown assignment operator {}", context->getText()), locate(context));
    }

    stmt::BinaryOperator Generator::decode_multiplicative_operator(CParser::MultiplicativeOperatorContext* context) {
        if      (context->Star())  return stmt::BinaryOperator::MUL;
        else if (context->Div())   return stmt::BinaryOperator::DIV;
        else if (context->Mod())   return stmt::BinaryOperator::MOD;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown multiplicative operator `{}`", context->getText()), locate(context));
    }

    stmt::BinaryOperator Generator::decode_additive_operator(CParser::AdditiveOperatorContext* context) {
        if      (context->Plus())  return stmt::BinaryOperator::PLUS;
        else if (context->Minus()) return stmt::BinaryOperator::MINUS;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown additive operator `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::emit_binary_operation(stmt::BinaryOperator op, std::shared_ptr<ExpressionResult> left, std::shared_ptr<ExpressionResult> right, CodeLocation location) {
        std::shared_ptr<Type> left_type = left->type(), right_type = right->type();

        if (!is_operator_valid(op, left_type, right_type))
            throw Diagnostic(DiagnosticLevel::ERROR, "Operation `{}` + `{}` is not valid");

        if (left_type->is_arithmetic() && right_type->is_arithmetic()) {
            auto [converted_left, converted_right] = emit_arithmetic_conversion(left->load(location), right->load(location), location);
            std::shared_ptr<Declaration> result = declare_temporary(converted_left->type, location);
            current_scope()->add_statement(std::make_shared<stmt::BinaryOp>(location, op, result, converted_left, converted_right));
            return make_expression(result, false, left->is_constexpr && right->is_constexpr);
        } else {
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-arithmetic operations are not implemented", location);
        }
    }

    bool Generator::is_operator_valid(stmt::BinaryOperator op, std::shared_ptr<Type> left, std::shared_ptr<Type> right) {
        std::shared_ptr<Type> left_unqualified = left->dequalify(), right_unqualified = right->dequalify();
        switch (op) {
            case stmt::BinaryOperator::MUL:
            case stmt::BinaryOperator::DIV:
            case stmt::BinaryOperator::MOD:
                return left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic();

            case stmt::BinaryOperator::PLUS:
                return (left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic()) ||
                       (left_unqualified->category == TypeCategory::POINTER && right_unqualified->is_arithmetic()) ||
                       (left_unqualified->is_arithmetic() && right_unqualified->category == TypeCategory::POINTER);

            case stmt::BinaryOperator::MINUS:
                return (left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic()) ||
                       (left_unqualified->category == TypeCategory::POINTER && right_unqualified->is_arithmetic());

            case stmt::BinaryOperator::LSHIFT:
            case stmt::BinaryOperator::RSHIFT:
            case stmt::BinaryOperator::BITWISE_AND:
            case stmt::BinaryOperator::BITWISE_XOR:
            case stmt::BinaryOperator::BITWISE_OR:
                return left_unqualified->is_integral() && right_unqualified->is_integral();

            case stmt::BinaryOperator::LT:
            case stmt::BinaryOperator::LE:
            case stmt::BinaryOperator::GE:
            case stmt::BinaryOperator::GT:
            case stmt::BinaryOperator::EQ:
            case stmt::BinaryOperator::NE:
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Relational operators are not implemented");

            case stmt::BinaryOperator::LOGICAL_AND:
            case stmt::BinaryOperator::LOGICAL_OR:
                return left_unqualified->has_truth_value() && right_unqualified->has_truth_value();
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown binary operator");
    }
}
