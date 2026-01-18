#include "diagnostic.h"
#include "ir/declaration.h"
#include "arch/x86_64/assembly.h"
#include "arch/x86_64/custom_targets.h"

namespace toycc::arch::x86_64 {
    void emit_addressof(StackFrame& frame, const TranslationMatch& match) {
        const ir::Statement& statement = match.group_match.statements[0]->statement();
        const ir::Operand& operand = statement.inputs[0];
        const ir::Operand& output  = statement.output.value();

        if (!operand.is_variable())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't take the address of something other than a variable", operand.location);
        if (output.is_constant() || output.is_label())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The result of an ADDRESSOF operator can't be a constant", operand.location);

        std::shared_ptr<ir::Declaration> variable = operand.declaration();
        if (variable->storage & ir::StorageClass::GLOBAL)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Taking the address of a global variable is not implemented", operand.location);

        std::optional<Location> destination = match.statements[0].output.value().location;
        if (!destination.has_value())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "ADDRESSOF statements without output location are not implemented", output.location);

        const std::string output_code = emit_operand(frame, output, destination.value());
        frame.output.statement(std::format("movq %rsp, {}", output_code));
        frame.output.statement(std::format("addq ${}, {}", frame.offset(variable), output_code));
        move_operand(frame, output, destination.value());
    }
}
