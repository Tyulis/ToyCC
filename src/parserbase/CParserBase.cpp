#include <algorithm>

#include "CommonTokenStream.h"
#include "parser/CLexer.h"
#include "parserbase/SymbolTable.h"
#include "parserbase/CParserBase.h"
#include "parser/CParser.h"

namespace toycc {
    CParserBase::CParserBase(antlr4::TokenStream* input) : antlr4::Parser(input) {}

    antlr4::Token* CParserBase::next_token() {
        return static_cast<antlr4::CommonTokenStream*>(getTokenStream())->LT(1);
    }

    std::string CParserBase::next_token_text() {
        return next_token()->getText();
    }

    bool CParserBase::IsAlignmentSpecifier() {
        std::shared_ptr<Symbol> resolved = st.Resolve(next_token_text());
        if (resolved.get() == nullptr)
            return false;
        else if (resolved->classification & TypeClassification::AlignmentSpecifier)
            return true;
        else
            return false;
    }

    bool CParserBase::IsAtomicTypeSpecifier() {
        std::shared_ptr<Symbol> resolved = st.Resolve(next_token_text());
        if (resolved.get() == nullptr)
            return false;
        else if (resolved->classification & TypeClassification::AtomicTypeSpecifier)
            return true;
        else
            return false;
    }

    bool CParserBase::IsAttributeDeclaration() {
        return IsAttributeSpecifierSequence();
    }

    bool CParserBase::IsAttributeSpecifier() {
        return next_token()->getType() == CLexer::LeftBracket;
    }

    bool CParserBase::IsAttributeSpecifierSequence() {
        return IsAttributeSpecifier();
    }

    bool CParserBase::IsDeclaration() {
        return IsDeclarationSpecifiers()
            || IsAttributeSpecifierSequence()
            || IsStaticAssertDeclaration()
            || IsAttributeDeclaration();
    }

    bool CParserBase::IsDeclarationSpecifier() {
        return IsStorageClassSpecifier()
            || IsTypeSpecifier()
            || IsTypeQualifier()
            || IsFunctionSpecifier()
            || IsAlignmentSpecifier();
    }

    bool CParserBase::IsDeclarationSpecifiers() {
        return IsDeclarationSpecifier();
    }

    bool CParserBase::IsEnumSpecifier() {
        return next_token()->getType() == CLexer::Enum;
    }

    bool CParserBase::IsFunctionSpecifier() {
        std::shared_ptr<Symbol> resolved = st.Resolve(next_token_text());
        if (resolved.get() == nullptr) {
            //// C can reference unresolved types if it's a
            // pointer.
            //var la2 = (this.InputStream as CommonTokenStream).LT(2).Text;
            //if (la2 != null && la2 == "*")
            //    result = true;
            //else
            return false;
        } else if (resolved->classification & TypeClassification::FunctionSpecifier)
            return true;
        else
            return false;
    }

    bool CParserBase::IsStatement() {
        return !IsDeclaration();
    }

    bool CParserBase::IsStaticAssertDeclaration() {
        return next_token()->getType() == CLexer::Static_assert;
    }

    bool CParserBase::IsStorageClassSpecifier() {
        std::shared_ptr<Symbol> resolved = st.Resolve(next_token_text());
        if (resolved.get() == nullptr)
            return false;
        else if (resolved->classification & TypeClassification::StorageClassSpecifier)
            return true;
        else
            return false;
    }

    bool CParserBase::IsStructOrUnionSpecifier() {
        antlr4::Token* token = next_token();
        return token->getType() == CLexer::Struct || token->getType() == CLexer::Union;
    }


    bool CParserBase::IsTypedefName() {
        std::shared_ptr<Symbol> resolved = st.Resolve(next_token_text());
        if (resolved.get() == nullptr) {
            //// C can reference unresolved types if it's a
            // pointer.
            //var la2 = (this.InputStream as CommonTokenStream).LT(2).Text;
            //if (la2 != null && la2 == "*")
            //    result = true;
            //else
            return false;
        } else if (!(resolved->classification & TypeClassification::Variable))
            return true;
        else
            return false;
    }

    bool CParserBase::IsTypeofSpecifier() {
        antlr4::Token* token = next_token();
        return token->getType() == CLexer::Typeof || token->getType() == CLexer::Typeof_unqual;
    }

    bool CParserBase::IsTypeQualifier() {
        std::shared_ptr<Symbol> resolved = st.Resolve(next_token_text());
        if (resolved.get() == nullptr)
            return false;
        else if (resolved->classification & TypeClassification::TypeQualifier)
            return true;
        else
            return false;
    }


    bool CParserBase::IsTypeSpecifier() {
        std::shared_ptr<Symbol> resolved = st.Resolve(next_token_text());
        if (resolved.get() != nullptr && (resolved->classification & TypeClassification::TypeSpecifier))
            return true;
        return IsAtomicTypeSpecifier() || IsStructOrUnionSpecifier() || IsEnumSpecifier() || IsTypedefName() || IsTypeofSpecifier();
    }

    void CParserBase::EnterDeclaration() {
        antlr4::ParserRuleContext* context = this->getContext();
        CParser::DeclarationContext* declaration_context = static_cast<CParser::DeclarationContext*>(context);
        CParser::DeclarationSpecifiersContext* declaration_specifiers = declaration_context->declarationSpecifiers();
        std::vector<CParser::DeclarationSpecifierContext*> declaration_specifier = declaration_specifiers->declarationSpecifier();

        bool is_typedef = std::ranges::any_of(declaration_specifier, [](CParser::DeclarationSpecifierContext* ds) {
            if (!ds->storageClassSpecifier())
                return false;
            return ds->storageClassSpecifier()->Typedef() != nullptr;
        });

        // Declare any typeSpecifiers that declare something.
        for (CParser::DeclarationSpecifierContext* ds :declaration_specifier) {
            if (ds->typeSpecifier() && ds->typeSpecifier()->structOrUnionSpecifier()) {
                CParser::StructOrUnionSpecifierContext* sous = ds->typeSpecifier()->structOrUnionSpecifier();
                if (sous->Identifier()) {
                    std::string id = sous->Identifier()->getText();
                    st.Define(std::make_shared<Symbol>(id, TypeClassification::TypeSpecifier));
                }
            }
        }

        if (declaration_context->initDeclaratorList()) {
            CParser::InitDeclaratorListContext* init_declaration_list = declaration_context->initDeclaratorList();
            std::vector<CParser::InitDeclaratorContext*> x = init_declaration_list->initDeclarator();

            for (CParser::InitDeclaratorContext* id : x) {
                if (id->declarator() && id->declarator()->directDeclarator() && id->declarator()->directDeclarator()->Identifier()) {
                    antlr4::tree::TerminalNode* identifier = id->declarator()->directDeclarator()->Identifier();
                    // If a typedef is used in the declaration, the declarator
                    // itself is a type, not a variable.
                    std::string text = identifier->getText();
                    if (is_typedef)
                        st.Define(std::make_shared<Symbol>(text, TypeClassification::TypeSpecifier));
                    else
                        st.Define(std::make_shared<Symbol>(text, TypeClassification::Variable));
                }
            }
        }
    }

    // Define to return "true" because "gcc -c -std=c2x" accepts an empty
    // struct-declaration-list.
    bool CParserBase::IsNullStructDeclarationListExtension() {
        return true;
    }
}
