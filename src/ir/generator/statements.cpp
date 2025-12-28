#include "diagnostic.h"
#include "ir/generator.h"

namespace toycc::ir {
    std::shared_ptr<Scope> Generator::decode_block(CParser::CompoundStatementContext* context) {
        std::shared_ptr<Scope> scope = decode_compound_statement(context, ScopeType::BLOCK);
        current_scope()->add_statement(std::make_shared<stmt::Block>(locate(context), scope));
        return scope;
    }

    std::shared_ptr<Scope> Generator::decode_compound_statement(CParser::CompoundStatementContext* context, ScopeType type) {
        std::shared_ptr<Scope> scope = std::make_shared<Scope>(type, current_scope()->function);
        decode_compound_statement(context, scope);
        return scope;
    }

    void Generator::decode_compound_statement(CParser::CompoundStatementContext* context, std::shared_ptr<Scope> scope) {
        ScopeFrame frame = in_scope(scope);

        if (context->blockItemList())
            decode_block_item_list(context->blockItemList());
    }

    void Generator::decode_block_item_list(CParser::BlockItemListContext* context) {
        for (CParser::BlockItemContext* item : context->blockItem()) {
            if (item->declaration())
                decode_declaration(item->declaration());
            else if (item->statement())
                decode_statement(item->statement());
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown block item type `{}`", item->getText()), locate(item));
        }
    }

    void Generator::decode_statement(CParser::StatementContext* context) {
        if (context->labeledStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Labeled statements are not implemented", locate(context));
        else if (context->compoundStatement())
            decode_compound_statement(context->compoundStatement(), ScopeType::BLOCK);
        else if (context->expressionStatement())
            decode_expression_statement(context->expressionStatement());
        else if (context->selectionStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Selection statements are not implemented", locate(context));
        else if (context->iterationStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Iteration statements are not implemented", locate(context));
        else if (context->jumpStatement())
            decode_jump_statement(context->jumpStatement());
        else if (context->asmStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inline assembly is not supported", locate(context));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown statement `{}`", context->getText()), locate(context));
    }

    void Generator::decode_expression_statement(CParser::ExpressionStatementContext* context) {
        if (context->expression())
            decode_expression(context->expression());
        // Otherwise it's a trailing semicolon -> skip
    }

    void Generator::decode_jump_statement(CParser::JumpStatementContext* context) {
        if (context->Goto())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "`goto` statements are not implemented", locate(context));
        else if (context->Continue())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "`continue` statements are not implemented", locate(context));
        else if (context->Break())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "`break` statements are not implemented", locate(context));
        else if (context->Return())
            decode_return_statement(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown jump statement `{}`", context->getText()), locate(context));
    }

    void Generator::decode_return_statement(CParser::JumpStatementContext* context) {
        std::shared_ptr<Declaration> current_function = current_scope()->function;
        if (current_function.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, "Return statement outside of a function definition", locate(context));

        if (context->expression()) {
            TypeSpecification return_type_spec = current_function->spec.return_type();
            if (return_type_spec.is_void())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't return a value in a function returning void", locate(context));

            std::shared_ptr<Declaration> expression_result = decode_expression(context->expression());
            std::shared_ptr<Declaration> return_value = emit_implicit_conversion(return_type_spec, expression_result, locate(context));
            current_scope()->add_statement(std::make_shared<stmt::Return>(locate(context), return_value));
        } else {
            if (!current_function->spec.return_type().is_void())
                throw Diagnostic(DiagnosticLevel::ERROR, "Return without a value within a function with a non-void return type", locate(context));
            current_scope()->add_statement(std::make_shared<stmt::Return>(locate(context)));
        }
    }
}
