#pragma once

#include "ir/scope.h"
#include "ir/declaration.h"

namespace toycc::ir {
    enum class StatementTag {
        MARKER,  // No-op statement used to mark label positions without needing to update all positions when labels are moved around
        BLOCK, FUNCTION,
        CALL,
        JUMP, JUMP_IF_TRUE, JUMP_IF_FALSE, RETURN, RETURN_VAL,

        // Unary operators
        COPY, PLUS, MINUS, ADDRESSOF, FLOAT_TO_FLOAT, INT_TO_FLOAT, FLOAT_TO_INT, SIGN_EXTEND, ZERO_EXTEND,

        // Binary operators
        MUL, DIV, MOD,
        ADD, SUB,
        LT, LE, GE, GT, EQ, NE,
        BITWISE_AND, BITWISE_XOR, BITWISE_OR, LSHIFT, ARITHMETIC_RSHIFT, LOGICAL_RSHIFT,
        LOGICAL_AND, LOGICAL_OR,

        NARROW = COPY,  // For now, with only x86_64, it's the same
    };

    struct Scope;
    struct Statement {
        StatementTag tag;
        CodeLocation location;
        std::vector<Operand> inputs;
        std::optional<Operand> output;
        std::shared_ptr<Scope> block;

        std::vector<Operand> operands() const;
        std::string ir_code() const;

        static Statement make_marker(CodeLocation location, std::string label);
        static Statement make_block(CodeLocation location, std::shared_ptr<Scope> block);
        static Statement make_function(CodeLocation location, std::shared_ptr<Declaration> function, std::shared_ptr<Scope> block);
        static Statement make_addressof(CodeLocation location, Operand object, Operand output);
        static Statement make_unary_operation(CodeLocation location, StatementTag tag, Operand input, Operand output);
        static Statement make_binary_operation(CodeLocation location, StatementTag tag, Operand left, Operand right, Operand output);
        static Statement make_call(CodeLocation location, Operand function, std::vector<Operand> arguments, std::optional<Operand> return_value = {});
        static Statement make_jump(CodeLocation location, std::string label);
        static Statement make_conditional_jump(CodeLocation location, Operand predicate, std::string label, bool jump_if_is = true);
        static Statement make_return(CodeLocation location);
        static Statement make_return(CodeLocation location, Operand return_value);
    };
}
