#pragma once

#include "ir/allocation.h"

namespace toycc::arch::x86_64 {
    enum class Register {
        A, B, C, D, SI, DI, R8, R9, R10, R11, R12, R13, R14, R15,
        MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7, MM8, MM9, MM10, MM11, MM12, MM13, MM14, MM15,
    };

    using Allocation = ir::Allocation<Register>;
    using AllocationTable = ir::AllocationTable<Register>;
}
