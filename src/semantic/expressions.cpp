#include <memory>
#include <concepts>
#include <algorithm>

#include "diagnostic.h"
#include "ir/type_expressions.h"
#include "ir/statement.h"
#include "semantic/analyzer.h"

namespace toycc::semantic {
    // ------------ Expressions
    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_initializer(CParser::InitializerContext* context) {
        if (context->LeftBrace() || context->RightBrace())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Initializer lists are not implemented", locate(context));
        else if (context->assignmentExpression())
            return decode_assignment_expression(context->assignmentExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown initializer `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_expression(CParser::ExpressionContext* context) {
        return decode_expression_list(context->assignmentExpression());
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_for_expression(CParser::ForExpressionContext* context) {
        return decode_expression_list(context->assignmentExpression());
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_expression_list(std::vector<CParser::AssignmentExpressionContext*> context) {
        std::shared_ptr<ExpressionResult> result;
        for (CParser::AssignmentExpressionContext* expression : context)
            result = decode_assignment_expression(expression);
        return result;  // In a comma-separated list of expressions, return the last one
    }


    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_assignment_expression(CParser::AssignmentExpressionContext* context) {
        const CodeLocation location = locate(context);

        if (context->conditionalExpression())
            return decode_conditional_expression(context->conditionalExpression());
        else if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences are not supported as assignment expressions", location);

        StatementTag op = decode_assignment_operator(context->assignmentOperator());
        std::shared_ptr<SemanticAnalyzer::ExpressionResult> destination = decode_unary_expression(context->unaryExpression());
        if (!destination->is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Assignment destination must be an lvalue");
        std::shared_ptr<ExpressionResult> source = decode_assignment_expression(context->assignmentExpression());

        if (op == StatementTag::COPY)
            emit_copy(destination->operand(), source->operand(), location, false);
        else
            emit_binary_operation(op, destination, source, destination, location);

        return destination;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_conditional_expression(CParser::ConditionalExpressionContext* context) {
        std::shared_ptr<ExpressionResult> predicate = decode_logical_or_expression(context->logicalOrExpression());

        if (!context->Question())
            return predicate;

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Conditional expressions are not implemented", locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_logical_or_expression(CParser::LogicalOrExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_logical_and_expression(context->logicalAndExpression()[0]);
        if (context->logicalAndExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Logical OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_logical_and_expression(CParser::LogicalAndExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_inclusive_or_expression(context->inclusiveOrExpression()[0]);
        if (context->inclusiveOrExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Logical AND expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_inclusive_or_expression(CParser::InclusiveOrExpressionContext* context) {
        const std::vector<CParser::ExclusiveOrExpressionContext*> operands = context->exclusiveOrExpression();
        const std::vector<antlr4::tree::TerminalNode*> operators = context->Or();

        std::shared_ptr<ExpressionResult> left = decode_exclusive_or_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_exclusive_or_expression(operands[operation_index + 1]);
            left = emit_binary_operation(StatementTag::BITWISE_OR, left, right, location);
        }
        return left;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_exclusive_or_expression(CParser::ExclusiveOrExpressionContext* context) {
        const std::vector<CParser::AndExpressionContext*> operands = context->andExpression();
        const std::vector<antlr4::tree::TerminalNode*> operators = context->Caret();

        std::shared_ptr<ExpressionResult> left = decode_and_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_and_expression(operands[operation_index + 1]);
            left = emit_binary_operation(StatementTag::BITWISE_XOR, left, right, location);
        }
        return left;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_and_expression(CParser::AndExpressionContext* context) {
        const std::vector<CParser::EqualityExpressionContext*> operands = context->equalityExpression();
        const std::vector<antlr4::tree::TerminalNode*> operators = context->And();

        std::shared_ptr<ExpressionResult> left = decode_equality_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_equality_expression(operands[operation_index + 1]);
            left = emit_binary_operation(StatementTag::BITWISE_AND, left, right, location);
        }
        return left;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_equality_expression(CParser::EqualityExpressionContext* context) {
        const std::vector<CParser::RelationalExpressionContext*> operands = context->relationalExpression();
        const std::vector<CParser::EqualityOperatorContext*> operators = context->equalityOperator();

        std::shared_ptr<ExpressionResult> left = decode_relational_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_relational_expression(operands[operation_index + 1]);
            left = emit_binary_operation(decode_equality_operator(operators[operation_index]), left, right, location);
        }
        return left;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_relational_expression(CParser::RelationalExpressionContext* context) {
        const std::vector<CParser::ShiftExpressionContext*> operands = context->shiftExpression();
        const std::vector<CParser::RelationalOperatorContext*> operators = context->relationalOperator();

        std::shared_ptr<ExpressionResult> left = decode_shift_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_shift_expression(operands[operation_index + 1]);
            left = emit_binary_operation(decode_relational_operator(operators[operation_index]), left, right, location);
        }
        return left;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_shift_expression(CParser::ShiftExpressionContext* context) {
        std::shared_ptr<ExpressionResult> result = decode_additive_expression(context->additiveExpression()[0]);
        if (context->additiveExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Shift expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_additive_expression(CParser::AdditiveExpressionContext* context) {
        const std::vector<CParser::MultiplicativeExpressionContext*> operands = context->multiplicativeExpression();
        const std::vector<CParser::AdditiveOperatorContext*> operators = context->additiveOperator();

        std::shared_ptr<ExpressionResult> left = decode_multiplicative_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_multiplicative_expression(operands[operation_index + 1]);
            if (left->type()->is_arithmetic() && right->type()->is_arithmetic())
                left = emit_binary_operation(decode_additive_operator(operators[operation_index]), left, right, location);
            else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-arithmetic additive expressions are not implemented");
        }
        return left;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_multiplicative_expression(CParser::MultiplicativeExpressionContext* context) {
        const std::vector<CParser::CastExpressionContext*> operands = context->castExpression();
        const std::vector<CParser::MultiplicativeOperatorContext*> operators = context->multiplicativeOperator();

        std::shared_ptr<ExpressionResult> left = decode_cast_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            std::shared_ptr<ExpressionResult> right = decode_cast_expression(operands[operation_index + 1]);
            if (left->type()->is_arithmetic() && right->type()->is_arithmetic())
                left = emit_binary_operation(decode_multiplicative_operator(operators[operation_index]), left, right, location);
            else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-arithmetic multiplicative expressions are not implemented");
        }
        return left;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_cast_expression(CParser::CastExpressionContext* context) {
        if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences as cast expressions are not implemented", locate(context));
        else if (context->typeName())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Cast expressions are not implemented");
        else if (context->unaryExpression())
            return decode_unary_expression(context->unaryExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown cast expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_unary_expression(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        if (!context->PlusPlus().empty() || !context->MinusMinus().empty() || !context->Sizeof().empty() || context->Alignof() || context->AndAnd())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary expressions are not implemented", locate(context));
        else if (context->postfixExpression())
            return decode_postfix_expression(context->postfixExpression());
        else if (context->unaryOperator() && context->castExpression())
            return decode_unary_operation(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown unary expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_unary_operation(CParser::UnaryExpressionContext* context) {
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


    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_unary_addressof(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        std::shared_ptr<ExpressionResult> operand = decode_cast_expression(context->castExpression());
        if (!operand->is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't take the address of an rvalue", locate(context));

        std::shared_ptr<Type> pointer_type = PointerType::make(anonymous_type(), location, operand->type());
        std::shared_ptr<Declaration> result = declare_temporary(pointer_type, location);
        emit(Statement::make_addressof(location, operand->lvalue(), result));
        return make_expression(LValue {result}, location);
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_unary_dereference(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        std::shared_ptr<ExpressionResult> operand = decode_cast_expression(context->castExpression());
        if (operand->type()->category != TypeCategory::POINTER)
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to dereference a non-pointer object", location);

        std::shared_ptr<ExpressionResult> source = std::make_shared<ExpressionResult>(*operand);
        return source->dereference(make_constant_zero(TypeCategory::INTEGER, location), location);
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_unary_plus(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary plus operations are not implemented", locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_unary_minus(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary minus operations are not implemented", locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_unary_bitwise_not(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary bitwise not operations are not implemented", locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_unary_logical_not(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary logical not operations are not implemented", locate(context));
    }


    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_postfix_expression(CParser::PostfixExpressionContext* context) {
        if (!context->primaryExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inline initializer lists are not implemented");

        std::shared_ptr<ExpressionResult> result = decode_primary_expression(context->primaryExpression());
        for (CParser::PostfixOperatorContext* postfix : context->postfixOperator()) {
            const CodeLocation location = locate(postfix);
            if (postfix->LeftBracket() || postfix->RightBracket())
                result = decode_array_index(result, postfix);
            else if (postfix->LeftParen() || postfix->RightParen())
                result = decode_function_call(result, postfix);
            else if (postfix->Dot() || postfix->Arrow())
                result = decode_member_access(result, postfix);
            else if (postfix->PlusPlus())
                result->postfix_increments.push_back(1);
            else if (postfix->MinusMinus())
                result->postfix_increments.push_back(-1);
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown postfix operator `{}`", postfix->getText()));
        }
        return result;
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_array_index(std::shared_ptr<SemanticAnalyzer::ExpressionResult> array, CParser::PostfixOperatorContext* postfix) {
        RValue index = decode_expression(postfix->expression())->rvalue();

        if (array->is_lvalue()) {
            LValue value = array->lvalue();
            value.indices.push_back(index);
            return make_expression(value, value.location);
        } else {
            RValue pointer = array->rvalue();
            LValue dereference(pointer, pointer.location(), {index});
            return make_expression(dereference, dereference.location);
        }
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_primary_expression(CParser::PrimaryExpressionContext* context) {
        const CodeLocation location = locate(context);
        if (context->Identifier())
            return make_expression(LValue {resolve(context->Identifier()->getText(), location), location}, location);
        else if (context->constant())
            return make_expression(decode_constant(context->constant()), location);
        else if (!context->StringLiteral().empty())
            return make_expression(decode_string_literal(context->StringLiteral()), location);
        else if (context->LeftParen() && context->expression() && context->RightParen())
            return decode_expression(context->expression());
        else if (context->genericSelection())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Generics are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Unknown primary expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_function_call(std::shared_ptr<ExpressionResult> function_expr, CParser::PostfixOperatorContext* call) {
        const CodeLocation location = locate(call);
        Operand function = function_expr->operand();
        std::shared_ptr<FunctionType> function_type = std::static_pointer_cast<FunctionType> (function.type());

        if (function_type->category != TypeCategory::FUNCTION)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Can't call object of type `{}` as a function", function_type->text()), locate(call));

        std::vector<Operand> parameters;
        if (call->argumentExpressionList()) {
            std::vector<CParser::AssignmentExpressionContext*> parameter_expressions = call->argumentExpressionList()->assignmentExpression();
            if (parameter_expressions.size() != function_type->parameters.size())
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid number of arguments : found {}, expected {}", parameter_expressions.size(), function_type->parameters.size()), locate(call));

            for (size_t param = 0; param < parameter_expressions.size(); param++) {
                const CodeLocation param_location = locate(parameter_expressions[param]);
                std::shared_ptr<ExpressionResult> expression_result = decode_assignment_expression(parameter_expressions[param]);
                Operand parameter = emit_implicit_conversion(function_type->parameters[param].type, expression_result->operand(), param_location);
                parameters.push_back(parameter);
            }
        } else {
            if (!function_type->parameters.empty())
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid number of arguments : found 0, expected {}", function_type->parameters.size()), locate(call));
        }

        RValue destination = declare_temporary(function_type->return_type, locate(call));
        if (function_type->return_type->category == TypeCategory::VOID)
            emit(Statement::make_call(location, function, parameters));
        else
            emit(Statement::make_call(location, function, parameters, destination));
        return make_expression(destination, location);
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_member_access(std::shared_ptr<ExpressionResult> object, CParser::PostfixOperatorContext* access) {
        const std::string member_name = access->Identifier()->getText();

        if (access->Dot())
            return decode_direct_member_access(object, member_name, locate(access));
        else if (access->Arrow())
            return decode_indirect_member_access(object, member_name, locate(access));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown member access type `{}`", access->getText()), locate(access));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_direct_member_access(std::shared_ptr<ExpressionResult> object, const std::string& member_name, CodeLocation location) {
        if (object->type()->category != TypeCategory::STRUCT && object->type()->category != TypeCategory::UNION)
            throw Diagnostic(DiagnosticLevel::ERROR, "Member access is only valid on struct and union types", location);

        std::shared_ptr<CompoundType> type = std::static_pointer_cast<CompoundType>(object->type());

        auto found_member = std::ranges::find_if(type->members, [&](const Member& member) {
            return member.name == member_name;
        });

        if (found_member == type->members.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Member `{}` is not defined in type `{}`", member_name, type->name), location);

        const size_t member_index = std::distance(type->members.begin(), found_member);
        Constant index(IntegerConstant(member_index), location, literal_integer_type);
        std::vector<RValue> indices = object->indices();
        indices.push_back(index);

        LValue result = {object->base(), location, indices};
        return make_expression(result, location);
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::decode_indirect_member_access(std::shared_ptr<ExpressionResult> object, const std::string& member_name, CodeLocation location) {
        if (object->type()->category != TypeCategory::POINTER)
            throw Diagnostic(DiagnosticLevel::ERROR, "Indirect member access is only valid on pointer to struct or union types", location);

        std::vector<RValue> indices = object->indices();
        indices.push_back(make_constant_zero(TypeCategory::INTEGER, location));

        std::shared_ptr<SemanticAnalyzer::ExpressionResult> dereference = make_expression(LValue {object->base(), location, indices}, location);
        return decode_direct_member_access(dereference, member_name, location);
    }


    StatementTag SemanticAnalyzer::decode_assignment_operator(CParser::AssignmentOperatorContext* context) {
        if      (context->Assign())            return StatementTag::COPY;
        else if (context->StarAssign())        return StatementTag::MUL;
        else if (context->DivAssign())         return StatementTag::DIV;
        else if (context->ModAssign())         return StatementTag::MOD;
        else if (context->PlusAssign())        return StatementTag::ADD;
        else if (context->MinusAssign())       return StatementTag::SUB;
        else if (context->LeftShiftAssign())   return StatementTag::LSHIFT;
        else if (context->RightShiftAssign())  return StatementTag::RSHIFT;
        else if (context->AndAssign())         return StatementTag::BITWISE_AND;
        else if (context->XorAssign())         return StatementTag::BITWISE_XOR;
        else if (context->OrAssign())          return StatementTag::BITWISE_OR;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown assignment operator {}", context->getText()), locate(context));
    }

    StatementTag SemanticAnalyzer::decode_equality_operator(CParser::EqualityOperatorContext* context) {
        if      (context->Equal())     return StatementTag::EQ;
        else if (context->NotEqual())  return StatementTag::NE;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown equality operator {}", context->getText()), locate(context));
    }

    StatementTag SemanticAnalyzer::decode_relational_operator(CParser::RelationalOperatorContext* context) {
        if      (context->Greater())       return StatementTag::GT;
        else if (context->GreaterEqual())  return StatementTag::GE;
        else if (context->Less())          return StatementTag::LT;
        else if (context->LessEqual())     return StatementTag::LE;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown relational operator {}", context->getText()), locate(context));
    }

    StatementTag SemanticAnalyzer::decode_multiplicative_operator(CParser::MultiplicativeOperatorContext* context) {
        if      (context->Star())  return StatementTag::MUL;
        else if (context->Div())   return StatementTag::DIV;
        else if (context->Mod())   return StatementTag::MOD;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown multiplicative operator `{}`", context->getText()), locate(context));
    }

    StatementTag SemanticAnalyzer::decode_additive_operator(CParser::AdditiveOperatorContext* context) {
        if      (context->Plus())  return StatementTag::ADD;
        else if (context->Minus()) return StatementTag::SUB;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown additive operator `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::emit_binary_operation(StatementTag op, std::shared_ptr<ExpressionResult> left, std::shared_ptr<ExpressionResult> right, CodeLocation location) {
        std::shared_ptr<Type> left_type = left->type(), right_type = right->type();

        if (!is_operator_valid(op, left_type, right_type))
            throw Diagnostic(DiagnosticLevel::ERROR, "This operator is not valid on these operands", location);

        if (left_type->is_arithmetic() && right_type->is_arithmetic()) {
            auto [converted_left, converted_right] = emit_arithmetic_conversion(left->operand(), right->operand(), location);
            std::shared_ptr<Type> result_type = operation_result_type(op, converted_left.type(), converted_right.type());
            std::shared_ptr<Declaration> result = declare_temporary(result_type, location);
            emit(Statement::make_binary_operation(location, op, converted_left, converted_right, result));
            return make_expression(RValue {result}, location);
        } else {
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-arithmetic operations are not implemented", location);
        }
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::emit_binary_operation(StatementTag op, std::shared_ptr<ExpressionResult> left, std::shared_ptr<ExpressionResult> right, std::shared_ptr<ExpressionResult> destination, CodeLocation location) {
        std::shared_ptr<SemanticAnalyzer::ExpressionResult> result = emit_binary_operation(op, left, right, location);

        if (!destination->is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Operation destination must be an lvalue");

        emit_copy(destination->operand(), result->operand(), location, false);
        return destination;
    }

    bool SemanticAnalyzer::is_operator_valid(StatementTag op, std::shared_ptr<Type> left, std::shared_ptr<Type> right) {
        std::shared_ptr<Type> left_unqualified = left->dequalify(), right_unqualified = right->dequalify();
        switch (op) {
            case StatementTag::MUL:
            case StatementTag::DIV:
            case StatementTag::MOD:
                return left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic();

            case StatementTag::ADD:
                return (left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic()) ||
                       (left_unqualified->category == TypeCategory::POINTER && right_unqualified->is_arithmetic()) ||
                       (left_unqualified->is_arithmetic() && right_unqualified->category == TypeCategory::POINTER);

            case StatementTag::SUB:
                return (left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic()) ||
                       (left_unqualified->category == TypeCategory::POINTER && right_unqualified->is_arithmetic());

            case StatementTag::LSHIFT:
            case StatementTag::RSHIFT:
            case StatementTag::BITWISE_AND:
            case StatementTag::BITWISE_XOR:
            case StatementTag::BITWISE_OR:
                return left_unqualified->is_integral() && right_unqualified->is_integral();

            case StatementTag::LT:
            case StatementTag::LE:
            case StatementTag::GE:
            case StatementTag::GT:
            case StatementTag::EQ:
            case StatementTag::NE:
                return left_unqualified->is_comparable() && right_unqualified->is_comparable();

            case StatementTag::LOGICAL_AND:
            case StatementTag::LOGICAL_OR:
                return left_unqualified->has_truth_value() && right_unqualified->has_truth_value();

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown binary operator");
        }
    }

    std::shared_ptr<Type> SemanticAnalyzer::operation_result_type(StatementTag op, std::shared_ptr<Type> left, std::shared_ptr<Type> right) {
        std::shared_ptr<Type> left_unqualified = left->dequalify(), right_unqualified = right->dequalify();
        switch (op) {
            case StatementTag::MUL:
            case StatementTag::DIV:
            case StatementTag::MOD:
            case StatementTag::ADD:
            case StatementTag::SUB:
            case StatementTag::LSHIFT:
            case StatementTag::RSHIFT:
            case StatementTag::BITWISE_AND:
            case StatementTag::BITWISE_XOR:
            case StatementTag::BITWISE_OR:
                return left_unqualified;

            case StatementTag::LT:
            case StatementTag::LE:
            case StatementTag::GE:
            case StatementTag::GT:
            case StatementTag::EQ:
            case StatementTag::NE:
            case StatementTag::LOGICAL_AND:
            case StatementTag::LOGICAL_OR:
                return boolean_type;

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown binary operator");
        }
    }
}
