#pragma once

#include <memory>
#include <vector>
#include <variant>

#include "code_location.h"
#include "ir/type.h"
#include "ir/declaration.h"

namespace toycc::semantic {
    using namespace toycc::ir;

    // -------- Structures to represent expression results -> ir/generator/values.cpp
    struct RValue {
        std::variant<std::shared_ptr<Declaration>, Constant> value;

        RValue(std::shared_ptr<Declaration> declaration);
        RValue(Constant value);

        operator Operand() const;

        bool is_constant() const;
        CodeLocation location() const;
        std::shared_ptr<Type> type() const;
        Constant constant() const;
        std::shared_ptr<Declaration> declaration() const;
        std::optional<size_t> as_index() const;

        std::string ir_code() const;
    };

    struct LValue {
        RValue base;
        CodeLocation location;
        std::vector<RValue> indices;

        LValue(std::shared_ptr<Declaration> declaration);
        LValue(RValue base, CodeLocation location, std::vector<RValue> indices = {});

        operator Operand() const;

        std::shared_ptr<Type> type() const;
        std::string ir_code() const;
    };

    // -------- Expression result to keep track of the lvalue/rvalue distinction during semantic analysis while preserving an unified interface -> semantic/expressionresult.cpp
    struct ExpressionResult {
    public:
        ExpressionResult(LValue result, CodeLocation location);
        ExpressionResult(RValue result, CodeLocation location);

        std::shared_ptr<Type> type() const;
        bool is_lvalue() const;

        LValue lvalue() const;
        RValue rvalue() const;
        Operand operand() const;
        RValue base() const;
        std::vector<RValue> indices() const;

        ExpressionResult dereference(RValue index, CodeLocation location) const;

    private:
        std::variant<LValue, RValue> result;
        CodeLocation location;
    };
}
