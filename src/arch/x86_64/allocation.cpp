#include "diagnostic.h"
#include "ir/type.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"
#include "util/alignment.hpp"

namespace toycc::arch::x86_64 {
    // -------- StackFrame
    StackFrame::StackFrame(const ir::Procedure& procedure) : procedure(procedure) {
        auto& declaration_index = allocations.get<ir::declaration_tag>();

        size_t integer_parameter_index = 0;
        for (std::shared_ptr<ir::Declaration> parameter : procedure.parameters) {
            if (parameter->type->category == ir::TypeCategory::INTEGER)
                declaration_index.insert(Allocation {parameter, INTEGER_REGISTER_ARGUMENTS[integer_parameter_index++]});
            else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-integer function parameters are not implemented", parameter->location);
        }
    }

    std::unordered_set<Location> StackFrame::locate(const ir::Operand& operand) const {
        if (operand.is_dereference())
            return {Location::memory};
        else if (operand.is_constant())
            return {Location::constant};
        else if (operand.is_label())
            return {Location::constant};
        else if (operand.is_variable())
            return locate(operand.declaration());
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown operand type", operand.location);
    }

    void StackFrame::insert_return() {
        output.statement("popq %rbp");
        output.statement("ret");
    }

    std::string StackFrame::str() const {
        CodeOutput code;

        code.statement("pushq %rbp");
        code.statement("movq %rsp, %rbp");
        if (current_offset > 0)
            code.statement(std::format("subq ${}, %rsp", align_offset(current_offset, 16)));  // The stack pointer must be aligned to 16 bytes before making a call

        code << output;

        return code.str();
    }

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code) {
        output << code.str();
        return output;
    }
}
