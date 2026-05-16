#include "diagnostic.h"
#include "gen/execmodel/x86_64/location.h"
#include "ir/flow.h"
#include "ir/declaration.h"
#include "arch/x86_64/assembly.h"
#include "arch/x86_64/custom_targets.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/transfer.h"

namespace toycc::arch::x86_64 {
    // --------- ADDRESSOF : Necessary because a generated translation model would unnecessarily transfer the operand to the stack
    static void emit_addressof_variable(StackFrame& frame, const ir::Operand& operand, const ir::Operand& output, Location destination) {
        std::shared_ptr<ir::Declaration> variable = operand.declaration();
        std::unordered_set<Location> current_locations = frame.locate(operand);

        Location source_location = Location::stack;
        if (variable->storage & ir::StorageClass::GLOBAL)
            source_location = Location::memory;

        frame.statement(std::format("leaq {}, {}", emit_operand(frame, operand, source_location), emit_operand(frame, output, destination)));
        move_operand(frame, output, destination);
    }

    static void emit_addressof_dereference(StackFrame& frame, const ir::Operand& operand, const ir::Operand& output, Location destination) {
        std::shared_ptr<ir::Declaration> pointer = operand.declaration();
        const ir::Constant& index_constant = operand.indices[0].constant();
        if (index_constant.tag() != ir::Constant::INTEGER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Array indices must be integer constants", operand.location);

        const ssize_t offset = static_cast<ssize_t>(index_constant.integer());

        std::optional<Location> register_location;
        std::optional<Location> banked_location;
        for (Location location : frame.locate(pointer)) {
            if (execmodel::x86_64::BANKED_LOCATIONS.contains(location))
                banked_location = location;
            else
                register_location = location;
        }

        if (register_location.has_value()) {  // The pointer is in registers -> trivial case, simple LEA for any case
            frame.statement(std::format("leaq {}({}), {}", offset, emit_operand(frame, pointer, register_location.value()), location_code(frame, output.declaration(), destination)));
        } else if (banked_location.has_value() && banked_location.value() == Location::stack) {  // The location is on the stack, a simple LEA with a specific offset works
            frame.statement(std::format("leaq {}(%rbp), {}", stack_offset(frame, pointer) + offset, location_code(frame, output.declaration(), destination)));
        } else if (banked_location.has_value() && banked_location.value() == Location::memory) {  // The location is elsewhere in memory, this requires a LEA then possibly an explicit offset
            std::string output_operand = location_code(frame, output.declaration(), destination);
            frame.statement(std::format("leaq {}, {}", emit_operand(frame, pointer, banked_location.value()), output_operand));
            if (offset > 0)
                frame.statement(std::format("addq ${}, {}", offset, output_operand));
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The pointer has no valid location", operand.location);

        move_operand(frame, output, destination);
    }

    void emit_addressof(StackFrame& frame, const TranslationMatch& match) {
        const ir::Statement& statement = match.group_match.statements[0]->statement();
        const ir::Operand& operand = statement.inputs[0];
        const ir::Operand& output  = statement.output.value();

        if (!operand.has_variable_base())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't take the address of something other than a variable", operand.location);
        if (output.is_constant() || output.is_label())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The result of an ADDRESSOF operator can't be a constant", operand.location);

        std::optional<Location> destination = *match.statements[0].output.value().locations.begin();
        if (!destination.has_value())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "ADDRESSOF statements without output location are not implemented", output.location);

        if (operand.is_variable())
            emit_addressof_variable(frame, operand, output, destination.value());
        else if (operand.is_dereference())
            emit_addressof_dereference(frame, operand, output, destination.value());
    }

    void emit_call(StackFrame& frame, const TranslationMatch& match) {
        const ir::Statement& statement = match.group_match.statements[0]->statement();
        std::shared_ptr<ir::Declaration> function = statement.inputs[0].declaration();

        // NOTE : The transfer step should flush all variables to memory regardless,
        //        so for now no need to save caller-saved registers

        frame.statement(std::format("call {}", function->name));

        // All variables in caller-saved registers are invalidated
        for (Location reg : CALLER_SAVED_REGISTERS) {
            std::shared_ptr<ir::Declaration> variable = frame.content(reg);
            if (variable.get() != nullptr)
                frame.free_location(variable, reg);
        }

        // Set the return value location
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
