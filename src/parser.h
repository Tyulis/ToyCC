#pragma once

#include <string>
#include <iostream>

#include "BaseErrorListener.h"

#include "source_map.h"
#include "parser/CLexer.h"
#include "parser/CParser.h"

namespace toycc {
    class ErrorHandler : public antlr4::BaseErrorListener {
    public:
        ErrorHandler(const SourceMap& source_map);

        virtual void syntaxError([[maybe_unused]] antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                                 size_t charPositionInLine, const std::string &msg, [[maybe_unused]] std::exception_ptr e);

        void check() const;

    private:
        const SourceMap& source_map;
        size_t nof_errors = 0;
    };

    class Parser {
        public:
            Parser(std::istream& code, SourceMap source_map);

            std::string to_tree_string();

        private:
            SourceMap source_map;
            ErrorHandler error_handler;

            antlr4::ANTLRInputStream input;
            parser::CLexer lexer;
            antlr4::CommonTokenStream tokens;
            parser::CParser parser;
    };
}
