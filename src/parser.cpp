#include <format>
#include <string>

#include "ParseTreeWalker.h"
#include "Recognizer.h"
#include "Token.h"

#include "parser.h"
#include "xml_output.h"
#include "diagnostic.h"
#include "ir_generator.h"
#include "util/strings.h"

namespace toycc {
    ErrorHandler::ErrorHandler(const SourceMap& source_map) : source_map(source_map) {}

    void ErrorHandler::syntaxError([[maybe_unused]] antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                                   size_t charPositionInLine, const std::string &msg, [[maybe_unused]] std::exception_ptr e)
    {
        const LinePosition source = source_map.at(line);
        std::string message = (offendingSymbol == nullptr)? std::format("Lexer error : {}", msg)
                                                          : std::format("Syntax error near `{}` : {}", to_printable(offendingSymbol->getText()), msg);
        Diagnostic diagnostic(Diagnostic::Level::ERROR, message, source.filename, source.line, charPositionInLine);
        std::cerr << diagnostic << std::endl;

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

    std::string Parser::to_lisp() {
        toycc::CParser::CompilationUnitContext* unit = parser.compilationUnit();
        const std::string tree_lisp = unit->toStringTree(true);

        error_handler.check();
        return tree_lisp;
    }

    std::string Parser::to_xml() {
        toycc::CParser::CompilationUnitContext* unit = parser.compilationUnit();
        XMLOutput listener(&parser);
        antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, unit);

        error_handler.check();
        return listener.result();
    }

    std::string Parser::to_ir() {
        toycc::CParser::CompilationUnitContext* unit = parser.compilationUnit();
        IRGenerator listener(source_map);
        antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, unit);

        error_handler.check();
        return "";
    }
}
