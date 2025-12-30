#include "diagnostic.h"
#include "ir/generator.h"

namespace toycc::ir {
    void Generator::decode_compilation_unit(CParser::CompilationUnitContext* context) {
        if (context->translationUnit())
            decode_translation_unit(context->translationUnit());
    }

    void Generator::decode_translation_unit(CParser::TranslationUnitContext* context) {
        for (CParser::ExternalDeclarationContext* declaration : context->externalDeclaration())
            decode_external_declaration(declaration);
    }

    void Generator::decode_external_declaration(CParser::ExternalDeclarationContext* context) {
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

    void Generator::decode_function_definition(CParser::FunctionDefinitionContext* context) {
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

        current_scope()->add_statement(std::make_shared<stmt::Function>(locate(context), function_scope, function_decl));
        decode_compound_statement(context->compoundStatement(), function_scope);
    }
}
