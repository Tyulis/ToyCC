#include "ir/generator.h"

namespace toycc::ir {
    std::string Generator::anonymous_identifier() {
        return std::format("@I{}", unique_id++);
    }

    std::string Generator::anonymous_label() {
        return std::format("@L{}", unique_id++);
    }

    std::shared_ptr<Scope> Generator::current_scope() {
        return scope_stack.back();
    }

    CodeLocation Generator::locate(antlr4::ParserRuleContext* context) const {
        antlr4::Token* start_token = context->getStart();
        LinePosition line = source_map.at(start_token->getLine());
        return {.filename = line.filename, .line = line.line, .character = start_token->getCharPositionInLine()};
    }

    CodeLocation Generator::locate(antlr4::tree::TerminalNode* token) const {
        LinePosition line = source_map.at(token->getSymbol()->getLine());
        return {.filename = line.filename, .line = line.line, .character = token->getSymbol()->getCharPositionInLine()};
    }
}
