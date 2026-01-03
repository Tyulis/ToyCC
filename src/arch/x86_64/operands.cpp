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

        for (const auto& [index, input] : std::ranges::enumerate_view(statement->inputs))
            operands.inputs.push_back(move_operand(frame, input, spec.inputs[index], statement->location));

        if (statement->output.has_value())
            clear_output(frame, operands, *statement->output, spec.output, statement->location);

        return operands;
    }

    LOC CodeGenerator::move_operand(StackFrame& frame, Operand& operand, Flags<LOC> allowed_locations, CodeLocation code_location) {
        if (operand.is_constant()) {
            if (allowed_locations & LOC::CONSTANT)
                return LOC::CONSTANT;

            LOC destination = frame.available_location(allowed_locations);
            if (destination == LOC::NONE)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "No allowed and available location for this operand", code_location);
            if (destination & MEMORY)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Temporary constant loading to memory is not supported", code_location);

            operand.value = load_constant(frame, operand.constant(), destination, code_location);
            return destination;
        } else {
            if (operand.is_dereference())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Dereference operands are not implemented", code_location);

            std::shared_ptr<Declaration> variable = operand.declaration();
            const LOC current_location = frame.locate(variable);

            if (current_location & allowed_locations)
                return current_location;

            LOC destination = frame.available_location(allowed_locations);
            if (destination == LOC::NONE)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "No allowed and available location for this operand", code_location);

            move_variable(frame, variable, destination, code_location);
            return destination;
        }
    }

    // Find an available output location, or clear one
    LOC CodeGenerator::clear_output(StackFrame&, const OperandLocation&, Operand&, Flags<LOC>, CodeLocation code_location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Instructions with outputs are not implemented", code_location);
    }


    std::string CodeGenerator::operand_ref(StackFrame& frame, const Operand& operand, CodeLocation code_location) const {
        std::stringstream code;

        if (operand.is_dereference()) {
            // At this point all dereferences are with constant offsets
            const Constant offset = operand.indices[0].constant();
            if (!offset.is_integer())
                throw Diagnostic(DiagnosticLevel::ERROR, "Pointer offsets must be integer constants", code_location);
            code << offset.integer() << "(";
        }

        if (operand.has_constant_base()) {
            const Constant& constant = operand.constant();
            if (constant.is_integer()) {
                code << "$" << constant.integer();
            } else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-integer constants are not implemented", code_location);
        } else {
            std::shared_ptr<Declaration> declaration = operand.declaration();
            code << variable_ref(frame, operand.declaration(), code_location);
        }

        if (operand.is_dereference())
            code << ")";

        return code.str();
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
