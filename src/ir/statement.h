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
    struct Statement {
        StatementTag tag;
        CodeLocation location;
        std::vector<Operand> inputs;
        std::optional<Operand> output;
        std::optional<std::string> label;
        std::shared_ptr<Scope> block;

        std::vector<Operand> operands() const;
        std::string ir_code() const;

        static std::shared_ptr<Statement> make_marker(CodeLocation location);
        static std::shared_ptr<Statement> make_block(CodeLocation location, std::shared_ptr<Scope> block);
        static std::shared_ptr<Statement> make_function(CodeLocation location, std::shared_ptr<Declaration> function, std::shared_ptr<Scope> block);
        static std::shared_ptr<Statement> make_addressof(CodeLocation location, Operand object, Operand output);
        static std::shared_ptr<Statement> make_unary_operation(CodeLocation location, StatementTag tag, Operand input, Operand output);
        static std::shared_ptr<Statement> make_binary_operation(CodeLocation location, StatementTag tag, Operand left, Operand right, Operand output);
        static std::shared_ptr<Statement> make_load(CodeLocation location, Operand source, Operand destination);
        static std::shared_ptr<Statement> make_call(CodeLocation location, Operand function, std::vector<Operand> arguments, Operand return_value);
        static std::shared_ptr<Statement> make_jump(CodeLocation location, std::string label);
        static std::shared_ptr<Statement> make_conditional_jump(CodeLocation location, Operand predicate, std::string label, bool jump_if_is = true);
        static std::shared_ptr<Statement> make_return(CodeLocation location, std::optional<Operand> return_value = {});
    };
}
