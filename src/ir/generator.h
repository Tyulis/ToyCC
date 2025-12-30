#pragma once

#include <memory>
#include "ParserRuleContext.h"

#include "code_location.h"
#include "ir/type_expressions.h"
#include "ir/declaration.h"
#include "ir/scope.h"
#include "ir/scopeframe.h"
#include "ir/statement.h"
#include "ir/type.h"
#include "parser/CParser.h"
#include "source_map.h"

namespace toycc::ir {
    std::shared_ptr<Scope> generate_ir(const SourceMap& source_map, CParser::CompilationUnitContext* context);

    // NOTE : This is functionally a visitor, but the ANTLR visitor would require any_casts everywhere, that would be really unwieldy
    class Generator {
    private:
        const SourceMap& source_map;
        ScopeStack scope_stack;

        size_t unique_id = 0;

        std::shared_ptr<Type> enum_underlying_type;
        std::shared_ptr<Type> boolean_type;
        std::shared_ptr<Type> character_type;

    public:
        Generator(const SourceMap& source_map, CParser::CompilationUnitContext* context);
        std::shared_ptr<Scope> get();

    private:
        // -------- External declarations -> ir/generator/external.cpp
        void decode_compilation_unit(CParser::CompilationUnitContext* context);
        void decode_translation_unit(CParser::TranslationUnitContext* context);
        void decode_external_declaration(CParser::ExternalDeclarationContext* context);
        void decode_function_definition(CParser::FunctionDefinitionContext* context);

        // -------- RAII expression result to keep track of lvalues and lingering effects -> ir/generator/expressionresult.cpp
        struct ExpressionResult {
        public:
            CodeLocation location;
            std::shared_ptr<Declaration> result;
            bool is_lvalue;
            bool is_constexpr;

            std::vector<std::shared_ptr<Declaration>> indices;
            std::vector<int> postfix_increments;

            ExpressionResult(CodeLocation location, std::shared_ptr<Declaration> result, bool is_lvalue, bool is_constexpr, Generator& generator);
            ~ExpressionResult();
            std::shared_ptr<Type> type() const;
            LValue lvalue() const;

            std::shared_ptr<Declaration> load(CodeLocation location);
            void store(std::shared_ptr<Declaration> source, CodeLocation location);

        private:
            Generator& generator;

            void apply_postfix_operations();
            void apply_pointer_postfix_operations();
            void apply_integer_postfix_operations();
        };

        std::shared_ptr<ExpressionResult> make_expression(std::shared_ptr<Declaration> declaration, bool is_lvalue, bool is_constexpr);

        // -------- Statements -> ir/generator/statements.cpp
        std::shared_ptr<Scope> decode_compound_statement(CParser::CompoundStatementContext* context, ScopeType type, std::string entry_label = {}, std::string exit_label = {});
        void decode_compound_statement(CParser::CompoundStatementContext* context, std::shared_ptr<Scope> scope);

        void decode_block_item_list(CParser::BlockItemListContext* context);
        void decode_statement(CParser::StatementContext* context, std::optional<ScopeType> scope_type = {}, std::string entry_label = {}, std::string exit_label = {});
        void decode_expression_statement(CParser::ExpressionStatementContext* context);
        void decode_selection_statement(CParser::SelectionStatementContext* context);
        void decode_if_statement(CParser::SelectionStatementContext* context);
        void decode_switch_statement(CParser::SelectionStatementContext* context);
        void decode_iteration_statement(CParser::IterationStatementContext* context);
        void decode_while_statement(CParser::IterationStatementContext* context);
        void decode_do_while_statement(CParser::IterationStatementContext* context);
        void decode_for_statement(CParser::IterationStatementContext* context);
        void decode_jump_statement(CParser::JumpStatementContext* context);
        void decode_return_statement(CParser::JumpStatementContext* context);

        void emit_conditional_jump(std::shared_ptr<ExpressionResult> predicate_expression, std::string destination_label, bool jump_if_is, CodeLocation location);

        // -------- Declarations -> ir/generator/declarations.cpp
        void decode_declaration(CParser::DeclarationContext* context);
        void decode_for_declaration(CParser::ForDeclarationContext* context);
        void decode_declaration(CParser::DeclarationSpecifiersContext* specifiers, CParser::InitDeclaratorListContext* init);
        void decode_declaration_specifiers(Declaration& declaration, CParser::DeclarationSpecifiersContext* specifiers);
        std::shared_ptr<Type> decode_specifier_qualifier_list(CParser::SpecifierQualifierListContext* context);

        Flags<StorageClass> decode_storage_class(CParser::StorageClassSpecifierContext* context);
        Flags<TypeQualifier> decode_type_qualifier_list(CParser::TypeQualifierListContext* context);
        Flags<TypeQualifier> decode_type_qualifier(CParser::TypeQualifierContext* context);
        Flags<FunctionSpecifier> decode_function_specifier(CParser::FunctionSpecifierContext* context);

        std::shared_ptr<Type> resolve_type_specifiers(std::vector<CParser::TypeSpecifierContext*> specifiers, bool is_typedef);
        TypeIdentifier decode_type_specifiers(std::vector<CParser::TypeSpecifierContext*> specifiers);
        TypeIdentifier decode_struct_or_union_specifier(CParser::StructOrUnionSpecifierContext* context);
        std::vector<Member> decode_member_declaration(CParser::MemberDeclarationContext* context);
        std::vector<Member> decode_member_declarator_list(CParser::MemberDeclaratorListContext* context, std::shared_ptr<Type> base_type);
        Member decode_struct_declarator(CParser::StructDeclaratorContext* context, std::shared_ptr<Type> base_type);

        size_t resolve_alignment_specifier(CParser::AlignmentSpecifierContext* context);
        void decode_declarator(Member& member, CParser::DeclaratorContext* context);
        void decode_direct_declarator(Member& member, CParser::DirectDeclaratorContext* context);
        void decode_function_direct_declarator(Member& spec, CParser::DirectDeclaratorContext* context);
        std::vector<Member> decode_parameter_type_list(CParser::ParameterTypeListContext* context);
        std::vector<Member> decode_parameter_list(CParser::ParameterListContext* context);
        Member decode_parameter_declaration(CParser::ParameterDeclarationContext* context);
        std::shared_ptr<Type> decode_pointer_spec(CParser::PointerContext* context, std::shared_ptr<Type> base_type);

        // -------- Expressions -> ir/generator/expressions.cpp
        std::shared_ptr<ExpressionResult> decode_initializer(CParser::InitializerContext* context);
        std::shared_ptr<ExpressionResult> decode_expression(CParser::ExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_for_expression(CParser::ForExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_expression_list(std::vector<CParser::AssignmentExpressionContext*> context);
        std::shared_ptr<ExpressionResult> decode_assignment_expression(CParser::AssignmentExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_conditional_expression(CParser::ConditionalExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_logical_or_expression(CParser::LogicalOrExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_logical_and_expression(CParser::LogicalAndExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_inclusive_or_expression(CParser::InclusiveOrExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_exclusive_or_expression(CParser::ExclusiveOrExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_and_expression(CParser::AndExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_equality_expression(CParser::EqualityExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_relational_expression(CParser::RelationalExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_shift_expression(CParser::ShiftExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_additive_expression(CParser::AdditiveExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_multiplicative_expression(CParser::MultiplicativeExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_cast_expression(CParser::CastExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_unary_expression(CParser::UnaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_unary_operation(CParser::UnaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_unary_addressof(CParser::UnaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_unary_dereference(CParser::UnaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_unary_plus(CParser::UnaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_unary_minus(CParser::UnaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_unary_bitwise_not(CParser::UnaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_unary_logical_not(CParser::UnaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_postfix_expression(CParser::PostfixExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_primary_expression(CParser::PrimaryExpressionContext* context);
        std::shared_ptr<ExpressionResult> decode_function_call(std::shared_ptr<Declaration> function, CParser::PostfixOperatorContext* call);

        std::optional<stmt::BinaryOperator> decode_assignment_operator(CParser::AssignmentOperatorContext* context);
        stmt::BinaryOperator decode_multiplicative_operator(CParser::MultiplicativeOperatorContext* context);
        stmt::BinaryOperator decode_additive_operator(CParser::AdditiveOperatorContext* context);

        std::shared_ptr<ExpressionResult> emit_binary_operation(stmt::BinaryOperator op, std::shared_ptr<ExpressionResult> left, std::shared_ptr<ExpressionResult> right, CodeLocation location);

        bool is_operator_valid(stmt::BinaryOperator op, std::shared_ptr<Type> left, std::shared_ptr<Type> right);

        // -------- Literals -> ir/generator/literals.cpp
        std::shared_ptr<Declaration> decode_constant(antlr4::tree::TerminalNode* terminal);
        std::shared_ptr<Declaration> decode_character_constant(antlr4::tree::TerminalNode* terminal);
        std::shared_ptr<Declaration> decode_floating_constant(antlr4::tree::TerminalNode* terminal);
        std::shared_ptr<Declaration> decode_integer_constant(antlr4::tree::TerminalNode* terminal);
        std::shared_ptr<Declaration> decode_decimal_constant(antlr4::tree::TerminalNode* terminal);
        std::shared_ptr<Declaration> decode_hexadecimal_constant(antlr4::tree::TerminalNode* terminal);
        std::shared_ptr<Declaration> decode_binary_constant(antlr4::tree::TerminalNode* terminal);
        std::shared_ptr<Declaration> decode_octal_constant(antlr4::tree::TerminalNode* terminal);
        std::shared_ptr<Declaration> decode_string_literal(std::vector<antlr4::tree::TerminalNode*> terminals);

        std::shared_ptr<Declaration> declare_integer_constant(size_t value, std::string suffix, CodeLocation location);

        // -------- Convenience class to make temporaries of a given type only when necessary -> ir/generator/temporarygenerator.cpp
        class TemporaryGenerator {
        public:
            TemporaryGenerator(std::shared_ptr<Type> type, CodeLocation location, Generator& generator);
            std::shared_ptr<Declaration> operator()() const;

        private:
            std::shared_ptr<Type> type;
            CodeLocation location;
            Generator& generator;
        };

        TemporaryGenerator make_temporary_generator(std::shared_ptr<Type> type, CodeLocation location);

        // -------- Conversions -> ir/generator/conversions.cpp
        enum class ConversionValidity {
            INVALID, EXPLICIT, IMPLICIT,
        };

        ConversionValidity get_conversion_validity(std::shared_ptr<Type> destination_type, std::shared_ptr<Type> source);
        std::shared_ptr<Declaration> emit_implicit_conversion(std::shared_ptr<Type> destination_type, std::shared_ptr<Declaration> source, CodeLocation location);
        std::shared_ptr<Declaration> emit_conversion(std::shared_ptr<Type> destination_type, std::shared_ptr<Declaration> source, CodeLocation location);

        // Internals
        std::shared_ptr<Declaration> emit_conversion(std::shared_ptr<Type> destination_type, std::shared_ptr<Type> effective_source_type, std::shared_ptr<Declaration> source,
                                                     CodeLocation location, TemporaryGenerator destination_generator);
        std::shared_ptr<Declaration> emit_conversion_to_bool(std::shared_ptr<BooleanType> destination_type, std::shared_ptr<Type> effective_source_type, std::shared_ptr<Declaration> source,
                                                     CodeLocation location, TemporaryGenerator destination_generator);
        std::shared_ptr<Declaration> emit_conversion_to_integer(std::shared_ptr<IntegerType> destination_type, std::shared_ptr<Type> effective_source_type, std::shared_ptr<Declaration> source,
                                                     CodeLocation location, TemporaryGenerator destination_generator);
        std::shared_ptr<Declaration> emit_conversion_to_float(std::shared_ptr<FloatingPointType> destination_type, std::shared_ptr<Type> effective_source_type, std::shared_ptr<Declaration> source,
                                                     CodeLocation location, TemporaryGenerator destination_generator);
        std::shared_ptr<Declaration> emit_conversion_to_pointer(std::shared_ptr<Type> destination_type, std::shared_ptr<Type> effective_source_type, std::shared_ptr<Declaration> source,
                                                     CodeLocation location, TemporaryGenerator destination_generator);
        std::shared_ptr<Declaration> emit_conversion_to_enum(std::shared_ptr<EnumType> destination_type, std::shared_ptr<Type> effective_source_type, std::shared_ptr<Declaration> source,
                                                     CodeLocation location, TemporaryGenerator destination_generator);

        std::shared_ptr<Declaration> emit_copy_conversion(std::shared_ptr<Declaration> source, CodeLocation location, TemporaryGenerator destination_generator,
                                                          stmt::ConversionOperation op = stmt::ConversionOperation::COPY);

        std::array<std::shared_ptr<Declaration>, 2> emit_arithmetic_conversion(std::shared_ptr<Declaration> left, std::shared_ptr<Declaration> right, CodeLocation location);
        void emit_copy(std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> source, CodeLocation location, bool initialize);

        // -------- State management -> ir/generator/state.cpp
        std::string anonymous_identifier();
        std::string anonymous_label();
        std::string anonymous_type();

        std::shared_ptr<Scope> current_scope();
        ScopeFrame in_scope(std::shared_ptr<Scope> scope);

        CodeLocation locate(antlr4::ParserRuleContext* context) const;
        CodeLocation locate(antlr4::tree::TerminalNode* context) const;

        // -------- Symbol management -> ir/generator/symbols.cpp
        void init_global_scope();
        std::shared_ptr<Type> add_builtin_type(std::string name);
        std::shared_ptr<Type> add_integer_type(std::string name, bool is_signed, size_t size, size_t alignment);
        std::shared_ptr<Type> add_floating_point_type(std::string name, size_t size, size_t alignment);

        std::shared_ptr<Scope> create_function_scope(std::shared_ptr<Declaration> declaration);

        std::shared_ptr<Declaration> declare(Declaration declaration);
        std::shared_ptr<Declaration> declare_temporary(std::shared_ptr<Type> type, CodeLocation location);

        std::optional<CodeLocation> locate_name(std::string name, bool current_scope_only = false);
        std::shared_ptr<Declaration> resolve_without_error(std::string name);
        std::shared_ptr<Declaration> resolve(std::string name, CodeLocation location);
        std::shared_ptr<Type> resolve_type_without_error(TypeIdentifier identifier);
        std::shared_ptr<Type> resolve_type(TypeIdentifier identifier, CodeLocation location);
    };
}
