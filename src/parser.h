#pragma once

#include <string>
#include <iostream>

#include "BaseErrorListener.h"

#include "source_map.h"
#include "gen/parser/CLexer.h"
#include "gen/parser/CParser.h"
#include "ir/scope.h"

namespace toycc {
    class ErrorHandler : public antlr4::BaseErrorListener {
    public:
        ErrorHandler(const SourceMap& source_map);

        virtual void syntaxError([[maybe_unused]] antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                size_t charPositionInLine, const std::string &msg, [[maybe_unused]] std::exception_ptr e) override;

        void check() const;

    private:
        const SourceMap& source_map;
        size_t nof_errors = 0;
    };

    class Parser {
        public:
            Parser(std::istream& code, SourceMap source_map);

            std::string to_lisp();
            std::string to_xml();
            std::shared_ptr<ir::Scope> to_ir();

        private:
            SourceMap source_map;
            ErrorHandler error_handler;

            antlr4::ANTLRInputStream input;
            CLexer lexer;
            antlr4::CommonTokenStream tokens;
            CParser parser;
    };
}
