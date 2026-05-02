#include "diagnostic.h"
#include "ir/statement.h"
#include "arch/datamodel.h"
#include "semantic/analyzer.h"

namespace toycc::semantic {
    std::shared_ptr<Scope> SemanticAnalyzer::decode_compound_statement(CParser::CompoundStatementContext* context, ScopeType type, std::string entry_label, std::string exit_label) {
        std::shared_ptr<Scope> scope = std::make_shared<Scope>(type, current_scope()->function, entry_label, exit_label);
        decode_compound_statement(context, scope);
        emit(Statement::make_block(locate(context), scope));
        return scope;
    }

    void SemanticAnalyzer::decode_compound_statement(CParser::CompoundStatementContext* context, std::shared_ptr<Scope> scope) {
        ScopeFrame frame = in_scope(scope);

        if (context->blockItemList())
            decode_block_item_list(context->blockItemList());
    }

    void SemanticAnalyzer::decode_block_item_list(CParser::BlockItemListContext* context) {
        for (CParser::BlockItemContext* item : context->blockItem()) {
            if (item->declaration())
                decode_declaration(item->declaration());
            else if (item->statement())
                decode_statement(item->statement());
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown block item type `{}`", item->getText()), locate(item));
        }
    }

    void SemanticAnalyzer::decode_statement(CParser::StatementContext* context, std::optional<ScopeType> scope_type, std::string entry_label, std::string exit_label) {
        if (context->compoundStatement()) {
            decode_compound_statement(context->compoundStatement(), scope_type.value_or(ScopeType::BLOCK), entry_label, exit_label);
            return;
        }

        // When the scope type is set (= it's a conditional, loop, ...) but this not a compound statement, emulate it by pushing a new scope for the statement
        std::shared_ptr<Scope> statement_scope = nullptr;
        if (scope_type.has_value()) {
            statement_scope = std::make_shared<Scope>(scope_type.value(), current_scope()->function, entry_label, exit_label);
            scope_stack.push_back(statement_scope);
        }

        if (context->labeledStatement())
            decode_labeled_statement(context->labeledStatement());
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

        // Pop the emulated scope
        if (scope_type.has_value()) {
            scope_stack.pop_back();
            emit(Statement::make_block(locate(context), statement_scope));
        }
    }

    void SemanticAnalyzer::decode_expression_statement(CParser::ExpressionStatementContext* context) {
        if (context->expression())
            decode_expression(context->expression());
        // Otherwise it's a trailing semicolon -> skip
    }

    void SemanticAnalyzer::decode_selection_statement(CParser::SelectionStatementContext* context) {
        if (context->If())
            decode_if_statement(context);
        else if (context->Switch())
            decode_switch_statement(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown selection statement `{}`", context->getText()), locate(context));
    }

    void SemanticAnalyzer::decode_if_statement(CParser::SelectionStatementContext* context) {
        const CodeLocation predicate_location = locate(context->expression());
        ExpressionResult predicate_expression = decode_expression(context->expression());

        const std::string label_after_if = anonymous_label();
        emit_conditional_jump(predicate_expression, label_after_if, false, locate(context));

        decode_statement(context->statement(0), ScopeType::CONDITIONAL);

        if (context->Else()) {
            const std::string label_after_else = anonymous_label();
            emit(Statement::make_jump(locate(context), label_after_else));     // If we entered the `if`, skip the `else` part
            emit_label(LabelType::INTERNAL, label_after_if, locate(context));  // Must be after the `else` skip otherwise the `else` path jumps to the second jump statement

            decode_statement(context->statement(1), ScopeType::CONDITIONAL);
            emit_label(LabelType::INTERNAL, label_after_else, locate(context));
        } else {
            emit_label(LabelType::INTERNAL, label_after_if, locate(context));
        }
    }

    void SemanticAnalyzer::decode_switch_statement(CParser::SelectionStatementContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Switch statements are not implemented", locate(context));
    }

    void SemanticAnalyzer::decode_iteration_statement(CParser::IterationStatementContext* context) {
        if (context->Do() && context->While())
            decode_do_while_statement(context);
        else if (!context->Do() && context->While())
            decode_while_statement(context);
        else if (context->For())
            decode_for_statement(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown iteration statement `{}`", context->getText()), locate(context));
    }

    void SemanticAnalyzer::decode_while_statement(CParser::IterationStatementContext* context) {
        const std::string entry_label = anonymous_label();
        const std::string exit_label  = anonymous_label();

        // First evaluation of the predicate right before the loop : when already false, don't enter
        const CodeLocation predicate_location = locate(context->expression());
        ExpressionResult entry_predicate_expression = decode_expression(context->expression());
        emit_conditional_jump(entry_predicate_expression, exit_label, false, predicate_location);

        // Then the loop body
        emit_label(LabelType::INTERNAL, entry_label, locate(context));
        decode_statement(context->statement(), ScopeType::LOOP, entry_label, exit_label);

        // Second evaluation of the predicate into the loop : when true, jump back to the beginning of the loop
        ExpressionResult loop_predicate_expression = decode_expression(context->expression());
        emit_conditional_jump(loop_predicate_expression, entry_label, true, predicate_location);
        emit_label(LabelType::INTERNAL, exit_label, locate(context));
    }

    void SemanticAnalyzer::decode_do_while_statement(CParser::IterationStatementContext* context) {
        const std::string entry_label = anonymous_label();
        const std::string exit_label  = anonymous_label();

        // Loop body, enter unconditionally
        emit_label(LabelType::INTERNAL, entry_label, locate(context));
        decode_statement(context->statement(), ScopeType::LOOP, entry_label, exit_label);

        // Evaluate the predicate at the end : when true, jump back to the beginning of the loop, otherwise fall through to exit
        ExpressionResult loop_predicate_expression = decode_expression(context->expression());
        emit_conditional_jump(loop_predicate_expression, entry_label, true, locate(context->expression()));
        emit_label(LabelType::INTERNAL, exit_label, locate(context));
    }

    void SemanticAnalyzer::decode_for_statement(CParser::IterationStatementContext* context) {
        // The for loop initialization is outside of the enclosing scope, push a new scope for it
        std::shared_ptr<Scope> scope = std::make_shared<Scope>(ScopeType::BLOCK, current_scope()->function);

        {
            ScopeFrame frame = in_scope(scope);

            // Emit the initialization before anything else
            CParser::ForConditionContext* for_condition = context->forCondition();
            if (for_condition->forDeclaration())
                decode_for_declaration(for_condition->forDeclaration());
            else if (for_condition->expression())
                decode_expression(for_condition->expression());
            // Otherwise that's an empty statement, no initialization

            const std::string entry_label = anonymous_label();
            const std::string exit_label  = anonymous_label();

            CParser::ForExpressionContext* predicate_context = for_condition->forPredicate;
            CParser::ForExpressionContext* increment_context = for_condition->forIncrement;

            // NOTE : No predicate means an infinite loop. Here, skipping the predicate means there is no exit condition
            if (predicate_context) {
                // First evaluation of the predicate right before the loop : when already false, don't enter
                ExpressionResult entry_predicate_expression = decode_for_expression(predicate_context);
                emit_conditional_jump(entry_predicate_expression, exit_label, false, locate(predicate_context));
            }

            // Then the loop body
            emit_label(LabelType::INTERNAL, entry_label, locate(context));
            decode_statement(context->statement(), ScopeType::LOOP, entry_label, exit_label);

            if (predicate_context) {
                // Second evaluation of the predicate into the loop : when false, exit
                ExpressionResult loop_predicate_expression = decode_for_expression(predicate_context);

                if (increment_context)  // With increment: predicate == true : fall through to the increment, jump afterwards | predicate == false : jump out of the loop
                    emit_conditional_jump(loop_predicate_expression, exit_label, false, locate(predicate_context));
                else  // Without increment : predicate == true : directly jump back to the beginning of the loop | predicate == false : fall through to the exit
                    emit_conditional_jump(loop_predicate_expression, entry_label, true, locate(predicate_context));
            }

            // When the loop predicate is true -> fall through to the increment statement then jump back to the beginning of the loop
            if (increment_context) {
                decode_for_expression(increment_context);
                emit(Statement::make_jump(locate(predicate_context), entry_label));
            }

            // After the loop
            emit_label(LabelType::INTERNAL, exit_label, locate(context));
        }

        emit(Statement::make_block(locate(context->statement()), scope));
    }

    void SemanticAnalyzer::decode_jump_statement(CParser::JumpStatementContext* context) {
        if (context->Goto())
            decode_goto_statement(context);
        else if (context->Continue())
            decode_continue_statement(context);
        else if (context->Break())
            decode_break_statement(context);
        else if (context->Return())
            decode_return_statement(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown jump statement `{}`", context->getText()), locate(context));
    }

    void SemanticAnalyzer::decode_goto_statement(CParser::JumpStatementContext* context) {
        emit(Statement::make_jump(locate(context), context->Identifier()->getText()));
    }

    void SemanticAnalyzer::decode_break_statement(CParser::JumpStatementContext* context) {
        std::shared_ptr<Scope> break_scope = upper_scope_of_type({ScopeType::LOOP, ScopeType::SWITCH});

        if (break_scope.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, "`break` statement outside of a loop or `switch` block", locate(context));

        emit(Statement::make_jump(locate(context), break_scope->exit_label));
    }

    void SemanticAnalyzer::decode_continue_statement(CParser::JumpStatementContext* context) {
        std::shared_ptr<Scope> continue_scope = upper_scope_of_type({ScopeType::LOOP});

        if (continue_scope.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, "`continue` statement outside of a loop", locate(context));

        emit(Statement::make_jump(locate(context), continue_scope->entry_label));
    }

    void SemanticAnalyzer::decode_return_statement(CParser::JumpStatementContext* context) {
        const CodeLocation location = locate(context);

        std::shared_ptr<Declaration> current_function = current_scope()->function;
        if (current_function.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, "Return statement outside of a function definition", location);
        if (current_function->type->category != TypeCategory::FUNCTION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The scope's current function doesn't have a function type", location);

        std::shared_ptr<FunctionType> function_type = std::static_pointer_cast<FunctionType> (current_function->type);

        if (context->expression()) {
            if (function_type->return_type->category == TypeCategory::VOID)
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't return a value in a function returning void", location);

            ExpressionResult expression_result = decode_expression(context->expression());
            Operand return_value = emit_implicit_conversion(function_type->return_type, expression_result.operand(), location);
            emit(Statement::make_return(location, return_value));
        } else {
            if (function_type->return_type->category != TypeCategory::VOID)
                throw Diagnostic(DiagnosticLevel::ERROR, "Return without a value within a function with a non-void return type", location);
            emit(Statement::make_return(location));
        }
    }

    void SemanticAnalyzer::decode_labeled_statement(CParser::LabeledStatementContext* context) {
        if (context->Case())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Case statements are not implemented", locate(context));
        else if (context->Default())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Default statements are not implemented", locate(context));
        else
            emit_label(LabelType::NAMED, context->Identifier()->getText(), locate(context));

        decode_statement(context->statement());
    }


    void SemanticAnalyzer::emit_conditional_jump(const ExpressionResult& predicate_expression, std::string destination_label, bool jump_if_is, CodeLocation location) {
        Operand predicate = emit_implicit_conversion(arch::DATAMODEL->boolean_type, predicate_expression.operand(), location);
        emit(Statement::make_conditional_jump(location, predicate, destination_label, jump_if_is));
    }
}
