#pragma once

namespace toycc::ir {
    enum class StatementTag : int {
        NOP,
    };

    struct Statement {
        StatementTag tag;
    };
}
