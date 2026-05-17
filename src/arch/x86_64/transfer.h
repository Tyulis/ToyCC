#pragma once

#include "arch/x86_64/allocation.h"
#include "arch/x86_64/execmodel.h"

namespace toycc::arch::x86_64 {
    void emit_transfers(StackFrame& frame, const flow::DependencyGraph& graph, TranslationMatch& match);

    void transfer(StackFrame& frame, std::shared_ptr<ir::Declaration> variable, Location destination);
    void transfer(StackFrame& frame, ir::Operand& operand, Location destination);
}
