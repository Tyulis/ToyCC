#include <format>
#include <string>
#include <sstream>

#include "json_output.h"
#include "TerminalNode.h"
#include "util/strings.h"
#include "parser/CParser.h"

namespace toycc {
    constexpr static std::string JSON_INDENT = "    ";

    std::any JSONOutputVisitor::visitChildren(antlr4::tree::ParseTree *node) {
        std::stringstream list;
        list << "[\n";

        for (size_t i = 0; i < node->children.size(); i++) {
            antlr4::tree::ParseTree* child = node->children[i];
            if (antlr4::tree::TerminalNode::is(child))
                list << JSON_INDENT << "\"" << escape(static_cast<antlr4::tree::TerminalNode*>(child)->getSymbol()->getText()) << "\"";
            else
                list << JSON_INDENT << std::any_cast<std::string>(node->children[i]->accept(this));

            if (i < node->children.size() - 1)
                list << ",\n";
        }

        list << "]";
        return list.str();
    }

    std::string JSONOutputVisitor::formatNode(std::string type, antlr4::tree::ParseTree* node) {
        return std::format("{{\"type\": \"{}\", \"children\": {}}}", type, indent(std::any_cast<std::string>(visitChildren(node)), false, JSON_INDENT));
    }

    std::any JSONOutputVisitor::visitPrimaryExpression(parser::CParser::PrimaryExpressionContext *context) {
        return formatNode("primaryExpression", context);
    }

    std::any JSONOutputVisitor::visitGenericSelection(parser::CParser::GenericSelectionContext *context) {
        return formatNode("genericSelection", context);
    }

    std::any JSONOutputVisitor::visitGenericAssocList(parser::CParser::GenericAssocListContext *context) {
        return formatNode("genericAssocList", context);
    }

    std::any JSONOutputVisitor::visitGenericAssociation(parser::CParser::GenericAssociationContext *context) {
        return formatNode("genericAssociation", context);
    }

    std::any JSONOutputVisitor::visitPostfixExpression(parser::CParser::PostfixExpressionContext *context) {
        return formatNode("postfixExpression", context);
    }

    std::any JSONOutputVisitor::visitArgumentExpressionList(parser::CParser::ArgumentExpressionListContext *context) {
        return formatNode("argumentExpressionList", context);
    }

    std::any JSONOutputVisitor::visitUnaryExpression(parser::CParser::UnaryExpressionContext *context) {
        return formatNode("unaryExpression", context);
    }

    std::any JSONOutputVisitor::visitUnaryOperator(parser::CParser::UnaryOperatorContext *context) {
        return formatNode("unaryOperator", context);
    }

    std::any JSONOutputVisitor::visitCastExpression(parser::CParser::CastExpressionContext *context) {
        return formatNode("castExpression", context);
    }

    std::any JSONOutputVisitor::visitMultiplicativeExpression(parser::CParser::MultiplicativeExpressionContext *context) {
        return formatNode("multiplicativeExpression", context);
    }

    std::any JSONOutputVisitor::visitAdditiveExpression(parser::CParser::AdditiveExpressionContext *context) {
        return formatNode("additiveExpression", context);
    }

    std::any JSONOutputVisitor::visitShiftExpression(parser::CParser::ShiftExpressionContext *context) {
        return formatNode("shiftExpression", context);
    }

    std::any JSONOutputVisitor::visitRelationalExpression(parser::CParser::RelationalExpressionContext *context) {
        return formatNode("relationalExpression", context);
    }

    std::any JSONOutputVisitor::visitEqualityExpression(parser::CParser::EqualityExpressionContext *context) {
        return formatNode("equalityExpression", context);
    }

    std::any JSONOutputVisitor::visitAndExpression(parser::CParser::AndExpressionContext *context) {
        return formatNode("andExpression", context);
    }

    std::any JSONOutputVisitor::visitExclusiveOrExpression(parser::CParser::ExclusiveOrExpressionContext *context) {
        return formatNode("exclusiveOrExpression", context);
    }

    std::any JSONOutputVisitor::visitInclusiveOrExpression(parser::CParser::InclusiveOrExpressionContext *context) {
        return formatNode("inclusiveOrExpression", context);
    }

    std::any JSONOutputVisitor::visitLogicalAndExpression(parser::CParser::LogicalAndExpressionContext *context) {
        return formatNode("logicalAndExpression", context);
    }

    std::any JSONOutputVisitor::visitLogicalOrExpression(parser::CParser::LogicalOrExpressionContext *context) {
        return formatNode("logicalOrExpression", context);
    }

    std::any JSONOutputVisitor::visitConditionalExpression(parser::CParser::ConditionalExpressionContext *context) {
        return formatNode("conditionalExpression", context);
    }

    std::any JSONOutputVisitor::visitAssignmentExpression(parser::CParser::AssignmentExpressionContext *context) {
        return formatNode("assignmentExpression", context);
    }

    std::any JSONOutputVisitor::visitAssignmentOperator(parser::CParser::AssignmentOperatorContext *context) {
        return formatNode("assignmentOperator", context);
    }

    std::any JSONOutputVisitor::visitExpression(parser::CParser::ExpressionContext *context) {
        return formatNode("expression", context);
    }

    std::any JSONOutputVisitor::visitConstantExpression(parser::CParser::ConstantExpressionContext *context) {
        return formatNode("constantExpression", context);
    }

    std::any JSONOutputVisitor::visitDeclaration(parser::CParser::DeclarationContext *context) {
        return formatNode("declaration", context);
    }

    std::any JSONOutputVisitor::visitDeclarationSpecifiers(parser::CParser::DeclarationSpecifiersContext *context) {
        return formatNode("declarationSpecifiers", context);
    }

    std::any JSONOutputVisitor::visitDeclarationSpecifiers2(parser::CParser::DeclarationSpecifiers2Context *context) {
        return formatNode("declarationSpecifiers2", context);
    }

    std::any JSONOutputVisitor::visitDeclarationSpecifier(parser::CParser::DeclarationSpecifierContext *context) {
        return formatNode("declarationSpecifier", context);
    }

    std::any JSONOutputVisitor::visitInitDeclaratorList(parser::CParser::InitDeclaratorListContext *context) {
        return formatNode("initDeclaratorList", context);
    }

    std::any JSONOutputVisitor::visitInitDeclarator(parser::CParser::InitDeclaratorContext *context) {
        return formatNode("initDeclarator", context);
    }

    std::any JSONOutputVisitor::visitStorageClassSpecifier(parser::CParser::StorageClassSpecifierContext *context) {
        return formatNode("storageClassSpecifier", context);
    }

    std::any JSONOutputVisitor::visitTypeSpecifier(parser::CParser::TypeSpecifierContext *context) {
        return formatNode("typeSpecifier", context);
    }

    std::any JSONOutputVisitor::visitStructOrUnionSpecifier(parser::CParser::StructOrUnionSpecifierContext *context) {
        return formatNode("structOrUnionSpecifier", context);
    }

    std::any JSONOutputVisitor::visitStructOrUnion(parser::CParser::StructOrUnionContext *context) {
        return formatNode("structOrUnion", context);
    }

    std::any JSONOutputVisitor::visitStructDeclarationList(parser::CParser::StructDeclarationListContext *context) {
        return formatNode("structDeclarationList", context);
    }

    std::any JSONOutputVisitor::visitStructDeclaration(parser::CParser::StructDeclarationContext *context) {
        return formatNode("structDeclaration", context);
    }

    std::any JSONOutputVisitor::visitSpecifierQualifierList(parser::CParser::SpecifierQualifierListContext *context) {
        return formatNode("specifierQualifierList", context);
    }

    std::any JSONOutputVisitor::visitStructDeclaratorList(parser::CParser::StructDeclaratorListContext *context) {
        return formatNode("structDeclaratorList", context);
    }

    std::any JSONOutputVisitor::visitStructDeclarator(parser::CParser::StructDeclaratorContext *context) {
        return formatNode("structDeclarator", context);
    }

    std::any JSONOutputVisitor::visitEnumSpecifier(parser::CParser::EnumSpecifierContext *context) {
        return formatNode("enumSpecifier", context);
    }

    std::any JSONOutputVisitor::visitEnumeratorList(parser::CParser::EnumeratorListContext *context) {
        return formatNode("enumeratorList", context);
    }

    std::any JSONOutputVisitor::visitEnumerator(parser::CParser::EnumeratorContext *context) {
        return formatNode("enumerator", context);
    }

    std::any JSONOutputVisitor::visitEnumerationConstant(parser::CParser::EnumerationConstantContext *context) {
        return formatNode("enumerationConstant", context);
    }

    std::any JSONOutputVisitor::visitAtomicTypeSpecifier(parser::CParser::AtomicTypeSpecifierContext *context) {
        return formatNode("atomicTypeSpecifier", context);
    }

    std::any JSONOutputVisitor::visitTypeQualifier(parser::CParser::TypeQualifierContext *context) {
        return formatNode("typeQualifier", context);
    }

    std::any JSONOutputVisitor::visitFunctionSpecifier(parser::CParser::FunctionSpecifierContext *context) {
        return formatNode("functionSpecifier", context);
    }

    std::any JSONOutputVisitor::visitAlignmentSpecifier(parser::CParser::AlignmentSpecifierContext *context) {
        return formatNode("alignmentSpecifier", context);
    }

    std::any JSONOutputVisitor::visitDeclarator(parser::CParser::DeclaratorContext *context) {
        return formatNode("declarator", context);
    }

    std::any JSONOutputVisitor::visitDirectDeclarator(parser::CParser::DirectDeclaratorContext *context) {
        return formatNode("directDeclarator", context);
    }

    std::any JSONOutputVisitor::visitVcSpecificModifier(parser::CParser::VcSpecificModifierContext *context) {
        return formatNode("vcSpecificModifier", context);
    }

    std::any JSONOutputVisitor::visitGccDeclaratorExtension(parser::CParser::GccDeclaratorExtensionContext *context) {
        return formatNode("gccDeclaratorExtension", context);
    }

    std::any JSONOutputVisitor::visitGccAttributeSpecifier(parser::CParser::GccAttributeSpecifierContext *context) {
        return formatNode("gccAttributeSpecifier", context);
    }

    std::any JSONOutputVisitor::visitGccAttributeList(parser::CParser::GccAttributeListContext *context) {
        return formatNode("gccAttributeList", context);
    }

    std::any JSONOutputVisitor::visitGccAttribute(parser::CParser::GccAttributeContext *context) {
        return formatNode("gccAttribute", context);
    }

    std::any JSONOutputVisitor::visitPointer(parser::CParser::PointerContext *context) {
        return formatNode("pointer", context);
    }

    std::any JSONOutputVisitor::visitTypeQualifierList(parser::CParser::TypeQualifierListContext *context) {
        return formatNode("typeQualifierList", context);
    }

    std::any JSONOutputVisitor::visitParameterTypeList(parser::CParser::ParameterTypeListContext *context) {
        return formatNode("parameterTypeList", context);
    }

    std::any JSONOutputVisitor::visitParameterList(parser::CParser::ParameterListContext *context) {
        return formatNode("parameterList", context);
    }

    std::any JSONOutputVisitor::visitParameterDeclaration(parser::CParser::ParameterDeclarationContext *context) {
        return formatNode("parameterDeclaration", context);
    }

    std::any JSONOutputVisitor::visitIdentifierList(parser::CParser::IdentifierListContext *context) {
        return formatNode("identifierList", context);
    }

    std::any JSONOutputVisitor::visitTypeName(parser::CParser::TypeNameContext *context) {
        return formatNode("typeName", context);
    }

    std::any JSONOutputVisitor::visitAbstractDeclarator(parser::CParser::AbstractDeclaratorContext *context) {
        return formatNode("abstractDeclarator", context);
    }

    std::any JSONOutputVisitor::visitDirectAbstractDeclarator(parser::CParser::DirectAbstractDeclaratorContext *context) {
        return formatNode("directAbstractDeclarator", context);
    }

    std::any JSONOutputVisitor::visitTypedefName(parser::CParser::TypedefNameContext *context) {
        return formatNode("typedefName", context);
    }

    std::any JSONOutputVisitor::visitInitializer(parser::CParser::InitializerContext *context) {
        return formatNode("initializer", context);
    }

    std::any JSONOutputVisitor::visitInitializerList(parser::CParser::InitializerListContext *context) {
        return formatNode("initializerList", context);
    }

    std::any JSONOutputVisitor::visitDesignation(parser::CParser::DesignationContext *context) {
        return formatNode("designation", context);
    }

    std::any JSONOutputVisitor::visitDesignatorList(parser::CParser::DesignatorListContext *context) {
        return formatNode("designatorList", context);
    }

    std::any JSONOutputVisitor::visitDesignator(parser::CParser::DesignatorContext *context) {
        return formatNode("designator", context);
    }

    std::any JSONOutputVisitor::visitStaticAssertDeclaration(parser::CParser::StaticAssertDeclarationContext *context) {
        return formatNode("staticAssertDeclaration", context);
    }

    std::any JSONOutputVisitor::visitStatement(parser::CParser::StatementContext *context) {
        return formatNode("statement", context);
    }

    std::any JSONOutputVisitor::visitLabeledStatement(parser::CParser::LabeledStatementContext *context) {
        return formatNode("labeledStatement", context);
    }

    std::any JSONOutputVisitor::visitCompoundStatement(parser::CParser::CompoundStatementContext *context) {
        return formatNode("compoundStatement", context);
    }

    std::any JSONOutputVisitor::visitBlockItemList(parser::CParser::BlockItemListContext *context) {
        return formatNode("blockItemList", context);
    }

    std::any JSONOutputVisitor::visitBlockItem(parser::CParser::BlockItemContext *context) {
        return formatNode("blockItem", context);
    }

    std::any JSONOutputVisitor::visitExpressionStatement(parser::CParser::ExpressionStatementContext *context) {
        return formatNode("expressionStatement", context);
    }

    std::any JSONOutputVisitor::visitSelectionStatement(parser::CParser::SelectionStatementContext *context) {
        return formatNode("selectionStatement", context);
    }

    std::any JSONOutputVisitor::visitIterationStatement(parser::CParser::IterationStatementContext *context) {
        return formatNode("iterationStatement", context);
    }

    std::any JSONOutputVisitor::visitForCondition(parser::CParser::ForConditionContext *context) {
        return formatNode("forCondition", context);
    }

    std::any JSONOutputVisitor::visitForDeclaration(parser::CParser::ForDeclarationContext *context) {
        return formatNode("forDeclaration", context);
    }

    std::any JSONOutputVisitor::visitForExpression(parser::CParser::ForExpressionContext *context) {
        return formatNode("forExpression", context);
    }

    std::any JSONOutputVisitor::visitJumpStatement(parser::CParser::JumpStatementContext *context) {
        return formatNode("jumpStatement", context);
    }

    std::any JSONOutputVisitor::visitCompilationUnit(parser::CParser::CompilationUnitContext *context) {
        return formatNode("compilationUnit", context);
    }

    std::any JSONOutputVisitor::visitTranslationUnit(parser::CParser::TranslationUnitContext *context) {
        return formatNode("translationUnit", context);
    }

    std::any JSONOutputVisitor::visitExternalDeclaration(parser::CParser::ExternalDeclarationContext *context) {
        return formatNode("externalDeclaration", context);
    }

    std::any JSONOutputVisitor::visitFunctionDefinition(parser::CParser::FunctionDefinitionContext *context) {
        return formatNode("functionDeclaration", context);
    }

    std::any JSONOutputVisitor::visitDeclarationList(parser::CParser::DeclarationListContext *context) {
        return formatNode("declarationList", context);
    }
}
