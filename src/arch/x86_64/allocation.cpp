#include "diagnostic.h"
#include "ir/type.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"
#include "util/alignment.hpp"

namespace toycc::arch::x86_64 {
    StackFrame::StackFrame(const ir::Procedure& procedure) {
        for (std::shared_ptr<ir::Declaration> declaration : procedure.locals)
            allocation[declaration] = {};

        for (std::shared_ptr<ir::Declaration> declaration : procedure.globals)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Global variables are not implemented", declaration->location);

        size_t integer_parameter_index = 0;
        for (std::shared_ptr<ir::Declaration> parameter : procedure.parameters) {
            if (parameter->type->category == ir::TypeCategory::INTEGER)
                allocation[parameter] = INTEGER_REGISTER_ARGUMENTS[integer_parameter_index++];
            else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-integer function parameters are not implemented", parameter->location);
        }
    }

    std::string StackFrame::str() const {
        CodeOutput code;

        code.statement("pushq %rbp");
        code.statement("movq %rsp, %rbp");
        if (current_position > 0)
            code.statement(std::format("subq ${}, %rsp", align_offset(current_position, 16)));  // The stack pointer must be aligned to 16 bytes before making a call

        code << output;

        code.statement("popq %rbp");
        code.statement("ret");

        return code.str();
    }

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code) {
        output << code.str();
        return output;
    }
}
