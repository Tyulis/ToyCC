#pragma once

#include <memory>
#include <cstddef>
#include <variant>
#include <unordered_map>

#include "ir/declaration.h"

namespace toycc::ir {
    template <typename Register>
    using Allocation = std::variant<Register, size_t>;  // Register or memory allocation relative to the stack frame / static area

    template <typename Register>
    using AllocationTable = std::unordered_map<std::shared_ptr<Declaration>, Allocation<Register>>;

    using StackFrame = std::unordered_map<std::shared_ptr<Declaration>, size_t>;
}
