#include "diagnostic.h"
#include "ir/declaration.h"
#include "arch/x86_64/assembly.h"
#include "arch/x86_64/execmodel.h"
#include "ir/flow.h"
#include "arch/x86_64/custom_targets.h"

namespace toycc::arch::x86_64 {
    // Necessary because a generated translation model would unnecessarily transfer the operand to the stack
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

        std::optional<Location> destination = *match.statements[0].output.value().locations.begin();
        if (!destination.has_value())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "ADDRESSOF statements without output location are not implemented", output.location);

        frame.statement(std::format("leaq {}, {}", emit_operand(frame, operand, Location::stack), emit_operand(frame, output, destination.value())));
        move_operand(frame, output, destination.value());
    }

    void emit_call(StackFrame& frame, const TranslationMatch& match) {
        const ir::Statement& statement = match.group_match.statements[0]->statement();
        std::shared_ptr<ir::Declaration> function = statement.inputs[0].declaration();

        // Save caller-saved register contents
        std::deque<std::pair<Location, std::shared_ptr<ir::Declaration>>> saved_registers;
        for (Location saved_register : CALLER_SAVED_REGISTERS) {
            std::shared_ptr<ir::Declaration> to_save = frame.content(saved_register);
            if (to_save.get() != nullptr) {
                frame.statement(std::format("pushq {}", emit_operand(saved_register, 8)));
                saved_registers.emplace_front(saved_register, to_save);
            }
        }

        frame.statement(std::format("call {}", function->name));

        // Pop the saved registers
        for (const auto& [saved_register, to_restore] : saved_registers)
            frame.statement(std::format("popq {}", emit_operand(saved_register, 8)));

        if (statement.output.has_value())
            move_operand(frame, statement.output.value(), RETURN_VALUE_LOCATION);
    }

    void emit_return(StackFrame& frame, const TranslationMatch&) {
        std::optional<ir::FlowGraph::Edge> exit_edge = frame.procedure.blocks.find_edge(frame.current_block, frame.procedure.exit_block);
        if (!exit_edge.has_value())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Found a return statement in a basic block with no edge towards the exit block");

        if (exit_edge->attr == ir::FlowType::JUMP && !frame.is_last_block) {
            const std::string& exit_label = frame.procedure.exit_block->label->name;
            frame.statement(std::format("jmp {}", exit_label));
        }
    }
}
