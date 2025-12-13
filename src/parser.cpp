#include "parser.h"

namespace toycc {
    Parser::Parser(std::istream& code, SourceMap source_map) : source_map(source_map), input(code), lexer(&input), tokens(&lexer), parser(&tokens) {

    }

    std::string Parser::to_tree_string() {
        toycc::parser::CParser::CompilationUnitContext* unit = parser.compilationUnit();
        return unit->toStringTree(true);
    }
}
