#include "semantic/analyzer.h"

namespace toycc::semantic {
    std::string SemanticAnalyzer::anonymous_identifier() {
        return std::format(".GI{}", unique_id++);
    }

    std::string SemanticAnalyzer::anonymous_label() {
        return std::format(".GL{}", unique_id++);
    }

    std::string SemanticAnalyzer::anonymous_type() {
        return std::format(".GT{}", unique_id++);
    }

    std::shared_ptr<Scope> SemanticAnalyzer::current_scope() {
        return scope_stack.back();
    }

    ScopeFrame SemanticAnalyzer::in_scope(std::shared_ptr<Scope> scope) {
        return {scope_stack, scope};
    }

    std::shared_ptr<Statement> SemanticAnalyzer::emit(std::shared_ptr<Statement> statement) {
        return current_scope()->add_statement(statement);
    }

    std::shared_ptr<Label> SemanticAnalyzer::emit_label(LabelType type, std::string name, CodeLocation location) {
        return current_scope()->add_label(type, name, location);
    }

    CodeLocation SemanticAnalyzer::locate(antlr4::ParserRuleContext* context) const {
        antlr4::Token* start_token = context->getStart();
        LinePosition line = source_map.at(start_token->getLine());
        return {.filename = line.filename, .line = line.line, .character = start_token->getCharPositionInLine()};
    }

    CodeLocation SemanticAnalyzer::locate(antlr4::tree::TerminalNode* token) const {
        LinePosition line = source_map.at(token->getSymbol()->getLine());
        return {.filename = line.filename, .line = line.line, .character = token->getSymbol()->getCharPositionInLine()};
    }
}
