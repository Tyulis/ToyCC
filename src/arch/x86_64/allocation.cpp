#include "config.h"
#include "debug/expression.h"
#include "diagnostic.h"
#include "gen/execmodel/x86_64/location.h"
#include "ir/declaration.h"
#include "ir/type.h"
#include "arch/x86_64/assembly.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"
#include "util/strings.h"
#include "util/alignment.hpp"

namespace toycc::arch::x86_64 {
    // RAII object to trace the locations before and after variable movement operations
    class LocationTracer {
        public:
            inline LocationTracer(StackFrame& frame, std::shared_ptr<ir::Declaration> variable, const std::string& operation) : frame(frame), variable(variable), operation(operation) {
                if (config::dev::with_location_trace)
                    locations_before = frame.locate(variable);
            }

            inline ~LocationTracer() {
                if (config::dev::with_location_trace) {
                    const std::unordered_set<Location> locations_after = frame.locate(variable);
                    frame.comment(std::format("{} {} : [{}] -> [{}]", operation, variable->name, join(locations_before, ", "), join(locations_after, ", ")));
                }
            }

        private:
            StackFrame& frame;
            const std::shared_ptr<ir::Declaration> variable;
            const std::string operation;
            std::unordered_set<Location> locations_before;
    };

    // -------- StackFrame
    static const std::unordered_set<Location> NONUNIQUE_LOCATIONS = {Location::constant, Location::memory, Location::stack};

    StackFrame::StackFrame(const flow::Procedure& procedure, debug::DebugInfo& debuginfo)
        : Parent(procedure, NONUNIQUE_LOCATIONS), name(procedure.declaration->name), debuginfo(debuginfo) {}

    // Remove all existing locations of this variable and move it elsewhere. If there is something at `location`, it is overwritten
    // Must be called after emitting the corresponding instruction
    void StackFrame::move(std::shared_ptr<ir::Declaration> declaration, Location location) {
        LocationTracer tracer(*this, declaration, "move");

        debug_clear_location(location);
        if (is_debug_variable(declaration)) {
            debug::AssemblyData expression = debug_location(declaration, location);
            debuginfo.variable(declaration)->location.move(expression, use_instruction_label());
        }

        Parent::move(declaration, location);
    }

    // Add another location for a variable. If there is already something at `location`, it is overwritten
    // Must be called after emitting the corresponding instruction
    void StackFrame::copy(std::shared_ptr<ir::Declaration> declaration, Location location) {
        LocationTracer tracer(*this, declaration, "copy");

        debug_clear_location(location);
        if (is_debug_variable(declaration)) {
            debug::AssemblyData expression = debug_location(declaration, location);
            debuginfo.variable(declaration)->location.copy(expression, use_instruction_label());
        }

        Parent::copy(declaration, location);
    }

    // Remove all locations of this variable
    // Must be called after emitting the corresponding instruction
    void StackFrame::free(std::shared_ptr<ir::Declaration> declaration) {
        LocationTracer tracer(*this, declaration, "free");
        if (is_debug_variable(declaration))
            debuginfo.variable(declaration)->location.free(use_instruction_label());

        Parent::free(declaration);
    }

    // Remove a location of a variable.
    // Must be called after emitting the corresponding instruction
    void StackFrame::free_location(std::shared_ptr<ir::Declaration> declaration, Location location) {
        LocationTracer tracer(*this, declaration, "free_location");
        if (is_debug_variable(declaration)) {
            debug::AssemblyData expression = debug_location(declaration, location);
            debuginfo.variable(declaration)->location.free_location(expression, use_instruction_label());
        }

        Parent::free_location(declaration, location);
    }

    std::unordered_set<Location> StackFrame::locate(const ir::Operand& operand) const {
        if (operand.is_dereference())
            return {Location::memory};
        else if (operand.is_constant())
            return {Location::constant};
        else if (operand.is_label())
            return {Location::constant};
        else if (operand.is_variable()) {
            if (operand.type()->storage_category() == ir::TypeCategory::FUNCTION)
                return {Location::constant};
            else
                return Parent::locate(operand.declaration());
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown operand type", operand.location);
    }

    // If one is available, return a free location among the `locations`
    std::optional<Location> StackFrame::allocate(const std::unordered_set<Location>& locations) const {
        const auto& location_index = allocations.template get<ir::location_tag>();
        for (Location location : locations) {
            if (toycc::execmodel::x86_64::BANKED_LOCATIONS.contains(location))
                continue;  // FIXME : For now, don't allocate additional space on the stack for intermediate allocations

            if (location_index.find(location) == location_index.end())
                return location;
        }
        return {};
    }

    std::unordered_set<std::shared_ptr<ir::Declaration>> StackFrame::allocated_variables() const {
        std::unordered_set<std::shared_ptr<ir::Declaration>> result;
        for (const Allocation& allocation : allocations)
            result.insert(allocation.declaration);
        return result;
    }

    // Get the canonical location of a variable, i.e where it should go in-between basic blocks
    Location StackFrame::canonical_location(std::shared_ptr<ir::Declaration> variable) const {
        if (variable->storage & ir::StorageClass::GLOBAL) {
            if (variable->type->storage_category() == ir::TypeCategory::FUNCTION)
                return Location::constant;
            else
                return Location::memory;
        } else {
            return Location::stack;
        }
    }

     bool StackFrame::is_free(Location location) const {
         if (location == Location::constant || location == Location::memory || location == Location::stack)
             return true;
         else
             return content(location).get() == nullptr;
    }

    bool StackFrame::any_free(const std::unordered_set<Location>& locations) const {
        for (Location location : locations)
            if (is_free(location))
                return true;
        return false;
    }


    // Create an intermediate declaration valid only within a code generation iteration. The variable is not inserted into the stack frame unless `offset` is called on it.
    std::shared_ptr<ir::Declaration> StackFrame::declare_intermediate(std::shared_ptr<ir::Type> type, CodeLocation code_location) {
        const std::string variable_name = std::format(".{}.SI{}", name, unique_id++);
        std::shared_ptr<ir::Declaration> declaration = std::make_shared<ir::Declaration>
                (variable_name, type, code_location,ir::StorageClass::AUTO | ir::StorageClass::TEMPORARY | ir::StorageClass::INTERMEDIATE);
        intermediates.insert(declaration);
        return declaration;
    }

    bool StackFrame::is_intermediate(std::shared_ptr<ir::Declaration> variable) const {
        return intermediates.contains(variable);
    }

    // Destroy all intermediates generated by `declare_intermediate`
    void StackFrame::flush_intermediates() {
        auto& declaration_index = allocations.get<ir::declaration_tag>();
        for (std::shared_ptr<ir::Declaration> intermediate : intermediates)
            declaration_index.erase(intermediate);
        intermediates.clear();
    }

    // Load the locations of the function parameters upon entry in the procedure
    void StackFrame::load_parameters() {
        size_t integer_index = 0;

        for (std::shared_ptr<ir::Declaration> parameter : procedure.parameters) {
            switch (parameter->type->storage_category()) {
                case ir::TypeCategory::BOOL:
                case ir::TypeCategory::INTEGER:
                    if (integer_index < INTEGER_REGISTER_ARGUMENTS.size())
                        copy(parameter, INTEGER_REGISTER_ARGUMENTS[integer_index++]);
                    else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Functions with more than {} integer parameters are not implemented", INTEGER_REGISTER_ARGUMENTS.size()), parameter->location);
                    break;

                default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Parameter loading for `{}` is not implemented", parameter->ir_code()), parameter->location);
            }
        }
    }

    // Enter a block, set up the initial locations of the block local variables
    void StackFrame::enter_block(std::shared_ptr<flow::BasicBlock> block, bool is_last) {
        current_block = block;
        is_last_block = is_last;

        if (config::dev::with_comment_trace) {
            linebreak();
            comment("-------------------------------------------------------------");
        }

        if (block == procedure.entry_block)
            load_parameters();

        for (std::shared_ptr<ir::Declaration> live : procedure.live_on_entry(block))
            if (!(block == procedure.entry_block && std::ranges::contains(procedure.parameters, live)))  // In the entry block, parameters are handled by load_parameters()
                copy(live, canonical_location(live));

        if (current_block->label.has_value() && current_block->label->type != ir::LabelType::FUNCTION)
            label(current_block->label->name);
    }

    void StackFrame::exit_block() {
        for (std::shared_ptr<ir::Declaration> variable : allocated_variables())
            free(variable);
    }

    void StackFrame::end() {
        for (std::shared_ptr<ir::Declaration> declaration : procedure.locals()) {
            if (!is_debug_variable(declaration))
                continue;

            // If the local variable has a stack location, set it as its default location
            if (stack_offsets.contains(declaration)) {
                debug::AssemblyData default_location = debug::Expression(debuginfo.format).reg_value(DWARF_REGISTER_MAPPING.at(Location::bp), stack_offset(*this, declaration)).encode();
                debuginfo.variable(declaration)->location.set_default(default_location);
            }

            debuginfo.append(debuginfo.variable(declaration));
        }
    }

    std::string StackFrame::str() const {
        CodeOutput code;

        // Generate the function symbol
        if (!(procedure.declaration->storage & ir::StorageClass::STATIC))
            code.directive(std::format(".globl {}", procedure.declaration->name));

        code.directive(std::format(".type {}, @function", procedure.declaration->name));
        code.label(procedure.declaration->name);

        if (toycc::config::dev::with_comment_trace)
            for (const auto& [variable, offset] : stack_offsets)
                code.comment(std::format("{}(%rbp) : {}", -static_cast<ssize_t>(offset + variable->type->size({})), variable->name));

        // Emit the prologue : setup the call frame and push the callee-saved registers
        code.directive(".cfi_startproc");
        code.statement("pushq %rbp");

        // The call frame starts at the stack pointer at the call site right before the `call` instruction
        // At that point, the call frame already contains the return address (8 bytes) + the saved %rbp (8 bytes) -> the CFA is at the current %rsp + 16
        code.directive(std::format(".cfi_def_cfa %rsp, {}", CALL_FRAME_BASE_OFFSET));
        code.directive(std::format(".cfi_offset %rbp, {}", -16));  // %rbp was saved from CFA - 16 to CFA - 8

        // Save the callee-saved registers
        ssize_t nof_saved_registers = 0;
        auto current_call_frame_offset = [&]() {  return CALL_FRAME_BASE_OFFSET + 8 * nof_saved_registers;  };
        auto current_save_offset = [&]() {  return -(current_call_frame_offset() + 8);  };

        for (Location reg : CALLEE_SAVED_REGISTERS) {
            if (used_locations.contains(reg)) {
                const std::string register_name = emit_operand(reg, 8);
                code.statement(std::format("pushq {}", register_name));
                code.directive(std::format(".cfi_def_cfa_register {}", current_call_frame_offset()));
                code.directive(std::format(".cfi_offset {}, {}", register_name, -current_save_offset()));
                nof_saved_registers += 1;
            }
        }

        // Set up the stack base pointer
        // Also stop using %rsp in the CFA rule since it can change at any time, instead use the fixed %rbp
        code.statement("movq %rsp, %rbp");
        code.directive(std::format(".cfi_def_cfa %rbp, {}", current_call_frame_offset()));

        // In non-leaf functions, reserve space on the stack for local variables
        // In leaf functions we can just use the 128-byte red zone to store local variables
        const bool is_leaf = procedure.is_leaf();
        const bool use_red_zone = (current_offset <= 128 && is_leaf);

        if (current_offset > 0 && !use_red_zone)
            code.statement(std::format("subq ${}, %rsp", align_offset(current_offset, 16)));  // The stack pointer must be aligned to 16 bytes before making a call

        // Then the actual code
        code << output;

        // Emit the epilogue : restore saved registers then return
        if (emit_instruction_label)
            code.label(get_instruction_label());  // Instruction label with the last index = valid until the end of the procedure (before popping the stack frame)

        for (Location reg : std::ranges::reverse_view(CALLEE_SAVED_REGISTERS)) {
            if (used_locations.contains(reg)) {
                const std::string register_name = emit_operand(reg, 8);
                code.statement(std::format("popq {}", register_name));
                code.statement(std::format(".cfi_restore {}", register_name));
            }
        }

        // Release the stack space reserved for local variables and restore the caller %rbp
        if (use_red_zone)
            code.statement("pop %rbp");  // Leaf function case : no space was explicitely allocated, just restore the caller %rbp
        else
            code.statement("leave");  // Release the reserved space (`mov %rbp, %rsp`), then restore the caller %rbp (`pop %rbp`), both are done by `leave`
        code.directive(".cfi_restore %rbp");

        // Now that %rbp is back to the caller value, go back to using %rsp to compute the CFA
        code.directive(".cfi_def_cfa %rsp, 8");

        // And return
        code.statement("ret");
        code.directive(".cfi_endproc");
        code.directive(std::format(".size {}, .-{}", name, name));
        return code.str();
    }

    std::string StackFrame::dump() const {
        std::stringstream result;
        result << "Frame " << name << " {code = {\n";
        result << indent(output.str(), true, "    ");
        result << "    }, allocations = {";
        result << dump_allocations();
        result << "}}";
        return result.str();
    }

    void StackFrame::label(std::string name) {
        output.label(name);
    }

    void StackFrame::statement(std::string code) {
        std::optional<std::string> comment;
        if (toycc::config::dev::with_comment_trace)
            comment = dump_allocations();

        if (emit_instruction_label)
            output.labeled_statement(get_instruction_label(), code, comment);
        else
            output.statement(code, comment);

        instruction_index += 1;
        emit_instruction_label = false;
    }

    void StackFrame::directive(std::string code) {
        output.directive(code);
    }

    void StackFrame::comment(std::string code) {
        output.comment(code);
    }

    void StackFrame::debug(std::string content) {
        output.debug(content);
    }

    void StackFrame::linebreak() {
        output.linebreak();
    }

    std::string StackFrame::dump_allocations() const {
        std::stringstream result;
        for (const Allocation& allocation : allocations)
            result << allocation.declaration->name << ": " << allocation.location << ", ";
        return result.str();
    }

    std::string StackFrame::get_instruction_label() const {
        return std::format(".L{}.II{}", procedure.declaration->name, instruction_index);
    }

    std::string StackFrame::use_instruction_label() {
        emit_instruction_label = true;
        return get_instruction_label();
    }

    bool StackFrame::is_debug_variable(std::shared_ptr<ir::Declaration> declaration) {
        return config::debug::enable &&
               !(declaration->storage & (ir::StorageClass::TEMPORARY | ir::StorageClass::GLOBAL)) &&
               declaration->type->category != ir::TypeCategory::FUNCTION;
    }

    debug::AssemblyData StackFrame::debug_location(std::shared_ptr<ir::Declaration> declaration, Location location) {
        if (location == Location::constant)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Constant debug locations are not implemented", declaration->location);
        else if (location == Location::memory && (declaration->storage & ir::StorageClass::GLOBAL))
            return debug::Expression(debuginfo.format).address(declaration->name).encode();
        else if (location == Location::stack)
            return debug::Expression(debuginfo.format).reg_value(DWARF_REGISTER_MAPPING.at(Location::bp), stack_offset(*this, declaration)).encode();
        else if (DWARF_REGISTER_MAPPING.contains(location))
            return debug::Expression(debuginfo.format).reg_location(DWARF_REGISTER_MAPPING.at(location)).encode();
        else
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't convert the given location to a DWARF expression", declaration->location);
    }

    // If the location is not banked and already contains a variable, update that variable’s debug location info
    // Must be called before the corresponding Parent::copy or Parent::move
    void StackFrame::debug_clear_location(Location location) {
        if (nonunique_locations.contains(location))
            return;

        std::shared_ptr<ir::Declaration> previous_content = content(location);
        if (previous_content.get() != nullptr && is_debug_variable(previous_content)) {
            debug::AssemblyData expression = debug_location(previous_content, location);
            debuginfo.variable(previous_content)->location.free_location(expression, use_instruction_label());
        }
    }

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code) {
        output << code.str();
        return output;
    }
}
