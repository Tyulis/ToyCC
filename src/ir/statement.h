#pragma once

#include <variant>
#include "ir/declaration.h"


namespace toycc::ir {
    namespace stmt {
        enum class Tag {
            NOP, BLOCK, FUNCTION,
            LOAD_CONST, DEREF_LOAD, DEREF_STORE, COPY,
            ADDRESS_OF, UNARY_OP, BINARY_OP, CALL,
            JUMP, RETURN,
        };

        enum class UnaryOperator {
            ADDRESSOF,
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

        enum class ConversionOperation {
            COPY, FLOAT_TO_FLOAT, INT_TO_FLOAT, BOOL_TO_FLOAT, BOOL_TO_INT,
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

        struct LoadConst : public Statement {
            using Constant = std::variant<ssize_t, size_t, long double, std::string>;

            std::shared_ptr<Declaration> destination;
            Constant value;

            LoadConst(CodeLocation location, std::shared_ptr<Declaration> destination, Constant value);
            virtual std::string ir_code() const override;
        };

        struct DerefLoad : public Statement {
            std::shared_ptr<Declaration> destination;
            LValue source_pointer;

            DerefLoad(CodeLocation location, std::shared_ptr<Declaration> destination, LValue source_pointer);
            virtual std::string ir_code() const override;
        };

        struct DerefStore: public Statement {
            LValue destination_pointer;
            std::shared_ptr<Declaration> source;

            DerefStore(CodeLocation location, LValue destination_pointer, std::shared_ptr<Declaration> source);
            virtual std::string ir_code() const override;
        };

        // Copy the value, with a format conversion when requested
        struct Copy : public Statement {
            ConversionOperation operation;
            std::shared_ptr<Declaration> destination;
            std::shared_ptr<Declaration> source;

            Copy(CodeLocation location, ConversionOperation operation, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> source);
            virtual std::string ir_code() const override;
        };

        struct AddressOf : public Statement {
            std::shared_ptr<Declaration> destination;
            LValue operand;

            AddressOf(CodeLocation location, std::shared_ptr<Declaration> destination, LValue operand);
            virtual std::string ir_code() const override;
        };

        struct UnaryOp : public Statement {
            UnaryOperator op;
            std::shared_ptr<Declaration> destination;
            std::shared_ptr<Declaration> operand;

            UnaryOp(CodeLocation location, UnaryOperator op, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> operand);
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

        struct Call : public Statement {
            std::shared_ptr<Declaration> destination;
            std::shared_ptr<Declaration> function;
            std::vector<std::shared_ptr<Declaration>> parameters;

            Call(CodeLocation location, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> function, std::vector<std::shared_ptr<Declaration>> parameters);
            virtual std::string ir_code() const override;
        };

        struct Jump : public Statement {
            std::string label;
            std::shared_ptr<Declaration> predicate;
            bool jump_if_is = true;

            Jump(CodeLocation location, std::string label);
            Jump(CodeLocation location, std::string label, std::shared_ptr<Declaration> predicate, bool jump_if_is = true);
            virtual std::string ir_code() const override;
        };

        struct Return : public Statement {
            std::shared_ptr<Declaration> declaration;

            Return(CodeLocation location);
            Return(CodeLocation location, std::shared_ptr<Declaration> declaration);
            virtual std::string ir_code() const override;
        };
    }
}

#include "ir/scope.h"
