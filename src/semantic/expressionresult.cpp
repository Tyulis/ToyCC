#include <variant>

#include "diagnostic.h"
#include "arch/datamodel.h"
#include "semantic/analyzer.h"
#include "semantic/values.h"

namespace toycc::semantic {
    static std::shared_ptr<Type> _index_type = nullptr;
    std::shared_ptr<Type> index_type() {
        if (_index_type.get() == nullptr)
            _index_type = std::make_shared<IntegerType>(".GTindex", BUILTIN_LOCATION, arch::DATAMODEL->pointer_size(), arch::DATAMODEL->pointer_alignment(), false);
        return _index_type;
    }

    // -------- ExpressionResult
    SemanticAnalyzer::ExpressionResult::ExpressionResult(LValue result, CodeLocation location) : result(result), location(location) {}
    SemanticAnalyzer::ExpressionResult::ExpressionResult(RValue result, CodeLocation location) : result(result), location(location) {}

    std::shared_ptr<Type> SemanticAnalyzer::ExpressionResult::type() const {
        return std::visit([&](auto&& val) {return val.type();}, result);
    }

    bool SemanticAnalyzer::ExpressionResult::is_lvalue() const {
        return std::holds_alternative<LValue>(result);
    }

    LValue SemanticAnalyzer::ExpressionResult::lvalue() const {
        if (!is_lvalue())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to convert an rvalue expression result to an lvalue", location);
        return std::get<LValue>(result);
    }

    RValue SemanticAnalyzer::ExpressionResult::rvalue() const {
        if (is_lvalue())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to convert an lvalue expression result to an rvalue", location);
        return std::get<RValue>(result);
    }

    Operand SemanticAnalyzer::ExpressionResult::operand() const {
        if (is_lvalue())  return lvalue();
        else              return rvalue();
    }

    RValue SemanticAnalyzer::ExpressionResult::base() const {
        if (is_lvalue())  return lvalue().base;
        else              return rvalue();
    }

    std::vector<RValue> SemanticAnalyzer::ExpressionResult::indices() const {
        if (is_lvalue())  return lvalue().indices;
        else              return {};
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::ExpressionResult::dereference(RValue index, CodeLocation location) const {
        if (is_lvalue()) {
            LValue lvalue = std::get<LValue>(result);
            lvalue.indices.push_back(index);
            return std::make_shared<ExpressionResult> (lvalue, location);
        } else {
            RValue pointer = std::get<RValue>(result);
            LValue dereferenced(pointer, location, {Constant {IntegerConstant(0), location, index_type()}});
            return std::make_shared<ExpressionResult> (dereferenced, location);
        }
    }

    // Wrap a simple declaration into an ExpressionResult
    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::make_expression(LValue lvalue, CodeLocation location) {
        return std::make_shared<ExpressionResult> (lvalue, location);
    }

    std::shared_ptr<SemanticAnalyzer::ExpressionResult> SemanticAnalyzer::make_expression(RValue rvalue, CodeLocation location) {
        return std::make_shared<ExpressionResult> (rvalue, location);
    }
}
