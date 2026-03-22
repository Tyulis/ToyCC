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
#include "semantic/values.h"
#include "gen/parser/CParser.h"
#include "source_map.h"

namespace toycc::semantic {
    using namespace toycc::ir;

    std::shared_ptr<Scope> generate_ir(const SourceMap& source_map, CParser::CompilationUnitContext* context);

    // NOTE : This is functionally a visitor, but the ANTLR visitor would require any_casts everywhere, that would be really unwieldy
    class SemanticAnalyzer {
        private:
            const SourceMap& source_map;
            ScopeStack scope_stack;

            size_t unique_id = 0;

            std::shared_ptr<Type> void_type;
            std::shared_ptr<Type> enum_underlying_type;
            std::shared_ptr<Type> boolean_type;
            std::shared_ptr<Type> character_type;
            std::shared_ptr<Type> size_type;
            std::shared_ptr<Type> literal_character_type;
            std::shared_ptr<Type> literal_integer_type;
            std::shared_ptr<Type> literal_floating_type;
            std::shared_ptr<Type> void_pointer_type;

        public:
            SemanticAnalyzer(const SourceMap& source_map, CParser::CompilationUnitContext* context);
            std::shared_ptr<Scope> get();

        private:
            // -------- External declarations -> semantic/external.cpp
            void decode_compilation_unit(CParser::CompilationUnitContext* context);
            void decode_translation_unit(CParser::TranslationUnitContext* context);
            void decode_external_declaration(CParser::ExternalDeclarationContext* context);
            void decode_function_definition(CParser::FunctionDefinitionContext* context);
            void decode_function_body(CParser::FunctionBodyContext* context, std::shared_ptr<Scope> function_scope);

            // -------- RAII expression result to keep track of lvalues and lingering effects -> semantic/expressionresult.cpp
            struct ExpressionResult {
                public:
                    std::variant<LValue, RValue> result;
                    CodeLocation location;
                    std::vector<int> postfix_increments;

                    ExpressionResult(LValue result, CodeLocation location, SemanticAnalyzer& analyzer);
                    ExpressionResult(RValue result, CodeLocation location, SemanticAnalyzer& analyzer);
                    ~ExpressionResult();

                    std::shared_ptr<Type> type() const;
                    bool is_lvalue() const;

                    LValue lvalue() const;
                    RValue rvalue() const;
                    Operand operand() const;
                    RValue base() const;
                    std::vector<RValue> indices() const;

                    std::shared_ptr<ExpressionResult> dereference(RValue index, CodeLocation location) const;

                private:
                    SemanticAnalyzer& analyzer;
                    void apply_postfix_operations();
            };

            std::shared_ptr<ExpressionResult> make_expression(LValue lvalue, CodeLocation location);
            std::shared_ptr<ExpressionResult> make_expression(RValue rvalue, CodeLocation location);

            // -------- Statements -> semantic/statements.cpp
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
            void decode_goto_statement(CParser::JumpStatementContext* context);
            void decode_return_statement(CParser::JumpStatementContext* context);
            void decode_labeled_statement(CParser::LabeledStatementContext* context);

            void emit_conditional_jump(std::shared_ptr<ExpressionResult> predicate_expression, std::string destination_label, bool jump_if_is, CodeLocation location);

            // -------- Declarations -> semantic/declarations.cpp
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
            Member decode_member_declarator(CParser::MemberDeclaratorContext* context, std::shared_ptr<Type> base_type);

            size_t resolve_alignment_specifier(CParser::AlignmentSpecifierContext* context);
            void decode_declarator(Member& member, CParser::DeclaratorContext* context);
            std::shared_ptr<Type> decode_declarator_pointer_level(CParser::DeclaratorPointerLevelContext* context, std::shared_ptr<Type> base_type);
            void decode_direct_declarator(Member& member, CParser::DirectDeclaratorContext* context);
            void decode_base_direct_declarator(Member& member, CParser::BaseDirectDeclaratorContext* context);
            void decode_direct_declarator_extension(Member& member, CParser::DirectDeclaratorExtensionContext* context);
            void decode_array_direct_declarator(Member& member, CParser::DirectDeclaratorExtensionContext* context);
            void decode_function_direct_declarator(Member& spec, CParser::DirectDeclaratorExtensionContext* context);

            void decode_abstract_declarator(Member& member, CParser::AbstractDeclaratorContext* context);
            void decode_direct_abstract_declarator(Member& member, CParser::DirectAbstractDeclaratorContext* context);
            void decode_direct_abstract_declarator_extension(Member& member, CParser::DirectAbstractDeclaratorExtensionContext* context);
            void decode_array_direct_abstract_declarator(Member& member, CParser::DirectAbstractDeclaratorExtensionContext* context);
            void decode_function_direct_abstract_declarator(Member& member, CParser::DirectAbstractDeclaratorExtensionContext* context);

            std::vector<Member> decode_parameter_type_list(CParser::ParameterTypeListContext* context);
            std::vector<Member> decode_parameter_list(CParser::ParameterListContext* context);
            std::optional<Member> decode_parameter_declaration(CParser::ParameterDeclarationContext* context);
            std::shared_ptr<Type> decode_pointer_spec(CParser::PointerContext* context, std::shared_ptr<Type> base_type);

            // -------- Expressions -> semantic/expressions.cpp
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
            std::shared_ptr<ExpressionResult> decode_prefix_expression(CParser::PrefixExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_unary_expression(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_unary_operation(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_unary_addressof(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_unary_dereference(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_unary_plus(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_unary_minus(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_unary_bitwise_not(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_unary_logical_not(CParser::UnaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_postfix_expression(CParser::PostfixExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_array_index(std::shared_ptr<ExpressionResult> array, CParser::PostfixOperatorContext* postfix);
            std::shared_ptr<ExpressionResult> decode_primary_expression(CParser::PrimaryExpressionContext* context);
            std::shared_ptr<ExpressionResult> decode_function_call(std::shared_ptr<ExpressionResult> function, CParser::PostfixOperatorContext* call);
            std::shared_ptr<ExpressionResult> decode_member_access(std::shared_ptr<ExpressionResult> structure, CParser::PostfixOperatorContext* access);
            std::shared_ptr<ExpressionResult> decode_direct_member_access(std::shared_ptr<ExpressionResult> structure, const std::string& member_name, CodeLocation location);
            std::shared_ptr<ExpressionResult> decode_indirect_member_access(std::shared_ptr<ExpressionResult> structure, const std::string& member_name, CodeLocation location);

            StatementTag decode_assignment_operator(CParser::AssignmentOperatorContext* context);
            StatementTag decode_equality_operator(CParser::EqualityOperatorContext* context);
            StatementTag decode_relational_operator(CParser::RelationalOperatorContext* context);
            StatementTag decode_multiplicative_operator(CParser::MultiplicativeOperatorContext* context);
            StatementTag decode_additive_operator(CParser::AdditiveOperatorContext* context);

            std::shared_ptr<ExpressionResult> emit_binary_operation(StatementTag op, std::shared_ptr<ExpressionResult> left, std::shared_ptr<ExpressionResult> right, CodeLocation location);
            std::shared_ptr<ExpressionResult> emit_binary_operation(StatementTag op, std::shared_ptr<ExpressionResult> left, std::shared_ptr<ExpressionResult> right, std::shared_ptr<ExpressionResult> destination, CodeLocation location);
            std::shared_ptr<SemanticAnalyzer::ExpressionResult> emit_arithmetic_binary_operation(StatementTag op, std::shared_ptr<ExpressionResult> left, std::shared_ptr<ExpressionResult> right, CodeLocation location);
            std::shared_ptr<SemanticAnalyzer::ExpressionResult> emit_pointer_binary_operation(StatementTag op, std::shared_ptr<ExpressionResult> left, std::shared_ptr<ExpressionResult> right, CodeLocation location);
            std::shared_ptr<ExpressionResult> emit_prefix_increment(std::shared_ptr<ExpressionResult> operand, StatementTag op, CodeLocation location);

            bool is_operator_valid(StatementTag op, std::shared_ptr<Type> left, std::shared_ptr<Type> right);
            std::shared_ptr<Type> operation_result_type(StatementTag op, std::shared_ptr<Type> left, std::shared_ptr<Type> right);

            // -------- Literals -> semantic/literals.cpp
            RValue decode_constant(CParser::ConstantContext* context);
            RValue decode_character_constant(antlr4::tree::TerminalNode* terminal);
            RValue decode_floating_constant(antlr4::tree::TerminalNode* terminal);
            RValue decode_integer_constant(antlr4::tree::TerminalNode* terminal);
            RValue decode_predefined_constant(CParser::PredefinedConstantContext* context);
            RValue decode_string_literal(std::vector<antlr4::tree::TerminalNode*> terminals);
            std::string decode_string_part(antlr4::tree::TerminalNode* terminal);

            // -------- Convenience class to make temporaries of a given type only when necessary -> semantic/temporarygenerator.cpp
            class TemporaryGenerator {
                public:
                    TemporaryGenerator(std::shared_ptr<Type> type, CodeLocation location, SemanticAnalyzer& analyzer);
                    std::shared_ptr<Declaration> operator()() const;

                private:
                    std::shared_ptr<Type> type;
                    CodeLocation location;
                    SemanticAnalyzer& analyzer;
            };

            TemporaryGenerator make_temporary_generator(std::shared_ptr<Type> type, CodeLocation location);

            // -------- Conversions -> semantic/conversions.cpp
            enum class ConversionValidity {
                INVALID, EXPLICIT, IMPLICIT,
            };

            ConversionValidity get_conversion_validity(std::shared_ptr<Type> destination_type, std::shared_ptr<Type> source);
            Operand emit_implicit_conversion(std::shared_ptr<Type> destination_type, Operand source, CodeLocation location);
            Operand emit_conversion(std::shared_ptr<Type> destination_type, Operand source, CodeLocation location);

            // Internals
            Operand emit_conversion(std::shared_ptr<Type> destination_type, std::shared_ptr<Type> effective_source_type, Operand source,
                                CodeLocation location, TemporaryGenerator destination_generator);
            Operand emit_conversion_to_bool(std::shared_ptr<BooleanType> destination_type, std::shared_ptr<Type> effective_source_type, Operand source,
                                CodeLocation location, TemporaryGenerator destination_generator);
            Operand emit_conversion_to_integer(std::shared_ptr<IntegerType> destination_type, std::shared_ptr<Type> effective_source_type, Operand source,
                                CodeLocation location, TemporaryGenerator destination_generator);
            Operand emit_conversion_to_float(std::shared_ptr<FloatingPointType> destination_type, std::shared_ptr<Type> effective_source_type, Operand source,
                                CodeLocation location, TemporaryGenerator destination_generator);
            Operand emit_conversion_to_pointer(std::shared_ptr<Type> destination_type, std::shared_ptr<Type> effective_source_type, Operand source,
                                CodeLocation location, TemporaryGenerator destination_generator);
            Operand emit_conversion_to_enum(std::shared_ptr<EnumType> destination_type, std::shared_ptr<Type> effective_source_type, Operand source,
                                CodeLocation location, TemporaryGenerator destination_generator);

            Operand emit_copy_conversion(std::shared_ptr<Type> destination_type, Operand source, CodeLocation location, TemporaryGenerator destination_generator, StatementTag op = StatementTag::COPY);

            std::array<Operand, 2> emit_arithmetic_conversion(Operand left, Operand right, CodeLocation location);
            void emit_copy(Operand destination, Operand source, CodeLocation location, bool initialize);

            RValue make_constant_zero(TypeCategory category, CodeLocation location);
            RValue make_constant_one(TypeCategory category, CodeLocation location);

            RValue make_constant_zero(std::shared_ptr<Type> type, CodeLocation location);
            RValue make_constant_one(std::shared_ptr<Type> type, CodeLocation location);

            // -------- State management -> semantic/state.cpp
            std::string anonymous_identifier();
            std::string anonymous_label();
            std::string anonymous_type();

            std::shared_ptr<Scope> current_scope();
            ScopeFrame in_scope(std::shared_ptr<Scope> scope);

            Statement& emit(const Statement& statement);
            Label& emit_label(LabelType type, std::string name, CodeLocation location);

            CodeLocation locate(antlr4::ParserRuleContext* context) const;
            CodeLocation locate(antlr4::tree::TerminalNode* context) const;

            // -------- Symbol management -> semantic/symbols.cpp
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
