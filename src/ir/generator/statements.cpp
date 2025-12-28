#include "diagnostic.h"
#include "ir/generator.h"
#include "ir/statement.h"

namespace toycc::ir {
    std::shared_ptr<Scope> Generator::decode_compound_statement(CParser::CompoundStatementContext* context, ScopeType type) {
        std::shared_ptr<Scope> scope = std::make_shared<Scope>(type, current_scope()->function);
        decode_compound_statement(context, scope);
        current_scope()->add_statement(std::make_shared<stmt::Block>(locate(context), scope));
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

    void Generator::decode_statement(CParser::StatementContext* context, std::optional<ScopeType> scope_type) {
        if (context->labeledStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Labeled statements are not implemented", locate(context));
        else if (context->compoundStatement())
            decode_compound_statement(context->compoundStatement(), scope_type.value_or(ScopeType::BLOCK));
        else if (context->expressionStatement())
            decode_expression_statement(context->expressionStatement());
        else if (context->selectionStatement())
            decode_selection_statement(context->selectionStatement());
        else if (context->iterationStatement())
            decode_iteration_statement(context->iterationStatement());
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

    void Generator::decode_selection_statement(CParser::SelectionStatementContext* context) {
        if (context->If())
            decode_if_statement(context);
        else if (context->Switch())
            decode_switch_statement(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown selection statement `{}`", context->getText()), locate(context));
    }

    void Generator::decode_if_statement(CParser::SelectionStatementContext* context) {
        const CodeLocation predicate_location = locate(context->expression());
        std::shared_ptr<ExpressionResult> predicate_expression = decode_expression(context->expression());
        std::shared_ptr<Declaration> predicate = convert_to_boolean(predicate_expression->load(predicate_location), predicate_location);

        const std::string label_after_if = anonymous_label();
        current_scope()->add_statement(std::make_shared<stmt::Jump>(locate(context), label_after_if, predicate, false));

        decode_statement(context->statement(0), ScopeType::CONDITIONAL);

        if (context->Else()) {
            const std::string label_after_else = anonymous_label();
            current_scope()->add_statement(std::make_shared<stmt::Jump>(locate(context), label_after_else));  // If we entered the `if`, skip the `else` part
            current_scope()->add_label(label_after_if);  // Must be after the `else` skip otherwise the `else` path jumps to the second jump statement

            decode_statement(context->statement(1), ScopeType::CONDITIONAL);
            current_scope()->add_label(label_after_else);
        } else {
            current_scope()->add_label(label_after_if);
        }
    }

    void Generator::decode_switch_statement(CParser::SelectionStatementContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Switch statements are not implemented", locate(context));
    }

    void Generator::decode_iteration_statement(CParser::IterationStatementContext* context) {
        if (context->Do() && context->While())
            decode_do_while_statement(context);
        else if (!context->Do() && context->While())
            decode_while_statement(context);
        else if (context->For())
            decode_for_statement(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown iteration statement `{}`", context->getText()), locate(context));
    }

    void Generator::decode_while_statement(CParser::IterationStatementContext* context) {
        const std::string label_entry= anonymous_label();
        const std::string label_exit = anonymous_label();

        // First evaluation of the predicate right before the loop : when already false, don't enter
        const CodeLocation predicate_location = locate(context->expression());
        std::shared_ptr<ExpressionResult> entry_predicate_expression = decode_expression(context->expression());
        std::shared_ptr<Declaration> entry_predicate = convert_to_boolean(entry_predicate_expression->load(predicate_location), predicate_location);
        current_scope()->add_statement(std::make_shared<stmt::Jump>(locate(context), label_exit, entry_predicate, false));

        // Then the loop body
        current_scope()->add_label(label_entry);
        decode_statement(context->statement(), ScopeType::LOOP);

        // Second evaluation of the predicate into the loop : when false, fall through
        std::shared_ptr<ExpressionResult> loop_predicate_expression = decode_expression(context->expression());
        std::shared_ptr<Declaration> loop_predicate = convert_to_boolean(loop_predicate_expression->load(predicate_location), predicate_location);
        current_scope()->add_statement(std::make_shared<stmt::Jump>(locate(context), label_entry, loop_predicate, false));
        current_scope()->add_label(label_exit);
    }

    void Generator::decode_do_while_statement(CParser::IterationStatementContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Do/while statements are not implemented", locate(context));
    }

    void Generator::decode_for_statement(CParser::IterationStatementContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "For statements are not implemented", locate(context));
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
        const CodeLocation location = locate(context);

        std::shared_ptr<Declaration> current_function = current_scope()->function;
        if (current_function.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, "Return statement outside of a function definition", location);

        if (context->expression()) {
            TypeSpecification return_type_spec = current_function->spec.return_type();
            if (return_type_spec.is_void())
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't return a value in a function returning void", location);

            std::shared_ptr<ExpressionResult> expression_result = decode_expression(context->expression());
            std::shared_ptr<Declaration> return_value = emit_implicit_conversion(return_type_spec, expression_result->load(location), location);
            current_scope()->add_statement(std::make_shared<stmt::Return>(location, return_value));
        } else {
            if (!current_function->spec.return_type().is_void())
                throw Diagnostic(DiagnosticLevel::ERROR, "Return without a value within a function with a non-void return type", location);
            current_scope()->add_statement(std::make_shared<stmt::Return>(location));
        }
    }
}
