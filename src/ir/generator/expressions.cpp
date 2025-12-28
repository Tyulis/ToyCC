#include "diagnostic.h"
#include "ir/generator.h"
#include "ir/statement.h"

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

        if (op.has_value()) {
            auto [converted_left, converted_right] = emit_arithmetic_conversion(destination->load(location), source->load(location), location);
            std::shared_ptr<Declaration> result = declare_temporary(converted_left->spec, location);
            current_scope()->add_statement(std::make_shared<stmt::BinaryOp>(locate(context), op.value(), result, converted_left, converted_right));
            source = make_expression(result, false);  // The value to assign is now the result of that operation
        }

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
            if (left->result->spec.is_pointer_type() || right->result->spec.is_pointer_type())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Pointer arithmetic is not implemented");
            auto [converted_left, converted_right] = emit_arithmetic_conversion(left->load(location), right->load(location), location);

            std::shared_ptr<Declaration> result = declare_temporary(converted_left->spec, location);
            current_scope()->add_statement(std::make_shared<stmt::BinaryOp>(locate(context), decode_additive_operator(operators[operation_index]), result, converted_left, converted_right));
            left = make_expression(result, false);
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
            auto [converted_left, converted_right] = emit_arithmetic_conversion(left->load(location), right->load(location), location);

            std::shared_ptr<Declaration> result = declare_temporary(converted_left->spec, location);
            current_scope()->add_statement(std::make_shared<stmt::BinaryOp>(location, decode_multiplicative_operator(operators[operation_index]), result, converted_left, converted_right));
            left = make_expression(result, false);
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

        ir::TypeSpecification result_spec = operand->type();
        result_spec.pointer_spec.emplace(result_spec.pointer_spec.cbegin());

        std::shared_ptr<Declaration> result = declare_temporary(result_spec, location);
        current_scope()->add_statement(std::make_shared<stmt::AddressOf>(location, result, operand->lvalue()));
        return make_expression(result, true);
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_unary_dereference(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        std::shared_ptr<ExpressionResult> operand = decode_cast_expression(context->castExpression());
        ir::TypeSpecification operand_spec = operand->type();
        if (!operand_spec.is_pointer_type())
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to dereference a non-pointer object", location);

        std::shared_ptr<ExpressionResult> result = std::make_shared<ExpressionResult>(*operand);
        result->indices.insert(result->indices.cbegin(), nullptr);  // nullptr is a shortcut for a simple dereference here
        result->is_lvalue = true;
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
            return make_expression(resolve(context->Identifier()->getText(), location), true);
        else if (context->Constant())
            return make_expression(decode_constant(context->Constant()), false);
        else if (!context->StringLiteral().empty())
            return make_expression(decode_string_literal(context->StringLiteral()), false);
        else if (context->LeftParen() && context->expression() && context->RightParen())
            return decode_expression(context->expression());
        else if (context->genericSelection())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Generics are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Unknown primary expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<Generator::ExpressionResult> Generator::decode_function_call(std::shared_ptr<Declaration> function, CParser::PostfixOperatorContext* call) {
        std::vector<std::shared_ptr<Declaration>> parameters;
        if (call->argumentExpressionList()) {
            std::vector<CParser::AssignmentExpressionContext*> parameter_expressions = call->argumentExpressionList()->assignmentExpression();
            if (parameter_expressions.size() != function->spec.parameters.size())
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid number of arguments : found {}, expected {}", parameter_expressions.size(), function->spec.parameters.size()), locate(call));

            for (size_t param = 0; param < parameter_expressions.size(); param++) {
                const CodeLocation param_location = locate(parameter_expressions[param]);
                std::shared_ptr<ExpressionResult> expression_result = decode_assignment_expression(parameter_expressions[param]);
                std::shared_ptr<Declaration> parameter = emit_implicit_conversion(function->spec.parameters[param].spec, expression_result->load(param_location), param_location);
                parameters.push_back(parameter);
            }
        } else {
            if (!function->spec.parameters.empty())
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid number of arguments : found 0, expected {}", function->spec.parameters.size()), locate(call));
        }

        std::shared_ptr<Declaration> destination = declare_temporary(function->spec.return_type(), locate(call));
        current_scope()->add_statement(std::make_shared<stmt::Call>(locate(call), destination, function, parameters));
        return make_expression(destination, false);
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
