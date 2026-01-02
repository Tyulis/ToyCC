#include "arch/x86_64/execmodel.h"

namespace toycc::arch::x86_64 {
    using toycc::ir::StatementTag;

    const std::unordered_map<ir::StatementTag, StatementIO> STATEMENT_IO = {
        {StatementTag::MARKER, {.lvalue_input = {}, .inputs = {},            .output = {}}},
        {StatementTag::RETURN, {.lvalue_input = {}, .inputs = {ACCUMULATOR}, .output = {}}},
    };
}
