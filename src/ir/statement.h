#pragma once

#include <variant>
#include "ir/declaration.h"


namespace toycc::ir {
    namespace stmt {
        enum class Tag {
            NOP, BLOCK, FUNCTION,
            LOAD_CONST, COPY,
            BINARY_OP,
            RETURN,
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
    }

    struct Statement {
        stmt::Tag tag;
        CodeLocation location;

        Statement(stmt::Tag tag, CodeLocation location);
        std::string tag_repr() const;
        virtual std::string ir_code() const = 0;
    };

    struct Scope;  // Forward declaration for compound statements
    namespace stmt {
        struct Block : public Statement {
            std::shared_ptr<Scope> scope;

            Block(CodeLocation location, std::shared_ptr<Scope> scope);
            Block(Tag tag, CodeLocation location, std::shared_ptr<Scope> scope);
            virtual std::string ir_code() const override;
        };

        struct Function : public Block {
            std::shared_ptr<Declaration> declaration;

            Function(CodeLocation location, std::shared_ptr<Scope> scope, std::shared_ptr<Declaration> declaration);
            virtual std::string ir_code() const override;
        };

        struct Return : public Statement {
            std::shared_ptr<Declaration> declaration;

            Return(CodeLocation location);
            Return(CodeLocation location, std::shared_ptr<Declaration> declaration);
            virtual std::string ir_code() const override;
        };

        struct LoadConst : public Statement {
            using Constant = std::variant<int, unsigned int, long, unsigned long, long long, unsigned long long, float, double, long double, char, std::string>;

            std::shared_ptr<Declaration> destination;
            Constant value;

            LoadConst(CodeLocation location, std::shared_ptr<Declaration> destination, Constant value);
            virtual std::string ir_code() const override;
        };

        struct Copy : public Statement {
            std::shared_ptr<Declaration> destination;
            std::shared_ptr<Declaration> source;

            Copy(CodeLocation location, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> source);
            virtual std::string ir_code() const override;
        };

        struct BinaryOp : public Statement {
            BinaryOperator op;
            std::shared_ptr<Declaration> destination;
            std::shared_ptr<Declaration> left;
            std::shared_ptr<Declaration> right;

            BinaryOp(CodeLocation location, BinaryOperator op, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> left, std::shared_ptr<Declaration> right);
            virtual std::string ir_code() const override;
        };
    }
}

#include "ir/scope.h"
