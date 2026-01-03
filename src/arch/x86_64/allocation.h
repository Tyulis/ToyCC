#pragma once

#include <memory>
#include <string>

#include "ir/flow.h"
#include "ir/allocation.h"
#include "arch/x86_64/output.h"
#include "arch/x86_64/execmodel.h"

namespace toycc::arch::x86_64 {
    using Allocation = ir::Allocation<LOC>;

    // Stack frame object that automatically generates its frame push and pop code
    struct StackFrame : ir::StackFrame<LOC> {
        StackFrame(const ir::Procedure& procedure);

        LOC available_location(Flags<LOC> allowed_locations) const;
        LOC locate(std::shared_ptr<ir::Declaration> declaration) const;
        std::shared_ptr<ir::Declaration> content(LOC location) const;

        void insert_return();
        std::string str() const;

        const ir::Procedure& procedure;
        CodeOutput output;
    };

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code);
}
