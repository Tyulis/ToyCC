#pragma once

#include <optional>
#include "ir/flow.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    std::optional<TranslationMatch> match_call(const StackFrame& frame, const ir::DependencyGraph& graph, const GroupMatch& group_match);
}
