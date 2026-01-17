#pragma once

#include "ir/declaration.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    void move_operand(StackFrame& frame, const ir::Operand& operand, Location to);
    std::string emit_operand(Location location, size_t size);
    std::string emit_operand(StackFrame& frame, const ir::Operand& operand, Location location);
}
