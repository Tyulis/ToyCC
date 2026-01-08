#include <sstream>

#include "diagnostic.h"
#include "ir/statement.h"
#include "util/strings.h"

namespace toycc::ir {
    // -------- Statement
    static std::string tag_repr(StatementTag tag) {
        switch (tag) {
            case StatementTag::MARKER:          return "MARKER";
            case StatementTag::BLOCK:           return "BLOCK";
            case StatementTag::FUNCTION:        return "FUNCTION";
            case StatementTag::LOAD:            return "LOAD";
            case StatementTag::CALL:            return "CALL";
            case StatementTag::JUMP:            return "JUMP";
            case StatementTag::JUMP_IF_TRUE:    return "JUMP_IF_TRUE";
            case StatementTag::JUMP_IF_FALSE:   return "JUMP_IF_FALSE";
            case StatementTag::RETURN:          return "RETURN";
            case StatementTag::COPY:            return "COPY";
            case StatementTag::ADDRESSOF:       return "ADDRESSOF";
            case StatementTag::PLUS:            return "PLUS";
            case StatementTag::MINUS:           return "MINUS";
            case StatementTag::FLOAT_TO_FLOAT:  return "FLOAT_TO_FLOAT";
            case StatementTag::INT_TO_FLOAT:    return "INT_TO_FLOAT";
            case StatementTag::FLOAT_TO_INT:    return "FLOAT_TO_INT";
            case StatementTag::MUL:             return "MUL";
            case StatementTag::DIV:             return "DIV";
            case StatementTag::MOD:             return "MOD";
            case StatementTag::ADD:             return "ADD";
            case StatementTag::SUB:             return "SUB";
            case StatementTag::LT:              return "LT";
            case StatementTag::LE:              return "LE";
            case StatementTag::GE:              return "GE";
            case StatementTag::GT:              return "GT";
            case StatementTag::EQ:              return "EQ";
            case StatementTag::NE:              return "NE";
            case StatementTag::BITWISE_AND:     return "BITWISE_AND";
            case StatementTag::BITWISE_XOR:     return "BITWISE_XOR";
            case StatementTag::BITWISE_OR:      return "BITWISE_OR";
            case StatementTag::LSHIFT:          return "LSHIFT";
            case StatementTag::RSHIFT:          return "RSHIFT";
            case StatementTag::LOGICAL_AND:     return "LOGICAL_AND";
            case StatementTag::LOGICAL_OR:      return "LOGICAL_OR";
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown statement tag");
    }

    std::vector<Operand> Statement::operands() const {
        std::vector<Operand> operands = inputs;
        if (output.has_value())
            operands.push_back(output.value());
        return operands;
    }

    std::string Statement::ir_code() const {
        std::stringstream code;
        code << tag_repr(tag);
        if (!inputs.empty()) {
            code << " (";
            for (size_t index = 0; index < inputs.size(); index++) {
                code << inputs[index].ir_code();
                if (index != inputs.size() - 1)
                    code << ", ";
            }
            code << ")";
        }

        if (output.has_value())
            code << " -> " << output->ir_code();

        if (block.get() != nullptr)
            code << " {\n" << indent(block->ir_code(), "    ") << "\n}";

        return code.str();
    }

    Statement Statement::make_marker(CodeLocation location, std::string label) {
        return {.tag = StatementTag::MARKER, .location = location, .inputs = {}, .output = {{label, location}}, .block = {}};
    }

    Statement Statement::make_block(CodeLocation location, std::shared_ptr<Scope> block) {
        return {.tag = StatementTag::BLOCK, .location = location, .inputs = {}, .output = {}, .block = block};
    }

    Statement Statement::make_function(CodeLocation location, std::shared_ptr<Declaration> function, std::shared_ptr<Scope> block) {
        return {.tag = StatementTag::FUNCTION, .location = location, .inputs = {}, .output = function, .block = block};
    }

    Statement Statement::make_addressof(CodeLocation location, Operand object, Operand output) {
        return {.tag = StatementTag::ADDRESSOF, .location = location, .inputs = {object}, .output = output, .block = {}};
    }

    Statement Statement::make_unary_operation(CodeLocation location, StatementTag tag, Operand input, Operand output) {
        return {.tag = tag, .location = location, .inputs = {input}, .output = output, .block = {}};
    }

    Statement Statement::make_binary_operation(CodeLocation location, StatementTag tag, Operand left, Operand right, Operand output) {
        return {.tag = tag, .location = location, .inputs = {left, right}, .output = output, .block = {}};
    }

    Statement Statement::make_load(CodeLocation location, Operand input, Operand destination) {
        return {.tag = StatementTag::LOAD, .location = location, .inputs = {input}, .output = destination, .block = {}};
    }

    Statement Statement::make_call(CodeLocation location, Operand function, std::vector<Operand> arguments, Operand return_value) {
        std::vector<Operand> inputs = {function};
        inputs.append_range(arguments);
        return {.tag = StatementTag::CALL, .location = location, .inputs = inputs, .output = return_value, .block = {}};
    }

    Statement Statement::make_jump(CodeLocation location, std::string label) {
        return {.tag = StatementTag::JUMP, .location = location, .inputs = {{label, location}}, .output = {}, .block = {}};
    }

    Statement Statement::make_conditional_jump(CodeLocation location, Operand predicate, std::string label, bool jump_if_is) {
        StatementTag tag = (jump_if_is == true ? StatementTag::JUMP_IF_TRUE : StatementTag::JUMP_IF_FALSE);
        return {.tag = tag, .location = location, .inputs = {{label, location}, predicate}, .output = {}, .block = {}};
    }

    Statement Statement::make_return(CodeLocation location, std::optional<Operand> return_value) {
        std::vector<Operand> inputs;
        if (return_value.has_value())
            inputs.push_back(return_value.value());
        return {.tag = StatementTag::RETURN, .location = location, .inputs = inputs, .output = {}, .block = {}};
    }
}
