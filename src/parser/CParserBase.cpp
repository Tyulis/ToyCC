#include <algorithm>

#include "CommonTokenStream.h"
#include "parser/SymbolTable.h"
#include "parser/CParserBase.h"
#include "gen/parser/CLexer.h"
#include "gen/parser/CParser.h"

namespace toycc {
    CParserBase::CParserBase(antlr4::TokenStream* input) : antlr4::Parser(input) {}

    antlr4::Token* CParserBase::next_token(size_t index) {
        return static_cast<antlr4::CommonTokenStream*>(getTokenStream())->LT(index);
    }

    std::string CParserBase::next_token_text(size_t index) {
        return next_token(index)->getText();
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

    bool CParserBase::IsTypeSpecifierQualifier() {
        return IsTypeSpecifier()
            || IsTypeQualifier()
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
        if (next_token(1)->getType() == CLexer::Identifier && next_token(2)->getType() == CLexer::Colon)
            return true;
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
        if (resolved.get() == nullptr)
            return false;
        else if (resolved->classification & TypeClassification::Variable)
            return false;
        else if (resolved->classification & TypeClassification::Function)
            return false;
        else
            return true;
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

    static antlr4::Token* GetDeclarationToken(CParser::DeclaratorContext* y) {
        // Go down the tree and find a declarator with Identifier.
        if (y == nullptr)
            return nullptr;

        // Check if this declarator has a direct declarator with an identifier
        if (y->directDeclarator() && y->directDeclarator()->baseDirectDeclarator()) {
            CParser::BaseDirectDeclaratorContext* baseDeclarator = y->directDeclarator()->baseDirectDeclarator();
            CParser::DeclaratorContext* more = baseDeclarator->declarator();
            antlr4::Token* token = GetDeclarationToken(more);
            if (token != nullptr)
                return token;
            if (baseDeclarator->Identifier() != nullptr)
                return baseDeclarator->Identifier()->getSymbol();
        }

        return nullptr;
    }

    static std::optional<std::string> GetDeclarationId(CParser::DeclaratorContext* y) {
        antlr4::Token* token = GetDeclarationToken(y);
        if (token == nullptr)  return {};
        else                   return token->getText();
    }

    void CParserBase::EnterDeclaration() {
        antlr4::ParserRuleContext* context = this->getContext();
        for (; context != nullptr; context = static_cast<antlr4::ParserRuleContext*>(context->parent)) {
            if (CParser::DeclarationContext::is(context)) {
                CParser::DeclarationContext* declaration_context = static_cast<CParser::DeclarationContext*>(context);
                std::vector<CParser::DeclarationSpecifierContext*> declaration_specifier;
                if (declaration_context->declarationSpecifiers())
                    declaration_specifier.append_range(declaration_context->declarationSpecifiers()->declarationSpecifier());

                // Declare any typeSpecifiers that declare something.
                for (CParser::DeclarationSpecifierContext* ds : declaration_specifier)
                    if (ds->typeSpecifier() && ds->typeSpecifier()->structOrUnionSpecifier() && ds->typeSpecifier()->structOrUnionSpecifier()->Identifier())
                        st.Define(std::make_shared<Symbol>(ds->typeSpecifier()->structOrUnionSpecifier()->Identifier()->getText(), TypeClassification::TypeSpecifier));

                if (declaration_context->initDeclaratorList()) {
                    std::vector<CParser::InitDeclaratorContext*> init_declarators = declaration_context->initDeclaratorList()->initDeclarator();

                    bool is_typedef = std::ranges::any_of(declaration_specifier, [](CParser::DeclarationSpecifierContext* ds) {
                        if (!ds->storageClassSpecifier())
                            return false;
                        return ds->storageClassSpecifier()->Typedef() != nullptr;
                    });

                    for (CParser::InitDeclaratorContext* id : init_declarators) {
                        if (!id->declarator())
                            continue;

                        std::optional<std::string> text = GetDeclarationId(id->declarator());
                        if (!text.has_value())
                            continue;

                        // If a typedef is used in the declaration, the declarator itself is a type, not a variable.
                        if (is_typedef)
                            st.Define(std::make_shared<Symbol>(text.value(), TypeClassification::TypeSpecifier));
                        else
                            st.Define(std::make_shared<Symbol>(text.value(), TypeClassification::Variable));
                    }
                }
            } else if (CParser::FunctionDefinitionContext::is(context)) {
                CParser::FunctionDefinitionContext* fd = static_cast<CParser::FunctionDefinitionContext*>(context);
                if (fd->declarator() && fd->declarator()->directDeclarator() && fd->declarator()->directDeclarator()->baseDirectDeclarator()->Identifier()) {
                    std::optional<std::string> text = fd->declarator()->directDeclarator()->baseDirectDeclarator()->Identifier()->getText();
                    st.Define(std::make_shared<Symbol>(text.value(), TypeClassification::Function));
                    return;
                }
            }
        }
    }

    // Define to return "true" because "gcc -c -std=c2x" accepts an empty struct-declaration-list.
    bool CParserBase::IsNullStructDeclarationListExtension() {
        return true;
    }

    void CParserBase::EnterScope() {
        st.PushBlockScope();
    }

    void CParserBase::ExitScope() {
        st.PopBlockScope();
    }

    bool CParserBase::IsCast() {
        antlr4::Token* t1 = next_token(1);
        antlr4::Token* t2 = next_token(2);
        if (t1->getType() != CLexer::LeftParen) {
            return false;
        } else if (t2->getType() != CLexer::Identifier) {
            return true;
        } else {
            std::shared_ptr<Symbol> resolved = st.Resolve(t2->getText());
            if (resolved.get() == nullptr)
                return false;
            else if (resolved->classification & TypeClassification::TypeSpecifier)
                return true;
            else
                return false;
        }
    }
}
