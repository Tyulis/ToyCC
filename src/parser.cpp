#include <format>
#include <string>

#include "Recognizer.h"
#include "Token.h"

#include "parser.h"
#include "diagnostic.h"

namespace toycc {
    ErrorHandler::ErrorHandler(const SourceMap& source_map) : source_map(source_map) {}

    void ErrorHandler::syntaxError([[maybe_unused]] antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                                   size_t charPositionInLine, const std::string &msg, [[maybe_unused]] std::exception_ptr e)
    {
        const LinePosition source = source_map.at(line);
        std::string message = (offendingSymbol == nullptr)? std::format("Lexer error : {}", msg)
                                                          : std::format("Syntax error : {}", msg);
        Diagnostic diagnostic(Diagnostic::Level::ERROR, message, source.filename, source.line, charPositionInLine);
        std::cerr << diagnostic.message() << std::endl;

        nof_errors += 1;
    }

    void ErrorHandler::check() const {
        if (nof_errors > 0)
            throw Diagnostic(Diagnostic::Level::ERROR, std::format("{} parsing error(s) found", nof_errors));
    }

    Parser::Parser(std::istream& code, SourceMap source_map) : source_map(source_map), error_handler(this->source_map), input(code), lexer(&input), tokens(&lexer), parser(&tokens) {
        lexer.removeErrorListeners();
        parser.removeErrorListeners();

        lexer.addErrorListener(&error_handler);
        parser.addErrorListener(&error_handler);
    }

    std::string Parser::to_tree_string() {
        toycc::parser::CParser::CompilationUnitContext* unit = parser.compilationUnit();
        const std::string tree_string = unit->toStringTree(true);

        error_handler.check();
        return tree_string;
    }
}
