#pragma once

#include <string>
#include <vector>
#include <iostream>

#include "source_map.h"
#include "parser/CLexer.h"
#include "parser/CParser.h"

namespace toycc {
    class Parser {
        public:
            Parser(std::istream& code, SourceMap source_map);

            std::string to_tree_string();

        private:
            SourceMap source_map;

            antlr4::ANTLRInputStream input;
            parser::CLexer lexer;
            antlr4::CommonTokenStream tokens;
            parser::CParser parser;
    };
}
