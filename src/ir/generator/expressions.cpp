#include "diagnostic.h"
#include "ir/generator.h"

namespace toycc::ir {
    // ------------ Expressions

    std::shared_ptr<Declaration> Generator::decode_initializer(CParser::InitializerContext* context) {
        if (context->LeftBrace() || context->RightBrace())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Initializer lists are not implemented", locate(context));
        else if (context->assignmentExpression())
            return decode_assignment_expression(context->assignmentExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown initializer `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Declaration> Generator::decode_expression(CParser::ExpressionContext* context) {
        std::shared_ptr<Declaration> result;
        for (CParser::AssignmentExpressionContext* expression : context->assignmentExpression())
            result = decode_assignment_expression(expression);
        return result;  // In a comma-separated list of expressions, return the last one
    }


    std::shared_ptr<Declaration> Generator::decode_assignment_expression(CParser::AssignmentExpressionContext* context) {
        if (context->conditionalExpression())
            return decode_conditional_expression(context->conditionalExpression());

        if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences are not supported as assignment expressions", locate(context));

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Assignment expressions are not implemented", locate(context));
        //const std::optional<stmt::BinaryOperator> op = decode_assignment_operator(context->assignmentOperator());
        //std::shared_ptr<Declaration> right_operand = decode_assignment_expression(context->assignmentExpression());
    }

    std::shared_ptr<Declaration> Generator::decode_conditional_expression(CParser::ConditionalExpressionContext* context) {
        std::shared_ptr<Declaration> predicate = decode_logical_or_expression(context->logicalOrExpression());

        if (!context->Question())
            return predicate;

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Conditional expressions are not implemented", locate(context));
    }

    std::shared_ptr<Declaration> Generator::decode_logical_or_expression(CParser::LogicalOrExpressionContext* context) {
        std::shared_ptr<Declaration> result = decode_logical_and_expression(context->logicalAndExpression()[0]);
        if (context->logicalAndExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Logical OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_logical_and_expression(CParser::LogicalAndExpressionContext* context) {
        std::shared_ptr<Declaration> result = decode_inclusive_or_expression(context->inclusiveOrExpression()[0]);
        if (context->inclusiveOrExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Logical AND expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_inclusive_or_expression(CParser::InclusiveOrExpressionContext* context) {
        std::shared_ptr<Declaration> result = decode_exclusive_or_expression(context->exclusiveOrExpression()[0]);
        if (context->exclusiveOrExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inclusive OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_exclusive_or_expression(CParser::ExclusiveOrExpressionContext* context) {
        std::shared_ptr<Declaration> result = decode_and_expression(context->andExpression()[0]);
        if (context->andExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Exclusive OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_and_expression(CParser::AndExpressionContext* context) {
        std::shared_ptr<Declaration> result = decode_equality_expression(context->equalityExpression()[0]);
        if (context->equalityExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "AND expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_equality_expression(CParser::EqualityExpressionContext* context) {
        std::shared_ptr<Declaration> result = decode_relational_expression(context->relationalExpression()[0]);
        if (context->relationalExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Equality expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_relational_expression(CParser::RelationalExpressionContext* context) {
        std::shared_ptr<Declaration> result = decode_shift_expression(context->shiftExpression()[0]);
        if (context->shiftExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Relational expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_shift_expression(CParser::ShiftExpressionContext* context) {
        std::shared_ptr<Declaration> result = decode_additive_expression(context->additiveExpression()[0]);
        if (context->additiveExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Shift expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_additive_expression(CParser::AdditiveExpressionContext* context) {
        const std::vector<CParser::MultiplicativeExpressionContext*> operands = context->multiplicativeExpression();
        const std::vector<CParser::AdditiveOperatorContext*> operators = context->additiveOperator();

        std::shared_ptr<Declaration> left = decode_multiplicative_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            auto [converted_left, converted_right] = emit_arithmetic_conversion(left, decode_multiplicative_expression(operands[operation_index + 1]), location);

            std::shared_ptr<Declaration> result = declare_temporary(converted_left->spec, location);
            current_scope()->add_statement(std::make_shared<stmt::BinaryOp>(locate(context), decode_additive_operator(operators[operation_index]), result, converted_left, converted_right));
            left = result;
        }
        return left;
    }

    std::shared_ptr<Declaration> Generator::decode_multiplicative_expression(CParser::MultiplicativeExpressionContext* context) {
        const std::vector<CParser::CastExpressionContext*> operands = context->castExpression();
        const std::vector<CParser::MultiplicativeOperatorContext*> operators = context->multiplicativeOperator();

        std::shared_ptr<Declaration> left = decode_cast_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            auto [converted_left, converted_right] = emit_arithmetic_conversion(left, decode_cast_expression(operands[operation_index + 1]), location);

            std::shared_ptr<Declaration> result = declare_temporary(converted_left->spec, location);
            current_scope()->add_statement(std::make_shared<stmt::BinaryOp>
            (locate(context), decode_multiplicative_operator(operators[operation_index]), result, converted_left, converted_right));
            left = result;
        }
        return left;
    }

    std::shared_ptr<Declaration> Generator::decode_cast_expression(CParser::CastExpressionContext* context) {
        if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences as cast expressions are not implemented", locate(context));
        else if (context->typeName())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Cast expressions are not implemented");
        else if (context->unaryExpression())
            return decode_unary_expression(context->unaryExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown cast expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Declaration> Generator::decode_unary_expression(CParser::UnaryExpressionContext* context) {
        if (!context->PlusPlus().empty() || !context->MinusMinus().empty() || !context->Sizeof().empty() || context->Alignof() || context->AndAnd() || context->unaryOperator() || context->castExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary expressions are not implemented", locate(context));

        return decode_postfix_expression(context->postfixExpression());
    }

    std::shared_ptr<Declaration> Generator::decode_postfix_expression(CParser::PostfixExpressionContext* context) {
        if (!context->primaryExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inline rvalue initializers are not implemented");

        std::shared_ptr<Declaration> result = decode_primary_expression(context->primaryExpression());
        for (CParser::PostfixOperatorContext* postfix : context->postfixOperator()) {
            const CodeLocation location = locate(postfix);
            if (postfix->LeftBracket() || postfix->RightBracket())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Array indexing is not implemented", location);
            else if (postfix->LeftParen() || postfix->RightParen())
                result = decode_function_call(result, postfix);
            else if (postfix->Dot() || postfix->Arrow())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Member access is not implemented", location);
            else if (postfix->PlusPlus() || postfix->MinusMinus())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Incrementation and decrementation operators are not implemented", location);
        }
        return result;
    }

    std::shared_ptr<Declaration> Generator::decode_primary_expression(CParser::PrimaryExpressionContext* context) {
        if (context->Identifier())
            return resolve(context->Identifier()->getText(), locate(context->Identifier()));
        else if (context->Constant())
            return decode_constant(context->Constant());
        else if (!context->StringLiteral().empty())
            return decode_string_literal(context->StringLiteral());
        else if (context->LeftParen() && context->expression() && context->RightParen())
            return decode_expression(context->expression());
        else if (context->genericSelection())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Generics are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Unknown primary expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Declaration> Generator::decode_function_call(std::shared_ptr<Declaration> function, CParser::PostfixOperatorContext* call) {
        std::shared_ptr<Declaration> destination = declare_temporary(function->spec.return_type(), locate(call));
        std::vector<std::shared_ptr<Declaration>> parameters;
        if (call->argumentExpressionList()) {
            std::vector<CParser::AssignmentExpressionContext*> parameter_expressions = call->argumentExpressionList()->assignmentExpression();
            if (parameter_expressions.size() != function->spec.parameters.size())
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid number of arguments : found {}, expected {}", parameter_expressions.size(), function->spec.parameters.size()), locate(call));

            for (size_t param = 0; param < parameter_expressions.size(); param++) {
                std::shared_ptr<Declaration> expression_result = decode_assignment_expression(parameter_expressions[param]);
                std::shared_ptr<Declaration> parameter = emit_implicit_conversion(function->spec.parameters[param].spec, expression_result, locate(parameter_expressions[param]));
                parameters.push_back(parameter);
            }
        } else {
            if (!function->spec.parameters.empty())
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid number of arguments : found 0, expected {}", function->spec.parameters.size()), locate(call));
        }

        current_scope()->add_statement(std::make_shared<stmt::Call>(locate(call), destination, function, parameters));
        return destination;
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
}
