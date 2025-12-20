#include "xml_output.h"
#include "util/strings.h"

namespace toycc {
    XMLOutput::XMLOutput(const CParser* parser) : parser(parser) {
        output << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
    }

    std::string XMLOutput::result() {
        return output.str();
    }

    void XMLOutput::visitTerminal(antlr4::tree::TerminalNode* node) {
        write_indent();
        output << "<terminal text=\"" << xml_escape(node->getText()) << "\" />\n";
    }

    void XMLOutput::enterEveryRule(antlr4::ParserRuleContext *ctx) {
        write_indent();
        output << "<" << parser->getRuleNames()[ctx->getRuleIndex()] << ">\n";
        depth += 1;
    }

    void XMLOutput::exitEveryRule(antlr4::ParserRuleContext *ctx) {
        depth -= 1;
        write_indent();
        output << "</" << parser->getRuleNames()[ctx->getRuleIndex()] << ">\n";
    }

    void XMLOutput::visitErrorNode(antlr4::tree::ErrorNode *node) {
        write_indent();
        output << "<error text=\"" << xml_escape(node->getText()) << "\" />\n";
    };

    void XMLOutput::write_indent() {
        for (int level = 0; level < depth; level++)
            output << "  ";
    }
}
