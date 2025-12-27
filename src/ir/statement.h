#pragma once

#include <variant>
#include "ir/declaration.h"
#include "util/flags.hpp"


namespace toycc::ir {
    namespace stmt {
        enum class Tag {
            NOP, BLOCK, FUNCTION,
            LOAD_CONST, CONVERSION,
            BINARY_OP, CALL,
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

        enum class ConversionOperation {
            INTEGER_SIZE_UP    = 0x00001,
            INTEGER_SIZE_DOWN  = 0x00002,
            SIGNED_TO_UNSIGNED = 0x00004,
            UNSIGNED_TO_SIGNED = 0x00008,

            FLOAT_SIZE_UP      = 0x00100,
            FLOAT_SIZE_DOWN    = 0x00200,

            INT_TO_FLOAT       = 0x01000,
            FLOAT_TO_INT       = 0x02000,

            INT_TO_BOOL        = 0x10000,
            BOOL_TO_INT        = 0x20000,
            FLOAT_TO_BOOL      = 0x40000,
            BOOL_TO_FLOAT      = 0x80000,
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
            using Constant = std::variant<int, unsigned int, long, unsigned long, long long, unsigned long long, float, double, long double, char, std::string>;

            std::shared_ptr<Declaration> destination;
            Constant value;

            LoadConst(CodeLocation location, std::shared_ptr<Declaration> destination, Constant value);
            virtual std::string ir_code() const override;
        };

        // Conversion operation, also serves as a copy operation when the `operation` is empty
        struct Conversion : public Statement {
            Flags<ConversionOperation> operation;
            std::shared_ptr<Declaration> destination;
            std::shared_ptr<Declaration> source;

            Conversion(CodeLocation location, Flags<ConversionOperation> operation, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> source);
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

        struct Return : public Statement {
            std::shared_ptr<Declaration> declaration;

            Return(CodeLocation location);
            Return(CodeLocation location, std::shared_ptr<Declaration> declaration);
            virtual std::string ir_code() const override;
        };
    }
}

#include "ir/scope.h"
