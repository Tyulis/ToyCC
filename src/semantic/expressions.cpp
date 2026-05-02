#include <memory>
#include <algorithm>

#include "diagnostic.h"
#include "gen/parser/CParser.h"
#include "ir/type.h"
#include "ir/type_expressions.h"
#include "ir/statement.h"
#include "arch/datamodel.h"
#include "semantic/analyzer.h"
#include "semantic/values.h"

namespace toycc::semantic {
    // ------------ Expressions
    ExpressionResult SemanticAnalyzer::decode_initializer(CParser::InitializerContext* context) {
        if (context->LeftBrace() || context->RightBrace())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Initializer lists are not implemented", locate(context));
        else if (context->assignmentExpression())
            return decode_assignment_expression(context->assignmentExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown initializer `{}`", context->getText()), locate(context));
    }

    ExpressionResult SemanticAnalyzer::decode_expression(CParser::ExpressionContext* context) {
        return decode_expression_list(context->assignmentExpression());
    }

    ExpressionResult SemanticAnalyzer::decode_for_expression(CParser::ForExpressionContext* context) {
        return decode_expression_list(context->assignmentExpression());
    }

    ExpressionResult SemanticAnalyzer::decode_expression_list(std::vector<CParser::AssignmentExpressionContext*> context) {
        for (size_t item = 0; item < context.size(); item++) {
            ExpressionResult result = decode_assignment_expression(context[item]);
            if (item == context.size() - 1)
                return result;  // In a comma-separated list of expressions, return the last one
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Empty expression list");
    }


    ExpressionResult SemanticAnalyzer::decode_assignment_expression(CParser::AssignmentExpressionContext* context) {
        const CodeLocation location = locate(context);

        if (context->conditionalExpression())
            return decode_conditional_expression(context->conditionalExpression());
        else if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences are not supported as assignment expressions", location);

        StatementTag op = decode_assignment_operator(context->assignmentOperator());
        ExpressionResult destination = decode_prefix_expression(context->prefixExpression());
        if (!destination.is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Assignment destination must be an lvalue");
        ExpressionResult source = decode_assignment_expression(context->assignmentExpression());

        if (op == StatementTag::COPY)
            emit_copy(destination.operand(), source.operand(), location, false);
        else
            emit_binary_operation(op, destination, source, destination, location);

        return destination;
    }

    ExpressionResult SemanticAnalyzer::decode_conditional_expression(CParser::ConditionalExpressionContext* context) {
        ExpressionResult predicate = decode_logical_or_expression(context->logicalOrExpression());

        if (!context->Question())
            return predicate;

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Conditional expressions are not implemented", locate(context));
    }

    ExpressionResult SemanticAnalyzer::decode_logical_or_expression(CParser::LogicalOrExpressionContext* context) {
        const std::vector<CParser::LogicalAndExpressionContext*> operands = context->logicalAndExpression();
        const std::vector<antlr4::tree::TerminalNode*> operators = context->OrOr();

        ExpressionResult operand = decode_logical_and_expression(context->logicalAndExpression()[0]);

        if (operators.size() == 0)
            return operand;

        std::shared_ptr<Declaration> result = declare_temporary(arch::DATAMODEL->boolean_type, locate(context));
        emit_copy(result, operand.operand(), locate(context), true);

        const std::string evaluation_finished = anonymous_label();
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);

            // Short-circuit evaluation : first `true` result skips the rest
            emit_conditional_jump(operand, evaluation_finished, true, location);

            // If it gets to this point, the previous `result` is false
            // So no need for an explicit operation, just replace the result with the new value
            operand = decode_logical_and_expression(operands[operation_index + 1]);
            emit_copy(result, operand.operand(), location, false);
        }
        emit_label(LabelType::INTERNAL, evaluation_finished, locate(context));

        return ExpressionResult {RValue {result}, locate(context)};
    }

    ExpressionResult SemanticAnalyzer::decode_logical_and_expression(CParser::LogicalAndExpressionContext* context) {
        const std::vector<CParser::InclusiveOrExpressionContext*> operands = context->inclusiveOrExpression();
        const std::vector<antlr4::tree::TerminalNode*> operators = context->AndAnd();

        ExpressionResult operand = decode_inclusive_or_expression(context->inclusiveOrExpression()[0]);

        if (operators.size() == 0)
            return operand;

        std::shared_ptr<Declaration> result = declare_temporary(arch::DATAMODEL->boolean_type, locate(context));
        emit_copy(result, operand.operand(), locate(context), true);

        const std::string evaluation_finished = anonymous_label();
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);

            // Short-circuit evaluation : first `false` result skips the rest
            emit_conditional_jump(operand, evaluation_finished, false, location);

            // If it gets to this point, the previous `result` is true
            // So no need for an explicit operation, just replace the result with the new value
            operand = decode_inclusive_or_expression(operands[operation_index + 1]);
            emit_copy(result, operand.operand(), location, false);
        }
        emit_label(LabelType::INTERNAL, evaluation_finished, locate(context));

        return ExpressionResult {RValue {result}, locate(context)};
    }

    ExpressionResult SemanticAnalyzer::decode_inclusive_or_expression(CParser::InclusiveOrExpressionContext* context) {
        const std::vector<CParser::ExclusiveOrExpressionContext*> operands = context->exclusiveOrExpression();
        const std::vector<antlr4::tree::TerminalNode*> operators = context->Or();

        ExpressionResult left = decode_exclusive_or_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            ExpressionResult right = decode_exclusive_or_expression(operands[operation_index + 1]);
            left = emit_binary_operation(StatementTag::BITWISE_OR, left, right, location);
        }
        return left;
    }

    ExpressionResult SemanticAnalyzer::decode_exclusive_or_expression(CParser::ExclusiveOrExpressionContext* context) {
        const std::vector<CParser::AndExpressionContext*> operands = context->andExpression();
        const std::vector<antlr4::tree::TerminalNode*> operators = context->Caret();

        ExpressionResult left = decode_and_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            ExpressionResult right = decode_and_expression(operands[operation_index + 1]);
            left = emit_binary_operation(StatementTag::BITWISE_XOR, left, right, location);
        }
        return left;
    }

    ExpressionResult SemanticAnalyzer::decode_and_expression(CParser::AndExpressionContext* context) {
        const std::vector<CParser::EqualityExpressionContext*> operands = context->equalityExpression();
        const std::vector<antlr4::tree::TerminalNode*> operators = context->And();

        ExpressionResult left = decode_equality_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            ExpressionResult right = decode_equality_expression(operands[operation_index + 1]);
            left = emit_binary_operation(StatementTag::BITWISE_AND, left, right, location);
        }
        return left;
    }

    ExpressionResult SemanticAnalyzer::decode_equality_expression(CParser::EqualityExpressionContext* context) {
        const std::vector<CParser::RelationalExpressionContext*> operands = context->relationalExpression();
        const std::vector<CParser::EqualityOperatorContext*> operators = context->equalityOperator();

        ExpressionResult left = decode_relational_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            ExpressionResult right = decode_relational_expression(operands[operation_index + 1]);
            left = emit_binary_operation(decode_equality_operator(operators[operation_index]), left, right, location);
        }
        return left;
    }

    ExpressionResult SemanticAnalyzer::decode_relational_expression(CParser::RelationalExpressionContext* context) {
        const std::vector<CParser::ShiftExpressionContext*> operands = context->shiftExpression();
        const std::vector<CParser::RelationalOperatorContext*> operators = context->relationalOperator();

        ExpressionResult left = decode_shift_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            ExpressionResult right = decode_shift_expression(operands[operation_index + 1]);
            left = emit_binary_operation(decode_relational_operator(operators[operation_index]), left, right, location);
        }
        return left;
    }

    ExpressionResult SemanticAnalyzer::decode_shift_expression(CParser::ShiftExpressionContext* context) {
        ExpressionResult result = decode_additive_expression(context->additiveExpression()[0]);
        if (context->additiveExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Shift expressions are not implemented", locate(context));
        return result;
    }

    ExpressionResult SemanticAnalyzer::decode_additive_expression(CParser::AdditiveExpressionContext* context) {
        const std::vector<CParser::MultiplicativeExpressionContext*> operands = context->multiplicativeExpression();
        const std::vector<CParser::AdditiveOperatorContext*> operators = context->additiveOperator();

        ExpressionResult left = decode_multiplicative_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            ExpressionResult right = decode_multiplicative_expression(operands[operation_index + 1]);
            left = emit_binary_operation(decode_additive_operator(operators[operation_index]), left, right, location);
        }
        return left;
    }

    ExpressionResult SemanticAnalyzer::decode_multiplicative_expression(CParser::MultiplicativeExpressionContext* context) {
        const std::vector<CParser::CastExpressionContext*> operands = context->castExpression();
        const std::vector<CParser::MultiplicativeOperatorContext*> operators = context->multiplicativeOperator();

        ExpressionResult left = decode_cast_expression(operands[0]);
        for (size_t operation_index = 0; operation_index < operators.size(); operation_index++) {
            const CodeLocation location = locate(operators[operation_index]);
            ExpressionResult right = decode_cast_expression(operands[operation_index + 1]);
            if (left.type()->is_arithmetic() && right.type()->is_arithmetic())
                left = emit_binary_operation(decode_multiplicative_operator(operators[operation_index]), left, right, location);
            else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-arithmetic multiplicative expressions are not implemented");
        }
        return left;
    }

    ExpressionResult SemanticAnalyzer::decode_cast_expression(CParser::CastExpressionContext* context) {
        if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences as cast expressions are not implemented", locate(context));
        else if (context->typeName())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Cast expressions are not implemented");
        else if (context->prefixExpression())
            return decode_prefix_expression(context->prefixExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown cast expression `{}`", context->getText()), locate(context));
    }

    ExpressionResult SemanticAnalyzer::decode_prefix_expression(CParser::PrefixExpressionContext* context) {
        ExpressionResult result = decode_unary_expression(context->unaryExpression());
        for (CParser::PrefixOperatorContext* op : context->prefixOperator()) {
            if (op->Sizeof())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "The sizeof operator is not implemented", locate(context));
            else if (op->PlusPlus())
                result = emit_increment(result, StatementTag::ADD, locate(context));
            else if (op->MinusMinus())
                result = emit_increment(result, StatementTag::SUB, locate(context));
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown prefix operator `{}`", op->getText()), locate(context));
        }

        return result;
    }

    ExpressionResult SemanticAnalyzer::decode_unary_expression(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        if (context->Sizeof() || context->Alignof() || context->AndAnd())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Extension unary expressions are not implemented", locate(context));
        else if (context->postfixExpression())
            return decode_postfix_expression(context->postfixExpression());
        else if (context->unaryOperator() && context->castExpression())
            return decode_unary_operation(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown unary expression `{}`", context->getText()), locate(context));
    }

    ExpressionResult SemanticAnalyzer::decode_unary_operation(CParser::UnaryExpressionContext* context) {
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


    ExpressionResult SemanticAnalyzer::decode_unary_addressof(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        ExpressionResult operand = decode_cast_expression(context->castExpression());
        if (!operand.is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't take the address of an rvalue", locate(context));

        std::shared_ptr<Type> pointer_type = PointerType::make(location, operand.type());
        std::shared_ptr<Declaration> result = declare_temporary(pointer_type, location);
        emit(Statement::make_addressof(location, operand.lvalue(), result));
        return ExpressionResult {LValue {result}, location};
    }

    ExpressionResult SemanticAnalyzer::decode_unary_dereference(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);

        ExpressionResult operand = decode_cast_expression(context->castExpression());
        if (operand.type()->category != TypeCategory::POINTER)
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to dereference a non-pointer object", location);

        return operand.dereference(make_constant_zero(TypeCategory::INTEGER, location), location);
    }

    ExpressionResult SemanticAnalyzer::decode_unary_plus(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary plus operations are not implemented", locate(context));
    }

    ExpressionResult SemanticAnalyzer::decode_unary_minus(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);
        ExpressionResult operand = decode_cast_expression(context->castExpression());

        std::shared_ptr<Declaration> result = declare_temporary(operand.type(), location);
        emit(Statement::make_unary_operation(location, StatementTag::MINUS, operand.operand(), result));
        return ExpressionResult {RValue {result}, location};
    }

    ExpressionResult SemanticAnalyzer::decode_unary_bitwise_not(CParser::UnaryExpressionContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary bitwise not operations are not implemented", locate(context));
    }

    ExpressionResult SemanticAnalyzer::decode_unary_logical_not(CParser::UnaryExpressionContext* context) {
        const CodeLocation location = locate(context);
        ExpressionResult operand = decode_cast_expression(context->castExpression());

        // The logical NOT is equivalent to `x == 0`
        std::shared_ptr<Declaration> result = declare_temporary(arch::DATAMODEL->boolean_type, location);
        emit(Statement::make_binary_operation(location, StatementTag::EQ, operand.operand(), make_constant_zero(operand.type(), location), result));
        return ExpressionResult {RValue {result}, location};
    }


    ExpressionResult SemanticAnalyzer::decode_postfix_expression(CParser::PostfixExpressionContext* context) {
        if (!context->primaryExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inline initializer lists are not implemented");

        ExpressionResult result = decode_primary_expression(context->primaryExpression());
        for (CParser::PostfixOperatorContext* postfix : context->postfixOperator()) {
            const CodeLocation location = locate(postfix);
            if (postfix->LeftBracket() || postfix->RightBracket())
                result = decode_array_index(result, postfix);
            else if (postfix->LeftParen() || postfix->RightParen())
                result = decode_function_call(result, postfix);
            else if (postfix->Dot() || postfix->Arrow())
                result = decode_member_access(result, postfix);
            else if (postfix->PlusPlus() || postfix->MinusMinus())
                result = decode_postfix_increment(result, postfix);
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown postfix operator `{}`", postfix->getText()));
        }
        return result;
    }

    ExpressionResult SemanticAnalyzer::decode_array_index(const ExpressionResult& array, CParser::PostfixOperatorContext* postfix) {
        RValue index = decode_expression(postfix->expression()).rvalue();

        if (array.is_lvalue()) {
            LValue value = array.lvalue();
            value.indices.push_back(index);
            return ExpressionResult {value, value.location};
        } else {
            RValue pointer = array.rvalue();
            LValue dereference(pointer, pointer.location(), {index});
            return ExpressionResult {dereference, dereference.location};
        }
    }

    ExpressionResult SemanticAnalyzer::decode_primary_expression(CParser::PrimaryExpressionContext* context) {
        const CodeLocation location = locate(context);
        if (context->Identifier())
            return ExpressionResult {LValue {resolve(context->Identifier()->getText(), location), location}, location};
        else if (context->constant())
            return ExpressionResult {decode_constant(context->constant()), location};
        else if (!context->StringLiteral().empty())
            return ExpressionResult {decode_string_literal(context->StringLiteral()), location};
        else if (context->LeftParen() && context->expression() && context->RightParen())
            return decode_expression(context->expression());
        else if (context->genericSelection())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Generics are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Unknown primary expression `{}`", context->getText()), locate(context));
    }

    ExpressionResult SemanticAnalyzer::decode_function_call(const ExpressionResult& function_expr, CParser::PostfixOperatorContext* call) {
        const CodeLocation location = locate(call);
        Operand function = function_expr.operand();
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
                ExpressionResult expression_result = decode_assignment_expression(parameter_expressions[param]);
                Operand parameter = emit_implicit_conversion(function_type->parameters[param].type, expression_result.operand(), param_location);
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
        return ExpressionResult {destination, location};
    }

    ExpressionResult SemanticAnalyzer::decode_member_access(const ExpressionResult& object, CParser::PostfixOperatorContext* access) {
        const std::string member_name = access->Identifier()->getText();

        if (access->Dot())
            return decode_direct_member_access(object, member_name, locate(access));
        else if (access->Arrow())
            return decode_indirect_member_access(object, member_name, locate(access));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown member access type `{}`", access->getText()), locate(access));
    }

    ExpressionResult SemanticAnalyzer::decode_direct_member_access(const ExpressionResult& object, const std::string& member_name, CodeLocation location) {
        if (object.type()->category != TypeCategory::STRUCT && object.type()->category != TypeCategory::UNION)
            throw Diagnostic(DiagnosticLevel::ERROR, "Member access is only valid on struct and union types", location);

        std::shared_ptr<CompoundType> type = std::static_pointer_cast<CompoundType>(object.type());

        auto found_member = std::ranges::find_if(type->members, [&](const Member& member) {
            return member.name == member_name;
        });

        if (found_member == type->members.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Member `{}` is not defined in type `{}`", member_name, type->name), location);

        const size_t member_index = std::distance(type->members.begin(), found_member);
        Constant index(IntegerConstant(member_index), location, arch::DATAMODEL->literal_integer_type);
        std::vector<RValue> indices = object.indices();
        indices.push_back(index);

        LValue result = {object.base(), location, indices};
        return ExpressionResult {result, location};
    }

    ExpressionResult SemanticAnalyzer::decode_indirect_member_access(const ExpressionResult& object, const std::string& member_name, CodeLocation location) {
        if (object.type()->category != TypeCategory::POINTER)
            throw Diagnostic(DiagnosticLevel::ERROR, "Indirect member access is only valid on pointer to struct or union types", location);

        std::vector<RValue> indices = object.indices();
        indices.push_back(make_constant_zero(TypeCategory::INTEGER, location));

        ExpressionResult dereference = ExpressionResult {LValue {object.base(), location, indices}, location};
        return decode_direct_member_access(dereference, member_name, location);
    }

    ExpressionResult SemanticAnalyzer::decode_postfix_increment(const ExpressionResult& target, CParser::PostfixOperatorContext* postfix) {
        const CodeLocation location = locate(postfix);
        if (!target.is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("The `{}` operator is only valid on lvalues", postfix->getText()), location);

        // Save the old value to a temporary variable, that's what is returned by this expression
        std::shared_ptr<Declaration> old_value = declare_temporary(target.type(), location);
        emit_copy(old_value, target.operand(), location, true);

        // Then increment the variable
        StatementTag op;
        if      (postfix->PlusPlus())    op = StatementTag::ADD;
        else if (postfix->MinusMinus())  op = StatementTag::SUB;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid postfix increment {}", postfix->getText()), location);
        emit_increment(target, op, location);

        return ExpressionResult {LValue {old_value}, location};
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

    ExpressionResult SemanticAnalyzer::emit_arithmetic_binary_operation(StatementTag op, const ExpressionResult& left, const ExpressionResult& right, CodeLocation location) {
        auto [converted_left, converted_right] = emit_arithmetic_conversion(left.operand(), right.operand(), location);
        std::shared_ptr<Type> result_type = operation_result_type(op, converted_left.type(), converted_right.type());
        std::shared_ptr<Declaration> result = declare_temporary(result_type, location);
        emit(Statement::make_binary_operation(location, op, converted_left, converted_right, result));
        return ExpressionResult {RValue {result}, location};
    }

    ExpressionResult SemanticAnalyzer::emit_pointer_arithmetic_binary_operation(StatementTag op, const ExpressionResult& left, const ExpressionResult& right, CodeLocation location) {
        std::shared_ptr<Type> left_type = left.type(), right_type = right.type();

        if (left_type->dequalify()->category == TypeCategory::POINTER) {
            RValue item_size = Constant {IntegerConstant(left_type->dereference({}, location)->size(location)), location, arch::DATAMODEL->size_type};
            ExpressionResult increment = emit_binary_operation(StatementTag::MUL, right, ExpressionResult {item_size, location}, location);
            std::shared_ptr<Declaration> result = declare_temporary(left_type, location);
            emit(Statement::make_binary_operation(location, op, left.operand(), increment.operand(), result));
            return ExpressionResult {RValue {result}, location};
        } else if (right_type->dequalify()->category == TypeCategory::POINTER) {
            RValue item_size = Constant {IntegerConstant(right_type->dereference({}, location)->size(location)), location, arch::DATAMODEL->size_type};
            ExpressionResult increment = emit_binary_operation(StatementTag::MUL, left, ExpressionResult {item_size, location}, location);
            std::shared_ptr<Declaration> result = declare_temporary(right_type, location);
            emit(Statement::make_binary_operation(location, op, increment.operand(), right.operand(), result));
            return ExpressionResult {RValue {result}, location};
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted pointer arithmetic without a pointer operand", location);
    }

    ExpressionResult SemanticAnalyzer::emit_pointer_pointer_binary_operation(StatementTag op, const ExpressionResult& left, const ExpressionResult& right, CodeLocation location) {
        if (op != StatementTag::SUB)
            throw Diagnostic(DiagnosticLevel::ERROR, "Invalid operator between two pointers", location);

        std::shared_ptr<Type> left_referenced_type  = left.type ()->dereference({}, location)->dequalify();
        std::shared_ptr<Type> right_referenced_type = right.type()->dereference({}, location)->dequalify();
        if (*left_referenced_type != *right_referenced_type)
            throw Diagnostic(DiagnosticLevel::ERROR, "Operator `-` between incompatible pointer types", location);

        // Raw offset between the two addresses
        std::shared_ptr<Declaration> address_difference = declare_temporary(arch::DATAMODEL->ptrdiff_type, location);
        emit(Statement::make_binary_operation(location, op, left.operand(), right.operand(), address_difference));

        // Like in pointer + arithmetic, pointer - pointer gives the number of *elements*, not bytes -> divide by the element size
        RValue item_size = Constant {IntegerConstant(left_referenced_type->size(location)), location, arch::DATAMODEL->ptrdiff_type};
        std::shared_ptr<Declaration> result = declare_temporary(arch::DATAMODEL->ptrdiff_type, location);
        emit(Statement::make_binary_operation(location, StatementTag::DIV, address_difference, item_size, result));

        return ExpressionResult {RValue {result}, location};
    }

    ExpressionResult SemanticAnalyzer::emit_binary_operation(StatementTag op, const ExpressionResult& left, const ExpressionResult& right, CodeLocation location) {
        std::shared_ptr<Type> left_type  = left.type();
        std::shared_ptr<Type> right_type = right.type();
        std::shared_ptr<Type> left_dequalified  = left_type->dequalify();
        std::shared_ptr<Type> right_dequalified = right_type->dequalify();

        if (!is_operator_valid(op, left_type, right_type))
            throw Diagnostic(DiagnosticLevel::ERROR, "This operator is not valid on these operands", location);

        if (left_type->is_arithmetic() && right_type->is_arithmetic())
            return emit_arithmetic_binary_operation(op, left, right, location);
        else if (left_dequalified->category == TypeCategory::POINTER && right_type->is_arithmetic())
            return emit_pointer_arithmetic_binary_operation(op, left, right, location);
        else if (left_type->is_arithmetic() && right_dequalified->category == TypeCategory::POINTER)
            return emit_pointer_arithmetic_binary_operation(op, left, right, location);
        else if (left_dequalified->category == TypeCategory::POINTER && right_dequalified->category == TypeCategory::POINTER)
            return emit_pointer_pointer_binary_operation(op, left, right, location);
        else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown operation configuration", location);
    }

    ExpressionResult SemanticAnalyzer::emit_binary_operation(StatementTag op, const ExpressionResult& left, const ExpressionResult& right, const ExpressionResult& destination, CodeLocation location) {
        ExpressionResult result = emit_binary_operation(op, left, right, location);

        if (!destination.is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Operation destination must be an lvalue");

        emit_copy(destination.operand(), result.operand(), location, false);
        return destination;
    }

    ExpressionResult SemanticAnalyzer::emit_increment(const ExpressionResult& operand, StatementTag op, CodeLocation location) {
        if (!operand.is_lvalue())
            throw Diagnostic(DiagnosticLevel::ERROR, "Prefix increment operators are only valid on lvalues", location);

        RValue one = (operand.type()->is_arithmetic() ? make_constant_one(operand.type(), location)
                                                       : make_constant_one(TypeCategory::INTEGER, location));  // For pointer arithmetic
        return emit_binary_operation(op, operand, ExpressionResult {one, location}, operand, location);
    }

    bool SemanticAnalyzer::is_operator_valid(StatementTag op, std::shared_ptr<Type> left, std::shared_ptr<Type> right) {
        std::shared_ptr<Type> left_unqualified = left->dequalify(), right_unqualified = right->dequalify();
        switch (op) {
            case StatementTag::MUL:
            case StatementTag::DIV:
            case StatementTag::MOD:
                return left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic();

            case StatementTag::ADD:
                return (left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic()) ||                                    // arithmetic + arithmetic -> arithmetic
                       (left_unqualified->category == TypeCategory::POINTER && right_unqualified->is_arithmetic()) ||                  // pointer    + arithmetic -> pointer
                       (left_unqualified->is_arithmetic() && right_unqualified->category == TypeCategory::POINTER);                    // arithmetic + pointer    -> pointer

            case StatementTag::SUB:
                return (left_unqualified->is_arithmetic() && right_unqualified->is_arithmetic()) ||                                    // arithmetic - arithmetic -> arithmetic
                       (left_unqualified->category == TypeCategory::POINTER && right_unqualified->is_arithmetic()) ||                  // pointer    - arithmetic -> pointer
                       (left_unqualified->category == TypeCategory::POINTER && right_unqualified->category == TypeCategory::POINTER);  // pointer    - pointer    -> arithmetic

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
                return arch::DATAMODEL->boolean_type;

            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown binary operator");
        }
    }
}
