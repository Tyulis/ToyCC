#pragma once

#include "ParseTree.h"
#include "parser/CParser.h"
#include "parser/CVisitor.h"

namespace toycc {
    class JSONOutputVisitor : public parser::CVisitor {
        virtual std::any visitChildren(antlr4::tree::ParseTree* node) override;
        std::string formatNode(std::string type, antlr4::tree::ParseTree* node);

        virtual std::any visitPrimaryExpression(parser::CParser::PrimaryExpressionContext *context) override;
        virtual std::any visitGenericSelection(parser::CParser::GenericSelectionContext *context) override;
        virtual std::any visitGenericAssocList(parser::CParser::GenericAssocListContext *context) override;
        virtual std::any visitGenericAssociation(parser::CParser::GenericAssociationContext *context) override;
        virtual std::any visitPostfixExpression(parser::CParser::PostfixExpressionContext *context) override;
        virtual std::any visitArgumentExpressionList(parser::CParser::ArgumentExpressionListContext *context) override;
        virtual std::any visitUnaryExpression(parser::CParser::UnaryExpressionContext *context) override;
        virtual std::any visitUnaryOperator(parser::CParser::UnaryOperatorContext *context) override;
        virtual std::any visitCastExpression(parser::CParser::CastExpressionContext *context) override;
        virtual std::any visitMultiplicativeExpression(parser::CParser::MultiplicativeExpressionContext *context) override;
        virtual std::any visitAdditiveExpression(parser::CParser::AdditiveExpressionContext *context) override;
        virtual std::any visitShiftExpression(parser::CParser::ShiftExpressionContext *context) override;
        virtual std::any visitRelationalExpression(parser::CParser::RelationalExpressionContext *context) override;
        virtual std::any visitEqualityExpression(parser::CParser::EqualityExpressionContext *context) override;
        virtual std::any visitAndExpression(parser::CParser::AndExpressionContext *context) override;
        virtual std::any visitExclusiveOrExpression(parser::CParser::ExclusiveOrExpressionContext *context) override;
        virtual std::any visitInclusiveOrExpression(parser::CParser::InclusiveOrExpressionContext *context) override;
        virtual std::any visitLogicalAndExpression(parser::CParser::LogicalAndExpressionContext *context) override;
        virtual std::any visitLogicalOrExpression(parser::CParser::LogicalOrExpressionContext *context) override;
        virtual std::any visitConditionalExpression(parser::CParser::ConditionalExpressionContext *context) override;
        virtual std::any visitAssignmentExpression(parser::CParser::AssignmentExpressionContext *context) override;
        virtual std::any visitAssignmentOperator(parser::CParser::AssignmentOperatorContext *context) override;
        virtual std::any visitExpression(parser::CParser::ExpressionContext *context) override;
        virtual std::any visitConstantExpression(parser::CParser::ConstantExpressionContext *context) override;
        virtual std::any visitDeclaration(parser::CParser::DeclarationContext *context) override;
        virtual std::any visitDeclarationSpecifiers(parser::CParser::DeclarationSpecifiersContext *context) override;
        virtual std::any visitDeclarationSpecifiers2(parser::CParser::DeclarationSpecifiers2Context *context) override;
        virtual std::any visitDeclarationSpecifier(parser::CParser::DeclarationSpecifierContext *context) override;
        virtual std::any visitInitDeclaratorList(parser::CParser::InitDeclaratorListContext *context) override;
        virtual std::any visitInitDeclarator(parser::CParser::InitDeclaratorContext *context) override;
        virtual std::any visitStorageClassSpecifier(parser::CParser::StorageClassSpecifierContext *context) override;
        virtual std::any visitTypeSpecifier(parser::CParser::TypeSpecifierContext *context) override;
        virtual std::any visitStructOrUnionSpecifier(parser::CParser::StructOrUnionSpecifierContext *context) override;
        virtual std::any visitStructOrUnion(parser::CParser::StructOrUnionContext *context) override;
        virtual std::any visitStructDeclarationList(parser::CParser::StructDeclarationListContext *context) override;
        virtual std::any visitStructDeclaration(parser::CParser::StructDeclarationContext *context) override;
        virtual std::any visitSpecifierQualifierList(parser::CParser::SpecifierQualifierListContext *context) override;
        virtual std::any visitStructDeclaratorList(parser::CParser::StructDeclaratorListContext *context) override;
        virtual std::any visitStructDeclarator(parser::CParser::StructDeclaratorContext *context) override;
        virtual std::any visitEnumSpecifier(parser::CParser::EnumSpecifierContext *context) override;
        virtual std::any visitEnumeratorList(parser::CParser::EnumeratorListContext *context) override;
        virtual std::any visitEnumerator(parser::CParser::EnumeratorContext *context) override;
        virtual std::any visitEnumerationConstant(parser::CParser::EnumerationConstantContext *context) override;
        virtual std::any visitAtomicTypeSpecifier(parser::CParser::AtomicTypeSpecifierContext *context) override;
        virtual std::any visitTypeQualifier(parser::CParser::TypeQualifierContext *context) override;
        virtual std::any visitFunctionSpecifier(parser::CParser::FunctionSpecifierContext *context) override;
        virtual std::any visitAlignmentSpecifier(parser::CParser::AlignmentSpecifierContext *context) override;
        virtual std::any visitDeclarator(parser::CParser::DeclaratorContext *context) override;
        virtual std::any visitDirectDeclarator(parser::CParser::DirectDeclaratorContext *context) override;
        virtual std::any visitVcSpecificModifier(parser::CParser::VcSpecificModifierContext *context) override;
        virtual std::any visitGccDeclaratorExtension(parser::CParser::GccDeclaratorExtensionContext *context) override;
        virtual std::any visitGccAttributeSpecifier(parser::CParser::GccAttributeSpecifierContext *context) override;
        virtual std::any visitGccAttributeList(parser::CParser::GccAttributeListContext *context) override;
        virtual std::any visitGccAttribute(parser::CParser::GccAttributeContext *context) override;
        virtual std::any visitPointer(parser::CParser::PointerContext *context) override;
        virtual std::any visitTypeQualifierList(parser::CParser::TypeQualifierListContext *context) override;
        virtual std::any visitParameterTypeList(parser::CParser::ParameterTypeListContext *context) override;
        virtual std::any visitParameterList(parser::CParser::ParameterListContext *context) override;
        virtual std::any visitParameterDeclaration(parser::CParser::ParameterDeclarationContext *context) override;
        virtual std::any visitIdentifierList(parser::CParser::IdentifierListContext *context) override;
        virtual std::any visitTypeName(parser::CParser::TypeNameContext *context) override;
        virtual std::any visitAbstractDeclarator(parser::CParser::AbstractDeclaratorContext *context) override;
        virtual std::any visitDirectAbstractDeclarator(parser::CParser::DirectAbstractDeclaratorContext *context) override;
        virtual std::any visitTypedefName(parser::CParser::TypedefNameContext *context) override;
        virtual std::any visitInitializer(parser::CParser::InitializerContext *context) override;
        virtual std::any visitInitializerList(parser::CParser::InitializerListContext *context) override;
        virtual std::any visitDesignation(parser::CParser::DesignationContext *context) override;
        virtual std::any visitDesignatorList(parser::CParser::DesignatorListContext *context) override;
        virtual std::any visitDesignator(parser::CParser::DesignatorContext *context) override;
        virtual std::any visitStaticAssertDeclaration(parser::CParser::StaticAssertDeclarationContext *context) override;
        virtual std::any visitStatement(parser::CParser::StatementContext *context) override;
        virtual std::any visitLabeledStatement(parser::CParser::LabeledStatementContext *context) override;
        virtual std::any visitCompoundStatement(parser::CParser::CompoundStatementContext *context) override;
        virtual std::any visitBlockItemList(parser::CParser::BlockItemListContext *context) override;
        virtual std::any visitBlockItem(parser::CParser::BlockItemContext *context) override;
        virtual std::any visitExpressionStatement(parser::CParser::ExpressionStatementContext *context) override;
        virtual std::any visitSelectionStatement(parser::CParser::SelectionStatementContext *context) override;
        virtual std::any visitIterationStatement(parser::CParser::IterationStatementContext *context) override;
        virtual std::any visitForCondition(parser::CParser::ForConditionContext *context) override;
        virtual std::any visitForDeclaration(parser::CParser::ForDeclarationContext *context) override;
        virtual std::any visitForExpression(parser::CParser::ForExpressionContext *context) override;
        virtual std::any visitJumpStatement(parser::CParser::JumpStatementContext *context) override;
        virtual std::any visitCompilationUnit(parser::CParser::CompilationUnitContext *context) override;
        virtual std::any visitTranslationUnit(parser::CParser::TranslationUnitContext *context) override;
        virtual std::any visitExternalDeclaration(parser::CParser::ExternalDeclarationContext *context) override;
        virtual std::any visitFunctionDefinition(parser::CParser::FunctionDefinitionContext *context) override;
        virtual std::any visitDeclarationList(parser::CParser::DeclarationListContext *context) override;
    };
}
