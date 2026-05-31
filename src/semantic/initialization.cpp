#include "diagnostic.h"
#include "ir/type.h"
#include "semantic/analyzer.h"

namespace toycc::semantic {
    // -------- Initializers
    void SemanticAnalyzer::decode_initializer(CParser::InitializerContext* context, std::shared_ptr<Declaration> variable) {
        if (context->LeftBrace() || context->RightBrace()) {
            if (context->initializerList())
                decode_initializer_list(context->initializerList(), variable);
            else
                default_initialize(variable, locate(context));
        } else if (context->assignmentExpression()) {
            ExpressionResult initializer = decode_assignment_expression(context->assignmentExpression());
            emit_copy(variable, initializer.operand(), locate(context), true);
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown initializer `{}`", context->getText()), locate(context));
    }

    void SemanticAnalyzer::decode_initializer_list(CParser::InitializerListContext* context, std::shared_ptr<Declaration>) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Initializer lists are not implemented", locate(context));
    }

    // 6.7.10.11 : Default initialization, for empty initializers (int variable = {}) and static / thread-local storage variables
    void SemanticAnalyzer::default_initialize(std::shared_ptr<Declaration> variable, const CodeLocation& location) {
        switch (variable->type->dequalify()->category) {
            case TypeCategory::POINTER:  // Initialize with a null pointer
                emit_copy(variable, make_constant_zero(variable->type, location), location, true);
                break;

            case TypeCategory::FLOAT:
            case TypeCategory::INTEGER:
            case TypeCategory::BOOL:
            case TypeCategory::ENUM:  // Initialize with zero
                emit_copy(variable, make_constant_zero(variable->type, location), location, true);
                break;

            case TypeCategory::ARRAY:
            case TypeCategory::STRUCT:
            case TypeCategory::UNION:  // Recursively default-initialize
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Aggregate default-initialization is not implemented", location);

            case TypeCategory::LABEL:
            case TypeCategory::FUNCTION:
            case TypeCategory::VOID:
            case TypeCategory::BUILTIN:
            case TypeCategory::BITFIELD:
            case TypeCategory::QUALIFIED:
            case TypeCategory::ALIGNED:
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Variable of type `{}` can't be default-initialized", variable->type->repr()), location);
        }
    }
}
