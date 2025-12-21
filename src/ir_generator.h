#pragma once

#include <memory>
#include "ParserRuleContext.h"

#include "ir/declaration.h"
#include "ir/scope.h"
#include "parser/CParser.h"
#include "parser/CBaseListener.h"
#include "source_map.h"

namespace toycc {
    class IRGenerator : public CBaseListener {
        private:
            const SourceMap& source_map;
            std::shared_ptr<ir::Scope> global_scope;
            std::deque<std::shared_ptr<ir::Scope>> scope_stack;

            size_t unique_id = 0;

        public:
            IRGenerator(const SourceMap& source_map);

            virtual void exitDeclarationDeclaration(CParser::DeclarationDeclarationContext* context) override;

        private:
            void decode_declaration(CParser::DeclarationDeclarationContext* context);
            void decode_declaration_specifier(ir::Declaration& declaration, CParser::DeclarationSpecifiersContext* context);
            std::string decode_type_specifier(CParser::TypeSpecifierContext* context);

            Flags<ir::StorageClass> decode_storage_class(CParser::StorageClassSpecifierContext* context);
            Flags<ir::TypeQualifier> decode_type_qualifier_list(CParser::TypeQualifierListContext* context);
            Flags<ir::TypeQualifier> decode_type_qualifier(CParser::TypeQualifierContext* context);
            Flags<ir::FunctionSpecifier> decode_function_specifier(CParser::FunctionSpecifierContext* context);
            std::string decode_type_specifier(std::vector<CParser::TypeSpecifierContext*> specifiers);
            size_t resolve_alignment_specifier(CParser::AlignmentSpecifierContext* context);
            void decode_declarator(ir::Declaration& declaration, CParser::DeclaratorContext* context);
            void decode_direct_declarator(ir::Declaration& declaration, CParser::DirectDeclaratorContext* context);
            void decode_initializer(ir::Declaration const& declaration, CParser::InitializerContext* context);
            std::vector<Flags<ir::TypeQualifier>> decode_pointer_spec(CParser::PointerContext* context);

        private:
            std::shared_ptr<ir::Scope> current_scope();
            std::optional<CodeLocation> get_name_location(std::string name, bool current_scope_only = false);
            CodeLocation get_location(antlr4::ParserRuleContext* context) const;
            std::string anonymous_identifier();


    };
}
