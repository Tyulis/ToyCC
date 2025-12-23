#pragma once

#include <memory>
#include "ParserRuleContext.h"

#include "ir/compound_type.h"
#include "ir/declaration.h"
#include "ir/scope.h"
#include "ir/statement.h"
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
            // -------- Declarations
            void decode_declaration(CParser::DeclarationDeclarationContext* context);
            void decode_declaration_specifier(ir::Declaration& declaration, CParser::DeclarationSpecifiersContext* context);
            ir::TypeSpecification decode_specifier_qualifier_list(CParser::SpecifierQualifierListContext* context);

            Flags<ir::StorageClass> decode_storage_class(CParser::StorageClassSpecifierContext* context);
            Flags<ir::TypeQualifier> decode_type_qualifier_list(CParser::TypeQualifierListContext* context);
            Flags<ir::TypeQualifier> decode_type_qualifier(CParser::TypeQualifierContext* context);
            Flags<ir::FunctionSpecifier> decode_function_specifier(CParser::FunctionSpecifierContext* context);

            ir::TypeSpecification resolve_type_specifier(std::vector<CParser::TypeSpecifierContext*> specifiers, bool is_typedef);
            ir::TypeIdentifier decode_type_specifier(std::vector<CParser::TypeSpecifierContext*> specifiers);
            ir::TypeIdentifier decode_struct_or_union_specifier(CParser::StructOrUnionSpecifierContext* context);
            std::vector<ir::StructMember> decode_struct_declaration(CParser::StructDeclarationContext* context);

            size_t resolve_alignment_specifier(CParser::AlignmentSpecifierContext* context);
            std::optional<std::string> decode_declarator(ir::TypeSpecification& spec, CParser::DeclaratorContext* context);
            std::optional<std::string> decode_direct_declarator(ir::TypeSpecification& spec, CParser::DirectDeclaratorContext* context);
            void decode_initializer(ir::Declaration const& declaration, CParser::InitializerContext* context);
            std::vector<Flags<ir::TypeQualifier>> decode_pointer_spec(CParser::PointerContext* context);

            // -------- Expressions
            std::string decode_assignment_expression(CParser::AssignmentExpressionContext* context);
            std::optional<ir::BinaryOperator> decode_assignment_operator(CParser::AssignmentOperatorContext* context);

            // -------- Utilities
            std::shared_ptr<ir::Scope> current_scope();
            std::optional<CodeLocation> get_name_location(std::string name, bool current_scope_only = false);
            CodeLocation get_location(antlr4::ParserRuleContext* context) const;
            std::string anonymous_identifier();

            void add_builtin_type(std::string name);
            void add_primitive_type(std::string name, bool is_signed, ir::PrimitiveSemantic semantic, size_t size, size_t alignment);
            std::shared_ptr<ir::Statement> add_statement(ir::Statement& statement);
            std::optional<ir::TypeSpecification> resolve_type(ir::TypeIdentifier identifier);
    };
}
