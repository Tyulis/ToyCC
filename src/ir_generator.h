#pragma once

#include <memory>
#include "ParserRuleContext.h"

#include "ir/compound_type.h"
#include "ir/declaration.h"
#include "ir/scope.h"
#include "ir/statement.h"
#include "ir/type.h"
#include "parser/CParser.h"
#include "source_map.h"

namespace toycc {
    std::shared_ptr<ir::Scope> generate_ir(const SourceMap& source_map, CParser::CompilationUnitContext* context);

    // NOTE : This is functionally a visitor, but the ANTLR visitor would require any_casts everywhere, that would be really unwieldy
    class IRGenerator {
        private:
            const SourceMap& source_map;
            std::deque<std::shared_ptr<ir::Scope>> scope_stack;

            size_t unique_id = 0;

        public:
            IRGenerator(const SourceMap& source_map, CParser::CompilationUnitContext* context);
            std::shared_ptr<ir::Scope> get();

        private:
            // -------- External declarations
            void decode_compilation_unit(CParser::CompilationUnitContext* context);
            void decode_translation_unit(CParser::TranslationUnitContext* context);
            void decode_external_declaration(CParser::ExternalDeclarationContext* context);
            void decode_function_definition(CParser::FunctionDefinitionContext* context);

            // -------- Statements
            void decode_compound_statement(CParser::CompoundStatementContext* context, std::shared_ptr<ir::Scope> scope);
            std::shared_ptr<ir::Scope> decode_compound_statement(CParser::CompoundStatementContext* context);

            void decode_block_item_list(CParser::BlockItemListContext* context);
            void decode_statement(CParser::StatementContext* context);
            void decode_jump_statement(CParser::JumpStatementContext* context);
            void decode_return_statement(CParser::JumpStatementContext* context);

            // -------- Declarations
            void decode_declaration(CParser::DeclarationContext* context);
            void decode_declaration_specifiers(ir::Declaration& declaration, std::vector<CParser::DeclarationSpecifierContext*> specifiers);
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
            std::optional<std::string> decode_function_direct_declarator(ir::TypeSpecification& spec, CParser::DirectDeclaratorContext* context);
            std::vector<ir::Declaration> decode_parameter_type_list(CParser::ParameterTypeListContext* context);
            std::vector<ir::Declaration> decode_parameter_list(CParser::ParameterListContext* context);
            ir::Declaration decode_parameter_declaration(CParser::ParameterDeclarationContext* context);
            void decode_initializer(ir::Declaration const& declaration, CParser::InitializerContext* context);
            std::vector<Flags<ir::TypeQualifier>> decode_pointer_spec(CParser::PointerContext* context);

            // -------- Expressions
            std::shared_ptr<ir::Declaration> decode_initializer(CParser::InitializerContext* context);
            std::shared_ptr<ir::Declaration> decode_expression(CParser::ExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_assignment_expression(CParser::AssignmentExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_conditional_expression(CParser::ConditionalExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_logical_or_expression(CParser::LogicalOrExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_logical_and_expression(CParser::LogicalAndExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_inclusive_or_expression(CParser::InclusiveOrExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_exclusive_or_expression(CParser::ExclusiveOrExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_and_expression(CParser::AndExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_equality_expression(CParser::EqualityExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_relational_expression(CParser::RelationalExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_shift_expression(CParser::ShiftExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_additive_expression(CParser::AdditiveExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_multiplicative_expression(CParser::MultiplicativeExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_cast_expression(CParser::CastExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_unary_expression(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_postfix_expression(CParser::PostfixExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_primary_expression(CParser::PrimaryExpressionContext* context);
            std::shared_ptr<ir::Declaration> decode_constant(antlr4::tree::TerminalNode* terminal);
            std::shared_ptr<ir::Declaration> decode_character_constant(antlr4::tree::TerminalNode* terminal);
            std::shared_ptr<ir::Declaration> decode_floating_constant(antlr4::tree::TerminalNode* terminal);
            std::shared_ptr<ir::Declaration> decode_integer_constant(antlr4::tree::TerminalNode* terminal);
            std::shared_ptr<ir::Declaration> decode_decimal_constant(antlr4::tree::TerminalNode* terminal);
            std::shared_ptr<ir::Declaration> decode_hexadecimal_constant(antlr4::tree::TerminalNode* terminal);
            std::shared_ptr<ir::Declaration> decode_binary_constant(antlr4::tree::TerminalNode* terminal);
            std::shared_ptr<ir::Declaration> decode_octal_constant(antlr4::tree::TerminalNode* terminal);
            std::shared_ptr<ir::Declaration> decode_string_literal(std::vector<antlr4::tree::TerminalNode*> terminals);

            std::shared_ptr<ir::Declaration> declare_integer_constant(size_t value, std::string suffix, CodeLocation location);


            std::optional<ir::stmt::BinaryOperator> decode_assignment_operator(CParser::AssignmentOperatorContext* context);
            ir::stmt::BinaryOperator decode_multiplicative_operator(CParser::MultiplicativeOperatorContext* context);
            ir::stmt::BinaryOperator decode_additive_operator(CParser::AdditiveOperatorContext* context);


            // -------- IR emission common functions
            std::shared_ptr<ir::Declaration> emit_implicit_conversion(ir::TypeSpecification target, std::shared_ptr<ir::Declaration> source, CodeLocation location);
            std::array<std::shared_ptr<ir::Declaration>, 2> emit_arithmetic_conversion(std::shared_ptr<ir::Declaration> left, std::shared_ptr<ir::Declaration> right, CodeLocation location);
            void emit_copy(std::shared_ptr<ir::Declaration> destination, std::shared_ptr<ir::Declaration> source, CodeLocation location, bool initialize);

            Flags<ir::stmt::ConversionOperation> implicit_conversion_operation(ir::TypeSpecification destination, ir::TypeSpecification source, CodeLocation location);
            Flags<ir::stmt::ConversionOperation> explicit_conversion_operation(ir::TypeSpecification destination, ir::TypeSpecification source, CodeLocation location);

            // -------- Utilities
            std::shared_ptr<ir::Scope> current_scope();
            std::optional<CodeLocation> locate_name(std::string name, bool current_scope_only = false);
            CodeLocation locate(antlr4::ParserRuleContext* context) const;
            CodeLocation locate(antlr4::tree::TerminalNode* context) const;
            std::string anonymous_identifier();

            void add_builtin_type(std::string name);
            void add_primitive_type(std::string name, bool is_signed, ir::PrimitiveSemantic semantic, size_t size, size_t alignment);
            void init_global_scope();
            std::shared_ptr<ir::Scope> create_function_scope(std::shared_ptr<ir::Declaration> declaration);
            std::shared_ptr<ir::Declaration> declare(ir::Declaration declaration);
            std::shared_ptr<ir::Declaration> declare_temporary(ir::TypeSpecification spec, CodeLocation location);
            std::shared_ptr<ir::Statement> add_statement(std::shared_ptr<ir::Statement> statement);

            std::shared_ptr<ir::Declaration> resolve_without_error(std::string name);
            std::shared_ptr<ir::Declaration> resolve(std::string name, CodeLocation location);
            std::optional<ir::TypeSpecification> resolve_type_without_error(ir::TypeIdentifier identifier);
            ir::TypeSpecification resolve_type(ir::TypeIdentifier identifier, CodeLocation location);

            // RAII class for pushing and popping scopes off the scope stack
            class ScopeFrame {
                public:
                    ScopeFrame(std::deque<std::shared_ptr<ir::Scope>>& scope_stack, std::shared_ptr<ir::Scope> scope);
                    ~ScopeFrame();

                private:
                    std::deque<std::shared_ptr<ir::Scope>>& scope_stack;
            };

            ScopeFrame in_scope(std::shared_ptr<ir::Scope> scope);
    };
}
