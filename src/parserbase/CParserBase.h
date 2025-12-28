#pragma once

#include "Parser.h"
#include "TokenStream.h"
#include "parserbase/SymbolTable.h"

// Parser base class translated from https://github.com/kaby76/grammars-v4/commit/26a6b40d2af7b7689d6fcfff2080310455a725e4

namespace toycc {
    class CParserBase : public antlr4::Parser {
        private:
            SymbolTable st;

            antlr4::Token* next_token();
            std::string next_token_text();

        protected:
            CParserBase(antlr4::TokenStream* input);

        public:
            bool IsAlignmentSpecifier();
            bool IsAtomicTypeSpecifier();
            bool IsAttributeDeclaration();
            bool IsAttributeSpecifier();
            bool IsAttributeSpecifierSequence();
            bool IsDeclaration();
            bool IsDeclarationSpecifier();
            bool IsDeclarationSpecifiers();
            bool IsEnumSpecifier();
            bool IsFunctionSpecifier();
            bool IsStatement();
            bool IsStaticAssertDeclaration();
            bool IsStorageClassSpecifier();
            bool IsStructOrUnionSpecifier();
            bool IsTypedefName();
            bool IsTypeofSpecifier();
            bool IsTypeQualifier();
            bool IsTypeSpecifier();
            void EnterDeclaration();
            bool IsNullStructDeclarationListExtension();
    };
}
