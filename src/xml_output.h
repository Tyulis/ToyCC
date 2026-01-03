#pragma once

#include <sstream>

#include "gen/parser/CParser.h"
#include "gen/parser/CParserBaseListener.h"

namespace toycc {
    class XMLOutput : public CParserBaseListener {
        public:
            XMLOutput(const CParser* parser);
            std::string result();

            virtual void visitTerminal(antlr4::tree::TerminalNode *node) override;
            virtual void enterEveryRule(antlr4::ParserRuleContext *ctx) override;
            virtual void exitEveryRule(antlr4::ParserRuleContext *ctx) override;
            virtual void visitErrorNode(antlr4::tree::ErrorNode *node) override;

        private:
            std::stringstream output;
            const CParser* parser;
            int depth = 0;

            void write_indent();
    };
}
