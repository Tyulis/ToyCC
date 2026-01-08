#include "arch/x86_64/codegen.h"
#include "diagnostic.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_statement(StackFrame& frame, Statement& statement) {
        move_operands(frame, statement);

        switch (statement.tag) {
            case StatementTag::RETURN:  generate_return(frame, statement);  break;
            default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Statement `{}` generation not implemented", statement.ir_code()), statement.location);
        }
    }

    void CodeGenerator::generate_return(StackFrame& frame, const Statement&) {
        // The return value is put into %rax by `move_operands`
        frame.insert_return();
    }
}
