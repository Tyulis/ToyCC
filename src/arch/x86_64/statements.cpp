#include "arch/x86_64/codegen.h"
#include "diagnostic.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_statement(StackFrame& frame, std::shared_ptr<Statement> statement) {
        move_operands(frame, statement);

        switch (statement->tag) {
            case StatementTag::MARKER:  generate_marker(frame, statement);  break;
            case StatementTag::RETURN:  generate_return(frame, statement);  break;
            default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Statement `{}` generation not implemented", statement->ir_code()), statement->location);
        }
    }

    void CodeGenerator::generate_marker(StackFrame& frame, std::shared_ptr<Statement> statement) {
        const auto& label_index = frame.procedure.labels.get<ir::marker_index_tag>();
        for (auto [it, end] = label_index.equal_range(statement); it != end; it++) {
            std::shared_ptr<Label> label = *it;
            if (label->name != frame.procedure.declaration->name)
                frame.output.label(label->name);
        }
    }

    void CodeGenerator::generate_return(StackFrame& frame, std::shared_ptr<Statement>) {
        // The return value is put into %rax by `move_operands`
        frame.insert_return();
    }
}
