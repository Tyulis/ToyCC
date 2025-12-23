#pragma once

#include <string>

namespace toycc::ir {
    enum class StatementTag {
        NOP, COPY, UNARY_OP, BINARY_OP,
    };

    enum class UnaryOperator {
        ADDRESSOF, DEREFERENCE, PLUS, MINUS, BITWISE_NOT, LOGICAL_NOT,
    };

    enum class BinaryOperator {
        MUL, DIV, MOD,
        PLUS, MINUS,
        LSHIFT, RSHIFT,
        LT, LE, GE, GT,
        EQ, NE,
        BITWISE_AND, BITWISE_XOR, BITWISE_OR,
        LOGICAL_AND, LOGICAL_OR,
    };

    struct Statement {
        StatementTag tag;
    };

    namespace stmt {
        struct Copy: public Statement {
            std::string source;
            std::string destination;
        };

        struct UnaryOp : public Statement {
            UnaryOperator op;
            std::string operand;
            std::string destination;
        };

        struct BinaryOp : public Statement {
            BinaryOperator op;
            std::string left;
            std::string right;
            std::string destination;
        };

        struct ConditionalOp : public Statement {
            std::string predicate;
            std::string if_true;
            std::string if_false;
            std::string destination;
        };
    }
}
