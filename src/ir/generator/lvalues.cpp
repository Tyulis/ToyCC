#include "diagnostic.h"
#include "ir/declaration.h"
#include "ir/generator.h"
#include "ir/statement.h"

namespace toycc::ir {
    std::shared_ptr<Declaration> Generator::load_lvalue(LValue value, CodeLocation location) {
        // NOTE : Don't apply postfix increments/decrements while loading, that's done after assignment
        if (value.indices.empty())
            return value.base_declaration;

        // Otherwise emit a dereference
        std::shared_ptr<Declaration> destination = declare_temporary(value.type(), location);
        current_scope()->add_statement(std::make_shared<stmt::DerefLoad>(location, destination, value.base_declaration, value.indices));
        return destination;
    }

    // Store the `source` into `destination`, and return the value of source, possibly with implicit conversions
    std::shared_ptr<Declaration> Generator::store_to_lvalue(LValue destination, std::shared_ptr<Declaration> source, CodeLocation location) {
        std::shared_ptr<Declaration> stored_value = emit_implicit_conversion(destination.type(), source, location);

        if (destination.indices.empty())
            current_scope()->add_statement(std::make_shared<stmt::Copy>(location, stmt::ConversionOperation::COPY, destination.base_declaration, stored_value));
        else
            current_scope()->add_statement(std::make_shared<stmt::DerefStore>(location, destination.base_declaration, destination.indices, stored_value));

        return stored_value;
    }

    void Generator::apply_lvalue_postfix_operations(LValue value, CodeLocation location) {
        if (!value.postfix_increments.empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Postfix increments and decrements are not implemented", location);
    }

    LValue Generator::decode_lvalue_unary_expression(CParser::UnaryExpressionContext* context) {
        if (!context->PlusPlus().empty() || !context->MinusMinus().empty() || !context->Sizeof().empty() || context->Alignof() || context->AndAnd())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Prefix operators in lvalue contexts are not implemented", locate(context));
        if (context->unaryOperator() && context->castExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary operators in lvalue contexts are not implemented", locate(context));

        if (context->unaryExpression())
            return decode_lvalue_unary_expression(context->unaryExpression());
        else if (context->postfixExpression())
            return decode_lvalue_postfix_expression(context->postfixExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown lvalue unary expression `{}`", context->getText()), locate(context));
    }

    LValue Generator::decode_lvalue_postfix_expression(CParser::PostfixExpressionContext* context) {
        if (!context->primaryExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inline initializer lists in lvalue contexts are not implemented", locate(context));

        LValue value = decode_lvalue_primary_expression(context->primaryExpression());
        for (CParser::PostfixOperatorContext* postfix : context->postfixOperator()) {
            if (postfix->LeftBracket() && postfix->expression() && postfix->RightBracket())
                value.indices.push_back(decode_expression(postfix->expression()));
            else if (postfix->LeftParen() && postfix->RightParen())
                value.base_declaration = decode_function_call(value.base_declaration, postfix);
            else if (postfix->Dot() || postfix->Arrow())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Structure member access in lvalue contexts is not implemented", locate(postfix));
            else if (postfix->PlusPlus())
                value.postfix_increments.push_back(1);
            else if (postfix->MinusMinus())
                value.postfix_increments.push_back(-1);
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown postfix operator `{}`", postfix->getText()), locate(postfix));
        }

        return value;
    }

    LValue Generator::decode_lvalue_primary_expression(CParser::PrimaryExpressionContext* context) {
        LValue value;
        value.base_declaration = decode_primary_expression(context);
        return value;
    }

}
