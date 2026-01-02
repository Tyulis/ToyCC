#pragma once

#include <memory>
#include <cstddef>
#include <variant>
#include <unordered_map>

#include "ir/declaration.h"

namespace toycc::ir {
    template <typename Register>
    using Allocation = std::variant<std::monostate, Register, size_t>;  // Unallocated / register / memory allocation relative to the stack frame / static area

    template <typename Register>
    struct StackFrame {
        std::unordered_map<std::shared_ptr<Declaration>, size_t> locals;
        size_t current_position = 0;

        std::unordered_map<std::shared_ptr<Declaration>, Allocation<Register>> allocation;

        size_t position(std::shared_ptr<Declaration> declaration);
    };
}
