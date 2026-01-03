#pragma once

#include "ir/scope.h"
#include "ir/declaration.h"

namespace toycc::ir {
    enum class StatementTag {
        MARKER,  // No-op statement used to mark label positions without needing to update all positions when labels are moved around
        BLOCK, FUNCTION,
        LOAD, CALL,
        JUMP, JUMP_IF_TRUE, JUMP_IF_FALSE, RETURN,

        // Unary operators
        COPY, PLUS, MINUS, ADDRESSOF, FLOAT_TO_FLOAT, INT_TO_FLOAT, FLOAT_TO_INT,

        // Binary operators
        MUL, DIV, MOD,
        ADD, SUB,
        LT, LE, GE, GT, EQ, NE,
        BITWISE_AND, BITWISE_XOR, BITWISE_OR, LSHIFT, RSHIFT,
        LOGICAL_AND, LOGICAL_OR,
    };

    struct Scope;

    // Semantic statement object
    struct Statement {
        public:
            StatementTag tag;
            CodeLocation location;
            std::optional<LValue> lvalue_input;
            std::vector<RValue> inputs;
            std::optional<LValue> output;
            std::optional<std::string> label;
            std::shared_ptr<Scope> block;

            std::string ir_code() const;

            static std::shared_ptr<Statement> make_marker(CodeLocation location);
            static std::shared_ptr<Statement> make_block(CodeLocation location, std::shared_ptr<Scope> block);
            static std::shared_ptr<Statement> make_function(CodeLocation location, std::shared_ptr<Declaration> function, std::shared_ptr<Scope> block);
            static std::shared_ptr<Statement> make_addressof(CodeLocation location, LValue object, LValue output);
            static std::shared_ptr<Statement> make_unary_operation(CodeLocation location, StatementTag tag, RValue input, LValue output);
            static std::shared_ptr<Statement> make_binary_operation(CodeLocation location, StatementTag tag, RValue left, RValue right, LValue output);
            static std::shared_ptr<Statement> make_load(CodeLocation location, LValue source, LValue destination);
            static std::shared_ptr<Statement> make_call(CodeLocation location, RValue function, std::vector<RValue> arguments, LValue return_value);
            static std::shared_ptr<Statement> make_jump(CodeLocation location, std::string label);
            static std::shared_ptr<Statement> make_conditional_jump(CodeLocation location, RValue predicate, std::string label, bool jump_if_is = true);
            static std::shared_ptr<Statement> make_return(CodeLocation location, std::optional<RValue> return_value = {});
    };
}
