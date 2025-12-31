#include <sstream>

#include "diagnostic.h"
#include "ir/statement.h"
#include "util/strings.h"

namespace toycc::ir {
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
            case StatementTag::FLOAT_TO_FLOAT:  return "FLOAT_TO_FLOAT";
            case StatementTag::INT_TO_FLOAT:    return "INT_TO_FLOAT";
            case StatementTag::FLOAT_TO_INT:    return "FLOAT_TO_INT";
            case StatementTag::MUL:             return "MUL";
            case StatementTag::DIV:             return "DIV";
            case StatementTag::MOD:             return "MOD";
            case StatementTag::PLUS:            return "PLUS";
            case StatementTag::MINUS:           return "MINUS";
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

    std::string Statement::ir_code() const {
        std::stringstream code;
        code << tag_repr(tag);
        if (lvalue_input.has_value())
            code << " (" << lvalue_input->ir_code() << ")";

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

        if (label.has_value())
            code << " to " << label.value();

        if (block.get() != nullptr)
            code << " {\n" << indent(block->ir_code(), "    ") << "\n}";

        return code.str();
    }

    std::shared_ptr<Statement> Statement::make_marker(CodeLocation location) {
        return std::make_shared<Statement> (Statement {.tag = StatementTag::MARKER, .location = location, .lvalue_input = {}, .inputs = {}, .output = {}, .label = {}, .block = {}});
    }

    std::shared_ptr<Statement> Statement::make_block(CodeLocation location, std::shared_ptr<Scope> block) {
        return std::make_shared<Statement> (Statement {.tag = StatementTag::BLOCK, .location = location, .lvalue_input = {}, .inputs = {}, .output = {}, .label = {}, .block = block});
    }

    std::shared_ptr<Statement> Statement::make_function(CodeLocation location, std::shared_ptr<Declaration> function, std::shared_ptr<Scope> block) {
        return std::make_shared<Statement> (Statement {.tag = StatementTag::FUNCTION, .location = location, .lvalue_input = {}, .inputs = {}, .output = function, .label = {}, .block = block});
    }

    std::shared_ptr<Statement> Statement::make_addressof(CodeLocation location, LValue object, LValue output) {
        return std::make_shared<Statement>(Statement {.tag = StatementTag::ADDRESSOF, .location = location, .lvalue_input = object, .inputs = {}, .output = output, .label = {}, .block = {}});
    }

    std::shared_ptr<Statement> Statement::make_unary_operation(CodeLocation location, StatementTag tag, RValue input, LValue output) {
        return std::make_shared<Statement> (Statement {.tag = tag, .location = location, .lvalue_input = {}, .inputs = {input}, .output = output, .label = {}, .block = {}});
    }

    std::shared_ptr<Statement> Statement::make_binary_operation(CodeLocation location, StatementTag tag, RValue left, RValue right, LValue output) {
        return std::make_shared<Statement> (Statement {.tag = tag, .location = location, .lvalue_input = {}, .inputs = {left, right}, .output = output, .label = {}, .block = {}});
    }

    std::shared_ptr<Statement> Statement::make_load(CodeLocation location, LValue input, LValue destination) {
        return std::make_shared<Statement> (Statement {.tag = StatementTag::LOAD, .location = location, .lvalue_input = input, .inputs = {}, .output = destination, .label = {}, .block = {}});
    }

    std::shared_ptr<Statement> Statement::make_call(CodeLocation location, RValue function, std::vector<RValue> arguments, LValue return_value) {
        std::vector<RValue> inputs = {function};
        inputs.append_range(arguments);
        return std::make_shared<Statement> (Statement {.tag = StatementTag::CALL, .location = location, .lvalue_input = {}, .inputs = inputs, .output = return_value, .label = {}, .block = {}});
    }

    std::shared_ptr<Statement> Statement::make_jump(CodeLocation location, std::string label) {
        return std::make_shared<Statement> (Statement {.tag = StatementTag::JUMP, .location = location, .lvalue_input = {}, .inputs = {}, .output = {}, .label = label, .block = {}});
    }

    std::shared_ptr<Statement> Statement::make_conditional_jump(CodeLocation location, RValue predicate, std::string label, bool jump_if_is) {
        StatementTag tag = (jump_if_is == true ? StatementTag::JUMP_IF_TRUE : StatementTag::JUMP_IF_FALSE);
        return std::make_shared<Statement> (Statement {.tag = tag, .location = location, .lvalue_input = {}, .inputs = {predicate}, .output = {}, .label = label, .block = {}});
    }

    std::shared_ptr<Statement> Statement::make_return(CodeLocation location, std::optional<RValue> return_value) {
        std::vector<RValue> inputs;
        if (return_value.has_value())
            inputs.push_back(return_value.value());
        return std::make_shared<Statement> (Statement {.tag = StatementTag::RETURN, .location = location, .lvalue_input = {}, .inputs = inputs, .output = {}, .label = {}, .block = {}});
    }
}
