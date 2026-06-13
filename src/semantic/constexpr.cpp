#include "diagnostic.h"
#include "semantic/analyzer.h"

namespace toycc::semantic {
    // Decode the expression into a block scope, then evaluate it
    Constant SemanticAnalyzer::evaluate_constant_expression(CParser::ConstantExpressionContext* context) {
        std::shared_ptr<Scope> scope = std::make_shared<Scope>(ScopeType::BLOCK, current_scope()->function);

        {
            ScopeFrame frame = in_scope(scope);
            decode_conditional_expression(context->conditionalExpression());
        }

        return evaluate_constant_scope(scope, locate(context));
    }

    Constant SemanticAnalyzer::evaluate_constant_scope(std::shared_ptr<Scope>, const CodeLocation& location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Constant expression evaluation is not implemented", location);
    }
}
