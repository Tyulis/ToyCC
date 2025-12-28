#include <format>
#include <sstream>
#include <variant>

#include "diagnostic.h"
#include "ir/statement.h"
#include "util/strings.h"

namespace toycc::ir {
    Statement::Statement(stmt::Tag tag, CodeLocation location) : tag(tag), location(location) {}

    std::string Statement::tag_repr() const {
        switch (tag) {
            case stmt::Tag::NOP:         return "NOP";
            case stmt::Tag::BLOCK:       return "BLOCK";
            case stmt::Tag::FUNCTION:    return "FUNCTION";

            case stmt::Tag::LOAD_CONST:  return "LOAD_CONST";
            case stmt::Tag::COPY:        return "COPY";

            case stmt::Tag::BINARY_OP:   return "BINARY_OP";
            case stmt::Tag::CALL:        return "CALL";

            case stmt::Tag::RETURN:      return "RETURN";
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid statement tag");
    }
}

namespace toycc::ir::stmt {
    static std::string binary_operator_repr(BinaryOperator op) {
        switch (op) {
            case BinaryOperator::MUL:          return "*";
            case BinaryOperator::DIV:          return "/";
            case BinaryOperator::MOD:          return "%";
            case BinaryOperator::PLUS:         return "+";
            case BinaryOperator::MINUS:        return "-";
            case BinaryOperator::LSHIFT:       return "<<";
            case BinaryOperator::RSHIFT:       return ">>";
            case BinaryOperator::LT:           return "<";
            case BinaryOperator::LE:           return "<=";
            case BinaryOperator::GE:           return ">=";
            case BinaryOperator::GT:           return ">";
            case BinaryOperator::EQ:           return "==";
            case BinaryOperator::NE:           return "!=";
            case BinaryOperator::BITWISE_AND:  return "&";
            case BinaryOperator::BITWISE_XOR:  return "^";
            case BinaryOperator::BITWISE_OR:   return "|";
            case BinaryOperator::LOGICAL_AND:  return "&&";
            case BinaryOperator::LOGICAL_OR:   return "||";
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid binary operator");
    }

    static std::string conversion_operation_repr(ConversionOperation operation) {
        switch (operation) {
            case ConversionOperation::COPY:           return "COPY";
            case ConversionOperation::FLOAT_TO_FLOAT: return "FLOAT_TO_FLOAT";
            case ConversionOperation::INT_TO_FLOAT:   return "INT_TO_FLOAT";
            case ConversionOperation::BOOL_TO_FLOAT:  return "BOOL_TO_FLOAT";
            case ConversionOperation::BOOL_TO_INT:    return "BOOL_TO_INT";
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid conversion operation");
    }


    Block::Block(CodeLocation location, std::shared_ptr<Scope> scope) : Statement(Tag::BLOCK, location), scope(scope) {}
    Block::Block(Tag tag, CodeLocation location, std::shared_ptr<Scope> scope) : Statement(tag, location), scope(scope) {}
    std::string Block::ir_code() const {
        return std::format("{} {{\n{}\n}}", tag_repr(), indent(scope->ir_code(), true, "    "));
    }

    Function::Function(CodeLocation location, std::shared_ptr<Scope> scope, std::shared_ptr<Declaration> declaration) : Block(Tag::FUNCTION, location, scope), declaration(declaration) {}
    std::string Function::ir_code() const {
        return std::format("{} {} {{\n{}\n}}", tag_repr(), declaration->name, indent(scope->ir_code(), true, "    "));
    }

    LoadConst::LoadConst(CodeLocation location, std::shared_ptr<Declaration> destination, Constant value)
            : Statement(Tag::LOAD_CONST, location), destination(destination), value(value) {}
    std::string LoadConst::ir_code() const {
        std::stringstream code;
        code << tag_repr() << " " << destination->name << " = ";

        if (std::holds_alternative<char>(value)) {
            std::string character;
            character.push_back(std::get<char>(value));
            code << "'" << escape(character) << "'";
        } else if (std::holds_alternative<std::string>(value)) {
            code << "\"" << escape(std::get<std::string>(value)) << "\"";
        } else {
            std::visit([&](auto&& val) { code << val; }, value);
        }
        return code.str();
    }

    Copy::Copy(CodeLocation location, ConversionOperation operation, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> source)
        : Statement(Tag::COPY, location), operation(operation), destination(destination), source(source) {}
    std::string Copy::ir_code() const {
        return std::format("{} {} = {} {}", tag_repr(), destination->name, conversion_operation_repr(operation), source->name);
    }

    BinaryOp::BinaryOp(CodeLocation location, BinaryOperator op, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> left, std::shared_ptr<Declaration> right)
        : Statement(Tag::BINARY_OP, location), op(op), destination(destination), left(left), right(right) {}
    std::string BinaryOp::ir_code() const {
        return std::format("{} {} = {} {} {}", tag_repr(), destination->name, left->name, binary_operator_repr(op), right->name);
    }

    Call::Call(CodeLocation location, std::shared_ptr<Declaration> destination, std::shared_ptr<Declaration> function, std::vector<std::shared_ptr<Declaration>> parameters)
        : Statement(Tag::CALL, location), destination(destination), function(function), parameters(parameters) {}
    std::string Call::ir_code() const {
        std::stringstream code;
        code << tag_repr() << " " << destination->name << " = " << function->name << "(";
        for (size_t param = 0; param < parameters.size(); param++) {
            code << parameters[param]->name;
            if (param < parameters.size() - 1)
                code << ", ";
        }
        code << ")";
        return code.str();
    }

    Return::Return(CodeLocation location) : Statement(Tag::RETURN, location), declaration(nullptr) {}
    Return::Return(CodeLocation location, std::shared_ptr<Declaration> declaration) : Statement(Tag::RETURN, location), declaration(declaration) {}
    std::string Return::ir_code() const {
        return std::format("{} {}", tag_repr(), declaration->name);
    }
}
