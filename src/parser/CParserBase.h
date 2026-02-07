#pragma once

#include <memory>
#include <string>

#include "Parser.h"
#include "TokenStream.h"
#include "code_location.h"
#include "parser/SymbolTable.h"

// Parser base class translated from https://github.com/antlr/grammars-v4/blob/master/c/CSharp/CParserBase.cs

namespace toycc {
    class CParserBase : public antlr4::Parser {
        private:
            SymbolTable st;

            antlr4::Token* next_token(size_t index = 1);
            std::string next_token_text(size_t index = 1);

            std::shared_ptr<Symbol> ResolveWithOutput(antlr4::Token* token);
            CodeLocation GetSourceLocation(antlr4::Token* token);

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
            bool IsTypeSpecifierQualifier();
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
            void EnterScope();
            void ExitScope();
            bool IsCast();
    };
}
