#pragma once

#include <optional>
#include "flow/block.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    std::optional<TranslationMatch> match_call(const StackFrame& frame, const flow::DependencyGraph& graph, const GroupMatch& group_match);
}
