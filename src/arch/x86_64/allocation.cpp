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

        for (std::shared_ptr<ir::Declaration> declaration : procedure.locals())
            if (!declaration_index.contains(declaration))
                declaration_index.insert(Allocation {declaration, LOC::NONE});
    }

    LOC StackFrame::locate(std::shared_ptr<ir::Declaration> declaration) const {
        return try_locate(declaration).value_or(LOC::NONE);
    }

    std::shared_ptr<ir::Declaration> StackFrame::content(LOC location) const {
        return try_content(location);
    }

    LOC StackFrame::available_location(Flags<LOC> allowed_locations) const {
        allowed_locations.clear(LOC::CONSTANT);
        allowed_locations.clear(LOC::NONE);

        for (const Allocation& allocation : allocations)
            if (!(allocation.location & REGISTERS))
                allowed_locations.clear(allocation.location);

        const Flags<LOC> allowed_registers = allowed_locations & REGISTERS;
        if (allowed_registers)
            return allowed_registers.first();
        else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "No register available");
    }

    void StackFrame::insert_return() {
        output.statement("popq %rbp");
        output.statement("ret");
    }

    std::string StackFrame::str() const {
        CodeOutput code;

        code.statement("pushq %rbp");
        code.statement("movq %rsp, %rbp");
        if (current_position > 0)
            code.statement(std::format("subq ${}, %rsp", align_offset(current_position, 16)));  // The stack pointer must be aligned to 16 bytes before making a call

        code << output;

        return code.str();
    }

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code) {
        output << code.str();
        return output;
    }
}
