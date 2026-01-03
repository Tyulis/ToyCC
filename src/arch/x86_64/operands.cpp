#include "arch/x86_64/codegen.h"
#include "arch/x86_64/execmodel.h"
#include "diagnostic.h"

namespace toycc::arch::x86_64 {
    OperandLocation CodeGenerator::move_operands(StackFrame& frame, std::shared_ptr<Statement> statement) {
        auto it = OPERAND_SPECS.find(statement->tag);
        if (it == OPERAND_SPECS.end())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Statement `{}` has no operand spec", statement->ir_code()), statement->location);

        const OperandSpec& spec = it->second;
        OperandLocation operands;

        if (statement->lvalue_input.has_value())
            operands.lvalue_input = move_operand(frame, *statement->lvalue_input, spec.lvalue_input, statement->location);
        for (const auto& [index, rvalue] : std::ranges::enumerate_view(statement->inputs))
            operands.inputs.push_back(move_operand(frame, rvalue, spec.inputs[index], statement->location));

        if (statement->output.has_value())
            clear_output(frame, operands, *statement->output, spec.output, statement->location);

        return operands;
    }

    LOC CodeGenerator::move_operand(StackFrame&, LValue&, Flags<LOC>, CodeLocation) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Lvalue inputs are not implemented");
        return LOC::NONE;
    }

    LOC CodeGenerator::move_operand(StackFrame& frame, RValue& rvalue, Flags<LOC> allowed_locations, CodeLocation code_location) {
        if (rvalue.is_constant()) {
            if (allowed_locations & LOC::CONSTANT)
                return LOC::CONSTANT;

            LOC destination = frame.available_location(allowed_locations);
            if (destination == LOC::NONE)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "No allowed and available location for this operand", rvalue.location());
            if (destination & MEMORY)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Temporary constant loading to memory is not supported", code_location);

            rvalue.value = load_constant(frame, rvalue.constant(), destination, code_location);
            return destination;
        } else {
            std::shared_ptr<Declaration> variable = rvalue.declaration();
            const LOC current_location = frame.locate(variable);

            if (current_location & allowed_locations)
                return current_location;

            LOC destination = frame.available_location(allowed_locations);
            if (destination == LOC::NONE)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "No allowed and available location for this operand", rvalue.location());

            move_variable(frame, variable, destination, code_location);
            return destination;
        }
    }

    // Find an available output location, or clear one
    LOC CodeGenerator::clear_output(StackFrame&, const OperandLocation&, LValue&, Flags<LOC>, CodeLocation code_location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Instructions with outputs are not implemented", code_location);
    }


    std::string CodeGenerator::operand_ref(StackFrame& frame, const LValue& lvalue, CodeLocation code_location) const {
        if (!lvalue.is_dereference())
            return operand_ref(frame, lvalue.base, code_location);

        // At this point all lvalues are either an identifier, or a pointer with a constant offset
        std::stringstream code;
        const Constant offset = lvalue.indices[0].constant();
        if (!offset.is_integer())
            throw Diagnostic(DiagnosticLevel::ERROR, "Pointer offsets must be integer constants", code_location);
        code << offset.integer() << "(" << operand_ref(frame, lvalue.base, code_location) << ")";
        return code.str();
    }

    std::string CodeGenerator::operand_ref(StackFrame& frame, const RValue& rvalue, CodeLocation code_location) const {
        if (rvalue.is_constant()) {
            std::stringstream repr;
            const Constant& constant = rvalue.constant();
            if (constant.is_integer()) {
                repr << "$" << constant.integer();
                return repr.str();
            } else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-integer constants are not implemented", code_location);
        } else {
            std::shared_ptr<Declaration> declaration = rvalue.declaration();
            return variable_ref(frame, rvalue.declaration(), code_location);
        }
    }

    std::string CodeGenerator::variable_ref(StackFrame& frame, std::shared_ptr<Declaration> declaration, CodeLocation code_location) const {
        return variable_ref(frame, declaration, frame.locate(declaration), code_location);
    }

    std::string CodeGenerator::variable_ref(StackFrame& frame, std::shared_ptr<Declaration> declaration, LOC location, CodeLocation code_location) const {
        std::stringstream repr;

        if (location == LOC::NONE)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to emit code for a variable that is not allocated in this stack frame", code_location);

        if (location == LOC::STACK) {
            size_t stack_offset = frame.position(declaration);
            repr << "-" << stack_offset << "(%rbp)";
            return repr.str();
        }

        std::optional<std::string> register_name = register_ref(location, declaration->type->size(code_location));
        if (register_name.has_value())
            return register_name.value();

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Operand location is not implemented", code_location);
    }


    std::optional<std::string> CodeGenerator::register_ref(LOC location, size_t size) const {
        auto loc_it = REGISTER_NAMES.find(location);
        if (loc_it == REGISTER_NAMES.end())
            return {};

        auto size_it = loc_it->second.find(size);
        if (size_it == loc_it->second.end())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "This register isn't available for this size");

        return size_it->second;
    }
}
