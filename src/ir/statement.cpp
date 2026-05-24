#include <sstream>

#include "ir/statement.h"
#include "arch/datamodel.h"
#include "util/strings.h"

namespace toycc::ir {
    // -------- Statement
    std::ostream& operator<< (std::ostream& stream, StatementTag tag) {
        switch (tag) {
            case StatementTag::MARKER:             return stream << "MARKER";
            case StatementTag::BLOCK:              return stream << "BLOCK";
            case StatementTag::FUNCTION:           return stream << "FUNCTION";
            case StatementTag::CALL:               return stream << "CALL";
            case StatementTag::JUMP:               return stream << "JUMP";
            case StatementTag::JUMP_IF_TRUE:       return stream << "JUMP_IF_TRUE";
            case StatementTag::JUMP_IF_FALSE:      return stream << "JUMP_IF_FALSE";
            case StatementTag::RETURN:             return stream << "RETURN";
            case StatementTag::RETURN_VAL:         return stream << "RETURN_VAL";
            case StatementTag::COPY:               return stream << "COPY";
            case StatementTag::NOT:                return stream << "NOT";
            case StatementTag::COMPLEMENT:         return stream << "COMPLEMENT";
            case StatementTag::ADDRESSOF:          return stream << "ADDRESSOF";
            case StatementTag::NEGATE:             return stream << "NEGATE";
            case StatementTag::FLOAT_TO_FLOAT:     return stream << "FLOAT_TO_FLOAT";
            case StatementTag::INT_TO_FLOAT:       return stream << "INT_TO_FLOAT";
            case StatementTag::FLOAT_TO_INT:       return stream << "FLOAT_TO_INT";
            case StatementTag::SIGN_EXTEND:        return stream << "SIGN_EXTEND";
            case StatementTag::ZERO_EXTEND:        return stream << "ZERO_EXTEND";
            case StatementTag::NARROW:             return stream << "NARROW";
            case StatementTag::MUL:                return stream << "MUL";
            case StatementTag::DIV:                return stream << "DIV";
            case StatementTag::MOD:                return stream << "MOD";
            case StatementTag::ADD:                return stream << "ADD";
            case StatementTag::SUB:                return stream << "SUB";
            case StatementTag::LT:                 return stream << "LT";
            case StatementTag::LE:                 return stream << "LE";
            case StatementTag::GE:                 return stream << "GE";
            case StatementTag::GT:                 return stream << "GT";
            case StatementTag::EQ:                 return stream << "EQ";
            case StatementTag::NE:                 return stream << "NE";
            case StatementTag::BITWISE_AND:        return stream << "BITWISE_AND";
            case StatementTag::BITWISE_XOR:        return stream << "BITWISE_XOR";
            case StatementTag::BITWISE_OR:         return stream << "BITWISE_OR";
            case StatementTag::LSHIFT:             return stream << "LSHIFT";
            case StatementTag::LOGICAL_RSHIFT:     return stream << "LOGICAL_RSHIFT";
            case StatementTag::ARITHMETIC_RSHIFT:  return stream << "ARITHMETIC_RSHIFT";
        }
        __builtin_unreachable();
    }

    std::vector<Operand> Statement::operands() const {
        std::vector<Operand> operands = inputs;
        if (output.has_value())
            operands.push_back(output.value());
        return operands;
    }

    std::string Statement::ir_code() const {
        std::stringstream code;
        code << tag;
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
        const Constant output = {PointerConstant {label, 0}, location, arch::DATAMODEL->label_type};
        return {.tag = StatementTag::MARKER, .location = location, .inputs = {}, .output = output, .block = {}};
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

    Statement Statement::make_call(CodeLocation location, Operand function, std::vector<Operand> arguments, std::optional<Operand> return_value) {
        std::vector<Operand> inputs = {function};
        inputs.append_range(arguments);
        return {.tag = StatementTag::CALL, .location = location, .inputs = inputs, .output = return_value, .block = {}};
    }

    Statement Statement::make_jump(CodeLocation location, std::string label) {
        const Constant input = {PointerConstant {label, 0}, location, arch::DATAMODEL->label_type};
        return {.tag = StatementTag::JUMP, .location = location, .inputs = {input}, .output = {}, .block = {}};
    }

    Statement Statement::make_conditional_jump(CodeLocation location, Operand predicate, std::string label, bool jump_if_is) {
        const Constant input = {PointerConstant {label, 0}, location, arch::DATAMODEL->label_type};
        StatementTag tag = (jump_if_is == true ? StatementTag::JUMP_IF_TRUE : StatementTag::JUMP_IF_FALSE);
        return {.tag = tag, .location = location, .inputs = {input, predicate}, .output = {}, .block = {}};
    }

    Statement Statement::make_return(CodeLocation location) {
        return {.tag = StatementTag::RETURN, .location = location, .inputs = {}, .output = {}, .block = {}};
    }

    Statement Statement::make_return(CodeLocation location, Operand return_value) {
        return {.tag = StatementTag::RETURN_VAL, .location = location, .inputs = {return_value}, .output = {}, .block = {}};
    }
}
