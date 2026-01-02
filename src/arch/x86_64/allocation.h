#pragma once

#include <string>

#include "ir/flow.h"
#include "ir/allocation.h"
#include "arch/x86_64/output.h"
#include "arch/x86_64/execmodel.h"

namespace toycc::arch::x86_64 {
    // Stack frame object that automatically generates its frame push and pop code
    struct StackFrame : ir::StackFrame<LOC> {
        StackFrame(const ir::Procedure& procedure);
        std::string str() const;

        const ir::Procedure& procedure;
        CodeOutput output;
    };

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code);
}
