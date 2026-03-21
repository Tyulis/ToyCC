#pragma once

#include "ir/declaration.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    void move_operand(StackFrame& frame, const ir::Operand& operand, Location to);

    ssize_t stack_offset(StackFrame& frame, std::shared_ptr<ir::Declaration> variable);
    std::string location_code(StackFrame& frame, std::shared_ptr<ir::Declaration> variable, Location location);
    std::string size_suffix(std::shared_ptr<ir::Declaration> variable);

    std::string emit_operand(Location location, size_t size);
    std::string emit_operand(StackFrame& frame, const ir::Operand& operand, Location location);
}
