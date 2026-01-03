#include "diagnostic.h"
#include "semantic/analyzer.h"

namespace toycc::semantic {
    void SemanticAnalyzer::decode_compilation_unit(CParser::CompilationUnitContext* context) {
        if (context->translationUnit())
            decode_translation_unit(context->translationUnit());
    }

    void SemanticAnalyzer::decode_translation_unit(CParser::TranslationUnitContext* context) {
        for (CParser::ExternalDeclarationContext* declaration : context->externalDeclaration())
            decode_external_declaration(declaration);
    }

    void SemanticAnalyzer::decode_external_declaration(CParser::ExternalDeclarationContext* context) {
        if (context->KW__extension__())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "__extension__ declarations are not supported", locate(context));

        if (context->functionDefinition())
            decode_function_definition(context->functionDefinition());
        else if (context->declaration())
            decode_declaration(context->declaration());
        else if (context->asmDefinition())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Assembly definitions are not supported", locate(context));
        else if (!context->Semi())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown external declaration `{}`", context->getText()), locate(context));
    }

    void SemanticAnalyzer::decode_function_definition(CParser::FunctionDefinitionContext* context) {
        if (context->declarationList())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Parameter declarations outside of the prototype are not supported", locate(context));

        Declaration declaration;
        decode_declaration_specifiers(declaration, context->declarationSpecifiers());
        decode_declarator(declaration, context->declarator());

        if (declaration.name.empty())
            throw Diagnostic(DiagnosticLevel::ERROR, "Anonymous functions are not allowed", locate(context));

        if (declaration.type->category != TypeCategory::FUNCTION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Function definition does not contain a function declaration", locate(context));

        std::shared_ptr<Declaration> function_decl = declare(declaration);
        std::shared_ptr<Scope> function_scope = create_function_scope(function_decl);

        emit(Statement::make_function(locate(context), function_decl, function_scope));
        decode_compound_statement(context->compoundStatement(), function_scope);
    }
}
